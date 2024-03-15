// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef MPIHANDLE_H_INCLUDED
#define MPIHANDLE_H_INCLUDED

#include "MpiLock.h"

/*TDSOverview.tex

  MPI has a number of data structures, most of which are represented by
  an opaque handle in an MPI program.  In the MPICH implementation of MPI,
  these handles are represented
  as integers; this makes implementation of the C/Fortran handle transfer
  calls (part of MPI-2) easy.

  MPID objects (again with the possible exception of 'MPI_Request's)
  are allocated by a common set of object allocation functions.
  These are
.vb
    void *MPIU_Handle_obj_create( MPIU_Object_alloc_t *objmem )
    void MPIU_Handle_obj_destroy( MPIU_Object_alloc_t *objmem, void *object )
.ve
  where 'objmem' is a pointer to a memory allocation object that knows
  enough to allocate objects, including the
  size of the object and the location of preallocated memory, as well
  as the type of memory allocator.  By providing the routines to allocate and
  free the memory, we make it easy to use the same interface to allocate both
  local and shared memory for objects (always using the same kind for each
  type of object).

  The names create/destroy were chosen because they are different from
  new/delete (C++ operations) and malloc/free.
  Any name choice will have some conflicts with other uses, of course.

  Reference Counts:
  Many MPI objects have reference count semantics.
  The semantics of MPI require that many objects that have been freed by the
  user
  (e.g., with 'MPI_Type_free' or 'MPI_Comm_free') remain valid until all
  pending
  references to that object (e.g., by an 'MPI_Irecv') are complete.  There
  are several ways to implement this; MPICH uses `reference counts` in the
  objects.  To support the 'MPI_THREAD_MULTIPLE' level of thread-safety, these
  reference counts must be accessed and updated atomically.
  A reference count for
  `any` object can be incremented (atomically)
  with 'MPIU_Object_add_ref(objptr)'
  and decremented with 'MPIU_Object_release_ref(objptr,newval_ptr)'.
  These have been designed so that then can be implemented as inlined
  macros rather than function calls, even in the multithreaded case, and
  can use special processor instructions that guarantee atomicity to
  avoid thread locks.
  The decrement routine sets the value pointed at by 'inuse_ptr' to 0 if
  the postdecrement value of the reference counter is zero, and to a non-zero
  value otherwise.  If this value is zero, then the routine that decremented
  the
  reference count should free the object.  This may be as simple as
  calling 'MPIU_Handle_obj_destroy' (for simple objects with no other allocated
  storage) or may require calling a separate routine to destroy the object.
  Because MPI uses 'MPI_xxx_free' to both decrement the reference count and
  free the object if the reference count is zero, we avoid the use of 'free'
  in the MPID routines.

  The 'inuse_ptr' approach is used rather than requiring the post-decrement
  value because, for reference-count semantics, all that is necessary is
  to know when the reference count reaches zero, and this can sometimes
  be implemented more cheaply that requiring the post-decrement value (e.g.,
  on IA32, there is an instruction for this operation).

  Question:
  Should we state that this is a macro so that we can use a register for
  the output value?  That avoids a store.  Alternately, have the macro
  return the value as if it was a function?

  Structure Definitions:
  The structure definitions in this document define `only` that part of
  a structure that may be used by code that is making use of the ADI.
  Thus, some structures, such as 'MPID_Comm', have many defined fields;
  these are used to support MPI routines such as 'MPI_Comm_size' and
  'MPI_Comm_remote_group'.  Other structures may have few or no defined
  members; these structures have no fields used outside of the ADI.
  In C++ terms,  all members of these structures are 'private'.

  For the initial implementation, we expect that the structure definitions
  will be designed for the multimethod device.  However, all items that are
  specific to a particular device (including the multi-method device)
  will be placed at the end of the structure;
  the document will clearly identify the members that all implementations
  will provide.  This simplifies much of the code in both the ADI and the
  implementation of the MPI routines because structure member can be directly
  accessed rather than using some macro or C++ style method interface.

 T*/


