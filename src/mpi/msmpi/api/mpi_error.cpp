// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "adio.h"
#include "adio_extern.h"


/*@
  MPI_Errhandler_free - Frees an MPI-style errorhandler

Input Parameter:
. errhandler - MPI error handler (handle).  Set to 'MPI_ERRHANDLER_NULL' on
exit.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Errhandler_free(
    _Inout_ MPI_Errhandler* errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Errhandler_free(*errhandler);

    int mpi_errno;
    if( errhandler == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errhandler" );
        goto fn_fail;
    }

    MPID_Errhandler *errhan_ptr;
    mpi_errno = MpiaErrhandlerValidateHandle( *errhandler, &errhan_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    int in_use;
    MPIR_Errhandler_release_ref( errhan_ptr,&in_use);
    if (!in_use)
    {
        MPIU_Handle_obj_free( &MPID_Errhandler_mem, errhan_ptr );
    }
    *errhandler = MPI_ERRHANDLER_NULL;

    TraceLeave_MPI_Errhandler_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_errhandler_free %p",
            errhandler
            )
        );
    TraceError(MPI_Errhandler_free, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Error_class - Converts an error code into an error class

Input Parameter:
. errorcode - Error code returned by an MPI routine

Output Parameter:
. errorclass - Error class associated with 'errorcode'

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Error_class(
    _In_ int errorcode,
    _Out_ int* errorclass
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Error_class(errorcode);

    int mpi_errno = MPI_SUCCESS;

    /* Validate parameters, especially handles needing to be converted */
    if( errorclass == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errorclass" );
        goto fn_fail;
    }

    /* We include the dynamic bit because this is needed to fully
       describe the dynamic error classes */
    *errorclass = errorcode & (ERROR_CLASS_MASK | ERROR_DYN_FLAG);

    TraceLeave_MPI_Error_class(*errorclass);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_error_class %d %p",
            errorcode,
            errorclass
            )
        );
    TraceError(MPI_Error_class, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Error_string - Return a string for a given error code

Input Parameters:
. errorcode - Error code returned by an MPI routine or an MPI error class

Output Parameter:
+ string - Text that corresponds to the errorcode
- resultlen - Length of string

Notes:  Error codes are the values return by MPI routines (in C) or in the
'ierr' argument (in Fortran).  These can be converted into error classes
with the routine 'MPI_Error_class'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Error_string(
    _In_ int errorcode,
    _Out_writes_z_(MPI_MAX_ERROR_STRING) char* string,
    _Out_ int* resultlen
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Error_string(errorcode);

    int mpi_errno = MPI_SUCCESS;

    /* Validate parameters, especially handles needing to be converted */
    if( string == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "string" );
        goto fn_fail;
    }
    if( resultlen == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "resultlen" );
        goto fn_fail;
    }

    MPIR_Err_get_string( errorcode, string, MPI_MAX_ERROR_STRING);
    *resultlen = static_cast<int>( MPIU_Strlen( string, MPI_MAX_ERROR_STRING ) );

    TraceLeave_MPI_Error_string(*resultlen,string);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_error_string %d %s %p",
            errorcode,
            string,
            resultlen
            )
        );
    TraceError(MPI_Error_string, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_create_errhandler - Create a communicator error handler

   Input Parameter:
. function - user defined error handling procedure (function)

   Output Parameter:
. errhandler - MPI error handler (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_create_errhandler(
    _In_ MPI_Comm_errhandler_fn* function,
    _Out_ MPI_Errhandler* errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_create_errhandler(function);

    int mpi_errno = MPI_SUCCESS;
    MPID_Errhandler *errhan_ptr;

    /* Validate parameters, especially handles needing to be converted */
    if( function == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "function" );
        goto fn_fail;
    }
    if( errhandler == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errhandler" );
        goto fn_fail;
    }

    errhan_ptr = (MPID_Errhandler *)MPIU_Handle_obj_alloc( &MPID_Errhandler_mem );
    if( errhan_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    *errhandler          = errhan_ptr->handle;
    errhan_ptr->kind     = MPID_COMM;
    errhan_ptr->errfn.comm.user_function = function;
    errhan_ptr->errfn.comm.proxy = MPIR_Comm_errhandler_c_proxy;

    TraceLeave_MPI_Comm_create_errhandler(*errhandler);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_create_errhandler %p %p",
            function,
            errhandler
            )
        );
    TraceLeave_MPI_Comm_create_errhandler(mpi_errno);
    goto fn_exit;
}


