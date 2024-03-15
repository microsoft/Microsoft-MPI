// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_adapter.cpp - Network Direct MPI CH3 Channel adapter functionality

--*/

#include "precomp.h"
#include "ch3u_nd1.h"


namespace CH3_ND
{
namespace v1
{

CAdapter::CAdapter() :
    m_nRef( 1 ),
    m_nMw( 0 ),
    m_fGCRPending( false )
{
    ZeroMemory( &m_Info, sizeof(m_Info) );
    ZeroMemory( &m_Addr, sizeof(m_Addr) );
    ExInitOverlapped( &m_ListenOv, GetConnSucceeded, GetConnFailed );
}


CAdapter::~CAdapter()
{
}


int
CAdapter::Init(
    __in sockaddr_in& Addr,
    __in ExSetHandle_t hExSet,
    __in SIZE_T ZCopyThreshold
    )
{
    HRESULT hr = NdOpenV1Adapter(
        (const struct sockaddr*)&Addr,
        sizeof(Addr),
        &m_pIAdapter.ref()
        );
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|open %x", hr );
    }

    SIZE_T Size = sizeof(m_Info);
    hr = m_pIAdapter->Query( 1, &m_Info, &Size );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|query %x", hr );

    //
    // Make sure we can use the adater for I/O.  No point in listening
    // if we can't create endpoints.
    //
    if( m_Info.MaxCqEntries < NDv1_CQ_ENTRIES_PER_EP )
    {
        return MPIU_E_ERR(
            "**ch3|nd|max_cq %d %d",
            m_Info.MaxCqEntries,
            NDv1_CQ_ENTRIES_PER_EP
            );
    }

    if( ZCopyThreshold != 0 )
    {
        m_Info.LargeRequestThreshold = ZCopyThreshold;
    }

    if( m_Info.LargeRequestThreshold < sizeof(nd_msg_t) )
    {
        //
        // We don't support zCopy mode for transfers smaller than a private buffer.
        //
        m_Info.LargeRequestThreshold = sizeof(nd_msg_t);
    }

    ExAttachHandle( hExSet, m_pIAdapter->GetFileHandle() );

    m_Addr = Addr;
    return MPI_SUCCESS;
}


MPI_RESULT
CAdapter::Create(
    _In_ sockaddr_in& Addr,
    _In_ ExSetHandle_t hExSet,
    _In_ SIZE_T ZCopyThreshold,
    _Outptr_ CAdapter** ppAdapter
    )
{
    StackGuardRef<CAdapter> pAdapter(new CAdapter() );
    if( pAdapter.get() == NULL )
        return MPIU_ERR_NOMEM();

    int mpi_errno = pAdapter->Init( Addr, hExSet, ZCopyThreshold );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    *ppAdapter = pAdapter.detach();
    return MPI_SUCCESS;
}


void
CAdapter::Shutdown()
{
    for( List<CCq>::iterator i = m_CqList.begin(); i != m_CqList.end(); NULL )
    {
        StackGuardRef<CCq> pCq( &*i );
        i = m_CqList.erase( i );
        pCq->Shutdown();
    }

    if( m_pIListen.get() != NULL )
    {
        if( m_fGCRPending == true )
        {
            OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
            m_pIListen->CancelOverlappedRequests();
            //
            // Since the progress engine is dead, we'll never get a completion
            // for the GetConnectionRequest.  Just release its reference and move on.
            //
            Release();
        }
        m_pIListen.free();
    }

    while( !m_MwPool.empty() )
    {
        StackGuardRef<CMw> pMw( &m_MwPool.front() );
        m_MwPool.pop_front();
    }

    if( m_pIAdapter.get() != NULL )
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
        m_pIAdapter->CancelOverlappedRequests();
    }
}


void
CAdapter::Release()
{
    MPIU_Assert( m_nRef > 0 );

    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}


