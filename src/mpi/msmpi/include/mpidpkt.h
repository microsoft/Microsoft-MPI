// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_MPIDPKT_H
#define HAVE_MPIDPKT_H

typedef GUID MPI_CONTEXT_ID;

#define MPID_CH3_PKT_SIZE 40

//
//c4480: Specifying underlying type for enum.
//
#pragma warning(push)
#pragma warning(disable: 4480)

enum MPIDI_CH3_Pkt_type_t : UINT16
{
    MPIDI_CH3_PKT_EAGER_SEND = 0,
    MPIDI_CH3_PKT_EAGER_SYNC_SEND,    /* FIXME: no sync eager */
    MPIDI_CH3_PKT_EAGER_SYNC_ACK,
    MPIDI_CH3_PKT_READY_SEND,
    MPIDI_CH3_PKT_RNDV_REQ_TO_SEND,
    MPIDI_CH3_PKT_RNDV_CLR_TO_SEND,
    MPIDI_CH3_PKT_RNDV_SEND,          /* FIXME: should be stream put */
    MPIDI_CH3_PKT_CANCEL_SEND_REQ,
    MPIDI_CH3_PKT_CANCEL_SEND_RESP,
    MPIDI_CH3_PKT_PUT,
    MPIDI_CH3_PKT_GET,
    MPIDI_CH3_PKT_ACCUMULATE,
    MPIDI_CH3_PKT_GET_ACCUMULATE,
    MPIDI_CH3_PKT_COMPARE_AND_SWAP,
    MPIDI_CH3_PKT_RMA_OP_RESP,
    MPIDI_CH3_PKT_LOCK,
    MPIDI_CH3_PKT_UNLOCK,
    MPIDI_CH3_PKT_LOCK_GRANTED,
    MPIDI_CH3_PKT_UNLOCK_DONE,

    /* ack for last op in a passive target rma (lock/unlock) */
    MPIDI_CH3_PKT_PT_RMA_DONE,

    /* ack for error in a passive target rma */
    MPIDI_CH3_PKT_PT_RMA_ERROR,

    /* passive target rma optimization for single op */
    MPIDI_CH3_PKT_LOCK_PUT_UNLOCK,
    MPIDI_CH3_PKT_LOCK_GET_UNLOCK,
    MPIDI_CH3_PKT_LOCK_ACCUM_UNLOCK,

    MPIDI_CH3_PKT_CLOSE,
    MPIDI_CH3_PKT_END_CH3,

    MPIDI_CH3_PKT_END_ALL
};


#define MPIDI_CH3_PKT_FLAGS_CLEAR               0x00
#define MPIDI_CH3_PKT_FLAG_COMPRESSED           0x01
#define MPIDI_CH3_PKT_FLAG_COMPRESSED_ALL_ZEROS 0x02
#define MPIDI_CH3_PKT_FLAG_RMA_LAST_OP          0x04
#define MPIDI_CH3_PKT_FLAG_RMA_UNLOCK           0x08

#define MPIDI_CH3_PKT_IS_COMPRESSED             0x03

#pragma warning(pop)


struct MPIDI_Message_match
{
    INT32 tag;
    INT32 rank;
    MPI_CONTEXT_ID context_id;
};


struct MPIDI_CH3_Pkt_base_t
{
    MPIDI_CH3_Pkt_type_t type;
    UINT16 flags;
};
C_ASSERT(sizeof(MPIDI_CH3_Pkt_base_t) == sizeof(INT32));


struct MPIDI_CH3_Pkt_eager_send_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_Message_match match;
    MPI_Request sender_req_id;  /* needed for ssend and send cancel */
    MPIDI_msg_sz_t data_sz;
};


struct MPIDI_CH3_Pkt_eager_sync_ack_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Request sender_req_id;
};


struct MPIDI_CH3_Pkt_rndv_clr_to_send_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Request sender_req_id;
    MPI_Request receiver_req_id;
};


struct MPIDI_CH3_Pkt_rndv_send_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Request receiver_req_id;
};


struct MPIDI_CH3_Pkt_cancel_send_req_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_Message_match match;
    MPI_Request sender_req_id;
};


struct MPIDI_CH3_Pkt_cancel_send_resp_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Request sender_req_id;
    int ack;
};


//
// Pack the rma header struct to 4 bytes since it will be folloing
// a 4 byte message type; thus making the addr member aligned on 8 bytes
//
#pragma pack(push, 4)
struct MPIDI_CH3_Win_hdr_t
{
    /* count of datatype */
    unsigned int count;

    /* displacement in bytes from window base (nonnegative integer) */
    /* use 64 bit for platform interop; don't use platform specific type */
    UINT64 disp;

    /* datatype in rma operation */
    MPI_Datatype datatype;

