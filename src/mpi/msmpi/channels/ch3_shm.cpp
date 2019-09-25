// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


//
// Calc the slot overhead as the part of the slot stuct before the payload
// Make sure that the overhead is smaller than the allocated slot size
//
#define SLOT_OVERHEAD offsetof(MPIDI_CH3I_SHM_Slot_t, data)
C_ASSERT(SLOT_OVERHEAD < sizeof(MPIDI_CH3I_SHM_Slot_t));


static inline MPIU_Bsize_t slots_to_bytes(int slots)
{
    return (slots * sizeof(MPIDI_CH3I_SHM_Slot_t) - SLOT_OVERHEAD);
}


static inline int bytes_to_slots(MPIU_Bsize_t size)
{
    size += SLOT_OVERHEAD + sizeof(MPIDI_CH3I_SHM_Slot_t) - 1;
    size /= sizeof(MPIDI_CH3I_SHM_Slot_t);
    return size;
}


static inline BOOL use_rma(MPIU_Bsize_t size)
{
    //
    // Use RMA if the size is bigger than the max slot size
    //
    return (size > SHM_MAX_SLOT_SIZE - SLOT_OVERHEAD);
}


//
// next_slot_and_notify
//
// move the index to the next slot and notify the other side if needed
//
static
inline
void
next_slot_and_notify(
    MPIDI_CH3I_SHM_Index_t* t,
    const MPIDI_CH3I_SHM_Index_t* o,
    MPIDI_CH3I_BootstrapQ queue,
    MPIU_Bsize_t size
    )
{
    int oni;

    //
    // if rma was used, only a pointer size data was actually written.
    //
    if(use_rma(size))
    {
        size = sizeof(void*);
    }

    //
    // Make sure that this index is visible to the other processor before reading the other side
    // notify_index. The other side first updates its notify_index before reading this index. Which
    // guarantee that the other side will detect any skipped notification.
    //
    t->index += bytes_to_slots(size);
    MPID_READ_WRITE_BARRIER();

    oni = o->notify_index;
    if(t->notify_capture != oni)
    {
        t->notify_capture = oni;
        MPIDI_CH3I_Notify_message(queue);
    }
}


static inline MPIDI_CH3I_SHM_Slot_t* shm_slot(MPIDI_CH3I_SHM_Queue_t* shm, int index)
{
    return &shm->slot[index % _countof(shm->slot)];
}


static inline MPIU_Bsize_t shm_send_buffer_avail(MPIDI_CH3I_SHM_Queue_t* shm, const MPIDI_CH3I_SHM_Slot_t* t, const MPIDI_CH3I_SHM_Slot_t* o)
{
    if(t >= o)
    {
        o = &shm->slot[_countof(shm->slot)];
    }
    MPIU_Assert( (o - t) < SHM_MAX_SLOT_SIZE );
    return min(
        slots_to_bytes(static_cast<int>(o - t)),
        SHM_MAX_SLOT_SIZE - SLOT_OVERHEAD);
}