int
CAdapter::Listen()
{
    HRESULT hr = m_pIAdapter->Listen(
        0,
        123,    // TODO: Pick a better protocol number.
        0,
        &m_Addr.sin_port,
        &m_pIListen.ref()
        );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|listen %x", hr );

    return GetConnectionRequest();
}


int CAdapter::GetConnectionRequest()
{
    if( m_pIListen.get() == NULL )
        return MPI_SUCCESS;

    HRESULT hr = m_pIAdapter->CreateConnector( &m_pIConnector.ref() );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|create_conn %x", hr );

    hr = m_pIListen->GetConnectionRequest( m_pIConnector.get(), &m_ListenOv.ov );
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|get_conn %x", hr );
    }

    m_fGCRPending = true;
    AddRef();
    return MPI_SUCCESS;
}


MPI_RESULT
CAdapter::GetAvailableCq(
    _Outptr_ CCq** ppCq
    )
{
    CCq* pCq;
    for( List<CCq>::iterator i = m_CqList.begin(); i != m_CqList.end(); ++i )
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
    int mpi_errno = CCq::Create( this, &pCq );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    m_CqList.push_back( *pCq );
    pCq->AddRef();

    *ppCq = pCq;
    return MPI_SUCCESS;
}


int
CAdapter::Poll(
    __out BOOL* pfProgress
    )
{
    *pfProgress = FALSE;

    // Poll for completions.
    for( List<CCq>::iterator i = m_CqList.begin(); i != m_CqList.end(); ++i )
    {
        int mpi_errno = i->Poll(pfProgress);
        if( mpi_errno != MPI_SUCCESS )
            return mpi_errno;
    }
    return MPI_SUCCESS;
}


int
CAdapter::Arm()
{
    // Arm for completions.
    for( List<CCq>::iterator i = m_CqList.begin(); i != m_CqList.end(); ++i )
    {
        int mpi_errno = i->Arm();
        if( mpi_errno != MPI_SUCCESS )
            return mpi_errno;
    }
    return MPI_SUCCESS;
}


int
CAdapter::CreateMr(
    __in const char* pBuf,
    __in SIZE_T Length,
    __out CMr** ppMr
    )
{
    return g_NdEnv.CreateMr( this, pBuf, Length, ppMr );
}


int
CAdapter::CreateMrVadAware(
    __in const char* pBuf,
    __inout MPIU_Bsize_t* pLength,
    __in MPIU_Bsize_t minLength,
    __out CMr** ppMr
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

_Success_(return == MPI_SUCCESS)
int
CAdapter::RegisterMemory(
    __in const char* pBuf,
    __in SIZE_T Length,
    __out ND_MR_HANDLE* phMr
    )
{
    OVERLAPPED Ov;
    Ov.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );
    if (Ov.hEvent == NULL)
    {
        return MPIU_ERR_NOMEM();
    }

    //
    // We set the lower bit of the event handle to prevent completion
    // from going to the IOCP.
    //
    Ov.hEvent = (HANDLE)((SIZE_T)Ov.hEvent | 0x1);
    Ov.Internal = ND_PENDING;
    HRESULT hr = m_pIAdapter->RegisterMemory( pBuf, Length, &Ov, phMr );

    if( hr == ND_PENDING )
    {
        SIZE_T BytesReturned;
        hr = m_pIAdapter->GetOverlappedResult( &Ov, &BytesReturned, TRUE );
    }

    CloseHandle( Ov.hEvent );

    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|regmem %x", hr );

    return MPI_SUCCESS;
}


int
CAdapter::DeregisterMemory(
    __in ND_MR_HANDLE hMr
    )
{
    MPIU_Assert( hMr );

    OVERLAPPED Ov;
    Ov.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );
    if( Ov.hEvent == NULL )
        return MPIU_ERR_NOMEM();

    //
    // We set the lower bit of the event handle to prevent completion
    // from going to the IOCP.
    //
    Ov.hEvent = (HANDLE)((SIZE_T)Ov.hEvent | 0x1);
    Ov.Internal = ND_PENDING;
    HRESULT hr = m_pIAdapter->DeregisterMemory( hMr, &Ov );

    if( hr == ND_PENDING )
    {
        SIZE_T BytesReturned;
        hr = m_pIAdapter->GetOverlappedResult(
            &Ov,
            &BytesReturned,
            TRUE );
    }

    CloseHandle( Ov.hEvent );

    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|deregmem %x", hr );

    return MPI_SUCCESS;
}


