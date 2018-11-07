// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_endpoint.cpp - Network Direct MPI CH3 Channel endpoint object

--*/

#include "precomp.h"
#include "ch3u_nd2.h"


namespace CH3_ND
{


Endpoint::Endpoint(
        _In_ IND2Connector& connector,
        _In_ const struct sockaddr_in& destAddr
    ) : m_pVc( nullptr ),
    m_pIConnector( &connector ),
    m_pIEndpoint( nullptr ),
    m_pCq( nullptr ),
    m_DestAddr( destAddr ),
    m_nRef( 1 ),
    m_cbRecv( nullptr ),
    m_pIMr( nullptr ),
    m_MrToken( 0 ),
    m_iSend( 0 ),
    m_nSends( 0 ),
    m_SendQueueDepth( 0 ),
    m_iRead( 0 ),
    m_nReads( 0 ),
    m_ReadOffset( 0 ),
    m_nRdComplToGive( 0 ),
    m_iRecv( 0 ),
    m_nRecvs( 0 ),
    m_RecvQueueDepth( 0 ),
    m_nSendCredits( 0 ),
    m_nCreditsToGive( 0 ),
    m_nRdComplToRecv( 0 ),
    m_nConnectRetries( 0 ),
    m_pRecvActive( nullptr ),
    m_fDisconnect( false )
{
    connector.AddRef();

    m_Buffers.Send = nullptr;
    m_Buffers.Recv = nullptr;
}


int Endpoint::Init(
    _Inout_ Adapter& adapter,
    _Inout_ MPIDI_VC_t* pVc
    )
{
    m_SendQueueDepth = g_NdEnv.SendQueueDepth();
    m_SendQueueMask = m_SendQueueDepth - 1;
    m_RecvQueueDepth = g_NdEnv.RecvQueueDepth();
    m_RecvQueueMask = m_RecvQueueDepth - 1;
    m_nConnectRetries = g_NdEnv.ConnectRetries();

    //
    // Find a CQ that we can use.
    //
    int mpi_errno = adapter.GetAvailableCq( &m_pCq.ref() );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    m_pCq->AllocateEntries();
    HRESULT hr = adapter.IAdapter()->CreateQueuePair(
        IID_IND2QueuePair,
        m_pCq->ICq(),
        m_pCq->ICq(),
        this,
        m_RecvQueueDepth,
        g_NdEnv.InitiatorQueueDepth(),
        1,
        1,
        m_pCq->GetAdapter()->GetInlineThreshold(),
        reinterpret_cast<void**>( &m_pIEndpoint.ref() )
        );
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|create_ep %x", hr );
    }

