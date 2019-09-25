// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/*
 * WARNING: Functions and macros in this file are for internal use only.
 * As such, they are only visible to the device and
 * channel.  Do not include them in the MPID macros.
 */

#if !defined(MPICH_MPIDIMPL_H_INCLUDED)
#define MPICH_MPIDIMPL_H_INCLUDED

#include "mpiimpl.h"
#include "mpidi_ch3_pre.h"

/* FIXME: These sizes need to be documented at least, and it should be
   easier to change them, at least at configure/build time */
#define MPIDI_CH3_EAGER_LIMIT_MIN       1500
#define MPIDI_CH3_EAGER_LIMIT_DEFAULT 128000



#if !defined(MPIDI_IOV_DENSITY_MIN)
#   define MPIDI_IOV_DENSITY_MIN (16 * 1024)
#endif

/*S
  MPIDI_PG_t - Process group description

  Notes:
  Every 'MPI_COMM_WORLD' known to this process has an associated process
  group.
  S*/
struct MPIDI_PG_t
{
    /* MPIU_Object field.  MPIDI_PG_t objects are not allocated using the
       MPIU_Object system, but we do use the associated reference counting
       routines.  Therefore, handle must be present, but is not used
       except by debugging routines */
    int handle;
    volatile long ref_count;

    /* Next pointer used to maintain a list of all process groups known to
       this process */
    struct MPIDI_PG_t* next;

    /* Number of processes in the process group */
    int size;

    /* VC table.  At present this is a pointer to an array of VC structures.
       Someday we may want make this a pointer to an array
       of VC references.  Thus, it is important to use MPIDI_PG_Get_vc()
       instead of directly referencing this field. */
    MPIDI_VC_t* vct;

    /* Process group ID.  The actual ID is defined and
       allocated by the process group.  It is kept in the
       device space because it is necessary for the device to be able to
       find a particular process group. */
    GUID id;

    //
    // An array of business cards. This pointer is non-NULL only
    // if the pg is "acquired" during the MPI_Comm_connect/accept
    //
    char** pBusinessCards;
};

#define MSMPI_MAX_BUSINESS_CARD_LENGTH 512

/*S
  MPIDI_Process_t - The information required about this process by the CH3
  device.

  S*/
struct MPIDI_Process_t
{
    MPIDI_PG_t* my_pg;
    char        my_businesscard[MSMPI_MAX_BUSINESS_CARD_LENGTH];
    int         my_pg_rank;
};

extern MPIDI_Process_t MPIDI_Process;


/*---------------------------
  BEGIN PROCESS GROUP SECTION
  ---------------------------*/

BOOL MPIDI_PG_Equal_ids(const GUID* id1, const GUID* id2);

void MPIDI_PG_Finalize(void);

MPI_RESULT
MPIDI_PG_Create(
    _In_  int          vct_sz,
    _In_  const GUID&  pg_id,
    _Outptr_ MPIDI_PG_t** pg_ptr
    );

void
MPIDI_PG_Destroy(
    _Post_ptr_invalid_ MPIDI_PG_t* pg
    );


_Ret_maybenull_
MPIDI_PG_t*
MPIDI_PG_Find(
    _In_ const GUID& id
    );

void
MPIDI_PG_Get_next(
    _Outptr_result_maybenull_ MPIDI_PG_t ** pgp
    );

void
MPIDI_PG_Iterate_reset(void);

MPI_RESULT
MPIDI_PG_Close_VCs( void );

MPI_RESULT
MPIDI_PG_GetConnString(
    _In_                    const MPIDI_PG_t* pg,
    _In_range_(>=, 0)       int               rank,
    _Out_writes_z_(bufsize) char*             buf,
    _In_                    size_t            bufsize
    );


int MPIDI_PG_Get_size(MPIDI_PG_t * pg);

/* CH3_PG_Init allows the channel to pre-initialize the process group */
void MPIDI_CH3_PG_Init(MPIDI_PG_t* pg);

#define MPIDI_PG_add_ref(pg_)                   \
{                                               \
    MPIU_Object_add_ref(pg_);                   \
}

