// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_endpoint.cpp - Network Direct MPI CH3 Channel endpoint object

--*/

#include "precomp.h"
#include "ch3u_nd1.h"


namespace CH3_ND
{
namespace v1
{

#define CREDIT_THRESHOLD (NDv1_RECVQ_SIZE / 2)


CEndpoint::CEndpoint() :
    m_pVc( NULL ),
    m_nRef( 1 ),
    m_hPrivateBufferMr( NULL ),
    m_iSend( 0 ),
    m_nSends( 0 ),
    m_iRead( 0 ),
    m_nReads( 0 ),
    m_ReadOffset( 0 ),
    m_nRdComplToGive( 0 ),
    m_iRecv( 0 ),
    m_nRecvs( 0 ),
    m_nSendCredits( 0 ),
    m_nCreditsToGive( 0 ),
    m_pRecvActive( NULL ),
    m_fDisconnect( false )
{
    for( SIZE_T i = 0; i < NDv1_SENDQ_SIZE; i++ )
    {
        m_SendResults[i].pfnSucceeded = SendSucceeded;
        m_SendResults[i].pfnFailed = SendFailed;
        m_SendResults[i].pEp = this;
    }

    for( SIZE_T i = 0; i < NDv1_RECVQ_SIZE; i++ )
    {
        m_RecvResults[i].pfnSucceeded = RecvSucceeded;
        m_RecvResults[i].pfnFailed = RecvFailed;
        m_RecvResults[i].pEp = this;
    }

    for( SIZE_T i = 0; i < NDv1_READ_LIMIT; i++ )
    {
        m_ReadResults[i].pfnSucceeded = ReadSucceeded;
        m_ReadResults[i].pfnFailed = ReadFailed;
        m_ReadResults[i].pEp = this;
    }
}


int CEndpoint::Init(
    __inout CAdapter* pAdapter,
    __in INDConnector* pIConnector,
    __in const struct sockaddr_in& DestAddr,
    __inout MPIDI_VC_t* pVc,
    __in SIZE_T Ird,
    __in SIZE_T Ord
    )
{
    pIConnector->AddRef();
    m_pIConnector.attach( pIConnector );

    m_DestAddr = DestAddr;

    //
    // Find a CQ that we can use.
    //
    int mpi_errno = pAdapter->GetAvailableCq( &m_pCq.ref() );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    m_pCq->AllocateEntries();
    HRESULT hr = m_pIConnector->CreateEndpoint(
        m_pCq->ICq(),
        m_pCq->ICq(),
        NDv1_PASSIVE_ENTRIES,
        NDv1_ACTIVE_ENTRIES,
        1,
        1,
        Ird,
        Ord,
        NULL,
        &m_pIEndpoint.ref()
        );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|create_ep %x", hr );

    //
    // Register the private buffer.
    //
    mpi_errno = pAdapter->RegisterMemory(
        (const char*)&m_Buffers,
        sizeof(m_Buffers),
        &m_hPrivateBufferMr
        );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    mpi_errno = PrepostReceives();
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    //
    // Associate the EP with the VC.
    //
    MPIU_Assert( pVc->ch.nd.pEpV1 == NULL );
    pVc->ch.nd.pEpV1 = this;
    m_pVc = pVc;
    AddRef();

    return MPI_SUCCESS;
}


int CEndpoint::Reinit()
{
    MPIDI_VC_t* pVc = m_pVc;
    StackGuardRef<CAdapter> pAdapter( m_pCq->Adapter() );
    pAdapter->AddRef();
    struct sockaddr_in DestAddr = m_DestAddr;

    Abandon( MPIDI_CH3I_VC_STATE_UNCONNECTED );

    StackGuardRef<INDConnector> pIConnector;
    HRESULT hr = pAdapter->IAdapter()->CreateConnector( &pIConnector.ref() );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|create_conn %x", hr );

    MPIU_Assert( pVc->ch.nd.pEpV1 == NULL );
    StackGuardRef<CEndpoint> pEp;
    int mpi_errno = CEndpoint::Create(
        pAdapter.get(),
        pIConnector.get(),
        DestAddr,
        pVc,
        pAdapter->IRL(),
        pAdapter->ORL(),
        &pEp.ref()
        );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    mpi_errno = pEp->Connect();
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    return MPI_SUCCESS;
}


CEndpoint::~CEndpoint()
{
    while( !m_MwList.empty() )
    {
        //
        // MW is not reusable since it is bound.  It will get released and destroyed.
        //
        StackGuardRef<CMw> pMw( &m_MwList.front() );
        m_MwList.pop_front();
        pMw->Unbind();
    }

    if( m_hPrivateBufferMr != NULL )
    {
        m_pCq->Adapter()->DeregisterMemory( m_hPrivateBufferMr );
    }

    m_pCq->FreeEntries();

    if( m_pVc )
    {
        MPIU_Assert( m_pVc->ch.nd.pEpV1 == NULL );
        m_pVc->ch.state = MPIDI_CH3I_VC_STATE_UNCONNECTED;
        MPIDI_CH3U_Handle_connection( m_pVc, MPIDI_VC_EVENT_TERMINATED );
    }
}


MPI_RESULT
CEndpoint::Create(
    _Inout_ CAdapter* pAdapter,
    _In_ INDConnector* pIConnector,
    _In_ const struct sockaddr_in& DestAddr,
    _Inout_ MPIDI_VC_t* pVc,
    _In_ SIZE_T Ird,
    _In_ SIZE_T Ord,
    _Outptr_ CEndpoint** ppEp
    )
{
    StackGuardRef<CEndpoint> pEp( new CEndpoint() );
    if( pEp.get() == NULL )
        return MPIU_ERR_NOMEM();

    int mpi_errno = pEp->Init( pAdapter, pIConnector, DestAddr, pVc, Ird, Ord );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    *ppEp = pEp.detach();
    return MPI_SUCCESS;
}


void CEndpoint::Release()
{
    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}


int CEndpoint::Connect()
{
    //
    // Connect
    //
    int mpi_errno = HandleTimeout();
    if( mpi_errno != MPI_SUCCESS )
    {
        //
        // We have requests preposted, so we need to flush them, etc.
        //
        Abandon( MPIDI_CH3I_VC_STATE_FAILED );
        return MPIU_ERR_FAIL( mpi_errno );
    }

    m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTING;
    return MPI_SUCCESS;
}


int
CEndpoint::CompleteConnect()
{
    SIZE_T len = sizeof(m_nSendCredits);
    //
    // We don't care about the Inbound or Outbound request limit here.
    //
    HRESULT hr = m_pIConnector->GetConnectionData( NULL, NULL, &m_nSendCredits, &len );
    if( (FAILED( hr ) && hr != ND_BUFFER_OVERFLOW) || len < sizeof(m_nSendCredits) )
    {
        if( hr == ND_CONNECTION_ABORTED || hr == ND_CONNECTION_INVALID )
            return Reinit();

        Abandon( MPIDI_CH3I_VC_STATE_FAILED );
        return MPIU_E_ERR(
            "**ch3|nd|conn_data %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            hr
            );
    }

//TODO: Wrap setting callbacks and calling NdCompleteConnect as inlines in a headder.
    ExInitOverlapped( &m_ConnectorOv, ConnCompleted, ConnFailed );

    hr = m_pIConnector->CompleteConnect( &m_ConnectorOv.ov );
    if( FAILED( hr ) )
    {
        return ConnFailed( hr );
    }

    MPIU_Assert( hr != ND_TIMEOUT );
    //
    // Add a reference for the overlapped.
    //
    AddRef();
    return MPI_SUCCESS;
}


int
CEndpoint::HandleTimeout()
{
    //
    // Note that we retry connections that time out indefinitely.
    // If the remote side exits for some reason, we will have a different error.
    //
    if( m_pVc == NULL )
    {
        //
        // The back-pointer has been cleared, which means we accepted the connection
        // using a different EP.  This EP is orphaned, and should go away.
        //
        return MPI_SUCCESS;
    }

    nd_caller_data_t pdata;
    pdata.Version = MSMPI_VER_EX;
    pdata.Credits = NDv1_RECVQ_SIZE;
    pdata.Rank = PMI_Get_rank();
    pdata.GroupId = MPIDI_Process.my_pg->id;

//TODO: Wrap setting callbacks and calling NdCompleteConnect as inlines in a headder.
    ExInitOverlapped( &m_ConnectorOv, ConnReqSucceeded, ConnReqFailed );

    HRESULT hr = m_pIConnector->Connect(
        m_pIEndpoint.get(),
        (const struct sockaddr*)&m_DestAddr,
        sizeof(m_DestAddr),
        123,
        0,
        &pdata,
        sizeof(pdata),
        &m_ConnectorOv.ov
        );
    if( FAILED( hr ) )
        return ConnReqFailed( hr );

    //
    // Add a reference for the overlapped.
    //
    AddRef();
    return MPI_SUCCESS;
}


int
WINAPI
CEndpoint::ConnReqSucceeded(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );

