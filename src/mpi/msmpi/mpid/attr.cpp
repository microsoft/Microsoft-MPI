// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*
 * Keyvals.  These are handled just like the other opaque objects in MPICH
 * The predefined keyvals (and their associated attributes) are handled
 * separately, without using the keyval
 * storage
 */

#ifndef MPID_KEYVAL_PREALLOC
#define MPID_KEYVAL_PREALLOC 16
#endif

/* Preallocated keyval objects */
MPID_Keyval MPID_Keyval_direct[MPID_KEYVAL_PREALLOC] = { {0} };
MPIU_Object_alloc_t MPID_Keyval_mem = { 0, 0, 0, 0, MPID_KEYVAL,
                                            sizeof(MPID_Keyval),
                                            MPID_Keyval_direct,
                                            MPID_KEYVAL_PREALLOC, };

#ifndef MPID_ATTR_PREALLOC
#define MPID_ATTR_PREALLOC 32
#endif

/* Preallocated keyval objects */
MPID_Attribute MPID_Attr_direct[MPID_ATTR_PREALLOC] = { {0} };
MPIU_Object_alloc_t MPID_Attr_mem = { 0, 0, 0, 0, MPID_ATTR,
                                            sizeof(MPID_Attribute),
                                            MPID_Attr_direct,
                                            MPID_ATTR_PREALLOC, };


