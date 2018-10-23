// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_cq.cpp - Network Direct MPI CH3 Channel completion queue object

--*/

#include "precomp.h"
#include "ch3u_nd1.h"


#define MSMPI_ND_DEFAULT_CQ_SIZE 128

namespace CH3_ND
{
namespace v1
{


CCq::CCq() :
    m_nRef( 1 ),
    m_Size( 0 ),
    m_nUsed( 0 ),
    m_fArmed( false )
{
    ExInitOverlapped( &m_NotifyOv, NotifySucceeded, NotifyFailed );
}


CCq::~CCq()
{
}


int CCq::Init( __in CAdapter* pAdapter )
{
    MPIU_Assert( pAdapter );

    //
    //TODO: Make world size an input into the channel init function if available.
    //
    //
    // We only size for connections to other ranks (we will never connect to our self)
    //
    m_Size = PMI_Get_size() - 1;

    if( m_Size == 0 )
    {
        //
        // If this process is a singleton, we start with a default and
        // can create more CQ's later if there are more connections created
        // through dynamic process
        //
        m_Size = MSMPI_ND_DEFAULT_CQ_SIZE;
    }

    //
    // Cap our size based on what the HW can support.
    //
    m_Size = static_cast<int>( min( (SIZE_T)m_Size, pAdapter->MaxEpPerCq() ) );
    SIZE_T nEntries = (SIZE_T)m_Size * NDv1_CQ_ENTRIES_PER_EP;

    HRESULT hr = pAdapter->IAdapter()->CreateCompletionQueue( nEntries, &m_pICq.ref() );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|create_cq %x", hr );

    pAdapter->AddRef();
    m_pAdapter.attach( pAdapter );
    return MPI_SUCCESS;
}


MPI_RESULT
CCq::Create(
    _In_ CAdapter* pAdapter,
    _Outptr_ CCq** ppCq
    )
{
    StackGuardRef<CCq> pCq( new CCq() );
    if( pCq.get() == NULL )
        return MPIU_ERR_NOMEM();

    int mpi_errno = pCq->Init( pAdapter );
    if( mpi_errno != MPI_SUCCESS )
        return mpi_errno;

    //
    // The caller is responsible for releasing the object instantiation reference.
    //
    *ppCq = pCq.detach();
    return MPI_SUCCESS;
}


void CCq::Shutdown()
{
    MPIU_Assert( m_pICq.get() != NULL );
    if( m_fArmed == true )
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here.");
        m_pICq->CancelOverlappedRequests();
        //
        // Since the progress engine is dead, we'll never get a completion
        // for the Notify.  Just release its reference and move on.
        //
        Release();
    }
}


void
CCq::Release()
{
    MPIU_Assert( m_nRef > 0 );

    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}


int CCq::Poll(_Inout_ BOOL* pfProgress)
{
    SIZE_T nResults;
    int mpi_errno;

    for( ;; )
    {
        ND_RESULT* pNdResult;
        nResults = m_pICq->GetResults( &pNdResult, 1 );
        bool fMpiRequestDone = false;

        if( nResults == 0 )
            return MPI_SUCCESS;

        *pfProgress = TRUE;
        nd_result_t* pResult = static_cast<nd_result_t*>( pNdResult );

        if( pResult->Status == ND_SUCCESS )
        {
            mpi_errno = pResult->pfnSucceeded( pResult, &fMpiRequestDone );
        }
        else
        {
            mpi_errno = pResult->pfnFailed( pResult, &fMpiRequestDone );
        }

        if( mpi_errno != MPI_SUCCESS )
            return MPIU_ERR_FAIL( mpi_errno );

        if( fMpiRequestDone )
            return MPI_SUCCESS;
    }
}


int
CCq::Arm()
{
    if( m_fArmed )
        return MPI_SUCCESS;

    HRESULT hr = m_pICq->Notify( ND_CQ_NOTIFY_ANY, &m_NotifyOv.ov );
    if( FAILED( hr ) )
        return MPIU_E_ERR( "**ch3|nd|notify %x", hr );

    m_fArmed = true;
    AddRef();
    return MPI_SUCCESS;
}


int
WINAPI
CCq::NotifySucceeded(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CCq> pCq( CONTAINING_RECORD( pOverlapped, CCq, m_NotifyOv ) );

    pCq->m_fArmed = false;

    MPIDI_CH3I_Progress_spin_up();
    return MPI_SUCCESS;
}


int
WINAPI
CCq::NotifyFailed(
    __in EXOVERLAPPED* pOverlapped
    )
{
    StackGuardRef<CCq> pCq( CONTAINING_RECORD( pOverlapped, CCq, m_NotifyOv ) );

    pCq->m_fArmed = false;

    SIZE_T nBytesRet;
    HRESULT hr = pCq->m_pICq->GetOverlappedResult( &pCq->m_NotifyOv.ov, &nBytesRet, FALSE );

    if( hr != ND_CANCELED )
        return MPIU_E_ERR( "**ch3|nd|notify %x", hr );

    return MPI_SUCCESS;
}


}   // namespace v1
}   // namespace CH3_ND


