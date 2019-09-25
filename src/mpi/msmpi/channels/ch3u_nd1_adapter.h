// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd_adapter.h - Network Direct MPI CH3 Channel adapter class

--*/

#pragma once

#ifndef CH3U_NDV1_ADAPTER_H
#define CH3U_NDV1_ADAPTER_H


namespace CH3_ND
{
namespace v1
{

class CCq;

class CAdapter
{
    friend class ListHelper<CAdapter>;

private:
    CAdapter();
    ~CAdapter();
    int
    Init(
        __in sockaddr_in& Addr,
        __in ExSetHandle_t hExSet,
        __in SIZE_T ZCopyThreshold
        );

public:
    static
    MPI_RESULT
    Create(
        _In_ sockaddr_in& Addr,
        _In_ ExSetHandle_t hExSet,
        _In_ SIZE_T ZCopyThreshold,
        _Outptr_ CAdapter** ppAdapter
        );

public:
    void Shutdown();

    LONG AddRef()
    {
        return ::InterlockedIncrement(&m_nRef);
    }

    void Release();

    int Listen();
    bool IsListening() const{ return m_pIListen.get() != NULL; }

    MPI_RESULT
    GetAvailableCq(
        _Outptr_ CCq** ppCq
        );

    int Poll(__out BOOL* pfProgress);
    int Arm();

    int CreateMr( __in const char* pBuf, __in SIZE_T Length, __out CMr** ppMr );
    int CreateMrVadAware(
        __in const char* pBuf,
        __inout MPIU_Bsize_t* pLength,
        __in MPIU_Bsize_t minLength,
        __out CMr** ppMr
        );

    int RegisterMemory( __in const char* pBuf, __in SIZE_T Length, __out ND_MR_HANDLE* phMr );
    int DeregisterMemory( __in ND_MR_HANDLE hMr );

    int AllocMw( __out CMw** ppMw );
    void FreeMw( __in CMw* pMw );

    SIZE_T IRL() const{ return min(m_Info.MaxInboundReadLimit, NDv1_READ_LIMIT); }
    SIZE_T ORL() const{ return min(m_Info.MaxOutboundReadLimit, NDv1_READ_LIMIT); }
    SIZE_T MaxEpPerCq() const{ return m_Info.MaxCqEntries / NDv1_CQ_ENTRIES_PER_EP; }
    SIZE_T MaxWindowSize() const{ return min( m_Info.MaxRegistrationSize, m_Info.MaxWindowSize); }
    SIZE_T MaxReadLength() const{ return min( m_Info.MaxRegistrationSize, m_Info.MaxOutboundLength); }

    bool UseRma( SIZE_T Size ) const
    {
        return Size > m_Info.LargeRequestThreshold;
    }

    const struct sockaddr_in& Addr() const{ return m_Addr; }

    INDAdapter* IAdapter(){ return m_pIAdapter.get(); }

private:
    int Accept(
        __in MPIDI_VC_t* pVc,
        __in INDConnector* pIConnector,
        __in SIZE_T InboundReadLimit,
        __in SIZE_T OutboundReadLimit,
        __in const nd_caller_data_t& CallerData
        );
    int GetConnectionRequest();

    int GetConnSucceeded();

private:
    static int WINAPI GetConnSucceeded( __in EXOVERLAPPED* pOverlapped );
    static int WINAPI GetConnFailed( __in EXOVERLAPPED* pOverlapped );

private:
    volatile LONG m_nRef;

    LIST_ENTRY m_link;

    List<CCq> m_CqList;

    StackGuardRef<INDAdapter> m_pIAdapter;
    ND_ADAPTER_INFO m_Info;

    //
    // Address cached to allow sharing of adapter with multiple EPs.
    //
    struct sockaddr_in m_Addr;

    StackGuardRef<INDListen> m_pIListen;
    StackGuardRef<INDConnector> m_pIConnector;
    bool m_fGCRPending;
    EXOVERLAPPED m_ListenOv;

    List<CMw> m_MwPool;
    SIZE_T m_nMw;

    static const SIZE_T x_MwPoolLimit = 32;

};

}   // namespace v1
}   // namespace CH3_ND

#endif // CH3U_NDV1_ADAPTER_H
