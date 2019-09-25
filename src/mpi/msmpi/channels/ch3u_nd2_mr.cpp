// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_mr.cpp - Network Direct MPI CH3 Channel Memory Registration

--*/

#include "precomp.h"
#include "ch3u_nd2.h"

namespace CH3_ND
{

Mr::Mr() :
    m_nRef( 1 ),
    m_pBuf( NULL ),
    m_Length( 0 ),
    m_pMr( NULL )
{
    m_link.Flink = m_link.Blink = &m_link;
}


Mr::~Mr()
{
    if( !Stale() )
    {
        int mpi_errno = m_pAdapter->DeregisterMemory( m_pMr );
        MPIU_Assert( mpi_errno == MPI_SUCCESS );
        UNREFERENCED_PARAMETER( mpi_errno );
    }
}


static void
RoundToPages(
    __in const char** ppBuf,
    __in SIZE_T* pcbBuf
    )
{
    //
    // Round pBuf down to page boundary.
    // Round Length up to multiple of page size.
    // Assume 4KB page size.
    //
    SIZE_T offset = reinterpret_cast<ULONG_PTR>(*ppBuf) & 0xFFF;
    *ppBuf -= offset;
    *pcbBuf += offset + 0xFFF;
    *pcbBuf &= ~0xFFFULL;
}


int
Mr::Init(
    _In_ Adapter& adapter,
    _In_reads_bytes_(cbBuf) const char* pBuf,
    _In_ SIZE_T cbBuf
    )
{
    RoundToPages( &pBuf, &cbBuf );

    int mpi_errno = adapter.RegisterMemory(
        pBuf,
        cbBuf,
        ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_READ,
        &m_pMr
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    adapter.AddRef();
    m_pAdapter.attach( &adapter );

    m_pBuf = pBuf;
    m_Length = cbBuf;
    return MPI_SUCCESS;
}


MPI_RESULT
Mr::Create(
    _In_ Adapter& adapter,
    _In_reads_(cbBuf)  const char* pBuf,
    _In_ SIZE_T cbBuf,
    _Outptr_ Mr** ppMr
    )
{
    StackGuardRef<Mr> pMr( new Mr() );
    if( pMr.get() == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    int mpi_errno = pMr->Init( adapter, pBuf, cbBuf );
    if( mpi_errno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL( mpi_errno );
    }

    *ppMr = pMr.detach();
    return MPI_SUCCESS;
}


void
Mr::Shutdown()
{
    MPIU_Assert( Idle() );
    MPIU_Assert( !Stale() );

    int mpi_errno = m_pAdapter->DeregisterMemory( m_pMr );
    MPIU_Assert( mpi_errno == MPI_SUCCESS );
    UNREFERENCED_PARAMETER( mpi_errno );
    m_pMr = NULL;

    m_pAdapter.free();
}


void
Mr::Release()
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
Mr::Matches(
    const Adapter& adapter,
    const char* pBuf,
    SIZE_T cbBuf
    )
{
    if( m_pAdapter.get() != &adapter )
    {
        return false;
    }

    RoundToPages( &pBuf, &cbBuf );

    if( pBuf != m_pBuf )
    {
        return false;
    }

    if( cbBuf != m_Length )
    {
        return false;
    }

    return true;
}


bool
Mr::Overlaps(
    const char* pBuf,
    SIZE_T cbBuf
    ) const
{
    if( Stale() )
    {
        return false;
    }

    if( pBuf >= m_pBuf + m_Length )
    {
        return false;
    }

    if( pBuf + cbBuf <= m_pBuf )
    {
        return false;
    }

    return true;
}

}   // namespace CH3_ND
