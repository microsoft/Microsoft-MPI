// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef REQUEST_H
#define REQUEST_H


#define MPIR_BARRIER_TAG                MPIR_INTERNAL_TAG(  1 )
#define MPIR_BCAST_TAG                  MPIR_INTERNAL_TAG(  2 )
#define MPIR_GATHER_TAG                 MPIR_INTERNAL_TAG(  3 )
#define MPIR_GATHERV_TAG                MPIR_INTERNAL_TAG(  4 )
#define MPIR_SCATTER_TAG                MPIR_INTERNAL_TAG(  5 )
#define MPIR_SCATTERV_TAG               MPIR_INTERNAL_TAG(  6 )
#define MPIR_ALLGATHER_TAG              MPIR_INTERNAL_TAG(  7 )
#define MPIR_ALLGATHERV_TAG             MPIR_INTERNAL_TAG(  8 )
#define MPIR_ALLTOALL_TAG               MPIR_INTERNAL_TAG(  9 )
#define MPIR_ALLTOALLV_TAG              MPIR_INTERNAL_TAG( 10 )
#define MPIR_REDUCE_TAG                 MPIR_INTERNAL_TAG( 11 )
#define MPIR_USER_REDUCE_TAG            MPIR_INTERNAL_TAG( 12 )
#define MPIR_USER_REDUCEA_TAG           MPIR_INTERNAL_TAG( 13 )
#define MPIR_ALLREDUCE_TAG              MPIR_INTERNAL_TAG( 14 )
#define MPIR_USER_ALLREDUCE_TAG         MPIR_INTERNAL_TAG( 15 )
#define MPIR_USER_ALLREDUCEA_TAG        MPIR_INTERNAL_TAG( 16 )
#define MPIR_REDUCE_SCATTER_TAG         MPIR_INTERNAL_TAG( 17 )
#define MPIR_USER_REDUCE_SCATTER_TAG    MPIR_INTERNAL_TAG( 18 )
#define MPIR_USER_REDUCE_SCATTERA_TAG   MPIR_INTERNAL_TAG( 19 )
#define MPIR_SCAN_TAG                   MPIR_INTERNAL_TAG( 20 )
#define MPIR_USER_SCAN_TAG              MPIR_INTERNAL_TAG( 21 )
#define MPIR_USER_SCANA_TAG             MPIR_INTERNAL_TAG( 22 )
#define MPIR_LOCALCOPY_TAG              MPIR_INTERNAL_TAG( 23 )
#define MPIR_EXSCAN_TAG                 MPIR_INTERNAL_TAG( 24 )
#define MPIR_ALLTOALLW_TAG              MPIR_INTERNAL_TAG( 25 )
#define MPIR_REDUCE_SCATTER_BLOCK_TAG   MPIR_INTERNAL_TAG( 26 )

#define MPIR_TAG_PRECONNECT             MPIR_INTERNAL_TAG( 128 )
#define MPIR_TAG_GET_CONTEXT_ID         MPIR_INTERNAL_TAG( 129 )
#define MPIR_TAG_COMM_CREATE_INTER      MPIR_INTERNAL_TAG( 130 )
#define MPIR_TAG_COMM_SPLIT_INTER       MPIR_INTERNAL_TAG( 131 )
#define MPIR_TAG_INTERCOMM_MERGE        MPIR_INTERNAL_TAG( 132 )
#define MPIR_TAG_CONNECT_ACCEPT_SYNC    MPIR_INTERNAL_TAG( 133 )
#define MPIR_TAG_CONNECT_ACCEPT_BIZCARD MPIR_INTERNAL_TAG( 134 )

//
// Non-blocking collectives use a series of tag sequences in the range defined
// below to compose the final tag in the format of 0x8SSSSIIR:
// SSSS - the tag sequence defined here.
//   II - the collective tag number defined above, for example: 2 for bcast.
//    R - reserved bits to allow encoding communicator information.
//
#define MPIR_NBC_BEGIN_TAG_SEQUENCE     1
#define MPIR_NBC_END_TAG_SEQUENCE       0xFFFF


/* Requests */
/* This currently defines a single structure type for all requests.
   Eventually, we may want a union type, as used in MPICH-1 */
struct MPID_Request;


//
// Forward declaration of NbcTask class
//
class NbcTask;