#define MPIDI_PG_release_ref(pg_)               \
{                                               \
    int inuse_;                                 \
    MPIU_Object_release_ref(pg_, &inuse_);      \
    if (inuse_ == 0) MPIDI_PG_Destroy(pg_);     \
}

#define MPIDI_PG_Get_size(pg_) ((pg_)->size)

/*-------------------------
  END PROCESS GROUP SECTION
  -------------------------*/


/*--------------------------------
  BEGIN VIRTUAL CONNECTION SECTION
  --------------------------------*/
/*E
  MPIDI_VC_State_t - States for a virtual connection.

  Notes:
  A closed connection is placed into 'STATE_INACTIVE'. (is this true?)
 E*/
enum MPIDI_VC_State_t
{
    MPIDI_VC_STATE_INACTIVE=1,
    MPIDI_VC_STATE_ACTIVE,
    MPIDI_VC_STATE_LOCAL_CLOSE,
    MPIDI_VC_STATE_REMOTE_CLOSE,
    MPIDI_VC_STATE_CLOSE_ACKED
};

struct MPID_Comm;

struct MPIDI_VC_t
{
    /* XXX - need better comment */
    /* MPIU_Object fields.  MPIDI_VC_t objects are not allocated using the
       MPIU_Object system, but we do use the associated
       reference counting routines.  The handle value is required
       when debugging objects (the handle kind is used in reporting
       on changes to the object).
    */
    int handle;
    volatile long ref_count;

    /* state of the VC */
    MPIDI_VC_State_t state;

    /* Process group to which this VC belongs */
    MPIDI_PG_t * pg;

    /* Rank of the process in that process group associated with this VC */
    int pg_rank;

    /* Local process ID */
    int lpid;

    /* eager message threshold */
    MPIDI_msg_sz_t eager_max_msg_sz;

    MPIDI_CH3I_VC_t ch;

    int node_id;

};

enum MPIDI_VC_Event_t
{
    MPIDI_VC_EVENT_TERMINATED
};


/* Initialize a new VC */
void MPIDI_VC_Init( MPIDI_VC_t *, MPIDI_PG_t *, int );

#define MPIDI_VC_add_ref( _vc ) \
    MPIU_Object_add_ref( _vc )

#define MPIDI_VC_release_ref( _vc, _inuse ) \
    MPIU_Object_release_ref( _vc, _inuse )

static inline MPIDI_VC_t* MPIDI_PG_Get_vc(MPIDI_PG_t* pg, int rank)
{
    MPIDI_VC_t* vc = &pg->vct[rank];
    if(vc->state == MPIDI_VC_STATE_INACTIVE)
    {
        vc->state = MPIDI_VC_STATE_ACTIVE;
    }

    return vc;
}

static inline MPIDI_VC_t* MPIDI_Comm_get_vc(MPID_Comm* comm, int rank)

{
    MPIDI_VC_t* vc = comm->vcr[rank];
    if(vc->state == MPIDI_VC_STATE_INACTIVE)
    {
        vc->state = MPIDI_VC_STATE_ACTIVE;
    }

    return vc;
}

/*------------------------------
  END VIRTUAL CONNECTION SECTION
  ------------------------------*/


/*---------------------------------
  BEGIN SEND/RECEIVE BUFFER SECTION
  ---------------------------------*/
#if !defined(MPIDI_CH3U_Offsetof)
#    define MPIDI_CH3U_Offsetof(struct_, field_) ((MPI_Aint) &((struct_*)0)->field_)
#endif

#if !defined(MPIDI_CH3U_SRBuf_size)
#    define MPIDI_CH3U_SRBuf_size (256 * 1024)
#endif

struct MPIDI_CH3U_SRBuf_element_t
{
    struct MPIDI_CH3U_SRBuf_element_t* next;
    char buf[MPIDI_CH3U_SRBuf_size];
};

extern MPIDI_CH3U_SRBuf_element_t * MPIDI_CH3U_SRBuf_pool;

