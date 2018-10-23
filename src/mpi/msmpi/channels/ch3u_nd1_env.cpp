// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_env.cpp - Network Direct MPI CH3 ND Environment

--*/

#include "precomp.h"
#include "ch3u_nd1.h"



#define MPIDI_CH3I_NDv1_DESCRIPTION_KEY "ndv1"


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
namespace v1
{

CEnvironment::CEnvironment() :
    m_hExSet( NULL ),
    m_fEnabled( false ),
    m_ZCopyThreshold( 0 ),
    m_MrCacheSize( 0 )
{
    m_NetAddr.s_addr = 0;
    m_NetMask.s_addr = 0;
}


int
CEnvironment::Init(
    __in ExSetHandle_t hExSet,
    __in SIZE_T ZCopyThreshold,
    __in MPIDI_msg_sz_t EagerLimit,
    __in UINT64 MrCacheLimit,
    __in int fEnableFallback
    )
{
    HRESULT hr = NdStartup();
    if( FAILED(hr) )
        return MPIU_E_ERR( "**ch3|nd|startup %x", hr );

    if( FAILED( ParseNetmask( L"MPICH_NETMASK", &m_NetAddr, &m_NetMask ) ) )
    {
        m_NetAddr.s_addr = 0;
        m_NetMask.s_addr = 0;
    }

    m_hExSet = hExSet;
    m_ZCopyThreshold = ZCopyThreshold;
    m_EagerLimit = EagerLimit;
    m_MrCacheLimit = MrCacheLimit;
    m_fEnabled = true;
    m_fEnableFallback = (fEnableFallback != 0);

    return MPI_SUCCESS;
}


void
CEnvironment::Shutdown()
{
    {
        CS lock( m_MrLock );
        while( !m_MrCache.empty() )
        {
            RemoveMr( m_MrCache.front() );
        }
    }

    for( List<CAdapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); NULL )
    {
        StackGuardRef<CAdapter> pAdapter( &*i );
        i = m_AdapterList.erase( i );
        pAdapter->Shutdown();
    }

    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
    NdCleanup();

    m_fEnabled = false;
    m_NetAddr.s_addr = 0;
    m_NetMask.s_addr = 0;
}


int
CEnvironment::Listen()
{
    if( !m_fEnabled )
    {
        return MPI_SUCCESS;
    }

    SIZE_T Size = 0;
    HRESULT hr = NdQueryAddressList( ND_QUERY_EXCLUDE_NDv2_ADDRESSES, NULL, &Size );
    if( hr == ND_SUCCESS && Size == 0 )
    {
        m_fEnabled = false;
        return MPI_SUCCESS;
    }

    if( hr != ND_BUFFER_OVERFLOW )
    {
        return MPIU_E_ERR( "**ch3|nd|query_addr %x", hr );
    }

retry:
    StackGuardArray<char> p = new char[Size];
    SOCKET_ADDRESS_LIST *pAddressList = (SOCKET_ADDRESS_LIST*)p.get();
    if( pAddressList == NULL )
        return MPI_ERR_NO_MEM;

    hr = NdQueryAddressList( ND_QUERY_EXCLUDE_NDv2_ADDRESSES, pAddressList, &Size );
    if( FAILED(hr) )
    {
        if( hr == ND_BUFFER_OVERFLOW )
        {
            p.free();
            goto retry;
        }

        return MPIU_E_ERR( "**ch3|nd|query_addr %x", hr );
    }

    //
    // If there are no addresses it means there is no ND support on the box.
    // Return success to let the job continue on without ND.
    //
    if( pAddressList->iAddressCount == 0 )
    {
        m_fEnabled = false;
        return MPI_SUCCESS;
    }

    //
    // Loop through the addresses looking for ones that match the requested
    // subnet mask and open the associated adapter.
    //
    for( int i = 0; i < pAddressList->iAddressCount; i++ )
    {
        if( pAddressList->Address[i].iSockaddrLength < sizeof(struct sockaddr_in) )
            continue;

        //
        // For now, only support IPv4 (subnet mask is IPv4 only)
        //
        if( pAddressList->Address[i].lpSockaddr->sa_family != AF_INET )
            continue;

        struct sockaddr_in* pAddr =
            (sockaddr_in*)pAddressList->Address[i].lpSockaddr;

        if( !AddressValid( *pAddr ) )
            continue;

        StackGuardRef<CAdapter> pAdapter;
        int mpi_errno = CreateAdapter( *pAddr, &pAdapter.ref() );
        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );

        mpi_errno = pAdapter->Listen();
        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );
    }
    return MPI_SUCCESS;
}


