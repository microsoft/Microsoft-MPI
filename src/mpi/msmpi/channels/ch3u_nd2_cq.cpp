// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_cq.cpp - Network Direct MPI CH3 Channel completion queue object

--*/

#include "precomp.h"
#include "ch3u_nd2.h"


#define MSMPI_ND_DEFAULT_CQ_SIZE 128

namespace CH3_ND
{


Cq::Cq() :
    m_nRef( 1 ),
    m_Size( 0 ),
    m_nUsed( 0 ),
    m_fArmed( false ),
    m_NotifyOv( NotifyHandler )
{
    m_link.Flink = m_link.Blink = &m_link;
}


Cq::~Cq()
{
}


int Cq::Init( __in Adapter& adapter )
{
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
    m_Size = min( m_Size, static_cast<int>( adapter.MaxEpPerCq() ) );
    DWORD nEntries = m_Size * g_NdEnv.CqDepthPerEp();

    HRESULT hr = adapter.IAdapter()->CreateCompletionQueue(
        IID_IND2CompletionQueue,
        adapter.GetOverlappedFile(),
        nEntries,
        0,
        0,
        reinterpret_cast<VOID**>( &m_pICq.ref() )
        );
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|create_cq %x", hr );
    }

    adapter.AddRef();
    m_pAdapter.attach( &adapter );
    return MPI_SUCCESS;
}


MPI_RESULT
Cq::Create(
    _Inout_ Adapter& adapter,
    _Outptr_ Cq** ppCq
    )
{
    StackGuardRef<Cq> pCq( new Cq() );
    if( pCq.get() == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    int mpi_errno = pCq->Init( adapter );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // The caller is responsible for releasing the object instantiation reference.
    //
    *ppCq = pCq.detach();
    return MPI_SUCCESS;
}


void Cq::Shutdown()
{
    MPIU_Assert( m_pICq.get() != NULL );
    if( m_fArmed == true )
    {
#pragma prefast(disable:25031, "Don't care about the status of cancel - no possible recovery.");
        m_pICq->CancelOverlappedRequests();
        //
        // Since the progress engine is dead, we'll never get a completion
        // for the Notify.  Just release its reference and move on.
        //
        Release();
    }
}


void
Cq::Release()
{
    MPIU_Assert( m_nRef > 0 );

    long value = ::InterlockedDecrement(&m_nRef);
    if( value != 0 )
    {
        return;
    }

    delete this;
}


int Cq::Poll(_Inout_ BOOL* pfProgress)
{
    SIZE_T nResults;
    int mpi_errno;

    for( ;; )
    {
        ND2_RESULT result;
        nResults = m_pICq->GetResults( &result, 1 );
        bool fMpiRequestDone = false;

        if( nResults == 0 )
        {
            return MPI_SUCCESS;
        }

        *pfProgress = TRUE;
        mpi_errno = Endpoint::CompletionHandler( &result, &fMpiRequestDone );
        if( mpi_errno != MPI_SUCCESS )
        {
            return MPIU_ERR_FAIL( mpi_errno );
        }

        if( fMpiRequestDone )
        {
            return MPI_SUCCESS;
        }
    }
}


int
Cq::Arm()
{
    if( m_fArmed )
    {
        return MPI_SUCCESS;
    }

    HRESULT hr = m_pICq->Notify( ND_CQ_NOTIFY_ANY, &m_NotifyOv );
    if( FAILED( hr ) )
    {
        return MPIU_E_ERR( "**ch3|nd|notify %x", hr );
    }
    if( hr == ND_PENDING )
    {
        //
        // NDv2 completion semantics will not report an immediate success
        // completion to the IOCP.  If we get ND_SUCCESS then the CQ was armed
        // and triggered immediately, and is thus no longer armed.
        //
        m_fArmed = true;
        AddRef();
    }

    return MPI_SUCCESS;
}


OACR_WARNING_DISABLE(28301, "PREfast does not handle FN types correctly.");
int
Cq::NotifyHandler(
    __in ND_OVERLAPPED* pOverlapped
    )
{
    StackGuardRef<Cq> pCq( CONTAINING_RECORD( pOverlapped, Cq, m_NotifyOv ) );

    pCq->m_fArmed = false;

    HRESULT hr = pCq->m_pICq->GetOverlappedResult(
        pOverlapped,
        FALSE
        );
    switch( hr )
    {
    case ND_SUCCESS:
        MPIDI_CH3I_Progress_spin_up();
        break;

    case ND_CANCELED:
        break;

    default:
        return MPIU_E_ERR( "**ch3|nd|notify %x", hr );
    }

    return MPI_SUCCESS;
}
OACR_WARNING_ENABLE(28301, "PREfast does not handle FN types correctly.");


}   // namespace CH3_ND


