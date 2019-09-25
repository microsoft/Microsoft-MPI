// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"

const char* MPID_Object_kind_names[MPID_OBJECT_KIND_MAX] =
{
    "",
    "communicator",
    "group",
    "datatype",
    "file",
    "error handler",
    "op",
    "info",
    "window",
    "keyval",
    "attribute",
    "request",
    "process group",
    "virtual connection",
    "generalized request"
};


#ifdef NEEDS_PRINT_HANDLE
static void MPIU_Print_handle( int handle );
#endif

/* This is the utility file for info that contains routines used to
   manage the arrays used to store handle objects.

   To use these routines, allocate the following in a utility file
   used by each object (e.g., info, datatype, comm, group, ... ).
   (The comment format // is used in this example but the usual
   C comment convention should be used of course.)  The usage is described
   below.

   // Declarations begin here

   // Define the number of preallocated entries # omitted)
   define MPID_<OBJ>_PREALLOC 256

   // Preallocated objects
   MPID_<obj> MPID_<obj>_direct[MPID_<OBJ>_PREALLOC];

   // Static declaration of the information about the block
   MPIU_Object_alloc_t MPID_<obj>_mem = { 0, 0, 0, 0, MPID_<obj>,
                                      sizeof(MPID_<obj>), MPID_<obj>_direct,
                                      MPID_<OBJ>_PREALLOC, };
   // Declarations end here

   These provide for lazy initialization; applications that do not need a
   particular object will not include any of the code or even reference
   the space.

   Note that these routines are designed for the MPI objects, and include the
   creation of a "handle" that is appropriate for the MPI object value.

   The following routines are implemented:
   void MPIU_Handle_direct_init( void *direct, int direct_size, int obj_size, int handle_type )
        Initialize the preallocated array (MPID_<obj>_direct) with
        direct_size elements each of obj_size.
        handle_type is the kind of object (e.g., MPID_INFO)

   void *MPIU_Handle_indirect_init( void (**indirect)[], int *indirect_size,
                                    int indirect_max_size,
                                    int indirect_block_size,
                                    int obj_size,
                                    int handle_type )
        Initialize the indirect array (MPID_<obj>_indirect) of size
        indirect_size, each block of which contains indirect_block_size
        members of size obj_size.  Returns the first available element, or
        NULL if no memory is available.
        Also incrementes indirect_size and assigns to indirect if it is null.

        The Handle_indirect routine and the data structures that it manages
        require a little more discussion.
        This routine allocates an array of pointers to a block of storage.
        The block of storage contains space for indirect_block_size
        instances of an object of obj_size.  These blocks are allocated
        as needed; the pointers to these blocks are stored in the
        indirect array.  The value of indirect_size is the number of
        valid pointers in indirect.  In other words, indirect[0] through
        indirect[*indirect_size-1] contain pointers to blocks of
        storage of size indirect_block_size * obj_size.  The array
        indirect has indirect_max_size entries, each holding a pointer.

        The rationale for this approach is that this approach can
        handle large amounts of memory; however, relatively little
        memory is used unless needed.  The definitions in
        mpich2/src/include/mpihandlemem.h define defaults for the
        indirect_max_size (HANDLE_BLOCK_INDEX_SIZE = 1024) and
        indirect_block_size (HANDLE_BLOCK_SIZE = 256) that permits
        the allocation of 256K objects.

   None of these routines is thread-safe.  Any routine that uses them
   must ensure that only one thread at a time may call them.

*/

/*
 * You can use this to allocated that necessary local structures
 */
#define MPID_HANDLE_MEM_ALLOC(Name,NAME) \
MPID_##Name MPID_##Name_direct[MPID_##NAME##_PREALLOC]; \
static int initialize = 0;\
static int MPID_##Name *avail=0;\
static MPID_##Name *(*MPID_##Name##_indirect)[] = 0;\
static int MPID_##Name##_indirect_size = 0;

/* This routine is called by finalize when MPI exits */
static int MPIU_Handle_free_indirect(void* ptr)
{
    int i;
    MPIU_Object_alloc_t* objmem = (MPIU_Object_alloc_t*)ptr;
    void* (*indirect)[] = objmem->indirect;
    int indirect_size = objmem->indirect_size;

    if(indirect == NULL)
        return 0;

    /* Remove indirect allocated storage */
    for (i = 0; i < indirect_size; i++)
    {
        MPIU_Free( (*indirect)[i] );
    }

    MPIU_Free( indirect );

    return 0;
}