int
CEnvironment::GetBusinessCard(
    __deref_inout_bcount_part( *pBusinessCardLength, *pBusinessCardLength ) char** pBusinessCard,
    _Inout_ int* pBusinessCardLength
    )
{
    if( !m_fEnabled )
        return MPI_SUCCESS;

    if( m_AdapterList.empty() )
        return MPI_SUCCESS;

    char Description[MAX_HOST_DESCRIPTION_LEN];
    Description[0] = '\0';
    char* pDescription = Description;
    int len = _countof( Description );

    for( List<CAdapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        if( !i->IsListening() )
            continue;

        int used = MPIU_Snprintf(
            pDescription,
            len,
            "%d.%d.%d.%d:%d ",
            i->Addr().sin_addr.s_net,
            i->Addr().sin_addr.s_host,
            i->Addr().sin_addr.s_lh,
            i->Addr().sin_addr.s_impno,
            i->Addr().sin_port
            );

        if( used < 0 || used >= len )
            break;

        pDescription += used;
        len -= used;
    }

    //
    // Make sure we have an address.
    //
    if( len == _countof( Description ) )
        return MPI_SUCCESS;

    int str_errno;
    str_errno = MPIU_Str_add_string_arg(
                    pBusinessCard,
                    pBusinessCardLength,
                    MPIDI_CH3I_NDv1_DESCRIPTION_KEY,
                    Description
                    );
    if (str_errno != MPIU_STR_SUCCESS)
    {
        if (str_errno == MPIU_STR_NOMEM)
            return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**buscard_len" );

        return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**buscard" );
    }

    return MPI_SUCCESS;
}


