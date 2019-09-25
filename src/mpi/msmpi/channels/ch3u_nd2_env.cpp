// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_env.cpp - Network Direct MPI CH3 ND Environment

--*/

#include "precomp.h"
#include "ch3u_nd2.h"


#define MPIDI_CH3I_ND_DESCRIPTION_KEY "nd_host"


typedef
BOOLEAN
(CALLBACK *PSECURE_MEMORY_CACHE_CALLBACK)(
    _In_reads_bytes_(Range) VOID* Addr,
    _In_ SIZE_T Range
    );

typedef
BOOL
(WINAPI *PADD_CACHE_CALLBACK)(
    __in PSECURE_MEMORY_CACHE_CALLBACK pfnCallback
    );

typedef
BOOL
(WINAPI *PREMOVE_CACHE_CALLBACK)(
    __in PSECURE_MEMORY_CACHE_CALLBACK pfnCallback
    );


namespace CH3_ND
{

Environment::Environment() :
    m_hExSet( NULL ),
    m_fEnabled( false ),
    m_fEnableFallback( false ),
    m_ZCopyThreshold( 0 ),
    m_MrCacheLimit( 0 ),
    m_EagerLimit( 0 ),
    m_MrCacheSize( 0 ),
    m_maxPort(0),
    m_minPort(0)
{
    m_NetAddr.s_addr = 0;
    m_NetMask.s_addr = 0;
}


static int RoundUpPow2( _In_range_( >, 0 ) int val, _In_range_( >, 0 ) int max )
{
    MPIU_Assert( val != 0 );

    val = PowerOf2Ceiling( val );
    if( val > max )
    {
        return max;
    }

    return val;
}


void
Environment::InitializePortRange()
{
    if( env_to_range(
            L"MSMPI_ND_PORT_RANGE",
            0,
            65535,
            FALSE,
            &m_minPort,
            &m_maxPort ) == FALSE )
    {
        m_minPort = m_maxPort = 0;
    }
}


int
Environment::Init(
    _In_ ExSetHandle_t hExSet,
    _In_ ULONG cbZCopyThreshold,
    _In_ MPIDI_msg_sz_t cbEagerLimit,
    _In_ UINT64 cbMrCacheLimit,
    _In_ int nConnectRetries,
    _In_ int fEnableFallback,
    _In_ int sendQueueDepth,
    _In_ int recvQueueDepth
    )
{
    HRESULT hr = NdStartup();
    if( FAILED(hr) )
    {
        return MPIU_E_ERR( "**ch3|nd|startup %x", hr );
    }

    if( FAILED( ParseNetmask( L"MPICH_NETMASK", &m_NetAddr, &m_NetMask ) ) )
    {
        m_NetAddr.s_addr = 0;
        m_NetMask.s_addr = 0;
    }

    m_hExSet = hExSet;
    ExRegisterCompletionProcessor( EX_KEY_ND, OverlappedHandler );
    m_ZCopyThreshold = cbZCopyThreshold;
    m_EagerLimit = cbEagerLimit;
    m_MrCacheLimit = cbMrCacheLimit;
    m_nConnectRetries = nConnectRetries;
    m_fEnabled = true;
    m_fEnableFallback = (fEnableFallback != 0);
    m_SendQueueDepth = static_cast<UINT8>(RoundUpPow2( sendQueueDepth, ND_MAX_QUEUE_DEPTH ));
    m_RecvQueueDepth = static_cast<UINT8>(RoundUpPow2( recvQueueDepth, ND_MAX_QUEUE_DEPTH ));

    InitializePortRange();

    //
    // We always initialize the NDv1 environment, even if we only use NDv2 later.
    //
    return v1::Init( hExSet, cbZCopyThreshold, cbEagerLimit, cbMrCacheLimit, fEnableFallback );
}


void
Environment::Shutdown()
{
    {
        CS lock( m_MrLock );
        while( !m_MrCache.empty() )
        {
            RemoveMr( m_MrCache.front() );
        }
    }

    for( List<Adapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); NULL )
    {
        StackGuardRef<Adapter> pAdapter( &*i );
        i = m_AdapterList.erase( i );
        pAdapter->Shutdown();
    }

#pragma prefast(disable:25031, "Don't care about the status of NdCleanup - no possible recovery.");
    NdCleanup();

