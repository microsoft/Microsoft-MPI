// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef ATTR_H
#define ATTR_H

/* Keyvals and attributes */
/*TKyOverview.tex

  Keyvals are MPI objects that, unlike most MPI objects, are defined to be
  integers rather than a handle (e.g., 'MPI_Comm').  However, they really
  `are` MPI opaque objects and are handled by the MPICH implementation in
  the same way as all other MPI opaque objects.  The only difference is that
  there is no 'typedef int MPI_Keyval;' in 'mpi.h'.  In particular, keyvals
  are encoded (for direct and indirect references) in the same way that
  other MPI opaque objects are

  Each keyval has a copy and a delete function associated with it.
  Unfortunately, these have a slightly different calling sequence for
  each language, particularly when the size of a pointer is
  different from the size of a Fortran integer.  The unions
  'MPID_Copy_function' and 'MPID_Delete_function' capture the differences
  in a single union type.

  Notes:
  One potential user error is to access an attribute in one language (say
  Fortran) that was created in another (say C).  We could add a check and
  generate an error message in this case; note that this would have to
  be an option, because (particularly when accessing the attribute from C),
  it may be what the user intended, and in any case, it is a valid operation.

  T*/
/*TAttrOverview.tex
 *
 * The MPI standard allows `attributes`, essentially an '(integer,pointer)'
 * pair, to be attached to communicators, windows, and datatypes.
 * The integer is a `keyval`, which is allocated by a call (at the MPI level)
 * to 'MPI_Comm/Type/Win_create_keyval'.  The pointer is the value of
 * the attribute.
 * Attributes are primarily intended for use by the user, for example, to save
 * information on a communicator, but can also be used to pass data to the
 * MPI implementation.  For example, an attribute may be used to pass
 * Quality of Service information to an implementation to be used with
 * communication on a particular communicator.
 * To provide the most general access by the ADI to all attributes, the
 * ADI defines a collection of routines that are used by the implementation
 * of the MPI attribute routines (such as 'MPI_Comm_get_attr').
 * In addition, the MPI routines involving attributes will invoke the
 * corresponding 'hook' functions (e.g., 'MPID_Dev_comm_attr_set_hook')
 * should the device define them.
 *
 * Attributes on windows and datatypes are defined by MPI but not of
 * interest (as yet) to the device.
 *
 * In addition, there are seven predefined attributes that the device must
 * supply to the implementation.  This is accomplished through
 * data values that are part of the 'MPIR_Process' data block.
 *  The predefined keyvals on 'MPI_COMM_WORLD' are\:
 *.vb
 * Keyval                     Related Module
 * MPI_APPNUM                 Dynamic
 * MPI_HOST                   Core
 * MPI_IO                     Core
 * MPI_LASTUSEDCODE           Error
 * MPI_TAG_UB                 Communication
 * MPI_UNIVERSE_SIZE          Dynamic
 * MPI_WTIME_IS_GLOBAL        Timer
 *.ve
 * The values stored in the 'MPIR_Process' block are the actual values.  For
 * example, the value of 'MPI_TAG_UB' is the integer value of the largest tag.
 * The
 * value of 'MPI_WTIME_IS_GLOBAL' is a '1' for true and '0' for false.  Likely
 * values for 'MPI_IO' and 'MPI_HOST' are 'MPI_ANY_SOURCE' and 'MPI_PROC_NULL'
 * respectively.
 *
 T*/

/* Because Comm, Datatype, and File handles are all ints, and because
   attributes are otherwise identical between the three types, we
   only store generic copy and delete functions.  This allows us to use
   common code for the attribute set, delete, and dup functions */
/*E
  MPID_Copy_function - MPID Structure to hold an attribute copy function

  Notes:
  The appropriate element of this union is selected by using the language
  field of the 'keyval'.

  Because 'MPI_Comm', 'MPI_Win', and 'MPI_Datatype' are all 'int's in
  MPICH2, we use a single C copy function rather than have separate
  ones for the Communicator, Window, and Datatype attributes.

  There are no corresponding typedefs for the Fortran functions.  The
  F77 function corresponds to the Fortran 77 binding used in MPI-1 and the
  F90 function corresponds to the Fortran 90 binding used in MPI-2.

  Module:
  Attribute-DS

  E*/