    //
    // Allocate our buffers based on the user-requested queue depths.
    //
    m_cbRecv = new ULONG[m_RecvQueueDepth];
    if( m_cbRecv == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    m_Buffers.Send = new nd_msg_t[m_SendQueueDepth + m_RecvQueueDepth];
    if( m_Buffers.Send == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    m_Buffers.Recv = m_Buffers.Send + m_SendQueueDepth;

    //
    // Register the private buffer.
    //
    mpi_errno = adapter.RegisterMemory(
        reinterpret_cast<const char*>(m_Buffers.Send),
        sizeof(nd_msg_t) * (m_SendQueueDepth + m_RecvQueueDepth),
        ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_DO_NOT_SECURE_VM,
        &m_pIMr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }
    m_MrToken = m_pIMr->GetLocalToken();

    mpi_errno = PrepostReceives();
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    //
    // Associate the EP with the VC.
    //
    MPIU_Assert( pVc->ch.nd.pEp == NULL );
    pVc->ch.nd.pEp = this;
    m_pVc = pVc;
    AddRef();

    return MPI_SUCCESS;
}


int Endpoint::Reinit()
{
    MPIDI_VC_t* pVc = m_pVc;
    StackGuardRef<Adapter> pAdapter( m_pCq->GetAdapter() );
    pAdapter->AddRef();
    struct sockaddr_in destAddr = m_DestAddr;

    Abandon( MPIDI_CH3I_VC_STATE_UNCONNECTED );

    StackGuardRef<IND2Connector> pIConnector;
    int mpi_errno = pAdapter->CreateConnector( &pIConnector.ref() );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    MPIU_Assert( pVc->ch.nd.pEp == NULL );
    StackGuardRef<Endpoint> pEp;
    mpi_errno = Endpoint::Create(
        *(pAdapter.get()),
        pIConnector.get(),
        destAddr,
        pVc,
        &pEp.ref()
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    mpi_errno = pEp->Connect();
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    return MPI_SUCCESS;
}


Endpoint::~Endpoint()
{
    if( m_pIMr != NULL )
    {
        m_pCq->GetAdapter()->DeregisterMemory( m_pIMr );
    }

    if( m_Buffers.Send != nullptr )
    {
        delete[] m_Buffers.Send;
    }

    if( m_cbRecv != nullptr )
    {
        delete[] m_cbRecv;
    }

    m_pCq->FreeEntries();

    if( m_pVc )
    {
        MPIU_Assert( m_pVc->ch.nd.pEp == NULL );
        m_pVc->ch.state = MPIDI_CH3I_VC_STATE_UNCONNECTED;
        MPIDI_CH3U_Handle_connection( m_pVc, MPIDI_VC_EVENT_TERMINATED );
    }
}


MPI_RESULT
Endpoint::Create(
    _Inout_ Adapter& adapter,
    _In_ IND2Connector* pIConnector,
    _In_ const struct sockaddr_in& destAddr,
    _Inout_ MPIDI_VC_t* pVc,
    _Outptr_ Endpoint** ppEp
    )
{
    StackGuardRef<Endpoint> pEp( new Endpoint( *pIConnector, destAddr ) );
    if( pEp.get() == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    int mpi_errno = pEp->Init(
        adapter,
        pVc
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    *ppEp = pEp.detach();
    return MPI_SUCCESS;
}


void Endpoint::Release()
{
    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}


int Endpoint::Connect()
{
    //
    // Connect
    //
    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
    Trace_ND_Info_Endpoint(
        this,
        EndpointConnect,
        0,
        inet_ntoa( localAddress.sin_addr ),
        localAddress.sin_port,
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port
        );

    m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTING;
    return HandleTimeout();
}


int
Endpoint::CompleteConnect()
{
    DWORD len = sizeof(m_nSendCredits);
    HRESULT hr = m_pIConnector->GetPrivateData( &m_nSendCredits, &len );
    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
    switch( hr )
    {
    case ND_BUFFER_OVERFLOW:
        break;

    case ND_SUCCESS:
        if( len >= sizeof(m_nSendCredits) )
        {
            break;
        }
        __fallthrough;

    case ND_INVALID_BUFFER_SIZE:
        MPIU_Assert( len < sizeof(m_nSendCredits) );
        Abandon( MPIDI_CH3I_VC_STATE_FAILED );
        Trace_ND_Error_Endpoint(
            this,
            EndpointCompleteConnectBufferSize,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );
        return MPIU_E_ERR(
            "**ch3|nd|conn_data_len %s %d %d",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            len
            );
    case ND_CONNECTION_ABORTED:
    case ND_CONNECTION_INVALID:
        Trace_ND_Error_Endpoint(
            this,
            EndpointCompleteConnectAbortedOrInvalid,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );
        return Reinit();
    default:
        Abandon( MPIDI_CH3I_VC_STATE_FAILED );
        Trace_ND_Error_Endpoint(
            this,
            EndpointCompleteConnectDefault,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );
        return MPIU_E_ERR(
            "**ch3|nd|conn_data %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            hr
            );
    }

//TODO: Wrap setting callbacks and calling CompleteConnect as inlines in a header.
    Trace_ND_Info_Endpoint(
        this,
        EndpointCompleteConnectCompleteConnect,
        0,
        inet_ntoa( localAddress.sin_addr ),
        localAddress.sin_port,
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port
        );

    AddRef();
    m_ConnectorOv.pfnCompletion = CompleteConnectHandler;
    hr = m_pIConnector->CompleteConnect( &m_ConnectorOv );
    switch( hr )
    {
    case ND_SUCCESS:
        Release();
        return ConnCompleted();

    case ND_PENDING:
        Trace_ND_Info_Endpoint(
            this,
            EndpointCompleteConnectPending,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );
        return MPI_SUCCESS;
    default:
        Release();
        return ConnFailed( hr );
    }
}


int
Endpoint::HandleTimeout()
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
    pdata.Credits = m_RecvQueueDepth;
    pdata.Rank = PMI_Get_rank();
    pdata.GroupId = MPIDI_Process.my_pg->id;

    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
    Trace_ND_Info_Endpoint(
        this,
        EndpointHandleTimeoutConnect,
        0,
        inet_ntoa( localAddress.sin_addr ),
        localAddress.sin_port,
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port
        );

//TODO: Wrap setting callbacks and calling Connect as inlines in a header.
    m_ConnectorOv.pfnCompletion = ConnectHandler;
    AddRef();
    HRESULT hr = m_pIConnector->Connect(
        m_pIEndpoint.get(),
        reinterpret_cast<const struct sockaddr*>(&m_DestAddr),
        sizeof(m_DestAddr),
        ND_READ_LIMIT,
        ND_READ_LIMIT,
        &pdata,
        sizeof(pdata),
        &m_ConnectorOv
        );

    OACR_WARNING_DISABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from NTSTATUS to HRESULT OK.");
    switch( hr )
    {
    case ND_SUCCESS:
        Release();
        return CompleteConnect();

    case ND_PENDING:
        return MPI_SUCCESS;

    case STATUS_BAD_NETWORK_NAME:
        if( m_nConnectRetries == 0 )
        {
            Release();
            return ConnReqFailed( hr );
        }
        --m_nConnectRetries;
        __fallthrough;
    case ND_TIMEOUT:
    case ND_IO_TIMEOUT:
        Trace_ND_Info_Endpoint(
            this,
            EndpointHandleTimeoutConnectTimeout,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );

        m_ConnectorOv.pfnCompletion = ConnectRetryHandler;
        PostQueuedCompletionStatus(
            m_pCq->GetAdapter()->GetExSetHandle(),
            0,
            EX_KEY_ND,
            &m_ConnectorOv
            );
        return MPI_SUCCESS;

    default:
        Release();
        return ConnReqFailed( hr );
    }
    OACR_WARNING_ENABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from NTSTATUS to HRESULT OK.");
}

OACR_WARNING_DISABLE(28301, "PREfast does not handle FN types correctly.");
int
Endpoint::ConnectRetryHandler(
    __in ND_OVERLAPPED* pOverlapped
    )
{
    StackGuardRef<Endpoint> pEp( CONTAINING_RECORD( pOverlapped, Endpoint, m_ConnectorOv ) );
    return pEp->HandleTimeout();
}


OACR_WARNING_DISABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from NTSTATUS to HRESULT OK.");
int
Endpoint::ConnectHandler(
    __in ND_OVERLAPPED* pOverlapped
    )
{
    StackGuardRef<Endpoint> pEp( CONTAINING_RECORD( pOverlapped, Endpoint, m_ConnectorOv ) );
    HRESULT hr = pEp->GetOverlappedResult();

    switch( hr )
    {
    case ND_SUCCESS:
        return pEp->CompleteConnect();

    case STATUS_BAD_NETWORK_NAME:
        if( pEp->m_nConnectRetries == 0 )
        {
            return pEp->ConnReqFailed( hr );
        }
        --pEp->m_nConnectRetries;
        __fallthrough;
    case ND_TIMEOUT:
    case ND_IO_TIMEOUT:
        return pEp->HandleTimeout();

    case ND_PENDING:
        //
        // Will be processed again at completion.
        //
        pEp.detach();
        return MPI_SUCCESS;

    default:
        return pEp->ConnReqFailed( hr );
    }
}
OACR_WARNING_ENABLE(COMPARING_HRESULT_TO_INT, "Implicit cast from NTSTATUS to HRESULT OK.");
OACR_WARNING_ENABLE(28301, "PREfast does not handle FN types correctly.");


int Endpoint::ConnReqFailed( __in HRESULT hr )
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
            struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
            Trace_ND_Info_Endpoint(
                this,
                EndpointConnReqFailedPassive,
                hr,
                inet_ntoa( localAddress.sin_addr ),
                localAddress.sin_port,
                inet_ntoa( m_DestAddr.sin_addr ),
                m_DestAddr.sin_port
                );
            Abandon( MPIDI_CH3I_VC_STATE_UNCONNECTED );
            return MPI_SUCCESS;
        }

        __fallthrough;

    case ND_CANCELED:
        {
            MPIU_Assert( m_pVc == NULL );
            struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
            Trace_ND_Info_Endpoint(
                this,
                EndpointConnReqFailedCanceled,
                hr,
                inet_ntoa( localAddress.sin_addr ),
                localAddress.sin_port,
                inet_ntoa( m_DestAddr.sin_addr ),
                m_DestAddr.sin_port
                );
            return MPI_SUCCESS;
        }

    default:
        {
            Abandon( MPIDI_CH3I_VC_STATE_FAILED );
            struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
            Trace_ND_Error_Endpoint(
                this,
                EndpointConnReqFailedFailed,
                hr,
                inet_ntoa( localAddress.sin_addr ),
                localAddress.sin_port,
                inet_ntoa( m_DestAddr.sin_addr ),
                m_DestAddr.sin_port
                );

            return MPIU_E_ERR(
                "**ch3|nd|conn %s %d %x",
                inet_ntoa( m_DestAddr.sin_addr ),
                m_DestAddr.sin_port,
                hr
                );
        }
    }
}

OACR_WARNING_DISABLE(28301, "PREfast does not handle FN types correctly.");
int
Endpoint::CompleteConnectHandler(
    __in ND_OVERLAPPED* pOverlapped
    )
{
    StackGuardRef<Endpoint> pEp( CONTAINING_RECORD( pOverlapped, Endpoint, m_ConnectorOv ) );
    HRESULT hr = pEp->GetOverlappedResult();

    if( FAILED( hr ) || hr == ND_TIMEOUT )
    {
        return pEp->ConnFailed( hr );
    }

    return pEp->ConnCompleted();
}
OACR_WARNING_ENABLE(28301, "PREfast does not handle FN types correctly.");


int Endpoint::ConnCompleted()
{
    SetState( MPIDI_CH3I_VC_STATE_CONNECTED );
    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
    Trace_ND_Info_Endpoint(
        this,
        EndpointConnCompleted,
        0,
        inet_ntoa( localAddress.sin_addr ),
        localAddress.sin_port,
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port
        );
    //
    // Woot, we're connected.  Start sending data.
    //
    m_pVc->eager_max_msg_sz = g_NdEnv.EagerLimit();
    MPIU_Assert( !MPIDI_CH3I_SendQ_empty(m_pVc) );
    bool fMpiRequestDone = false;
    return ProcessSends( &fMpiRequestDone );
}


int Endpoint::ConnFailed( __in HRESULT hr )
{
    MPIU_Assert( FAILED( hr ) );
    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();

    switch( hr )
    {
    case ND_CONNECTION_ABORTED:
    case ND_CONNECTION_INVALID:
    case ND_TIMEOUT:
    case ND_IO_TIMEOUT:
        Trace_ND_Info_Endpoint(
            this,
            EndpointConnFailedRetry,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );
        return Reinit();

    default:
        break;
    }

    Abandon( MPIDI_CH3I_VC_STATE_FAILED );
    Trace_ND_Error_Endpoint(
        this,
        EndpointConnFailedFail,
        hr,
        inet_ntoa( localAddress.sin_addr ),
        localAddress.sin_port,
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port
        );

    return MPIU_E_ERR(
        "**ch3|nd|comp_conn %s %d %x",
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port,
        hr
        );
}


int
Endpoint::Accept(
    __in const UINT8 nSendCredits
    )
{
    m_nSendCredits = nSendCredits;
    UINT8 creditsToGive = m_RecvQueueDepth;

    m_ConnectorOv.pfnCompletion = AcceptHandler;
    AddRef();
    HRESULT hr = m_pIConnector->Accept(
        m_pIEndpoint.get(),
        ND_READ_LIMIT,
        ND_READ_LIMIT,
        &creditsToGive,
        sizeof(creditsToGive),
        &m_ConnectorOv
        );

    m_pVc->ch.channel = MPIDI_CH3I_CH_TYPE_ND;
    m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTING;

    switch( hr )
    {
    case ND_SUCCESS:
        Release();
        return AcceptCompleted();

    case ND_PENDING:
        {
            struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
            Trace_ND_Info_Endpoint(
                this,
                EndpointAcceptPending,
                0,
                inet_ntoa( localAddress.sin_addr ),
                localAddress.sin_port,
                inet_ntoa( m_DestAddr.sin_addr ),
                m_DestAddr.sin_port
                );
            return MPI_SUCCESS;
        }

    default:
        Release();
        return AcceptFailed( hr );
    }
}


int
Endpoint::PrepostReceives()
{
    //
    // Prepost receives.
    //
    ND2_SGE sge;
    sge.BufferLength = sizeof(nd_msg_t);
    sge.MemoryRegionToken = m_MrToken;

    for( UINT8 i = 0; i < m_RecvQueueDepth; i++ )
    {
        sge.Buffer = &m_Buffers.Recv[i];
        AddRef();
        HRESULT hr = m_pIEndpoint->Receive(
            &m_cbRecv[i],
            &sge,
            1
            );
        if( FAILED(hr) )
        {
            Release();
            //
            // Flushing assumes that the endpoint will complete the posted receives
            // with a ND_CANCELED status.  What do we do if this fails?
            //
#pragma prefast(disable:25031, "Don't care about the status of flush - no possible recovery.");
            m_pIEndpoint->Flush();
            struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
            Trace_ND_Error_Endpoint(
                this,
                EndpointPrepostReceivesFailed,
                hr,
                inet_ntoa( localAddress.sin_addr ),
                localAddress.sin_port,
                inet_ntoa( m_DestAddr.sin_addr ),
                m_DestAddr.sin_port
                );
            return MPIU_E_ERR( "**ch3|nd|recv %x", hr );
        }
    }

    return MPI_SUCCESS;
}


void
Endpoint::Abandon(
    __in MPIDI_CH3I_VC_state_t state
    )
{
    MPIU_Assert( m_pIEndpoint.get() != NULL );

#pragma prefast(disable:25031, "Don't care about the status of flush - no possible recovery.");
    m_pIEndpoint->Flush();

#pragma prefast(disable:25031, "Don't care about the status of cancel - no possible recovery.");
    m_pIConnector->CancelOverlappedRequests();

    //
    // We must clear the back pointer so that a subsequent release doesn't
    // close the VC - we're just orphaning the endpoint.
    //
    if( m_pVc != NULL )
    {
        SetState( state );

        //
        // Disassociate the endpoint from the VC.
        //
        m_pVc->ch.nd.pEp = NULL;
        Release();
        m_pVc = NULL;
    }
}


OACR_WARNING_DISABLE(28301, "PREfast does not handle FN types correctly.");
int
Endpoint::AcceptHandler(
    __in ND_OVERLAPPED* pOverlapped
    )
{
    StackGuardRef<Endpoint> pEp( CONTAINING_RECORD( pOverlapped, Endpoint, m_ConnectorOv ) );

    HRESULT hr = pEp->GetOverlappedResult();
    if( hr != ND_SUCCESS )
    {
        return pEp->AcceptFailed( hr );
    }

    return pEp->AcceptCompleted();
}
OACR_WARNING_ENABLE(28301, "PREfast does not handle FN types correctly.");


int Endpoint::AcceptCompleted()
{
    m_pVc->eager_max_msg_sz = g_NdEnv.EagerLimit();

    //
    // We don't transition the VC to the connected state until we've received
    // our first packet.  This is to honor the iWARP requirement that the
    // active side send first.  However, the first packet can race the accept
    // completion, so we may need to make progress on queued receives.
    //

    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
    Trace_ND_Info_Endpoint(
        this,
        EndpointAcceptCompleted,
        0,
        inet_ntoa( localAddress.sin_addr ),
        localAddress.sin_port,
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port
        );
    switch( m_pVc->ch.state )
    {
    case MPIDI_CH3I_VC_STATE_CONNECTING:
        m_pVc->ch.state = MPIDI_CH3I_VC_STATE_ACCEPTED;
        return MPI_SUCCESS;

    case MPIDI_CH3I_VC_STATE_PENDING_ACCEPT:
        m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTED;
        break;

    default:
        MPIU_Assert(
            m_pVc->ch.state == MPIDI_CH3I_VC_STATE_CONNECTING ||
            m_pVc->ch.state == MPIDI_CH3I_VC_STATE_PENDING_ACCEPT
            );
        break;
    }

    bool requestDone = false;
    return Progress( &requestDone );
}


int Endpoint::AcceptFailed( __in HRESULT hr )
{
    MPIU_Assert( FAILED( hr ) || hr == ND_TIMEOUT );
    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();

    switch( hr )
    {
    case ND_CONNECTION_ABORTED:
    case ND_TIMEOUT:
    case ND_IO_TIMEOUT:
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
        Trace_ND_Info_Endpoint(
            this,
            EndpointAcceptFailedAbortedOrTimeout,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );
        return MPI_SUCCESS;

    default:
        Abandon( MPIDI_CH3I_VC_STATE_FAILED );
        Trace_ND_Error_Endpoint(
            this,
            EndpointAcceptFailedFailed,
            hr,
            inet_ntoa( localAddress.sin_addr ),
            localAddress.sin_port,
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port
            );
        return MPIU_E_ERR( "**ch3|nd|accept %x", hr );
    }
}


void Endpoint::Disconnect()
{
    m_fDisconnect = true;
    struct sockaddr_in localAddress = m_pCq->GetAdapter()->Addr();
    Trace_ND_Info_Endpoint(
        this,
        EndpointDisconnect,
        0,
        inet_ntoa( localAddress.sin_addr ),
        localAddress.sin_port,
        inet_ntoa( m_DestAddr.sin_addr ),
        m_DestAddr.sin_port
        );

    //
    // We need to make sure that any outbound requests (send, bind/unbind, and RDMA) complete
    // before we tear the connection down.
    //
    if( m_nSends != 0 )
    {
        return;
    }

    //
    // We need to make sure that any queued requests get sent before connection tear down.
    //
    if( !MPIDI_CH3I_SendQ_empty(m_pVc))
    {
        return;
    }

    DoDisconnect();
}


void Endpoint::DoDisconnect()
{
    //
    // Disassociate the endpoint from the VC.  Note that we leave the back pointer
    // so that we will call MPIDI_CH3U_Handle_connection when the reference count
    // reaches zero.
    //
    m_pVc->ch.nd.pEp = NULL;
    Release();

    m_ConnectorOv.pfnCompletion = DisconnectHandler;
    //
    // We disconnect asynchronously to allow things to overlap
    // as much as possible (helps jobs terminate quickly).
    //
    AddRef();
    HRESULT hr = m_pIConnector->Disconnect( &m_ConnectorOv );
    if( hr == ND_PENDING )
    {
        return;
    }

    if( FAILED( hr ) )
    {
        Release();
#pragma prefast(disable:25031, "Don't care about the status of flush - no possible recovery.");
        m_pIEndpoint->Flush();
    }

    Release();
}

OACR_WARNING_DISABLE(28301, "PREfast does not handle FN types correctly.");
int
Endpoint::DisconnectHandler(
    __in ND_OVERLAPPED* pOverlapped
    )
{
    StackGuardRef<Endpoint> pEp( CONTAINING_RECORD( pOverlapped, Endpoint, m_ConnectorOv ) );
    HRESULT hr = pEp->GetOverlappedResult();
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|dconn %x", hr );
    }
    return MPI_SUCCESS;
}
OACR_WARNING_ENABLE(28301, "PREfast does not handle FN types correctly.");