int
CAdapter::AllocMw(
    __out CMw** ppMw
    )
{
    if( m_MwPool.empty() )
        return CMw::Create( this, ppMw );

    *ppMw = &m_MwPool.front();
    m_MwPool.pop_front();
    m_nMw--;
    return MPI_SUCCESS;
}


void
CAdapter::FreeMw(
    __in CMw* pMw
    )
{
    if( m_nMw > x_MwPoolLimit )
    {
        pMw->Release();
        return;
    }

    m_MwPool.push_front( *pMw );
    m_nMw++;
}


inline
int
CAdapter::Accept(
    __in MPIDI_VC_t* pVc,
    __in INDConnector* pIConnector,
    __in SIZE_T InboundReadLimit,
    __in SIZE_T OutboundReadLimit,
    __in const nd_caller_data_t& CallerData
    )
{
    struct sockaddr_in DestAddr;
    SIZE_T Len = sizeof(DestAddr);
    HRESULT hr = pIConnector->GetPeerAddress(
        (struct sockaddr*)&DestAddr,
        &Len
        );
    if( FAILED( hr ) )
    {
        if( hr != ND_CONNECTION_ABORTED && hr != ND_CONNECTION_INVALID )
            return MPIU_E_ERR( "**ch3|nd|peer_addr %x", hr );

        //
        // Possibly a timeout on the active side aborted the connection.  The active side
        // will retry, so this is not a fatal error.
        //
        // Reject the connection so we don't leak resources.
        //
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
        pIConnector->Reject( NULL, 0 );
        return MPI_SUCCESS;
    }

    StackGuardRef<CEndpoint> pEp;
    int mpi_errno = CEndpoint::Create(
        this,
        pIConnector,
        DestAddr,
        pVc,
        min( InboundReadLimit, IRL() ),
        min( OutboundReadLimit, ORL() ),
        &pEp.ref()
        );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    mpi_errno = pEp->Accept( CallerData.Credits );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    return MPI_SUCCESS;
}


int
WINAPI
CAdapter::GetConnSucceeded(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CAdapter> pAdapter(
        CONTAINING_RECORD( pOverlapped, CAdapter, m_ListenOv.ov )
        );

    pAdapter->m_fGCRPending = false;
    return pAdapter->GetConnSucceeded();
}