/* request type */
#define MPIDI_REQUEST_TYPE_RECV                         0
#define MPIDI_REQUEST_TYPE_SEND                         1
#define MPIDI_REQUEST_TYPE_RSEND                        2
#define MPIDI_REQUEST_TYPE_SSEND                        3
/* We need a BSEND type for persistent bsends (see mpid_startall.c) */
#define MPIDI_REQUEST_TYPE_BSEND                        4
#define MPIDI_REQUEST_TYPE_PUT_RESP                     5
#define MPIDI_REQUEST_TYPE_GET_RESP                     6
#define MPIDI_REQUEST_TYPE_ACCUM_RESP                   7
#define MPIDI_REQUEST_TYPE_PUT_RESP_DERIVED_DT          8
#define MPIDI_REQUEST_TYPE_GET_RESP_DERIVED_DT          9
#define MPIDI_REQUEST_TYPE_ACCUM_RESP_DERIVED_DT        10
#define MPIDI_REQUEST_TYPE_PT_SINGLE_PUT                11
#define MPIDI_REQUEST_TYPE_PT_SINGLE_ACCUM              12
#define MPIDI_REQUEST_TYPE_GET_ACCUM_RESP               13
#define MPIDI_REQUEST_TYPE_GET_ACCUM_RESP_DERIVED_DT    14
#define MPIDI_REQUEST_TYPE_COMPARE_AND_SWAP_RESP        15


/* request message type */
#define MPIDI_REQUEST_NO_MSG 0
#define MPIDI_REQUEST_EAGER_MSG 1
#define MPIDI_REQUEST_RNDV_MSG 2
#define MPIDI_REQUEST_SELF_MSG 3

//
// Size of the preallocated request array
//
#define MPID_REQUEST_PREALLOC 16

/*@
  MPIDI_CH3_Request_destroy - Release resources in use by an existing request
  object.

  Input Parameters:
. req - pointer to the request object

@*/
void MPIDI_CH3_Request_destroy(MPID_Request * req);


struct MPIDI_CH3I_Request_t
{
    /* store message id while the message is queued before send */
    int msg_id;
};

struct CompressionBuffer
{
    MPIDI_msg_sz_t size;
    unsigned char data[ANYSIZE_ARRAY];
};


#define MPIDI_RMA_FLAG_LAST_OP       0x1
#define MPIDI_RMA_FLAG_LOCK_RELEASE  0x2

struct MPIDI_Request_t
{
    MPIDI_Message_match match;

    /* user_buf, user_count, and datatype needed to process rendezvous messages. */
    void * user_buf;
    int user_count;
    TypeHandle datatype;

    //
    //Non-null compression_buffer indicates that the payload of a message being sent is compressed.
    //The pkt.base.flags indicates that the payload of a message being received is compressed.
    //
    CompressionBuffer* pCompressionBuffer;

    /* segment, segment_first, and segment_size are used when processing
       non-contiguous datatypes */
    MPID_Segment* segment_ptr;
    MPIDI_msg_sz_t segment_first;
    MPIDI_msg_sz_t segment_size;

    /* iov and iov_count define the data to be transferred/received.
       iov_offset points to the current head element in the IOV */
    MPID_IOV iov[MPID_IOV_LIMIT];
    int iov_count;
    int iov_offset;

    /* OnDataAvail is the action to take when data is now available.
       For example, when an operation described by an iov has
       completed.  This replaces the MPIDI_CA_t (completion action)
       field used through MPICH2 1.0.4. */
    int (*OnDataAvail)( MPIDI_VC_t *, struct MPID_Request *, int * );
    /* OnFinal is used in the following case:
       OnDataAvail is set to a function, and that function has processed
       all of the data.  At that point, the OnDataAvail function can
       reset OnDataAvail to OnFinal.  This is normally used when processing
       non-contiguous data, where there is one more action to take (such
       as a get-response) when processing of the non-contiguous data
       completes. This value need not be initialized unless OnDataAvail
       is set to a non-null value (and then only in certain cases) */
    int (*OnFinal)( MPIDI_VC_t *, struct MPID_Request *, int * );

    /* tmpbuf and tmpbuf_sz describe temporary storage used for things like
       unexpected eager messages and packing/unpacking
       buffers.  tmpuf_off is the current offset into the temporary buffer. */
    void          *tmpbuf;
    MPIDI_msg_sz_t tmpbuf_off;
    MPIDI_msg_sz_t tmpbuf_sz;