MPI_RESULT
MPID_Key_create(
    _In_ MPID_Object_kind kind,
    _In_opt_ MPI_Comm_copy_attr_function *copy_attr_fn,
    _In_opt_ MPI_Comm_delete_attr_function *delete_attr_fn,
    _In_opt_ void *extra_state,
    _Outptr_ MPID_Keyval** ppKey
    )
{
    MPID_Keyval* pKeyval = static_cast<MPID_Keyval*>(
        MPIU_Handle_obj_alloc( &MPID_Keyval_mem )
        );
    if( pKeyval == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // The handle encodes the keyval kind.  Modify it to have the correct field
    //
    pKeyval->SpecializeHandle( kind );
    pKeyval->kind                 = kind;
    pKeyval->extra_state          = extra_state;
    pKeyval->copyfn.user_function = copy_attr_fn;
    pKeyval->copyfn.proxy         = MPIR_Attr_copy_c_proxy;
    pKeyval->delfn.user_function  = delete_attr_fn;
    pKeyval->delfn.proxy          = MPIR_Attr_delete_c_proxy;

    *ppKey = pKeyval;
    return MPI_SUCCESS;
}


void MPID_Attr_free(MPID_Attribute *attr_ptr)
{
    MPIU_Handle_obj_free(&MPID_Attr_mem, attr_ptr);
}

/*
  This function deletes a single attribute.
  It is called by both the function to delete a list and attribute set/put val.
  Return the MPI return code from the delete function; MPI_SUCCESS if there is
  no delete function.

  Even though there are separate keyvals for communicators, types, and files,
  we can use the same function because the handle for these is always an int
  in MPICH2.

  Note that this simply invokes the attribute delete function.  It does not
  remove the attribute from the list of attributes.
*/
MPI_RESULT MPIR_Call_attr_delete(int handle, MPID_Attribute *attr_p)
{
    int rc;
    MPID_Keyval* kv = attr_p->keyval;

    if(kv->delfn.user_function == NULL)
        return MPI_SUCCESS;

    rc = kv->delfn.proxy(
                kv->delfn.user_function,
                handle,
                attr_p->keyval->handle,
                attr_p->value,
                attr_p->keyval->extra_state
                );

    if(rc != 0)
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**userdel %d", rc);

    return MPI_SUCCESS;
}


/*
  This function copies a single attribute.
  It is called by the function to copy a list of attribute
  Return the return code from the copy function; MPI_SUCCESS if there is
  no copy function.

  Even though there are separate keyvals for communicators, types, and files,
  we can use the same function because the handle for these is always an int
  in MPICH2.

  Note that this simply invokes the attribute copy function.
*/
MPI_RESULT
MPIR_Call_attr_copy(
    int handle,
    MPID_Attribute *attr_p,
    void** value_copy,
    int* flag)
{
    int rc;
    MPID_Keyval* kv = attr_p->keyval;

    if(kv->copyfn.user_function == NULL)
        return MPI_SUCCESS;

    rc = kv->copyfn.proxy(
                kv->copyfn.user_function,
                handle,
                attr_p->keyval->handle,
                attr_p->keyval->extra_state,
                attr_p->value,
                value_copy,
                flag
                );

    if(rc != 0)
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**usercopy %d", rc);

    return MPI_SUCCESS;
}


_Success_( return == TRUE )
BOOL
MPID_Attr_get(
    _In_ const MPID_Attribute* pAttrList,
    _In_ MPID_Keyval* pKey,
    _Outptr_ void** pValue
    )
{
    for( const MPID_Attribute* pAttr = pAttrList;
        pAttr != nullptr;
        pAttr = pAttr->next )
    {
        if( pAttr->keyval == pKey )
        {
            *pValue = pAttr->value;
            return TRUE;
        }
    }
    return FALSE;
}


MPI_RESULT
MPID_Attr_set(
    _Inout_ _Outptr_ MPID_Attribute** ppAttrList,
    _In_ MPID_Keyval* pKey,
    _In_ int handle,
    _In_opt_ void* value
    )
{
    /* Look for attribute.  They are ordered by keyval handle.  This uses
       a simple linear list algorithm because few applications use more than a
       handful of attributes */

    MPID_Attribute** old_p = ppAttrList;
    MPID_Attribute* pAttr = *ppAttrList;
    while( pAttr != nullptr )
    {
        if( pAttr->keyval->handle < pKey->handle )
        {
            old_p = &pAttr->next;
            pAttr = pAttr->next;
            continue;
        }

        if( pAttr->keyval == pKey )
        {
            /* If found, call the delete function before replacing the
               attribute */
            int mpi_errno = MPIR_Call_attr_delete( handle, pAttr );
            if( mpi_errno == MPI_SUCCESS )
            {
                pAttr->value = value;
            }
            return mpi_errno;
        }

        //
        // Found insertion location.
        //
        break;
    }

    MPID_Attribute *new_p = (MPID_Attribute *)MPIU_Handle_obj_alloc( &MPID_Attr_mem );
    if( new_p == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_p->keyval        = pKey;
    MPIR_Keyval_add_ref(pKey);

    new_p->value         = value;
    new_p->next          = pAttr;
    *old_p = new_p;

    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Attr_delete(
    _Inout_ _Outptr_result_maybenull_ MPID_Attribute** ppAttrList,
    _In_ const MPID_Keyval* pKey,
    _In_ int handle
    )
{
    MPID_Attribute** old_p = ppAttrList;
    MPID_Attribute* pAttr  = *ppAttrList;

    while( pAttr != nullptr )
    {
        if( pAttr->keyval->handle == pKey->handle )
        {
            break;
        }
        old_p = &pAttr->next;
        pAttr = pAttr->next;
    }

    if( pAttr != nullptr )
    {
        /* Run the delete function, if any, and then free the attribute
           storage */
        int mpi_errno = MPIR_Call_attr_delete( handle, pAttr );

        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        int in_use;
        /* We found the attribute.  Remove it from the list */
        *old_p = pAttr->next;
        /* Decrement the use of the keyval */
        MPIR_Keyval_release_ref( pAttr->keyval, &in_use );
        if (!in_use)
        {
            MPIU_Handle_obj_free( &MPID_Keyval_mem, pAttr->keyval );
        }
        MPID_Attr_free( pAttr );
    }
    return MPI_SUCCESS;
}


/* Routine to duplicate an attribute list */
MPI_RESULT
MPIR_Attr_dup_list(
    int handle,
    MPID_Attribute *old_attrs,
    MPID_Attribute **new_attr )
{
    MPID_Attribute* p;
    MPID_Attribute* new_p;
    void* new_value = NULL;
    MPI_RESULT mpi_errno;

    for(p = old_attrs; p != NULL; p = p->next)
    {
        /* call the attribute copy function (if any) */
        int flag = 0;
        mpi_errno = MPIR_Call_attr_copy(
                        handle,
                        p,
                        &new_value,
                        &flag
                        );

        if(mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        /* If flag was returned as true, then insert this attribute into the new list (new_attr) */
        if(!flag)
            continue;

        /* duplicate the attribute by creating new storage, copying the
        attribute value, and invoking the copy function */
        new_p = (MPID_Attribute *)MPIU_Handle_obj_alloc( &MPID_Attr_mem );
        if( new_p == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        new_p->keyval = p->keyval;
        MPIR_Keyval_add_ref(p->keyval);

        new_p->value = new_value;
        new_p->next = 0;

        *new_attr = new_p;
        new_attr = &new_p->next;
    }

    return MPI_SUCCESS;
}


/* Routine to delete an attribute list */
MPI_RESULT
MPIR_Attr_delete_list(
    _In_ int handle,
    _Inout_ _Outptr_result_maybenull_ MPID_Attribute** attr
    )
{
    MPID_Attribute *p, *new_p;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    p = *attr;
    while (p)
    {
        /* delete the attribute by first executing the delete routine, if any,
           determine the the next attribute, and recover the attributes
           storage */
        new_p = p->next;

        /* For this attribute, find the delete function for the
           corresponding keyval */
        /* Still to do: capture any error returns but continue to
           process attributes */
        mpi_errno = MPIR_Call_attr_delete( handle, p );
        if( mpi_errno != MPI_SUCCESS )
        {
            break;
        }

        int in_use;
        MPIR_Keyval_release_ref( p->keyval, &in_use);
        if (!in_use)
        {
            MPIU_Handle_obj_free( &MPID_Keyval_mem, p->keyval );
        }

        MPIU_Handle_obj_free( &MPID_Attr_mem, p );

        p = new_p;
    }

    //
    // Update the list to reflect its remaining content:
    // - In case of failure, we stopped deleting attributes, and p points to
    // the attribute that failed deletion.
    // - In case of success, p is NULL.
    //
    *attr = p;
    return mpi_errno;
}


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
    )
{
    return user_function(handle, keyval, extra_state, attrib, attrib_copy, flag);
}


int
MPIAPI
MPIR_Attr_delete_c_proxy(
    MPI_Comm_delete_attr_function* user_function,
    int handle,
    int keyval,
    void* attrib,
    void* extra_state
    )
{
    return user_function(handle, keyval, attrib, extra_state);
}