inline void MPIDI_CH3U_SRBuf_get(_In_ MPID_Request *req)
{
    MPIDI_CH3U_SRBuf_element_t *tmp;
    if (MPIDI_CH3U_SRBuf_pool == NULL)
    {
        MPIDI_CH3U_SRBuf_pool = MPIU_Malloc_obj(MPIDI_CH3U_SRBuf_element_t);
        if(MPIDI_CH3U_SRBuf_pool != NULL)
            MPIDI_CH3U_SRBuf_pool->next = NULL;
    }
    if(MPIDI_CH3U_SRBuf_pool != NULL)
    {
        tmp = MPIDI_CH3U_SRBuf_pool;
        MPIDI_CH3U_SRBuf_pool = MPIDI_CH3U_SRBuf_pool->next;
        tmp->next = NULL;
        req->dev.tmpbuf = tmp->buf;
    }
    else
    {
        req->dev.tmpbuf = NULL;
    }
}

inline void MPIDI_CH3U_SRBuf_free(_In_ MPID_Request* req)
{
    MPIDI_CH3U_SRBuf_element_t * tmp;
    MPIU_Assert(req->using_srbuf());
    req->clear_using_srbuf();
    tmp = CONTAINING_RECORD(req->dev.tmpbuf, MPIDI_CH3U_SRBuf_element_t, buf);
    tmp->next = MPIDI_CH3U_SRBuf_pool;
    MPIDI_CH3U_SRBuf_pool = tmp;
    req->dev.tmpbuf = NULL;
}

inline void MPIDI_CH3U_SRBuf_alloc(_In_ MPID_Request* req)
{
    MPIDI_CH3U_SRBuf_get(req);
    if (req->dev.tmpbuf != NULL)
    {
        req->dev.tmpbuf_sz = MPIDI_CH3U_SRBuf_size;
        req->set_using_srbuf();
    }
    else
    {
        req->dev.tmpbuf_sz = 0;
    }
}


/*-------------------------------
  END SEND/RECEIVE BUFFER SECTION
  -------------------------------*/


/* Prototypes for internal device routines */
MPI_RESULT
MPIDI_Isend_self(
    MPID_Request* sreq
    );

MPI_RESULT MPIDI_CH3U_Comm_FinishPending( MPID_Comm * );

#define MPIDI_MAX_KVS_VALUE_LEN    4096

/* ------------------------------------------------------------------------- */
/* mpirma.h (in src/mpi/rma?) */
/* ------------------------------------------------------------------------- */

/* internal */
MPI_RESULT MPIDI_CH3I_Release_lock(MPID_Win * win_ptr);
bool MPIDI_CH3I_Try_acquire_win_lock(MPID_Win * win_ptr, int requested_lock);
MPI_RESULT MPIDI_CH3I_Send_lock_granted_pkt(MPIDI_VC_t * vc, MPI_Win source_win_handle, int rank, int lock_type);
MPI_RESULT MPIDI_CH3I_Send_unlock_done_pkt(MPIDI_VC_t * vc, MPI_Win source_win_handle, int rank);
MPI_RESULT MPIDI_CH3I_Send_pt_rma_done_pkt(MPIDI_VC_t *vc, MPI_Win source_win_handle, int rank, bool unlock);
MPI_RESULT MPIDI_CH3I_Send_pt_rma_error_pkt(MPIDI_VC_t *vc, MPI_Win source_win_handle);


/* Inform the process group of our connection information string (business
   card) */
MPI_RESULT MPIDI_PG_SetConnInfo( const MPIDI_PG_t* pg, int rank, const char *connString );

/* NOTE: Channel function prototypes are in mpidi_ch3_post.h since some of the
   macros require their declarations. */

/* Channel definitions */

/*@
  MPIDI_CH3_ffSend - A fire & forget request to send a CH3 packet.  The caller is never notified
  of the send completion as there is no request associated with it.

  BUGBUG:erezh: fix all the call sites not to use this function and eventually remove it.

  Input Parameters:
+ vc - virtual connection over which to send the CH3 packet
- pkt - pointer to a MPIDI_CH3_Pkt_t structure containing the substructure to be sent

  Return value:
  An mpi error code.

  NOTE:
  The packet structure may be allocated on the stack.

  IMPLEMETORS:
  If the send can not be completed immediately, the packet structure must be
  stored internally until the request is complete.

@*/
MPI_RESULT MPIDI_CH3_ffSend(MPIDI_VC_t * vc, const MPIDI_CH3_Pkt_t* pkt);