/*TOpaqOverview.tex
  MPI Opaque Objects:

  MPI Opaque objects such as 'MPI_Comm' or 'MPI_Datatype' are specified by
  integers (in the MPICH2 implementation); the MPI standard calls these
  handles.
  Out of range values are invalid; the value 0 is reserved.
  For most (with the possible exception of
  'MPI_Request' for performance reasons) MPI Opaque objects, the integer
  encodes both the kind of object (allowing runtime tests to detect a datatype
  passed where a communicator is expected) and important properties of the
  object.  Even the 'MPI_xxx_NULL' values should be encoded so that
  different null handles can be distinguished.  The details of the encoding
  of the handles is covered in more detail in the MPICH2 Design Document.
  For the most part, the ADI uses pointers to the underlying structures
  rather than the handles themselves.  However, each structure contains an
  'handle' field that is the corresponding integer handle for the MPI object.

  MPID objects (objects used within the implementation of MPI) are not opaque.

  T*/

/* Known MPI object types.  These are used for both the error handlers
   and for the handles.  This is a 4 bit value.  0 is reserved for so
   that all-zero handles can be flagged as an error. */
/*E
  MPID_Object_kind - Object kind (communicator, window, or file)

  Notes:
  This enum is used by keyvals and errhandlers to indicate the type of
  object for which MPI opaque types the data is valid.  These are defined
  as bits to allow future expansion to the case where an object is value for
  multiple types (for example, we may want a universal error handler for
  errors return).  This is also used to indicate the type of MPI object a
  MPI handle represents.  It is an enum because only this applies only the
  the MPI and internal MPICH2 objects.

  The 'MPID_PROCGROUP' kind is used to manage process groups (different
  from MPI Groups) that are used to keep track of collections of
  processes (each 'MPID_PROCGROUP' corresponds to a group of processes
  that define an 'MPI_COMM_WORLD'.  This becomes important only
  when MPI-2 dynamic process features are supported.  'MPID_VCONN' is
  a virtual connection; while this is not part of the overall ADI3
  design, an object that manages connections to other processes is
  a common need, and 'MPID_VCONN' may be used for that.

  Module:
  Attribute-DS
  E*/


//
//  Handle values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-------+---------------------------------------------------+
//  |Typ| Kind  |                     Index                         |
//  +---+-------+---------------------------------------------------+
//
//
//  Handles of type HANDLE_TYPE_INDIRECT are laid out as follows:
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-------+-------------------+-------------------------------+
//  |Typ| Kind  |       Block       |              Index            |
//  +---+-------+-------------------+-------------------------------+
//
//  where
//
//      Typ - is the handle type:
//
#define HANDLE_TYPE_INVALID  0x0
#define HANDLE_TYPE_BUILTIN  0x1
#define HANDLE_TYPE_DIRECT   0x2
#define HANDLE_TYPE_INDIRECT 0x3

#define HANDLE_TYPE_MASK 0xc0000000
#define HANDLE_TYPE_SHIFT 30
#define HANDLE_GET_TYPE(a) (((a) & HANDLE_TYPE_MASK)>>HANDLE_TYPE_SHIFT)
#define HANDLE_SET_TYPE(a,kind) ((a)|((kind)<<HANDLE_TYPE_SHIFT))

#define HANDLE_IS_VALID(a) (HANDLE_GET_TYPE(a) != HANDLE_TYPE_INVALID)
#define HANDLE_IS_BUILTIN(a) (HANDLE_GET_TYPE(a) == HANDLE_TYPE_BUILTIN)

//
//      Kind - is the kind of MPI object:
//
typedef enum MPID_Object_kind
{
  MPID_COMM       = 0x1,
  MPID_GROUP      = 0x2,
  MPID_DATATYPE   = 0x3,
  MPID_FILE       = 0x4,               /* This is not used */
  MPID_ERRHANDLER = 0x5,
  MPID_OP         = 0x6,
  MPID_INFO       = 0x7,
  MPID_WIN        = 0x8,
  MPID_KEYVAL     = 0x9,
  MPID_ATTR       = 0xa,
  MPID_REQUEST    = 0xb,
  MPID_MESSAGE    = 0xc,
  MPID_VCONN      = 0xd,
  MPID_GREQ_CLASS = 0xf,
  MPID_OBJECT_KIND_MAX

} MPID_Object_kind;

extern const char* MPID_Object_kind_names[MPID_OBJECT_KIND_MAX];

