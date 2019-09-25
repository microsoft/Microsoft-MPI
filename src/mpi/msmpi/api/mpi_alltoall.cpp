// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@
MPI_Alltoall - Sends data from all to all processes

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements to send to each process (integer)
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements received from any process (integer)
. recvtype - data type of receive buffer elements (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)

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
MPI_Alltoall(
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
    TraceEnter_MPI_Alltoall(comm,sendbuf,recvbuf,sendtype,recvtype,sendcount,recvcount);

    TypeHandle hRecvType;
    TypeHandle hSendType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( recvbuf == MPI_IN_PLACE )
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

    if( sendbuf != MPI_IN_PLACE )
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

    MPID_Request* creq_ptr;

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        //
        // intracommunicator
        //
        mpi_errno = MPIR_Ialltoall_intra(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            comm_ptr,
            MPIR_ALLTOALL_TAG,
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
        //
        // intercommunicator
        //
        mpi_errno = MPIR_Ialltoall_inter(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            comm_ptr,
            MPIR_ALLTOALL_TAG,
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

    TraceLeave_MPI_Alltoall();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_alltoall %p %d %D %p %d %D %C",
            sendbuf,
            sendcount,
            sendtype,
            recvbuf,
            recvcount,
            recvtype,
            comm
            )
        );
    TraceError(MPI_Alltoall, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Ialltoall - Sends data from all to all processes in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcount - number of elements to send to each process (integer)
. sendtype - data type of send buffer elements (handle)
. recvcount - number of elements received from any process (integer)
. recvtype - data type of receive buffer elements (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)
. request - communication handle (handle)

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
MPI_Ialltoall(
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
    TraceEnter_MPI_Ialltoall(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        sendcount,
        recvcount
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

    if( recvbuf == MPI_IN_PLACE )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
        goto fn_fail;
    }

    TypeHandle hRecvType;
    TypeHandle hSendType;
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

    if( sendbuf != MPI_IN_PLACE )
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

    MPID_Request* pReq;

    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        //
        // intracommunicator
        //
        mpi_errno = MPIR_Ialltoall_intra(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            pComm,
            MPIR_GET_ASYNC_TAG(MPIR_ALLTOALL_TAG),
            &pReq
            );
    }
    else
    {
        //
        // intercommunicator
        //
        mpi_errno = MPIR_Ialltoall_inter(
            sendbuf,
            sendcount,
            hSendType,
            recvbuf,
            recvcount,
            hRecvType,
            pComm,
            MPIR_GET_ASYNC_TAG(MPIR_ALLTOALL_TAG),
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

    TraceLeave_MPI_Ialltoall( *request );

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ialltoall %p %d %D %p %d %D %C %p",
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
    TraceError(MPI_Ialltoall, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Alltoallv - Sends data from all to all processes; each process may
   send a different amount of data and provide displacements for the input
   and output data.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcounts - integer array equal to the group size
specifying the number of elements to send to each processor
. sdispls - integer array (of length group size). Entry
 'j'  specifies the displacement (relative to sendbuf  from
which to take the outgoing data destined for process  'j'
. sendtype - data type of send buffer elements (handle)
. recvcounts - integer array equal to the group size
specifying the maximum number of elements that can be received from
each processor
. rdispls - integer array (of length group size). Entry
 'i'  specifies the displacement (relative to recvbuf  at
which to place the incoming data from process  'i'
. recvtype - data type of receive buffer elements (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)

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
MPI_Alltoallv(
    _In_opt_ const void* sendbuf,
    _In_opt_ const int sendcounts[],
    _In_opt_ const int sdispls[],
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ const int rdispls[],
    _In_ MPI_Datatype recvtype,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Alltoallv(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        TraceArrayLength(1),
        sendcounts,
        TraceArrayLength(1),
        recvcounts,
        TraceArrayLength(1),
        sdispls,
        TraceArrayLength(1),
        rdispls
        );

    TypeHandle hRecvType;
    TypeHandle hSendType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( recvbuf == MPI_IN_PLACE )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
        goto fn_fail;
    }

    int sendcnt_sentinel = 0;
    int recvcnt_sentinel = 0;

    for( int i = 0; i < comm_ptr->remote_size; i++ )
    {
        if( sendbuf != MPI_IN_PLACE )
        {
            if( sendcounts[i] < 0 )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", sendcounts[i] );
                goto fn_fail;
            }
            sendcnt_sentinel |= sendcounts[i];
        }

        if( recvcounts[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
            goto fn_fail;
        }

        recvcnt_sentinel |= recvcounts[i];
    }

    mpi_errno = MpiaDatatypeValidate(
        recvbuf,
        recvcnt_sentinel,
        recvtype,
        "recvtype",
        &hRecvType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( sendbuf != MPI_IN_PLACE )
    {
        mpi_errno = MpiaDatatypeValidate(
            sendbuf,
            sendcnt_sentinel,
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

    MPID_Request* creq_ptr;
    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        //
        // intracommunicator
        //
        mpi_errno = MPIR_Ialltoallv_intra(
            sendbuf,
            sendcounts,
            sdispls,
            hSendType,
            recvbuf,
            recvcounts,
            rdispls,
            hRecvType,
            comm_ptr,
            MPIR_ALLTOALLV_TAG,
            &creq_ptr);
    }
    else
    {
        //
        // intercommunicator
        //
        mpi_errno = MPIR_Ialltoallv_inter(
            sendbuf,
            sendcounts,
            sdispls,
            hSendType,
            recvbuf,
            recvcounts,
            rdispls,
            hRecvType,
            comm_ptr,
            MPIR_ALLTOALLV_TAG,
            &creq_ptr);
    }
    if (mpi_errno == MPI_SUCCESS)
    {
        mpi_errno = MPIR_Wait(creq_ptr);
        creq_ptr->Release();
    }
    else
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Alltoallv();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_alltoallv %p %p %p %D %p %p %p %D %C",
            sendbuf,
            sendcounts,
            sdispls,
            sendtype,
            recvbuf,
            recvcounts,
            rdispls,
            recvtype,
            comm
            )
        );
    TraceError(MPI_Alltoallv, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Ialltoallv - Sends data from all to all processes in a nonblocking way.
   Each process may send a different amount of data and provide displacements
   for the input and output data.

   Input Parameters:
   + sendbuf - starting address of send buffer (choice)
   . sendcounts - integer array equal to the group size
   specifying the number of elements to send to each processor
   . sdispls - integer array (of length group size). Entry
   'j'  specifies the displacement (relative to sendbuf  from
   which to take the outgoing data destined for process  'j'
   . sendtype - data type of send buffer elements (handle)
   . recvcounts - integer array equal to the group size
   specifying the maximum number of elements that can be received from
   each processor
   . rdispls - integer array (of length group size). Entry
   'i'  specifies the displacement (relative to recvbuf  at
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
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Ialltoallv(
    _In_opt_ const void* sendbuf,
    _In_opt_ const int sendcounts[],
    _In_opt_ const int sdispls[],
    _In_ MPI_Datatype sendtype,
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ const int rdispls[],
    _In_ MPI_Datatype recvtype,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ialltoallv(
        comm,
        sendbuf,
        recvbuf,
        sendtype,
        recvtype,
        TraceArrayLength(1),
        sendcounts,
        TraceArrayLength(1),
        recvcounts,
        TraceArrayLength(1),
        sdispls,
        TraceArrayLength(1),
        rdispls
        );

    const MPID_Comm *pComm;
    int mpi_errno = MpiaCommValidateHandle(comm, const_cast<MPID_Comm**>(&pComm));
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    if (recvbuf == MPI_IN_PLACE)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**recvbuf_inplace");
        goto fn_fail;
    }

    int sendcnt_sentinel = 0;
    int recvcnt_sentinel = 0;
    for (int i = 0; i < pComm->remote_size; i++)
    {
        if (sendbuf != MPI_IN_PLACE)
        {
            if (sendcounts[i] < 0)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", sendcounts[i]);
                goto fn_fail;
            }
            sendcnt_sentinel |= sendcounts[i];
        }

        if (recvcounts[i] < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", recvcounts[i]);
            goto fn_fail;
        }

        recvcnt_sentinel |= recvcounts[i];
    }

    TypeHandle hRecvType;
    TypeHandle hSendType;
    mpi_errno = MpiaDatatypeValidate(
        recvbuf,
        recvcnt_sentinel,
        recvtype,
        "recvtype",
        &hRecvType
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (sendbuf != MPI_IN_PLACE)
    {
        mpi_errno = MpiaDatatypeValidate(
            sendbuf,
            sendcnt_sentinel,
            sendtype,
            "sendtype",
            &hSendType
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
    }
    else
    {
        hSendType = g_hBuiltinTypes.MPI_Datatype_null;
    }

    MPID_Request* pReq;
    unsigned int tag = MPIR_GET_ASYNC_TAG(MPIR_ALLTOALLV_TAG);

    if (pComm->comm_kind != MPID_INTERCOMM)
    {
        //
        // intracommunicator
        //
        mpi_errno = MPIR_Ialltoallv_intra(
            sendbuf,
            sendcounts,
            sdispls,
            hSendType,
            recvbuf,
            recvcounts,
            rdispls,
            hRecvType,
            pComm,
            tag,
            &pReq
            );
    }
    else
    {
        //
        // intercommunicator
        //
        mpi_errno = MPIR_Ialltoallv_inter(
            sendbuf,
            sendcounts,
            sdispls,
            hSendType,
            recvbuf,
            recvcounts,
            rdispls,
            hRecvType,
            pComm,
            tag,
            &pReq
            );
    }

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user
    //
    MPIU_Assert(pReq != nullptr);
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ialltoallv(*request);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(pComm),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ialltoallv %p %p %p %D %p %p %p %D %C %p",
            sendbuf,
            sendcounts,
            sdispls, 
            sendtype,
            recvbuf,
            recvcounts,
            rdispls,
            recvtype,
            comm,
            request
            )
        );
    TraceError(MPI_Ialltoallv, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Alltoallw - Generalized all-to-all communication allowing different
   datatypes, counts, and displacements for each partner

   Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcounts - integer array equal to the group size specifying the number of
  elements to send to each processor (integer)
. sdispls - integer array (of length group size). Entry j specifies the
  displacement in bytes (relative to sendbuf) from which to take the outgoing
  data destined for process j
. sendtypes - array of datatypes (of length group size). Entry j specifies the
  type of data to send to process j (handle)
. recvcounts - integer array equal to the group size specifying the number of
   elements that can be received from each processor (integer)
. rdispls - integer array (of length group size). Entry i specifies the
  displacement in bytes (relative to recvbuf) at which to place the incoming
  data from process i
. recvtypes - array of datatypes (of length group size). Entry i specifies
  the type of data received from process i (handle)
- comm - communicator (handle)

 Output Parameter:
. recvbuf - address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Alltoallw(
    _In_opt_ const void* sendbuf,
    _In_opt_ const int sendcounts[],
    _In_opt_ const int sdispls[],
    _In_opt_ const MPI_Datatype sendtypes[],
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ const int rdispls[],
    _In_ const MPI_Datatype recvtypes[],
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Alltoallw(
        comm,
        sendbuf,
        recvbuf,
        TraceArrayLength(1),
        (const unsigned int *)sendtypes,
        TraceArrayLength(1),
        (const unsigned int *)recvtypes,
        TraceArrayLength(1),
        sendcounts,
        TraceArrayLength(1),
        recvcounts,
        TraceArrayLength(1),
        sdispls,
        TraceArrayLength(1),
        rdispls
        );

    StackGuardArray<TypeHandle> typeHandleBuf;
    TypeHandle sendTypeHandleArray[64];
    TypeHandle recvTypeHandleArray[64];
    TypeHandle* pSendTypeHandles = sendTypeHandleArray;
    TypeHandle* pRecvTypeHandles = recvTypeHandleArray;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( recvbuf == MPI_IN_PLACE )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
        goto fn_fail;
    }

    //
    // Dynamicaly allocate an array to store the validated type handles
    // if the communicator exceeds the size of the stack arrays.
    //
    if( comm_ptr->remote_size > RTL_NUMBER_OF(sendTypeHandleArray) )
    {
        typeHandleBuf = new TypeHandle[comm_ptr->remote_size * 2];
        if( typeHandleBuf == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        pSendTypeHandles = typeHandleBuf;
        pRecvTypeHandles = pSendTypeHandles + comm_ptr->remote_size;
    }

    for( int i = 0; i < comm_ptr->remote_size; i++ )
    {
        mpi_errno = MpiaDatatypeValidate(
            recvbuf,
            recvcounts[i],
            recvtypes[i],
            "recvtypes",
            &pRecvTypeHandles[i]
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcounts[i],
                sendtypes[i],
                "sendtypes",
                &pSendTypeHandles[i]
                );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
    }

    MPID_Request* creq_ptr;
    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        //
        // intracommunicator
        //
        // WARNING!!! The datatype handle arrays might contain bogus
        // values if counts are zero, and lead to access violations
        // in the underlying layers.
        //
        mpi_errno = MPIR_Ialltoallw_intra(
            sendbuf,
            sendcounts,
            sdispls,
            pSendTypeHandles,
            recvbuf,
            recvcounts,
            rdispls,
            pRecvTypeHandles,
            comm_ptr,
            MPIR_ALLTOALLW_TAG,
            &creq_ptr
            );
    }
    else
    {
        //
        // intercommunicator
        //
        // WARNING!!! The datatype handle arrays might contain bogus
        // values if counts are zero, and lead to access violations
        // in the underlying layers.
        //

        //
        // MPI_IN_PLACE is only valid for intracomms.
        //
        if (sendbuf == MPI_IN_PLACE)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**sendbuf_inplace");
            goto fn_fail;
        }

        mpi_errno = MPIR_Ialltoallw_inter(
            sendbuf,
            sendcounts,
            sdispls,
            pSendTypeHandles,
            recvbuf,
            recvcounts,
            rdispls,
            pRecvTypeHandles,
            comm_ptr,
            MPIR_ALLTOALLW_TAG,
            &creq_ptr
            );
    }
    if (mpi_errno == MPI_SUCCESS)
    {
        mpi_errno = MPIR_Wait(creq_ptr);
        creq_ptr->Release();
    }
    else
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Alltoallw();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_alltoallw %p %p %p %p %p %p %p %p %C",
            sendbuf,
            sendcounts,
            sdispls,
            sendtypes,
            recvbuf,
            recvcounts,
            rdispls,
            recvtypes,
            comm
            )
        );
    TraceError(MPI_Alltoallw, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Ialltoallw - Generalized all-to-all communication allowing different 
datatypes, counts, and displacements for each partner in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. sendcounts - integer array equal to the group size specifying the number of
elements to send to each processor (integer)
. sdispls - integer array (of length group size). Entry j specifies the
displacement in bytes (relative to sendbuf) from which to take the outgoing
data destined for process j
. sendtypes - array of datatypes (of length group size). Entry j specifies the
type of data to send to process j (handle)
. recvcounts - integer array equal to the group size specifying the number of
elements that can be received from each processor (integer)
. rdispls - integer array (of length group size). Entry i specifies the
displacement in bytes (relative to recvbuf) at which to place the incoming
data from process i
. recvtypes - array of datatypes (of length group size). Entry i specifies
the type of data received from process i (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Ialltoallw(
    _In_opt_ const void* sendbuf,
    _In_opt_ const int sendcounts[],
    _In_opt_ const int sdispls[],
    _In_opt_ const MPI_Datatype sendtypes[],
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ const int rdispls[],
    _In_ const MPI_Datatype recvtypes[],
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ialltoallw(
        comm,
        sendbuf,
        recvbuf,
        TraceArrayLength(1),
        (const unsigned int *)sendtypes,
        TraceArrayLength(1),
        (const unsigned int *)recvtypes,
        TraceArrayLength(1),
        sendcounts,
        TraceArrayLength(1),
        recvcounts,
        TraceArrayLength(1),
        sdispls,
        TraceArrayLength(1),
        rdispls
        );

    StackGuardArray<TypeHandle> typeHandleBuf;
    TypeHandle sendTypeHandleArray[64];
    TypeHandle recvTypeHandleArray[64];
    TypeHandle* pSendTypeHandles = sendTypeHandleArray;
    TypeHandle* pRecvTypeHandles = recvTypeHandleArray;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle(comm, const_cast<MPID_Comm**>(&comm_ptr));
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    if (recvbuf == MPI_IN_PLACE)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**recvbuf_inplace");
        goto fn_fail;
    }

    //
    // Dynamicaly allocate an array to store the validated type handles
    // if the communicator exceeds the size of the stack arrays.
    //
    if (comm_ptr->remote_size > RTL_NUMBER_OF(sendTypeHandleArray))
    {
        typeHandleBuf = new TypeHandle[comm_ptr->remote_size * 2];
        if (typeHandleBuf == nullptr)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        pSendTypeHandles = typeHandleBuf;
        pRecvTypeHandles = pSendTypeHandles + comm_ptr->remote_size;
    }

    for (int i = 0; i < comm_ptr->remote_size; i++)
    {
        mpi_errno = MpiaDatatypeValidate(
            recvbuf,
            recvcounts[i],
            recvtypes[i],
            "recvtypes",
            &pRecvTypeHandles[i]
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }

        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MpiaDatatypeValidate(
                sendbuf,
                sendcounts[i],
                sendtypes[i],
                "sendtypes",
                &pSendTypeHandles[i]
                );
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }
        }
    }

    MPID_Request* pReq;
    unsigned int tag = MPIR_GET_ASYNC_TAG(MPIR_ALLTOALLW_TAG);

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        //
        // intracommunicator
        //
        // WARNING!!! The datatype handle arrays might contain bogus
        // values if counts are zero, and lead to access violations
        // in the underlying layers.
        //
        mpi_errno = MPIR_Ialltoallw_intra(
            sendbuf,
            sendcounts,
            sdispls,
            pSendTypeHandles,
            recvbuf,
            recvcounts,
            rdispls,
            pRecvTypeHandles,
            comm_ptr,
            tag,
            &pReq
            );
    }
    else
    {
        //
        // intercommunicator
        //
        // WARNING!!! The datatype handle arrays might contain bogus
        // values if counts are zero, and lead to access violations
        // in the underlying layers.
        //

        //
        // MPI_IN_PLACE is only valid for intracomms.
        //
        if (sendbuf == MPI_IN_PLACE)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**sendbuf_inplace");
            goto fn_fail;
        }

        mpi_errno = MPIR_Ialltoallw_inter(
            sendbuf,
            sendcounts,
            sdispls,
            pSendTypeHandles,
            recvbuf,
            recvcounts,
            rdispls,
            pRecvTypeHandles,
            comm_ptr,
            tag,
            &pReq
            );
    }

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    //
    // Return the handle of the request to the user
    //
    MPIU_Assert(pReq != nullptr);
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ialltoallw(*request);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ialltoallw %p %p %p %p %p %p %p %p %C %p",
            sendbuf,
            sendcounts,
            sdispls,
            sendtypes,
            recvbuf,
            recvcounts,
            rdispls,
            recvtypes,
            comm,
            request
            )
        );
    TraceError(MPI_Ialltoallw, mpi_errno);
    goto fn_exit;
}
