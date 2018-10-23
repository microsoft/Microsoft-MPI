// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_mw.h - Network Direct MPI CH3 Channel Memory Window

--*/

#pragma once


namespace CH3_ND
{
namespace v1
{

class CAdapter;
class CMr;
class CEndpoint;

class CMw
{
    friend class ListHelper<CMw>;
private:
    CMw();
    ~CMw();
    int Init( CAdapter* pAdapter );

public:
    static
    MPI_RESULT
    Create(
        _In_ CAdapter* pAdapter,
        _Outptr_ CMw** ppMw
        );

public:
    LONG AddRef()
    {
        return ::InterlockedIncrement(&m_nRef);
    }

    void Release();

    INDMemoryWindow* IMw(){ return m_pIMw.get(); }

    void
    Bind(
        __in CEndpoint* pEp,
        __in CMr* pMr,
        __in nd_result_t::CompletionRoutine pfnBindSucceeded,
        __in nd_result_t::CompletionRoutine pfnBindFailed,
        __in nd_result_t::CompletionRoutine pfnInvalidateSucceeded,
        __in nd_result_t::CompletionRoutine pfnInvalidateFailed
        );

    void Unbind();

    inline CEndpoint* Ep(){ return m_pEp.get(); }

    inline ND_RESULT* BindResult(){ return &m_BindResult; }
    inline ND_RESULT* InvalidateResult(){ return &m_InvalidateResult; }

    static
    inline
    CMw*
    FromBindResult(
        __in nd_result_t* pResult
        )
    {
        return CONTAINING_RECORD( pResult, CMw, m_BindResult );
    }

    static
    inline
    CMw*
    FromInvalidateResult(
        __in nd_result_t* pResult
        )
    {
        return CONTAINING_RECORD( pResult, CMw, m_InvalidateResult );
    }

private:
    LIST_ENTRY m_link;

    volatile LONG m_nRef;

    StackGuardRef<INDMemoryWindow> m_pIMw;

    nd_result_t m_BindResult;
    //
    // The ND SPI requires that an invalidate result be provided when creating a MW,
    // for use when using Send w/ invalidate.  We don't use send w/ invalidate, though
    // we do use this same result for explicit invalidation.
    //
    nd_result_t m_InvalidateResult;

    StackGuardRef<CMr> m_pMr;
    StackGuardRef<CEndpoint> m_pEp;

};

}   // namespace v1
}   // namespace CH3_ND