int
MPIAPI
MPIR_Attr_copy_c_proxy(
    MPI_Comm_copy_attr_function* user_function,
    int handle,
    int keyval,
    void* extra_state,
    void* attrib,
    void** attrib_copy,
    int* flag
    );

struct MPID_Copy_function
{
    MPI_Comm_copy_attr_function* user_function;
    MPID_Attr_copy_proxy* proxy;
};

/*E
  MPID_Delete_function - MPID Structure to hold an attribute delete function

  Notes:
  The appropriate element of this union is selected by using the language
  field of the 'keyval'.

  Because 'MPI_Comm', 'MPI_Win', and 'MPI_Datatype' are all 'int's in
  MPICH2, we use a single C delete function rather than have separate
  ones for the Communicator, Window, and Datatype attributes.

  There are no corresponding typedefs for the Fortran functions.  The
  F77 function corresponds to the Fortran 77 binding used in MPI-1 and the
  F90 function corresponds to the Fortran 90 binding used in MPI-2.

  Module:
  Attribute-DS

  E*/

int
MPIAPI
MPIR_Attr_delete_c_proxy(
    MPI_Comm_delete_attr_function* user_function,
    int handle,
    int keyval,
    void* attrib,
    void* extra_state
    );

struct MPID_Delete_function
{
    MPI_Comm_delete_attr_function* user_function;
    MPID_Attr_delete_proxy* proxy;
};

/*S
  MPID_Keyval - Structure of an MPID keyval

  Module:
  Attribute-DS

  S*/
struct MPID_Keyval
{
    int                  handle;
    volatile long        ref_count;
    MPID_Object_kind     kind;
    void                 *extra_state;
    MPID_Copy_function   copyfn;
    MPID_Delete_function delfn;

public:
    void SpecializeHandle( MPID_Object_kind kind )
    {
        handle = (handle & ~(0x03c00000)) | (kind << 22);
    }

    static MPID_Object_kind GetKind( int handle )
    {
        return static_cast<MPID_Object_kind>( (handle & 0x03c00000) >> 22);
    }
};

extern MPIU_Object_alloc_t MPID_Keyval_mem;
extern MPID_Keyval MPID_Keyval_direct[];

//
// Keyval accounting
//
#if DBG
#   define MPID_Keyval_track_user_alloc() ::InterlockedIncrement(&MPID_Keyval_mem.num_user_alloc)
#   define MPID_Keyval_track_user_free() InterlockedDecrement(&MPID_Keyval_mem.num_user_alloc)
#else
#   define MPID_Keyval_track_user_alloc()
#   define MPID_Keyval_track_user_free()
#endif

#define MPIR_Keyval_add_ref( _keyval ) \
    MPIU_Object_add_ref( _keyval )

#define MPIR_Keyval_release_ref( _keyval, _inuse ) \
    MPIU_Object_release_ref( _keyval, _inuse )

/* Attributes need no ref count or handle, but since we want to use the
   common block allocator for them, we must provide those elements
*/
/*S
  MPID_Attribute - Structure of an MPID attribute

  Notes:
  Attributes don''t have 'ref_count's because they don''t have reference
  count semantics.  That is, there are no shallow copies or duplicates
  of an attibute.  An attribute is copied when the communicator that
  it is attached to is duplicated.  Subsequent operations, such as
  'MPI_Comm_attr_free', can change the attribute list for one of the
  communicators but not the other, making it impractical to keep the
  same list.  (We could defer making the copy until the list is changed,
  but even then, there would be no reference count on the individual
  attributes.)

  A pointer to the keyval, rather than the (integer) keyval itself is
  used since there is no need within the attribute structure to make
  it any harder to find the keyval structure.

  The attribute value is a 'void *'.  If 'sizeof(MPI_Fint)' > 'sizeof(void*)',
  then this must be changed (no such system has been encountered yet).
  For the Fortran 77 routines in the case where 'sizeof(MPI_Fint)' <
  'sizeof(void*)', the high end of the 'void *' value is used.  That is,
  we cast it to 'MPI_Fint *' and use that value.

  Module:
  Attribute-DS

 S*/