int
Endpoint::Send(
    __inout const MPID_IOV* pIov,
    __in int nIov,
    __out MPIU_Bsize_t* pcbSent
    )
{
    MPIU_Assert( MPIDI_CH3I_SendQ_empty(m_pVc) );

    *pcbSent = 0;

    int iIov = 0;
    const char* pSrc = pIov[iIov].buf;
    MPIU_Bsize_t cbSrc = pIov[iIov].len;

    do
    {
        if( !OkToSend() )
        {
            return MPI_SUCCESS;
        }

        nd_msg_t* pMsg = &m_Buffers.Send[m_iSend];

        UINT8* pDst = pMsg->Data;
        MPIU_Bsize_t cbDst = sizeof(pMsg->Data);

        //
        // Copy data to the private buffer.
        //
        while( cbDst && cbSrc )
        {
            if( m_pCq->GetAdapter()->UseRma( cbSrc )  )
            {
                //
                // This function cannot send via zCopy since there is no associated
                // MPI request.
                //
                // If we haven't copied anything into our buffer, we return.  Otherwise
                // we send what we have copied so far and then return.
                //
                if( cbDst == sizeof(pMsg->Data) )
                {
                    return MPI_SUCCESS;
                }

                break;
            }

            if( cbSrc > cbDst )
            {
                CopyMemory( pDst, pSrc, cbDst );
                pSrc += cbDst;
                cbSrc -= cbDst;
                cbDst = 0;
                break;
            }

            CopyMemory( pDst, pSrc, cbSrc );
            pDst += cbSrc;
            cbDst -= cbSrc;
            iIov++;
            if( iIov == nIov )
            {
                break;
            }

            pSrc = pIov[iIov].buf;
            cbSrc = pIov[iIov].len;
        }

        *pcbSent += sizeof(pMsg->Data) - cbDst;

        int mpi_errno = SendNextMsg( NdMsgTypeData, sizeof(*pMsg) - cbDst );
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

    } while( iIov < nIov );

    return MPI_SUCCESS;
}


