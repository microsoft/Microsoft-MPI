// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd.h - Network Direct MPI CH3 Channel

--*/

#pragma once

#ifndef CH3U_ND_CQ_H
#define CH3U_ND_CQ_H


namespace CH3_ND
{

class Adapter;

class Cq
{
    friend class ListHelper<Cq>;

    LIST_ENTRY m_link;

    StackGuardRef<Adapter> m_pAdapter;
    StackGuardRef<IND2CompletionQueue> m_pICq;

    volatile LONG m_nRef;
    int m_Size;
    int m_nUsed;

    bool m_fArmed;
    ND_OVERLAPPED m_NotifyOv;

private:
    Cq();
    Cq( const Cq& rhs );
    ~Cq();
    int Init( __in Adapter& adapter );
    Cq& operator = ( const Cq& rhs );

public:
    static
    MPI_RESULT
    Create(
        _Inout_ Adapter& adapter,
        _Outptr_ Cq** ppCq
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

    IND2CompletionQueue* ICq(){ return m_pICq.get(); }

    Adapter* GetAdapter(){ return m_pAdapter.get(); }

private:
    static ND_OVERLAPPED::FN_CompletionRoutine NotifyHandler;
};

}   // namespace CH3_ND

#endif  // #define CH3U_ND_CQ_H