    return pEp->ConnReqSucceeded( pEp->GetOverlappedResult() );
}


int CEndpoint::ConnReqSucceeded( __in HRESULT hr )
{
    int mpi_errno;
    if( hr == ND_TIMEOUT )
    {
        mpi_errno = HandleTimeout();
    }
    else
    {
        mpi_errno = CompleteConnect();
    }

    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    return MPI_SUCCESS;
}


int
WINAPI
CEndpoint::ConnReqFailed(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );

    return pEp->ConnReqFailed( pEp->GetOverlappedResult() );
}


int CEndpoint::ConnReqFailed( __in HRESULT hr )
{
    MPIU_Assert( m_pIConnector.get() != NULL );

    MPIU_Assert( FAILED( hr ) );
    switch( hr )
    {
    case ND_CONNECTION_REFUSED:
        if( m_pVc != NULL )
        {
            //
            // The connection was rejected because we needed to take the passive
            // role.  We are processing the rejection before the peer's connection
            // request, and must roll back the VC state to UNCONNECTED so that the
            // accept logic works properly.
            //
            Abandon( MPIDI_CH3I_VC_STATE_UNCONNECTED );
            return MPI_SUCCESS;
        }

        __fallthrough;

    case ND_CANCELED:
        MPIU_Assert( m_pVc == NULL );
        return MPI_SUCCESS;

    default:
        Abandon( MPIDI_CH3I_VC_STATE_FAILED );
        return MPIU_E_ERR(
            "**ch3|nd|conn %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            hr
            );
    }
}


int
WINAPI
CEndpoint::ConnCompleted(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );

    return pEp->ConnCompleted();
}


int CEndpoint::ConnCompleted()
{
    SetState( MPIDI_CH3I_VC_STATE_CONNECTED );

    //
    // Woot, we're connected.  Start sending data.
    //
    m_pVc->eager_max_msg_sz = g_NdEnv.EagerLimit();
    MPIU_Assert( !MPIDI_CH3I_SendQ_empty(m_pVc) );
    bool fMpiRequestDone = false;
    return ProcessSends( &fMpiRequestDone );
}


int
WINAPI
CEndpoint::ConnFailed(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );

    return pEp->ConnFailed( pEp->GetOverlappedResult() );
}


int CEndpoint::ConnFailed( __in HRESULT hr )
{
    MPIU_Assert( FAILED( hr ) );

    if( hr == ND_CONNECTION_ABORTED || hr == ND_CONNECTION_INVALID )
        return Reinit();

    Abandon( MPIDI_CH3I_VC_STATE_FAILED );

    return MPIU_E_ERR(
        "**ch3|nd|comp_conn %s %d %x",
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port,
        hr
        );
}


int
CEndpoint::Accept(
    __in const UINT8 SendCredits
    )
{
    m_nSendCredits = SendCredits;
    UINT8 CreditsToGive = NDv1_RECVQ_SIZE;

    ExInitOverlapped( &m_ConnectorOv, AcceptSucceeded, AcceptFailed );
    HRESULT hr = m_pIConnector->Accept(
        m_pIEndpoint.get(),
        &CreditsToGive,
        sizeof(CreditsToGive),
        &m_ConnectorOv.ov
        );
    if( FAILED( hr ) )
        return AcceptFailed( hr );

    m_pVc->ch.channel = MPIDI_CH3I_CH_TYPE_NDv1;
    m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTING;

    //
    // Take a reference for the overlapped.
    //
    AddRef();
    return MPI_SUCCESS;
}

int
CEndpoint::PrepostReceives()
{
    //
    // Prepost receives.
    //
    ND_SGE Sge;
    Sge.Length = sizeof(nd_msg_t);
    Sge.hMr = m_hPrivateBufferMr;

    m_pIEndpoint->StartRequestBatch();

    for( int i = 0; i < NDv1_RECVQ_SIZE; i++ )
    {
        Sge.pAddr = &m_Buffers.Recv[i];
        HRESULT hr = m_pIEndpoint->Receive(
            &m_RecvResults[i],
            &Sge,
            1
            );
        if( FAILED(hr) )
        {
            //
            // Flushing assumes that the endpoint will complete the posted receives
            // with a ND_CANCELED status.  What do we do if this fails?
            //
            // Temporarily suppress the OACR warning here until we figure out what to
            // do. Actually, we might be deprecating NDv1 before we get to do this
            //
            OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "No good recovery strategy");
            m_pIEndpoint->Flush();
            return MPIU_E_ERR( "**ch3|nd|recv %x", hr );
        }
        //
        // The nd_result_t points back to the EP - take a reference until it completes.
        //
        AddRef();
    }

    m_pIEndpoint->SubmitRequestBatch();
    return MPI_SUCCESS;
}


void
CEndpoint::Abandon(
    __in MPIDI_CH3I_VC_state_t State
    )
{
    MPIU_Assert( m_pIEndpoint.get() != NULL );

    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
    m_pIEndpoint->Flush();

    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
    m_pIConnector->CancelOverlappedRequests();

    //
    // We must clear the back pointer so that a subsequent release doesn't
    // close the VC - we're just orphaning the endpoint.
    //
    if( m_pVc != NULL )
    {
        SetState( State );

        //
        // Disassociate the endpoint from the VC.
        //
        m_pVc->ch.nd.pEpV1 = NULL;
        Release();
        m_pVc = NULL;
    }
}