    m_fEnabled = false;
    m_NetAddr.s_addr = 0;
    m_NetMask.s_addr = 0;
}


int
Environment::Listen()
{
    if( !m_fEnabled )
    {
        return v1::Listen();
    }

    SIZE_T cbAddressList = 0;
    HRESULT hr = NdQueryAddressList( ND_QUERY_EXCLUDE_NDv1_ADDRESSES, NULL, &cbAddressList );
    if( hr == ND_SUCCESS && cbAddressList == 0 )
    {
        //
        // No NDv2 providers, try NDv1.
        //
        m_fEnabled = false;
        Trace_ND_Info_EnvironmentListen(this, EnvironmentListenNoNDv2Providers, hr);
        return v1::Listen();
    }

    if( hr != ND_BUFFER_OVERFLOW )
    {
        Trace_ND_Error_EnvironmentListen(this, EnvironmentListenQueryAddressListForSizeFailed, hr);
        return MPIU_E_ERR( "**ch3|nd|query_addr %x", hr );
    }

retry:
    StackGuardArray<char> p = new char[cbAddressList];
    if( p == NULL )
    {
        return MPI_ERR_NO_MEM;
    }
    SOCKET_ADDRESS_LIST *pAddressList = (SOCKET_ADDRESS_LIST*)p.get();
    if( pAddressList == NULL )
    {
        return MPI_ERR_NO_MEM;
    }

    hr = NdQueryAddressList( ND_QUERY_EXCLUDE_NDv1_ADDRESSES, pAddressList, &cbAddressList );
    if( FAILED(hr) )
    {
        if( hr == ND_BUFFER_OVERFLOW )
        {
            p.free();
            goto retry;
        }
        Trace_ND_Error_EnvironmentListen(this, EnvironmentListenQueryAddressListFailed, hr);
        return MPIU_E_ERR( "**ch3|nd|query_addr %x", hr );
    }

    //
    // If there are no addresses it means there is no ND support on the box.
    // Return success to let the job continue on without ND.
    //
    if( pAddressList->iAddressCount == 0 )
    {
        //
        // No NDv2 providers, try NDv1.
        //
        m_fEnabled = false;
        Trace_ND_Info_EnvironmentListen(this, EnvironmentListenNoNDv2Providers, hr);
        return v1::Listen();
    }

    //
    // Loop through the addresses looking for ones that match the requested
    // subnet mask and open the associated adapter.
    //
    for( int i = 0; i < pAddressList->iAddressCount; i++ )
    {
        if( pAddressList->Address[i].iSockaddrLength < sizeof(struct sockaddr_in) )
        {
            continue;
        }

        //
        // For now, only support IPv4 (subnet mask is IPv4 only)
        //
        if( pAddressList->Address[i].lpSockaddr->sa_family != AF_INET )
        {
            continue;
        }

        struct sockaddr_in* pAddr =
            (sockaddr_in*)pAddressList->Address[i].lpSockaddr;

        if( !AddressValid( *pAddr ) )
        {
            continue;
        }

        StackGuardRef<Adapter> pAdapter;
        int mpi_errno = CreateAdapter( *pAddr, &pAdapter.ref() );
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        mpi_errno = pAdapter->Listen(m_minPort, m_maxPort);
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }
        Trace_ND_Info_EnvironmentListen_Success(this, inet_ntoa(pAdapter->Addr().sin_addr), ntohs(pAdapter->Addr().sin_port));
    }
    return MPI_SUCCESS;
}


int
Environment::GetBusinessCard(
    __deref_inout_bcount_part( *pcbBusinessCard, *pcbBusinessCard ) char** pszBusinessCard,
    _Inout_ int* pcbBusinessCard
    )
{
    if( !m_fEnabled )
    {
        return v1::GetBusinessCard( pszBusinessCard, pcbBusinessCard );
    }

    if( m_AdapterList.empty() )
    {
        return MPI_SUCCESS;
    }

    char description[MAX_HOST_DESCRIPTION_LEN];
    description[0] = '\0';
    char* pDescription = description;
    int len = _countof( description );

    for( List<Adapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        if( !i->IsListening() )
        {
            continue;
        }

        int used = MPIU_Snprintf(
            pDescription,
            len,
            "%d.%d.%d.%d:%d ",
            i->Addr().sin_addr.s_net,
            i->Addr().sin_addr.s_host,
            i->Addr().sin_addr.s_lh,
            i->Addr().sin_addr.s_impno,
            ntohs(i->Addr().sin_port)
            );

        if( used < 0 || used >= len )
        {
            break;
        }

        pDescription += used;
        len -= used;
    }

    //
    // Make sure we have an address.
    //
    if( len == _countof( description ) )
    {
        return MPI_SUCCESS;
    }

    int str_errno = MPIU_Str_add_string_arg(
                    pszBusinessCard,
                    pcbBusinessCard,
                    MPIDI_CH3I_ND_DESCRIPTION_KEY,
                    description
                    );
    if (str_errno != MPIU_STR_SUCCESS)
    {
        if (str_errno == MPIU_STR_NOMEM)
        {
            return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**buscard_len" );
        }
        Trace_ND_Error_EnvironmentGetBusinessCard(this, str_errno, pszBusinessCard, pcbBusinessCard);
        return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**buscard" );
    }

    return MPI_SUCCESS;
}