    MPIDI_msg_sz_t recv_data_sz;
    MPI_Request    sender_req_id;

    union
    {
        unsigned int state;

        struct
        {
            unsigned int msg_type : 4;
            unsigned int req_type : 4;
            unsigned int use_srbuf : 1;
            unsigned int sync_ack : 1;
            unsigned int rma_last_op : 1;
            unsigned int rma_unlock : 1;

        } flags;
    };

    volatile long  cancel_pending;
    volatile long  recv_pending_count;

    /* The next 10 are for RMA */
    MPI_Op op;

    /* For accumulate, since data is first read into a tmp_buf */
    void* accum_user_buf;

    /* For derived datatypes at target */
    struct MPIDI_RMA_dtype_info *dtype_info;
    DLOOP_Dataloop *dataloop;

    /* displacement sent by source to reference target win buffer */
    size_t target_disp;

    /* req. handle needed to implement derived datatype gets  */
    MPI_Request request_handle;

    MPI_Win target_win_handle;
    MPI_Win source_win_handle;

    int target_rank;

    /* to indicate a lock-put-unlock optimization case */
    bool single_op_opt;

    /* for single lock-put-unlock optimization */
    struct MPIDI_Win_lock_queue *lock_queue_entry;

    /* Occasionally, when a message cannot be sent, we need to cache the
       data that is required.  The fields above (such as userbuf and tmpbuf)
       are used for the message data.  However, we also need space for the
       message packet. This field provide a generic location for that.
     */
    MPIDI_CH3_Pkt_t pkt;
    struct MPID_Request * next;
};



/*E
  MPID_Request_kind - Kinds of MPI Requests

  Module:
  Request-DS

  NOTE:
  MPID_REQUEST_MPROBE signifies that this is a request created by
   MPI_Mprobe or MPI_Improbe.  Since we use MPI_Request objects as our
   MPI_Message objects, we use this separate kind in order to provide stronger
   error checking.  Once a message (backed by a request) is promoted to a real
   request by calling MPI_Mrecv/MPI_Imrecv, we actually modify the kind to be
   MPID_REQUEST_RECV in order to keep completion logic as simple as possible.

  E*/
enum MPID_Request_kind_t
{
    MPID_REQUEST_UNDEFINED,
    MPID_BUFFERED_SEND,
    MPID_REQUEST_SEND,
    MPID_REQUEST_RECV,
    MPID_PERSISTENT_SEND,
    MPID_PERSISTENT_RECV,
    MPID_GREQUEST,
    MPID_REQUEST_MPROBE,
    MPID_REQUEST_NBC,
    MPID_REQUEST_NOOP,
    MPID_REQUEST_RMA_GET,
    MPID_REQUEST_RMA_PUT,
    MPID_REQUEST_RMA_RESP,
    MPID_LAST_REQUEST_KIND

};

/* Generalized requests callbacks helpers */

struct MPID_Grequest_query_function
{
    MPI_Grequest_query_function* user_function;
    MPIR_Grequest_query_proxy* proxy;

};


struct MPID_Grequest_free_function
{
    MPI_Grequest_free_function* user_function;
    MPIR_Grequest_free_proxy* proxy;

};


struct MPID_Grequest_cancel_function
{
    MPI_Grequest_cancel_function* user_function;
    MPIR_Grequest_cancel_proxy* proxy;

};


struct MPID_Grequest
{
    MPID_Grequest_query_function query;
    MPID_Grequest_free_function free;
    MPID_Grequest_cancel_function cancel;
    void* extra_state;

};

#ifdef HAVE_DEBUGGER_SUPPORT
struct MPIR_Sendq;
#endif


/*S
  MPID_Request - Description of the Request data structure

  Module:
  Request-DS

  Notes:
  If it is necessary to remember the MPI datatype, this information is
  saved within the device-specific fields provided by 'MPID_DEV_REQUEST_DECL'.

  Requests come in many flavors, as stored in the 'kind' field.  It is
  expected that each kind of request will have its own structure type
  (e.g., 'MPID_Request_send_t') that extends the 'MPID_Request'.

  S*/