int
WINAPI
CEndpoint::AcceptSucceeded(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );

    if( pEp->GetOverlappedResult() == ND_TIMEOUT )
        return pEp->AcceptFailed( ND_TIMEOUT );

    pEp->m_pVc->eager_max_msg_sz = g_NdEnv.EagerLimit();

    //
    // We don't transition the VC to the connected state until we've received
    // our first packet.  This is to honor the iWARP requirement that the
    // active side send first.  However, the first packet can race the accept
    // completion, so we may need to make progress on queued receives.
    //
    switch( pEp->m_pVc->ch.state )
    {
    case MPIDI_CH3I_VC_STATE_CONNECTING:
        pEp->m_pVc->ch.state = MPIDI_CH3I_VC_STATE_ACCEPTED;
        return MPI_SUCCESS;

    case MPIDI_CH3I_VC_STATE_PENDING_ACCEPT:
        pEp->m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTED;
        break;

    default:
        MPIU_Assert(
            pEp->m_pVc->ch.state == MPIDI_CH3I_VC_STATE_CONNECTING ||
            pEp->m_pVc->ch.state == MPIDI_CH3I_VC_STATE_PENDING_ACCEPT
            );
        break;
    }

    bool requestDone = false;
    return pEp->Progress( &requestDone );
}


int
WINAPI
CEndpoint::AcceptFailed(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );

    return pEp->AcceptFailed( pEp->GetOverlappedResult() );
}


int CEndpoint::AcceptFailed( __in HRESULT hr )
{
    MPIU_Assert( FAILED( hr ) || hr == ND_TIMEOUT );

    if( hr == ND_CONNECTION_ABORTED || hr == ND_TIMEOUT )
    {
        //
        // Note that there might be send requests in the send queue.  If the connection
        // timed out the remote side will retry.  Once the connection gets established
        // the queued sends will get processed - no need to do anything with them here.
        //
        // We must move the state back to UNCONNECTED here, even though we may have a
        // send in the queue.  Leaving the state as CONNECTING could incorrectly lead
        // us to think we should take the active role in connection establishment when
        // we don't have a connection request outstanding.
        //

        Abandon( MPIDI_CH3I_VC_STATE_UNCONNECTED );
        return MPI_SUCCESS;
    }

    Abandon( MPIDI_CH3I_VC_STATE_FAILED );
    return MPIU_E_ERR( "**ch3|nd|accept %x", hr );
}


void CEndpoint::Disconnect()
{
    m_fDisconnect = true;

    //
    // We need to make sure that any outbound requests (send, bind/unbind, and RDMA) complete
    // before we tear the connection down.
    //
    if( m_nSends != 0 )
        return;

    //
    // We need to make sure that any queued requests get sent before connection tear down.
    //
    if( !MPIDI_CH3I_SendQ_empty(m_pVc))
        return;

    DoDisconnect();
}


void CEndpoint::DoDisconnect()
{
    //
    // Disassociate the endpoint from the VC.  Note that we leave the back pointer
    // so that we will call MPIDI_CH3U_Handle_connection when the reference count
    // reaches zero.
    //
    m_pVc->ch.nd.pEpV1 = NULL;

    //
    // Release the VC ownership over this EP and
    // Take a reference for the duration of the overlapped request
    //
    // AddRef();
    // Release();
    ExInitOverlapped( &m_ConnectorOv, DisconnectSucceeded, DisconnectFailed );
    //
    // We disconnect asynchronously to allow things to overlap
    // as much as possible (helps jobs terminate quickly).
    //
    HRESULT hr;
    hr = m_pIConnector->Disconnect( &m_ConnectorOv.ov );
    if( FAILED( hr ) )
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
        m_pIEndpoint->Flush();
        Release();
    }
}


int
WINAPI
CEndpoint::DisconnectSucceeded(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );
    return MPI_SUCCESS;
}


int
WINAPI
CEndpoint::DisconnectFailed(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CEndpoint> pEp(
        CONTAINING_RECORD( pOverlapped, CEndpoint, m_ConnectorOv.ov )
        );
    return MPIU_E_ERR( "**ch3|nd|dconn %x", pEp->GetOverlappedResult() );
}


int
CEndpoint::Send(
    __inout const MPID_IOV* pIov,
    __in int nIov,
    __out MPIU_Bsize_t* pnBytes
    )
{
    MPIU_Assert( MPIDI_CH3I_SendQ_empty(m_pVc) );

    *pnBytes = 0;

    int iIov = 0;
    const char* pSrc = pIov[iIov].buf;
    MPIU_Bsize_t nSrc = pIov[iIov].len;

    do
    {
        if( !OkToSend() )
            return MPI_SUCCESS;

        nd_msg_t* pMsg = &m_Buffers.Send[m_iSend];

        UINT8* pDst = pMsg->Data;
        MPIU_Bsize_t nDst = sizeof(pMsg->Data);

        //
        // Copy data to the private buffer.
        //
        while( nDst && nSrc )
        {
            if( m_pCq->Adapter()->UseRma( nSrc )  )
            {
                //
                // This function cannot send via zCopy since there is no associated
                // MPI request.
                //
                // If we haven't copied anything into our buffer, we return.  Otherwise
                // we send what we have copied so far and then return.
                //
                if( nDst == sizeof(pMsg->Data) )
                    return MPI_SUCCESS;

                break;
            }

            if( nSrc > nDst )
            {
                CopyMemory( pDst, pSrc, nDst );
                pSrc += nDst;
                nSrc -= nDst;
                nDst = 0;
                break;
            }

            CopyMemory( pDst, pSrc, nSrc );
            pDst += nSrc;
            nDst -= nSrc;
            iIov++;
            if( iIov == nIov )
                break;

            pSrc = pIov[iIov].buf;
            nSrc = pIov[iIov].len;
        }

        *pnBytes += (MPIU_Bsize_t)sizeof(pMsg->Data) - nDst;

        int mpi_errno = SendMsg( NdMsgTypeData, sizeof(*pMsg) - nDst );
        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );

    } while( iIov < nIov );

    return MPI_SUCCESS;
}