EXTERN_C
void
MPIAPI
MPIR_Comm_errhandler_set_proxy(MPI_Errhandler handler, MPID_Comm_errhandler_proxy* proxy)
{
    MPID_Errhandler* errhandler;
    if( MpiaErrhandlerValidateHandle( handler, &errhandler ) != MPI_SUCCESS )
    {
        return;
    }

    if( errhandler->kind != MPID_COMM )
    {
        MPIU_Assert( errhandler->kind == MPID_COMM );
        return;
    }

    errhandler->errfn.comm.proxy = proxy;
}


/*@
   MPI_Comm_get_errhandler - Get the error handler attached to a communicator

   Input Parameter:
. comm - communicator (handle)

   Output Parameter:
. errhandler - handler currently associated with communicator (handle)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_get_errhandler(
    _In_ MPI_Comm comm,
    _Out_ MPI_Errhandler* errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_get_errhandler(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( errhandler == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errhandler" );
        goto fn_fail;
    }

    /* Check for default error handler */
    if (!comm_ptr->errhandler)
    {
        *errhandler = MPI_ERRORS_ARE_FATAL;
    }
    else
    {
        *errhandler = comm_ptr->errhandler->handle;
        MPIR_Errhandler_add_ref(comm_ptr->errhandler);
    }

    TraceLeave_MPI_Comm_get_errhandler(*errhandler);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_get_errhandler %C %p",
            comm,
            errhandler
            )
        );
    TraceError(MPI_Comm_get_errhandler, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_set_errhandler - Set the error handler for a communicator

   Input Parameters:
+ comm - communicator (handle)
- errhandler - new error handler for communicator (handle)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_OTHER

.seealso MPI_Comm_get_errhandler, MPI_Comm_call_errhandler
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_set_errhandler(
    _In_ MPI_Comm comm,
    _In_ MPI_Errhandler errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_set_errhandler(comm, errhandler);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Errhandler *errhan_ptr;
    mpi_errno = MpiaErrhandlerValidateHandle( errhandler, &errhan_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( errhandler != MPI_ERRORS_ARE_FATAL && errhandler != MPI_ERRORS_RETURN )
    {
        /* Also check for a valid errhandler kind */
        if (errhan_ptr->kind != MPID_COMM)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**errhandnotcomm" );
            goto fn_fail;
        }
    }

    /* We don't bother with the case where the errhandler is NULL;
       in this case, the error handler was the original, MPI_ERRORS_ARE_FATAL,
       which is builtin and can never be freed. */
    if (comm_ptr->errhandler != NULL)
    {
        if (HANDLE_GET_TYPE(errhandler) != HANDLE_TYPE_BUILTIN)
        {
            int in_use;

            MPIR_Errhandler_release_ref(comm_ptr->errhandler,&in_use);
            if (!in_use)
            {
                MPID_Errhandler_free( comm_ptr->errhandler );
            }
        }
    }

    MPIR_Errhandler_add_ref(errhan_ptr);
    comm_ptr->errhandler = errhan_ptr;

    TraceLeave_MPI_Comm_set_errhandler();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_set_errhandler %C %E",
            comm, errhandler
            )
        );
    TraceError(MPI_Comm_set_errhandler, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_call_errhandler - Call the error handler installed on a
   communicator

 Input Parameters:
+ comm - communicator with error handler (handle)
- errorcode - error code (integer)

 Note:
 Assuming the input parameters are valid, when the error handler is set to
 MPI_ERRORS_RETURN, this routine will always return MPI_SUCCESS.

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_call_errhandler(
    _In_ MPI_Comm comm,
    _In_ int errorcode
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_call_errhandler(comm, errorcode);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Check for predefined error handlers */
    if (!comm_ptr->errhandler ||
        comm_ptr->errhandler->handle == MPI_ERRORS_ARE_FATAL)
    {
        mpi_errno = MPIR_Err_return_comm( comm_ptr, "MPI_Comm_call_errhandler", errorcode );
        goto fn_exit;
    }

    if (comm_ptr->errhandler->handle == MPI_ERRORS_RETURN)
    {
        /* MPI_ERRORS_RETURN should always return MPI_SUCCESS */
        goto fn_exit;
    }

    /* Process any user-defined error handling function */
    comm_ptr->errhandler->errfn.comm.proxy(
        comm_ptr->errhandler->errfn.comm.user_function,
        &comm_ptr->handle,
        &errorcode
        );

  fn_exit:
    TraceLeave_MPI_Comm_call_errhandler(errorcode);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_call_errhandler %C %d",
            comm,
            errorcode
            )
        );
    TraceError(MPI_Comm_call_errhandler, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Win_create_errhandler - Create an error handler for use with MPI window
   objects

   Input Parameter:
. function - user defined error handling procedure (function)

   Output Parameter:
. errhandler - MPI error handler (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_create_errhandler(
    _In_ MPI_Win_errhandler_fn* function,
    _Out_ MPI_Errhandler* errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_create_errhandler(function);

    int mpi_errno = MPI_SUCCESS;
    MPID_Errhandler *errhan_ptr;

    /* Validate parameters, especially handles needing to be converted */
    if( function == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "function" );
        goto fn_fail;
    }
    if( errhandler == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errhandler" );
        goto fn_fail;
    }

    errhan_ptr = (MPID_Errhandler *)MPIU_Handle_obj_alloc( &MPID_Errhandler_mem );
    if( errhan_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    *errhandler          = errhan_ptr->handle;
    errhan_ptr->kind     = MPID_WIN;
    errhan_ptr->errfn.win.user_function = function;
    errhan_ptr->errfn.win.proxy = MPIR_Win_errhandler_c_proxy;

    TraceLeave_MPI_Win_create_errhandler(*errhandler);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_create_errhandler %p %p",
            function,
            errhandler
            )
        );
    TraceError(MPI_Win_create_errhandler, mpi_errno);
    goto fn_exit;
}