int CAdapter::GetConnSucceeded()
{
    StackGuardRef<INDConnector> pIConnector( m_pIConnector.detach() );
    int mpi_errno;

    //
    // Get the private data.
    //
    SIZE_T InboundReadLimit;
    SIZE_T OutboundReadLimit;
    nd_caller_data_t CallerData;
    SIZE_T Len = sizeof(CallerData);
    HRESULT hr = pIConnector->GetConnectionData(
        &InboundReadLimit,
        &OutboundReadLimit,
        &CallerData,
        &Len
        );

    if( (FAILED( hr ) && hr != ND_BUFFER_OVERFLOW) || Len < sizeof(CallerData) )
    {
        //
        // Possibly a timeout on the active side aborted the connection.  The active side
        // will retry, so this is not a fatal error.  Note that Reject will get the next
        // connection request.
        //
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
        pIConnector->Reject( NULL, 0 );

        //
        // Possibly a timeout on the active side aborted the connection.  The active side
        // will retry, so this is not a fatal error.  Get the next connection request.
        //
        if( hr != ND_CONNECTION_ABORTED && hr != ND_CONNECTION_INVALID )
        {
            return MPIU_E_ERR( "**ch3|nd|conn_data %x", hr );
        }

        goto next;
    }

    if( CallerData.Version != MSMPI_VER_EX )
    {
#pragma prefast(disable:25031, "Don't care about the status of reject - no possible recovery.");
        pIConnector->Reject( NULL, 0 );
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**version %d %d %d %d %d %d",
            MSMPI_VER_MAJOR( CallerData.Version ),
            MSMPI_VER_MINOR( CallerData.Version ),
            MSMPI_VER_BUILD( CallerData.Version ),
            MSMPI_VER_MAJOR( MSMPI_VER_EX ),
            MSMPI_VER_MINOR( MSMPI_VER_EX ),
            MSMPI_VER_BUILD( MSMPI_VER_EX )
            );
    }

    //
    // Validate the program ID.
    //
    MPIDI_PG_t* pg = MPIDI_PG_Find( CallerData.GroupId );
    if( pg == NULL )
    {
        pIConnector->Reject( NULL, 0 );
        return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**pglookup %g", CallerData.GroupId );
    }

    // TODO: Should we validate the source address with the business card?

    //
    // Get the VC
    //
    int pg_rank = CallerData.Rank;

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
        return MPIU_E_ERR( "**rank %d %g %d", pg_rank, pg->id, pg->size );
    }

    switch( pVc->ch.state )
    {
    case MPIDI_CH3I_VC_STATE_CONNECTING:
        MPIU_Assert( pVc->ch.nd.pEpV1 != NULL );

        //
        // If the other program group's ID is less than ours, or we're
        // in the same group but the other side's rank is less than ours,
        // we reject.  They'll accept.
        //
        RPC_STATUS status;
        if( (pg == MPIDI_Process.my_pg && Mpi.CommWorld->rank > pg_rank) ||
            UuidCompare( &MPIDI_Process.my_pg->id, &pg->id, &status ) > 0 )
        {
            pIConnector->Reject( NULL, 0 );

            //
            // Not an error, the other side will accept.
            //
            break;
        }

        {
            CEndpoint* pEp = pVc->ch.nd.pEpV1;

            //
            // The other side should reject the connection.  However, due to timings it's possible
            // that it will see our connection request after it has connected, used, and disconnected,
            // and will treat it as a new connection.  Proactively shutdown the EP to abort the
            // connection request to avoid this.
            //
            pEp->Abandon( MPIDI_CH3I_VC_STATE_UNCONNECTED );
        }
        __fallthrough;

    case MPIDI_CH3I_VC_STATE_UNCONNECTED:
        if( pVc->state != MPIDI_VC_STATE_ACTIVE )
        {
            //
            // Odd case of getting a connection request when we're shutting down.
            //
            pIConnector->Reject( NULL, 0 );
            return MPIU_E_ERR( "**vc_state %d", pVc->state );
        }

        MPIU_Assert( pVc->ch.nd.pEpV1 == NULL );
        mpi_errno = Accept(
            pVc,
            pIConnector.get(),
            InboundReadLimit,
            OutboundReadLimit,
            CallerData
            );
        if( mpi_errno != MPI_SUCCESS )
        {
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
        pIConnector->Reject( NULL, 0 );
        break;
    }

next:
    //
    // Get the next connection request.
    //
    mpi_errno = GetConnectionRequest();
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    return MPI_SUCCESS;
}


int
WINAPI
CAdapter::GetConnFailed(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CAdapter> pAdapter(
        CONTAINING_RECORD( pOverlapped, CAdapter, m_ListenOv.ov )
        );
    StackGuardRef<INDConnector> pIConnector( pAdapter->m_pIConnector.detach() );

    pAdapter->m_fGCRPending = false;

    SIZE_T nBytesRet;
    HRESULT hr = pAdapter->m_pIListen->GetOverlappedResult( &pOverlapped->ov, &nBytesRet, FALSE );
    switch( hr )
    {
    case ND_CANCELED:
        return MPI_SUCCESS;

    default:
        return MPIU_E_ERR( "**ch3|nd|get_conn %x", hr );
    }
}

}   // namespace v1
}   // namespace CH3_ND