//
// CEndpoint::ProcessSendsUnsafe
//
// Description:
//  This routine tries to make forward progress sending queued requests.
//
// Arguments:
//  pfMpiRequestDone - Indicates whether an MPI request was completed.
//
// Return Value:
//  MPI_SUCCESS if all possible progress was made.
//  MPI error values if a fatal error is encountered.
//
// Notes:
//  This function will send small buffers by copying them into a
//  pre-registered buffer.  Large buffers are registered with the
//  appropriate ND adapter, a memory window is created and bound to
//  the registered buffer, and the memory window descriptor is sent
//  to the connected peer.
//
//  When processing large buffers that are being sent via the zCopy
//  mechanism, the advertised memory windows are tracked in a per-endpoint
//  list.  More than a single MW can be advertised for a single request at
//  the same time.
//
//  The code loops through the IOVs in order, looking for data to send.
//  Messages always start off as NdMsgTypeData, and are switched to
//  NdMsgTypeSrcAvail as needed/appropriate.
//
int
CEndpoint::ProcessSendsUnsafe(
    _Inout_ bool* pfMpiRequestDone
    )
{
    int mpi_errno;

    MPID_Request* pReq = MPIDI_CH3I_SendQ_head_unsafe(m_pVc);
    if( pReq == NULL )
    {
        return SendFlowControlMsg();
    }

    //
    // Check to see if all the data for this request was sent.  Requests
    // being transfered via zCopy will stall the send queue until the
    // remote peer has read the data, and sent the RdCompl notification.
    //
    // Note that if we do have a request stalled at the head of the request
    // queue we need to send any RdCompl notifications to prevent deadlock.
    //
    if( pReq->dev.iov_count == pReq->dev.iov_offset )
    {
        return SendFlowControlMsg();
    }

    for( ;; )
    {
        if( !OkToSend() )
        {
            return MPI_SUCCESS;
        }


        nd_msg_t* pMsg = &m_Buffers.Send[m_iSend];

        //
        // Messages always start off as NdMsgTypeData
        //
        nd_msg_type_t MsgType = NdMsgTypeData;

        int iIov = pReq->dev.iov_offset;
        const char* pSrc = pReq->dev.iov[iIov].buf;
        MPIU_Bsize_t nSrc = pReq->dev.iov[iIov].len;

        UINT8* pDst = pMsg->Data;
        MPIU_Bsize_t nDst = sizeof(pMsg->Data);

        //
        // Format the private buffer.
        //
        while( nDst && nSrc )
        {
            if( m_pCq->Adapter()->UseRma( nSrc ) )
            {
                //
                // If we don't have room for the MW descriptor, break and
                // send this message.  We'll send the SrcAvail in the next
                // message.  Note that we don't copy any data from this IOV
                // for fear of bringing it just bellow the zCopy threshold
                // and sticking with bCopy.
                //
                if( nDst < sizeof(ND_MW_DESCRIPTOR) )
                    break;

                //
                // We must cap the size of the memory registration to what the adapter can support.
                //
                MPIU_Bsize_t rmaSize = (MPIU_Bsize_t)min( nSrc, m_pCq->Adapter()->MaxWindowSize() );

                //
                // TODO: Make CreateMr asynchronous.
                //
                StackGuardRef<CMr> pMr;
                mpi_errno = m_pCq->Adapter()->CreateMrVadAware(
                    pSrc,
                    &rmaSize,
                    sizeof(pMsg->Data),
                    &pMr.ref()
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return MPIU_ERR_FAIL( mpi_errno );
                }

                if( rmaSize < sizeof(pMsg->Data) )
                {
                    // Not worth doing zcopy for.
                    goto bcopy;
                }

                //
                // We're going to be placing the MW descriptor at the end of the
                // message.
                //
                nDst -= sizeof(ND_MW_DESCRIPTOR);

                //
                // We piggy-back some of the data in the SrcAvail message
                // so we only need to bind the remainder of the buffer.
                //
                MPIU_Assert( rmaSize - nDst > 0 );
                mpi_errno = BindMw(
                    pMr.get(),
                    pSrc + nDst,
                    rmaSize - nDst,
                    &pMsg->SrcAvail.MwDescriptor
                    );
                if( mpi_errno != MPI_SUCCESS )
                {
                    return MPIU_ERR_FAIL( mpi_errno );
                }

                //
                // Fill the rest of the buffer and adjust the size of the descriptor
                //
                CopyMemory( pDst, pSrc, nDst );
                nDst = 0;

                MsgType = NdMsgTypeSrcAvail;

                if( rmaSize < nSrc )
                {
                    pReq->dev.iov[iIov].len -= rmaSize;
                    pReq->dev.iov[iIov].buf += rmaSize;
                }
                else
                {
                    iIov++;
                }
                break;
            }

bcopy:
            if( nSrc > nDst )
            {
                CopyMemory( pDst, pSrc, nDst );
                pSrc += nDst;
                nSrc -= nDst;
                nDst = 0;
                pReq->dev.iov[iIov].len = nSrc;
                pReq->dev.iov[iIov].buf = (iovsendbuf_t*)pSrc;
                break;
            }

            CopyMemory( pDst, pSrc, nSrc );
            pDst += nSrc;
            nDst -= nSrc;
            iIov++;
            if( iIov == pReq->dev.iov_count )
                break;

            pSrc = pReq->dev.iov[iIov].buf;
            nSrc = pReq->dev.iov[iIov].len;
        }
        pReq->dev.iov_offset = iIov;

        mpi_errno = SendMsg( MsgType, sizeof(*pMsg) - nDst );
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        //
        // More of the iov to send; go send the next portion.
        //
        if( pReq->dev.iov_offset != pReq->dev.iov_count )
            continue;

        //
        // zCopy operations outstanding - wait until complete before moving to next request.
        //
        if( !m_MwList.empty() )
        {
            return MPI_SUCCESS;
        }

        if( !m_MwInvalidatingList.empty() )
        {
            return MPI_SUCCESS;
        }

        //
        // Done sending this iov, indicate this.
        //
        int fComplete;
        mpi_errno = MPIDI_CH3U_Handle_send_req( m_pVc, pReq, &fComplete );
        if (mpi_errno != MPI_SUCCESS)
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        if( !fComplete )
        {
            //
            // The iov got reloaded as the request is not done yet; reset the iov_offset
            // and go send the next portion.
            //
            TraceSendNd_Continue(MPIDI_Process.my_pg_rank, m_pVc->pg_rank, pReq->ch.msg_id, pReq->dev.iov_count, pReq->iov_size());
            pReq->dev.iov_offset = 0;
            continue;
        }

        //
        // This request is done, remove it from the queue and go send the next request.
        //
        TraceSendNd_Done(MPIDI_Process.my_pg_rank, m_pVc->pg_rank, pReq->ch.msg_id);
        MPIDI_CH3I_SendQ_dequeue_unsafe( m_pVc );
        *pfMpiRequestDone = true;

        pReq = MPIDI_CH3I_SendQ_head_unsafe(m_pVc);
        if( pReq == NULL )
        {
            return MPI_SUCCESS;
        }

        TraceSendNd_Head(MPIDI_Process.my_pg_rank, m_pVc->pg_rank, pReq->ch.msg_id, pReq->dev.iov_count, pReq->iov_size(), pReq->dev.pkt.type);
    }
}


int
CEndpoint::Progress( __inout bool *pfMpiRequestDone )
{
    int mpi_errno = ProcessReceives( pfMpiRequestDone );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    //
    // Note that the EP could have been disconnected during the receive processing,
    // so we must check that it hasn't before calling ProcessSends.
    //
    if( m_pVc->ch.nd.pEp == NULL )
    {
        return MPI_SUCCESS;
    }

    //
    // We always try to make forward progress on the send side.  Any
    // receive could have given us more credits, and if we don't try now
    // we could hang:
    //  - The CQ is empty the next time we poll, so we unwind
    //  - We go to the Ex engine to wait for a CQ notification
    //  - If the other side is waiting for data that we have pending,
    //  waiting for credits, we're hung.
    //
    mpi_errno = ProcessSends( pfMpiRequestDone );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    return MPI_SUCCESS;
}


int
CEndpoint::SendFlowControlMsg(
    )
{
    if( !OkToSend() )
        return MPI_SUCCESS;

    //
    // We send an update below our credit threshold if there are any
    // read complete notifications to send.
    //
    if( m_nCreditsToGive < CREDIT_THRESHOLD && m_nRdComplToGive == 0 )
        return MPI_SUCCESS;

    int mpi_errno = SendMsg( NdMsgTypeFlowControl, sizeof(nd_hdr_t) );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    return MPI_SUCCESS;
}