//
// Endpoint::ProcessSendsUnsafe
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
Endpoint::ProcessSendsUnsafe(
    __inout bool* pfMpiRequestDone
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
        nd_msg_type_t msgType = NdMsgTypeData;

        int iIov = pReq->dev.iov_offset;
        const char* pSrc = pReq->dev.iov[iIov].buf;
        MPIU_Bsize_t cbSrc = pReq->dev.iov[iIov].len;

        UINT8* pDst = pMsg->Data;
        MPIU_Bsize_t cbDst = sizeof(pMsg->Data);

        //
        // Format the private buffer.
        //
        while( cbDst && cbSrc )
        {
            if( m_pCq->GetAdapter()->UseRma( cbSrc ) )
            {
                //
                // If we don't have room for the MW descriptor, break and
                // send this message.  We'll send the SrcAvail in the next
                // message.  Note that we don't copy any data from this IOV
                // for fear of bringing it just bellow the zCopy threshold
                // and sticking with bCopy.
                //
                if( cbDst < sizeof(ND_MW_DESCRIPTOR) )
                {
                    break;
                }

                //
                // We must cap the size of the memory registration to what the adapter can support.
                //
                MPIU_Bsize_t rmaSize = static_cast<MPIU_Bsize_t>(
                    min( cbSrc, m_pCq->GetAdapter()->MaxReadLength() )
                    );

                //
                // TODO: Make CreateMr asynchronous.
                //
                StackGuardRef<Mr> pMr;
                mpi_errno = m_pCq->GetAdapter()->CreateMrVadAware(
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
                cbDst -= sizeof(ND_MW_DESCRIPTOR);

                //
                // We piggy-back some of the data in the SrcAvail message
                // so we only need to bind the remainder of the buffer.
                //
                MPIU_Assert( rmaSize - cbDst > 0 );
                pMsg->SrcAvail.Buffer = reinterpret_cast<ULONG_PTR>( pSrc ) + cbDst;
                pMsg->SrcAvail.Length = rmaSize - cbDst;
                pMsg->SrcAvail.Token = pMr->IMr()->GetRemoteToken();

                //
                // Fill the rest of the buffer and adjust the size of the descriptor
                //
                if( cbDst > 0 )
                {
                    CopyMemory( pDst, pSrc, cbDst );
                    cbDst = 0;
                }

                msgType = NdMsgTypeSrcAvail;

                if( rmaSize < cbSrc )
                {
                    pReq->dev.iov[iIov].len -= rmaSize;
                    pReq->dev.iov[iIov].buf += rmaSize;
                }
                else
                {
                    iIov++;
                }

                m_nRdComplToRecv++;
                break;
            }

bcopy:
            if( cbSrc > cbDst )
            {
                CopyMemory( pDst, pSrc, cbDst );
                pSrc += cbDst;
                cbSrc -= cbDst;
                cbDst = 0;
                pReq->dev.iov[iIov].len = cbSrc;
                pReq->dev.iov[iIov].buf = const_cast<iovsendbuf_t*>(pSrc);
                break;
            }

            CopyMemory( pDst, pSrc, cbSrc );
            pDst += cbSrc;
            cbDst -= cbSrc;
            iIov++;
            if( iIov == pReq->dev.iov_count )
            {
                break;
            }

            pSrc = pReq->dev.iov[iIov].buf;
            cbSrc = pReq->dev.iov[iIov].len;
        }
        pReq->dev.iov_offset = iIov;

        mpi_errno = SendNextMsg( msgType, sizeof(*pMsg) - cbDst );
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        //
        // More of the iov to send; go send the next portion.
        //
        if( pReq->dev.iov_offset != pReq->dev.iov_count )
        {
            continue;
        }

        //
        // zCopy operations outstanding - wait until complete before moving to next request.
        //
        if( m_nRdComplToRecv > 0 )
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
            TraceSendNd_Continue(
                MPIDI_Process.my_pg_rank,
                m_pVc->pg_rank,
                pReq->ch.msg_id,
                pReq->dev.iov_count,
                pReq->iov_size()
                );
            pReq->dev.iov_offset = 0;
            continue;
        }

        //
        // This request is done, remove it from the queue and go send the next request.
        //
        TraceSendNd_Done(
            MPIDI_Process.my_pg_rank,
            m_pVc->pg_rank,
            pReq->ch.msg_id
            );
        MPIDI_CH3I_SendQ_dequeue_unsafe( m_pVc );
        *pfMpiRequestDone = true;

        pReq = MPIDI_CH3I_SendQ_head_unsafe(m_pVc);
        if( pReq == NULL )
        {
            return MPI_SUCCESS;
        }

        TraceSendNd_Head(
            MPIDI_Process.my_pg_rank,
            m_pVc->pg_rank,
            pReq->ch.msg_id,
            pReq->dev.iov_count,
            pReq->iov_size(),
            pReq->dev.pkt.type
            );
    }
}


