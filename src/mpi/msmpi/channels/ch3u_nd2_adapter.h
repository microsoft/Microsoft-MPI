// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd_adapter.h - Network Direct MPI CH3 Channel adapter class

--*/

#pragma once

#ifndef CH3U_ND_ADAPTER_H
#define CH3U_ND_ADAPTER_H


namespace CH3_ND
{

class Cq;

class Adapter
{
    friend class ListHelper<Adapter>;

    ExSetHandle_t m_hExSet;

    LIST_ENTRY m_link;

    List<Cq> m_CqList;

    StackGuardRef<IND2Adapter> m_pIAdapter;
    StackGuardRef<IND2Listener> m_pIListener;
    StackGuardRef<IND2Connector> m_pIConnector;

    HANDLE m_hOverlappedFile;
    ND2_ADAPTER_INFO m_Info;

    //
    // Address cached to allow sharing of adapter with multiple EPs.
    //
    struct sockaddr_in m_Addr;

    volatile LONG m_nRef;
    bool m_fGCRPending;
    ND_OVERLAPPED m_ListenOv;

private:
    Adapter( __in ExSetHandle_t hExSet );
    Adapter( const Adapter& rhs );
    ~Adapter();
    int
    Init(
        __in sockaddr_in& addr,
        __in ExSetHandle_t hExSet,
        __in ULONG cbZCopyThreshold
        );
    Adapter& operator = ( const Adapter& rhs );

public:
    static
    MPI_RESULT
    Create(
        _In_ sockaddr_in& addr,
        _In_ ExSetHandle_t hExSet,
        _In_ ULONG cbZCopyThreshold,
        _Outptr_ Adapter** ppAdapter
        );

public:
    void Shutdown();

    LONG AddRef()
    {
        return ::InterlockedIncrement(&m_nRef);
    }

    void Release();

    MPI_RESULT Listen(
        _In_opt_ int minPort,
        _In_opt_ int maxPort
        );
    bool IsListening() const{ return m_pIListener.get() != NULL; }

    MPI_RESULT
    GetAvailableCq(
        _Outptr_ Cq** ppCq
        );

    int
    Poll(
        _Out_ BOOL* pfProgress
        );

    int Arm();

    int Connect(
        __in const struct sockaddr_in& destAddr,
        __inout MPIDI_VC_t* pVc
        );

    int CreateMrVadAware(
        __in const char* pBuf,
        __inout MPIU_Bsize_t* pLength,
        __in MPIU_Bsize_t minLength,
        __out Mr** ppMr
        );


    int CreateConnector( __deref_out IND2Connector** ppConnector );

    int CreateMr( __in const char* pBuf, __in SIZE_T cbBuf, __out Mr** ppMr );

    MPI_RESULT
    RegisterMemory(
        _In_ const char* pBuf,
        _In_ SIZE_T cbBuf,
        _In_ DWORD flags,
        _Outptr_ IND2MemoryRegion** ppMr
        );
    static int DeregisterMemory( __in IND2MemoryRegion* pMr );

    ULONG MaxEpPerCq() const;

    ULONG MaxReadLength() const
    {
        return static_cast<ULONG>(min( m_Info.MaxRegistrationSize, m_Info.MaxTransferLength));
    }

    inline bool UseRma( SIZE_T cbSend ) const
    {
        return cbSend > m_Info.LargeRequestThreshold;
    }

    inline bool UseInline( SIZE_T cbSend ) const
    {
        return cbSend <= m_Info.InlineRequestThreshold;
    }

    inline ULONG GetInlineThreshold() const
    {
        return m_Info.InlineRequestThreshold;
    }

    const struct sockaddr_in& Addr() const{ return m_Addr; }

    IND2Adapter* IAdapter(){ return m_pIAdapter.get(); }
    HANDLE GetOverlappedFile(){ return m_hOverlappedFile; }

    ExSetHandle_t GetExSetHandle(){ return m_hExSet; }

private:
    int Accept(
        __in MPIDI_VC_t* pVc,
        __in IND2Connector* pIConnector,
        __in const nd_caller_data_t& callerData
        );
    int GetConnectionRequest();

    int GetConnSucceeded( _In_ IND2Connector* pIConnector );

private:
    static ND_OVERLAPPED::FN_CompletionRoutine GetConnReqHandler;
};

}   // namespace CH3_ND

#endif // CH3U_ND_ADAPTER_H