int
CEndpoint::SendMsg(
    __in nd_msg_type_t Type,
    __in SIZE_T Length
    )
{
    nd_msg_t* pMsg = &m_Buffers.Send[m_iSend];

    MPIU_Assert( Type == NdMsgTypeFlowControl || Length > sizeof(nd_hdr_t) );
    pMsg->Type = Type;
    pMsg->Credits = m_nCreditsToGive;
    pMsg->nRdCompl = m_nRdComplToGive;

    ND_SGE Sge;
    Sge.pAddr = pMsg;
    Sge.hMr = m_hPrivateBufferMr;
    Sge.Length = Length;

    //
    // If we have Read operations outstanding our send could indicate a Read
    // complete (nd_msg_t::RdCompl > 0).  Tell the HW to delay the send until
    // all outstanding Reads are complete.  If there are no reads outstanding,
    // the ND_OP_FLAG_READ_FENCE has no effect.
    //
    HRESULT hr = m_pIEndpoint->Send(
        &m_SendResults[m_iSend],
        &Sge,
        1,
        ND_OP_FLAG_READ_FENCE
        );
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR(
            "**ch3|nd|send %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            hr
            );
    }
    m_iSend = ++m_iSend % NDv1_SENDQ_SIZE;
    m_nSends++;
    m_nSendCredits--;
    m_nCreditsToGive = 0;
    m_nRdComplToGive = 0;
    AddRef();

    return MPI_SUCCESS;
}


int
CEndpoint::SendSucceeded(
    __in nd_result_t* pResult,
    __out bool* pfMpiRequestDone
    )
{
    nd_send_result_t *pSendResult = static_cast<nd_send_result_t*>(pResult);
    StackGuardRef<CEndpoint> pEp( pSendResult->pEp );

    pEp->m_nSends--;

    MPIU_Assert( pEp->m_pVc->ch.nd.pEpV1 != NULL );

    int mpi_errno = pEp->ProcessSends( pfMpiRequestDone );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    if( pEp->m_fDisconnect && pEp->m_nSends == 0 && MPIDI_CH3I_SendQ_empty(pEp->m_pVc) )
    {
        pEp->DoDisconnect();
    }

    return MPI_SUCCESS;
}


MPI_RESULT
CEndpoint::SendFailed(
    _In_ nd_result_t* pResult,
    _Out_ bool* pfMpiRequestDone
    )
{
    nd_send_result_t *pSendResult = static_cast<nd_send_result_t*>(pResult);
    StackGuardRef<CEndpoint> pEp( pSendResult->pEp );

    if( pResult->Status == ND_CANCELED && pEp->m_fDisconnect == true )
    {
        //
        // If we are disconnecting, and the last send gets flushed, pretend it
        // succeeded as we are now disconnected.
        //
        pEp.detach();
        return SendSucceeded( pResult, pfMpiRequestDone );
    }

    pEp->m_nSends--;

    return MPIU_E_ERR(
        "**ch3|nd|send_err %s %d %x",
        inet_ntoa( pEp->m_DestAddr.sin_addr ),
        pEp->m_DestAddr.sin_port,
        pSendResult->Status
        );
}


int
CEndpoint::ProcessReceives(
    __out bool* pfMpiRequestDone
    )
{
    int mpi_errno;
    *pfMpiRequestDone = false;

    while( m_nRecvs > 0 )
    {
        switch( m_Buffers.Recv[m_iRecv].Type )
        {
        case NdMsgTypeFlowControl:
            //
            // Already handled the Credits/RdCompl when the message was first received.
            //
            break;

        case NdMsgTypeData:
            //
            // We must delay processing receives until all in-flight RDMA read
            // operations complete.
            //
            if( m_nReads > 0 )
                return MPI_SUCCESS;

            mpi_errno = ProcessDataMsg( m_iRecv, pfMpiRequestDone );
            if( mpi_errno != MPI_SUCCESS )
                return MPIU_ERR_FAIL( mpi_errno );

            break;

        case NdMsgTypeSrcAvail:
            if( m_nReads == NDv1_READ_LIMIT )
                return MPI_SUCCESS;

            bool fRepost = false;
            if( m_ReadOffset != 0 )
            {
                //
                // Continue processing an existing SrcAvail.
                //
                mpi_errno = ReadToIov(
                    m_Buffers.Recv[m_iRecv].SrcAvail.MwDescriptor,
                    &fRepost );
            }
            else
            {
                MPIU_Assert( m_pRecvActive == NULL ||
                    m_pRecvActive->dev.iov_count != m_pRecvActive->dev.iov_offset );

                //
                // Process a new SrcAvail (still has bcopy payload).
                //
                mpi_errno = ProcessSrcAvailMsg( m_iRecv, &fRepost );
            }

            if( mpi_errno != MPI_SUCCESS )
                return MPIU_ERR_FAIL( mpi_errno );

            if( !fRepost )
                return MPI_SUCCESS;
            //
            // We fully consumed this SrcAvail (RDMA reads are issued, but not yet
            // complete.)  We want the next send to convey the read complete, and
            // we can set this now because sends are fenced by RDMA reads (they
            // won't get processed until all previous Read requests complete
            // processing by the HW.)
            //
            m_nRdComplToGive++;
            m_ReadOffset = 0;
            break;
        }

        //
        // Repost the receive buffer.
        //
        if( !m_fDisconnect )
        {
            ND_SGE Sge;
            Sge.pAddr = &m_Buffers.Recv[m_iRecv];
            Sge.Length = sizeof(m_Buffers.Recv[m_iRecv]);
            Sge.hMr = m_hPrivateBufferMr;
            HRESULT hr = m_pIEndpoint->Receive( &m_RecvResults[m_iRecv], &Sge, 1 );
            if( FAILED( hr ) )
                return MPIU_E_ERR( "**ch3|nd|recv %x", hr );

            //
            // The nd_result_t points back to the EP - take a reference until it completes.
            //
            AddRef();

            m_nCreditsToGive++;
        }

        //
        // Remove the request from the pending queue since it is complete.
        //
        m_nRecvs--;
        m_iRecv = ++m_iRecv % NDv1_RECVQ_SIZE;
    }
    return MPI_SUCCESS;
}


static
MPIU_Bsize_t
__CopyToIov(
    const UINT8* buffer,
    MPIU_Bsize_t size,
    MPID_IOV* iov,
    int iov_count,
    int* piov_offset
    )
{
    MPIU_Bsize_t nb = size;
    MPID_IOV* p = &iov[*piov_offset];
    const MPID_IOV* end = &iov[iov_count];

    while( p < end && nb > 0 )
    {
        MPIU_Bsize_t iov_len = p->len;

        if( iov_len <= nb )
        {
            memcpy( p->buf, buffer, iov_len );
            nb -= iov_len;
            buffer += iov_len;
            p++;
        }
        else
        {
            memcpy( p->buf, buffer, nb );
            p->buf = (iovrecvbuf_t*)p->buf + nb;
            p->len -= nb;
            nb = 0;
        }
    }

    //
    // It's safe to cast to int here because p is bound by iov + iov_count
    // and iov_count is an int
    //
    *piov_offset = static_cast<int>(p - iov); // next offset to process
    return (size - nb); // number of bytes copied
}