static void
MPIU_Handle_direct_init(
    _Inout_ void *direct,
    _In_range_(>,0) int direct_size,
    _In_range_(>,0) int obj_size,
    int handle_type
    )
{
    int i;
    char *ptr = (char *)direct;
    MPIU_Handle_common *hptr = NULL;

    MPIU_Assert(direct_size > 0);
    for(i = 0; i < direct_size; i++)
    {
        hptr = (MPIU_Handle_common *)ptr;
        ptr  = ptr + obj_size;
        hptr->next = (MPIU_Handle_common*)ptr;
        hptr->handle = ((unsigned)HANDLE_TYPE_DIRECT << HANDLE_TYPE_SHIFT) |
            (handle_type << HANDLE_MPI_KIND_SHIFT) | i;
    }
    MPIU_Assert(hptr != NULL);

OACR_WARNING_SUPPRESS(26500, "Suppress false anvil warning.")
    hptr->next = NULL;
}

/* indirect is really a pointer to a pointer to an array of pointers */
static MPIU_Handle_common *MPIU_Handle_indirect_init( void *(**indirect)[],
                                        int *indirect_size,
                                        int indirect_max_size,
                                        int indirect_block_size, int obj_size,
                                        int handle_type )
{
    MPIU_Handle_common *block_ptr;
    MPIU_Handle_common *hptr = NULL;
    char               *ptr;
    int                i;

    /* Must create new storage for dynamically allocated objects */
    /* Create the table */
    if (!*indirect)
    {
        /* printf( "Creating indirect table\n" ); */
        *indirect = (void *(*)[])MPIU_Calloc( indirect_max_size,
                                              sizeof(void *) );
        if (!*indirect)
        {
            return NULL;
        }
        *indirect_size = 0;
    }

    /* See if we can allocate another block */
    if (*indirect_size >= indirect_max_size-1)
    {
        return NULL;
    }

    /* Create the next block */
    /* printf( "Adding indirect block %d\n", MPID_Info_indirect_size ); */
    block_ptr = (MPIU_Handle_common*)MPIU_Calloc( indirect_block_size, obj_size );
    if (!block_ptr)
    {
        return NULL;
    }
    ptr = (char *)block_ptr;
    for (i=0; i<indirect_block_size; i++)
    {
        hptr       = (MPIU_Handle_common *)ptr;
        ptr        = ptr + obj_size;
        hptr->next = (MPIU_Handle_common *)ptr;
        hptr->handle   = ((unsigned)HANDLE_TYPE_INDIRECT << HANDLE_TYPE_SHIFT) |
            (handle_type << HANDLE_MPI_KIND_SHIFT) |
            (*indirect_size << HANDLE_INDIRECT_SHIFT) | i;
    }
    MPIU_Assert(hptr != NULL);
OACR_WARNING_SUPPRESS(26500, "Suppress false anvil warning.")
    hptr->next = NULL;
    /* We're here because avail is null, so there is no need to set
       the last block ptr to avail */
    /* printf( "loc of update is %x\n", &(**indirect)[*indirect_size] );  */
    (**indirect)[*indirect_size] = block_ptr;
    *indirect_size = *indirect_size + 1;
    return block_ptr;
}

/*+
  MPIU_Handle_obj_alloc - Create an object using the handle allocator

  Input Parameter:
. objmem - Pointer to object memory block.

  Return Value:
  Pointer to new object.  Null if no more objects are available or can
  be allocated.

  Notes:
  In addition to returning a pointer to a new object, this routine may
  allocate additional space for more objects.

  This routine is thread-safe.

  This routine is performance-critical (it may be used to allocate
  MPI_Requests) and should not call any other routines in the common
  case.

  +*/
static MPIU_Handle_common* MPIU_Handle_obj_grow(MPIU_Object_alloc_t* objmem)
{
    MPIU_Handle_common* ptr;

    int objsize = objmem->size;
    int objkind = objmem->kind;

    if (!objmem->initialized)
    {
#if DBG
        objmem->num_alloc = 0;
        objmem->num_user_alloc = 0;
#endif

        /* Setup the first block.  This is done here so that short MPI
           jobs do not need to include any of the Info code if no
           Info-using routines are used */
        objmem->initialized = 1;
        MPIU_Handle_direct_init(
            objmem->direct,
            objmem->direct_size,
            objsize,
            objkind
            );

        ptr = (MPIU_Handle_common*)objmem->direct;

        /* ptr points to object to allocate */

        /* Tell finalize to free up any memory that we allocate.
         * The 0 makes this the lowest priority callback, so
         * that other callbacks will finish before this one is invoked.
         */
        MPIR_Add_finalize(MPIU_Handle_free_indirect, objmem, 0);
    }
    else
    {
        /* no space left in direct block; setup the indirect block. */

        ptr = MPIU_Handle_indirect_init(
                &objmem->indirect,
                &objmem->indirect_size,
                HANDLE_BLOCK_INDEX_SIZE,
                HANDLE_BLOCK_SIZE,
                objsize,
                objkind
                );

        /* ptr points to object to allocate */
    }

    return ptr;
}

