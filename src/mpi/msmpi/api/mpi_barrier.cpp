// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@

MPI_Barrier - Blocks until all processes in the communicator have
reached this routine.

Input Parameter:
. comm - communicator (handle)

Notes:
Blocks the caller until all processes in the communicator have called it;
that is, the call returns at any process only after all members of the
communicator have entered the call.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Barrier(
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Barrier(comm);

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;
            mpi_errno = MPIR_Ibarrier_intra(
                                const_cast<MPID_Comm*>(comm_ptr),
                                &creq_ptr
                                );
            if( mpi_errno == MPI_SUCCESS )
            {
                mpi_errno = MPIR_Wait(creq_ptr);
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Barrier_intra( comm_ptr );
        }
    }
    else
    {
        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;
            mpi_errno = MPIR_Ibarrier_inter(
                                const_cast<MPID_Comm*>(comm_ptr),
                                &creq_ptr
                                );
            if( mpi_errno == MPI_SUCCESS )
            {
                mpi_errno = MPIR_Wait(creq_ptr);
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Barrier_inter( comm_ptr );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Barrier();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_barrier %C",
            comm
            )
        );
    TraceError(MPI_Barrier, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Ibarrier - Blocks until all processes in the communicator have
reached this routine in a nonblocking way.

Input Parameter:
. comm - communicator (handle)

Output Parameters:
. request - communication request (handle)

Notes:
Blocks the caller until all processes in the communicator have called it;
that is, the call returns at any process only after all members of the
communicator have entered the call.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_ERR_ARG
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Ibarrier(
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ibarrier( comm );

    MPID_Comm *pComm;
    int mpi_errno = MpiaCommValidateHandle( comm, &pComm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    MPID_Request *pReq;
    if ( pComm->comm_kind != MPID_INTERCOMM )
    {
        mpi_errno = MPIR_Ibarrier_intra( pComm, &pReq );
    }
    else
    {
        mpi_errno = MPIR_Ibarrier_inter( pComm, &pReq );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user
    //
    MPIU_Assert( pReq != nullptr );
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ibarrier( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ibarrier %C %p",
            comm,
            request
            )
        );
    TraceError(MPI_Ibarrier, mpi_errno);
    goto fn_exit;
}