    /*
     * Used in the last RMA operation in each epoch for decrementing rma op
     * counter in active target rma and for unlocking window in passive target
     * rma. Otherwise set to NULL
     */
    MPI_Win target;

    /*
     * Used in the last RMA operation in an epoch in the case of passive target
     * rma with shared locks. Otherwise set to NULL
     */
    MPI_Win source;

};
#pragma pack(pop)


struct MPIDI_CH3_Pkt_get_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_CH3_Win_hdr_t win;
    int dataloop_size;   /* for derived datatypes */
    MPI_Request request_handle;
};


struct MPIDI_CH3_Pkt_rma_resp_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Win win_source;
    int rank;
    MPI_Request request_handle;
};


struct MPIDI_CH3_Pkt_put_accum_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_CH3_Win_hdr_t win;
    int dataloop_size;   /* for derived datatypes */
    MPI_Op op;
    MPI_Request request_handle;
};

struct MPIDI_CH3_Pkt_get_accum_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_CH3_Win_hdr_t win;
    int dataloop_size;   /* for derived datatypes */
    MPI_Op op;
    MPI_Request request_handle;
};

struct MPIDI_CH3_Pkt_compare_and_swap_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_CH3_Win_hdr_t win;
    MPI_Request request_handle;
};

struct MPIDI_CH3_Pkt_lock_t : public MPIDI_CH3_Pkt_base_t
{
    int lock_type;
    MPI_Win win_target;
    MPI_Win win_source;
};


struct MPIDI_CH3_Pkt_unlock_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Win win_source;
    MPI_Win win_target;
};


struct MPIDI_CH3_Pkt_pt_rma_done_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Win win_source;
    int rank;
};


typedef MPIDI_CH3_Pkt_pt_rma_done_t MPIDI_CH3_Pkt_pt_rma_error_t;


struct MPIDI_CH3_Pkt_lock_granted_t : public MPIDI_CH3_Pkt_base_t
{
    MPI_Win win_source;
    int rank;
    int remote_lock_type;
};

typedef MPIDI_CH3_Pkt_pt_rma_done_t MPIDI_CH3_Pkt_unlock_done_t;

struct MPIDI_CH3_Pkt_lock_get_unlock_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_CH3_Win_hdr_t win;
    int lock_type;
    MPI_Request request_handle;
};


struct MPIDI_CH3_Pkt_lock_put_accum_unlock_t : public MPIDI_CH3_Pkt_base_t
{
    MPIDI_CH3_Win_hdr_t win;
    int lock_type;
    MPI_Op op;
};


struct MPIDI_CH3_Pkt_close_t : public MPIDI_CH3_Pkt_base_t
{
    int ack;
};


union MPIDI_CH3_Pkt_t
{
    MPIDI_CH3_Pkt_type_t type;
    MPIDI_CH3_Pkt_base_t base;
    MPIDI_CH3_Pkt_eager_send_t eager_send;
    MPIDI_CH3_Pkt_eager_sync_ack_t eager_sync_ack;
    MPIDI_CH3_Pkt_rndv_clr_to_send_t rndv_clr_to_send;
    MPIDI_CH3_Pkt_rndv_send_t rndv_send;
    MPIDI_CH3_Pkt_cancel_send_req_t cancel_send_req;
    MPIDI_CH3_Pkt_cancel_send_resp_t cancel_send_resp;
    MPIDI_CH3_Pkt_put_accum_t put_accum;
    MPIDI_CH3_Pkt_rma_resp_t rma_resp;
    MPIDI_CH3_Pkt_get_t get;
    MPIDI_CH3_Pkt_get_accum_t get_accum;
    MPIDI_CH3_Pkt_compare_and_swap_t compare_and_swap;
    MPIDI_CH3_Pkt_lock_t lock;
    MPIDI_CH3_Pkt_unlock_t unlock;
    MPIDI_CH3_Pkt_lock_granted_t lock_granted;
    MPIDI_CH3_Pkt_unlock_done_t unlock_done;
    MPIDI_CH3_Pkt_pt_rma_done_t pt_rma_done;
    MPIDI_CH3_Pkt_pt_rma_error_t pt_rma_error;
    MPIDI_CH3_Pkt_lock_get_unlock_t lock_get_unlock;
    MPIDI_CH3_Pkt_lock_put_accum_unlock_t lock_put_accum_unlock;
    MPIDI_CH3_Pkt_close_t close;
};
C_ASSERT(sizeof(MPIDI_CH3_Pkt_t) == MPID_CH3_PKT_SIZE);


static inline void MPIDI_Pkt_init(MPIDI_CH3_Pkt_base_t* pkt, MPIDI_CH3_Pkt_type_t type)
{
    pkt->type = type;
    pkt->flags = MPIDI_CH3_PKT_FLAGS_CLEAR;
}

#endif /*HAVE_MPIDPKT_H*/
