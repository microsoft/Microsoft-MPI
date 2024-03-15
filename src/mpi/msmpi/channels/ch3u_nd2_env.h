// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd_env.h - Network Direct MPI CH3 ND Environment

--*/

#pragma once

#ifndef CH3U_ND_ENV_H
#define CH3U_ND_ENV_H


namespace CH3_ND
{

//
// Global per-process ND tracking structure.
//
class Environment
{
    List<Adapter> m_AdapterList;
    ExSetHandle_t m_hExSet;
    struct in_addr m_NetAddr;
    struct in_addr m_NetMask;
    bool m_fEnabled;
    bool m_fEnableFallback;
    ULONG m_ZCopyThreshold;
    UINT64 m_MrCacheLimit;
    MPIDI_msg_sz_t m_EagerLimit;

    CriticalSection m_MrLock;
    List<Mr> m_MrCache;
    UINT64 m_MrCacheSize;
    int m_nConnectRetries;

    UINT8 m_SendQueueDepth;
    UINT8 m_RecvQueueDepth;

    int m_maxPort;
    int m_minPort;

private:
    Environment( const Environment& rhs );
    Environment& operator = ( const Environment& rhs );

public:
    Environment();

    int
    Init(
        _In_ ExSetHandle_t hExSet,
        _In_ ULONG cbZCopyThreshold,
        _In_ MPIDI_msg_sz_t cbEagerLimit,
        _In_ UINT64 cbMrCacheLimit,
        _In_ int nConnectRetries,
        _In_ int fEnableFallback,
        _In_ int sendQueueDepth,
        _In_ int recvQueueDepth
        );

    void Shutdown();

    int Listen();

    int
    GetBusinessCard(
        __deref_inout_bcount_part( *pcbBusinessCard, *pcbBusinessCard ) char** pszBusinessCard,
        _Inout_ int* pcbBusinessCard
        );

    int
    Connect(
        __inout MPIDI_VC_t* pVc,
        __in_z const char* szBusinessCard,
        __in int fForceUse,
        __out int* pbHandled
        );

    int
    Poll(
        _Out_ BOOL* pfProgress
        );

    int Arm();

    int CreateMr(
        __in Adapter& adapter,
        __in const char* pBuf,
        __in SIZE_T cbBuf,
        __out Mr** ppMr
        );

    void FlushMrCache( __in const char* pBuf, __in SIZE_T cbBuf );

    int EagerLimit() const { return m_EagerLimit; }

    inline UINT16 ConnectRetries() const
    {
        return static_cast<UINT16>( min(m_nConnectRetries, USHRT_MAX) );
    }

    inline UINT8 SendQueueDepth() const
    {
        return m_SendQueueDepth;
    }

    inline UINT8 RecvQueueDepth() const
    {
        return m_RecvQueueDepth;
    }

    inline ULONG InitiatorQueueDepth() const
    {
        return static_cast<ULONG>(SendQueueDepth()) + ND_READ_LIMIT;
    }

    inline ULONG CqDepthPerEp() const
    {
        return InitiatorQueueDepth() + RecvQueueDepth();
    }

private:
    MPI_RESULT
    CreateAdapter(
        _In_ sockaddr_in& addr,
        _Outptr_ Adapter** ppAdapter
        );

    Adapter* LookupAdapter( _In_ const sockaddr_in& addr );

    static int
    GetAddrsFromBc(
        _In_z_ const char* szBusinessCard,
        _Out_writes_to_(nAddrs,return) struct sockaddr_in* addrs,
        _In_ int nAddrs
        );

    bool AddressValid( __in const struct sockaddr_in& addr ) const
    {
        return (addr.sin_addr.s_addr & m_NetMask.s_addr) == m_NetAddr.s_addr;
    }

    _Success_(return != nullptr)
    Adapter*
    ResolveAdapter(
        _In_reads_(nAddrs) const struct sockaddr_in* remoteAddrs,
        _In_ SIZE_T nAddrs,
        _Out_ SIZE_T* piRemoteAddr
        );

    inline void FlushIdleMrs( __in UINT64 cacheSize );
    inline void InsertMr( __in Mr& mr );
    inline void RemoveMr( __in Mr& mr );

    static int WINAPI OverlappedHandler( DWORD bytesTransferred, VOID* pOverlapped );

    void InitializePortRange();
};


extern Environment g_NdEnv;

}   // namespace CH3_ND

#endif // CH3U_ND_ENV_H