int
Endpoint::Progress( __inout bool *pfMpiRequestDone )
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
Endpoint::SendFlowControlMsg(
    )
{
    if( !OkToSend() )
    {
        return MPI_SUCCESS;
    }

    //
    // We send an update below our credit threshold if there are any
    // read complete notifications to send.
    //
    if( m_nCreditsToGive < (m_RecvQueueDepth >> 1) && m_nRdComplToGive == 0 )
    {
        return MPI_SUCCESS;
    }

    int mpi_errno = SendNextMsg( NdMsgTypeFlowControl, sizeof(nd_hdr_t) );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    return MPI_SUCCESS;
}


int
Endpoint::SendNextMsg(
    __in nd_msg_type_t type,
    __in ULONG cbSend
    )
{
    nd_msg_t* pMsg = &m_Buffers.Send[m_iSend];

    MPIU_Assert( type == NdMsgTypeFlowControl || cbSend > sizeof(nd_hdr_t) );
    pMsg->Type = type;
    pMsg->Credits = m_nCreditsToGive;
    pMsg->nRdCompl = m_nRdComplToGive;

    ND2_SGE sge;
    sge.Buffer = pMsg;
    sge.BufferLength = cbSend;
    sge.MemoryRegionToken = m_MrToken;

    ULONG flags = 0;
    if( m_pCq->GetAdapter()->UseInline( cbSend ) == true )
    {
        flags |= ND_OP_FLAG_INLINE;
    }
    if( m_nRdComplToGive != 0 )
    {
        //
        // If we have Read operations outstanding our send could indicate a Read
        // complete (nd_msg_t::RdCompl > 0).  Tell the HW to delay the send until
        // all outstanding Reads are complete.  If there are no reads outstanding,
        // the ND_OP_FLAG_READ_FENCE has no effect.
        //
        flags |= ND_OP_FLAG_READ_FENCE;
    }

    AddRef();
    HRESULT hr = m_pIEndpoint->Send(
        NULL,
        &sge,
        1,
        flags
        );
    if( FAILED( hr ) )
    {
        Release();
        return MPIU_E_ERR(
            "**ch3|nd|send %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            hr
            );
    }
    m_iSend = ++m_iSend & m_SendQueueMask;
    m_nSends++;
    m_nSendCredits--;
    m_nCreditsToGive = 0;
    m_nRdComplToGive = 0;

    return MPI_SUCCESS;
}