int CEndpoint::ProcessFlowControlData( __in UINT8 iRecv )
{
    m_nSendCredits = m_nSendCredits + m_Buffers.Recv[iRecv].Credits;

    if( m_Buffers.Recv[iRecv].nRdCompl == 0 )
        return MPI_SUCCESS;

    UINT8 nRdCompl = m_Buffers.Recv[iRecv].nRdCompl;
    while( nRdCompl-- )
    {
        int mpi_errno = InvalidateMw();
        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );
    }
    return MPI_SUCCESS;
}


MPI_RESULT
CEndpoint::ProcessDataMsg(
    _In_ UINT8 iRecv,
    _Out_ bool* pfMpiRequestDone
    )
{
    int mpi_errno;
    const UINT8* pSrc = m_Buffers.Recv[iRecv].Data;

    if( m_pRecvActive == NULL )
    {
        MPIU_Assert( m_RecvResults[iRecv].BytesTransferred >= sizeof(MPIDI_CH3_Pkt_t) );

        //
        // TODO: Change this so it can queue up received buffers to avoid
        // a copy (only copy once to the app's buffer).
        //

        m_pVc->ch.n_recv++;
        TraceRecvNd_Packet(m_pVc->pg_rank, MPIDI_Process.my_pg_rank, m_pVc->ch.n_recv, ((MPIDI_CH3_Pkt_t*)pSrc)->type);
        mpi_errno = MPIDI_CH3U_Handle_recv_pkt(
            m_pVc,
            reinterpret_cast<const MPIDI_CH3_Pkt_t*>(pSrc),
            &m_pRecvActive
            );

        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );

        if( m_pRecvActive == NULL )
        {
            //
            // Zero-byte MPI message.  Return to repost the receive.
            //
            MPIU_Assert( m_RecvResults[iRecv].BytesTransferred == sizeof(MPIDI_CH3_Pkt_t) );
            TraceRecvNd_Done(m_pVc->pg_rank, MPIDI_Process.my_pg_rank, m_pVc->ch.n_recv);
            *pfMpiRequestDone = true;
            return MPI_SUCCESS;
        }

        m_pRecvActive->dev.iov_offset = 0;
        m_RecvResults[iRecv].BytesTransferred -= sizeof(MPIDI_CH3_Pkt_t);
        pSrc += sizeof(MPIDI_CH3_Pkt_t);
    }

    MPIU_Assert( m_pRecvActive != NULL );

    while( m_RecvResults[iRecv].BytesTransferred )
    {
        MPIU_Bsize_t nCopy;
        nCopy = __CopyToIov(
            pSrc,
            (MPIU_Bsize_t)m_RecvResults[iRecv].BytesTransferred,
            m_pRecvActive->dev.iov,
            m_pRecvActive->dev.iov_count,
            &m_pRecvActive->dev.iov_offset
            );

        pSrc += nCopy;
        m_RecvResults[iRecv].BytesTransferred -= nCopy;

        if( m_pRecvActive->dev.iov_count != m_pRecvActive->dev.iov_offset )
            continue;

        TraceRecvNd_Data(m_pVc->pg_rank, MPIDI_Process.my_pg_rank, m_pVc->ch.n_recv);
        int fComplete;
        mpi_errno = MPIDI_CH3U_Handle_recv_req(
            m_pVc,
            m_pRecvActive,
            &fComplete
            );

        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );

        if( fComplete )
        {
            MPIU_Assert( m_RecvResults[iRecv].BytesTransferred == 0 );
            m_pRecvActive = NULL;
            TraceRecvNd_Done(m_pVc->pg_rank, MPIDI_Process.my_pg_rank, m_pVc->ch.n_recv);
            *pfMpiRequestDone = true;
            return MPI_SUCCESS;
        }

        m_pRecvActive->dev.iov_offset = 0;
    }

    *pfMpiRequestDone = false;
    return MPI_SUCCESS;
}


//
// ProcessSrcAvailMsg
//
// Description:
//  This routine handles a received SrcAvail message.
//
// Notes:
//  If we get here, we have at least a single RDMA read we can issue.
//  We copy the first bit of the data, and then Read the next chunks.
//
//  We stop issuing RDMA reads if we:
//      1. Consume all IOVs
//      2. Reach our limit of outstanding RDMA reads
//
MPI_RESULT
CEndpoint::ProcessSrcAvailMsg(
    _In_ UINT8 iRecv,
    _Out_ bool* pfRepost
    )
{
    int mpi_errno;
    const UINT8* pSrc = m_Buffers.Recv[iRecv].Data;

    MPIU_Assert( m_RecvResults[iRecv].BytesTransferred == sizeof(nd_src_avail_t) );
    MPIU_Assert( m_nReads < NDv1_READ_LIMIT );
    MPIU_Assert( m_ReadOffset == 0 );

    m_RecvResults[iRecv].BytesTransferred -= sizeof(ND_MW_DESCRIPTOR);

    if( m_pRecvActive == NULL )
    {
        MPIU_Assert( m_RecvResults[iRecv].BytesTransferred >= sizeof(MPIDI_CH3_Pkt_t) );

        m_pVc->ch.n_recv++;
        TraceRecvNd_Packet(m_pVc->pg_rank, MPIDI_Process.my_pg_rank, m_pVc->ch.n_recv, ((MPIDI_CH3_Pkt_t*)pSrc)->type);
        mpi_errno = MPIDI_CH3U_Handle_recv_pkt(
            m_pVc,
            reinterpret_cast<const MPIDI_CH3_Pkt_t*>(pSrc),
            &m_pRecvActive
            );

        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );

        MPIU_Assert( m_pRecvActive != NULL );

        m_pRecvActive->dev.iov_offset = 0;
        m_RecvResults[iRecv].BytesTransferred -= sizeof(MPIDI_CH3_Pkt_t);
        pSrc += sizeof(MPIDI_CH3_Pkt_t);
    }

    MPIU_Assert( m_pRecvActive != NULL );

    //
    // Copy piggy-back data.
    //
    while( m_RecvResults[iRecv].BytesTransferred )
    {
        MPIU_Bsize_t nCopy;
        nCopy = __CopyToIov(
            pSrc,
            (MPIU_Bsize_t)m_RecvResults[iRecv].BytesTransferred,
            m_pRecvActive->dev.iov,
            m_pRecvActive->dev.iov_count,
            &m_pRecvActive->dev.iov_offset
            );

        pSrc += nCopy;
        m_RecvResults[iRecv].BytesTransferred -= nCopy;

        MPIU_Assert( m_pRecvActive->dev.iov_count != m_pRecvActive->dev.iov_offset );
    }

    return ReadToIov( m_Buffers.Recv[iRecv].SrcAvail.MwDescriptor, pfRepost );
}