int
CEnvironment::Connect(
    _Inout_ MPIDI_VC_t* pVc,
    _In_ const char* BusinessCard,
    _In_ int fForceUse,
    _Out_ int* pbHandled
    )
{
    *pbHandled = FALSE;

    if( !m_fEnabled )
        return MPI_SUCCESS;

    MPIU_Assert( pVc->ch.nd.pEpV1 == NULL);
    struct sockaddr_in RemoteAddrs[4] = {0};
    int mpi_errno;

    //
    // Decode business card
    //
    mpi_errno = GetAddrsFromBc(
        BusinessCard,
        RemoteAddrs,
        _countof(RemoteAddrs)
        );

    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    bool fRemoteNd = RemoteAddrs[0].sin_addr.s_addr != 0;

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
            return MPIU_E_ERR( "**ch3|nd|not_both_force %d %s", pVc->pg_rank, BusinessCard );

        //
        // No ND locally, but force use: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // There is no matching NetworkDirect adapter and the socket interconnect is disabled.
        // Check the local NetworkDirect configuration or clear the MPICH_DISABLE_SOCK environment variable.
        //
        if( fRemoteNd && fForceUse )
            return MPIU_E_ERR( "**ch3|nd|not_here_force %d %s", pVc->pg_rank, BusinessCard );

        //
        // ND info in business card & adapter list is empty & no fallback: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // There is no matching NetworkDirect adapter and fallback to the socket interconnect is disabled.
        // Check the local NetworkDirect configuration or set the MPICH_ND_ENABLE_FALLBACK environment variable to true.
        //
        if( fRemoteNd && !m_fEnableFallback )
            return MPIU_E_ERR( "**ch3|nd|not_here_fallback %d %s", pVc->pg_rank, BusinessCard );

        //
        // No ND info in business card, but not force use: SUCCESS
        // Adapter list is empty, but fallback enabled: SUCCESS
        //
        MPIU_Assert( !fForceUse || m_fEnableFallback );
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
            return MPIU_E_ERR( "**ch3|nd|not_there_force %d %s", pVc->pg_rank, BusinessCard );

        //
        // No ND info in business card & adapter list is not empty & no fallback: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // There is no matching NetworkDirect adapter and fallback to the socket interconnect is disabled.
        // Check the remote NetworkDirect configuration or set the MPICH_ND_ENABLE_FALLBACK environment variable to true.
        //
        if( !m_fEnableFallback )
            return MPIU_E_ERR( "**ch3|nd|not_there_fallback %d %s", pVc->pg_rank, BusinessCard );

        //
        // Not being forced, fallback OK: SUCCESS
        //
        return MPI_SUCCESS;
    }

    //
    // Resolve address
    //
    SIZE_T iAddr;
    StackGuardRef<CAdapter> pAdapter(
        ResolveAdapter(
            RemoteAddrs,
            _countof(RemoteAddrs),
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
            return MPIU_E_ERR( "**ch3|nd|no_path_force %d %s", pVc->pg_rank, BusinessCard );

        //
        // Both sides have ND, but there's no path and no falback: FAIL
        //
        // [ch3:nd] Could not connect via NetworkDirect to rank %d with business card (%s).
        // The local and remote ranks have active NetworkDirect adapters but a route via NetworkDirect could not be resolved and fallback to the socket interconnect is disabled.
        // Check NetworkDirect configuration or set the MPICH_ND_ENABLE_FALLBACK environment variable to true.
        //
        if( !m_fEnableFallback )
            return MPIU_E_ERR( "**ch3|nd|no_path_fallback %d %s", pVc->pg_rank, BusinessCard );

        //
        // Both sides have ND and there's no path, but fallback OK: SUCCESS
        //
        return MPI_SUCCESS;
    }

    *pbHandled = TRUE;

    //
    // We have an adapter now - create the endpoint.
    //
    StackGuardRef<INDConnector> pIConnector;
    HRESULT hr = pAdapter->IAdapter()->CreateConnector( &pIConnector.ref() );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|create_conn %x", hr );

    MPIU_Assert( pVc->ch.nd.pEpV1 == NULL );
    StackGuardRef<CEndpoint> pEp;
    mpi_errno = CEndpoint::Create(
        pAdapter.get(),
        pIConnector.get(),
        RemoteAddrs[iAddr],
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

    pVc->ch.channel = MPIDI_CH3I_CH_TYPE_NDv1;
    return MPI_SUCCESS;
}


int CEnvironment::Poll(__out BOOL* pfProgress)
{
    *pfProgress = FALSE;

    // Poll the CQs for completions.
    for( List<CAdapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        int mpi_errno = i->Poll(pfProgress);
        if( mpi_errno != MPI_SUCCESS )
            return mpi_errno;
    }

    return MPI_SUCCESS;
}


int CEnvironment::Arm()
{
    // Poll the CQs for completions.
    for( List<CAdapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        int mpi_errno = i->Arm();
        if( mpi_errno != MPI_SUCCESS )
            return mpi_errno;
    }

    return MPI_SUCCESS;
}


void CEnvironment::InsertMr( __in CMr& Mr )
{
    m_MrCache.push_front( Mr );
    m_MrCacheSize += Mr.GetLength();
    Mr.AddRef();
}


void CEnvironment::RemoveMr( __in CMr& Mr )
{
    m_MrCache.remove( Mr );
    m_MrCacheSize -= Mr.GetLength();
    Mr.Release();
}


int CEnvironment::CreateMr(
    __in CAdapter* pAdapter,
    __in const char* pBuf,
    __in SIZE_T Length,
    __out CMr** ppMr
    )
{
    CS lock( m_MrLock );
    for( List<CMr>::iterator i = m_MrCache.begin(); i != m_MrCache.end(); NULL )
    {
        if( i->Matches( pAdapter, pBuf, Length ) )
        {
            m_MrCache.remove( *i );
            m_MrCache.push_front( *i );
            *ppMr = &*i;
            (*ppMr)->AddRef();
            return MPI_SUCCESS;
        }

        CMr& Mr = *i;
        ++i;
        //
        // Remove stale entries from the list to keep the list from growing too large.
        //
        if( Mr.Stale() )
        {
            RemoveMr( Mr );
        }
    }

    int mpi_errno = CMr::Create( pAdapter, pBuf, Length, ppMr );
    if( mpi_errno != MPI_SUCCESS )
    {
        //
        // Purge all idle entries to maximize the chance that registration
        // will succeed and try again.
        //
        FlushIdleMrs( 0 );
        mpi_errno = CMr::Create( pAdapter, pBuf, Length, ppMr );
        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );
    }

    InsertMr( **ppMr );

    FlushIdleMrs( m_MrCacheLimit );

    return MPI_SUCCESS;
}


inline void CEnvironment::FlushIdleMrs( __in UINT64 CacheSize )
{
    //
    // Walk the list backward and retire cache overflow entries.
    // N.B. This is an awkward way to walk the list back; List<> should
    //      expose a reverse_iterator and rbegin() rend() pair.
    //
    for( List<CMr>::iterator i = --(m_MrCache.end()); i != m_MrCache.end(); --i )
    {
        //
        // Flush Idle entries from the cache if over the limit.
        //
        if( m_MrCacheSize <= CacheSize )
            return;

        if( i->Idle() )
        {
            CMr& Mr = *i;
            ++i;
            RemoveMr( Mr );
        }
    }
}