void MPIDI_CH3I_SHM_writev_rma(MPIDI_VC_t *vc, const MPID_IOV* iov, int iov_n, MPIU_Bsize_t* num_bytes_ptr, BOOL fUseRma)
{
    int iv = 0;
    MPIU_Bsize_t total = 0;
    MPIDI_CH3I_SHM_Queue_t* shm = vc->ch.shm.send.shm;

    const char * pSrc = iov[iv].buf;
    MPIU_Bsize_t nSrc = iov[iv].len;

    while(iv < iov_n && !shm_is_full(shm)
        && (fUseRma || !use_rma(nSrc))
        )
    {
        MPIDI_CH3I_SHM_Slot_t* s = shm_slot(shm, shm->send.index);
        const MPIDI_CH3I_SHM_Slot_t* o = shm_slot(shm, shm->recv.index);
        MPIU_Bsize_t nSize = shm_send_buffer_avail(shm, s, o);
        MPIU_Bsize_t nDst = nSize;
        char* pDst = s->data;
        BOOL fRma = FALSE;

        for(;;)
        {
            if(!use_rma(nSrc))
            {
                if(fRma)
                    break;
                if(nSrc > nDst)
                {
                    memcpy(pDst, pSrc, nDst);
                    pSrc += nDst;
                    nSrc -= nDst;
                    nDst = 0;
                    break;
                }

                memcpy(pDst, pSrc, nSrc);
                pDst += nSrc;
                nDst -= nSrc;
            }
            else
            {
                if(!fUseRma)
                    break;

                if(nDst != nSize)
                    break;

                fRma = TRUE;
                vc->ch.shm.send.wait_for_rma = TRUE;
                *(const void**)pDst = pSrc;
                nDst -= nSrc;
            }

            iv++;
            if(iv == iov_n)
                break;

            pSrc = iov[iv].buf;
            nSrc = iov[iv].len;
        }

        nDst = nSize - nDst;
        total += nDst;
        s->num_bytes = nDst;

        //
        // Make sure that slot data write to memory is complete before marking the slot as used
        //
        MPID_WRITE_BARRIER();
        next_slot_and_notify(&shm->send, &shm->recv, vc->ch.shm.queue, nDst);
    }

    *num_bytes_ptr = total;
}


static inline void copy_buffer(
    _Out_writes_bytes_(size) void* pDst,
    _In_  const void* pSrc, 
    _In_ MPIU_Bsize_t size, 
    HANDLE hProcess
    )
{
    BOOL fSucc;

    if(hProcess == NULL)
    {
        memcpy(pDst, pSrc, size);
        return;
    }

    fSucc = ReadProcessMemory(hProcess, pSrc, pDst, size, NULL);
    MPIU_Assert(fSucc);
}


static inline MPIU_Bsize_t copy_to_iov(const char* buffer, MPIU_Bsize_t size, MPID_IOV* iov, int iov_count, int* piov_offset, HANDLE hProcess)
{
    MPIU_Bsize_t nb = size;
    MPID_IOV* p = &iov[*piov_offset];
    const MPID_IOV* end = &iov[iov_count];

    while(p < end && nb > 0)
    {
        MPIU_Bsize_t iov_len = p->len;

        if(iov_len <= nb)
        {
            copy_buffer(p->buf, buffer, iov_len, hProcess);
            nb -= iov_len;
            buffer += iov_len;
            p++;
        }
        else
        {
            copy_buffer(p->buf, buffer, nb, hProcess);
            p->buf = (char*)p->buf + nb;
            p->len -= nb;
            nb = 0;
        }
    }

    //
    // It's safe to cast to int here because p is bound by iov + iov_count
    // and iov_count is an int
    //
    *piov_offset = static_cast<int>(p - iov); // next offset to process
    return (size - nb); // number of bytes copied
}


static inline const char* slot_recv_buffer(MPIDI_CH3I_SHM_Slot_t* s)
{
    if(!use_rma(s->num_bytes))
    {
        return s->data;
    }

    return *(const char**)s->data;
}


