// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@

MPI_Scatter - Sends data from one process to all other processes in a
communicator

Input Parameters:
+ sendbuf - address of send buffer (choice, significant
only at 'root')
. sendcount - number of elements sent to each process
(integer, significant only at 'root')
. sendtype - data type of send buffer elements (significant only at 'root')
(handle)
. recvcount - number of elements in receive buffer (integer)
. recvtype - data type of receive buffer elements (handle)
. root - rank of sending process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(sendbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Scatter(
    _In_range_(!=, recvbuf) _In_opt_ const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _When_(root != MPI_PROC_NULL, _Out_opt_) void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Scatter(comm,sendbuf,recvbuf,sendtype,recvtype,sendcount,recvcount,root);

    TypeHandle hSendType;
    TypeHandle hRecvType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
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

        if( comm_ptr->rank == root )
        {
            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            /* catch common aliasing cases */
            mpi_errno = MpiaBufferValidateAliasing(
                sendbuf,
                sendtype,
                recvbuf,
                recvtype,
                sendcount
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            if( recvcount > 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );;
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        if (recvbuf != MPI_IN_PLACE)
        {
            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        MPID_Request* creq_ptr;
        mpi_errno = MPIR_Iscatter_intra(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            root,
            comm_ptr,
            MPIR_SCATTER_TAG,
            &creq_ptr
            );
        if( mpi_errno == MPI_SUCCESS )
        {
            mpi_errno = MPIR_Wait( creq_ptr );
            creq_ptr->Release();
        }
    }
    else
    {
        if (root == MPI_ROOT)
        {
            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if (root != MPI_PROC_NULL)
        {
            mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( recvcount > 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );;
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            goto fn_done;
        }

        MPID_Request* creq_ptr;
        mpi_errno = MPIR_Iscatter_inter(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            root,
            comm_ptr,
            MPIR_SCATTER_TAG,
            &creq_ptr
            );
        if( mpi_errno == MPI_SUCCESS )
        {
            mpi_errno = MPIR_Wait( creq_ptr );
            creq_ptr->Release();
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

fn_done:
    TraceLeave_MPI_Scatter();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_scatter %p %d %D %p %d %D %d %C",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            root,
            comm
            )
        );
    TraceError(MPI_Scatter, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Iscatter - Sends data from one process to all other processes in a
               communicator in a nonblocking way.

Input Parameters:
+ sendbuf - address of send buffer (choice, significant
only at 'root')
. sendcount - number of elements sent to each process
(integer, significant only at 'root')
. sendtype - data type of send buffer elements (significant only at 'root')
(handle)
. recvcount - number of elements in receive buffer (integer)
. recvtype - data type of receive buffer elements (handle)
. root - rank of sending process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(sendbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Iscatter(
    _In_range_(!=, recvbuf) _In_opt_ const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _When_(root != MPI_PROC_NULL, _Out_opt_) void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iscatter(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        sendcount,
        recvcount,
        root
        );

    MPID_Comm* pComm;
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

    TypeHandle hSendType;
    TypeHandle hRecvType;
    MPID_Request* pReq;
    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        mpi_errno = MpiaCommValidateRoot( pComm, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if( pComm->rank == root )
        {
            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            mpi_errno = MpiaBufferValidateAliasing(
                sendbuf,
                sendtype,
                recvbuf,
                recvtype,
                sendcount
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            if( recvcount > 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );;
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        if( recvbuf != MPI_IN_PLACE )
        {
            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        mpi_errno = MPIR_Iscatter_intra(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            root,
            pComm,
            MPIR_GET_ASYNC_TAG(MPIR_SCATTER_TAG),
            &pReq
            );
    }
    else
    {
        if( root == MPI_ROOT )
        {
            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if( root != MPI_PROC_NULL )
        {
            mpi_errno = MpiaCommValidateRoot( pComm, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( recvcount > 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );;
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        mpi_errno = MPIR_Iscatter_inter(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            root,
            pComm,
            MPIR_GET_ASYNC_TAG(MPIR_SCATTER_TAG),
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

    TraceLeave_MPI_Iscatter( *request );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iscatter %p %d %D %p %d %D %d %C %p",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            root,
            comm,
            request
            )
        );
    TraceError(MPI_Iscatter, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Scatterv - Scatters a buffer in parts to all processes in a communicator

Input Parameters:
+ sendbuf - address of send buffer (choice, significant only at 'root')
. sendcounts - integer array (of length group size)
specifying the number of elements to send to each processor
. displs - integer array (of length group size). Entry
 'i'  specifies the displacement (relative to sendbuf  from
which to take the outgoing data to process  'i'
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements in receive buffer (integer)
. recvtype - data type of receive buffer elements (handle)
. root - rank of sending process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(sendbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Scatterv(
    _In_opt_ const void* sendbuf,
    _In_opt_ const int sendcounts[],
    _In_opt_ const int displs[],
    _In_ MPI_Datatype sendtype,
    _When_(root != MPI_PROC_NULL, _Out_opt_) void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Scatterv(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        TraceArrayLength(1),
        sendcounts,
        recvcount,
        TraceArrayLength(1),
        displs,
        root
        );

    TypeHandle hSendType;
    TypeHandle hRecvType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    int i, comm_size, rank;

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        rank = comm_ptr->rank;
        comm_size = comm_ptr->remote_size;

        if (rank == root)
        {
            int sendcount_sentinel = 0;
            for (i=0; i<comm_size; i++)
            {
                if( sendcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", sendcounts[i] );
                    goto fn_fail;
                }
                sendcount_sentinel |= sendcounts[i];

                if( recvbuf != MPI_IN_PLACE )
                {
                    mpi_errno = MpiaBufferValidateAliasing(
                        static_cast<const UINT8*>( sendbuf ) + displs[i],
                        sendtype,
                        recvbuf,
                        recvtype,
                        sendcounts[i]
                        );
                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto fn_fail;
                    }
                }
            }

            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount_sentinel,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount_sentinel > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }
        }
        else
        {
            if( recvcount != 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        if( recvbuf != MPI_IN_PLACE && recvcount != 0 )
        {
            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
    }
    else
    {
        if (root == MPI_ROOT)
        {
            comm_size = comm_ptr->remote_size;
            int sendcount_sentinel = 0;
            for( i = 0; i < comm_size; i++ )
            {
                if( sendcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", sendcounts[i] );
                    goto fn_fail;
                }
                sendcount_sentinel |= sendcounts[i];
            }

            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount_sentinel,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount_sentinel > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if (root != MPI_PROC_NULL)
        {
            mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( recvcount != 0 )
            {
                mpi_errno = MpiaDatatypeValidate(
                    recvbuf,
                    recvcount,
                    recvtype,
                    "recvtype",
                    &hRecvType
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto fn_fail;
                }
            }
            else
            {
                hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( recvcount != 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );;
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            goto fn_done;
        }
    }

    if( Mpi.ForceAsyncWorkflow )
    {
        MPID_Request* creq_ptr;

        mpi_errno = MPIR_Iscatterv(
            sendbuf,
            sendcounts,
            displs,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            root,
            const_cast<MPID_Comm*>( comm_ptr ),
            &creq_ptr
            );
        if( mpi_errno == MPI_SUCCESS )
        {
            mpi_errno = MPIR_Wait( creq_ptr );
            creq_ptr->Release();
        }
    }
    else
    {
        mpi_errno = MPIR_Scatterv(
            sendbuf,
            sendcounts,
            displs,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            root,
            comm_ptr
            );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

fn_done:
    TraceLeave_MPI_Scatterv();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_scatterv %p %p %p %D %p %d %D %d %C",
            sendbuf,
            sendcounts,
            displs,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            root,
            comm
            )
        );
    TraceError(MPI_Scatterv, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Iscatterv - Scatters a buffer in parts to all processes in a communicator
                in a nonblocking way.

Input Parameters:
+ sendbuf - address of send buffer (choice, significant only at 'root')
. sendcounts - integer array (of length group size)
specifying the number of elements to send to each processor
. displs - integer array (of length group size). Entry
 'i'  specifies the displacement (relative to sendbuf  from
which to take the outgoing data to process  'i'
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements in receive buffer (integer)
. recvtype - data type of receive buffer elements (handle)
. root - rank of sending process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(sendbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Iscatterv(
    _In_opt_ const void* sendbuf,
    _In_opt_ const int sendcounts[],
    _In_opt_ const int displs[],
    _In_ MPI_Datatype sendtype,
    _When_(root != MPI_PROC_NULL, _Out_opt_) void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iscatterv(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        TraceArrayLength(1),
        sendcounts,
        recvcount,
        TraceArrayLength(1),
        displs,
        root
        );

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

    TypeHandle hSendType;
    TypeHandle hRecvType;
    int i;
    int comm_size;
    int rank;
    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        mpi_errno = MpiaCommValidateRoot( pComm, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        rank = pComm->rank;
        comm_size = pComm->remote_size;

        if( rank == root )
        {
            int sendcount_sentinel = 0;
            for( i = 0; i < comm_size; i++ )
            {
                if( sendcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", sendcounts[i] );
                    goto fn_fail;
                }
                sendcount_sentinel |= sendcounts[i];

                if( recvbuf != MPI_IN_PLACE )
                {
                    mpi_errno = MpiaBufferValidateAliasing(
                        static_cast<const UINT8*>( sendbuf ) + displs[i],
                        sendtype,
                        recvbuf,
                        recvtype,
                        sendcounts[i]
                        );
                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto fn_fail;
                    }
                }
            }

            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount_sentinel,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount_sentinel > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }
        }
        else
        {
            if( recvcount != 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        if( recvbuf != MPI_IN_PLACE && recvcount != 0 )
        {
            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
    }
    else
    {
        if( root == MPI_ROOT )
        {
            comm_size = pComm->remote_size;
            int sendcount_sentinel = 0;
            for( i = 0; i < comm_size; i++ )
            {
                if( sendcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", sendcounts[i] );
                    goto fn_fail;
                }
                sendcount_sentinel |= sendcounts[i];
            }

            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcount_sentinel,
                sendtype,
                "sendtype",
                &hSendType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendcount_sentinel > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if( root != MPI_PROC_NULL )
        {
            mpi_errno = MpiaCommValidateRoot( pComm, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( recvcount != 0 )
            {
                mpi_errno = MpiaDatatypeValidate(
                    recvbuf,
                    recvcount,
                    recvtype,
                    "recvtype",
                    &hRecvType
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    goto fn_fail;
                }
            }
            else
            {
                hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( recvcount != 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );;
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
    }

    MPID_Request* pReq;
    mpi_errno = MPIR_Iscatterv(
        sendbuf,
        sendcounts,
        displs,
        hSendType,
        recvbuf,
        recvcount,
        hRecvType,
        root,
        pComm,
        &pReq
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user.
    //
    MPIU_Assert( pReq != nullptr );
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Iscatterv( *request );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iscatterv %p %p %p %D %p %d %D %d %C %p",
            sendbuf,
            sendcounts,
            displs,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            root,
            comm,
            request
            )
        );
    TraceError( MPI_Iscatterv, mpi_errno );
    goto fn_exit;
}