EXTERN_C
void
MPIAPI
MPIR_Win_errhandler_set_proxy(MPI_Errhandler handler, MPID_Win_errhandler_proxy* proxy)
{
    MPID_Errhandler* errhandler;
    if( MpiaErrhandlerValidateHandle( handler, &errhandler ) != MPI_SUCCESS )
    {
        return;
    }

    if( errhandler->kind != MPID_WIN )
    {
        MPIU_Assert( errhandler->kind == MPID_WIN );
        return;
    }

    errhandler->errfn.win.proxy = proxy;
}


/*@
   MPI_Win_call_errhandler - Call the error handler installed on a
   window object

   Input Parameters:
+ win - window with error handler (handle)
- errorcode - error code (integer)

 Note:
 Assuming the input parameters are valid, when the error handler is set to
 MPI_ERRORS_RETURN, this routine will always return MPI_SUCCESS.

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_call_errhandler(
    _In_ MPI_Win win,
    _In_ int errorcode
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_call_errhandler(win, errorcode);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (!win_ptr->errhandler ||
        win_ptr->errhandler->handle == MPI_ERRORS_ARE_FATAL)
    {
        mpi_errno = MPIR_Err_return_win( win_ptr, FCNAME, errorcode );
        goto fn_exit;
    }

    if (win_ptr->errhandler->handle == MPI_ERRORS_RETURN)
    {
        /* MPI_ERRORS_RETURN should always return MPI_SUCCESS */
        goto fn_exit;
    }

    /* Process any user-defined error handling function */
    win_ptr->errhandler->errfn.win.proxy(
        win_ptr->errhandler->errfn.win.user_function,
        &win_ptr->handle,
        &errorcode
        );

  fn_exit:
    TraceLeave_MPI_Win_call_errhandler(errorcode);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_call_errhandler %W %d",
            win,
            errorcode
            )
        );
    TraceError(MPI_Win_call_errhandler, mpi_errno);
    goto fn_exit1;
}