/*@
  MPIDI_CH3_SendRequest - A non-blocking request to send a CH3 packet and
  associated data using an existing request object.  When
  the send is complete the channel implementation will call the
  OnDataAvail routine in the request, if any.

  Input Parameters:
+ vc - virtual connection over which to send the CH3 packet and data
. sreq - pointer to the send request object with its iov iov_count, iov_offset
  and pkt intialized.

  Return value:
  An mpi error code.

  NOTE:
  The first element in the vector must point to the packet structure within the request.

  If the send completes immediately, the channel implementation still must
  call the OnDataAvail routine in the request, if any.
@*/
MPI_RESULT MPIDI_CH3_SendRequest(MPIDI_VC_t* vc, MPID_Request* sreq);


/*@
  MPIDI_CH3_Connection_terminate - terminate the underlying connection associated with the specified VC

  Input Parameters:
. vc - virtual connection

  Return value:
  An MPI error code
@*/
int MPIDI_CH3_Connection_terminate(MPIDI_VC_t * vc);


VOID MPID_Recvq_lock_exclusive();
VOID MPID_Recvq_unlock_exclusive();

VOID MPID_Recvq_lock_shared();
VOID MPID_Recvq_unlock_shared();

/*
 * Channel utility prototypes
 */
_Success_(return == TRUE)
BOOL
MPID_Recvq_probe_dq_msg_unsafe(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Outptr_ MPID_Request **message
    );

inline BOOL
MPID_Recvq_probe_dq_msg(
    _In_range_(>=, MPI_ANY_SOURCE) int source,
    _In_range_(>=, MPI_ANY_TAG) int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Outptr_ MPID_Request **message
    )
{
    MPID_Recvq_lock_exclusive();
    BOOL result = MPID_Recvq_probe_dq_msg_unsafe(
                    source,
                    tag,
                    context_id,
                    message);
    MPID_Recvq_unlock_exclusive();
    return result;
}

_Success_(return == TRUE)
BOOL
MPID_Recvq_probe_msg_unsafe(
    _In_ int source,
    _In_ int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Out_ MPI_Status* s
    );

inline BOOL
MPID_Recvq_probe_msg(
    _In_ int source,
    _In_ int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Out_ MPI_Status* s
    )
{
    MPID_Recvq_lock_shared();
    BOOL result = MPID_Recvq_probe_msg_unsafe(
                    source,
                    tag,
                    context_id,
                    s
                    );
    MPID_Recvq_unlock_shared();
    return result;
}


MPID_Request *
MPID_Recvq_dq_msg_by_id_unsafe(
    _In_ MPI_Request sreq_id,
    _In_ const MPIDI_Message_match *match
    );


inline MPID_Request *
MPID_Recvq_dq_msg_by_id(
    _In_ MPI_Request sreq_id,
    _In_ const MPIDI_Message_match *match
    )
{
    MPID_Recvq_lock_exclusive();
    MPID_Request* result = MPID_Recvq_dq_msg_by_id_unsafe(
                                sreq_id,
                                match);
    MPID_Recvq_unlock_exclusive();
    return result;
}


MPID_Request*
MPID_Recvq_dq_unexpected_or_new_posted_unsafe(
    _In_ int source,
    _In_ int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Out_ int * foundp
    );


//
// This function will dequeue or create a new recv request.  Upon
//  creation, while still under the RecvqLock, it will invoke
//  the InitFunc to initialize/process the state of the recv request.
//
// Upon failure, this function will remove the request from the posted q
//
template<typename FnInitT>
inline MPI_RESULT
MPID_Recvq_dq_unexpected_or_new_posted(
    _In_ int source,
    _In_ int tag,
    _In_ const MPI_CONTEXT_ID& context_id,
    _Out_ int * foundp,
    _Out_ MPID_Request** pRequest,
    _In_ FnInitT InitFunc
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPID_Recvq_lock_exclusive();
    MPID_Request* req = MPID_Recvq_dq_unexpected_or_new_posted_unsafe(
                                source,
                                tag,
                                context_id,
                                foundp);
    if (req == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_exit;
    }

    mpi_errno = InitFunc(req, *foundp != FALSE);

    if (mpi_errno != MPI_SUCCESS && *foundp == FALSE)
    {
        MPID_Recvq_erase_posted_unsafe( req );
        req->Release();
        req = nullptr;
    }

fn_exit:
    MPID_Recvq_unlock_exclusive();
    *pRequest = req;
    return mpi_errno;

}

