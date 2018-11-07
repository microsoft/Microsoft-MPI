// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_adapter.cpp - Network Direct MPI CH3 Channel adapter functionality

--*/

#include "precomp.h"
#include "ch3u_nd2.h"


namespace CH3_ND
{

Adapter::Adapter( __in ExSetHandle_t hExSet ) :
    m_hExSet( hExSet ),
    m_nRef( 1 ),
    m_hOverlappedFile( INVALID_HANDLE_VALUE ),
    m_fGCRPending( false ),
    m_ListenOv( GetConnReqHandler )
{
    m_link.Flink = m_link.Blink = &m_link;
    ZeroMemory( &m_Info, sizeof(m_Info) );
    ZeroMemory( &m_Addr, sizeof(m_Addr) );
}


Adapter::~Adapter()
{
    if( m_hOverlappedFile != INVALID_HANDLE_VALUE )
    {
        CloseHandle( m_hOverlappedFile );
    }
}


int
Adapter::Init(
    __in sockaddr_in& addr,
    __in ExSetHandle_t hExSet,
    __in ULONG cbZCopyThreshold
    )
{
    HRESULT hr = NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr*>(&addr),
        sizeof(addr),
        reinterpret_cast<void**>( &m_pIAdapter.ref() )
        );
    if( FAILED( hr ) )
    {
        Trace_ND_Error_AdapterInit(this, AdapterInitOpen, hr, &addr, hExSet, cbZCopyThreshold);
        return MPIU_E_ERR( "**ch3|nd|open %x", hr );
    }

    m_Info.InfoVersion = ND_VERSION_2;
    ULONG cbInfo = sizeof(m_Info);
    hr = m_pIAdapter->Query( &m_Info, &cbInfo );
    if( FAILED( hr ) )
    {
        Trace_ND_Error_AdapterInit(this, AdapterInitQuery, hr, &addr, hExSet, cbZCopyThreshold);
        return MPIU_E_ERR( "**ch3|nd|query %x", hr );
    }

    //
    // Make sure we can use the adater for I/O.  No point in listening
    // if we can't create endpoints.
    //
    if( m_Info.MaxCompletionQueueDepth < g_NdEnv.CqDepthPerEp() )
    {
        Trace_ND_Error_AdapterInit(this, AdapterInitCQDepth, hr, &addr, hExSet, cbZCopyThreshold);
        return MPIU_E_ERR(
            "**ch3|nd|max_cq %d %d",
            m_Info.MaxCompletionQueueDepth,
            g_NdEnv.CqDepthPerEp()
            );
    }

    if( m_Info.MaxInitiatorQueueDepth < g_NdEnv.InitiatorQueueDepth() )
    {
        Trace_ND_Error_AdapterInit(this, AdapterInitInitiatorQDepth, hr, &addr, hExSet, cbZCopyThreshold);
        return MPIU_E_ERR(
            "**ch3|nd|max_sq %d %d",
            m_Info.MaxInitiatorQueueDepth,
            g_NdEnv.InitiatorQueueDepth()
            );
    }

    if( m_Info.MaxReceiveQueueDepth < g_NdEnv.RecvQueueDepth() )
    {
        Trace_ND_Error_AdapterInit(this, AdapterInitRecvQDepth, hr, &addr, hExSet, cbZCopyThreshold);
        return MPIU_E_ERR(
            "**ch3|nd|max_rq %d %d",
            m_Info.MaxReceiveQueueDepth,
            g_NdEnv.RecvQueueDepth()
            );
    }

    hr = m_pIAdapter->CreateOverlappedFile( &m_hOverlappedFile );
    if( FAILED( hr ) )
    {
        Trace_ND_Error_AdapterInit(this, AdapterInitCreateOverlapped, hr, &addr, hExSet, cbZCopyThreshold);
        return MPIU_E_ERR( "**ch3|nd|ov_file %x", hr );
    }

    if( cbZCopyThreshold != 0 )
    {
        m_Info.LargeRequestThreshold = cbZCopyThreshold;
    }

    if( m_Info.LargeRequestThreshold < sizeof(nd_msg_t) )
    {
        //
        // We don't support zCopy mode for transfers smaller than a private buffer.
        //
        m_Info.LargeRequestThreshold = sizeof(nd_msg_t);
    }