struct MPID_Attribute
{
    int          handle;
    volatile long ref_count;
    MPID_Keyval  *keyval;           /* Keyval structure for this attribute */
    struct MPID_Attribute *next;    /* Pointer to next in the list */
    void *      value;              /* Stored value */
};

extern MPIU_Object_alloc_t MPID_Attr_mem;


int MPIR_Attr_dup_list( int, MPID_Attribute *, MPID_Attribute ** );


int MPIR_Attr_delete_list(
    _In_ int handle,
    _Inout_ _Outptr_result_maybenull_ MPID_Attribute** attr
    );


/*@
  MPID_Get_universe_size - Return the number of processes that the current
  process management environment can handle

  Return value:
    The universe size; MPIR_UNIVERSE_SIZE_NOT_AVAILABLE if the
    size cannot be determined
@*/
int MPID_Get_universe_size();

#define MPIR_UNIVERSE_SIZE_NOT_SET -1
#define MPIR_UNIVERSE_SIZE_NOT_AVAILABLE -2


struct PreDefined_attrs
{
    int appnum;          /* Application number provided by mpiexec (MPI-2) */
    int host;            /* host */
    int io;              /* standard io allowed */
    int lastusedcode;    /* last used error code (MPI-2) */
    int tag_ub;          /* Maximum message tag */
    int universe;        /* Universe size from mpiexec (MPI-2) */
    int wtime_is_global; /* Wtime is global over processes in COMM_WORLD */
};

int MPIR_Call_attr_delete(int handle, MPID_Attribute *attr_p);

//
// TODO: Probably don't need this anymore now that we have MPID_Attr_delete?
//
void MPID_Attr_free(MPID_Attribute *attr_ptr);


//
// Define a type for a validated datatype handle.
//
class KeyHandle
{
    int m_hKey;

public:
    KeyHandle()
        : m_hKey( MPI_KEYVAL_INVALID )
    {
    }


    explicit KeyHandle( int key)
        : m_hKey( key )
    {
    }


    inline int GetMpiHandle() const
    {
        return m_hKey;
    }


    inline _Ret_ MPID_Keyval* Get() const
    {
        MPIU_Assert( HANDLE_GET_TYPE( m_hKey ) != HANDLE_TYPE_BUILTIN );
        MPID_Keyval* pKey;
        MPID_Keyval_get_ptr_valid( m_hKey, pKey );
        return pKey;
    }
};


MPI_RESULT
MPID_Key_create(
    _In_ MPID_Object_kind kind,
    _In_opt_ MPI_Comm_copy_attr_function *copy_attr_fn,
    _In_opt_ MPI_Comm_delete_attr_function *delete_attr_fn,
    _In_opt_ void *extra_state,
    _Outptr_ MPID_Keyval** ppKey
    );


_Success_( return == TRUE )
BOOL
MPID_Attr_get(
    _In_ const MPID_Attribute* pAttrList,
    _In_ MPID_Keyval* pKey,
    _Outptr_ void** pValue
    );


MPI_RESULT
MPID_Attr_set(
    _Inout_ _Outptr_  MPID_Attribute** ppAttrList,
    _In_ MPID_Keyval* pKey,
    _In_ int handle,
    _In_opt_ void* value
    );


MPI_RESULT
MPID_Attr_delete(
    _Inout_ _Outptr_result_maybenull_ MPID_Attribute** ppAttrList,
    _In_ const MPID_Keyval* pKey,
    _In_ int handle
    );


#endif // ATTR_H
