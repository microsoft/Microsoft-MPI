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

static bool IsRmaRequest(_In_ const MPID_Request* pReq)
{
    return (pReq->kind == MPID_REQUEST_RMA_GET || pReq->kind == MPID_REQUEST_RMA_PUT);
}

_Success_(return == MPI_SUCCESS)
static MPI_RESULT RmaForwardProgress(
    _In_ MPID_Request* pReq,
    _Out_ bool* rmaFlushCalled
    )
{
    bool flushCalled = false;

    MPID_Win *win_ptr;
    MPI_RESULT mpi_errno = MpiaWinValidateHandle(pReq->dev.source_win_handle, &win_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_exit;
    }

    int target_rank = pReq->dev.target_rank;
    MPIU_Assert(target_rank != MPI_PROC_NULL);

    mpi_errno = MPID_Win_flush(target_rank, win_ptr, true);
    if (mpi_errno == MPI_SUCCESS)
    {
        flushCalled = true;
    }

fn_exit:
    *rmaFlushCalled = flushCalled;
    return mpi_errno;
}


/*@
    MPI_Test  - Tests for the completion of a request

Input Parameter:
. request - MPI request (handle)

Output Parameter:
+ flag - true if operation completed (logical)
- status - status object (Status).  May be 'MPI_STATUS_IGNORE'.

.N ThreadSafe

.N waitstatus

.N Fortran

.N FortranStatus

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
@*/
EXTERN_C
_Success_(return == MPI_SUCCESS && *flag != 0)
int
MPIAPI
MPI_Test(
    _Inout_ _Post_equal_to_(MPI_REQUEST_NULL) MPI_Request* request,
    _mpi_out_flag_ int* flag,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Test(*request);

    MPID_Request *request_ptr;
    int mpi_errno;

    /* Validate parameters, especially handles needing to be converted */
    if( request == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != nullptr */
    if( status == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    if( *request == MPI_REQUEST_NULL )
    {
        MPIR_Status_set_empty(status);
        *flag = TRUE;
        mpi_errno = MPI_SUCCESS;
        goto fn_exit;
    }

    mpi_errno = MpiaRequestValidate( *request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *flag = FALSE;

    /* If the request is already completed AND we want to avoid calling
     the progress engine, we could make the call to MPID_Progress_pump
     conditional on the request not being completed. */
    mpi_errno = MPID_Progress_pump();
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (IsRmaRequest(request_ptr) && !request_ptr->test_complete())
    {
        bool rmaFlushCalled;
        mpi_errno = RmaForwardProgress(request_ptr, &rmaFlushCalled);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
    }

    if (request_ptr->test_complete())
    {
        *flag = TRUE;

        int active_flag;
        mpi_errno = MPIR_Request_complete(request, request_ptr, status, &active_flag);
        ON_ERROR_FAIL(mpi_errno);
    }

  fn_exit:
    TraceLeave_MPI_Test(*flag, SENTINEL_SAFE_SIZE(status), status);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        request_ptr ? request_ptr->comm : nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_test %p %p %p",
            request,
            flag,
            status
            )
        );
    TraceError(MPI_Test, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Testall - Tests for the completion of all previously initiated
    requests

Input Parameters:
+ count - lists length (integer)
- array_of_requests - array of requests (array of handles)

Output Parameters:
+ flag - True if all requests have completed; false otherwise (logical)
- array_of_statuses - array of status objects (array of Status).  May be
 'MPI_STATUSES_IGNORE'.

Notes:
  'flag' is true only if all requests have completed.  Otherwise, flag is
  false and neither the 'array_of_requests' nor the 'array_of_statuses' is
  modified.

If one or more of the requests completes with an error, 'MPI_ERR_IN_STATUS' is
returned.  An error value will be present is elements of 'array_of_status'
associated with the requests.  Likewise, the 'MPI_ERROR' field in the status
elements associated with requests that have successfully completed will be
'MPI_SUCCESS'.  Finally, those requests that have not completed will have a
value of 'MPI_ERR_PENDING'.

While it is possible to list a request handle more than once in the
'array_of_requests', such an action is considered erroneous and may cause the
program to unexecpectedly terminate or produce incorrect results.

.N ThreadSafe

.N waitstatus

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_IN_STATUS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
@*/
EXTERN_C
_Success_(return == MPI_SUCCESS && *flag != 0)
int
MPIAPI
MPI_Testall(
    _In_range_(>=, 0) int count,
    _mpi_updates_(count) MPI_Request array_of_requests[],
    _mpi_out_flag_ int* flag,
    _Out_writes_opt_(count) MPI_Status array_of_statuses[]
)
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Testall(count,TraceArrayLength(count),(const unsigned int*)array_of_requests);

    MPID_Request * request_ptr_array[MPID_REQUEST_PTR_ARRAY_SIZE];
    MPID_Request ** request_ptrs = request_ptr_array;
    StackGuardArray<MPID_Request*> request_ptr_buf;
    MPI_Status * status_ptr;
    int i;
    int n_completed;
    int active_flag;
    int rc;
    int mpi_errno = MPI_SUCCESS;

    /* Check the arguments */
    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    if( count != 0 )
    {
        if( array_of_requests == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
            goto fn_fail;
        }
        /* NOTE: MPI_STATUSES_IGNORE != nullptr */
        if( array_of_statuses == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_statuses" );
            goto fn_fail;
        }
    }

    if (count > MPID_REQUEST_PTR_ARRAY_SIZE)
    {
        request_ptr_buf = new MPID_Request*[count];
        if( request_ptr_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        request_ptrs = request_ptr_buf;
    }

    /* Convert MPI request handles to a request object pointers */
    n_completed = 0;
    for (i = 0; i < count; i++)
    {
        if (array_of_requests[i] != MPI_REQUEST_NULL)
        {
            mpi_errno = MpiaRequestValidate( array_of_requests[i], &request_ptrs[i] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            request_ptrs[i] = nullptr;
            n_completed += 1;
        }
    }

    mpi_errno = MPID_Progress_pump();
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    bool rmaFlushCalled = false;
    for (i = 0; i < count; i++)
    {
        if (request_ptrs[i] == nullptr)
        {
            continue;
        }
        if (request_ptrs[i]->test_complete() == false)
        {
            if (IsRmaRequest(request_ptrs[i]) && !rmaFlushCalled)
            {
                mpi_errno = RmaForwardProgress(request_ptrs[i], &rmaFlushCalled);
                if (mpi_errno != MPI_SUCCESS)
                {
                    goto fn_fail;
                }
            }
            else
            {
                continue;
            }
        }

        n_completed++;
        if (MPIR_Request_get_error(request_ptrs[i]) != MPI_SUCCESS)
        {
            mpi_errno = MPI_ERR_IN_STATUS;
        }
    }

    if (n_completed == count || mpi_errno == MPI_ERR_IN_STATUS)
    {
        n_completed = 0;
        for (i = 0; i < count; i++)
        {
            if (request_ptrs[i] != nullptr)
            {
                if (request_ptrs[i]->test_complete())
                {
                    n_completed ++;

                    status_ptr = (array_of_statuses != MPI_STATUSES_IGNORE) ? &array_of_statuses[i] : MPI_STATUS_IGNORE;
                    rc = MPIR_Request_complete(&array_of_requests[i], request_ptrs[i], status_ptr, &active_flag);
                    if (mpi_errno == MPI_ERR_IN_STATUS && status_ptr != MPI_STATUS_IGNORE)
                    {
                        if (active_flag)
                        {
                            status_ptr->MPI_ERROR = rc;
                        }
                        else
                        {
                            status_ptr->MPI_ERROR = MPI_SUCCESS;
                        }
                    }
                }
                else
                {
                    if (mpi_errno == MPI_ERR_IN_STATUS && array_of_statuses != MPI_STATUSES_IGNORE)
                    {
                        array_of_statuses[i].MPI_ERROR = MPI_ERR_PENDING;
                    }
                }
            }
            else
            {
                n_completed ++;
                if (array_of_statuses != MPI_STATUSES_IGNORE)
                {
                    MPIR_Status_set_empty(&array_of_statuses[i]);
                    if (mpi_errno == MPI_ERR_IN_STATUS)
                    {
                        array_of_statuses[i].MPI_ERROR = MPI_SUCCESS;
                    }
                }
            }
        }
    }

    *flag = (n_completed == count) ? TRUE : FALSE;

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Testall(*flag, count, SENTINEL_SAFE_COUNT(array_of_statuses,TraceArrayLength(count)), SENTINEL_SAFE_SIZE(array_of_statuses), array_of_statuses);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_testall %d %p %p %p",
            count,
            array_of_requests,
            flag,
            array_of_statuses
            )
        );
    TraceError(MPI_Testall, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_Testany - Tests for completion of any previdously initiated
                  requests

Input Parameters:
+ count - list length (integer)
- array_of_requests - array of requests (array of handles)

Output Parameters:
+ index - index of operation that completed, or 'MPI_UNDEFINED'  if none
  completed (integer)
. flag - true if one of the operations is complete (logical)
- status - status object (Status).  May be 'MPI_STATUS_IGNORE'.

Notes:

While it is possible to list a request handle more than once in the
'array_of_requests', such an action is considered erroneous and may cause the
program to unexecpectedly terminate or produce incorrect results.

.N ThreadSafe

.N waitstatus

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
_Success_(return == MPI_SUCCESS && *flag != 0)
int
MPIAPI
MPI_Testany(
    _In_range_(>=, 0) int count,
    _mpi_updates_(count) MPI_Request array_of_requests[],
    _mpi_out_range_(index, MPI_UNDEFINED, (count - 1)) int* index,
    _mpi_out_flag_ int* flag,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Testany(count,TraceArrayLength(count),(const unsigned int*)array_of_requests);

    MPID_Request * request_ptr_array[MPID_REQUEST_PTR_ARRAY_SIZE];
    MPID_Request ** request_ptrs = request_ptr_array;
    StackGuardArray<MPID_Request*> request_ptr_buf;
    int i;
    int n_inactive;
    int active_flag;
    int mpi_errno = MPI_SUCCESS;

    /* Check the arguments */
    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }

    if (count != 0)
    {
        if( array_of_requests == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
        }
        /* NOTE: MPI_STATUS_IGNORE != nullptr */
        if( status == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        }
    }

    if( index == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "index" );
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    /* Convert MPI request handles to a request object pointers */
    if (count > MPID_REQUEST_PTR_ARRAY_SIZE)
    {
        request_ptr_buf = new MPID_Request*[count];
        if( request_ptr_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        request_ptrs = request_ptr_buf;
    }

    n_inactive = 0;
    for (i = 0; i < count; i++)
    {
        if (array_of_requests[i] != MPI_REQUEST_NULL)
        {
            mpi_errno = MpiaRequestValidate( array_of_requests[i], &request_ptrs[i] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            request_ptrs[i] = nullptr;
            n_inactive += 1;
        }
    }

    if (n_inactive == count)
    {
        *flag = TRUE;
        *index = MPI_UNDEFINED;
        //
        // If count == 0, status could be nullptr.
        //
        if (status != nullptr)
        {
            MPIR_Status_set_empty(status);
        }
        goto fn_exit;
    }

    *flag = FALSE;
    *index = MPI_UNDEFINED;

    mpi_errno = MPID_Progress_pump();
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    bool rmaFlushCalled = false;
    for (i = 0; i < count; i++)
    {
        if (request_ptrs[i] == nullptr)
        {
            continue;
        }
        if (request_ptrs[i]->test_complete() == false)
        {
            if (IsRmaRequest(request_ptrs[i]) && !rmaFlushCalled)
            {
                mpi_errno = RmaForwardProgress(request_ptrs[i], &rmaFlushCalled);
                if (mpi_errno != MPI_SUCCESS)
                {
                    goto fn_fail;
                }
            }
            else
            {
                continue;
            }
        }

        mpi_errno = MPIR_Request_complete(&array_of_requests[i],
                                          request_ptrs[i],
                                          status, &active_flag);
        if (active_flag)
        {
            *flag = TRUE;
            *index = i;
            goto fn_exit;
        }
        else
        {
            n_inactive += 1;
        }
    }

    if (n_inactive == count)
    {
        *flag = TRUE;
        *index = MPI_UNDEFINED;
        /* status set to empty by MPIR_Request_complete() */
    }

  fn_exit:
    TraceLeave_MPI_Testany(*index, *flag, SENTINEL_SAFE_SIZE(status), status);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_testany %d %p %p %p %p",
            count,
            array_of_requests,
            index,
            flag,
            status
            )
        );
    TraceError(MPI_Testany, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Testsome - Tests for some given requests to complete

Input Parameters:
+ incount - length of array_of_requests (integer)
- array_of_requests - array of requests (array of handles)

Output Parameters:
+ outcount - number of completed requests (integer)
. array_of_indices - array of indices of operations that
completed (array of integers)
- array_of_statuses - array of status objects for
    operations that completed (array of Status).  May be 'MPI_STATUSES_IGNORE'.

Notes:

While it is possible to list a request handle more than once in the
'array_of_requests', such an action is considered erroneous and may cause the
program to unexecpectedly terminate or produce incorrect results.

.N ThreadSafe

.N waitstatus

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_IN_STATUS

@*/
EXTERN_C
_Success_(return == MPI_SUCCESS && *outcount > 0)
int
MPIAPI
MPI_Testsome(
    _In_range_(>=, 0) int incount,
    _mpi_updates_(incount) MPI_Request array_of_requests[],
    _mpi_out_range_(outcount, MPI_UNDEFINED, incount) int* outcount,
    _mpi_writes_to_(incount,*outcount) int array_of_indices[],
    _Out_writes_to_opt_(incount, *outcount) MPI_Status array_of_statuses[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Testsome(incount, TraceArrayLength(incount), (const unsigned int*)array_of_requests);

    MPID_Request * request_ptr_array[MPID_REQUEST_PTR_ARRAY_SIZE];
    MPID_Request ** request_ptrs = request_ptr_array;
    StackGuardArray<MPID_Request*> request_ptr_buf;
    MPI_Status * status_ptr;
    int i;
    int n_active;
    int n_inactive;
    int active_flag;
    int rc;
    int mpi_errno = MPI_SUCCESS;

    /* Check the arguments */
    if( incount < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", incount );;
        goto fn_fail;
    }

    if (incount != 0)
    {
        if( array_of_requests == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
            goto fn_fail;
        }

        if( array_of_indices == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_indices" );
            goto fn_fail;
        }
        /* NOTE: MPI_STATUSES_IGNORE != nullptr */
        if( array_of_statuses == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_statuses" );
            goto fn_fail;
        }
    }

    if( outcount == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "outcount" );
        goto fn_fail;
    }

    /* Convert MPI request handles to a request object pointers */
    if (incount > MPID_REQUEST_PTR_ARRAY_SIZE)
    {
        request_ptr_buf = new MPID_Request*[incount];
        if( request_ptr_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        request_ptrs = request_ptr_buf;
    }

    *outcount = 0;

    n_inactive = 0;
    for (i = 0; i < incount; i++)
    {
        if (array_of_requests[i] != MPI_REQUEST_NULL)
        {
            mpi_errno = MpiaRequestValidate( array_of_requests[i], &request_ptrs[i] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            request_ptrs[i] = nullptr;
            n_inactive += 1;
        }
    }

    if (n_inactive == incount)
    {
        *outcount = MPI_UNDEFINED;
        goto fn_exit;
    }

    n_active = 0;

    mpi_errno = MPID_Progress_pump();
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    bool rmaFlushCalled = false;
    for (i = 0; i < incount; i++)
    {
        if (request_ptrs[i] == nullptr)
        {
            continue;
        }

        if (request_ptrs[i]->test_complete() == false)
        {
            if (IsRmaRequest(request_ptrs[i]) && !rmaFlushCalled)
            {
                mpi_errno = RmaForwardProgress(request_ptrs[i], &rmaFlushCalled);
                if (mpi_errno != MPI_SUCCESS)
                {
                    goto fn_fail;
                }
            }
            else
            {
                continue;
            }
        }

        status_ptr = (array_of_statuses != MPI_STATUSES_IGNORE) ? &array_of_statuses[n_active] : MPI_STATUS_IGNORE;
        rc = MPIR_Request_complete(&array_of_requests[i], request_ptrs[i],
                                   status_ptr, &active_flag);
        if (active_flag)
        {
            array_of_indices[n_active] = i;
            n_active += 1;

            if (rc == MPI_SUCCESS)
            {
                request_ptrs[i] = nullptr;
            }
            else
            {
                mpi_errno = MPI_ERR_IN_STATUS;
                if (status_ptr != MPI_STATUS_IGNORE)
                {
                    status_ptr->MPI_ERROR = rc;
                }
            }
        }
        else
        {
            request_ptrs[i] = nullptr;
            n_inactive += 1;
        }
    }

    if (mpi_errno == MPI_ERR_IN_STATUS)
    {
        if (array_of_statuses != MPI_STATUSES_IGNORE)
        {
            for (i = 0; i < n_active; i++)
            {
                if (request_ptrs[array_of_indices[i]] == nullptr)
                {
                    array_of_statuses[i].MPI_ERROR = MPI_SUCCESS;
                }
            }
        }
        *outcount = n_active;
    }
    else if (n_active > 0)
    {
        *outcount = n_active;
    }
    else if (n_inactive == incount)
    {
        *outcount = MPI_UNDEFINED;
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

  fn_exit:
    TraceLeave_MPI_Testsome(*outcount, TraceArrayLength(*outcount), array_of_indices, SENTINEL_SAFE_COUNT(array_of_statuses,TraceArrayLength(*outcount)), SENTINEL_SAFE_SIZE(array_of_statuses), array_of_statuses);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_testsome %d %p %p %p %p",
            incount,
            array_of_requests,
            outcount,
            array_of_indices,
            array_of_statuses
            )
        );
    TraceError(MPI_Testsome, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Wait - Waits for an MPI request to complete

Input Parameter:
. request - request (handle)

Output Parameter:
. status - status object (Status).  May be 'MPI_STATUS_IGNORE'.

.N waitstatus

.N ThreadSafe

.N Fortran

.N FortranStatus

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Wait(
    _Inout_ _Post_equal_to_(MPI_REQUEST_NULL) MPI_Request* request,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Wait(*request);

    MPID_Request * request_ptr;
    int active_flag;
    int mpi_errno = MPI_SUCCESS;

    /* Check the arguments */
    if( request == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != nullptr */
    if( status == nullptr )
    {
        request_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    if (*request == MPI_REQUEST_NULL)
    {
        MPIR_Status_set_empty(status);
        mpi_errno = MPI_SUCCESS;
        goto fn_exit;
    }

    mpi_errno = MpiaRequestValidate( *request, &request_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (!request_ptr->test_complete())
    {
        if (IsRmaRequest(request_ptr))
        {
            bool rmaFlushCalled;
            mpi_errno = RmaForwardProgress(request_ptr, &rmaFlushCalled);
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }
        }
        while (!request_ptr->test_complete())
        {
            mpi_errno = MPID_Progress_wait();
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }
        }
    }

    mpi_errno = MPIR_Request_complete(request, request_ptr, status, &active_flag);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

  fn_exit:
    TraceLeave_MPI_Wait(SENTINEL_SAFE_SIZE(status),status);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        (request_ptr != nullptr) ? request_ptr->comm : nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_wait %p %p",
            request,
            status
            )
        );
    TraceError(MPI_Wait, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Waitall - Waits for all given MPI Requests to complete

Input Parameters:
+ count - list length (integer)
- array_of_requests - array of request handles (array of handles)

Output Parameter:
. array_of_statuses - array of status objects (array of Statuses).  May be
  'MPI_STATUSES_IGNORE'.

Notes:

If one or more of the requests completes with an error, 'MPI_ERR_IN_STATUS' is
returned.  An error value will be present is elements of 'array_of_status'
associated with the requests.  Likewise, the 'MPI_ERROR' field in the status
elements associated with requests that have successfully completed will be
'MPI_SUCCESS'.  Finally, those requests that have not completed will have a
value of 'MPI_ERR_PENDING'.

While it is possible to list a request handle more than once in the
array_of_requests, such an action is considered erroneous and may cause the
program to unexecpectedly terminate or produce incorrect results.

.N waitstatus

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
.N MPI_ERR_IN_STATUS
@*/
EXTERN_C
MPI_METHOD
MPI_Waitall(
    _In_range_(>=, 0) int count,
    _mpi_updates_(count) MPI_Request array_of_requests[],
    _Out_writes_opt_(count) MPI_Status array_of_statuses[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Waitall(count,TraceArrayLength(count),(const unsigned int*)array_of_requests);

    MPID_Request * request_ptr;
    MPI_Status * status_ptr;
    int i;
    int active_flag;
    int mpi_errno = MPI_SUCCESS;
    int rc;
    BOOL have_errors = FALSE;

    /* Check the arguments */
    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }

    if (count != 0)
    {
        if( array_of_requests == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
            goto fn_fail;
        }
        /* NOTE: MPI_STATUSES_IGNORE != nullptr */

        if( array_of_statuses == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_statuses" );
            goto fn_fail;
        }
    }

    bool rmaFlushCalled = false;
    for (i = 0; i < count; i++)
    {
        status_ptr = (array_of_statuses != MPI_STATUSES_IGNORE) ? &array_of_statuses[i] : MPI_STATUS_IGNORE;
        request_ptr = nullptr;
        if (array_of_requests[i] == MPI_REQUEST_NULL)
        {
            MPIR_Status_set_empty(status_ptr);
            continue;
        }

        rc = MpiaRequestValidate(array_of_requests[i], &request_ptr);
        if (rc != MPI_SUCCESS)
        {
            mpi_errno = MPI_ERR_IN_STATUS;
            MPIR_Status_set_error(status_ptr, rc);
            have_errors = TRUE;
            continue;
        }

        if (have_errors)
        {
            MPIR_Status_set_error(status_ptr, MPI_ERR_PENDING);
            continue;
        }

OACR_WARNING_SUPPRESS(26501, "Suppress false anvil warning.")
        if (!request_ptr->test_complete())
        {
            if (IsRmaRequest(request_ptr) && !rmaFlushCalled)
            {
                mpi_errno = RmaForwardProgress(request_ptr, &rmaFlushCalled);
                if (mpi_errno != MPI_SUCCESS)
                {
                    goto fn_fail;
                }
            }
            while (!request_ptr->test_complete())
            {
                rc = MPID_Progress_wait();
                if (rc != MPI_SUCCESS)
                {
                    mpi_errno = rc;
                    goto fn_fail;
                }
            }
        }

        rc = MPIR_Request_complete(&array_of_requests[i], request_ptr, status_ptr, &active_flag);
        MPIR_Status_set_error(status_ptr, rc);
        if (rc != MPI_SUCCESS)
        {
            mpi_errno = MPI_ERR_IN_STATUS;
            have_errors = TRUE;
        }
    }

    TraceLeave_MPI_Waitall(mpi_errno, count, SENTINEL_SAFE_COUNT(array_of_statuses,TraceArrayLength(count)), SENTINEL_SAFE_SIZE(array_of_statuses),array_of_statuses);

  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_waitall %d %p %p",
            count,
            array_of_requests,
            array_of_statuses
            )
        );
    TraceError(MPI_Waitall, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Waitany - Waits for any specified MPI Request to complete

Input Parameters:
+ count - list length (integer)
- array_of_requests - array of requests (array of handles)

Output Parameters:
+ index - index of handle for operation that completed (integer).  In the
range '0' to 'count-1'.  In Fortran, the range is '1' to 'count'.
- status - status object (Status).  May be 'MPI_STATUS_IGNORE'.

Notes:
If all of the requests are 'MPI_REQUEST_NULL', then 'index' is returned as
'MPI_UNDEFINED', and 'status' is returned as an empty status.

While it is possible to list a request handle more than once in the
array_of_requests, such an action is considered erroneous and may cause the
program to unexecpectedly terminate or produce incorrect results.

.N waitstatus

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Waitany(
    _In_range_(>=, 0) int count,
    _mpi_updates_(count) MPI_Request array_of_requests[],
    _mpi_out_range_(index, MPI_UNDEFINED, (count - 1)) int* index,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Waitany(count, TraceArrayLength(count), (const unsigned int*)array_of_requests);

    MPID_Request * request_ptr_array[MPID_REQUEST_PTR_ARRAY_SIZE];
    MPID_Request ** request_ptrs = request_ptr_array;
    StackGuardArray<MPID_Request*> request_ptr_buf;
    int i;
    int n_inactive;
    int active_flag;
    int mpi_errno = MPI_SUCCESS;

    /* Check the arguments */
    if( count < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", count );;
        goto fn_fail;
    }

    if (count != 0)
    {
        if( array_of_requests == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
            goto fn_fail;
        }
        /* NOTE: MPI_STATUS_IGNORE != nullptr */
        if( status == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
            goto fn_fail;
        }
    }

    if( index == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "index" );
        goto fn_fail;
    }

    if (count > MPID_REQUEST_PTR_ARRAY_SIZE)
    {
        request_ptr_buf = new MPID_Request*[count];
        if( request_ptr_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        request_ptrs = request_ptr_buf;
    }

    /* Convert MPI request handles to a request object pointers */
    n_inactive = 0;
    for (i = 0; i < count; i++)
    {
        if (array_of_requests[i] != MPI_REQUEST_NULL)
        {
            mpi_errno = MpiaRequestValidate( array_of_requests[i], &request_ptrs[i] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            request_ptrs[i] = nullptr;
            n_inactive += 1;
        }
    }

    if (n_inactive == count)
    {
        *index = MPI_UNDEFINED;
        MPIR_Status_set_empty(status);
        goto fn_exit;
    }

    bool rmaFlushCalled = false;
    for(;;)
    {
        for (i = 0; i < count; i++)
        {
            if (request_ptrs[i] == nullptr)
            {
                continue;
            }
            if (request_ptrs[i]->test_complete() == false)
            {
                if (IsRmaRequest(request_ptrs[i]) && !rmaFlushCalled)
                {
                    mpi_errno = RmaForwardProgress(request_ptrs[i], &rmaFlushCalled);
                    if (mpi_errno != MPI_SUCCESS)
                    {
                        goto fn_exit;
                    }
                }
                else
                {
                    continue;
                }
            }

            mpi_errno = MPIR_Request_complete(&array_of_requests[i],
                                              request_ptrs[i], status,
                                              &active_flag);
            if (active_flag)
            {
                *index = i;
                goto fn_exit;
            }
            else
            {
                n_inactive += 1;
                request_ptrs[i] = nullptr;

                if (n_inactive == count)
                {
                    *index = MPI_UNDEFINED;
                    /* status is set to empty by MPIR_Request_complete */
                    goto fn_exit;
                }
            }
        }

        mpi_errno = MPID_Progress_wait();
        if (mpi_errno != MPI_SUCCESS)
            goto fn_exit;
    }

  fn_exit:
    TraceLeave_MPI_Waitany(*index, SENTINEL_SAFE_SIZE(status), status);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_waitany %d %p %p %p",
            count,
            array_of_requests,
            index,
            status
            )
        );
    TraceError(MPI_Waitany, mpi_errno);
    goto fn_exit1;
}


/*@
    MPI_Waitsome - Waits for some given MPI Requests to complete

Input Parameters:
+ incount - length of array_of_requests (integer)
- array_of_requests - array of requests (array of handles)

Output Parameters:
+ outcount - number of completed requests (integer)
. array_of_indices - array of indices of operations that
completed (array of integers)
- array_of_statuses - array of status objects for
    operations that completed (array of Status).  May be 'MPI_STATUSES_IGNORE'.

Notes:
  The array of indicies are in the range '0' to 'incount - 1' for C and
in the range '1' to 'incount' for Fortran.

NULL requests are ignored; if all requests are NULL, then the routine
returns with 'outcount' set to 'MPI_UNDEFINED'.

While it is possible to list a request handle more than once in the
array_of_requests, such an action is considered erroneous and may cause the
program to unexecpectedly terminate or produce incorrect results.

'MPI_Waitsome' provides an interface much like the Unix 'select' or 'poll'
calls and, in a high qualilty implementation, indicates all of the requests
that have completed when 'MPI_Waitsome' is called.
However, 'MPI_Waitsome' only guarantees that at least one
request has completed; there is no guarantee that `all` completed requests
will be returned, or that the entries in 'array_of_indices' will be in
increasing order. Also, requests that are completed while 'MPI_Waitsome' is
executing may or may not be returned, depending on the timing of the
completion of the message.

.N waitstatus

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
.N MPI_ERR_IN_STATUS
@*/
EXTERN_C
MPI_METHOD
MPI_Waitsome(
    _In_range_(>=, 0) int incount,
    _mpi_updates_(incount) MPI_Request array_of_requests[],
    _mpi_out_range_(outcount, MPI_UNDEFINED, incount) int* outcount,
    _mpi_writes_to_(incount,*outcount) int array_of_indices[],
    _Out_writes_to_opt_(incount, *outcount) MPI_Status array_of_statuses[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Waitsome(incount,TraceArrayLength(incount),(const unsigned int*)array_of_requests);

    MPID_Request * request_ptr_array[MPID_REQUEST_PTR_ARRAY_SIZE];
    MPID_Request ** request_ptrs = request_ptr_array;
    StackGuardArray<MPID_Request*> request_ptr_buf;
    MPI_Status * status_ptr;
    int i;
    int n_active;
    int n_inactive;
    int active_flag;
    int rc;
    int mpi_errno = MPI_SUCCESS;

    /* Check the arguments */
    if( incount < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", incount );;
        goto fn_fail;
    }

    if (incount != 0)
    {
        if( array_of_requests == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
            goto fn_fail;
        }
        if( array_of_indices == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_indices" );
            goto fn_fail;
        }
        /* NOTE: MPI_STATUSES_IGNORE != nullptr */
        if( array_of_statuses == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_statuses" );
            goto fn_fail;
        }
    }

    if( outcount == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "outcount" );
        goto fn_fail;
    }

    if (incount > MPID_REQUEST_PTR_ARRAY_SIZE)
    {
        request_ptr_buf = new MPID_Request*[incount];
        if( request_ptr_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        request_ptrs = request_ptr_buf;
    }

    *outcount = 0;

    /* Convert MPI request handles to a request object pointers */
    n_inactive = 0;
    for (i = 0; i < incount; i++)
    {
        if (array_of_requests[i] != MPI_REQUEST_NULL)
        {
            mpi_errno = MpiaRequestValidate( array_of_requests[i], &request_ptrs[i] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            n_inactive += 1;
            request_ptrs[i] = nullptr;
        }
    }

    if (n_inactive == incount)
    {
        *outcount = MPI_UNDEFINED;
        goto fn_exit;
    }

    /* Bill Gropp says MPI_Waitsome() is expected to try to make
       progress even if some requests have already completed;
       therefore, we kick the pipes once and then fall into a loop
       checking for completion and waiting for progress. */
    mpi_errno = MPID_Progress_pump();
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    n_active = 0;
    bool rmaFlushCalled = false;
    for(;;)
    {
        for (i = 0; i < incount; i++)
        {
            if (request_ptrs[i] == nullptr)
            {
                continue;
            }
            if (request_ptrs[i]->test_complete() == false)
            {
                if (IsRmaRequest(request_ptrs[i]) && !rmaFlushCalled)
                {
                    mpi_errno = RmaForwardProgress(request_ptrs[i], &rmaFlushCalled);
                    if (mpi_errno != MPI_SUCCESS)
                    {
                        goto fn_exit;
                    }
                }
                else
                {
                    continue;
                }
            }

            status_ptr = (array_of_statuses != MPI_STATUSES_IGNORE) ? &array_of_statuses[n_active] : MPI_STATUS_IGNORE;
            rc = MPIR_Request_complete(&array_of_requests[i], request_ptrs[i], status_ptr, &active_flag);
            if (active_flag)
            {
                array_of_indices[n_active] = i;
                n_active += 1;

                if (rc == MPI_SUCCESS)
                {
                    request_ptrs[i] = nullptr;
                }
                else
                {
                    mpi_errno = MPI_ERR_IN_STATUS;
                    if (status_ptr != MPI_STATUS_IGNORE)
                    {
                        status_ptr->MPI_ERROR = rc;
                    }
                }
            }
            else
            {
                request_ptrs[i] = nullptr;
                n_inactive += 1;
            }
        }

        if (mpi_errno == MPI_ERR_IN_STATUS)
        {
            if (array_of_statuses != MPI_STATUSES_IGNORE)
            {
                for (i = 0; i < n_active; i++)
                {
                    if (request_ptrs[array_of_indices[i]] == nullptr)
                    {
                        array_of_statuses[i].MPI_ERROR = MPI_SUCCESS;
                    }
                }
            }
            *outcount = n_active;
            break;
        }
        else if (n_active > 0)
        {
            *outcount = n_active;
            break;
        }
        else if (n_inactive == incount)
        {
            *outcount = MPI_UNDEFINED;
            break;
        }

        mpi_errno = MPID_Progress_wait();
        if (mpi_errno != MPI_SUCCESS)
            goto fn_fail;
    }

  fn_exit:
    TraceLeave_MPI_Waitsome(*outcount, TraceArrayLength(*outcount), (const unsigned int*)array_of_indices, SENTINEL_SAFE_COUNT(array_of_statuses,TraceArrayLength(*outcount)), SENTINEL_SAFE_SIZE(array_of_statuses), array_of_statuses);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_waitsome %d %p %p %p %p",
            incount,
            array_of_requests,
            outcount,
            array_of_indices,
            array_of_statuses
            )
        );
    TraceError(MPI_Waitsome, mpi_errno);
    goto fn_exit1;
}


//
// Number of threads in the queuelock FIFO.  Under normal uses, the count is > 0,
// however when a thread executes MSMPI_Waitsome_interruptible, it biases the count
// so that it uses the negative space.  Thus, a simple check for < 0 indicates whether
// a thread is in MSMPI_Waitsome_interruptible, and a check for > (LONG_MIN + 1)
// indicates that there are additional threads and MSMPI_Waitsome_interruptible should
// be interrupted.
//
// This routine does not work with requests associated with RMA operations
//
volatile LONG s_LockQueueDepth = 0;

EXTERN_C
MPI_METHOD
MSMPI_Waitsome_interruptible(
    _In_range_(>=, 0) int incount,
    _Inout_updates_opt_(incount) MPI_Request array_of_requests[],
    _Out_ _Deref_out_range_(MPI_UNDEFINED, incount) int* outcount,
    _Out_writes_to_opt_(incount,*outcount) int array_of_indices[],
    _Out_writes_to_opt_(incount,*outcount) MPI_Status array_of_statuses[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MSMPI_Waitsome_interruptible(incount,TraceArrayLength(incount),(const unsigned int*)array_of_requests);

    MPID_Request * request_ptr_array[MPID_REQUEST_PTR_ARRAY_SIZE];
    MPID_Request ** request_ptrs = request_ptr_array;
    StackGuardArray<MPID_Request*> request_ptr_buf;
    MPI_Status * status_ptr;
    int i;
    int n_active;
    int n_inactive;
    int active_flag;
    int rc;
    int mpi_errno = MPI_SUCCESS;

    /* Check the arguments */
    if( incount < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", incount );;
        goto fn_fail;
    }

    if (incount != 0)
    {
        if( array_of_requests == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_requests" );
            goto fn_fail;
        }
        if( array_of_indices == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_indices" );
            goto fn_fail;
        }
        /* NOTE: MPI_STATUSES_IGNORE != nullptr */
        if( array_of_statuses == nullptr )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "array_of_statuses" );
            goto fn_fail;
        }
    }

    if( outcount == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "outcount" );
        goto fn_fail;
    }

    /* Convert MPI request handles to a request object pointers */
    if (incount > MPID_REQUEST_PTR_ARRAY_SIZE)
    {
        request_ptr_buf = new MPID_Request*[incount];
        if( request_ptr_buf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        request_ptrs = request_ptr_buf;
    }

    *outcount = 0;

    //
    // Move the lock queue depth to the negative space so that other threads
    // can detect that we're here.  Note that we don't check for other callers
    // here as we would never make progress if two threads are both trying
    // to call into this.  Instead we always try to make a little progress.
    // The case of contention into this call will end up equivalent to MPI_Testsome.
    //
    InterlockedExchangeAdd( &s_LockQueueDepth, LONG_MIN );

    n_inactive = 0;
    for (i = 0; i < incount; i++)
    {
        if (array_of_requests[i] != MPI_REQUEST_NULL)
        {
            mpi_errno = MpiaRequestValidate( array_of_requests[i], &request_ptrs[i] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            n_inactive += 1;
            request_ptrs[i] = nullptr;
        }
    }

    if (n_inactive == incount)
    {
        *outcount = MPI_UNDEFINED;
        goto fn_exit;
    }

    /* Bill Gropp says MPI_Waitsome() is expected to try to make
       progress even if some requests have already completed;
       therefore, we kick the pipes once and then fall into a loop
       checking for completion and waiting for progress. */
    mpi_errno = MPID_Progress_pump();
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    n_active = 0;
    for(;;)
    {
        for (i = 0; i < incount; i++)
        {
            if( request_ptrs[i] == nullptr ||
                request_ptrs[i]->test_complete() == false )
            {
                continue;
            }

            status_ptr = (array_of_statuses != MPI_STATUSES_IGNORE) ? &array_of_statuses[n_active] : MPI_STATUS_IGNORE;
            rc = MPIR_Request_complete(&array_of_requests[i], request_ptrs[i], status_ptr, &active_flag);
            if (active_flag)
            {
                array_of_indices[n_active] = i;
                n_active += 1;

                if (rc == MPI_SUCCESS)
                {
                    request_ptrs[i] = nullptr;
                }
                else
                {
                    mpi_errno = MPI_ERR_IN_STATUS;
                    if (status_ptr != MPI_STATUS_IGNORE)
                    {
                        status_ptr->MPI_ERROR = rc;
                    }
                }
            }
            else
            {
                request_ptrs[i] = nullptr;
                n_inactive += 1;
            }
        }

        if (mpi_errno == MPI_ERR_IN_STATUS)
        {
            if (array_of_statuses != MPI_STATUSES_IGNORE)
            {
                for (i = 0; i < n_active; i++)
                {
                    if (request_ptrs[array_of_indices[i]] == nullptr)
                    {
                        array_of_statuses[i].MPI_ERROR = MPI_SUCCESS;
                    }
                }
            }
            *outcount = n_active;
            break;
        }
        else if (n_active > 0)
        {
            *outcount = n_active;
            break;
        }
        else if (n_inactive == incount)
        {
            *outcount = MPI_UNDEFINED;
            break;
        }

        mpi_errno = MPID_Progress_wait_interruptible();
        if( mpi_errno == MPI_ERR_PENDING )
        {
            *outcount = 0;
            mpi_errno = MPI_SUCCESS;
            break;
        }
        else if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

  fn_exit:
    TraceLeave_MSMPI_Waitsome_interruptible(*outcount, TraceArrayLength(*outcount), (const unsigned int*)array_of_indices, SENTINEL_SAFE_COUNT(array_of_statuses,TraceArrayLength(*outcount)), SENTINEL_SAFE_SIZE(array_of_statuses), array_of_statuses);
  fn_exit1:
    //
    // Note that -LONG_MIN cannot be represented as a positive value, and is identical in
    // binary representation as LONG_MIN.  Thus, adding either LONG_MIN or -LONG_MIN simply
    // toggles the MSB.
    //
    InterlockedExchangeAdd( &s_LockQueueDepth, -LONG_MIN );

    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        nullptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**msmpi_waitsome_interruptible %d %p %p %p %p",
            incount,
            array_of_requests,
            outcount,
            array_of_indices,
            array_of_statuses
            )
        );
    TraceError(MSMPI_Waitsome_interruptible, mpi_errno);
    goto fn_exit1;
}


static MSMPI_Lock_queue* volatile s_pLockQueueTail = nullptr;

EXTERN_C
void
MPIAPI
MSMPI_Queuelock_acquire( __out MSMPI_Lock_queue* queue )
{
    TraceEnter_MSMPI_Queuelock_acquire();

    MPIU_Assert( queue != nullptr );

    //
    // Must clear the next pointer here as it could be set as soon as we
    // return from the interlocked exchange below.
    //
    queue->next = nullptr;

    LONG queueDepth = InterlockedIncrement( &s_LockQueueDepth );

    MSMPI_Lock_queue* pOldTail = static_cast<MSMPI_Lock_queue*>(
        InterlockedExchangePointer(
            reinterpret_cast<VOID* volatile *>(&s_pLockQueueTail),
            queue
            )
        );
    if( pOldTail == nullptr )
    {
        //
        // We're the first ones, so we have the lock.
        //
        queue->flags = 0;
        goto fn_exit;
    }

    //
    // Set our sentinel bit and chain ourselves.  We will spin until the bit is
    // cleared by the caller ahead of us.  We must make sure the flag is set before
    // we chain or the current tail could clear it before we set it and we'd be
    // deadlocked.  Note that flags and next are both declared volatile, and thus
    // cannot be re-ordered - no additional memory barriers are needed here.
    //
    queue->flags = 1;
    MPIU_Assert( pOldTail->next == nullptr );
    pOldTail->next = queue;

    if( queueDepth < 0 )
    {
        ExPostCompletion( MPIDI_CH3I_set, EX_KEY_WAIT_INTERRUPT, nullptr, 0 );
    }

    while( queue->flags != 0 )
    {
        SwitchToThread();
    }

fn_exit:
    TraceLeave_MSMPI_Queuelock_acquire();
    return;
}


EXTERN_C
void
MPIAPI
MSMPI_Queuelock_release( __in MSMPI_Lock_queue* queue )
{
    TraceEnter_MSMPI_Queuelock_release();

    MPIU_Assert( queue != nullptr );

    if( queue->next == nullptr )
    {
        const MSMPI_Lock_queue* pNewTail = static_cast<const MSMPI_Lock_queue*>(
            InterlockedCompareExchangePointer(
                reinterpret_cast<VOID* volatile *>(&s_pLockQueueTail),
                nullptr,
                queue
                )
            );

        if( pNewTail == queue )
        {
            //
            // We were at the tail, nobody else follows.
            //
            MPIU_Assert( queue->next == nullptr );
            goto fn_exit;
        }

        //
        // Wait for the lock chain to be valid.
        //
        while( queue->next == nullptr )
        {
            SwitchToThread();
        }
    }

    MPIU_Assert( queue->next != nullptr );
    MPIU_Assert( queue->next->flags != 0 );
    queue->next->flags = 0;

fn_exit:
    InterlockedDecrement( &s_LockQueueDepth );
    TraceLeave_MSMPI_Queuelock_release();
}