int
CEndpoint::RecvSucceeded(
    __in nd_result_t* pResult,
    __out bool* pfMpiRequestDone
    )
{
    nd_recv_result_t *pRecvResult = static_cast<nd_recv_result_t*>(pResult);
    StackGuardRef<CEndpoint> pEp( pRecvResult->pEp );
    int mpi_errno;
    *pfMpiRequestDone = false;

    //
    // We don't perform runtime checks here because we own and control the
    // receive buffer.  If we don't receive a header, we will end up processing
    // garbage data, which we could have received anyway.
    //
    // There is no buffer overrun risk as the hardware prevents buffer overruns.
    //
    MPIU_Assert( pRecvResult->BytesTransferred >= sizeof(nd_hdr_t) );
    pRecvResult->BytesTransferred -= sizeof(nd_hdr_t);

    UINT8 iRecv = (pEp->m_iRecv + pEp->m_nRecvs) % NDv1_RECVQ_SIZE;
    MPIU_Assert( pRecvResult == &pEp->m_RecvResults[iRecv] );
    //
    // Credits and RdCompl fields affect the send queue, so we process
    // them OOB from the payload.
    //
    mpi_errno = pEp->ProcessFlowControlData( iRecv );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    //
    // We must maintain ordering of receives for much of the logic of the endpoint
    // to work properly, as it depends on a circular buffer of receives that are
    // used in order.  For simplicity, we queue everything and then process the queue.
    //
    pEp->m_nRecvs++;

    //
    // Because iWARP requires that the active side of a connection be the first
    // to send any data in RDMA mode, we can't move the passive side to the
    // connected state until it has received its first message from the active side.
    // However the Accept overlapped request can race the receive completion.
    // To prevent trying to disconnect a short-lived connection before it has
    // been fully accepted, we delay the state transition until both the Accept
    // has completed AND we have received the first message.
    //
    switch( pEp->m_pVc->ch.state )
    {
    case MPIDI_CH3I_VC_STATE_UNCONNECTED:
        MPIU_Assert( pEp->m_pVc->ch.state != MPIDI_CH3I_VC_STATE_UNCONNECTED );
        return MPIU_ERR_FAIL( MPI_ERR_INTERN );

    case MPIDI_CH3I_VC_STATE_CONNECTING:
        pEp->m_pVc->ch.state = MPIDI_CH3I_VC_STATE_PENDING_ACCEPT;
        return MPI_SUCCESS;

    case MPIDI_CH3I_VC_STATE_PENDING_ACCEPT:
        return MPI_SUCCESS;

    case MPIDI_CH3I_VC_STATE_ACCEPTED:
        pEp->m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTED;
        break;

    default:
        break;
    }

    return pEp->Progress( pfMpiRequestDone );
}


int
CEndpoint::RecvFailed(
    _In_ nd_result_t* pResult,
    bool* /*pfMpiRequestDone*/
    )
{
    nd_recv_result_t *pRecvResult = static_cast<nd_recv_result_t*>(pResult);
    StackGuardRef<CEndpoint> pEp( pRecvResult->pEp );

    if( pResult->Status == ND_CANCELED )
        return MPI_SUCCESS;

    return MPIU_E_ERR(
        "**ch3|nd|recv_err %s %d %x",
        inet_ntoa( pEp->m_DestAddr.sin_addr ),
        pEp->m_DestAddr.sin_port,
        pRecvResult->Status
        );
}

_Success_ (return == MPI_SUCCESS)
int
CEndpoint::BindMw(
    __in CMr* pMr,
    __in const void* pBuf,
    __in SIZE_T Length,
    __out ND_MW_DESCRIPTOR* pMwDesc
    )
{
    StackGuardRef<CMw> pMw;
    int mpi_errno = m_pCq->Adapter()->AllocMw( &pMw.ref() );
    if (mpi_errno != MPI_SUCCESS)
    {
        return MPIU_ERR_FAIL(mpi_errno);
    }

    pMw->Bind( this, pMr, BindSucceeded, BindFailed, InvalidateSucceeded, InvalidateFailed );

    HRESULT hr = m_pIEndpoint->Bind(
        pMw->BindResult(),
        // pMw->InvalidateResult(),
        pMr->GetHandle(),
        pMw->IMw(),
        pBuf,
        Length,
        ND_OP_FLAG_ALLOW_READ,
        pMwDesc
        );
    if( FAILED( hr ) )
    {
        pMw->Unbind();
        m_pCq->Adapter()->FreeMw( pMw.detach() );
        return MPIU_E_ERR( "**ch3|nd|bind %x", hr );
    }

    m_MwList.push_back( *pMw.detach() );

    //
    // Take a reference for the duration of the bind.
    //
    AddRef();
    return MPI_SUCCESS;
}


int
CEndpoint::BindSucceeded(
    _In_ nd_result_t* pResult,
    bool* /*pfMpiRequestDone*/
    )
{
    CMw* pMw = CMw::FromBindResult( pResult );
    StackGuardRef<CEndpoint> pEp( pMw->Ep() );
    return MPI_SUCCESS;
}


int
CEndpoint::BindFailed(
    _In_ nd_result_t* pResult,
    bool* /*pfMpiRequestDone*/
    )
{
    CMw* pMw( CMw::FromBindResult( pResult ) );

    StackGuardRef<CEndpoint> pEp( pMw->Ep() );

    pEp->m_MwList.remove( *pMw );

    pMw->Unbind();
    pEp->m_pCq->Adapter()->FreeMw( pMw );

    return MPIU_E_ERR( "**ch3|nd|bind_err %x", pResult->Status );
}


int CEndpoint::InvalidateMw()
{
    //
    // Security: Need to check that the list isn't empty.
    //
    if( m_MwList.empty() )
        return MPIU_E_ERR( "**fail" );

    CMw* pMw = &m_MwList.front();
    m_MwList.pop_front();
    m_MwInvalidatingList.push_back( *pMw );

    HRESULT hr = m_pIEndpoint->Invalidate( pMw->InvalidateResult(), pMw->IMw(), 0 );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|unbind %x", hr );

    //
    // Take a reference for the duration of the invalidate.
    //
    AddRef();
    return MPI_SUCCESS;
}


int
CEndpoint::InvalidateSucceeded(
    __in nd_result_t* pResult,
    __out bool* pfMpiRequestDone
    )
{
    CMw* pMw = CMw::FromInvalidateResult( pResult );
    StackGuardRef<CEndpoint> pEp( pMw->Ep() );

    pEp->m_MwInvalidatingList.remove( *pMw );

    pMw->Unbind();
    pEp->m_pCq->Adapter()->FreeMw( pMw );

    if( !pEp->m_MwList.empty() )
        return MPI_SUCCESS;

    if( !pEp->m_MwInvalidatingList.empty() )
        return MPI_SUCCESS;

    {
        SendQLock lock(pEp->m_pVc);

        MPID_Request* pReq = MPIDI_CH3I_SendQ_head_unsafe( pEp->m_pVc );
        MPIU_Assert( pReq );

        if( pReq->dev.iov_offset != pReq->dev.iov_count )
        {
            return MPI_SUCCESS;
        }

        //
        // Done sending this iov, indicate this.
        //
        int fComplete;
        int mpi_errno = MPIDI_CH3U_Handle_send_req( pEp->m_pVc, pReq, &fComplete );
        if (mpi_errno != MPI_SUCCESS)
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        if( !fComplete )
        {
            //
            // The iov got reloaded as the request is not done yet; reset the iov_offset
            // and go send the next portion.
            //
            TraceSendNd_Continue(MPIDI_Process.my_pg_rank, pEp->m_pVc->pg_rank, pReq->ch.msg_id, pReq->dev.iov_count, pReq->iov_size());
            pReq->dev.iov_offset = 0;
        }
        else
        {
            //
            // This request is done, remove it from the queue and go send the next request.
            //
            TraceSendNd_Done(MPIDI_Process.my_pg_rank, pEp->m_pVc->pg_rank, pReq->ch.msg_id);
            MPIDI_CH3I_SendQ_dequeue_unsafe( pEp->m_pVc );
            *pfMpiRequestDone = true;
        }

        return pEp->ProcessSendsUnsafe( pfMpiRequestDone );
    }
}