#define HANDLE_MPI_KIND_SHIFT 26
#define HANDLE_GET_MPI_KIND(a) ( ((a) & 0x3c000000) >> HANDLE_MPI_KIND_SHIFT )
#define HANDLE_SET_MPI_KIND(a,kind) ( ((a) & 0xc3ffffff) | ((kind) << HANDLE_MPI_KIND_SHIFT) )

//
//      Block - for indirect handles only, the index of the indirect block to
//              which the Index applies
//
#define HANDLE_INDIRECT_SHIFT 16
#define HANDLE_INDIRECT_BLOCK(a) (((a)& 0x03FF0000) >> HANDLE_INDIRECT_SHIFT)
/* Handle block is between 1 and 1024 *elements* */
#define HANDLE_BLOCK_SIZE 256
C_ASSERT(HANDLE_BLOCK_SIZE <= 1024);

//
//      Index - Index of the object.  For indirect handles, the index is relative ot the block.
//
#define HANDLE_INDIRECT_INDEX(a) ((a) & 0x0000FFFF)
#define HANDLE_BLOCK_INDEX_SIZE 1024
C_ASSERT(HANDLE_BLOCK_SIZE < 65536);

#define HANDLE_INDEX_MASK 0x03FFFFFF
#define HANDLE_DIRECT_INDEX(a) ((a) & HANDLE_INDEX_MASK)

#define HANDLE_BUILTIN_INDEX(a) ((a) & 0x000000FF)

/* ALL objects have the handle as the first value. */
/* Inactive (unused and stored on the appropriate avail list) objects
   have MPIU_Handle_common as the head */
typedef struct MPIU_Handle_common
{
    int handle;
    volatile long ref_count;
    struct MPIU_Handle_common *next;   /* Free handles use this field to point to the next
                     free object */

} MPIU_Handle_common;

/* All *active* (in use) objects have the handle as the first value; objects
   with referene counts have the reference count as the second value.
   See MPIU_Object_add_ref and MPIU_Object_release_ref. */
typedef struct MPIU_Handle_head
{
    int handle;
    volatile long ref_count;

} MPIU_Handle_head;

/* This type contains all of the data, except for the direct array,
   used by the object allocators. */
typedef struct MPIU_Object_alloc_t
{
    MPIU_Handle_common *avail;          /* Next available object */
    int                initialized;     /* */
    void              *(*indirect)[];   /* Pointer to indirect object blocks */
    int                indirect_size;   /* Number of allocated indirect blocks */
    MPID_Object_kind   kind;            /* Kind of object this is for */
    int                size;            /* Size of an individual object */
    void               *direct;         /* Pointer to direct block, used
                                           for allocation */
    int                direct_size;     /* Size of direct block */

    MPI_RWLOCK         alloc_lock;

#if DBG
    int                num_alloc;             /* Number of objects out of the pool. */
    volatile long      num_user_alloc;        /* Number of objects out of the pool owned by the client. */
    volatile long      num_user_msg_alloc;    /* Number of objects out of the pool of kind MPID_MESSAGE owned by the client. */
#endif

} MPIU_Object_alloc_t;


_Success_(return!=nullptr)
_Ret_valid_
void* MPIU_Handle_obj_alloc(
    _In_ MPIU_Object_alloc_t* objmem
    );

void MPIU_Handle_obj_free(
    _In_ MPIU_Object_alloc_t* objmem,
    _In_ _Post_ptr_invalid_ void* obj
    );

_Success_(return!=nullptr)
void* MPIU_Handle_get_ptr_indirect(
    _In_ int handle,
    _In_ MPIU_Object_alloc_t* objmem
    );


/* ------------------------------------------------------------------------- */
/* mpiobjref.h */
/* ------------------------------------------------------------------------- */


/*M
   MPIU_Object_add_ref - Increment the reference count for an MPI object

   Synopsis:
.vb
    MPIU_Object_add_ref( MPIU_Object *ptr )
.ve

   Input Parameter:
.  ptr - Pointer to the object.

   Notes:
   In an unthreaded implementation, this function will usually be implemented
   as a single-statement macro.  In an 'MPI_THREAD_MULTIPLE' implementation,
   this routine must implement an atomic increment operation, using, for
   example, a lock on datatypes or special assembly code such as
.vb
   try-again:
      load-link          refcount-address to r2
      add                1 to r2
      store-conditional  r2 to refcount-address
      if failed branch to try-again:
.ve
   on RISC architectures or
.vb
   lock
   inc                   refcount-address or
.ve
   on IA32; "lock" is a special opcode prefix that forces atomicity.  This
   is not a separate instruction; however, the GNU assembler expects opcode
   prefixes on a separate line.

   Module:
   MPID_CORE

   Question:
   This accesses the 'ref_count' member of all MPID objects.  Currently,
   that member is typed as 'volatile int'.  However, for a purely polling,
   thread-funnelled application, the 'volatile' is unnecessary.  Should
   MPID objects use a 'typedef' for the 'ref_count' that can be defined
   as 'volatile' only when needed?  For now, the answer is no; there isn''t
   enough to be gained in that case.
M*/

