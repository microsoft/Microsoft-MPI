// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd_cq.h - Network Direct MPI CH3 Channel

--*/

#pragma once

#ifndef CH3U_NDV1_CQ_H
#define CH3U_NDV1_CQ_H


namespace CH3_ND
{
namespace v1
{

class CAdapter;

class CCq
{
    friend class ListHelper<CCq>;

private:
    CCq();
    ~CCq();
    int Init( __in CAdapter* pAdapter );

public:
    static
    MPI_RESULT
    Create(
        _In_ CAdapter* pAdapter,
        _Outptr_ CCq** ppCq
        );

    void Shutdown();

    LONG AddRef()
    {
        return ::InterlockedIncrement(&m_nRef);
    }

    void Release();

    int Poll(_Inout_ BOOL* pfProgress);
    int Arm();

    bool Full() const{ return (m_nUsed + 1) > m_Size; }
    void AllocateEntries(){ m_nUsed++; }
    void FreeEntries(){ m_nUsed--; }

    INDCompletionQueue* ICq(){ return m_pICq.get(); }

    CAdapter* Adapter(){ return m_pAdapter.get(); }

private:
    static int WINAPI NotifySucceeded( __in EXOVERLAPPED* pOverlapped );
    static int WINAPI NotifyFailed( __in EXOVERLAPPED* pOverlapped );

private:
    volatile LONG m_nRef;

    LIST_ENTRY m_link;

    StackGuardRef<CAdapter> m_pAdapter;
    int m_Size;
    int m_nUsed;
    StackGuardRef<INDCompletionQueue> m_pICq;

    bool m_fArmed;
    EXOVERLAPPED m_NotifyOv;
};

}   // namespace v1
}   // namespace CH3_ND

#endif  // #define CH3U_NDV1_CQ_H