int
Endpoint::SendCompletion(
    __in const ND2_RESULT* pResult,
    __out bool* pfMpiRequestDone
    )
{
    m_nSends--;

    if( FAILED( pResult->Status ) )
    {
        //
        // If we are disconnecting, and the last send gets flushed, pretend it
        // succeeded as we are now disconnected.
        //
        if( !(pResult->Status == ND_CANCELED && m_fDisconnect == true) )
        {
            return MPIU_E_ERR(
                "**ch3|nd|send_err %s %d %x",
                inet_ntoa( m_DestAddr.sin_addr ),
                m_DestAddr.sin_port,
                pResult->Status
                );
        }
    }

    MPIU_Assert( m_pVc->ch.nd.pEp != NULL );

    int mpi_errno = ProcessSends( pfMpiRequestDone );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    if( m_fDisconnect && m_nSends == 0 && MPIDI_CH3I_SendQ_empty(m_pVc) )
    {
        DoDisconnect();
    }

    return MPI_SUCCESS;
}


int
Endpoint::ProcessReceives(
    _Out_ bool* pfMpiRequestDone
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
            {
                return MPI_SUCCESS;
            }

            mpi_errno = ProcessDataMsg( m_iRecv, pfMpiRequestDone );
            if( mpi_errno != MPI_SUCCESS )
            {
                return MPIU_ERR_FAIL( mpi_errno );
            }

            break;

        case NdMsgTypeSrcAvail:
            if( m_nReads == ND_READ_LIMIT )
            {
                return MPI_SUCCESS;
            }

            bool fRepost = false;
            if( m_ReadOffset != 0 )
            {
                //
                // Continue processing an existing SrcAvail.
                //
                mpi_errno = ReadToIov(
                    m_Buffers.Recv[m_iRecv].SrcAvail,
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
            {
                return MPIU_ERR_FAIL( mpi_errno );
            }

            if( !fRepost )
            {
                return MPI_SUCCESS;
            }
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

        MPIU_Assert( m_cbRecv[m_iRecv] == 0 );

        //
        // Repost the receive buffer.
        //
        if( !m_fDisconnect )
        {
            ND2_SGE sge;
            sge.Buffer = &m_Buffers.Recv[m_iRecv];
            sge.BufferLength = sizeof(m_Buffers.Recv[m_iRecv]);
            sge.MemoryRegionToken = m_MrToken;

            AddRef();
            HRESULT hr = m_pIEndpoint->Receive( &m_cbRecv[m_iRecv], &sge, 1 );
            if( FAILED( hr ) )
            {
                Release();
                return MPIU_E_ERR( "**ch3|nd|recv %x", hr );
            }

            m_nCreditsToGive++;
        }

        //
        // Remove the request from the pending queue since it is complete.
        //
        m_nRecvs--;
        m_iRecv = ++m_iRecv & m_RecvQueueMask;
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

    *piov_offset = static_cast<int>(p - iov); // next offset to process
    return (size - nb); // number of bytes copied
}


int Endpoint::ProcessFlowControlData( __in UINT8 iRecv, __out bool* pfMpiRequestDone )
{
    m_nSendCredits = m_nSendCredits + m_Buffers.Recv[iRecv].Credits;

    return ProcessReadComplete( m_Buffers.Recv[iRecv].nRdCompl, pfMpiRequestDone );
}


int
Endpoint::ProcessDataMsg(
    _In_ UINT8 iRecv,
    _Inout_ bool* pfMpiRequestDone
    )
{
    int mpi_errno;
    const UINT8* pSrc = m_Buffers.Recv[iRecv].Data;

    if( m_pRecvActive == NULL )
    {
        MPIU_Assert( m_cbRecv[iRecv] >= sizeof(MPIDI_CH3_Pkt_t) );

        //
        // TODO: Change this so it can queue up received buffers to avoid
        // a copy (only copy once to the app's buffer).
        //

        m_pVc->ch.n_recv++;
        TraceRecvNd_Packet(
            m_pVc->pg_rank,
            MPIDI_Process.my_pg_rank,
            m_pVc->ch.n_recv,
            reinterpret_cast<const MPIDI_CH3_Pkt_t*>(pSrc)->type
            );
        mpi_errno = MPIDI_CH3U_Handle_recv_pkt(
            m_pVc,
            reinterpret_cast<const MPIDI_CH3_Pkt_t*>(pSrc),
            &m_pRecvActive
            );

        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        m_cbRecv[iRecv] -= sizeof(MPIDI_CH3_Pkt_t);

        if( m_pRecvActive == NULL )
        {
            //
            // Zero-byte MPI message.  Return to repost the receive.
            //
            MPIU_Assert( m_cbRecv[iRecv] == 0 );
            TraceRecvNd_Done(
                m_pVc->pg_rank,
                MPIDI_Process.my_pg_rank,
                m_pVc->ch.n_recv
                );
            *pfMpiRequestDone = true;
            return MPI_SUCCESS;
        }

        m_pRecvActive->dev.iov_offset = 0;
        pSrc += sizeof(MPIDI_CH3_Pkt_t);
    }

    MPIU_Assert( m_pRecvActive != NULL );

    while( m_cbRecv[iRecv] )
    {
        MPIU_Bsize_t nCopy;
        nCopy = __CopyToIov(
            pSrc,
            static_cast<MPIU_Bsize_t>(m_cbRecv[iRecv]),
            m_pRecvActive->dev.iov,
            m_pRecvActive->dev.iov_count,
            &m_pRecvActive->dev.iov_offset
            );

        pSrc += nCopy;
        m_cbRecv[iRecv] -= nCopy;

        if( m_pRecvActive->dev.iov_count != m_pRecvActive->dev.iov_offset )
        {
            continue;
        }

        TraceRecvNd_Data(
            m_pVc->pg_rank,
            MPIDI_Process.my_pg_rank,
            m_pVc->ch.n_recv
            );
        int fComplete;
        mpi_errno = MPIDI_CH3U_Handle_recv_req(
            m_pVc,
            m_pRecvActive,
            &fComplete
            );

        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        if( fComplete )
        {
            MPIU_Assert( m_cbRecv[iRecv] == 0 );
            m_pRecvActive = NULL;
            TraceRecvNd_Done(
                m_pVc->pg_rank,
                MPIDI_Process.my_pg_rank,
                m_pVc->ch.n_recv
                );
            *pfMpiRequestDone = true;
            return MPI_SUCCESS;
        }

        m_pRecvActive->dev.iov_offset = 0;
    }

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
int
Endpoint::ProcessSrcAvailMsg(
    __in UINT8 iRecv,
    __out bool* pfRepost
    )
{
    int mpi_errno;
    const UINT8* pSrc = m_Buffers.Recv[iRecv].Data;
    *pfRepost = false;

    MPIU_Assert( m_cbRecv[iRecv] == sizeof(nd_src_avail_t) );
    MPIU_Assert( m_nReads < ND_READ_LIMIT );
    MPIU_Assert( m_ReadOffset == 0 );

    m_cbRecv[iRecv] -= sizeof(UINT64) + sizeof(UINT64) + sizeof(UINT32);

    if( m_pRecvActive == NULL )
    {
        MPIU_Assert( m_cbRecv[iRecv] >= sizeof(MPIDI_CH3_Pkt_t) );

        m_pVc->ch.n_recv++;
        TraceRecvNd_Packet(
            m_pVc->pg_rank,
            MPIDI_Process.my_pg_rank,
            m_pVc->ch.n_recv,
            reinterpret_cast<const MPIDI_CH3_Pkt_t*>(pSrc)->type
            );
        mpi_errno = MPIDI_CH3U_Handle_recv_pkt(
            m_pVc,
            reinterpret_cast<const MPIDI_CH3_Pkt_t*>(pSrc),
            &m_pRecvActive
            );

        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        MPIU_Assert( m_pRecvActive != NULL );

        m_pRecvActive->dev.iov_offset = 0;
        m_cbRecv[iRecv] -= sizeof(MPIDI_CH3_Pkt_t);
        pSrc += sizeof(MPIDI_CH3_Pkt_t);
    }

    MPIU_Assert( m_pRecvActive != NULL );

    //
    // Copy piggy-back data.
    //
    while( m_cbRecv[iRecv] )
    {
        MPIU_Bsize_t nCopy;
        nCopy = __CopyToIov(
            pSrc,
            static_cast<MPIU_Bsize_t>(m_cbRecv[iRecv]),
            m_pRecvActive->dev.iov,
            m_pRecvActive->dev.iov_count,
            &m_pRecvActive->dev.iov_offset
            );

        pSrc += nCopy;
        m_cbRecv[iRecv] -= nCopy;

        //TODO: Handle reloading the IOV...
        MPIU_Assert( m_pRecvActive->dev.iov_count != m_pRecvActive->dev.iov_offset );
    }

    return ReadToIov( m_Buffers.Recv[iRecv].SrcAvail, pfRepost );
}


int
Endpoint::RecvCompletion(
    _In_ const ND2_RESULT* pResult,
    _Out_ bool* pfMpiRequestDone
    )
{
    *pfMpiRequestDone = false;

    switch( pResult->Status )
    {
    case ND_SUCCESS:
        break;
    case ND_CANCELED:
        return MPI_SUCCESS;
    default:
        return MPIU_E_ERR(
            "**ch3|nd|recv_err %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            pResult->Status
            );
    }

    UINT8 iRecv = (m_iRecv + m_nRecvs) & m_RecvQueueMask;
    MPIU_Assert( pResult->RequestContext == &m_cbRecv[iRecv] );

    //
    // We don't perform runtime checks here because we own and control the
    // receive buffer.  If we don't receive a header, we will end up processing
    // garbage data, which we could have received anyway.
    //
    // There is no buffer overrun risk as the hardware prevents buffer overruns.
    //
    MPIU_Assert( pResult->BytesTransferred >= sizeof(nd_hdr_t) );
    m_cbRecv[iRecv] = pResult->BytesTransferred - sizeof(nd_hdr_t);

    //
    // Credits and RdCompl fields affect the send queue, so we process
    // them OOB from the payload.
    //
    int mpi_errno = ProcessFlowControlData( iRecv, pfMpiRequestDone );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    //
    // We must maintain ordering of receives for much of the logic of the endpoint
    // to work properly, as it depends on a circular buffer of receives that are
    // used in order.  For simplicity, we queue everything and then process the queue.
    //
    m_nRecvs++;

    //
    // Because iWARP requires that the active side of a connection be the first
    // to send any data in RDMA mode, we can't move the passive side to the
    // connected state until it has received its first message from the active side.
    // However the Accept overlapped request can race the receive completion.
    // To prevent trying to disconnect a short-lived connection before it has
    // been fully accepted, we delay the state transition until both the Accept
    // has completed AND we have received the first message.
    //
    switch( m_pVc->ch.state )
    {
    case MPIDI_CH3I_VC_STATE_UNCONNECTED:
        MPIU_Assert( m_pVc->ch.state != MPIDI_CH3I_VC_STATE_UNCONNECTED );
        return MPIU_ERR_FAIL( MPI_ERR_INTERN );

    case MPIDI_CH3I_VC_STATE_CONNECTING:
        m_pVc->ch.state = MPIDI_CH3I_VC_STATE_PENDING_ACCEPT;
        return MPI_SUCCESS;

    case MPIDI_CH3I_VC_STATE_PENDING_ACCEPT:
        return MPI_SUCCESS;

    case MPIDI_CH3I_VC_STATE_ACCEPTED:
        m_pVc->ch.state = MPIDI_CH3I_VC_STATE_CONNECTED;
        break;

    default:
        break;
    }

    return Progress( pfMpiRequestDone );
}


int
Endpoint::ProcessReadComplete(
    __in UINT8 nRdCompl,
    __out bool* pfMpiRequestDone
    )
{
    if( nRdCompl == 0 )
    {
        return MPI_SUCCESS;
    }

    m_nRdComplToRecv = m_nRdComplToRecv - nRdCompl;
    if( m_nRdComplToRecv > 0 )
    {
        return MPI_SUCCESS;
    }


    {
        SendQLock lock(m_pVc);

        MPID_Request* pReq = MPIDI_CH3I_SendQ_head_unsafe( m_pVc );
        MPIU_Assert( pReq );

        if( pReq->dev.iov_offset != pReq->dev.iov_count )
        {
            return MPI_SUCCESS;
        }

        //
        // Done sending this iov, indicate this.
        //
        int fComplete;
        int mpi_errno = MPIDI_CH3U_Handle_send_req( m_pVc, pReq, &fComplete );
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
            TraceSendNd_Continue(
                MPIDI_Process.my_pg_rank,
                m_pVc->pg_rank,
                pReq->ch.msg_id,
                pReq->dev.iov_count,
                pReq->iov_size()
                );
            pReq->dev.iov_offset = 0;
        }
        else
        {
            //
            // This request is done, remove it from the queue and go send the next request.
            //
            TraceSendNd_Done(
                MPIDI_Process.my_pg_rank,
                m_pVc->pg_rank,
                pReq->ch.msg_id
                );
            MPIDI_CH3I_SendQ_dequeue_unsafe( m_pVc );
            *pfMpiRequestDone = true;
        }

        return ProcessSendsUnsafe( pfMpiRequestDone );
    }
}


int
Endpoint::Read(
    _In_ const nd_src_avail_t& srcAvail,
    _In_reads_(nDst) char* pDst,
    _In_ MPIU_Bsize_t nDst,
    _Inout_ MPIU_Bsize_t* pcbRead
    )
{
    //
    // Cap the maximum read size to the smaller of the maximum Read size
    // supported by the adapter.
    //
    nDst = static_cast<MPIU_Bsize_t>( min(nDst, m_pCq->GetAdapter()->MaxReadLength() ) );

    //
    // Register our local buffers and perform the RDMA read.
    //
    StackGuardRef<Mr> pMr;
    int mpi_errno = m_pCq->GetAdapter()->CreateMrVadAware( pDst, &nDst, 0, &pMr.ref() );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    //
    // Issue the RDMA read.
    //
    ND2_SGE sge;
    sge.Buffer = pDst;
    sge.BufferLength = nDst;
    sge.MemoryRegionToken = pMr->IMr()->GetLocalToken();

    AddRef();
    HRESULT hr = m_pIEndpoint->Read(
        pMr.get(),
        &sge,
        1,
        srcAvail.Buffer + m_ReadOffset,
        srcAvail.Token,
        0
        );
    if( hr != ND_SUCCESS )
    {
        Release();
        return MPIU_E_ERR(
            "**ch3|nd|read %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            hr
            );
    }

    m_nReads++;
    m_iRead = ++m_iRead % ND_READ_LIMIT;

    //
    // We hold the reference on the MR until the read completes.
    //
    pMr.detach();

    *pcbRead = nDst;
    return MPI_SUCCESS;
}


int
Endpoint::ReadToIov(
    _In_ const nd_src_avail_t& srcAvail,
    _Out_ bool* pfRepost
    )
{
    *pfRepost = false;

    MPIU_Bsize_t cbSrc = static_cast<MPIU_Bsize_t>( srcAvail.Length - m_ReadOffset );
    MPIU_Assert( cbSrc > 0 );

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
    while( p < end && m_nReads < ND_READ_LIMIT )
    {
        int mpi_errno = Read( srcAvail, p->buf, min( p->len, cbSrc ), &cbRead );
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

        if( cbRead < cbSrc )
        {
            m_ReadOffset += cbRead;
            cbSrc -= static_cast<MPIU_Bsize_t>(cbRead);
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

    m_pRecvActive->dev.iov_offset = static_cast<int>(p - m_pRecvActive->dev.iov);
    return MPI_SUCCESS;
}


int
Endpoint::ReadCompletion(
    __in ND2_RESULT* pResult,
    __out bool* pfMpiRequestDone
    )
{
    MPIU_Assert( m_pRecvActive != NULL );
    MPIU_Assert( m_nReads > 0 );

    //
    // Release the MR immedately as the RegistrationCacheCallback might get called and
    // fail the assertion in Mr::Shutdown() that the MR is Idle().
    //
    static_cast<Mr*>( pResult->RequestContext )->Release();

    m_nReads--;

    if( FAILED( pResult->Status ) )
    {
        return MPIU_E_ERR(
            "**ch3|nd|read_err %s %d %x",
            inet_ntoa( m_DestAddr.sin_addr ),
            m_DestAddr.sin_port,
            pResult->Status
            );
    }

    if( m_pRecvActive->dev.iov_offset == m_pRecvActive->dev.iov_count )
    {
        //
        // If the receive request has all its buffers in flight, we need to wait until
        // all reads complete before reloading the IOV.  There's no reason to call
        // ProcessRecvs or ProcessSends until this happens.
        //
        if( m_nReads != 0 )
        {
            return MPI_SUCCESS;
        }

        TraceRecvNd_Data(
            m_pVc->pg_rank,
            MPIDI_Process.my_pg_rank,
            m_pVc->ch.n_recv
            );

        int fComplete;
        int mpi_errno = MPIDI_CH3U_Handle_recv_req(
            m_pVc,
            m_pRecvActive,
            &fComplete
            );

        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        if( fComplete )
        {
            m_pRecvActive = NULL;
            TraceRecvNd_Done(
                m_pVc->pg_rank,
                MPIDI_Process.my_pg_rank,
                m_pVc->ch.n_recv
                );
            *pfMpiRequestDone = true;
        }
        else
        {
            m_pRecvActive->dev.iov_offset = 0;
        }
    }

    return Progress( pfMpiRequestDone );
}


int
Endpoint::CompletionHandler(
    __in ND2_RESULT* pResult,
    __out bool* pfMpiRequestDone
    )
{
    StackGuardRef<Endpoint> pEp( static_cast<Endpoint*>( pResult->QueuePairContext ) );

    switch( pResult->RequestType )
    {
    case Nd2RequestTypeSend:
        return pEp->SendCompletion( pResult, pfMpiRequestDone );
    case Nd2RequestTypeReceive:
        return pEp->RecvCompletion( pResult, pfMpiRequestDone );
    case Nd2RequestTypeRead:
        return pEp->ReadCompletion( pResult, pfMpiRequestDone );
    default:
        __assume( 0 );
    }
}

}   // namespace CH3_ND