MPID_Request*
MPID_Recvq_dq_recv_unsafe(
    _In_ MPID_Request* rreq
    );

inline MPID_Request*
MPID_Recvq_dq_recv(
    _In_ MPID_Request* rreq
    )
{
    MPID_Recvq_lock_exclusive();
    MPID_Request* result = MPID_Recvq_dq_recv_unsafe(
                                rreq
                                );
    MPID_Recvq_unlock_exclusive();
    return result;
}

MPID_Request*
MPID_Recvq_dq_posted_or_new_unexpected_unsafe(
    _In_ const MPIDI_Message_match * match,
    _Out_ int * foundp
    );

//
// This function will dequeue or create a new message.  Upon
//  creation, while still under the RecvqLock, it will invoke
//  the InitFunc to initialize/process the state of the message.
//
// Upon failure, this function will remove the request from the unexpected q
//
template<typename FnInitT>
inline MPI_RESULT
MPID_Recvq_dq_posted_or_new_unexpected(
    _In_ const MPIDI_Message_match * match,
    _Out_ int * foundp,
    _Out_ MPID_Request** pRequest,
    _In_ FnInitT InitFunc
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPID_Recvq_lock_exclusive();
    MPID_Request* req = MPID_Recvq_dq_posted_or_new_unexpected_unsafe(
                                match,
                                foundp);
    if (req == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_exit;
    }

    mpi_errno = InitFunc(req, *foundp != FALSE);

    if (mpi_errno != MPI_SUCCESS && *foundp == FALSE)
    {
        MPID_Recvq_erase_unexpected_unsafe( req );
        req->Release();
        req = nullptr;
    }

fn_exit:
    MPID_Recvq_unlock_exclusive();
    *pRequest = req;
    return mpi_errno;
}

void
MPID_Recvq_erase_posted_unsafe(
    _In_ MPID_Request* rreq
    );

void
MPID_Recvq_erase_unexpected_unsafe(
    _In_ MPID_Request* rreq
    );

inline void
MPID_Recvq_erase_unexpected(
    _In_ MPID_Request* rreq
    )
{
    MPID_Recvq_lock_exclusive();
    MPID_Recvq_erase_unexpected_unsafe(rreq);
    MPID_Recvq_unlock_exclusive();
}

void MPID_Recvq_flush_unsafe(void);

inline void MPID_Recvq_flush(void)
{
    MPID_Recvq_lock_exclusive();
    MPID_Recvq_flush_unsafe();
    MPID_Recvq_unlock_exclusive();
}


MPI_RESULT
MPIDI_CH3U_Request_load_send_iov(
    _In_ MPID_Request* sreq
    );

MPI_RESULT
MPIDI_CH3U_Request_load_recv_iov(
    _In_ MPID_Request* rreq
    );

void
MPIDI_CH3U_Request_unpack_uebuf(
    _In_ MPID_Request * rreq
    );

void
MPIDI_CH3U_Request_unpack_srbuf(
    _In_ MPID_Request * rreq
    );


/*
 * Channel upcall prototypes
 */

MPI_RESULT MPIDI_CH3U_Post_data_receive(int found, MPID_Request * rreq);
MPI_RESULT MPIDI_CH3U_Post_data_receive_found(MPID_Request * rreqp);

MPI_RESULT MPIDI_CH3U_Post_data_receive_unexpected(MPID_Request * rreqp);

/*@
  MPIDI_CH3_GetParentPort - obtain the port name associated with the parent

  Output Parameters:
.  parent_port_name - the port name associated with the parent communicator

  Return value:
  A MPI error code.

  NOTE:
  'MPIDI_CH3_GetParentPort' should only be called if the initialization
  (in the current implementation, done with the static function
  'InitPGFromPMI' in 'mpid_init.c') has determined that this process
  in fact has a parent.
@*/
MPI_RESULT MPIDI_CH3_GetParentPort( _Outptr_result_z_ char ** parent_port_name);