/*M
   MPIU_Object_release_ref - Decrement the reference count for an MPI object

   Synopsis:
.vb
   MPIU_Object_release_ref( MPIU_Object *ptr, int *inuse_ptr )
.ve

   Input Parameter:
.  objptr - Pointer to the object.

   Output Parameter:
.  inuse_ptr - Pointer to the value of the reference count after decrementing.
   This value is either zero or non-zero. See below for details.

   Notes:
   In an unthreaded implementation, this function will usually be implemented
   as a single-statement macro.  In an 'MPI_THREAD_MULTIPLE' implementation,
   this routine must implement an atomic decrement operation, using, for
   example, a lock on datatypes or special assembly code such as
.vb
   try-again:
      load-link          refcount-address to r2
      sub                1 to r2
      store-conditional  r2 to refcount-address
      if failed branch to try-again:
      store              r2 to newval_ptr
.ve
   on RISC architectures or
.vb
      lock
      dec                   refcount-address
      if zf store 0 to newval_ptr else store 1 to newval_ptr
.ve
   on IA32; "lock" is a special opcode prefix that forces atomicity.  This
   is not a separate instruction; however, the GNU assembler expects opcode
   prefixes on a separate line.  'zf' is the zero flag; this is set if the
   result of the operation is zero.  Implementing a full decrement-and-fetch
   would require more code and the compare and swap instruction.

   Once the reference count is decremented to zero, it is an error to
   change it.  A correct MPI program will never do that, but an incorrect one
   (particularly a multithreaded program with a race condition) might.

   The following code is `invalid`\:
.vb
   MPID_Object_release_ref( datatype_ptr );
   if (datatype_ptr->ref_count == 0) MPID_Datatype_free( datatype_ptr );
.ve
   In a multi-threaded implementation, the value of 'datatype_ptr->ref_count'
   may have been changed by another thread, resulting in both threads calling
   'MPID_Datatype_free'.  Instead, use
.vb
   MPID_Object_release_ref( datatype_ptr, &inUse );
   if (!inuse)
       MPID_Datatype_free( datatype_ptr );
.ve

   Module:
   MPID_CORE
  M*/

/* The MPIU_DBG... statements are macros that vanish unless
   --enable-g=log is selected.  MPIU_HANDLE_CHECK_REFCOUNT is
   defined above, and adds an additional sanity check for the refcounts
*/
template<typename T>
__forceinline void MPIU_Object_set_ref(_In_ T* objptr, _In_ long val)
{
    ::InterlockedExchange(&objptr->ref_count, val);
}

template<typename T>
__forceinline void MPIU_Object_add_ref(_In_ T* objptr)
{
    ::InterlockedIncrement(&objptr->ref_count);
}

template<typename T>
__forceinline void MPIU_Object_release_ref(_In_ T* objptr, _Out_ BOOL* inuse_ptr)
{
    *inuse_ptr = ::InterlockedDecrement(&objptr->ref_count);
}

/* ------------------------------------------------------------------------- */
/* mpiobjref.h */
/* ------------------------------------------------------------------------- */

/* Convert Handles to objects for MPI types that have predefined objects */
/* Question.  Should this do ptr=0 first, particularly if doing --enable-strict
   complication? */
