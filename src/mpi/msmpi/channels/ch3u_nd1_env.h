// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd_env.h - Network Direct MPI CH3 ND Environment

--*/

#pragma once

#ifndef CH3U_NDV1_ENV_H
#define CH3U_NDV1_ENV_H


namespace CH3_ND
{
namespace v1
{

//
// Global per-process ND tracking structure.
//
class CEnvironment
{
public:
    CEnvironment();

    int
    Init(
        __in ExSetHandle_t hExSet,
        __in SIZE_T ZCopyThreshold,
        __in MPIDI_msg_sz_t EagerLimit,
        __in UINT64 MrCacheLimit,
        __in int fEnableFallback
        );

    void Shutdown();

    int Listen();

    int
    GetBusinessCard(
        __deref_inout_bcount_part( *pBusinessCardLength, *pBusinessCardLength ) char** pBusinessCard,
        _Inout_ int* pBusinessCardLength
        );

    int
    Connect(
        _Inout_ MPIDI_VC_t* pVc,
        _In_ const char* BusinessCard,
        _In_ int fForceUse,
        _Out_ int* pbHandled
        );

    int Poll(__out BOOL* pfProgress);
    int Arm();

    int CreateMr(
        __in CAdapter* pAdapter,
        __in const char* pBuf,
        __in SIZE_T Length,
        __out CMr** ppMr
        );

    void FlushMrCache( __in const char* pBuf, __in SIZE_T Length );

    int EagerLimit() const { return m_EagerLimit; }

private:
    MPI_RESULT
    CreateAdapter(
        _In_ sockaddr_in& Addr,
        _Outptr_ CAdapter** ppAdapter
        );

    CAdapter* LookupAdapter( _In_ const sockaddr_in& Addr );

    static int
    GetAddrsFromBc(
        const char* BusinessCard,
        struct sockaddr_in* Addrs,
        int nAddrs
        );

    bool AddressValid( __in const struct sockaddr_in& Addr ) const
    {
        return (Addr.sin_addr.s_addr & m_NetMask.s_addr) == m_NetAddr.s_addr;
    }

    _Success_(return != nullptr)
    CAdapter*
    ResolveAdapter(
        _In_reads_(nAddrs) const struct sockaddr_in* RemoteAddrs,
        _In_ SIZE_T nAddrs,
        _Out_ SIZE_T* piRemoteAddr
        );

    inline void FlushIdleMrs( __in UINT64 CacheSize );
    inline void InsertMr( __in CMr& Mr );
    inline void RemoveMr( __in CMr& Mr );

private:
    List<CAdapter> m_AdapterList;
    ExSetHandle_t m_hExSet;
    struct in_addr m_NetAddr;
    struct in_addr m_NetMask;
    bool m_fEnabled;
    bool m_fEnableFallback;
    SIZE_T m_ZCopyThreshold;
    UINT64 m_MrCacheLimit;
    MPIDI_msg_sz_t m_EagerLimit;

    CriticalSection m_MrLock;
    List<CMr> m_MrCache;
    UINT64 m_MrCacheSize;
};


extern CEnvironment g_NdEnv;

}   // namespace v1
}   // namespace CH3_ND

#endif // CH3U_NDV1_ENV_H
