// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


static MPI_RESULT
GatherValidateRootParams(
    _Pre_maybenull_ const void *sendbuf,
    _In_ int sendcount,
    _In_ MPI_Datatype sendtype,
    _Out_ TypeHandle* phSendType,
    _Pre_maybenull_ void *recvbuf,
    _In_ int recvcount,
    _In_ MPI_Datatype recvtype,
    _Out_ TypeHandle* phRecvType
    )
{
    if( recvbuf == MPI_IN_PLACE )
    {
        return MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
    }

    int mpi_errno = MpiaDatatypeValidate(
        recvbuf,
        recvcount,
        recvtype,
        "recvtype",
        phRecvType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MpiaDatatypeValidate(
            sendbuf,
            sendcount,
            sendtype,
            "sendtype",
            phSendType
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    else
    {
        *phSendType = g_hBuiltinTypes.MPI_Datatype_null;
    }
    return MPI_SUCCESS;
}


/*@

MPI_Gather - Gathers together values from a group of processes

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements for any single receive (integer,
significant only at root)
. recvtype - data type of recv buffer elements
(significant only at root) (handle)
. root - rank of receiving process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice, significant only at 'root')

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
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Gather(
    _In_opt_ _When_(sendtype == recvtype, _In_range_(!=, recvbuf)) const void* sendbuf,
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
    TraceEnter_MPI_Gather(comm,sendbuf,recvbuf,sendtype,recvtype,sendcount,recvcount,root);

    TypeHandle hSendType;
    TypeHandle hRecvType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
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
            mpi_errno = GatherValidateRootParams(
                sendbuf,
                sendcount,
                sendtype,
                &hSendType,
                recvbuf,
                recvcount,
                recvtype,
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaBufferValidateAliasing(
                sendbuf,
                sendtype,
                recvbuf,
                recvtype,
                recvcount
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            //
            // Receive parameter are ignored.  MPI_IN_PLACE is only valid at the root.
            //
            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

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

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Igather_intra(
                sendbuf,
                sendcount,
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
            mpi_errno = MPIR_Gather_intra(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcount,
                hRecvType,
                root,
                comm_ptr
                );
        }
    }
    else
    {
        if( root == MPI_ROOT )
        {
            if( recvcount > 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
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

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if( root != MPI_PROC_NULL )
        {
            mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

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

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            goto fn_done;
        }

        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Igather_inter(
                sendbuf,
                sendcount,
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
            mpi_errno = MPIR_Gather_inter(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcount,
                hRecvType,
                root,
                comm_ptr
                );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

fn_done:
    TraceLeave_MPI_Gather();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_gather %p %d %D %p %d %D %d %C",
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
    TraceError(MPI_Gather, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Igather - Gathers together values from a group of processes
              in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements for any single receive (integer,
significant only at root)
. recvtype - data type of recv buffer elements
(significant only at root) (handle)
. root - rank of receiving process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice, significant only at 'root')
. request - communication request (handle)

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
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Igather(
    _In_opt_ _When_(sendtype == recvtype, _In_range_(!=, recvbuf)) const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Igather(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        sendcount,
        recvcount,
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
    MPID_Request *pReq;
    if (pComm->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( pComm, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if( pComm->rank == root )
        {
            mpi_errno = GatherValidateRootParams(
                sendbuf,
                sendcount,
                sendtype,
                &hSendType,
                recvbuf,
                recvcount,
                recvtype,
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaBufferValidateAliasing(
                sendbuf,
                sendtype,
                recvbuf,
                recvtype,
                recvcount
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            //
            // Receive parameter are ignored. MPI_IN_PLACE is only valid at the root.
            //
            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

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

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        mpi_errno = MPIR_Igather_intra(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            root,
            pComm,
            &pReq
            );
    }
    else
    {
        if( root == MPI_ROOT )
        {
            if( recvcount > 0 && recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
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

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if( root != MPI_PROC_NULL )
        {
            mpi_errno = MpiaCommValidateRoot( pComm, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( sendcount > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

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

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }

        mpi_errno = MPIR_Igather_inter(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
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

    TraceLeave_MPI_Igather( *request );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_igather %p %d %D %p %d %D %d %C %p",
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
    TraceError(MPI_Igather, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Gatherv - Gathers into specified locations from all processes in a group

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcounts - integer array (of length group size)
containing the number of elements that are received from each process
(significant only at 'root')
. displs - integer array (of length group size). Entry
 'i'  specifies the displacement relative to recvbuf  at
which to place the incoming data from process  'i'  (significant only
at root)
. recvtype - data type of recv buffer elements
(significant only at 'root') (handle)
. root - rank of receiving process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice, significant only at 'root')

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Gatherv(
    _In_opt_ const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _When_(root != MPI_PROC_NULL, _Out_opt_) void* recvbuf,
    _In_opt_ const int recvcounts[],
    _In_opt_ const int displs[],
    _In_ MPI_Datatype recvtype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Gatherv(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        sendcount,
        TraceArrayLength(1),
        recvcounts,
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

    int i, rank;

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        rank = comm_ptr->rank;
        if( rank == root )
        {
            int recvcount = 0;
            for( i = comm_ptr->remote_size - 1; i >= 0; i-- )
            {
                if( recvcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
                    goto fn_fail;
                }
                recvcount |= recvcounts[i];

                if( sendbuf != MPI_IN_PLACE )
                {
                    mpi_errno = MpiaBufferValidateAliasing(
                        sendbuf,
                        sendtype,
                        static_cast<UINT8*>( recvbuf ) + displs[i],
                        recvtype,
                        recvcounts[i]
                        );
                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto fn_fail;
                    }
                }
            }

            mpi_errno = GatherValidateRootParams(
                sendbuf,
                sendcount,
                sendtype,
                &hSendType,
                recvbuf,
                recvcount,
                recvtype,
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            //
            // Receive parameters are ignored.  MPI_IN_PLACE only allowed at root.
            //
            if( sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

            if( sendcount != 0 )
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
            }
            else
            {
                hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
    }
    else
    {
        if (root == MPI_ROOT)
        {
            int recvcount_sentinel = 0;
            for( i = comm_ptr->remote_size - 1; i >= 0; i-- )
            {
                if( recvcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
                    goto fn_fail;
                }
                recvcount_sentinel |= recvcounts[i];
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount_sentinel,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if (root != MPI_PROC_NULL)
        {
            mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

            if( sendcount != 0 )
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
            }
            else
            {
                hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            goto fn_done;
        }
    }

    if( Mpi.ForceAsyncWorkflow )
    {
        MPID_Request* creq_ptr;

        mpi_errno = MPIR_Igatherv(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcounts,
            displs,
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
        mpi_errno = MPIR_Gatherv(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcounts,
            displs,
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
    TraceLeave_MPI_Gatherv();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_gatherv %p %d %D %p %p %p %D %d %C",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            root,
            comm
            )
        );
    TraceError(MPI_Gatherv, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Igatherv - Gathers into specified locations from all processes in a group
               in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcounts - integer array (of length group size)
containing the number of elements that are received from each process
(significant only at 'root')
. displs - integer array (of length group size). Entry
 'i'  specifies the displacement relative to recvbuf  at
which to place the incoming data from process  'i'  (significant only
at root)
. recvtype - data type of recv buffer elements
(significant only at 'root') (handle)
. root - rank of receiving process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice, significant only at 'root')
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Igatherv(
    _In_opt_ const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_opt_ const int recvcounts[],
    _In_opt_ const int displs[],
    _In_ MPI_Datatype recvtype,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Igatherv(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        sendcount,
        TraceArrayLength(1),
        recvcounts,
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
    int rank;
    if (pComm->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( pComm, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        rank = pComm->rank;
        if( rank == root )
        {
            int recvcount = 0;
            for( i = pComm->remote_size - 1; i >= 0; i-- )
            {
                if( recvcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
                    goto fn_fail;
                }
                recvcount |= recvcounts[i];

                if( sendbuf != MPI_IN_PLACE )
                {
                    mpi_errno = MpiaBufferValidateAliasing(
                        sendbuf,
                        sendtype,
                        static_cast<UINT8*>( recvbuf ) + displs[i],
                        recvtype,
                        recvcounts[i]
                        );
                    if( mpi_errno != MPI_SUCCESS )
                    {
                        goto fn_fail;
                    }
                }
            }

            mpi_errno = GatherValidateRootParams(
                sendbuf,
                sendcount,
                sendtype,
                &hSendType,
                recvbuf,
                recvcount,
                recvtype,
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            //
            // Receive parameters are ignored. MPI_IN_PLACE only allowed at root.
            //
            if( sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

            if( sendcount != 0 )
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
            }
            else
            {
                hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
    }
    else
    {
        if (root == MPI_ROOT)
        {
            int recvcount_sentinel = 0;
            for( i = pComm->remote_size - 1; i >= 0; i-- )
            {
                if( recvcounts[i] < 0 )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
                    goto fn_fail;
                }
                recvcount_sentinel |= recvcounts[i];
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidate(
                recvbuf,
                recvcount_sentinel,
                recvtype,
                "recvtype",
                &hRecvType
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else if (root != MPI_PROC_NULL)
        {
            mpi_errno = MpiaCommValidateRoot( pComm, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            //
            // MPI_IN_PLACE is only valid for intracomms.
            //
            if( sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
                goto fn_fail;
            }

            if( sendcount != 0 )
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
            }
            else
            {
                hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            }

            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
        else
        {
            hSendType = g_hBuiltinTypes.MPI_Datatype_null;
            hRecvType = g_hBuiltinTypes.MPI_Datatype_null;
        }
    }

    MPID_Request* pReq;
    mpi_errno = MPIR_Igatherv(
        sendbuf,
        sendcount,
        hSendType,
        recvbuf,
        recvcounts,
        displs,
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

    TraceLeave_MPI_Igatherv( *request );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_igatherv %p %d %D %p %p %p %D %d %C %p",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            root,
            comm,
            request
            )
        );
    TraceError(MPI_Igatherv, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Allgather - Gathers data from all tasks and distribute the combined
    data to all tasks

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements received from any process (integer)
. recvtype - data type of receive buffer elements (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)

Notes:
 The MPI standard (1.0 and 1.1) says that
.n
.n
 The jth block of data sent from  each proess is received by every process
 and placed in the jth block of the buffer 'recvbuf'.
.n
.n
 This is misleading; a better description is
.n
.n
 The block of data sent from the jth process is received by every
 process and placed in the jth block of the buffer 'recvbuf'.
.n
.n
 This text was suggested by Rajeev Thakur and has been adopted as a
 clarification by the MPI Forum.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Allgather(
    _In_opt_ _When_(sendtype == recvtype, _In_range_(!=, recvbuf)) const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Allgather(comm,sendbuf,recvbuf,sendtype,recvtype,sendcount,recvcount);

    TypeHandle hSendType;
    TypeHandle hRecvType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Every process is root.
    //
    mpi_errno = GatherValidateRootParams(
        sendbuf,
        sendcount,
        sendtype,
        &hSendType,
        recvbuf,
        recvcount,
        recvtype,
        &hRecvType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Iallgather_intra(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcount,
                hRecvType,
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
            mpi_errno = MPIR_Allgather_intra(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcount,
                hRecvType,
                comm_ptr
                );
        }
    }
    else
    {
        //
        // MPI_IN_PLACE is only valid for intracomms.
        //
        if( sendbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }

        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Iallgather_inter(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcount,
                hRecvType,
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
            mpi_errno = MPIR_Allgather_inter(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcount,
                hRecvType,
                comm_ptr
                );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Allgather();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_allgather %p %d %D %p %d %D %C",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            comm
            )
        );
    TraceError(MPI_Allgather, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Iallgather - Gathers data from all tasks and distribute the combined
    data to all tasks in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements received from any process (integer)
. recvtype - data type of receive buffer elements (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)
. request - communication handle (handle)

Notes:
 The MPI standard (1.0 and 1.1) says that
.n
.n
 The jth block of data sent from  each proess is received by every process
 and placed in the jth block of the buffer 'recvbuf'.
.n
.n
 This is misleading; a better description is
.n
.n
 The block of data sent from the jth process is received by every
 process and placed in the jth block of the buffer 'recvbuf'.
.n
.n
 This text was suggested by Rajeev Thakur and has been adopted as a
 clarification by the MPI Forum.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Iallgather(
    _In_opt_ _When_(sendtype == recvtype, _In_range_(!=, recvbuf)) const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype recvtype,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iallgather(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        sendcount,
        recvcount
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

    //
    // Every process is root.
    //
    mpi_errno = GatherValidateRootParams(
        sendbuf,
        sendcount,
        sendtype,
        &hSendType,
        recvbuf,
        recvcount,
        recvtype,
        &hRecvType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request* pReq;

    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        mpi_errno = MPIR_Iallgather_intra(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            pComm,
            &pReq
            );
    }
    else
    {
        //
        // MPI_IN_PLACE is only valid for intracomms.
        //
        if( sendbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }

        mpi_errno = MPIR_Iallgather_inter(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            pComm,
            &pReq
            );
    }

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

    TraceLeave_MPI_Iallgather( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iallgather %p %d %D %p %d %D %C %p",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            comm,
            request
            )
        );
    TraceError( MPI_Iallgather, mpi_errno );
    goto fn_exit;
}


/*@

MPI_Allgatherv - Gathers data from all tasks and deliver the combined data
                 to all tasks

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcounts - integer array (of length group size)
containing the number of elements that are to be received from each process
. displs - integer array (of length group size). Entry
 'i'  specifies the displacement (relative to recvbuf ) at
which to place the incoming data from process  'i'
. recvtype - data type of receive buffer elements (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_ERR_BUFFER
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Allgatherv(
    _In_opt_ const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ const int displs[],
    _In_ MPI_Datatype recvtype,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Allgatherv(
                    comm,
                    sendbuf,
                    recvbuf,
                    sendtype,
                    recvtype,
                    sendcount,
                    TraceArrayLength(1),
                    recvcounts,
                    TraceArrayLength(1),
                    displs
                    );

    TypeHandle hSendType;
    TypeHandle hRecvType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    int recvcount = 0;
    int comm_size = comm_ptr->remote_size;
    for( int i=0; i < comm_size; i++ )
    {
        if( recvcounts[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
            goto fn_fail;
        }

        recvcount |= recvcounts[i];
    }

    //
    // Every process is root.
    //
    mpi_errno = GatherValidateRootParams(
        sendbuf,
        sendcount,
        sendtype,
        &hSendType,
        recvbuf,
        recvcount,
        recvtype,
        &hRecvType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Iallgatherv_intra(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcounts,
                displs,
                hRecvType,
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
            mpi_errno = MPIR_Allgatherv_intra(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcounts,
                displs,
                hRecvType,
                comm_ptr
                );
        }
    }
    else
    {
        //
        // MPI_IN_PLACE is only valid for intracomms.
        //
        if( sendbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }

        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Iallgatherv_inter(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcounts,
                displs,
                hRecvType,
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
            mpi_errno = MPIR_Allgatherv_inter(
                sendbuf,
                sendcount,
                hSendType,
                recvbuf,
                recvcounts,
                displs,
                hRecvType,
                comm_ptr
                );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Allgatherv();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_allgatherv %p %d %D %p %p %p %D %C",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            comm
            )
        );
    TraceError(MPI_Allgatherv, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Iallgatherv - Gathers data from all tasks and deliver the combined data
                 to all tasks in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements in send buffer (integer)
. sendtype - data type of send buffer elements (handle)
. recvcounts - integer array (of length group size)
containing the number of elements that are to be received from each process
. displs - integer array (of length group size). Entry
 'i'  specifies the displacement (relative to recvbuf ) at
which to place the incoming data from process  'i'
. recvtype - data type of receive buffer elements (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_ERR_ARG
.N MPI_ERR_BUFFER
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Iallgatherv(
    _In_opt_ const void* sendbuf,
    _In_range_(>=, 0) int sendcount,
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ const int displs[],
    _In_ MPI_Datatype recvtype,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iallgatherv(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        sendcount,
        TraceArrayLength(1),
        recvcounts,
        TraceArrayLength(1),
        displs
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

    int recvcount = 0;
    for( int i = 0; i < pComm->remote_size; i++ )
    {
        if( recvcounts[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
            goto fn_fail;
        }

        recvcount |= recvcounts[i];
    }

    TypeHandle hSendType;
    TypeHandle hRecvType;

    //
    // Every process is root.
    //
    mpi_errno = GatherValidateRootParams(
        sendbuf,
        sendcount,
        sendtype,
        &hSendType,
        recvbuf,
        recvcount,
        recvtype,
        &hRecvType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Request* pReq;

    if (pComm->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MPIR_Iallgatherv_intra(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcounts,
            displs,
            hRecvType,
            pComm,
            &pReq
            );
    }
    else
    {
        //
        // MPI_IN_PLACE is only valid for intracomms.
        //
        if( sendbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }

        mpi_errno = MPIR_Iallgatherv_inter(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcounts,
            displs,
            hRecvType,
            pComm,
            &pReq
            );
    }

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

    TraceLeave_MPI_Iallgatherv( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iallgatherv %p %d %D %p %p %p %D %C %p",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcounts,
            displs,
            recvtype,
            comm,
            request
            )
        );
    TraceError(MPI_Iallgatherv, mpi_errno);
    goto fn_exit;
}