_Success_(return!=nullptr)
_Ret_valid_
void* MPIU_Handle_obj_alloc(
    _In_ MPIU_Object_alloc_t* objmem
    )
{
    MpiRwLockAcquireExclusive(&objmem->alloc_lock);

    MPIU_Handle_common* ptr = objmem->avail;
    if(ptr == nullptr)
    {
        ptr = MPIU_Handle_obj_grow(objmem);
    }

    if (ptr != nullptr)
    {
        objmem->avail = ptr->next;
        ptr->ref_count = 1;

#if DBG
        objmem->num_alloc++;
#endif
    }

    MpiRwLockReleaseExclusive(&objmem->alloc_lock);

    return ptr;
}

/*+
  MPIU_Handle_obj_free - Free an object allocated with MPID_Handle_obj_new

  Input Parameters:
+ objmem - Pointer to object block
- object - Object to delete

  Notes:
  This routine assumes that only a single thread calls it at a time; this
  is true for the SINGLE_CS approach to thread safety
  +*/
void MPIU_Handle_obj_free(
    _In_ MPIU_Object_alloc_t* objmem,
    _In_ _Post_ptr_invalid_ void* object
    )
{
    MpiRwLockAcquireExclusive(&objmem->alloc_lock);

    MPIU_Handle_common *obj = (MPIU_Handle_common *)object;
    obj->next           = objmem->avail;
    objmem->avail       = obj;
#if DBG
    objmem->num_alloc--;
#endif

    MpiRwLockReleaseExclusive(&objmem->alloc_lock);
}

/*
 * Get an pointer to dynamically allocated storage for objects.
 */
_Success_(return!=nullptr)
void* MPIU_Handle_get_ptr_indirect(
    _In_ int handle,
    _In_ MPIU_Object_alloc_t* objmem
    )
{
    void* ptr;
    int block_num, index_num;

    MPIU_Assert(HANDLE_GET_TYPE(handle) == HANDLE_TYPE_INDIRECT);

    /* Check for a valid handle type */
    if (HANDLE_GET_MPI_KIND(handle) != objmem->kind)
    {
        return nullptr;
    }

    MpiRwLockAcquireShared(&objmem->alloc_lock);

    /* Find the block */
    block_num = HANDLE_INDIRECT_BLOCK(handle);
    if (block_num >= objmem->indirect_size)
    {
        ptr = nullptr;
    }
    else
    {
        /* Find the entry */
        index_num = HANDLE_INDIRECT_INDEX(handle);
        /* If we could declare the blocks to a known size object, we
         could do something like
           return &( (MPID_Info**)*MPIU_Info_mem.indirect)[block_num][index_num];
         since we cannot, we do the calculation by hand.
        */
        /* Get the pointer to the block of addresses.  This is an array of
           void * */
        {
            char *block_ptr;
            /* Get the pointer to the block */
            block_ptr = (char *)(*(objmem->indirect))[block_num];
            /* Get the item */
            block_ptr += index_num * objmem->size;
            ptr = block_ptr;
        }
    }

    MpiRwLockReleaseShared(&objmem->alloc_lock);

    return ptr;
}

#ifdef NEEDS_PRINT_HANDLE
/* For debugging */
static void MPIU_Print_handle( int handle )
{
    int type, kind, block, index;

    type = HANDLE_GET_MPI_KIND(handle);
    kind = HANDLE_GET_TYPE(handle);
    switch (type)
    {
    case HANDLE_TYPE_INVALID:
        printf( "invalid" );
        break;
    case HANDLE_TYPE_BUILTIN:
        printf( "builtin" );
        break;
    case HANDLE_TYPE_DIRECT:
        index = HANDLE_DIRECT_INDEX(handle);
        printf( "direct: %d", index );
        break;
    case HANDLE_TYPE_INDIRECT:
        block = HANDLE_INDIRECT_BLOCK(handle);
        index = HANDLE_INDIRECT_INDEX(handle);
        printf( "indirect: block %d index %d", block, index );
        break;
    }
}
#endif