int
CEndpoint::InvalidateFailed(
    _In_ nd_result_t* pResult,
    bool* /*pfMpiRequestDone*/
    )
{
    CMw* pMw = CMw::FromInvalidateResult( pResult );

    StackGuardRef<CEndpoint> pEp( pMw->Ep() );

    pEp->m_MwInvalidatingList.remove( *pMw );

    pMw->Unbind();
    pEp->m_pCq->Adapter()->FreeMw( pMw );

    return MPIU_E_ERR( "**ch3|nd|unbind_err %x", pResult->Status );
}


int
CEndpoint::Read(
    _In_ const ND_MW_DESCRIPTOR& MwDescriptor,
    _In_reads_(nDst) char* pDst,
    _In_ MPIU_Bsize_t nDst,
    _Inout_ MPIU_Bsize_t* pcbRead
    )
{
    //
    // Cap the maximum read size to the smaller of the maximum Read size
    // supported by the adapter.
    //
    nDst = static_cast<MPIU_Bsize_t>( min(nDst, m_pCq->Adapter()->MaxReadLength() ) );

    //
    // Register our local buffers and perform the RDMA read.
    //
    StackGuardRef<CMr> pMr;
    int mpi_errno = m_pCq->Adapter()->CreateMrVadAware( pDst, &nDst, 0, &pMr.ref() );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    //
    // Issue the RDMA read.
    //
    ND_SGE Sge;
    Sge.pAddr = pDst;
    Sge.Length = nDst;
    Sge.hMr = pMr->GetHandle();

    m_ReadResults[m_iRead].pMr = pMr.get();

    HRESULT hr = m_pIEndpoint->Read(
        &m_ReadResults[m_iRead],
        &Sge,
        1,
        &MwDescriptor,
        m_ReadOffset,
        0
        );
    if( hr != ND_SUCCESS )
    {
        return MPIU_E_ERR(
            "**ch3|nd|read %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            hr
            );
    }

    m_nReads++;
    m_iRead = ++m_iRead % NDv1_READ_LIMIT;

    //
    // Take a reference on the EP until the read completes.
    //
    AddRef();
    //
    // We hold the reference on the MR until the read completes.
    //
    pMr.detach();

    *pcbRead = nDst;
    return MPI_SUCCESS;
}


int
CEndpoint::ReadToIov(
    _In_ const ND_MW_DESCRIPTOR& MwDescriptor,
    _Out_ bool* pfRepost
    )
{
    MPIU_Bsize_t nSrc = (MPIU_Bsize_t)
        (_byteswap_uint64( MwDescriptor.Length ) - m_ReadOffset);
    MPIU_Assert( nSrc > 0 );

    *pfRepost = false;

    //
    // Note that the function will behave properly when the IOV offset equals the IOV count.
    // The assert is here to trap cases where we don't expect this to happen.  We do expect
    // it to happen if all buffers in the IOV are in flight, but not yet complete.
    //
    MPIU_Assert( m_pRecvActive->dev.iov_offset < m_pRecvActive->dev.iov_count || m_nReads > 0 );
    MPIU_Assert( m_pRecvActive->dev.iov[m_pRecvActive->dev.iov_offset].len > 0 );

    MPID_IOV* p = &m_pRecvActive->dev.iov[m_pRecvActive->dev.iov_offset];
    const MPID_IOV* end = &m_pRecvActive->dev.iov[m_pRecvActive->dev.iov_count];

    MPIU_Bsize_t cbRead;
    while( p < end && m_nReads < NDv1_READ_LIMIT )
    {
        int mpi_errno = Read( MwDescriptor, p->buf, min( p->len, nSrc ), &cbRead );
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        if( cbRead < p->len )
        {
            p->buf = (iovrecvbuf_t*)p->buf + cbRead;
            p->len -= static_cast<MPIU_Bsize_t>(cbRead);
        }
        else
        {
            p++;
        }

        if( cbRead < nSrc )
        {
            m_ReadOffset += cbRead;
            nSrc -= static_cast<MPIU_Bsize_t>(cbRead);
        }
        else
        {
            //
            // We've fully consumed the SrcAvail - repost the receive buffer.
            //
            *pfRepost = true;
            break;
        }
    }

    m_pRecvActive->dev.iov_offset = (int)(p - m_pRecvActive->dev.iov);
    return MPI_SUCCESS;
}


int
CEndpoint::ReadSucceeded(
    __in nd_result_t* pResult,
    __out bool* pfMpiRequestDone
    )
{
    nd_read_result_t* pReadResult = static_cast<nd_read_result_t*>(pResult);
    StackGuardRef<CEndpoint> pEp( pReadResult->pEp );

    MPIU_Assert( pEp->m_pRecvActive != NULL );
    MPIU_Assert( pEp->m_nReads > 0 );

    //
    // Release the MR immedately as the RegistrationCacheCallback might get called and
    // fail the assertion in CMr::Shutdown() that the MR is Idle().
    //
    pReadResult->pMr->Release();

    pEp->m_nReads--;

    if( pEp->m_pRecvActive->dev.iov_offset == pEp->m_pRecvActive->dev.iov_count )
    {
        //
        // If the receive request has all its buffers in flight, we need to wait until
        // all reads complete before reloading the IOV.  There's no reason to call
        // ProcessRecvs or ProcessSends until this happens.
        //
        if( pEp->m_nReads != 0 )
            return MPI_SUCCESS;

        TraceRecvNd_Data(pEp->m_pVc->pg_rank, MPIDI_Process.my_pg_rank, pEp->m_pVc->ch.n_recv);
        int fComplete;
        int mpi_errno = MPIDI_CH3U_Handle_recv_req(
            pEp->m_pVc,
            pEp->m_pRecvActive,
            &fComplete
            );

        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );

        if( fComplete )
        {
            pEp->m_pRecvActive = NULL;
            TraceRecvNd_Done(pEp->m_pVc->pg_rank, MPIDI_Process.my_pg_rank, pEp->m_pVc->ch.n_recv);
            *pfMpiRequestDone = true;
        }
        else
        {
            pEp->m_pRecvActive->dev.iov_offset = 0;
        }
    }

    return pEp->Progress( pfMpiRequestDone );
}


int
CEndpoint::ReadFailed(
    _In_ nd_result_t* pResult,
    bool* /*pfMpiRequestDone*/
    )
{
    nd_read_result_t* pReadResult = static_cast<nd_read_result_t*>(pResult);
    StackGuardRef<CEndpoint> pEp( pReadResult->pEp );
    pReadResult->pMr->Release();
    --pEp->m_nReads;
    return MPIU_E_ERR(
        "**ch3|nd|read_err %s %d %x",
        inet_ntoa( pEp->m_DestAddr.sin_addr ),
        pEp->m_DestAddr.sin_port,
        pReadResult->Status
        );
}

}   // namespace v1
}   // namespace CH3_ND


void
MPIDI_CH3I_Ndv1_disconnect(
    __inout MPIDI_VC_t* pVc
    )
{
    MPIU_Assert( pVc->ch.nd.pEpV1 != NULL );
    pVc->ch.nd.pEpV1->Disconnect();
}
