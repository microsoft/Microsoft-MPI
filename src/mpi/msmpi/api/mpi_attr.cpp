// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


EXTERN_C
void
MPIAPI
MPIR_Keyval_set_proxy(
    int keyval,
    MPID_Attr_copy_proxy copy_proxy,
    MPID_Attr_delete_proxy delete_proxy
    )
{
    MPID_Keyval*  keyval_ptr;
    MPID_Keyval_get_ptr( keyval, keyval_ptr );
    if(keyval_ptr == NULL)
        return;

    keyval_ptr->copyfn.proxy = copy_proxy;
    keyval_ptr->delfn.proxy = delete_proxy;
}


/*@
   MPI_Comm_create_keyval - Create a new attribute key

Input Parameters:
+ copy_fn - Copy callback function for 'keyval'
. delete_fn - Delete callback function for 'keyval'
- extra_state - Extra state for callback functions

Output Parameter:
. comm_keyval - key value for future access (integer)

Notes:
Key values are global (available for any and all communicators).

Default copy and delete functions are available.  These are
+ MPI_COMM_NULL_COPY_FN   - empty copy function
. MPI_COMM_NULL_DELETE_FN - empty delete function
- MPI_COMM_DUP_FN         - simple dup function

There are subtle differences between C and Fortran that require that the
copy_fn be written in the same language from which 'MPI_Comm_create_keyval'
is called.
This should not be a problem for most users; only programers using both
Fortran and C in the same program need to be sure that they follow this rule.

.N AttrErrReturn

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS

.seealso MPI_Comm_free_keyval
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_create_keyval(
    _In_opt_ MPI_Comm_copy_attr_function* comm_copy_attr_fn,
    _In_opt_ MPI_Comm_delete_attr_function* comm_delete_attr_fn,
    _Out_ int* comm_keyval,
    _In_opt_ void* extra_state
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_create_keyval(comm_copy_attr_fn,comm_delete_attr_fn,extra_state);

    int mpi_errno;
    if( comm_keyval == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm_keyval" );
        goto fn_fail;
    }

    MPID_Keyval* keyval_ptr;
    mpi_errno = MPID_Key_create(
        MPID_COMM,
        comm_copy_attr_fn,
        comm_delete_attr_fn,
        extra_state,
        &keyval_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Keyval_track_user_alloc();
    *comm_keyval = keyval_ptr->handle;

    TraceLeave_MPI_Comm_create_keyval(*comm_keyval);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_create_keyval %p %p %p %p",
            comm_copy_attr_fn,
            comm_delete_attr_fn,
            comm_keyval,
            extra_state
            )
        );
    TraceError(MPI_Comm_create_keyval, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_free_keyval - Frees an attribute key for communicators

Input Parameter:
. comm_keyval - Frees the integer key value (integer)

   Notes:
Key values are global (they can be used with any and all communicators)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_PERM_KEY
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_free_keyval(
    _Inout_ int* comm_keyval
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_free_keyval(*comm_keyval);

    KeyHandle hKey;

    int mpi_errno;
    if( comm_keyval == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm_keyval" );
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( *comm_keyval, MPID_COMM, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Keyval* keyval_ptr = hKey.Get();
    int in_use;
    MPIR_Keyval_release_ref( keyval_ptr, &in_use);
    if (!in_use)
    {
        MPIU_Handle_obj_free( &MPID_Keyval_mem, keyval_ptr );
    }
    *comm_keyval = MPI_KEYVAL_INVALID;
    MPID_Keyval_track_user_free();

    TraceLeave_MPI_Comm_free_keyval();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_free_keyval %p",
            comm_keyval
            )
        );
    TraceError(MPI_Comm_free_keyval, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_delete_attr - Deletes an attribute value associated with a key on
   a  communicator

Input Parameters:
+ comm - communicator to which attribute is attached (handle)
- comm_keyval - The key value of the deleted attribute (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_PERM_KEY

.seealso MPI_Comm_set_attr, MPI_Comm_create_keyval
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_delete_attr(
    _In_ MPI_Comm comm,
    _In_ int comm_keyval
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_delete_attr(comm, comm_keyval);

    KeyHandle hKey;
    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( comm_keyval, MPID_COMM, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Attr_delete( &comm_ptr->attributes, hKey.Get(), comm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Comm_delete_attr();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_delete_attr %C %d",
            comm,
            comm_keyval
            )
        );
    TraceError(MPI_Comm_delete_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_get_attr - Retrieves attribute value by key

Input Parameters:
+ comm - communicator to which attribute is attached (handle)
- keyval - key value (integer)

Output Parameters:
+ attr_value - attribute value, unless 'flag' = false
- flag -  true if an attribute value was extracted;  false if no attribute is
  associated with the key

   Notes:
    Attributes must be extracted from the same language as they were inserted
    in with 'MPI_Comm_set_attr'.  The notes for C and Fortran below explain
    why.

Notes for C:
    Even though the 'attr_value' arguement is declared as 'void *', it is
    really the address of a void pointer.  See the rationale in the
    standard for more details.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_KEYVAL
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_get_attr(
    _In_ MPI_Comm comm,
    _In_ int comm_keyval,
    _When_(*flag != 0, _Out_) void* attribute_val,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_get_attr(comm, comm_keyval);

    static PreDefined_attrs attr_copy;    /* Used to provide a copy of the
                                             predefined attributes */

    KeyHandle hKey;
    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( comm_keyval, MPID_COMM, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( attribute_val == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "attribute_val" );
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    if (HANDLE_GET_TYPE(comm_keyval) == HANDLE_TYPE_BUILTIN)
    {
        *flag = MPID_Comm_get_attr(
            comm_ptr,
            comm_keyval,
            static_cast<void**>(attribute_val)
            );
    }
    else
    {
        *flag = MPID_Attr_get(
            comm_ptr->attributes,
            hKey.Get(),
            static_cast<void**>(attribute_val)
            );
    }

    TraceLeave_MPI_Comm_get_attr(attribute_val,*flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_get_attr %C %d %p %p",
            comm,
            comm_keyval,
            attribute_val,
            flag
            )
        );
    TraceError(MPI_Comm_get_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_set_attr - Stores attribute value associated with a key

Input Parameters:
+ comm - communicator to which attribute will be attached (handle)
. keyval - key value, as returned by  'MPI_Comm_create_keyval' (integer)
- attribute_val - attribute value

Notes:
Values of the permanent attributes 'MPI_TAG_UB', 'MPI_HOST', 'MPI_IO',
'MPI_WTIME_IS_GLOBAL', 'MPI_UNIVERSE_SIZE', 'MPI_LASTUSEDCODE', and
'MPI_APPNUM' may not be changed.

The datatype of the attribute value depends on whether C, C++, or Fortran
is being used.
In C and C++, an attribute value is a pointer ('void *'); in Fortran, it is an
address-sized integer.

If an attribute is already present, the delete function (specified when the
corresponding keyval was created) will be called.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_KEYVAL
.N MPI_ERR_PERM_KEY

.seealso MPI_Comm_create_keyval, MPI_Comm_delete_attr
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_set_attr(
    _In_ MPI_Comm comm,
    _In_ int comm_keyval,
    _In_opt_ void* attribute_val
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_set_attr(comm, comm_keyval,attribute_val);

    KeyHandle hKey;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( comm_keyval, MPID_COMM, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Attr_set(
        &comm_ptr->attributes,
        hKey.Get(),
        comm,
        attribute_val
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Comm_set_attr();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_set_attr %C %d %p",
            comm,
            comm_keyval,
            attribute_val
            )
        );
    TraceError(MPI_Comm_set_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_create_keyval - Create an attribute keyval for MPI datatypes

   Input Parameters:
+ type_copy_attr_fn - copy callback function for type_keyval (function)
. type_delete_attr_fn - delete callback function for type_keyval (function)
- extra_state - extra state for callback functions

   Output Parameter:
. type_keyval - key value for future access (integer)

Notes:

   Default copy and delete functions are available.  These are
+ MPI_TYPE_NULL_COPY_FN   - empty copy function
. MPI_TYPE_NULL_DELETE_FN - empty delete function
- MPI_TYPE_DUP_FN         - simple dup function

.N AttrErrReturn

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Type_create_keyval(
    _In_opt_ MPI_Type_copy_attr_function* type_copy_attr_fn,
    _In_opt_ MPI_Type_delete_attr_function* type_delete_attr_fn,
    _Out_ int* type_keyval,
    _In_opt_ void* extra_state
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_create_keyval(type_copy_attr_fn,type_delete_attr_fn,extra_state);

    int mpi_errno = MPI_SUCCESS;
    if( type_keyval == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "type_keyval" );
        goto fn_fail;
    }

    MPID_Keyval* keyval_ptr;
    mpi_errno = MPID_Key_create(
        MPID_DATATYPE,
        type_copy_attr_fn,
        type_delete_attr_fn,
        extra_state,
        &keyval_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Keyval_track_user_alloc();
    *type_keyval = keyval_ptr->handle;

    TraceLeave_MPI_Type_create_keyval(*type_keyval);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_create_keyval %p %p %p %p",
            type_copy_attr_fn,
            type_delete_attr_fn,
            type_keyval,
            extra_state
            )
        );
    TraceError(MPI_Type_create_keyval, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_free_keyval - Frees an attribute key for datatypes

Input Parameter:
. keyval - Frees the integer key value (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
.N MPI_ERR_KEYVAL
@*/
EXTERN_C
MPI_METHOD
MPI_Type_free_keyval(
    _Inout_ int* type_keyval
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_free_keyval(*type_keyval);

    KeyHandle hKey;

    int mpi_errno = MPI_SUCCESS;
    if( type_keyval == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "type_keyval" );
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( *type_keyval, MPID_DATATYPE, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Keyval* keyval_ptr = hKey.Get();
    int in_use;
    MPIR_Keyval_release_ref( keyval_ptr, &in_use);
    if (!in_use)
    {
        MPIU_Handle_obj_free( &MPID_Keyval_mem, keyval_ptr );
    }
    *type_keyval = MPI_KEYVAL_INVALID;
    MPID_Keyval_track_user_free();

    TraceLeave_MPI_Type_free_keyval();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_free_keyval %p",
            type_keyval
            )
        );
    TraceError(MPI_Type_free_keyval, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_set_attr - Stores attribute value associated with a key

Input Parameters:
+ datatype - MPI Datatype to which attribute will be attached (handle)
. keyval - key value, as returned by  'MPI_Type_create_keyval' (integer)
- attribute_val - attribute value

Notes:

The datatype of the attribute value depends on whether C or Fortran is being used.
In C, an attribute value is a pointer ('void *'); in Fortran, it is an
address-sized integer.

If an attribute is already present, the delete function (specified when the
corresponding keyval was created) will be called.
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_TYPE
.N MPI_ERR_KEYVAL
@*/
EXTERN_C
MPI_METHOD
MPI_Type_set_attr(
    _In_ MPI_Datatype datatype,
    _In_ int type_keyval,
    _In_opt_ void* attribute_val
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_set_attr(datatype,type_keyval,attribute_val);

    TypeHandle hType;
    KeyHandle hKey;

    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( type_keyval, MPID_DATATYPE, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Attr_set(
        &hType.Get()->attributes,
        hKey.Get(),
        datatype,
        attribute_val
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Type_set_attr();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_set_attr %D %d %p",
            datatype,
            type_keyval,
            attribute_val
            )
        );
    TraceError(MPI_Type_set_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_delete_attr - Deletes an attribute value associated with a key on
   a datatype

   Input Parameters:
+  datatype - MPI datatype to which attribute is attached (handle)
-  type_keyval - The key value of the deleted attribute (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
.N MPI_ERR_KEYVAL
@*/
EXTERN_C
MPI_METHOD
MPI_Type_delete_attr(
    _In_ MPI_Datatype datatype,
    _In_ int type_keyval
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_delete_attr(datatype, type_keyval);

    TypeHandle hType;
    KeyHandle hKey;

    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( type_keyval, MPID_DATATYPE, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Attr_delete( &hType.Get()->attributes, hKey.Get(), datatype );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Type_delete_attr();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_delete_attr %D %d",
            datatype,
            type_keyval
            )
        );
    TraceError(MPI_Type_delete_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Type_get_attr - Retrieves attribute value by key

   Input Parameters:
+ datatype - datatype to which the attribute is attached (handle)
- type_keyval - key value (integer)

   Output Parameters:
+ attribute_val - attribute value, unless flag = false
- flag - false if no attribute is associated with the key (logical)

   Notes:
    Attributes must be extracted from the same language as they were inserted
    in with 'MPI_Type_set_attr'.  The notes for C and Fortran below explain
    why.

Notes for C:
    Even though the 'attr_value' arguement is declared as 'void *', it is
    really the address of a void pointer.  See the rationale in the
    standard for more details.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_KEYVAL
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Type_get_attr(
    _In_ MPI_Datatype datatype,
    _In_ int type_keyval,
    _When_(*flag != 0, _Out_) void* attribute_val,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Type_get_attr(datatype, type_keyval);

    TypeHandle hType;
    KeyHandle hKey;

    int mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( type_keyval, MPID_DATATYPE, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( attribute_val == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "attribute_val" );
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    *flag = MPID_Attr_get(
        hType.Get()->attributes,
        hKey.Get(),
        static_cast<void**>(attribute_val)
        );

    TraceLeave_MPI_Type_get_attr((*(void**)attribute_val),*flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_type_get_attr %D %d %p %p",
            datatype,
            type_keyval,
            attribute_val,
            flag
            )
        );
    TraceError(MPI_Type_get_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_create_keyval - Create an attribute keyval for MPI window objects

   Input Parameters:
+ win_copy_attr_fn - copy callback function for win_keyval (function)
. win_delete_attr_fn - delete callback function for win_keyval (function)
- extra_state - extra state for callback functions

   Output Parameter:
. win_keyval - key value for future access (integer)

   Notes:
   Default copy and delete functions are available.  These are
+ MPI_WIN_NULL_COPY_FN   - empty copy function
. MPI_WIN_NULL_DELETE_FN - empty delete function
- MPI_WIN_DUP_FN         - simple dup function

.N AttrErrReturn

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_create_keyval(
    _In_opt_ MPI_Win_copy_attr_function* win_copy_attr_fn,
    _In_opt_ MPI_Win_delete_attr_function* win_delete_attr_fn,
    _Out_ int* win_keyval,
    _In_opt_ void* extra_state
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_create_keyval(win_copy_attr_fn,win_delete_attr_fn,extra_state);

    int mpi_errno = MPI_SUCCESS;
    if( win_keyval == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "win_keyval" );
        goto fn_fail;
    }

    MPID_Keyval* keyval_ptr;
    mpi_errno = MPID_Key_create(
        MPID_WIN,
        win_copy_attr_fn,
        win_delete_attr_fn,
        extra_state,
        &keyval_ptr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Keyval_track_user_alloc();
    *win_keyval = keyval_ptr->handle;

    TraceLeave_MPI_Win_create_keyval(*win_keyval);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_create_keyval %p %p %p %p",
            win_copy_attr_fn,
            win_delete_attr_fn,
            win_keyval,
            extra_state
            )
        );
    TraceError(MPI_Win_create_keyval, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_free_keyval - Frees an attribute key for MPI RMA windows

   Input Parameter:
. win_keyval - key value (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
.N MPI_ERR_KEYVAL
@*/
EXTERN_C
MPI_METHOD
MPI_Win_free_keyval(
    _Inout_ int* win_keyval
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_free_keyval(*win_keyval);

    KeyHandle hKey;

    int mpi_errno = MPI_SUCCESS;
    if( win_keyval == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "win_keyval" );
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( *win_keyval, MPID_WIN, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Keyval* keyval_ptr = hKey.Get();
    int in_use;
    MPIR_Keyval_release_ref( keyval_ptr, &in_use);
    if (!in_use)
    {
        MPIU_Handle_obj_free( &MPID_Keyval_mem, keyval_ptr );
    }
    *win_keyval = MPI_KEYVAL_INVALID;
    MPID_Keyval_track_user_free();

    TraceLeave_MPI_Win_free_keyval();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_free_keyval %p",
            win_keyval
            )
        );
    TraceError(MPI_Win_free_keyval, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_set_attr - Stores attribute value associated with a key

Input Parameters:
+ win - MPI window object to which attribute will be attached (handle)
. keyval - key value, as returned by  'MPI_Win_create_keyval' (integer)
- attribute_val - attribute value

Notes:

The datatype of the attribute value depends on whether C or Fortran is being used.
In C, an attribute value is a pointer ('void *'); in Fortran, it is an
address-sized integer.

If an attribute is already present, the delete function (specified when the
corresponding keyval was created) will be called.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_KEYVAL
@*/
EXTERN_C
MPI_METHOD
MPI_Win_set_attr(
    _In_ MPI_Win win,
    _In_ int win_keyval,
    _In_opt_ void* attribute_val
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_set_attr(win,win_keyval,attribute_val);

    KeyHandle hKey;
    
    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( win_keyval, MPID_WIN, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Attr_set(
        &win_ptr->attributes,
        hKey.Get(),
        win,
        attribute_val
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_set_attr();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_set_attr %W %d %p",
            win,
            win_keyval,
            attribute_val
            )
        );
    TraceError(MPI_Win_set_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_delete_attr - Deletes an attribute value associated with a key on
   a datatype

   Input Parameters:
+ win - window from which the attribute is deleted (handle)
- win_keyval - key value (integer)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_KEYVAL
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_delete_attr(
    _In_ MPI_Win win,
    _In_ int win_keyval
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_delete_attr(win, win_keyval);

    KeyHandle hKey;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( win_keyval, MPID_WIN, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidateNotPermanent( hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPID_Attr_delete( &win_ptr->attributes, hKey.Get(), win );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Win_delete_attr();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_delete_attr %W %d",
            win,
            win_keyval
            )
        );
    TraceError(MPI_Win_delete_attr, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_get_attr - Get attribute cached on an MPI window object

   Input Parameters:
+ win - window to which the attribute is attached (handle)
- win_keyval - key value (integer)

   Output Parameters:
+ attribute_val - attribute value, unless flag is false
- flag - false if no attribute is associated with the key (logical)

   Notes:
   The following attributes are predefined for all MPI Window objects\:

+ MPI_WIN_BASE - window base address.
. MPI_WIN_SIZE - window size, in bytes.
. MPI_WIN_DISP_UNIT - displacement unit associated with the window.
. MPI_WIN_CREATE_FLAVOR - how the window was created.
- MPI_WIN_MODEL - memory model for the window.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_KEYVAL
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_get_attr(
    _In_ MPI_Win win,
    _In_ int win_keyval,
    _When_(*flag != 0, _Out_) void* attribute_val,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_get_attr(win, win_keyval);

    KeyHandle hKey;

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaKeyvalValidate( win_keyval, MPID_WIN, &hKey );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( attribute_val == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "attribute_val" );
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    if (HANDLE_GET_TYPE(win_keyval) == HANDLE_TYPE_BUILTIN)
    {
        *flag = MPID_Win_get_attr(
            win_ptr,
            win_keyval,
            static_cast<void**>(attribute_val)
            );
    }
    else
    {
        *flag = MPID_Attr_get(
            win_ptr->attributes,
            hKey.Get(),
            static_cast<void**>(attribute_val)
            );
    }

    TraceLeave_MPI_Win_get_attr(attribute_val,*flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_get_attr %W %d %p %p",
            win,
            win_keyval,
            attribute_val,
            flag
            )
        );
    TraceError(MPI_Win_get_attr, mpi_errno);
    goto fn_exit;
}


/*D

MPI_DUP_FN - A function to simple-mindedly copy attributes

D*/
EXTERN_C
MPI_METHOD
MPIR_Dup_fn(
    _In_ MPI_Comm /*oldcomm*/,
    _In_ int /*keyval*/,
    _In_opt_ void* /*extra_state*/,
    _In_opt_ void* attribute_val_in,
    _Out_ void* attribute_val_out,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    /* Set attr_out, the flag and return success */
    *static_cast<void**>(attribute_val_out) = attribute_val_in;
    (*flag) = 1;
    return (MPI_SUCCESS);
}