struct MPID_Request
{
    int          handle;
    volatile long ref_count;
    MPID_Request_kind_t kind;
    /* completion counter */
    volatile long cc;

    /* A comm is needed to find the proper error handler */
    MPID_Comm *comm;
    /* Status is needed for wait/test/recv */
    MPIR_Status status;

    MPIDI_Request_t dev;
    union
    {
        struct
        {
            //
            // Persistent requests have their own "real" requests.
            // Receive requests have partnering send requests when src=dest. etc.
            //
            struct MPID_Request *partner_request;

            //
            // User-defined request support
            //
            MPID_Grequest greq;

            //
            // Other, device-specific information
            //
            MPIDI_CH3I_Request_t ch;
        };
        struct
        {
            BYTE* tmpBuf;
            NbcTask* tasks;
            ULONG nTasks;
            ULONG nCompletedTasks;
            unsigned tag;
        } nbc;
    };

    //
    // support for callbacks
    //
    ThreadHandle* threadInfo;
    MSMPI_Request_callback* pfnCallback;
    MPIR_Status* pStatus;

#ifdef HAVE_DEBUGGER_SUPPORT
    MPIR_Sendq* sendq_req_ptr;
#endif

    //
    // Request reference counting
    //
public:

    void AddRef();

    void Release();

    //
    // Request Packet functions
    //
public:

    MPIDI_CH3_Pkt_t* get_pkt();

    void init_pkt( MPIDI_CH3_Pkt_type_t type );

    void init_pkt( const MPIDI_CH3_Pkt_t& pkt );


    //
    // Request iov functions
    //
public:

    MPID_IOV* iov();

    void add_recv_iov( void* buf, int len );

    void add_send_iov( const void* buf, int len );

    int iov_size() const;

    //
    // Adjust the iovec in the request by the supplied number of bytes.
    // If the iovec has been consumed, return true; otherwise return false.
    //
    bool adjust_iov( MPIDI_msg_sz_t nb );

    //
    // Request completion count functions
    //
public:
    MPI_RESULT
    set_apc(
        MSMPI_Request_callback* callback_fn,
        MPI_Status* callback_status
        );

    void signal_apc();

    void signal_completion();

    void postpone_completion();

    bool is_internal_complete() const;

    bool test_complete();

    inline MPI_RESULT execute();

    inline MPI_RESULT execute_send();

    inline MPI_RESULT execute_ssend();

    inline MPI_RESULT execute_rsend();

    inline void cancel();

    //
    // Request NBC functions
    //
    MPI_RESULT execute_nbc_tasklist( _In_range_(<, ULONG_MAX) ULONG nextTask = 0 );

    void cancel_nbc_tasklist();

    void print_nbc_tasklist();

private:

    bool preq_is_complete() const;

    bool bsend_is_complete() const;

    bool nbcreq_test_complete();

    //
    // Request Flags and Status functions
    //
public:

    int get_type() const;

    void set_type( int type );

    int get_msg_type() const;

    void set_msg_type( int msgtype );

    int get_and_set_cancel_pending();

    int dec_and_get_recv_pending();

    bool using_srbuf() const;

    void set_using_srbuf();

    void clear_using_srbuf();

    bool needs_sync_ack() const;

    void set_sync_ack_needed();

    bool is_rma_last_op() const;

    void set_rma_last_op();

    void clear_rma_last_op();

    bool rma_unlock_needed() const;

    void set_rma_unlock();

    void clear_rma_unlock();
};


extern MPIU_Object_alloc_t MPID_Request_mem;
/* Preallocated request objects */
extern MPID_Request MPID_Request_direct[];

//
// Request accounting
//
#if DBG
#   define MPID_Request_track_user_alloc() InterlockedIncrement(&MPID_Request_mem.num_user_alloc)
#   define MPID_Request_track_user_adjust( count ) InterlockedExchangeAdd(&MPID_Request_mem.num_user_alloc, count)
#   define MPID_Request_track_user_free() InterlockedDecrement(&MPID_Request_mem.num_user_alloc)
#   define MPID_Message_track_user_alloc() InterlockedIncrement(&MPID_Request_mem.num_user_msg_alloc)
#   define MPID_Message_track_user_free() InterlockedDecrement(&MPID_Request_mem.num_user_msg_alloc)
#else
#   define MPID_Request_track_user_alloc()
#   define MPID_Request_track_user_adjust( c )
#   define MPID_Request_track_user_free()
#   define MPID_Message_track_user_alloc()
#   define MPID_Message_track_user_free()
#endif


