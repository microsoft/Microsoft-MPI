// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


int
MPIDI_CH3I_SHM_start_write(
    _In_ MPIDI_VC_t*    vc,
    _In_ MPID_Request*  sreq
    )
{
    /* FIXME: the current code only agressively writes the first IOV.  Eventually it should be changed to agressively write
    as much as possible.  Ideally, the code would be shared between the send routines and the progress engine. */
    TraceSendShm_Inline(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size(), sreq->dev.pkt.type);

    int mpi_errno = MPI_SUCCESS;
    MPIU_Bsize_t nb = 0;
    MPIDI_CH3I_SHM_writev(vc, sreq->iov(), sreq->dev.iov_count, &nb);

    MPIU_Assert( sreq->dev.iov_offset == 0 );
    if( !sreq->adjust_iov( nb ) )
    {
        MPIDI_CH3I_SendQ_enqueue_unsafe(vc, sreq);

        //
        // The shm sub-channel is eiter full, or wants to take the RMA path.
        // Poke the shm sub-channel in case of the latter.
        //
        BOOL fProgress;
        mpi_errno = MPIDI_CH3I_SHM_write_progress_vc(vc, &fProgress);
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

        TraceSendShm_Continue(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id, sreq->dev.iov_count, sreq->iov_size());
    }
    else
    {
        TraceSendShm_Done(MPIDI_Process.my_pg_rank, vc->pg_rank, sreq->ch.msg_id);
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


//
// This function will process the sendq for the specified VC.
// NOTE: The SendQ lock must be held.
//

int MPIDI_CH3I_SHM_write_progress_vc(
    _In_ MPIDI_VC_t * vc,
    _Out_ BOOL* pfProgress
    )
{
    int mpi_errno = MPI_SUCCESS;
    *pfProgress = FALSE;
    MPIU_Bsize_t nb;
    int complete;
    MPID_Request* req;

    req = MPIDI_CH3I_SendQ_head_unsafe(vc);
    while (req != nullptr)
    {
        if(req->dev.iov_offset >= 0)
        {
            MPIDI_CH3I_SHM_writev_rma(vc, req->dev.iov + req->dev.iov_offset, req->dev.iov_count - req->dev.iov_offset, &nb, TRUE);

            if(nb == 0)
            {
                return MPI_SUCCESS;
            }

            if( !req->adjust_iov( nb ) )
            {
                MPIU_Assert(req->dev.iov_offset < req->dev.iov_count);
                *pfProgress = TRUE;
                return MPI_SUCCESS;
            }
        }

        if(vc->ch.shm.send.wait_for_rma)
        {
            //
            // The write is complete, but we need to wait for the RMA operation to complete (the
            // other side reads the message); wait until the shm queue is completely empty.
            //
            if(!shm_is_empty(vc->ch.shm.send.shm))
            {
                //
                // Use the iov_offset field to mark that this request is still in progress
                // but no write is required
                //
                req->dev.iov_offset = -1;
                return MPI_SUCCESS;
            }

            vc->ch.shm.send.wait_for_rma = FALSE;
        }

        /* Write operation complete */
        mpi_errno = MPIDI_CH3U_Handle_send_req(vc, req, &complete);
        ON_ERROR_FAIL(mpi_errno);


        if (!complete)
        {
            TraceSendShm_Continue(MPIDI_Process.my_pg_rank, vc->pg_rank, req->ch.msg_id, req->dev.iov_count, req->iov_size());
            req->dev.iov_offset = 0;
            continue;
        }

        TraceSendShm_Done(MPIDI_Process.my_pg_rank, vc->pg_rank, req->ch.msg_id);
        MPIDI_CH3I_SendQ_dequeue_unsafe(vc);

        req = MPIDI_CH3I_SendQ_head_unsafe(vc);
        if(req == nullptr)
        {
            return MPI_SUCCESS;
        }

        TraceSendShm_Head(MPIDI_Process.my_pg_rank, vc->pg_rank, req->ch.msg_id, req->dev.iov_count, req->iov_size(), req->dev.pkt.type);
    }

fn_fail:
    return mpi_errno;

}


int MPIDI_CH3I_SHM_write_progress(
        _In_ MPIDI_VC_t * vcChain,
        _Out_ BOOL* pfProgress
        )
{
    int mpi_errno = MPI_SUCCESS;
    *pfProgress = FALSE;

    for( MPIDI_VC_t * vc = vcChain; vc != NULL; vc = vc->ch.shm.send.next_vc)
    {
        SendQLock lock(vc);

        if (MPIDI_CH3I_SendQ_empty_unsafe(vc) == false)
        {
            mpi_errno = MPIDI_CH3I_SHM_write_progress_vc(vc, pfProgress);
            break;
        }
    }
    return mpi_errno;
}
