// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
MPI_Bcast - Broadcasts a message from the process with rank "root" to
            all other processes of the communicator

Input/Output Parameter:
. buffer - starting address of buffer (choice)

Input Parameters:
+ count - number of entries in buffer (integer)
. datatype - data type of buffer (handle)
. root - rank of broadcast root (integer)
- comm - communicator (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_ROOT
@*/
EXTERN_C
MPI_METHOD
MPI_Bcast(
    _Pre_opt_valid_ void* buffer,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Bcast(comm,buffer,datatype,count,root);

    TypeHandle hType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate( buffer, count, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Ibcast_intra(
                buffer,
                count,
                hType,
                root,
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
            mpi_errno = MPIR_Bcast_intra(
                buffer,
                count,
                hType,
                root,
                comm_ptr
                );
        }
    }
    else
    {
        //
        // The root for an intercommunicator broadcast is either the rank
        // of the root process in the remote group, MPI_ROOT if the local
        // rank is the root, or MPI_PROC_NULL if some other process in the
        // local group is the root.
        //
        C_ASSERT( MPI_ROOT < MPI_PROC_NULL );
        if( root < MPI_ROOT || root == MPI_ANY_SOURCE || root >= comm_ptr->remote_size )
        {
            return MPIU_ERR_CREATE(MPI_ERR_ROOT, "**root %d", root );
        }

        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Ibcast_inter(
                buffer,
                count,
                hType,
                root,
                const_cast<MPID_Comm*>(comm_ptr),
                &creq_ptr
                );
            if (mpi_errno == MPI_SUCCESS)
            {
                mpi_errno = MPIR_Wait(creq_ptr);
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Bcast_inter(
                buffer,
                count,
                hType,
                root,
                comm_ptr
                );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Bcast();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_bcast %p %d %D %d %C",
            buffer,
            count,
            datatype,
            root,
            comm
            )
        );
    TraceError(MPI_Bcast, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Ibcast - Broadcasts a message from the process with rank "root" to
             all other processes of the communicator in a nonblocking way

Input/Output Parameters:
. buffer - starting address of buffer (choice)

Input Parameters:
+ count - number of entries in buffer (non-negative integer)
. datatype - datatype of buffer (handle)
. root - rank of broadcast root (integer)
- comm - communicator (handle)

Output Parameters:
. request - communication request (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_ROOT
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Ibcast(
    _Pre_opt_valid_ void* buffer,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ibcast( buffer, count, datatype, root, comm );

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

    TypeHandle hType;
    mpi_errno = MpiaDatatypeValidate( buffer, count, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request *pReq;

    if (pComm->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( pComm, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        mpi_errno = MPIR_Ibcast_intra(
            buffer,
            count,
            hType,
            root,
            pComm,
            &pReq
            );
    }
    else
    {
        //
        // The root for an intercommunicator broadcast is either the rank
        // of the root process in the remote group, MPI_ROOT if the local
        // rank is the root, or MPI_PROC_NULL if some other process in the
        // local group is the root.
        //
        C_ASSERT( MPI_ROOT < MPI_PROC_NULL );
        if( root < MPI_ROOT || root == MPI_ANY_SOURCE || root >= pComm->remote_size )
        {
            return MPIU_ERR_CREATE(MPI_ERR_ROOT, "**root %d", root );
        }

        mpi_errno = MPIR_Ibcast_inter(
            buffer,
            count,
            hType,
            root,
            pComm,
            &pReq
            );
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

    TraceLeave_MPI_Ibcast( *request );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(pComm),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ibcast %p %d %D %d %C %p",
            buffer,
            count,
            datatype,
            root,
            comm,
            request
            )
        );
    TraceError(MPI_Ibcast, mpi_errno);
    goto fn_exit;
}
