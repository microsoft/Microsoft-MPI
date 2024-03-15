// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#if !defined(MPICH_MPIDI_CH3_IMPL_H_INCLUDED)
#define MPICH_MPIDI_CH3_IMPL_H_INCLUDED

#include "mpidimpl.h"
#include "pmi.h"
#include "mpisock.h"
#include "ch3u_sock.h"
#include "ch3u_sshm.h"
#include "ch3u_nd.h"
#include "ch3i_progress.h"
#include <process.h>

#if defined(__cplusplus)
extern "C" {
#endif

void _ReadBarrier(void);
void _ReadWriteBarrier(void);
void _WriteBarrier(void);

#if defined(__cplusplus)
}
#endif

#pragma intrinsic(_ReadBarrier)
#pragma intrinsic(_ReadWriteBarrier)
#pragma intrinsic(_WriteBarrier)


#define MPID_WRITE_BARRIER() _WriteBarrier()
#define MPID_READ_BARRIER() _ReadBarrier()
#define MPID_READ_WRITE_BARRIER() MemoryBarrier()

//
// Set the minimum slot size to be cache aligned
// Make sure that a complete packet header fits in the minimum slot size
//
#define SHM_MIN_SLOT_SIZE SYSTEM_CACHE_ALIGNMENT_SIZE
C_ASSERT(SHM_MIN_SLOT_SIZE >= sizeof(MPIDI_CH3_Pkt_t));

//
// Set the maximum slot size to be page size
//
#define SHM_MAX_SLOT_SIZE (4 * 1024)
C_ASSERT(SHM_MAX_SLOT_SIZE >= SHM_MIN_SLOT_SIZE);


struct MPIDI_CH3I_SHM_Slot_t
{
    MPIU_Bsize_t num_bytes;
    char data[SHM_MIN_SLOT_SIZE - __alignof(MPIU_Bsize_t)];
};


struct MPIDI_CH3I_SHM_Index_t
{
    volatile int index;
    volatile int notify_index;
    int notify_capture;
};


//
// The number of slots that fit in 32K memory
//
#define MPIDI_CH3I_NUM_SLOTS ((32 * 1024) / sizeof(MPIDI_CH3I_SHM_Slot_t))

struct MPIDI_CH3I_SHM_Queue_t
{
    //
    // Version comes first, so that we can change things after it without potentially breaking things.
    //
    UINT32 ver;
    int pg_rank;
    GUID pg_id;
    DECLSPEC_CACHEALIGN
    MPIDI_CH3I_SHM_Index_t recv;    // on a seperate cache line
    DECLSPEC_CACHEALIGN
    MPIDI_CH3I_SHM_Index_t send;    // on a seperate cache line
    DECLSPEC_CACHEALIGN
    MPIDI_CH3I_SHM_Slot_t slot[MPIDI_CH3I_NUM_SLOTS];
};

//
// Make sure that the largest slot fits in the slots buffer
//
C_ASSERT(SHM_MAX_SLOT_SIZE <= RTL_FIELD_SIZE(MPIDI_CH3I_SHM_Queue_t, slot));


static inline BOOL shm_is_empty(const MPIDI_CH3I_SHM_Queue_t* shm)
{
    return (shm->recv.index == shm->send.index);
}


static inline BOOL shm_is_full(MPIDI_CH3I_SHM_Queue_t* shm)
{
    return (shm->send.index - shm->recv.index == _countof(shm->slot));
}


struct MPIDI_CH3I_Process_t
{
    MPIDI_VC_t *shm_reading_list, *shm_writing_list;
    BOOL disable_shm;
    BOOL disable_sock;
    int progress_fixed_spin;
    int shm_eager_limit;
    int sock_eager_limit;
};

extern MPIDI_CH3I_Process_t MPIDI_CH3I_Process;

//
// Request Queue functions
//

static inline void
MPIDI_CH3I_lock_queue_exclusive(
    _In_ const MPIDI_CH3I_Request_Queue_t* q
    )
{
    MpiLockEnter(&q->lock);
}

static inline void
MPIDI_CH3I_unlock_queue_exclusive(
    _In_ const MPIDI_CH3I_Request_Queue_t* q
    )
{
    MpiLockLeave(&q->lock);
}


static inline void
MPIDI_CH3I_enqueue_unsafe(
    _In_ MPIDI_CH3I_Request_Queue_t* q,
    _In_ MPID_Request* req
    )
{
    req->AddRef();

    req->dev.next = NULL;
    if (q->head == NULL)
    {
        q->head = req;
    }
    else
    {
        q->tail->dev.next = req;
    }
    q->tail = req;
}


static inline void
MPIDI_CH3I_dequeue_unsafe(
    _In_ MPIDI_CH3I_Request_Queue_t* q
    )
{
    MPID_Request* req = q->head;
    q->head = q->head->dev.next;
    req->Release();
}


_Success_(return!=nullptr)
static inline MPID_Request*
MPIDI_CH3I_head_unsafe(
    _In_ MPIDI_CH3I_Request_Queue_t* q
    )
{
    return q->head;
}


static inline int
MPIDI_CH3I_empty_unsafe(
    const MPIDI_CH3I_Request_Queue_t* q
    )
{
    return (q->head == NULL);
}


