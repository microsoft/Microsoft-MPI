// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


#if !defined(MPID_REQUEST_PTR_ARRAY_SIZE)
#define MPID_REQUEST_PTR_ARRAY_SIZE 16
#endif


/*@
    MPI_Request_free - Frees a communication request object

Input Parameter:
. request - communication request (handle)

Notes:

This routine is normally used to free inactive persistent requests created with
either 'MPI_Recv_init' or 'MPI_Send_init' and friends.  It `is` also
permissible to free an active request.  However, once freed, the request can no
longer be used in a wait or test routine (e.g., 'MPI_Wait') to determine
completion.

This routine may also be used to free a non-persistent requests such as those
created with 'MPI_Irecv' or 'MPI_Isend' and friends.  Like active persistent
requests, once freed, the request can no longer be used with test/wait routines
to determine completion.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG

.see also: MPI_Isend, MPI_Irecv, MPI_Issend, MPI_Ibsend, MPI_Irsend,
MPI_Recv_init, MPI_Send_init, MPI_Ssend_init, MPI_Rsend_init, MPI_Wait,
MPI_Test, MPI_Waitall, MPI_Waitany, MPI_Waitsome, MPI_Testall, MPI_Testany,
MPI_Testsome
@*/
EXTERN_C
MPI_METHOD
MPI_Request_free(
    _Inout_ _Post_equal_to_(MPI_REQUEST_NULL) MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Request_free(*request);

    int mpi_errno;

    MPID_Request *request_ptr;
    if( request == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaRequestValidate( *request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Progress_pump();

    switch (request_ptr->kind)
    {
        case MPID_BUFFERED_SEND:
        case MPID_REQUEST_SEND:
        case MPID_REQUEST_RECV:
        case MPID_REQUEST_RMA_GET:
        case MPID_REQUEST_RMA_PUT:
        {
            break;
        }

        case MPID_PERSISTENT_SEND:
        case MPID_PERSISTENT_RECV:
        {
            /* If this is an active persistent request, we must also
               release the partner request. */
            if (request_ptr->partner_request != NULL)
            {
                request_ptr->partner_request->Release();
            }
            break;
        }

        case MPID_GREQUEST:
        {
            mpi_errno = MPIR_Grequest_free(request_ptr);
            break;
        }

        default:
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**request_invalid_kind %d", request_ptr->kind);
            break;
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    request_ptr->Release();
    *request = MPI_REQUEST_NULL;
    MPID_Request_track_user_free();

    TraceLeave_MPI_Request_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        request_ptr ? request_ptr->comm : nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_request_free %p",
            request
            )
        );
    TraceError(MPI_Request_free, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Request_get_status - Nondestructive test for the completion of a Request

Input Parameter:
.  request - request (handle)

Output Parameters:
+  flag - true if operation has completed (logical)
-  status - status object (Status).  May be 'MPI_STATUS_IGNORE'.

   Notes:
   Unlike 'MPI_Test', 'MPI_Request_get_status' does not deallocate or
   deactivate the request.  A call to one of the test/wait routines or
   'MPI_Request_free' should be made to release the request object.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
_Success_(return == MPI_SUCCESS && *flag != 0)
int
MPIAPI
MPI_Request_get_status(
    _In_ MPI_Request request,
    _Out_ _Deref_out_range_(0, 1) int* flag,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Request_get_status(request);

    MPID_Request *request_ptr;
    int mpi_errno;

    if( flag == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    if( request == MPI_REQUEST_NULL )
    {
        MPIR_Status_set_empty(status);
        *flag = TRUE;
        mpi_errno = MPI_SUCCESS;
        goto fn_exit1;
    }

    mpi_errno = MpiaRequestValidate( request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (!request_ptr->test_complete())
    {
        /* request not complete. Pump the progress engine. Req #3130 */
        mpi_errno = MPID_Progress_pump();
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    if (request_ptr->test_complete())
    {
        switch(request_ptr->kind)
        {
        case MPID_BUFFERED_SEND:
        case MPID_REQUEST_SEND:
        case MPID_REQUEST_RMA_PUT:
        {
            if (status != MPI_STATUS_IGNORE)
            {
                MPIR_Status_set_cancelled(
                    status,
                    request_ptr->status.cancelled
                    );
            }
            mpi_errno = request_ptr->status.MPI_ERROR;
            break;
        }

        case MPID_REQUEST_RECV:
        case MPID_REQUEST_RMA_GET:
        {
            MPIR_Request_extract_status(request_ptr, status);
            mpi_errno = request_ptr->status.MPI_ERROR;
            break;
        }

        case MPID_PERSISTENT_SEND:
        {
            const MPID_Request * prequest_ptr = request_ptr->partner_request;

            if (prequest_ptr != NULL)
            {
                if (status != MPI_STATUS_IGNORE)
                {
                    MPIR_Status_set_cancelled(
                        status,
                        request_ptr->status.cancelled
                        );
                }
                mpi_errno = prequest_ptr->status.MPI_ERROR;
            }
            else
            {
                if (request_ptr->status.MPI_ERROR != MPI_SUCCESS)
                {
                    /* if the persistent request failed to start then
                       make the error code available */
                    if (status != MPI_STATUS_IGNORE)
                    {
                        MPIR_Status_set_cancelled(
                            status,
                            request_ptr->status.cancelled
                            );
                    }
                    mpi_errno = request_ptr->status.MPI_ERROR;
                }
                else
                {
                    MPIR_Status_set_empty(status);
                }
            }

            break;
        }

        case MPID_PERSISTENT_RECV:
        {
            const MPID_Request * prequest_ptr = request_ptr->partner_request;

            if (prequest_ptr != NULL)
            {
                MPIR_Request_extract_status(prequest_ptr, status);
                mpi_errno = prequest_ptr->status.MPI_ERROR;
            }
            else
            {
                /* if the persistent request failed to start then
                   make the error code available */
                mpi_errno = request_ptr->status.MPI_ERROR;
                MPIR_Status_set_empty(status);
            }

            break;
        }

        case MPID_GREQUEST:
        {
            mpi_errno = MPIR_Grequest_query(request_ptr);
            MPIR_Request_extract_status(request_ptr, status);

            break;
        }

        default:
            break;
        }

        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        *flag = TRUE;
    }
    else
    {
        *flag = FALSE;
    }

fn_exit1:
    TraceLeave_MPI_Request_get_status(*flag,SENTINEL_SAFE_SIZE(status),status);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        request_ptr ? request_ptr->comm : nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_request_get_status %R %p %p",
            request,
            flag,
            status
            )
        );
    TraceError(MPI_Request_get_status, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Request_set_apc - Sets an APC callback to be invoked on the calling thread
    when the request completes.

Input Parameter:
. request - communication request (handle)
. callback_fn - APC callback function (pointer)
. callback_status - MPI_Status to pass to callback (pointer)

.N ThreadSafe

.N Fortran

.N NULL

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MSMPI_Request_set_apc(
    _In_ MPI_Request request,
    _In_ MSMPI_Request_callback* callback_fn,
    _In_ MPI_Status* callback_status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MSMPI_Request_set_apc(request, callback_fn, callback_status);

    MPID_Request * request_ptr;
    int mpi_errno = MpiaRequestValidate( request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Support for persistent request requires the partner request to
    // find a way back to the parent and that link today does not exist.
    //
    if( request_ptr->kind == MPID_PERSISTENT_SEND ||
        request_ptr->kind == MPID_PERSISTENT_RECV )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**request_invalid_kind %d",
            request_ptr->kind
            );
        goto fn_fail;
    }

    if( callback_fn == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "callback_fn" );
        goto fn_fail;
    }

    if( callback_status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "callback_status" );
        goto fn_fail;
    }

    mpi_errno = request_ptr->set_apc( callback_fn, callback_status );

    TraceLeave_MSMPI_Request_set_apc();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        request_ptr ? request_ptr->comm : nullptr,
        __FUNCTION__,
        MPIU_ERR_GET(
            mpi_errno,
            "**msmpi_req_set_apc %R %p %p",
            request,
            callback_fn,
            callback_status
            )
        );
    TraceError(MSMPI_Request_set_apc, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Start - Initiates a communication with a persistent request handle

Input Parameter:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST

@*/
EXTERN_C
_Pre_satisfies_(*request != MPI_REQUEST_NULL)
MPI_METHOD
MPI_Start(
    _Inout_ MPI_Request* request
    )
{
    OACR_USE_PTR( request );
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Start(*request);

    MPID_Request *request_ptr;
    int mpi_errno;

    if( request == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaRequestValidate( *request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request_ptr->kind != MPID_PERSISTENT_SEND &&
        request_ptr->kind != MPID_PERSISTENT_RECV )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestnotpersist" );
        goto fn_fail;
    }

    if( request_ptr->partner_request != nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestpersistactive" );
        goto fn_fail;
    }

    mpi_errno = MPID_Startall(1, &request_ptr);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Start();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        request_ptr ? request_ptr->comm : nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_start %p",
            request
            )
        );
    TraceError(MPI_Start, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Startall - Starts a collection of persistent requests

Input Parameters:
+ count - list length (integer)
- array_of_requests - array of requests (array of handle)

   Notes:

   Unlike 'MPI_Waitall', 'MPI_Startall' does not provide a mechanism for
   returning multiple errors nor pinpointing the request(s) involved.
   Futhermore, the behavior of 'MPI_Startall' after an error occurs is not
   defined by the MPI standard.  If well-defined error reporting and behavior
   are required, multiple calls to 'MPI_Start' should be used instead.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_REQUEST
@*/
EXTERN_C
MPI_METHOD
MPI_Startall(
    _In_range_(>=, 0) int count,
    _mpi_updates_(count) MPI_Request array_of_requests[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Startall(count, TraceArrayLength(count), (unsigned int*)array_of_requests);

    MPID_Request * request_ptr_array[MPID_REQUEST_PTR_ARRAY_SIZE];
    MPID_Request ** request_ptrs = request_ptr_array;
    StackGuardArray<MPID_Request*> request_ptr_buf;
    int i;
    int mpi_errno = MPI_SUCCESS;

    /* Validate handle parameters needing to be converted */
    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }

    if( count == 0 )
    {
        goto fn_exit;
    }

    if( array_of_requests == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
        goto fn_fail;
    }

    /* Convert MPI request handles to a request object pointers */
    if (count > _countof(request_ptr_array))
    {
        request_ptr_buf = new MPID_Request*[count];
        if( request_ptr_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        request_ptrs = request_ptr_buf;
    }

    for (i = 0; i < count; i++)
    {
        mpi_errno = MpiaRequestValidate( array_of_requests[i], &request_ptrs[i] );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if( request_ptrs[i]->kind != MPID_PERSISTENT_SEND &&
            request_ptrs[i]->kind != MPID_PERSISTENT_RECV )
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestnotpersist" );
            goto fn_fail;
        }

        if( request_ptrs[i]->partner_request != nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestpersistactive" );
            goto fn_fail;
        }
    }

    mpi_errno = MPID_Startall(count, request_ptrs);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Startall();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_startall %d %p",
            count,
            array_of_requests
            )
        );
    TraceError(MPI_Startall, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Grequest_complete - Notify MPI that a user-defined request is complete

   Input Parameter:
.  request - Generalized request to mark as complete

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS

.seealso: MPI_Grequest_start
@*/
EXTERN_C
MPI_METHOD
MPI_Grequest_complete(
    _In_ MPI_Request request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Grequest_complete(request);

    MPID_Request *request_ptr;
    int mpi_errno = MpiaRequestValidate( request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (request_ptr->kind != MPID_GREQUEST)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG,  "**notgenreq" );
        goto fn_fail;
    }

    /* Set the request as completed.  This does not change the
       reference count on the generalized request */
    if(!request_ptr->test_complete())
    {
        request_ptr->signal_completion();
        Mpi.WakeProgress(true);
    }

    /* The request release comes with the wait/test, not this complete
       routine, so we don't call the MPID_Request_release routine */

    TraceLeave_MPI_Grequest_complete();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_grequest_complete %R",
            request
            )
        );
    TraceError(MPI_Grequest_complete, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Grequest_start - Create and return a user-defined request

Input Parameters:
+ query_fn - callback function invoked when request status is queried (function)
. free_fn - callback function invoked when request is freed (function)
. cancel_fn - callback function invoked when request is cancelled (function)
- extra_state - Extra state passed to the above functions.

Output Parameter:
.  request - Generalized request (handle)

 Notes on the callback functions:
 The return values from the callback functions must be a valid MPI error code
 or class.  This value may either be the return value from any MPI routine
 (with one exception noted below) or any of the MPI error classes.
 For portable programs, 'MPI_ERR_OTHER' may be used; to provide more
 specific information, create a new MPI error class or code with
 'MPI_Add_error_class' or 'MPI_Add_error_code' and return that value.

 The MPI standard is not clear on the return values from the callback routines.
 However, there are notes in the standard that imply that these are MPI error
 codes.  For example, pages 169 line 46 through page 170, line 1 require that
 the 'free_fn' return an MPI error code that may be used in the MPI completion
 functions when they return 'MPI_ERR_IN_STATUS'.

 The one special case is the error value returned by 'MPI_Comm_dup' when
 the attribute callback routine returns a failure.  The MPI standard is not
 clear on what values may be used to indicate an error return.  Further,
 the Intel MPI test suite made use of non-zero values to indicate failure,
 and expected these values to be returned by the 'MPI_Comm_dup' when the
 attribute routines encountered an error.  Such error values may not be valid
 MPI error codes or classes.  Because of this, it is the user's responsibility
 to either use valid MPI error codes in return from the attribute callbacks,
 if those error codes are to be returned by a generalized request callback,
 or to detect and convert those error codes to valid MPI error codes (recall
 that MPI error classes are valid error codes).

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Grequest_start(
    _In_ MPI_Grequest_query_function* query_fn,
    _In_ MPI_Grequest_free_function* free_fn,
    _In_ MPI_Grequest_cancel_function* cancel_fn,
    _In_opt_ void* extra_state,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Grequest_start(query_fn,free_fn,cancel_fn,extra_state);

    int mpi_errno = MPI_SUCCESS;
    MPID_Request *lrequest_ptr;

    /* Validate parameters if error checking is enabled */
    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    lrequest_ptr = MPID_Request_create(MPID_GREQUEST);
    if( lrequest_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    lrequest_ptr->comm                 = NULL;
    lrequest_ptr->greq.query.user_function = query_fn;
    lrequest_ptr->greq.query.proxy = MPIR_Grequest_query_c_proxy;
    lrequest_ptr->greq.free.user_function = free_fn;
    lrequest_ptr->greq.free.proxy = MPIR_Grequest_free_c_proxy;
    lrequest_ptr->greq.cancel.user_function = cancel_fn;
    lrequest_ptr->greq.cancel.proxy = MPIR_Grequest_cancel_c_proxy;
    lrequest_ptr->greq.extra_state = extra_state;
    *request = lrequest_ptr->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Grequest_start(*request);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_grequest_start %p %p %p %p %p",
            query_fn,
            free_fn,
            cancel_fn,
            extra_state,
            request
            )
        );
    TraceError(MPI_Grequest_start, mpi_errno);
    goto fn_exit;
}


EXTERN_C
void
MPIAPI
MPIR_Grequest_set_proxy(
    MPI_Request request,
    MPIR_Grequest_query_proxy query_proxy,
    MPIR_Grequest_free_proxy free_proxy,
    MPIR_Grequest_cancel_proxy cancel_proxy
    )
{
    MPID_Request *greq_ptr;
    int mpi_errno = MpiaRequestValidate( request, &greq_ptr );
    if( mpi_errno == MPI_SUCCESS )
    {
        greq_ptr->greq.cancel.proxy = cancel_proxy;
        greq_ptr->greq.free.proxy = free_proxy;
        greq_ptr->greq.query.proxy = query_proxy;
    }
}


/*@
    MPI_Cancel - Cancels a communication request

Input Parameter:
. request - communication request (handle)

Notes:
The primary expected use of 'MPI_Cancel' is in multi-buffering
schemes, where speculative 'MPI_Irecvs' are made.  When the computation
completes, some of these receive requests may remain; using 'MPI_Cancel' allows
the user to cancel these unsatisfied requests.

Cancelling a send operation is much more difficult, in large part because the
send will usually be at least partially complete (the information on the tag,
size, and source are usually sent immediately to the destination).
Users are
advised that cancelling a send, while a local operation (as defined by the MPI
standard), is likely to be expensive (usually generating one or more internal
messages).

.N ThreadSafe

.N Fortran

.N NULL

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
@*/
EXTERN_C
_Pre_satisfies_(*request != MPI_REQUEST_NULL)
MPI_METHOD
MPI_Cancel(
    _In_ MPI_Request* request
    )
{
    OACR_USE_PTR( request );
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Cancel(*request);

    int mpi_errno;

    MPID_Request *request_ptr;
    if( request == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MpiaRequestValidate( *request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    switch (request_ptr->kind)
    {
        case MPID_BUFFERED_SEND:
        case MPID_REQUEST_SEND:
            mpi_errno = MPID_Cancel_send(request_ptr);
            break;

        case MPID_REQUEST_RECV:
            MPID_Cancel_recv(request_ptr);
            break;

        case MPID_REQUEST_RMA_PUT:
        case MPID_REQUEST_RMA_GET:
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestrmacancel");
            break;

        case MPID_PERSISTENT_SEND:
            if (request_ptr->partner_request != NULL)
            {
                mpi_errno = MPID_Cancel_send(request_ptr->partner_request);
            }
            else
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestpersistactive");
            }

            break;

        case MPID_PERSISTENT_RECV:
            if (request_ptr->partner_request != NULL)
            {
                MPID_Cancel_recv(request_ptr->partner_request);
            }
            else
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestpersistactive");
            }

            break;

        case MPID_GREQUEST:
            mpi_errno = MPIR_Grequest_cancel(request_ptr);
            break;

        default:
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN,"**cancelunknown");
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Cancel();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        request_ptr ? request_ptr->comm : nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_cancel %p",
            request
            )
        );
    TraceError(MPI_Cancel, mpi_errno);
    goto fn_exit;
}