int MPIDI_CH3I_SHM_read_progress(MPIDI_VC_t* vc, BOOL* pfProgress)
{
    int mpi_errno;
    int complete;
    const char * pSrc;
    MPIU_Bsize_t nSrc;

    for( ; vc != NULL; vc = vc->ch.shm.recv.next_vc)
    {
        MPID_Request* rreq = vc->ch.shm.recv.request;
        MPIDI_CH3I_SHM_Queue_t* shm = vc->ch.shm.recv.shm;

next_slot:
        while(!shm_is_empty(shm))
        {
            MPIDI_CH3I_SHM_Slot_t* s = shm_slot(shm, shm->recv.index);
            pSrc = slot_recv_buffer(s) + vc->ch.shm.recv.slot_offset;
            nSrc = s->num_bytes - vc->ch.shm.recv.slot_offset;

            *pfProgress = TRUE;
            if(rreq == NULL)
            {
                //
                // An mpi message must start in a new slot
                //
                MPIU_Assert(vc->ch.shm.recv.slot_offset == 0);
                MPIU_Assert(nSrc >= sizeof(MPIDI_CH3_Pkt_t));
                MPIU_Assert(!use_rma(nSrc));

                vc->ch.n_recv++;
                TraceRecvShm_Packet(vc->pg_rank, MPIDI_Process.my_pg_rank, vc->ch.n_recv, ((MPIDI_CH3_Pkt_t*)pSrc)->type);
                mpi_errno = MPIDI_CH3U_Handle_recv_pkt(vc, reinterpret_cast<const MPIDI_CH3_Pkt_t*>(pSrc), &rreq);
                ON_ERROR_FAIL(mpi_errno);

                if(rreq == NULL)
                {
                    //
                    // An mpi message without data; free the slot and return.
                    // Make sure that reading slot data is complete before marking this slot as free
                    //
                    MPIU_Assert(s->num_bytes == sizeof(MPIDI_CH3_Pkt_t));
                    vc->ch.shm.recv.slot_offset = 0;
                    MPID_READ_BARRIER();
                    next_slot_and_notify(&shm->recv, &shm->send, vc->ch.shm.queue, sizeof(MPIDI_CH3_Pkt_t));

                    TraceRecvShm_Done(vc->pg_rank, MPIDI_Process.my_pg_rank, vc->ch.n_recv);
                    return MPI_SUCCESS;
                }

                nSrc -= sizeof(MPIDI_CH3_Pkt_t);
                pSrc += sizeof(MPIDI_CH3_Pkt_t);

                rreq->dev.iov_offset = 0;
                vc->ch.shm.recv.request = rreq;
            }

            for(;;)
            {
                MPIU_Bsize_t nCopy;
                nCopy = copy_to_iov(
                            pSrc,
                            nSrc,
                            rreq->dev.iov,
                            rreq->dev.iov_count,
                            &rreq->dev.iov_offset,
                            use_rma(s->num_bytes) ? vc->ch.shm.recv.hProcess : NULL
                            );

                nSrc -= nCopy;
                pSrc += nCopy;

                if(nSrc > 0)
                {
                    //
                    // More data in this slot, we must have filled the iov
                    //
                    MPIU_Assert(rreq->dev.iov_offset == rreq->dev.iov_count);
                    vc->ch.shm.recv.slot_offset = s->num_bytes - nSrc;
                    break;
                }

                //
                // No more data in this slot; free it.
                // Make sure that reading slot data is complete before marking this slot as free
                //
                vc->ch.shm.recv.slot_offset = 0;
                MPID_READ_BARRIER();
                next_slot_and_notify(&shm->recv, &shm->send, vc->ch.shm.queue, s->num_bytes);

                if(rreq->dev.iov_offset == rreq->dev.iov_count)
                    break;

                goto next_slot;
            }

            MPIU_Assert(rreq->dev.iov_offset == rreq->dev.iov_count);

            TraceRecvShm_Data(vc->pg_rank, MPIDI_Process.my_pg_rank, vc->ch.n_recv);
            mpi_errno = MPIDI_CH3U_Handle_recv_req(vc, vc->ch.shm.recv.request, &complete);
            ON_ERROR_FAIL(mpi_errno);

            if(complete)
            {
                //
                // The entire message was read, return to caller.
                //
                vc->ch.shm.recv.request = NULL;
                TraceRecvShm_Done(vc->pg_rank, MPIDI_Process.my_pg_rank, vc->ch.n_recv);
                return MPI_SUCCESS;
            }

            //
            // The iov was reloaded, reset the offset and go read the next data chunk
            //
            rreq->dev.iov_offset = 0;
        }
    }

    return MPI_SUCCESS;

fn_fail:
    return mpi_errno;
}