/*@
   MPI_Win_get_errhandler - Get the error handler for the MPI RMA window

   Input Parameter:
. win - window (handle)

   Output Parameter:
. errhandler - error handler currently associated with window (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Win_get_errhandler(
    _In_ MPI_Win win,
    _Out_ MPI_Errhandler* errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_get_errhandler(win);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( errhandler == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errhandler" );
        goto fn_fail;
    }

    if (win_ptr->errhandler)
    {
        *errhandler = win_ptr->errhandler->handle;
        MPIR_Errhandler_add_ref(win_ptr->errhandler);
    }
    else
    {
        /* Use the default */
        *errhandler = MPI_ERRORS_ARE_FATAL;
    }

    TraceLeave_MPI_Win_get_errhandler(*errhandler);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_get_errhandler %W %p",
            win,
            errhandler
            )
        );
    TraceError(MPI_Win_get_errhandler, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Win_set_errhandler - Set window error handler

   Input Parameters:
+ win - window (handle)
- errhandler - new error handler for window (handle)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_WIN
@*/
EXTERN_C
MPI_METHOD
MPI_Win_set_errhandler(
    _In_ MPI_Win win,
    _In_ MPI_Errhandler errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Win_set_errhandler(win, errhandler);

    MPID_Win *win_ptr;
    int mpi_errno = MpiaWinValidateHandle( win, &win_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Errhandler *errhan_ptr;
    mpi_errno = MpiaErrhandlerValidateHandle( errhandler, &errhan_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( errhandler != MPI_ERRORS_ARE_FATAL && errhandler != MPI_ERRORS_RETURN )
    {
        /* Also check for a valid errhandler kind */
        if (errhan_ptr->kind != MPID_WIN)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**errhandnotwin" );
            goto fn_fail;
        }
    }

    if (win_ptr->errhandler != NULL)
    {
        if (HANDLE_GET_TYPE(errhandler) != HANDLE_TYPE_BUILTIN)
        {
            int  in_use;
            MPIR_Errhandler_release_ref(win_ptr->errhandler,&in_use);
            if (!in_use)
            {
                MPID_Errhandler_free( win_ptr->errhandler );
            }
        }
    }

    MPIR_Errhandler_add_ref(errhan_ptr);
    win_ptr->errhandler = errhan_ptr;

    TraceLeave_MPI_Win_set_errhandler();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_win(
        win_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_win_set_errhandler %W %E",
            win,
            errhandler
            )
        );
    TraceError(MPI_Win_set_errhandler, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_File_call_errhandler - Call the error handler installed on a
   file

   Input Parameters:
+ fh - MPI file with error handler (handle)
- errorcode - error code (integer)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_FILE
@*/
EXTERN_C
MPI_METHOD
MPI_File_call_errhandler(
    _In_ MPI_File file,
    _In_ int errorcode
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_call_errhandler(file, errorcode);

    MPID_Errhandler *e;
    MPI_Errhandler eh = MPIR_ROMIO_Get_file_errhand( file );

    int mpi_errno = MpiaErrhandlerValidateHandle( eh, &e );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_exit;
    }

    if (!e || e->handle == MPI_ERRORS_RETURN)
    {
        /* MPI_ERRORS_RETURN should always return MPI_SUCCESS */
        goto fn_exit;
    }

    if (e->handle == MPI_ERRORS_ARE_FATAL)
    {
        mpi_errno = MPIR_Err_return_comm( NULL, "MPI_File_call_errhandler", errorcode );
        goto fn_exit;
    }

    /* Process any user-defined error handling function */
    e->errfn.file.proxy(
        e->errfn.file.user_function,
        &file,
        &errorcode
        );

    TraceLeave_MPI_File_call_errhandler(errorcode);

  fn_exit:
    MpiaExit();
    return mpi_errno;
}


/*@
   MPI_File_create_errhandler - Create a file error handler

   Input Parameter:
. function - user defined error handling procedure (function)

   Output Parameter:
. errhandler - MPI error handler (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_File_create_errhandler(
    _In_ MPI_File_errhandler_fn* function,
    _Out_ MPI_Errhandler* errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_create_errhandler(function);

    int mpi_errno = MPI_SUCCESS;
    MPID_Errhandler *errhan_ptr;

    /* Validate parameters, especially handles needing to be converted */
    if( function == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "function" );
        goto fn_fail;
    }
    if( errhandler == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errhandler" );
        goto fn_fail;
    }

    errhan_ptr = (MPID_Errhandler *)MPIU_Handle_obj_alloc( &MPID_Errhandler_mem );
    if( errhan_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    *errhandler          = errhan_ptr->handle;
    errhan_ptr->kind     = MPID_FILE;
    errhan_ptr->errfn.file.user_function = function;
    errhan_ptr->errfn.file.proxy = MPIR_File_errhandler_c_proxy;

    TraceLeave_MPI_File_create_errhandler(*errhandler);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIO_Err_return_file(
        MPI_FILE_NULL,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_file_create_errhandler %p %p",
            function,
            errhandler
            )
        );
    TraceError(MPI_File_create_errhandler, mpi_errno);
    goto fn_exit;
}