void
CEnvironment::FlushMrCache(
    __in const char* pBuf,
    __in SIZE_T Length
    )
{
    CS lock( m_MrLock );
    for( List<CMr>::iterator i = m_MrCache.begin(); i != m_MrCache.end(); ++i )
    {
        if( !i->Overlaps( pBuf, Length ) )
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


CAdapter* CEnvironment::LookupAdapter( _In_ const sockaddr_in& Addr )
{
    for( List<CAdapter>::iterator i = m_AdapterList.begin(); i != m_AdapterList.end(); ++i )
    {
        if( i->Addr().sin_addr.s_addr == Addr.sin_addr.s_addr )
        {
            i->AddRef();
            return &*i;
        }
    }
    return NULL;
}


MPI_RESULT
CEnvironment::CreateAdapter(
    _In_ sockaddr_in& Addr,
    _Outptr_ CAdapter** ppAdapter
    )
{
    CAdapter* pAdapter;

    int mpi_errno = CAdapter::Create( Addr, m_hExSet, m_ZCopyThreshold, &pAdapter );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    m_AdapterList.push_front( *pAdapter );
    pAdapter->AddRef();

    *ppAdapter = pAdapter;
    return MPI_SUCCESS;
}


int
CEnvironment::GetAddrsFromBc(
    const char* BusinessCard,
    struct sockaddr_in* Addrs,
    int nAddrs
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
        BusinessCard,
        MPIDI_CH3I_NDv1_DESCRIPTION_KEY,
        host_description, _countof(host_description)
        );
    if( str_errno != MPIU_STR_SUCCESS )
        return MPI_SUCCESS;

    const char* p = host_description;

    MPIU_Assert( nAddrs );

    for(;;)
    {
        *hosts = '\0';

        str_errno = MPIU_Str_get_string( &p, hosts, _countof(hosts) );
        if( str_errno != MPIU_STR_SUCCESS )
            return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**argstr_missinghost" );

        n = MPIU_Strlen( hosts, _countof(hosts) );
        //
        // If we don't save any addresses (due to netmask not matching)
        // or didn't have any addresses, return success.
        //
        if( n == 0 )
            return MPI_SUCCESS;

        int net, host, lh, impno, port;
        n = sscanf_s( hosts, "%d.%d.%d.%d:%d", &net, &host, &lh, &impno, &port );

        //
        // Make sure we got a valid address.
        //
        if( n != 5 )
            return MPIU_E_ERR( "**ch3|nd|badbuscard" );

        if( net > 255 || net < 1 ||
            host > 255 || host < 0 ||
            lh > 255 || lh < 0 ||
            impno > 255 || impno < 0 ||
            port > 65535 || port < 1 )
        {
            return MPIU_E_ERR( "**ch3|nd|badbuscard" );
        }

        Addrs[address_saved].sin_addr.s_net = (char)net;
        Addrs[address_saved].sin_addr.s_host = (char)host;
        Addrs[address_saved].sin_addr.s_lh = (char)lh;
        Addrs[address_saved].sin_addr.s_impno = (char)impno;
        Addrs[address_saved].sin_port = (short)port;
        Addrs[address_saved].sin_family = AF_INET;

        address_saved++;

        if( address_saved == nAddrs )
            return MPI_SUCCESS;
    }
}


_Success_(return != nullptr)
CAdapter*
CEnvironment::ResolveAdapter(
    _In_reads_(nAddrs) const struct sockaddr_in* RemoteAddrs,
    _In_ SIZE_T nAddrs,
    _Out_ SIZE_T* piRemoteAddr
    )
{
    struct sockaddr_in LocalAddr;
    CAdapter* pAdapter = nullptr;

    for( SIZE_T i = 0; i < nAddrs; i++ )
    {
        if( RemoteAddrs[i].sin_addr.s_addr == 0 ||
            RemoteAddrs[i].sin_addr.s_addr == INADDR_NONE )
        {
            break;
        }

        SIZE_T Len = sizeof(LocalAddr);
        HRESULT hr = NdResolveAddress(
            (const struct sockaddr*)&RemoteAddrs[i],
            sizeof(RemoteAddrs[i]),
            (struct sockaddr*)&LocalAddr,
            &Len
            );
        if( FAILED( hr ) )
            continue;

        pAdapter = LookupAdapter( LocalAddr );
        if( pAdapter != nullptr )
        {
            *piRemoteAddr = i;
            break;
        }
    }

    return pAdapter;
}


CEnvironment g_NdEnv;


}   // namespace v1
}   // namespace CH3_ND
