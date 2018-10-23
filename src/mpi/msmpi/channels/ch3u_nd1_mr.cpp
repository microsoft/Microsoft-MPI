// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_mr.cpp - Network Direct MPI CH3 Channel Memory Registration

--*/

#include "precomp.h"
#include "ch3u_nd1.h"


namespace CH3_ND
{
namespace v1
{

CMr::CMr() :
    m_nRef( 1 ),
    m_pBuf( NULL ),
    m_Length( 0 ),
    m_hMr( NULL )
{
}


CMr::~CMr()
{
    if( !Stale() )
    {
        int mpi_errno = m_pAdapter->DeregisterMemory( m_hMr );
        MPIU_Assert( mpi_errno == MPI_SUCCESS );
        UNREFERENCED_PARAMETER( mpi_errno );
    }
}


static void
RoundToPages(
    __in const char** ppBuf,
    __in SIZE_T* pLength
    )
{
    //
    // Round pBuf down to page boundary.
    // Round Length up to multiple of page size.
    // Assume 4KB page size.
    //
    SIZE_T Offset = (ULONG_PTR)*ppBuf & 0xFFF;
    *ppBuf -= Offset;
    *pLength += Offset + 0xFFF;
    *pLength &= ~0xFFFULL;
}


int
CMr::Init(
    _In_ CAdapter* pAdapter,
    _In_reads_(Length) const char* pBuf,
    SIZE_T Length 
    )
{
    MPIU_Assert( pAdapter );

    RoundToPages( &pBuf, &Length );

    int mpi_errno = pAdapter->RegisterMemory( pBuf, Length, &m_hMr );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    pAdapter->AddRef();
    m_pAdapter.attach( pAdapter );

    m_pBuf = pBuf;
    m_Length = Length;
    return MPI_SUCCESS;
}


MPI_RESULT
CMr::Create(
    _In_ CAdapter* pAdapter,
    _In_reads_(Length) const char* pBuf,
    _In_ SIZE_T Length,
    _Outptr_ CMr** ppMr
    )
{
    StackGuardRef<CMr> pMr( new CMr() );
    if( pMr.get() == NULL )
        return MPIU_ERR_NOMEM();

    int mpi_errno = pMr->Init( pAdapter, pBuf, Length );
    if( mpi_errno != MPI_SUCCESS )
        return MPIU_ERR_FAIL( mpi_errno );

    *ppMr = pMr.detach();
    return MPI_SUCCESS;
}


void
CMr::Shutdown()
{
    MPIU_Assert( Idle() );
    MPIU_Assert( !Stale() );

    int mpi_errno = m_pAdapter->DeregisterMemory( m_hMr );
    MPIU_Assert( mpi_errno == MPI_SUCCESS );
    UNREFERENCED_PARAMETER( mpi_errno );

    m_pAdapter.free();
}


void
CMr::Release()
{
    MPIU_Assert( m_nRef > 0 );

    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}


bool
CMr::Matches(
    const CAdapter* pAdapter,
    const char* pBuf,
    SIZE_T Length
    )
{
    if( m_pAdapter.get() != pAdapter )
        return false;

    RoundToPages( &pBuf, &Length );

    if( pBuf != m_pBuf )
        return false;

    if( Length != m_Length )
        return false;

    return true;
}


bool
CMr::Overlaps(
    const char* pBuf,
    SIZE_T Length
    ) const
{
    if( Stale() )
        return false;

    if( pBuf >= m_pBuf + m_Length )
        return false;

    if( pBuf + Length <= m_pBuf )
        return false;

    return true;
}

}   // namespace v1
}   // namespace CH3_ND
