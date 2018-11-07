// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#if !defined(MPICH_MPIDRMA_H_INCLUDED)
#define MPICH_MPIDRMA_H_INCLUDED

/*
 * RMA Declarations.  We should move these into something separate from
 * a Request.
 */
/* to send derived datatype across in RMA ops */
typedef struct MPIDI_RMA_dtype_info
{
    MPI_Aint extent;
    MPI_Aint ub;
    MPI_Aint lb;
    MPI_Aint true_ub;
    MPI_Aint true_lb;

    /* pointer needed to update pointers within dataloop on remote side */
    void* dataloop;
    int dataloop_depth;

    int max_contig_blocks;
    int size;

    /* not needed because this info is sent in packet header. remove it after lock/unlock is implemented in the device */
    int dataloop_size;

    MPI_Datatype eltype;
    bool is_contig;
    bool has_sticky_ub;
    bool has_sticky_lb;

} MPIDI_RMA_dtype_info;


typedef enum MPIDI_Rma_op_t
{
    MPIDI_RMA_OP_LOCK,
    MPIDI_RMA_OP_GET,
    MPIDI_RMA_OP_PUT,
    MPIDI_RMA_OP_ACCUMULATE,
    MPIDI_RMA_OP_GET_ACCUMULATE,
    MPIDI_RMA_OP_COMPARE_AND_SWAP,

} MPIDI_Rma_op_t;


/* for keeping track of RMA ops, which will be executed at the next sync call */
typedef struct MPIDI_RMA_ops
{
    /* pointer to next element in list */
    struct MPIDI_RMA_ops *next;

    MPIDI_Rma_op_t type;
    void* origin_addr;
    int origin_count;
    TypeHandle origin_datatype;
    void* result_addr;
    int result_count;
    TypeHandle result_datatype;
    int target_rank;
    MPI_Aint target_disp;
    int target_count;
    TypeHandle target_datatype;

    /* for accumulate */
    MPI_Op op;

    /* for win_lock */
    int lock_type;
    int assert;

    /* for Rput, Rget and Raccumulate */
    MPID_Request* request;

} MPIDI_RMA_ops;


typedef struct MPIDI_PT_single_op
{
    /* put, get, or accum. */
    int type;
    int count;
    void *addr;
    TypeHandle datatype;
    MPI_Op op;

    /* for gets */
    MPI_Request request_handle;

    /* to indicate if the data has been received */
    int data_recd;

    /* For queued puts and accumulates, data is copied here.  Must be last in struct */
    UINT8 data[1];

} MPIDI_PT_single_op;


typedef struct MPIDI_Win_lock_queue
{
    struct MPIDI_Win_lock_queue *next;
    int lock_type;
    MPI_Win src_win;
    MPIDI_VC_t * vc;
    struct MPIDI_PT_single_op *pt_single_op;  /* to store info for lock-put-unlock optimization */

} MPIDI_Win_lock_queue;


#endif