/*@
   MPIDI_CH3_FreeParentPort - This routine frees the storage associated with
   a parent port (allocted with MPIDH_CH3_GetParentPort).

  @*/
void MPIDI_CH3_FreeParentPort( void );

/* added by brad.  business card related global and functions */
#define MAX_HOST_DESCRIPTION_LEN 256
int MPIDI_CH3_Get_business_card(_Out_writes_z_ ( length ) char *value, int length);


/*
 * Channel upcall prototypes
 */

/*E
  MPIDI_CH3U_Handle_recv_pkt- Handle a freshly received CH3 packet.

  Input Parameters:
+ vc - virtual connection over which the packet was received
- pkt - pointer to the CH3 packet

  Output Parameter:
. rreqp - receive request defining data to be received; may be NULL

  NOTE:
  Multiple threads may not simultaneously call this routine with the same
  virtual connection.  This constraint eliminates the
  need to lock the VC and thus improves performance.  If simultaneous upcalls
  for a single VC are a possible, then the calling
  routine must serialize the calls (perhaps by locking the VC).  Special
  consideration may need to be given to packet ordering
  if the channel has made guarantees about ordering.
E*/
MPI_RESULT
MPIDI_CH3U_Handle_recv_pkt(
    MPIDI_VC_t* vc,
    const MPIDI_CH3_Pkt_t* pkt,
    MPID_Request** rreqp
    );

/*@
  MPIDI_CH3U_Handle_recv_req - Process a receive request for which all of the
  data has been received (and copied) into the
  buffers described by the request's IOV.

  Input Parameters:
+ vc - virtual connection over which the data was received
- rreq - pointer to the receive request object

  Output Parameter:
. complete - data transfer for the request has completed
@*/
MPI_RESULT
MPIDI_CH3U_Handle_recv_req(
    MPIDI_VC_t* vc,
    MPID_Request* rreq,
    int* complete
    );

/*@
  MPIDI_CH3U_Handle_send_req - Process a send request for which all of the
  data described the request's IOV has been completely
  buffered and/or sent.

  Input Parameters:
+ vc - virtual connection over which the data was sent
- sreq - pointer to the send request object

  Output Parameter:
. complete - data transfer for the request has completed
@*/
MPI_RESULT MPIDI_CH3U_Handle_send_req(
    MPIDI_VC_t * vc,
    MPID_Request * sreq,
    int * complete
    );

void MPIDI_CH3U_Handle_connection(MPIDI_VC_t * vc, MPIDI_VC_Event_t event);

MPI_RESULT MPIDI_CH3U_VC_SendClose( MPIDI_VC_t *vc, int rank );
MPI_RESULT MPIDI_CH3U_VC_WaitForClose( void );
#ifdef MPIDI_CH3_HAS_CHANNEL_CLOSE
int MPIDI_CH3_Channel_close( void );
#else
#define MPIDI_CH3_Channel_close( )   MPI_SUCCESS
#endif

/*@
  MPIDI_CH3_Init - Initialize the channel implementation.

  Input Parameters:
+ has_parent - boolean value that is true if this MPI job was spawned by
  another set of MPI processes
. pg_ptr - the new process group representing MPI_COMM_WORLD
- pg_rank - my rank in the process group

  Return value:
  A MPI error code.

Notes:
MPID_Init has called 'PMI_Init' and created the process group structure
before this routine is called.
@*/
int MPIDI_CH3_Init(int has_parent, int pg_rank, const MPIDI_PG_t* worldpg);

/*@
  MPIDI_CH3_Finalize - Shutdown the channel implementation.

  Return value:
  A MPI error class.
@*/
int MPIDI_CH3_Finalize(void);

/*@
  MPIDI_CH3_VC_Init - Perform channel-specific initialization of a VC

  Input Parameter:
. vc - Virtual connection to initialize
  @*/
void MPIDI_CH3_VC_Init(MPIDI_VC_t* vc);