/*@
  MPID_Request_create - Create and return a bare request

  Return value:
  A pointer to a new request object.

  Notes:
  This routine is intended for use by 'MPI_Grequest_start' only.

  The request object returned by this routine should be initialized such that
  ref_count is one and handle contains a valid handle referring to the object.
  @*/
MPID_Request* MPID_Request_create(MPID_Request_kind_t kind);


static inline MPID_Request* MPID_Request_create_base( MPID_Request_kind_t kind );


/*
 * MPID_Requests
 *
 * MPI Requests are handles to MPID_Request structures.  These are used
 * for most communication operations to provide a uniform way in which to
 * define pending operations.  As such, they contain many fields that are
 * only used by some operations (logically, an MPID_Request is a union type).
 *
 * There are several kinds of requests.  They are
 *    Send, Receive, RMA, User, Persistent
 * In addition, send and RMA requests may be "incomplete"; this means that
 * they have not sent their initial packet, and they may store additional
 * data about the operation that will be used when the initial packet
 * can be sent.
 *
 * Also, requests that are used internally within blocking MPI routines
 * (only Send and Receive requests) do not require references to
 * (or increments of the reference counts) communicators or datatypes.
 * Thus, freeing these requests also does not require testing or
 * decrementing these fields.
 *
 * Finally, we want to avoid multiple tests for a failure to allocate
 * a request.  Thus, the request allocation macros will jump to fn_fail
 * if there is an error.  This is akin to using a "throw" in C++.
 *
 * For example, a posted (unmatched) receive queue entry needs only:
 *     match info
 *     buffer info (address, count, datatype)
 *     if nonblocking, communicator (used for finding error handler)
 *     if nonblocking, cancelled state
 * Once matched, a receive queue entry also needs
 *     actual match info
 *     message type (eager, rndv, eager-sync)
 *     completion state (is all data available)
 *        If destination datatype is non-contiguous, it also needs
 *        current unpack state.
 * An unexpected message (in the unexpected receive queue) needs only:
 *     match info
 *     message type (eager, rndv, eager-sync)
 *     if (eager, eager-sync), data
 *     completion state (is all data available?)
 * A send request requires only
 *     message type (eager, rndv, eager-sync)
 *     completion state (has all data been sent?)
 *     canceled state
 *     if nonblocking, communicator (used for finding error handler)
 *     if the initial envelope is still pending (e.g., could not write yet)
 *         match info
 *     if the data is still pending (rndv or would not send eager)
 *         buffer info (address, count, datatype)
 * RMA requests require (what)?
 * User (generalized) requests require
 *     function pointers for operations
 *     completion state
 *     cancelled state
 */


inline
MPID_Request*
MPIDI_Request_create_sreq(
    _In_     int reqtype,
    _In_opt_ const void* buf,
    _In_     int count,
    _In_     TypeHandle datatype,
    _In_     int rank,
    _In_     int tag,
    _In_     MPID_Comm* comm
    );

/* This is the receive request version of MPIDI_Request_create_sreq */
inline MPID_Request* MPIDI_Request_create_rreq();

/* creates an MPID_Result which is marked as complete */
inline MPID_Request* MPIDI_Request_create_completed_req();

int
MPIAPI
MPIR_Grequest_query_c_proxy(
    MPI_Grequest_query_function* user_function,
    void* extra_state,
    MPI_Status* status
    );

int
MPIAPI
MPIR_Grequest_free_c_proxy(
    MPI_Grequest_free_function* user_function,
    void* extra_state
    );

int
MPIAPI
MPIR_Grequest_cancel_c_proxy(
    MPI_Grequest_cancel_function* user_function,
    void* extra_state,
    int complete
    );

MPI_RESULT MPIR_Request_complete(MPI_Request *, MPID_Request *, MPI_Status *, int *);
MPI_RESULT MPIR_Request_get_error(MPID_Request *);


static inline int MPIR_Wait( MPID_Request* req );


static inline void
MPIR_Request_extract_status(
    _In_ const MPID_Request *req,
    _Inout_ MPI_Status *status
    );


#endif // REQUEST_H