    ExAttachHandle( m_hExSet, m_hOverlappedFile, EX_KEY_ND );

    m_Addr = addr;
    return MPI_SUCCESS;
}


MPI_RESULT
Adapter::Create(
    _In_ sockaddr_in& addr,
    _In_ ExSetHandle_t hExSet,
    _In_ ULONG cbZCopyThreshold,
    _Outptr_ Adapter** ppAdapter
    )
{
    StackGuardRef<Adapter> pAdapter( new Adapter( hExSet ) );
    if( pAdapter.get() == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    int mpi_errno = pAdapter->Init( addr, hExSet, cbZCopyThreshold );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    *ppAdapter = pAdapter.detach();
    return MPI_SUCCESS;
}


ULONG Adapter::MaxEpPerCq() const
{
    return m_Info.MaxCompletionQueueDepth / g_NdEnv.CqDepthPerEp();
}


void
Adapter::Shutdown()
{
    for( List<Cq>::iterator i = m_CqList.begin(); i != m_CqList.end(); NULL )
    {
        StackGuardRef<Cq> pCq( &*i );
        i = m_CqList.erase( i );
        pCq->Shutdown();
    }

    if( m_pIListener.get() != NULL )
    {
        if( m_fGCRPending == true )
        {
#pragma prefast(disable:25031, "Don't care about the status of cancel - no possible recovery.");
            m_pIListener->CancelOverlappedRequests();
            //
            // Since the progress engine is dead, we'll never get a completion
            // for the GetConnectionRequest.  Just release its reference and move on.
            //
            Release();
        }
        m_pIListener.free();
    }
    Trace_ND_Info_AdapterShutdown(this);
}


void
Adapter::Release()
{
    MPIU_Assert( m_nRef > 0 );

    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}

MPI_RESULT
Adapter::Listen(
    _In_opt_ int     minPort,
    _In_opt_ int     maxPort
    )
{
    HRESULT hr;
    int mpi_errno = MPI_SUCCESS;

    //
    // minPort and maxPort must be > 0
    // minPort must be < maxPort
    // maxPort must be <= MAX_PORT_VALUE
    //  if that is not all true, we force both to 0
    //  and will use ephemeral ports
    //
    if (!(minPort > 0 && maxPort > 0 &&
        minPort <= maxPort && maxPort <= 0xFFFF))
    {
        minPort = 0;
        maxPort = 0;
    }

    //
    // Because the min/max range is inclusive, the loop
    //  works once if the values are 0, and will loop
    //  through the list of ports finding the next one
    //  it can bind with.
    //
    // All errors during this loop will move to the next
    //  port to attempt.  Ehpemeral ports (values==0) don't
    //  have more ports, so the first error is the final error.
    //  for ranged ports, the last error on the last port
    //  will be the one that is reported.
    //
    for (int port = minPort; port <= maxPort; port++)
    {
        mpi_errno = MPI_SUCCESS;

        if (m_pIListener.get() != NULL)
        {
            m_pIListener.free();
        }

        hr = m_pIAdapter->CreateListener(
            IID_IND2Listener,
            m_hOverlappedFile,
            reinterpret_cast<void**>( &m_pIListener.ref() )
            );
        if( FAILED( hr ) )
        {
            Trace_ND_Error_AdapterListen(this, AdapterListenCreateListener, hr);
            mpi_errno = MPIU_E_ERR( "**ch3|nd|create_listen %x", hr );
            continue;
        }

        m_Addr.sin_port = htons(static_cast<USHORT>(port));
        hr = m_pIListener->Bind(
            reinterpret_cast<const struct sockaddr*>(&m_Addr),
            sizeof(m_Addr)
            );
        if (FAILED(hr))
        {
            Trace_ND_Error_AdapterListen(this, AdapterListenBind, hr);
            mpi_errno = MPIU_E_ERR( "**ch3|nd|bindlisten %x", hr );
            continue;
        }


        ULONG cbAddr = sizeof(m_Addr);
        hr = m_pIListener->GetLocalAddress(
            reinterpret_cast<struct sockaddr*>(&m_Addr),
            &cbAddr
            );
        if( FAILED( hr ) )
        {
            Trace_ND_Error_AdapterListen(this, AdapterListenGetLocalAddress, hr);
            mpi_errno = MPIU_E_ERR( "**ch3|nd|listen_addr %x", hr );
            continue;
        }

        hr = m_pIListener->Listen( 0 );
        if( FAILED( hr ) )
        {
            Trace_ND_Error_AdapterListen(this, AdapterListenListen, hr);
            mpi_errno = MPIU_E_ERR( "**ch3|nd|listen %x", hr );
            continue;
        }
        break;
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    return GetConnectionRequest();
}


int Adapter::GetConnectionRequest()
{
    if( m_pIListener.get() == NULL )
    {
        return MPI_SUCCESS;
    }

    HRESULT hr = m_pIAdapter->CreateConnector(
        IID_IND2Connector,
        m_hOverlappedFile,
        reinterpret_cast<void**>( &m_pIConnector.ref() )
        );
    if( FAILED( hr ) )
    {
        Trace_ND_Error_AdapterGetConnectionRequest(this, GetConnectionRequestCreateConnector, hr);
        return MPIU_E_ERR( "**ch3|nd|create_conn %x", hr );
    }

    hr = m_pIListener->GetConnectionRequest( m_pIConnector.get(), &m_ListenOv );
    if( FAILED( hr ) )
    {
        Trace_ND_Error_AdapterGetConnectionRequest(this, GetConnectionRequestGetConnectionRequest, hr);
        return MPIU_E_ERR( "**ch3|nd|get_conn %x", hr );
    }

    m_fGCRPending = true;
    AddRef();
    return MPI_SUCCESS;
}


MPI_RESULT
Adapter::GetAvailableCq(
    _Outptr_ Cq** ppCq
    )
{
    Cq* pCq;
    for( List<Cq>::iterator i = m_CqList.begin(); i != m_CqList.end(); ++i )
    {
        if( !i->Full() )
        {
            i->AddRef();
            *ppCq = &*i;
            return MPI_SUCCESS;
        }
    }

    //
    // No existing CQ could accomodate us, so create a new one.
    //
    int mpi_errno = Cq::Create( *this, &pCq );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    m_CqList.push_back( *pCq );
    pCq->AddRef();

    *ppCq = pCq;
    return MPI_SUCCESS;
}


int
Adapter::Poll(
    _Out_ BOOL* pfProgress
    )
{
    *pfProgress = FALSE;

    // Poll for completions.
    for( List<Cq>::iterator i = m_CqList.begin(); i != m_CqList.end(); ++i )
    {
        int mpi_errno = i->Poll(pfProgress);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    return MPI_SUCCESS;
}


int
Adapter::Arm()
{
    // Arm for completions.
    for( List<Cq>::iterator i = m_CqList.begin(); i != m_CqList.end(); ++i )
    {
        int mpi_errno = i->Arm();
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }
    return MPI_SUCCESS;
}


int
Adapter::Connect(
    __in const struct sockaddr_in& destAddr,
    __inout MPIDI_VC_t* pVc
    )
{
    StackGuardRef<IND2Connector> pIConnector;
    int mpi_errno = CreateConnector( &pIConnector.ref() );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    MPIU_Assert( pVc->ch.nd.pEp == NULL );
    StackGuardRef<Endpoint> pEp;
    mpi_errno = Endpoint::Create(
        *this,
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
    Trace_ND_Info_AdapterConnect(this, inet_ntoa(destAddr.sin_addr), ntohs(destAddr.sin_port), &destAddr, pVc);
    return MPI_SUCCESS;
}


int
Adapter::CreateConnector(
    __deref_out IND2Connector** ppConnector
    )
{
    HRESULT hr = m_pIAdapter->CreateConnector(
        IID_IND2Connector,
        m_hOverlappedFile,
        reinterpret_cast<void**>( ppConnector )
        );
    if( FAILED( hr ) )
    {
        Trace_ND_Error_AdapterCreateConnector(this, AdapterCreateConectorCreateConnector, hr);
        return MPIU_E_ERR( "**ch3|nd|create_conn %x", hr );
    }

    hr = (*ppConnector)->Bind(
        reinterpret_cast<const struct sockaddr*>( &m_Addr ),
        sizeof(m_Addr)
        );
    if( FAILED( hr ) )
    {
        Trace_ND_Error_AdapterCreateConnector(this, AdapterCreateConectorBind, hr);
        return MPIU_E_ERR( "**ch3|nd|bindconn %x", hr );
    }
    return MPI_SUCCESS;
}


int
Adapter::CreateMr(
    __in const char* pBuf,
    __in SIZE_T cbBuf,
    __out Mr** ppMr
    )
{
    return g_NdEnv.CreateMr( *this, pBuf, cbBuf, ppMr );
}


int
Adapter::CreateMrVadAware(
    __in const char* pBuf,
    __inout MPIU_Bsize_t* pLength,
    __in MPIU_Bsize_t minLength,
    __out Mr** ppMr
    )
{
    int mpi_errno = CreateMr( pBuf, *pLength, ppMr );
    if( mpi_errno != MPI_SUCCESS )
    {
        //
        // See if we are crossing VAD boundaries.
        //
        MEMORY_BASIC_INFORMATION info;
        SIZE_T ret = VirtualQuery( pBuf, &info, sizeof(info) );
        if( ret != sizeof(info) )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        //
        // Cap the size to the VAD boundary.
        //
        MPIU_Assert( info.BaseAddress <= pBuf );
        *pLength = (MPIU_Bsize_t)(info.RegionSize -
            ((ULONG_PTR)pBuf - (ULONG_PTR)info.BaseAddress));
        if( *pLength < minLength )
        {
            return MPI_SUCCESS;
        }

        mpi_errno = CreateMr( pBuf, *pLength, ppMr );
    }
    return mpi_errno;
}


MPI_RESULT
Adapter::RegisterMemory(
    _In_ const char* pBuf,
    _In_ SIZE_T cbBuf,
    _In_ DWORD flags,
    _Outptr_ IND2MemoryRegion** ppMr
    )
{
    IND2MemoryRegion* pMr;
    HRESULT hr = m_pIAdapter->CreateMemoryRegion(
        IID_IND2MemoryRegion,
        m_hOverlappedFile,
        reinterpret_cast<void**>( &pMr )
        );
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|create_mr %x", hr );
    }

    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );
    if( ov.hEvent == NULL )
    {
        pMr->Release();
        return MPIU_ERR_NOMEM();
    }

    //
    // We set the lower bit of the event handle to prevent completion
    // from going to the IOCP.
    //
    ov.hEvent = reinterpret_cast<HANDLE>(
        reinterpret_cast<SIZE_T>(ov.hEvent) | 0x1
        );
    hr = pMr->Register(
        pBuf,
        cbBuf,
        flags,
        &ov
        );

    if( hr == ND_PENDING )
    {
        hr = pMr->GetOverlappedResult( &ov, TRUE );
    }

    CloseHandle( ov.hEvent );

    if( FAILED( hr ) )
    {
        pMr->Release();
        return MPIU_E_ERR( "**ch3|nd|regmem %x", hr );
    }

    *ppMr = pMr;
    return MPI_SUCCESS;
}


int
Adapter::DeregisterMemory(
    __in IND2MemoryRegion* pMr
    )
{
    MPIU_Assert( pMr );

    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );
    if( ov.hEvent == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // We set the lower bit of the event handle to prevent completion
    // from going to the IOCP.
    //
    ov.hEvent = reinterpret_cast<HANDLE>(
        reinterpret_cast<SIZE_T>(ov.hEvent) | 0x1
        );
    HRESULT hr = pMr->Deregister( &ov );

    if( hr == ND_PENDING )
    {
        hr = pMr->GetOverlappedResult( &ov, TRUE );
    }

    CloseHandle( ov.hEvent );

    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|deregmem %x", hr );
    }

    pMr->Release();
    return MPI_SUCCESS;
}


inline
int
Adapter::Accept(
    __in MPIDI_VC_t* pVc,
    __in IND2Connector* pIConnector,
    __in const nd_caller_data_t& callerData
    )
{
    struct sockaddr_in destAddr;
    ULONG cbDestAddr = sizeof(destAddr);
    HRESULT hr = pIConnector->GetPeerAddress(
        reinterpret_cast<struct sockaddr*>(&destAddr),
        &cbDestAddr
        );
    if( FAILED( hr ) )
    {
        if( hr != ND_CONNECTION_ABORTED && hr != ND_CONNECTION_INVALID )
        {
            Trace_ND_Error_AdapterAccept_GetPeerAddress(this, hr, pVc, pIConnector, &callerData);
            return MPIU_E_ERR( "**ch3|nd|peer_addr %x", hr );
        }

        //
        // Possibly a timeout on the active side aborted the connection.  The active side
        // will retry, so this is not a fatal error.
        //
        // Reject the connection so we don't leak resources.
        //
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Info_AdapterAccept_Reject(this, hr, pVc, pIConnector, &callerData);
        return MPI_SUCCESS;
    }

    StackGuardRef<Endpoint> pEp;
    int mpi_errno = Endpoint::Create(
        *this,
        pIConnector,
        destAddr,
        pVc,
        &pEp.ref()
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    mpi_errno = pEp->Accept( callerData.Credits );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }
    Trace_ND_Info_AdapterAccept_Success(this, inet_ntoa(destAddr.sin_addr), ntohs(destAddr.sin_port), pVc, pIConnector, &callerData);

    return MPI_SUCCESS;
}

OACR_WARNING_DISABLE(28301, "PREfast does not handle FN types correctly.");
int
Adapter::GetConnReqHandler(
    __in ND_OVERLAPPED* pOverlapped
    )
{
    StackGuardRef<Adapter> pAdapter(
        CONTAINING_RECORD( pOverlapped, Adapter, m_ListenOv )
        );
    StackGuardRef<IND2Connector> pIConnector( pAdapter->m_pIConnector.detach() );

    pAdapter->m_fGCRPending = false;

    HRESULT hr = pAdapter->m_pIListener->GetOverlappedResult(
        pOverlapped,
        FALSE
        );
    switch( hr )
    {
    case ND_SUCCESS:
        break;

    case ND_CANCELED:
        return MPI_SUCCESS;

    default:
        Trace_ND_Error_AdapterGetConnReqHandler(pAdapter.operator->(), hr, pOverlapped);
        return MPIU_E_ERR( "**ch3|nd|get_conn %x", hr );
    }

    int mpi_errno = pAdapter->GetConnSucceeded( pIConnector.get() );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    //
    // Get the next connection request.
    //
    mpi_errno = pAdapter->GetConnectionRequest();
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    return MPI_SUCCESS;
}
OACR_WARNING_ENABLE(28301, "PREfast does not handle FN types correctly.");


int Adapter::GetConnSucceeded( _In_ IND2Connector* pIConnector )
{
    int mpi_errno;

    //
    // Get the private data.
    //
    nd_caller_data_t callerData;
    DWORD cbCallerData = sizeof(callerData);
    HRESULT hr = pIConnector->GetPrivateData(
        &callerData,
        &cbCallerData
        );

    switch( hr )
    {
    case ND_BUFFER_OVERFLOW:
        break;

    case ND_SUCCESS:
        if( cbCallerData >= sizeof(callerData) )
        {
            break;
        }
        __fallthrough;

    case ND_INVALID_BUFFER_SIZE:
        MPIU_Assert( cbCallerData < sizeof(callerData) );
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Error_AdapterGetConnSucceeded(this, AdapterGetConnSucceededInvalidBufferSize, hr, pIConnector);
        return MPIU_E_ERR( "**ch3|nd|conn_data_len %d", cbCallerData );

    case ND_CONNECTION_ABORTED:
    case ND_CONNECTION_INVALID:
        //
        // Possibly a timeout on the active side aborted the connection.  The active side
        // will retry, so this is not a fatal error.
        //
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Info_AdapterGetConnSucceeded(this, AdapterGetConnSucceededAbortOrInvalid, hr, pIConnector);
        return MPI_SUCCESS;

    default:
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Error_AdapterGetConnSucceeded(this, AdapterGetConnSucceededReject, hr, pIConnector);
        return MPIU_E_ERR( "**ch3|nd|conn_data %x", hr );
    }

    if( callerData.Version != MSMPI_VER_EX )
    {
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Error_AdapterGetConnSucceeded(this, AdapterGetConnSucceededMismatchedVersion, hr, pIConnector);
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**version %d %d %d %d %d %d",
            MSMPI_VER_MAJOR( callerData.Version ),
            MSMPI_VER_MINOR( callerData.Version ),
            MSMPI_VER_BUILD( callerData.Version ),
            MSMPI_VER_MAJOR( MSMPI_VER_EX ),
            MSMPI_VER_MINOR( MSMPI_VER_EX ),
            MSMPI_VER_BUILD( MSMPI_VER_EX )
            );
    }

    //
    // Validate the program ID.
    //
    MPIDI_PG_t* pg = MPIDI_PG_Find( callerData.GroupId );
    if( pg == NULL )
    {
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Error_AdapterGetConnSucceeded(this, AdapterGetConnSucceededPGFind, hr, pIConnector);
        return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**pglookup %g", callerData.GroupId );
    }

    // TODO: Should we validate the source address with the business card?

    //
    // Get the VC
    //
    int pg_rank = callerData.Rank;

    MPIDI_VC_t* pVc;
    if( (pg_rank >= 0) && (pg_rank < pg->size) )
    {
        pVc = MPIDI_PG_Get_vc( pg, pg_rank );
        MPIU_Assert( pVc->pg_rank == pg_rank );
    }
    else
    {
        //
        // bad rank, reject connection.
        //
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Error_AdapterGetConnSucceeded(this, AdapterGetConnSucceededRank, hr, pIConnector);
        return MPIU_E_ERR( "**rank %d %g %d", pg_rank, pg->id, pg->size );
    }

    switch( pVc->ch.state )
    {
    case MPIDI_CH3I_VC_STATE_CONNECTING:
        MPIU_Assert( pVc->ch.nd.pEp != NULL );

        //
        // If the other program group's ID is less than ours, or we're
        // in the same group but the other side's rank is less than ours,
        // we reject.  They'll accept.
        //
        RPC_STATUS status;
        if( (pg == MPIDI_Process.my_pg && Mpi.CommWorld->rank > pg_rank) ||
            UuidCompare( &MPIDI_Process.my_pg->id, &pg->id, &status ) > 0 )
        {
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
            pIConnector->Reject( NULL, 0 );
            Trace_ND_Info_AdapterGetConnSucceeded(this, AdapterGetConnSucceededHeadToHeadReject, hr, pIConnector);
            //
            // Not an error, the other side will accept.
            //
            break;
        }

        {
            Endpoint* pEp = pVc->ch.nd.pEp;

            //
            // The other side should reject the connection.  However, due to timings it's possible
            // that it will see our connection request after it has connected, used, and disconnected,
            // and will treat it as a new connection.  Proactively shutdown the EP to abort the
            // connection request to avoid this.
            //
            Trace_ND_Info_AdapterGetConnSucceeded(this, AdapterGetConnSucceededHeadToHeadShutdown, hr, pIConnector);
            pEp->Abandon( MPIDI_CH3I_VC_STATE_UNCONNECTED );
        }
        __fallthrough;

    case MPIDI_CH3I_VC_STATE_UNCONNECTED:
        if( pVc->state != MPIDI_VC_STATE_ACTIVE )
        {
            //
            // Odd case of getting a connection request when we're shutting down.
            //
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
            pIConnector->Reject( NULL, 0 );
            Trace_ND_Error_AdapterGetConnSucceeded(this, AdapterGetConnSucceededAdapterShutdown, hr, pIConnector);
            return MPIU_E_ERR( "**vc_state %d", pVc->state );
        }

        MPIU_Assert( pVc->ch.nd.pEp == NULL );
        mpi_errno = Accept(
            pVc,
            pIConnector,
            callerData
            );
        if( mpi_errno != MPI_SUCCESS )
        {
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
            pIConnector->Reject( NULL, 0 );
            return MPIU_ERR_FAIL( mpi_errno );
        }
        break;

    default:
        //
        // Already connected or failed.  Reject the connection.
        // This handles the case where a connection request was delayed
        // potentially due to connection management packet loss.
        //
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        Trace_ND_Info_AdapterGetConnSucceeded(this, AdapterGetConnSucceededDefaultReject, hr, pIConnector);
        break;
    }

    Trace_ND_Info_AdapterGetConnSucceeded(this, AdapterGetConnSucceededSuccess, hr, pIConnector);
    return MPI_SUCCESS;
}

}   // namespace CH3_ND