using namespace CH3_ND;


int
MPIDI_CH3I_Nd_start_write(
    __inout MPIDI_VC_t* vc,
    __inout MPID_Request* sreq
    )
{
    /* FIXME: the current code only agressively writes the first IOV.
    Eventually it should be changed to agressively write as much as possible.
    Ideally, the code would be shared between the send routines and the
    progress engine. */
    TraceSendNd_Inline(
        MPIDI_Process.my_pg_rank,
        vc->pg_rank,
        sreq->ch.msg_id,
        sreq->dev.iov_count,
        sreq->iov_size(),
        sreq->dev.pkt.type
        );

    MPIU_Bsize_t nb = 0;
    int mpi_errno = MPIDI_CH3I_Nd_writev(
        vc,
        sreq->iov(),
        sreq->dev.iov_count,
        &nb
        );
    ON_ERROR_FAIL(mpi_errno);

    MPIU_Assert( sreq->dev.iov_offset == 0 );
    if( !sreq->adjust_iov( nb ) )
    {
        MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);

        //
        // The ND sub-channel is either full, or wants to take the zCopy path.
        // Poke the ND sub-channel in case of the latter.
        //
        mpi_errno = MPIDI_CH3I_Nd_write_progress( vc );
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

        TraceSendNd_Continue(
            MPIDI_Process.my_pg_rank,
            vc->pg_rank,
            sreq->ch.msg_id,
            sreq->dev.iov_count,
            sreq->iov_size()
            );
    }
    else
    {
        TraceSendNd_Done(
            MPIDI_Process.my_pg_rank,
            vc->pg_rank,
            sreq->ch.msg_id
            );
    }

fn_exit:
fn_fail:
    return mpi_errno;
}


int
MPIDI_CH3I_Nd_writev(
    __inout MPIDI_VC_t* pVc,
    __inout const MPID_IOV* pIov,
    __in int nIov,
    __out MPIU_Bsize_t* pnBytes
    )
{
    MPIU_Assert( pVc != NULL );

    CH3_ND::Endpoint* pEp = pVc->ch.nd.pEp;
    MPIU_Assert( pEp );

    return pEp->Send( pIov, nIov, pnBytes );
}


int
MPIDI_CH3I_Nd_write_progress(
    __inout MPIDI_VC_t* pVc
    )
{
    MPIU_Assert( pVc );

    CH3_ND::Endpoint* pEp = pVc->ch.nd.pEp;
    MPIU_Assert( pEp );

    bool fMpiRequestDone;
    return pEp->ProcessSends( &fMpiRequestDone );
}


int
MPIDI_CH3I_Nd_progress(
    __out BOOL* pfProgress
    )
{
    return g_NdEnv.Poll(pfProgress);
}


MPI_RESULT
MPIDI_CH3I_Nd_enable_notification(
    _Out_ BOOL* pfProgress
    )
{
    int mpi_errno = g_NdEnv.Arm();
    if( mpi_errno == MPI_SUCCESS )
    {
        return g_NdEnv.Poll( pfProgress );
    }
    return mpi_errno;
}