int
Environment::Connect(
    __inout MPIDI_VC_t* pVc,
    __in_z const char* szBusinessCard,
    __in int fForceUse,
    __out int* pbHandled
    )
{
    *pbHandled = FALSE;
    if( !m_fEnabled )
    {
        return v1::Connect( pVc, szBusinessCard, fForceUse, pbHandled );
    }

    MPIU_Assert( pVc->ch.nd.pEp == NULL);
    struct sockaddr_in remoteAddrs[4] = {0};

    //
    // Decode business card
    //
    int mpi_errno = GetAddrsFromBc(
        szBusinessCard,
        remoteAddrs,
        _countof(remoteAddrs)
        );

    if( mpi_errno != MPI_SUCCESS )
    {
        Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectGetAddrsFromBc, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
        return MPIU_ERR_FAIL( mpi_errno );
    }

    bool fRemoteNd = remoteAddrs[0].sin_addr.s_addr != 0;

    if( m_AdapterList.empty() )
    {
        //
        // No ND on either, but force use: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // A matching NetworkDirect adapter is not available on either rank and the socket interconnect is disabled.
        // Check NetworkDirect configuration or clear the MPICH_DISABLE_SOCK environment variable.
        //
        if( !fRemoteNd && fForceUse )
        {
            Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectNoLocalNoRemoteForce, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
            return MPIU_E_ERR( "**ch3|nd|not_both_force %d %s", pVc->pg_rank, szBusinessCard );
        }

        //
        // No ND locally, but force use: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // There is no matching NetworkDirect adapter and the socket interconnect is disabled.
        // Check the local NetworkDirect configuration or clear the MPICH_DISABLE_SOCK environment variable.
        //
        if( fRemoteNd && fForceUse )
        {
            Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectNoLocalForce, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
            return MPIU_E_ERR( "**ch3|nd|not_here_force %d %s", pVc->pg_rank, szBusinessCard );
        }

        //
        // ND info in business card & adapter list is empty & no fallback: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // There is no matching NetworkDirect adapter and fallback to the socket interconnect is disabled.
        // Check the local NetworkDirect configuration or set the MPICH_ND_ENABLE_FALLBACK environment variable to true.
        //
        if( fRemoteNd && !m_fEnableFallback )
        {
            Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectNoLocalNoFallbackForce, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
            return MPIU_E_ERR( "**ch3|nd|not_here_fallback %d %s", pVc->pg_rank, szBusinessCard );
        }

        //
        // No ND info in business card, but not force use: SUCCESS
        // Adapter list is empty, but fallback enabled: SUCCESS
        //
        MPIU_Assert( !fForceUse || m_fEnableFallback );
        Trace_ND_Info_EnvironmentConnect(this, EnvironmentConnectNoLocalFallback, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
        return MPI_SUCCESS;
    }

    if( !fRemoteNd )
    {
        //
        // No ND on peer, but force use.
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // There is no matching NetworkDirect adapter and the socket interconnect is disabled.
        // Check the remote NetworkDirect configuration or clear the MPICH_DISABLE_SOCK environment variable.
        //
        if( fForceUse )
        {
            Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectNoRemoteForce, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
            return MPIU_E_ERR( "**ch3|nd|not_there_force %d %s", pVc->pg_rank, szBusinessCard );
        }

        //
        // No ND info in business card & adapter list is not empty & no fallback: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // There is no matching NetworkDirect adapter and fallback to the socket interconnect is disabled.
        // Check the remote NetworkDirect configuration or set the MPICH_ND_ENABLE_FALLBACK environment variable to true.
        //
        if( !m_fEnableFallback )
        {
            Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectNoRemoteNoFallback, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
            return MPIU_E_ERR( "**ch3|nd|not_there_fallback %d %s", pVc->pg_rank, szBusinessCard );
        }

        //
        // Not being forced, fallback OK: SUCCESS
        //
        Trace_ND_Info_EnvironmentConnect(this, EnvironmentConnectNoRemoteFallback, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
        return MPI_SUCCESS;
    }

    //
    // Resolve address
    //
    SIZE_T iAddr;
    StackGuardRef<Adapter> pAdapter(
        ResolveAdapter(
            remoteAddrs,
            _countof(remoteAddrs),
            &iAddr
            )
        );

    if( pAdapter.get() == NULL )
    {
        //
        // Both sides have ND, but there's no path and we're forcing use: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // The local and remote ranks have active NetworkDirect adapters but a route via NetworkDirect could not be resolved and the socket interconnect is disabled.
        // Check NetworkDirect configuration or clear the MPICH_DISABLE_SOCK environment variable.
        //
        if( fForceUse )
        {
            Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectNoPathForce, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
            return MPIU_E_ERR( "**ch3|nd|no_path_force %d %s", pVc->pg_rank, szBusinessCard );
        }

        //
        // Both sides have ND, but there's no path and no falback: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // The local and remote ranks have active NetworkDirect adapters but a route via NetworkDirect could not be resolved and fallback to the socket interconnect is disabled.
        // Check NetworkDirect configuration or set the MPICH_ND_ENABLE_FALLBACK environment variable to true.
        //
        if( !m_fEnableFallback )
        {
            Trace_ND_Error_EnvironmentConnect(this, EnvironmentConnectNoPathNoFallback, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
            return MPIU_E_ERR( "**ch3|nd|no_path_fallback %d %s", pVc->pg_rank, szBusinessCard );
        }

        //
        // Both sides have ND and there's no path, but fallback OK: SUCCESS
        //
        Trace_ND_Info_EnvironmentConnect(this, EnvironmentConnectNoPathFallback, mpi_errno, pVc->pg_rank, szBusinessCard, pVc, szBusinessCard, fForceUse, pbHandled);
        return MPI_SUCCESS;
    }

    *pbHandled = TRUE;

    //
    // ResolveAdapter could have pumped the progress engine, and connected this VC.  Check
    // the VC state and unwind as appropriate.
    //
    if( pVc->ch.state == MPIDI_CH3I_VC_STATE_UNCONNECTED )
    {
        mpi_errno = pAdapter->Connect( remoteAddrs[iAddr], pVc );
        if( mpi_errno == MPI_SUCCESS )
        {
            pVc->ch.channel = MPIDI_CH3I_CH_TYPE_ND;
        }
    }
    Trace_ND_Info_EnvironmentConnect_Success(this, pVc->pg_rank, szBusinessCard);
    return mpi_errno;
}


int
Environment::Poll(
    _Out_ BOOL* pfProgress
    )
{
    *pfProgress = FALSE;

    if( m_fEnabled == false )
    {
        return v1::Poll( pfProgress );
    }

    // Poll the CQs for completions.
    for( List<Adapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        int mpi_errno = i->Poll(pfProgress);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    return MPI_SUCCESS;
}


int Environment::Arm()
{
    if( m_fEnabled == false )
    {
        return v1::Arm();
    }

    // Poll the CQs for completions.
    for( List<Adapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        int mpi_errno = i->Arm();
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    return MPI_SUCCESS;
}


void Environment::InsertMr( __in Mr& mr )
{
    m_MrCache.push_front( mr );
    m_MrCacheSize += mr.GetLength();
    mr.AddRef();
}


void Environment::RemoveMr( __in Mr& mr )
{
    m_MrCache.remove( mr );
    m_MrCacheSize -= mr.GetLength();
    mr.Release();
}


int Environment::CreateMr(
    __in Adapter& adapter,
    __in const char* pBuf,
    __in SIZE_T cbBuf,
    __out Mr** ppMr
    )
{
    CS lock( m_MrLock );
    for( List<Mr>::iterator i = m_MrCache.begin(); i != m_MrCache.end(); NULL )
    {
        if( i->Matches( adapter, pBuf, cbBuf ) )
        {
            m_MrCache.remove( *i );
            m_MrCache.push_front( *i );
            *ppMr = &*i;
            (*ppMr)->AddRef();
            return MPI_SUCCESS;
        }

        Mr& mr = *i;
        ++i;
        //
        // Remove stale entries from the list to keep the list from growing too large.
        //
        if( mr.Stale() )
        {
            RemoveMr( mr );
        }
    }

    int mpi_errno = Mr::Create( adapter, pBuf, cbBuf, ppMr );
    if( mpi_errno != MPI_SUCCESS )
    {
        //
        // Purge all idle entries to maximize the chance that registration
        // will succeed and try again.
        //
        FlushIdleMrs( 0 );
        mpi_errno = Mr::Create( adapter, pBuf, cbBuf, ppMr );
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }
    }

    InsertMr( **ppMr );

    FlushIdleMrs( m_MrCacheLimit );

    return MPI_SUCCESS;
}


inline void Environment::FlushIdleMrs( __in UINT64 cacheSize )
{
    //
    // Walk the list backward and retire cache overflow entries.
    // N.B. This is an awkward way to walk the list back; List<> should
    //      expose a reverse_iterator and rbegin() rend() pair.
    //
    for( List<Mr>::iterator i = --(m_MrCache.end()); i != m_MrCache.end(); --i )
    {
        //
        // Flush Idle entries from the cache if over the limit.
        //
        if( m_MrCacheSize <= cacheSize )
        {
            return;
        }

        if( i->Idle() )
        {
            Mr& mr = *i;
            ++i;
            RemoveMr( mr );
        }
    }
}


void
Environment::FlushMrCache(
    __in const char* pBuf,
    __in SIZE_T cbBuf
    )
{
    CS lock( m_MrLock );
    for( List<Mr>::iterator i = m_MrCache.begin(); i != m_MrCache.end(); ++i )
    {
        if( !i->Overlaps( pBuf, cbBuf ) )
        {
            continue;
        }

        //
        // ISSUE: Don't release the MR! erezh 3/4/2008
        // Releasing the MR would cause the memory to be free while we are processing the
        // Registration Cache Callback, thus hitting bug Win7#160331.
        // Keep the object around longer; it would be freed by the cache limit in when
        // the next MR gets allocated or when the adapter get's freed.
        //
        i->Shutdown();
    }
}


Adapter* Environment::LookupAdapter( _In_ const sockaddr_in& addr )
{
    for( List<Adapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        if( i->Addr().sin_addr.s_addr == addr.sin_addr.s_addr )
        {
            i->AddRef();
            return &*i;
        }
    }
    return NULL;
}


MPI_RESULT
Environment::CreateAdapter(
    _In_ sockaddr_in& addr,
    _Outptr_ Adapter** ppAdapter
    )
{
    Adapter* pAdapter;

    int mpi_errno = Adapter::Create( addr, m_hExSet, m_ZCopyThreshold, &pAdapter );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    m_AdapterList.push_front( *pAdapter );
    pAdapter->AddRef();

    *ppAdapter = pAdapter;
    return MPI_SUCCESS;
}


int
Environment::GetAddrsFromBc(
    _In_z_ const char* szBusinessCard,
    _Out_writes_to_(nAddrs, return) struct sockaddr_in* addrs,
    _In_ int nAddrs
    )
{
    size_t n;
    int str_errno;
    int address_saved = 0;
    char host_description[MAX_HOST_DESCRIPTION_LEN];
    char hosts[MAX_HOST_DESCRIPTION_LEN];

    //
    // Decode business card.  Note that there might not be any ND information,
    // in which case we just return success.
    //
    str_errno = MPIU_Str_get_string_arg(
        szBusinessCard,
        MPIDI_CH3I_ND_DESCRIPTION_KEY,
        host_description, _countof(host_description)
        );
    if( str_errno != MPIU_STR_SUCCESS )
    {
        return MPI_SUCCESS;
    }

    const char* p = host_description;

    MPIU_Assert( nAddrs );

    for(;;)
    {
        *hosts = '\0';

        str_errno = MPIU_Str_get_string( &p, hosts, _countof(hosts) );
        if( str_errno != MPIU_STR_SUCCESS )
        {
            return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**argstr_missinghost" );
        }

        n = MPIU_Strlen( hosts, _countof(hosts) );
        //
        // If we don't save any addresses (due to netmask not matching)
        // or didn't have any addresses, return success.
        //
        if( n == 0 )
        {
            return MPI_SUCCESS;
        }

        int net, host, lh, impno, port;
        n = sscanf_s( hosts, "%d.%d.%d.%d:%d", &net, &host, &lh, &impno, &port );

        //
        // Make sure we got a valid address.
        //
        if( n != 5 )
        {
            return MPIU_E_ERR( "**ch3|nd|badbuscard" );
        }

        if( net > 255 || net < 1 ||
            host > 255 || host < 0 ||
            lh > 255 || lh < 0 ||
            impno > 255 || impno < 0 ||
            port > 65535 || port < 1 )
        {
            return MPIU_E_ERR( "**ch3|nd|badbuscard" );
        }

        addrs[address_saved].sin_addr.s_net = static_cast<char>(net);
        addrs[address_saved].sin_addr.s_host = static_cast<char>(host);
        addrs[address_saved].sin_addr.s_lh = static_cast<char>(lh);
        addrs[address_saved].sin_addr.s_impno = static_cast<char>(impno);
        addrs[address_saved].sin_port = htons(static_cast<USHORT>(port));
        addrs[address_saved].sin_family = AF_INET;

        address_saved++;

        if( address_saved == nAddrs )
        {
            return MPI_SUCCESS;
        }
    }
}


static HRESULT
NdResolveAddressWithRetries(
    _In_bytecount_(cbRemoteAddress) const struct sockaddr* pRemoteAddress,
    _In_ SIZE_T cbRemoteAddress,
    _Out_bytecap_(*pcbLocalAddress) struct sockaddr* pLocalAddress,
    _Inout_ SIZE_T* pcbLocalAddress,
    _In_ UINT16 nRetries
    )
{
    HRESULT hr = NdResolveAddress(
        pRemoteAddress,
        cbRemoteAddress,
        pLocalAddress,
        pcbLocalAddress
        );

    while( FAILED(hr) && nRetries > 0 )
    {
        if( MPIU_Sleep( 3000 + (rand() % 2000) ) != MPI_SUCCESS )
        {
            return E_FAIL;
        }

        hr = NdResolveAddress(
            pRemoteAddress,
            cbRemoteAddress,
            pLocalAddress,
            pcbLocalAddress
            );

        --nRetries;
    }

    return hr;
}


_Success_(return != nullptr)
Adapter*
Environment::ResolveAdapter(
    _In_reads_(nAddrs) const struct sockaddr_in* remoteAddrs,
    _In_ SIZE_T nAddrs,
    _Out_ SIZE_T* piRemoteAddr
    )
{
    struct sockaddr_in localAddr;
    Adapter* pAdapter = nullptr;

    for( SIZE_T i = 0; i < nAddrs; i++ )
    {
        if( remoteAddrs[i].sin_addr.s_addr == 0 ||
            remoteAddrs[i].sin_addr.s_addr == INADDR_NONE )
        {
            break;
        }

        SIZE_T cbLocalAddr = sizeof(localAddr);
        HRESULT hr = NdResolveAddressWithRetries(
            (const struct sockaddr*)&remoteAddrs[i],
            sizeof(remoteAddrs[i]),
            (struct sockaddr*)&localAddr,
            &cbLocalAddr,
            ConnectRetries()
            );
        if( FAILED( hr ) )
        {
            continue;
        }

        pAdapter = LookupAdapter( localAddr );
        if( pAdapter != nullptr )
        {
            *piRemoteAddr = i;
            break;
        }
    }

    return pAdapter;
}

OACR_WARNING_DISABLE(28301, "PREfast does not handle FN types correctly.");
int
WINAPI
Environment::OverlappedHandler(
    __in DWORD /*bytesTransferred*/,
    __in VOID* pOverlapped
    )
{
    ND_OVERLAPPED* pNdOv = static_cast<ND_OVERLAPPED*>(
        static_cast<OVERLAPPED*>( pOverlapped )
        );
    MPIU_Assert( pNdOv->pfnCompletion != NULL );
    return pNdOv->pfnCompletion( pNdOv );
}
OACR_WARNING_ENABLE(28301, "PREfast does not handle FN types correctly.");

Environment g_NdEnv;


}   // namespace CH3_ND
