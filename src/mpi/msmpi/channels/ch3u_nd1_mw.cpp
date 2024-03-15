// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_mw.cpp - Network Direct MPI CH3 Channel Memory Window

--*/


#include "precomp.h"
#include "ch3u_nd1.h"

namespace CH3_ND
{
namespace v1
{


CMw::CMw() :
    m_nRef( 1 )
{
    m_BindResult.pfnSucceeded = NULL;
    m_BindResult.pfnFailed = NULL;

    m_InvalidateResult.pfnSucceeded = NULL;
    m_InvalidateResult.pfnFailed = NULL;
}


CMw::~CMw()
{
}


int
CMw::Init(
    CAdapter* pAdapter
    )
{
    HRESULT hr = pAdapter->IAdapter()->CreateMemoryWindow(
        &m_InvalidateResult,
        &m_pIMw.ref()
        );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|create_mw %x", hr );

    return MPI_SUCCESS;
}


MPI_RESULT
CMw::Create(
    _In_ CAdapter* pAdapter,
    _Outptr_ CMw** ppMw
    )
{
    StackGuardRef<CMw> pMw( new CMw() );
    if( pMw.get() == NULL )
        return MPIU_ERR_NOMEM();

    int mpi_errno = pMw->Init( pAdapter );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    *ppMw = pMw.detach();
    return MPI_SUCCESS;
}


void
CMw::Release()
{
    MPIU_Assert( m_nRef > 0 );

    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}


void
CMw::Bind(
    __in CEndpoint* pEp,
    __in CMr* pMr,
    __in nd_result_t::CompletionRoutine pfnBindSucceeded,
    __in nd_result_t::CompletionRoutine pfnBindFailed,
    __in nd_result_t::CompletionRoutine pfnInvalidateSucceeded,
    __in nd_result_t::CompletionRoutine pfnInvalidateFailed
    )
{
    MPIU_Assert( m_pEp.get() == NULL );
    pEp->AddRef();
    m_pEp.attach( pEp );

    pMr->AddRef();
    m_pMr.attach( pMr );

    m_BindResult.pfnSucceeded = pfnBindSucceeded;
    m_BindResult.pfnFailed = pfnBindFailed;
    m_InvalidateResult.pfnSucceeded = pfnInvalidateSucceeded;
    m_InvalidateResult.pfnFailed = pfnInvalidateFailed;
}


void
CMw::Unbind()
{
    MPIU_Assert( m_pEp.get() != NULL );
    m_pEp.free();

    m_pMr.free();

    m_BindResult.pfnSucceeded = NULL;
    m_BindResult.pfnFailed = NULL;
    m_InvalidateResult.pfnSucceeded = NULL;
    m_InvalidateResult.pfnFailed = NULL;
}

}   // namespace v1
}   // namespace CH3_ND
