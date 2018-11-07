// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "adio.h"


/*@
    MPI_Info_create - Creates a new info object

   Output Parameter:
. info - info object created (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Info_create(
    _Out_ MPI_Info* info
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_create();

    int mpi_errno = MPI_SUCCESS;
    if( info == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "info" );
        goto fn_fail;
    }

    const MPID_Info* info_ptr = MPIU_Info_alloc_node(NULL, NULL);
    if( info_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    *info = info_ptr->handle;

    /* this is the first structure in this linked list. it is
       always kept empty. new (key,value) pairs are added after it. */

    TraceLeave_MPI_Info_create(*info);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_create %p",
            info
            )
        );
    TraceError(MPI_Info_create, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Info_delete - Deletes a (key,value) pair from info

  Input Parameters:
+ info - info object (handle)
- key - key (string)

.N NotThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N
@*/
EXTERN_C
MPI_METHOD
MPI_Info_delete(
    _In_ MPI_Info info,
    _In_z_ const char* key
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_delete(info, key);

    MPID_Info* info_ptr;
    int mpi_errno = MpiaInfoValidateHandle( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaInfoValidateKey( key );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Info* prev_ptr = info_ptr;

    /* start from info->next; the first one is a dummy node */
    for (info_ptr = info_ptr->next; info_ptr != nullptr; info_ptr = info_ptr->next)
    {
        if( CompareStringA( LOCALE_INVARIANT,
                            0,
                            info_ptr->key,
                            -1,
                            key,
                            -1 ) != CSTR_EQUAL )
        {
            prev_ptr = info_ptr;
            continue;
        }

        prev_ptr->next = info_ptr->next;
        MPIU_Info_free_node(info_ptr);
        break;
    }

    /* If curr_ptr is not defined, we never found the key */
    if( info_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INFO_NOKEY, "**infonokey %s", key);
        goto fn_fail;
    }

    TraceLeave_MPI_Info_delete();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_delete %I %s",
            info,
            key
            )
        );
    TraceError(MPI_Info_delete, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Info_dup - Returns a duplicate of the info object

Input Parameters:
. info - info object (handle)

Output Parameters:
. newinfo - duplicate of info object (handle)

.N ThreadSafeInfoRead

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Info_dup(
    _In_ MPI_Info info,
    _Out_ MPI_Info* newinfo
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_dup(info);

    const MPID_Info* info_ptr;
    int mpi_errno = MpiaInfoValidateHandle( info, const_cast<MPID_Info**>(&info_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newinfo == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newinfo" );
        goto fn_fail;
    }

    /* Note that this routine allocates info elements one at a time.
       In the multithreaded case, each allocation may need to acquire
       and release the allocation lock.  If that is ever a problem, we
       may want to add an "allocate n elements" routine and execute this
       it two steps: count and then allocate */
    /* FIXME : multithreaded */

    /* note that the first MPID_Info is a dummy entry set by MPI_Info_create */
    MPID_Info* dup_ptr = NULL;
    MPID_Info** dup_pptr = &dup_ptr;
    while(info_ptr != NULL)
    {
        *dup_pptr = MPIU_Info_alloc_node(info_ptr->key, info_ptr->value);
        if( *dup_pptr == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            MPIU_Info_free_list( dup_ptr );
            goto fn_fail;
        }

        info_ptr = info_ptr->next;
        dup_pptr = &(*dup_pptr)->next;
    }

    *newinfo = dup_ptr->handle;

    TraceLeave_MPI_Info_dup(*newinfo);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_dup %I %p",
            info,
            newinfo
            )
        );
    TraceError(MPI_Info_dup, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Info_free - Frees an info object

Input Parameter:
. info - info object to be freed (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Info_free(
    _Inout_ MPI_Info* info
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_free(*info);

    int mpi_errno;
    if( info == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "info" );
        goto fn_fail;
    }

    MPID_Info* info_ptr;
    mpi_errno = MpiaInfoValidateHandle( *info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIU_Info_free_list( info_ptr );
    *info = MPI_INFO_NULL;

    TraceLeave_MPI_Info_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_free %p",
            info
            )
        );
    TraceError(MPI_Info_free, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Info_get - Retrieves the value associated with a key

Input Parameters:
+ info - info object (handle)
. key - key (string)
- valuelen - length of value argument (integer)

Output Parameters:
+ value - value (string)
- flag - true if key defined, false if not (boolean)


.N ThreadSafeInfoRead

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
.N MPI_ERR_INFO_KEY
.N MPI_ERR_ARG
.N MPI_ERR_INFO_VALUE
@*/
EXTERN_C
MPI_METHOD
MPI_Info_get(
    _In_ MPI_Info info,
    _In_z_ const char* key,
    _In_ int valuelen,
    _When_(*flag != 0, _Out_writes_z_(valuelen)) char* value,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_get(info, key, valuelen);

    const MPID_Info* info_ptr;
    int mpi_errno = MpiaInfoValidateHandle( info, const_cast<MPID_Info**>(&info_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaInfoValidateKey( key );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( valuelen < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "valuelen", valuelen );
        goto fn_fail;
    }

    if( value == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INFO_VALUE, "**infovalnull");
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    *value = '\0';
    *flag = 0;

    /* start from info->next; the first one is a dummy node */
    for (info_ptr = info_ptr->next; info_ptr; info_ptr = info_ptr->next)
    {
        if( CompareStringA( LOCALE_INVARIANT,
                            0,
                            info_ptr->key,
                            -1,
                            key,
                            -1 ) != CSTR_EQUAL )
        {
            continue;
        }
        MPIU_Strncpy(value, info_ptr->value, valuelen);
        *flag = 1;
        break;
    }

    TraceLeave_MPI_Info_get(value, *flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_get %I %s %d %p %p",
            info,
            key,
            valuelen,
            value,
            flag
            )
        );
    TraceError(MPI_Info_get, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Info_get_nkeys - Returns the number of currently defined keys in info

Input Parameters:
. info - info object (handle)

Output Parameters:
. nkeys - number of defined keys (integer)

.N ThreadSafeInfoRead

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Info_get_nkeys(
    _In_ MPI_Info info,
    _Out_ int* nkeys
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_get_nkeys(info);

    const MPID_Info* info_ptr;
    int mpi_errno = MpiaInfoValidateHandle( info, const_cast<MPID_Info**>(&info_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( nkeys == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "nkeys" );
        goto fn_fail;
    }

    /* start from info->next; the first one is a dummy node */
    int n = 0;
    for (info_ptr = info_ptr->next; info_ptr; info_ptr = info_ptr->next)
    {
        n++;
    }

    *nkeys = n;

    TraceLeave_MPI_Info_get_nkeys(*nkeys);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_get_nkeys %I %p",
            info,
            nkeys
            )
        );
    TraceError(MPI_Info_get_nkeys, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Info_get_nthkey - Returns the nth defined key in info

Input Parameters:
+ info - info object (handle)
- n - key number (integer)

Output Parameters:
. keys - key (string).  The maximum number of characters is 'MPI_MAX_INFO_KEY'.

.N ThreadSafeInfoRead

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Info_get_nthkey(
    _In_ MPI_Info info,
    _In_range_(>=, 0) int n,
    _Out_writes_z_(MPI_MAX_INFO_KEY) char* key
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_get_nthkey(info, n);

    const MPID_Info* info_ptr;
    int mpi_errno = MpiaInfoValidateHandle( info, const_cast<MPID_Info**>(&info_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( n < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "n", n );
        goto fn_fail;
    }

    if( key == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INFO_KEY, "**infokeynull");
        goto fn_fail;
    }

    /* start from info->next; the first one is a dummy node */
    int nkeys = 0;
    for (info_ptr = info_ptr->next; info_ptr; info_ptr = info_ptr->next)
    {
        if (nkeys == n)
        {
            MPIU_Strncpy( key, info_ptr->key, MPI_MAX_INFO_KEY );
            goto fn_exit2;
        }

        nkeys++;
    }

    if( info_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**infonkey %d %d", n, nkeys);
        goto fn_fail;
    }

fn_exit2:
    TraceLeave_MPI_Info_get_nthkey(key);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_get_nthkey %I %d %p",
            info,
            n,
            key
            )
        );
    TraceError(MPI_Info_get_nthkey, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Info_get_valuelen - Retrieves the length of the value associated with
    a key

    Input Parameters:
+ info - info object (handle)
- key - key (string)

    Output Parameters:
+ valuelen - length of value argument (integer)
- flag - true if key defined, false if not (boolean)

.N ThreadSafeInfoRead

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INFO_KEY
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Info_get_valuelen(
    _In_ MPI_Info info,
    _In_z_ const char* key,
    _Out_ _Deref_out_range_(0, MPI_MAX_INFO_VAL) int* valuelen,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_get_valuelen(info, key);

    const MPID_Info* info_ptr;
    int mpi_errno = MpiaInfoValidateHandle( info, const_cast<MPID_Info**>(&info_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaInfoValidateKey( key );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( valuelen == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "valuelen" );
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    *flag = 0;
    *valuelen = 0;

    /* start from info->next; the first one is a dummy node */
    for (info_ptr = info_ptr->next; info_ptr; info_ptr = info_ptr->next)
    {
        if( CompareStringA( LOCALE_INVARIANT,
                            0,
                            info_ptr->key,
                            -1,
                            key,
                            -1 ) != CSTR_EQUAL )
        {
            continue;
        }
        *valuelen = static_cast<int>(
            MPIU_Strlen( info_ptr->value, MPI_MAX_INFO_VAL + 1 ) );
        MPIU_Assert( *valuelen <= MPI_MAX_INFO_VAL );

        *flag = 1;
        break;
    }

    TraceLeave_MPI_Info_get_valuelen(*valuelen, *flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_get_valuelen %I %s %p %p",
            info,
            key,
            valuelen,
            flag
            )
        );
    TraceError(MPI_Info_get_valuelen, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Info_set - Adds a (key,value) pair to info

Input Parameters:
+ info - info object (handle)
. key - key (string)
- value - value (string)

.N NotThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INFO_KEY
.N MPI_ERR_INFO_VALUE
.N MPI_ERR_EXHAUSTED
@*/
EXTERN_C
MPI_METHOD
MPI_Info_set(
    _In_ MPI_Info info,
    _In_z_ const char* key,
    _In_z_ const char* value
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Info_set(info, key, value);

    MPID_Info* info_ptr;
    int mpi_errno = MpiaInfoValidateHandle( info, &info_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaInfoValidateKey( key );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( value == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INFO_VALUE, "**infovalnull");
        goto fn_fail;
    }

    if( MPIU_Strlen( value, MPI_MAX_INFO_VAL + 1 ) > MPI_MAX_INFO_VAL )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_INFO_VALUE,
            "**infovallong %s %d %d",
            value,
            MPIU_Strlen(value, MPI_MAX_INFO_VAL + 1),
            MPI_MAX_INFO_VAL
            );
        goto fn_fail;
    }

    MPID_Info* last_ptr = info_ptr;

    /* start from info->next; the first one is a dummy node */
    for (info_ptr = info_ptr->next; info_ptr; info_ptr = info_ptr->next)
    {
        char* tmp;

        if( CompareStringA( LOCALE_INVARIANT,
                            0,
                            info_ptr->key,
                            -1,
                            key,
                            -1 ) != CSTR_EQUAL )
        {
            last_ptr = info_ptr;
            continue;
        }

        /* Key already present; replace value */
        tmp = MPIU_Strdup(value);
        if( tmp == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        MPIU_Free(info_ptr->value);
        info_ptr->value = tmp;
        goto fn_exit;
    }

    /* Key not present, insert value */
    info_ptr = MPIU_Info_alloc_node(key, value);
    if( info_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    last_ptr->next = info_ptr;

  fn_exit:
    TraceLeave_MPI_Info_set();
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_info_set %I %s %s",
            info,
            key,
            value
            )
        );
    TraceError(MPI_Info_set, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_File_set_info - Sets new values for the hints associated with a file

Input Parameters:
. fh - file handle (handle)
. info - info object (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_set_info(
    _In_ MPI_File fh,
    _In_ MPI_Info info
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_set_info(fh,info);

    ADIOI_FileD* file;
    int mpi_errno = MpiaFileValidateHandle( fh, &file );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Info* info_ptr;
    mpi_errno = MpiaInfoValidateHandle( info, const_cast<MPID_Info**>(&info_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = ADIO_SetInfo(file, info);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_set_info();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(file, mpi_errno);
    TraceError(MPI_File_set_info, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_info - Returns the hints for a file that are actually being used by MPI

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. info_used - info object (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_info(
    _In_ MPI_File fh,
    _Out_ MPI_Info* info_used
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_info(fh);

    ADIOI_FileD* file;
    int mpi_errno = MpiaFileValidateHandle( fh, &file );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = NMPI_Info_dup(file->info, info_used);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_get_info(*info_used);
fn_exit:
    MpiaExit();
    return  mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(file, mpi_errno);
    TraceError(MPI_File_get_info, mpi_errno);
    goto fn_exit;
}