EXTERN_C
void
MPIAPI
MPIR_File_errhandler_set_proxy(MPI_Errhandler handler, MPID_File_errhandler_proxy* proxy)
{
    MPID_Errhandler* errhandler;
    if( MpiaErrhandlerValidateHandle( handler, &errhandler ) != MPI_SUCCESS )
    {
        return;
    }

    if( errhandler->kind != MPID_FILE )
    {
        MPIU_Assert( errhandler->kind == MPID_FILE );
        return;
    }

    errhandler->errfn.file.proxy = proxy;
}


/*@
   MPI_File_get_errhandler - Get the error handler attached to a file

   Input Parameter:
. file - MPI file (handle)

   Output Parameter:
. errhandler - handler currently associated with file (handle)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_errhandler(
    _In_ MPI_File file,
    _Out_ MPI_Errhandler* errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_errhandler(file);

    int mpi_errno;

    if( errhandler == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errhandler" );
        goto fn_fail;
    }

    MPI_Errhandler eh = MPIR_ROMIO_Get_file_errhand( file );

    MPID_Errhandler *e;
    mpi_errno = MpiaErrhandlerValidateHandle( eh, &e );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_exit;
    }

OACR_WARNING_SUPPRESS(26500, "Suppress false anvil warning.")
    MPIR_Errhandler_add_ref( e );
    *errhandler = e->handle;

    TraceLeave_MPI_File_get_errhandler(*errhandler);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIO_Err_return_file(
        file,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_file_get_errhandler %F %p",
            file,
            errhandler
            )
        );

    TraceError(MPI_File_get_errhandler, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_File_set_errhandler - Set the error handler for an MPI file

   Input Parameters:
+ file - MPI file (handle)
- errhandler - new error handler for file (handle)

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_File_set_errhandler(
    _In_ MPI_File file,
    _In_ MPI_Errhandler errhandler
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_set_errhandler(file, errhandler);

    MPID_Errhandler *errhan_ptr;

    int mpi_errno = MpiaErrhandlerValidateHandle( errhandler, &errhan_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( errhandler != MPI_ERRORS_ARE_FATAL && errhandler != MPI_ERRORS_RETURN )
    {
        /* Also check for a valid errhandler kind */
        if (errhan_ptr->kind != MPID_FILE)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**errhandnotfile" );
            goto fn_fail;
        }
    }

    if (HANDLE_GET_TYPE(errhandler) != HANDLE_TYPE_BUILTIN)
    {
        MPI_Errhandler old_errhandler = MPIR_ROMIO_Get_file_errhand( file );

        MPID_Errhandler *old_errhandler_ptr;
        mpi_errno = MpiaErrhandlerValidateHandle( old_errhandler, &old_errhandler_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if (old_errhandler_ptr)
        {
            int in_use;
            MPIR_Errhandler_release_ref(old_errhandler_ptr,&in_use);
            if (!in_use)
            {
                MPID_Errhandler_free( old_errhandler_ptr );
            }
        }
    }

    MPIR_ROMIO_Set_file_errhand( file, errhandler );
    MPIR_Errhandler_add_ref(errhan_ptr);

    TraceLeave_MPI_File_set_errhandler();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIO_Err_return_file(
        file,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_file_set_errhandler %F %E",
            file,
            errhandler
            )
        );

    TraceError(MPI_File_set_errhandler, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Add_error_class - Add an MPI error class to the known classes

   Output Parameter:
.  errorclass - New error class

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Add_error_class(
    _Out_ int* errorclass
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Add_error_class();

    int mpi_errno = MPI_SUCCESS;
    int new_class;

    if( errorclass == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errorclass" );
        goto fn_fail;
    }

    new_class = MPIR_Err_add_class( );
    if(new_class < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**noerrclasses");
        goto fn_fail;
    }

    *errorclass = new_class;
    if(new_class > Mpi.Attributes.lastusedcode)
    {
        Mpi.Attributes.lastusedcode = new_class;
    }

    TraceLeave_MPI_Add_error_class(*errorclass);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_add_error_class %p",
            errorclass
            )
        );
    TraceError(MPI_Add_error_class, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Add_error_code - Add and MPI error code to an MPI error class

   Input Parameter:
.  errorclass - Error class to add an error code.

   Output Parameter:
.  errorcode - New error code for this error class.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_OTHER
@*/
EXTERN_C
MPI_METHOD
MPI_Add_error_code(
    _In_ int errorclass,
    _Out_ int* errorcode
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Add_error_code(errorclass);

    int mpi_errno = MPI_SUCCESS;
    int new_code;

    /* Validate parameters, especially handles needing to be converted */
    /* FIXME: verify that errorclass is a dynamic class */
    if( errorcode == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "errorcode" );
        goto fn_fail;
    }

    new_code = MPIR_Err_add_code( errorclass );
    if(new_code < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**noerrcodes");
        goto fn_fail;
    }

    *errorcode = new_code;

    TraceLeave_MPI_Add_error_code(*errorcode);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_add_error_code %d %p",
            errorclass,
            errorcode
            )
        );
    TraceError(MPI_Add_error_code, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Add_error_string - Associates an error string with an MPI error code or
   class

   Input Parameters:
+ errorcode - error code or class (integer)
- string - text corresponding to errorcode (string)

   Notes:
The string must be no more than 'MPI_MAX_ERROR_STRING' characters long.
The length of the string is as defined in the calling language.
The length of the string does not include the null terminator in C or C++.
Note that the string is 'const' even though the MPI standard does not
specify it that way.

According to the MPI-2 standard, it is erroneous to call 'MPI_Add_error_string'
for an error code or class with a value less than or equal
to 'MPI_ERR_LASTCODE'.  Thus, you cannot replace the predefined error messages
with this routine.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Add_error_string(
    _In_ int errorcode,
    _In_z_ const char* string
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Add_error_string(errorcode, string);

    int mpi_errno;

    if( string == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "string" );
        goto fn_fail;
    }

    mpi_errno = MPIR_Err_set_msg( errorcode, (const char *)string );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Add_error_string();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_add_error_string %d %s",
            errorcode,
            string
            )
        );
    TraceError(MPI_Add_error_string, mpi_errno);
    goto fn_exit;
}


#ifdef HAVE_FORTRAN_BINDING

//
// Error function to support Fortran binding libs.
//

EXTERN_C int MPIAPI MPIR_Error(int errcode, const char fcname[])
{
    return MPIR_Err_return_comm(NULL, fcname, errcode);
}

#endif // HAVE_FORTRAN_BINDING