int
MPIDI_CH3I_Ndv1_start_write(
    __inout MPIDI_VC_t* vc,
    __inout MPID_Request* sreq
    )
{
    /* FIXME: the current code only agressively writes the first IOV.  Eventually it should be changed to agressively write
    as much as possible.  Ideally, the code would be shared between the send routines and the progress engine. */
    TraceSendNd_Inline(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);
    MPIU_Bsize_t nb = 0;
    int mpi_errno = MPIDI_CH3I_Ndv1_writev(vc, sreq->iov(), sreq->dev.iov_count, &nb);
    ON_ERROR_FAIL(mpi_errno);

    MPIU_Assert( sreq->dev.iov_offset == 0 );
    if( !sreq->adjust_iov( nb ) )
    {
        MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);

        //
        // The ND sub-channel is either full, or wants to take the zCopy path.
        // Poke the ND sub-channel in case of the latter.
        //
        mpi_errno = MPIDI_CH3I_Ndv1_write_progress( vc );
        ON_ERROR_FAIL(mpi_errno);
        goto fn_exit;
    }

    int complete;
    mpi_errno = MPIDI_CH3U_Handle_send_req(vc, sreq, &complete);
    ON_ERROR_FAIL(mpi_errno);

    if (complete == FALSE)
    {
        sreq->dev.iov_offset = 0;

        MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);

        //
        // Let the progress engine continue the send
        //

        TraceSendNd_Continue(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size());
    }
    else
    {
        TraceSendNd_Done(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


int
MPIDI_CH3I_Ndv1_writev(
    __inout MPIDI_VC_t* pVc,
    __inout const MPID_IOV* pIov,
    __in int nIov,
    __out MPIU_Bsize_t* pnBytes
    )
{
    MPIU_Assert( pVc != NULL );

    CH3_ND::v1::CEndpoint* pEp = pVc->ch.nd.pEpV1;
    MPIU_Assert( pEp );

    return pEp->Send( pIov, nIov, pnBytes );
}


int
MPIDI_CH3I_Ndv1_write_progress(
    __inout MPIDI_VC_t* pVc
    )
{
    MPIU_Assert( pVc );

    CH3_ND::v1::CEndpoint* pEp = pVc->ch.nd.pEpV1;
    MPIU_Assert( pEp );

    bool fMpiRequestDone;
    return pEp->ProcessSends( &fMpiRequestDone );
}


int
MPIDI_CH3I_Ndv1_progress(
    __out BOOL* pfProgress
    )
{
    return CH3_ND::v1::g_NdEnv.Poll(pfProgress);
}


int
MPIDI_CH3I_Ndv1_enable_notification(
    void
    )
{
    return CH3_ND::v1::g_NdEnv.Arm();
}