static inline int
MPIDI_CH3I_empty(
    _In_ const MPIDI_CH3I_Request_Queue_t* q
    )
{
    int value;
    MPIDI_CH3I_lock_queue_exclusive(q);
    value = MPIDI_CH3I_empty_unsafe(q);
    MPIDI_CH3I_unlock_queue_exclusive(q);
    return value;
}


#define MPIDI_CH3I_Get_SendQ(vc) \
     (&(vc)->ch.sendq)


#define MPIDI_CH3I_SendQ_empty(vc) MPIDI_CH3I_empty(MPIDI_CH3I_Get_SendQ(vc))

#define MPIDI_CH3I_SendQ_enqueue_unsafe(vc, req)    MPIDI_CH3I_enqueue_unsafe(MPIDI_CH3I_Get_SendQ(vc), req)
#define MPIDI_CH3I_SendQ_dequeue_unsafe(vc)         MPIDI_CH3I_dequeue_unsafe(MPIDI_CH3I_Get_SendQ(vc))
#define MPIDI_CH3I_SendQ_head_unsafe(vc)            MPIDI_CH3I_head_unsafe(MPIDI_CH3I_Get_SendQ(vc))
#define MPIDI_CH3I_SendQ_empty_unsafe(vc)           MPIDI_CH3I_empty_unsafe(MPIDI_CH3I_Get_SendQ(vc))

#define MPIDI_CH3I_SendQ_lock_exclusive(vc)         MPIDI_CH3I_lock_queue_exclusive(MPIDI_CH3I_Get_SendQ(vc))
#define MPIDI_CH3I_SendQ_unlock_exclusive(vc)       MPIDI_CH3I_unlock_queue_exclusive(MPIDI_CH3I_Get_SendQ(vc))


//
// Simple lock class for SendQ
//
class SendQLock
{
    MPIDI_VC_t*  Vc;

public:
    SendQLock(_In_ MPIDI_VC_t* vc)
        : Vc(vc)
    {
        MPIDI_CH3I_SendQ_lock_exclusive(Vc);
    }

    ~SendQLock()
    {
        MPIDI_CH3I_SendQ_unlock_exclusive(Vc);
    }

private:
    SendQLock(const SendQLock& other);
    SendQLock& operator =(const SendQLock& other);
};




#define MPIDU_MAX_SHM_BLOCK_SIZE ((unsigned int)2*1024*1024*1024)

MPI_RESULT MPIDI_CH3I_VC_post_connect(_In_ MPIDI_VC_t *, _In_ MPID_Request *);
MPI_RESULT MPIDI_CH3I_VC_start_connect(_In_ MPIDI_VC_t * vc, _In_ MPID_Request * sreq);
int MPIDI_CH3I_Shm_connect(MPIDI_VC_t *, const char *, int *);
int MPIDI_CH3I_SSM_VC_post_read(MPIDI_VC_t *, MPID_Request *);
int MPIDI_CH3I_SSM_VC_post_write(MPIDI_VC_t *, MPID_Request *);

/* FIXME: These should be known only by the code that is managing
   the business cards */
#define MPIDI_CH3I_SHM_HOST_KEY          "shm_host"
#define MPIDI_CH3I_SHM_PID_KEY           "shm_pid"
#define MPIDI_CH3I_SHM_QUEUE_KEY         "shm_queue"

int MPIDI_CH3I_Accept_shm_connection(DWORD pid, PVOID p);
int MPIDI_CH3I_Accept_shm_message(DWORD d1, PVOID p1);

int MPIDI_CH3I_SHM_Get_mem(int size, MPIDI_CH3I_SHM_info_t* info);
int MPIDI_CH3I_SHM_Attach_to_mem(int pid, int shm_id, MPIDI_CH3I_SHM_info_t* info);
int MPIDI_CH3I_SHM_Unlink_mem(MPIDI_CH3I_SHM_info_t *p);
int MPIDI_CH3I_SHM_Release_mem(MPIDI_CH3I_SHM_info_t *p);
int MPIDI_CH3I_SHM_read_progress(MPIDI_VC_t* vc, BOOL* pfProgress);

#define MPIDI_CH3I_SHM_writev(vc_, iov_, n_, nb_) MPIDI_CH3I_SHM_writev_rma(vc_, iov_, n_, nb_, FALSE)

void MPIDI_CH3I_SHM_writev_rma(MPIDI_VC_t *vc, const MPID_IOV *iov, int n, MPIU_Bsize_t *num_bytes_ptr, BOOL fUseRma);

int
MPIDI_CH3I_SHM_start_write(
    _In_ MPIDI_VC_t* vc,
    _In_ MPID_Request* sreq
    );
void MPIDI_CH3I_SHM_Remove_vc_references(MPIDI_VC_t *vc);
void MPIDI_CH3I_SHM_Add_to_reader_list(MPIDI_VC_t *vc);
void MPIDI_CH3I_SHM_Add_to_writer_list(MPIDI_VC_t *vc);
void MPIDI_CH3U_Finalize_ssm_memory( void );


#define MPICH_MSG_QUEUE_NAME    "/mpich_msg_queue"
#define MPICH_MSG_QUEUE_PREFIX  "mpich2q"
#define MPICH_MSG_QUEUE_ID      12345

#endif /* !defined(MPICH_MPIDI_CH3_IMPL_H_INCLUDED) */