#define MPID_Getb_ptr(kind,a,bmsk,ptr)                                  \
{                                                                       \
   switch (HANDLE_GET_TYPE(a)) {                                        \
      case HANDLE_TYPE_BUILTIN:                                         \
          ptr=MPID_##kind##_builtin+((a)&(bmsk));                       \
          __analysis_assume(ptr != nullptr);                               \
          break;                                                        \
      case HANDLE_TYPE_DIRECT:                                          \
          ptr=MPID_##kind##_direct+HANDLE_DIRECT_INDEX(a);              \
          __analysis_assume(ptr != nullptr);                               \
          break;                                                        \
      case HANDLE_TYPE_INDIRECT:                                        \
          ptr=((MPID_##kind*)                                           \
               MPIU_Handle_get_ptr_indirect(a,&MPID_##kind##_mem));     \
          __analysis_assume(ptr != nullptr);                               \
          break;                                                        \
      case HANDLE_TYPE_INVALID:                                         \
      default:                                                          \
          ptr=0;                                                        \
          break;                                                        \
    }                                                                   \
}

#define MPID_Getb_ptr_valid(kind,a,bmsk,ptr)                            \
{                                                                       \
   switch (HANDLE_GET_TYPE(a)) {                                        \
      case HANDLE_TYPE_BUILTIN:                                         \
          ptr=MPID_##kind##_builtin+((a)&(bmsk));                       \
          __analysis_assume(ptr != nullptr);                            \
          break;                                                        \
      case HANDLE_TYPE_DIRECT:                                          \
          ptr=MPID_##kind##_direct+HANDLE_DIRECT_INDEX(a);              \
          __analysis_assume(ptr != nullptr);                            \
          break;                                                        \
      case HANDLE_TYPE_INDIRECT:                                        \
          ptr=((MPID_##kind*)                                           \
               MPIU_Handle_get_ptr_indirect(a,&MPID_##kind##_mem));     \
          __analysis_assume(ptr != nullptr);                            \
          break;                                                        \
      case HANDLE_TYPE_INVALID:                                         \
      default:                                                          \
          MPID_Abort(nullptr, TRUE, 0, "Invalid Handle type for " #kind);\
          break;                                                        \
    }                                                                   \
}


/* Convert handles to objects for MPI types that do _not_ have any predefined
   objects */
/* Question.  Should this do ptr=0 first, particularly if doing --enable-strict
   complication? */
#define MPID_Get_ptr(kind,a,ptr)                                    \
{                                                                   \
   switch (HANDLE_GET_TYPE(a)) {                                    \
      case HANDLE_TYPE_DIRECT:                                      \
          ptr=MPID_##kind##_direct+HANDLE_DIRECT_INDEX(a);          \
          __analysis_assume(ptr != nullptr);                        \
          break;                                                    \
      case HANDLE_TYPE_INDIRECT:                                    \
          ptr=((MPID_##kind*)                                       \
               MPIU_Handle_get_ptr_indirect(a,&MPID_##kind##_mem)); \
          __analysis_assume(ptr != nullptr);                        \
          break;                                                    \
      case HANDLE_TYPE_INVALID:                                     \
      case HANDLE_TYPE_BUILTIN:                                     \
      default:                                                      \
          ptr=0;                                                    \
          break;                                                    \
     }                                                              \
}

#define MPID_Get_ptr_valid(kind,a,ptr)                              \
{                                                                   \
   switch (HANDLE_GET_TYPE(a)) {                                    \
      case HANDLE_TYPE_DIRECT:                                      \
          ptr=MPID_##kind##_direct+HANDLE_DIRECT_INDEX(a);          \
          __analysis_assume(ptr != nullptr);                        \
          break;                                                    \
      case HANDLE_TYPE_INDIRECT:                                    \
          ptr=((MPID_##kind*)                                       \
               MPIU_Handle_get_ptr_indirect(a,&MPID_##kind##_mem)); \
          __analysis_assume(ptr != nullptr);                        \
          break;                                                    \
      case HANDLE_TYPE_INVALID:                                     \
      case HANDLE_TYPE_BUILTIN:                                     \
      default:                                                      \
          MPID_Abort(nullptr, TRUE, 0, "Invalid Handle type for " #kind);\
          break;                                                    \
     }                                                              \
}



/* FIXME: the masks should be defined with the handle definitions instead
   of inserted here as literals */
#define MPID_Group_get_ptr(a,ptr)      MPID_Getb_ptr(Group,a,0x03ffffff,ptr)
#define MPID_File_get_ptr(a,ptr)       MPID_Get_ptr(File,a,ptr)
#define MPID_Errhandler_get_ptr(a,ptr) MPID_Getb_ptr(Errhandler,a,0x3,ptr)
#define MPID_Op_get_ptr(a,ptr)         MPID_Getb_ptr(Op,a,0x000000ff,ptr)
#define MPID_Info_get_ptr(a,ptr)       MPID_Get_ptr(Info,a,ptr)
#define MPID_Win_get_ptr(a,ptr)        MPID_Get_ptr(Win,a,ptr)
#define MPID_Request_get_ptr(a,ptr)    MPID_Get_ptr(Request,a,ptr)


#define MPID_Group_get_ptr_valid(a,ptr)      MPID_Getb_ptr_valid(Group,a,0x03ffffff,ptr)
#define MPID_File_get_ptr_valid(a,ptr)       MPID_Get_ptr_valid(File,a,ptr)
#define MPID_Errhandler_get_ptr_valid(a,ptr) MPID_Getb_ptr_valid(Errhandler,a,0x3,ptr)
#define MPID_Op_get_ptr_valid(a,ptr)         MPID_Getb_ptr_valid(Op,a,0x000000ff,ptr)
#define MPID_Info_get_ptr_valid(a,ptr)       MPID_Get_ptr_valid(Info,a,ptr)
#define MPID_Win_get_ptr_valid(a,ptr)        MPID_Get_ptr_valid(Win,a,ptr)
#define MPID_Request_get_ptr_valid(a,ptr)    MPID_Get_ptr_valid(Request,a,ptr)



/* Keyvals have a special format. This is roughly MPID_Get_ptrb, but
   the handle index is in a smaller bit field.  In addition,
   there is no storage for the builtin keyvals.
   For the indirect case, we mask off the part of the keyval that is
   in the bits normally used for the indirect block index.
*/
#define MPID_Keyval_get_ptr(a,ptr)     \
{                                                                       \
   switch (HANDLE_GET_TYPE(a)) {                                        \
      case HANDLE_TYPE_BUILTIN:                                         \
          ptr=0;                                                        \
          break;                                                        \
      case HANDLE_TYPE_DIRECT:                                          \
          ptr=MPID_Keyval_direct+((a)&0x3fffff);                        \
          break;                                                        \
      case HANDLE_TYPE_INDIRECT:                                        \
          ptr=((MPID_Keyval*)                                           \
             MPIU_Handle_get_ptr_indirect((a)&0xfc3fffff,&MPID_Keyval_mem)); \
          break;                                                        \
      case HANDLE_TYPE_INVALID:                                         \
      default:                              \
          ptr=0;                            \
          break;                            \
    }                                                                   \
}

#define MPID_Keyval_get_ptr_valid(a,ptr)     \
{                                                                       \
   switch (HANDLE_GET_TYPE(a)) {                                        \
      case HANDLE_TYPE_DIRECT:                                          \
          ptr=MPID_Keyval_direct+((a)&0x3fffff);                        \
          break;                                                        \
      case HANDLE_TYPE_INDIRECT:                                        \
          ptr=((MPID_Keyval*)                                           \
             MPIU_Handle_get_ptr_indirect((a)&0xfc3fffff,&MPID_Keyval_mem)); \
          break;                                                        \
      case HANDLE_TYPE_BUILTIN:                                         \
      case HANDLE_TYPE_INVALID:                                         \
      default:                                                          \
          MPID_Abort(nullptr, TRUE, 0, "Invalid Handle type for Keyval"); \
          break;                                                        \
    }                                                                   \
}


template<typename T> bool SetName( _Inout_ T* obj, _In_z_ const char* name )
{
    if( obj->name == obj->GetDefaultName() )
    {
        char* tempName = static_cast<char*>(MPIU_Malloc( MPI_MAX_OBJECT_NAME ));
        if( tempName == nullptr )
        {
            return false;
        }
        obj->name = tempName;
    }
    MPIU_Strncpy( const_cast<char*>(obj->name), name, MPI_MAX_OBJECT_NAME );
    return true;
}


template<typename T> void CleanupName( _Inout_ T* obj )
{
    if( obj->name != obj->GetDefaultName() && obj->name != nullptr )
    {
        MPIU_Free( const_cast<char*>(obj->name) );
        obj->name = nullptr;
    }
}


template<typename T> void InitName( _Inout_ T* obj )
{
    obj->name = obj->GetDefaultName();
}



#endif /* MPIHANDLE_H_INCLUDED */