/*@
   MPIDI_CH3_PG_Destroy - Perform any channel-specific actions when freeing
   a process group

    Input Parameter:
.   pg - Process group on which to act
@*/
int MPIDI_CH3_PG_Destroy( MPIDI_PG_t* pg );

/* Routines in support of ch3 */

/* Here are the packet handlers */
MPI_RESULT Handle_MPIDI_CH3_PKT_EAGER_SEND(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_READY_SEND(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);

MPI_RESULT Handle_MPIDI_CH3_PKT_EAGER_SYNC_SEND(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_EAGER_SYNC_ACK(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);

MPI_RESULT Handle_MPIDI_CH3_PKT_RNDV_REQ_TO_SEND(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_RNDV_CLR_TO_SEND(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_RNDV_SEND(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);

MPI_RESULT Handle_MPIDI_CH3_PKT_CANCEL_SEND_REQ(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_CANCEL_SEND_RESP(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);

MPI_RESULT Handle_MPIDI_CH3_PKT_PUT(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_ACCUMULATE(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_GET(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_GET_ACCUMULATE(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_COMPARE_AND_SWAP(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_RMA_RESP(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);

MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_GRANTED(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_UNLOCK_DONE(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_PT_RMA_DONE(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_PT_RMA_ERROR(const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_PUT_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_ACCUM_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);
MPI_RESULT Handle_MPIDI_CH3_PKT_LOCK_GET_UNLOCK(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);

MPI_RESULT Handle_MPIDI_CH3_PKT_CLOSE(MPIDI_VC_t* vc, const MPIDI_CH3_Pkt_t* pkt, MPID_Request** rreqp);

/* Routines to create packets (used in implementing MPI communications */
MPI_RESULT
MPIDI_CH3_SendEager(
    MPID_Request* sreq,
    MPIDI_CH3_Pkt_type_t reqtype,
    MPIDI_msg_sz_t data_sz,
    int dt_contig,
    MPI_Aint dt_true_lb
    );

MPI_RESULT
MPIDI_CH3_SendRndv(
    MPID_Request* sreq,
    MPIDI_msg_sz_t data_sz
    );

MPI_RESULT
MPIDI_CH3_SendNoncontig(
    MPIDI_VC_t* vc,
    MPID_Request* sreq
    );

/* Routines to ack packets, called in the receive routines when a
   message is matched */
MPI_RESULT MPIDI_CH3_EagerSyncAck( MPIDI_VC_t *, const MPID_Request * );
MPI_RESULT MPIDI_CH3_RecvRndv( MPIDI_VC_t *, MPID_Request * );

/* Handler routines to continuing after an IOV is processed (assigned to the
   OnDataAvail field in the device part of a request) */
MPI_RESULT
MPIDI_CH3_RecvHandler_UnpackUEBufComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_ReloadIOV(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_UnpackSRBufReloadIOV(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_UnpackSRBufComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_PutRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_PutRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_AccumRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_GetAccumRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
);

MPI_RESULT
MPIDI_CH3_RecvHandler_AccumRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_CompareSwapRespComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
);

MPI_RESULT
MPIDI_CH3_RecvHandler_SinglePutAccumComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_GetRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_RecvHandler_GetAccumulateRespPredefinedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
);

MPI_RESULT
MPIDI_CH3_RecvHandler_GetAccumulateRespDerivedDTComplete(
    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
);

/* Send Handlers */
MPI_RESULT
MPIDI_CH3_SendHandler_ReloadIOV(    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

MPI_RESULT
MPIDI_CH3_SendHandler_GetSendRespComplete(    _Inout_ MPIDI_VC_t * vc,
    _In_ MPID_Request * rreq,
    _Out_ int * complete
    );

//
//SMP utilities
//
MPI_RESULT MPIDI_Populate_vc_node_ids(MPIDI_PG_t *pg);

//
// A completion handler to process RPC Async Notification
//
MPI_RESULT
WINAPI
RpcAsyncNotificationHandler(
    _In_ DWORD                 bytesTransferred,
    _In_ VOID*                 pOverlapped
    );

#endif /* !defined(MPICH_MPIDIMPL_H_INCLUDED) */
