// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "pmi.h"

/* What is the arrangement of VCRT and VCR and VC?

   Each VC (the virtual connection itself) is refered to by a reference
   (pointer) or VCR.
   Each communicator has a VCRT, which is nothing more than a
   structure containing a count (size) and an array of pointers to
   virtual connections (as an abstraction, this could be a sparse
   array, allowing a more scalable representation on massively
   parallel systems).
 */


//
// Returns whether there are any active connections.
//
static bool pending_vcs(_In_ const MPIDI_VCRT_t *vcrt)
{
    const MPIDI_VC_t* const * vc = vcrt->vcr_table;
    int size = vcrt->size;

    /* Compute the number of pending ops.
       A virtual connection has pending operations if the state
       is not INACTIVE or if the sendq is not null */
    for (int i = 0; i < size; i++)
    {
        if (vc[i]->state != MPIDI_VC_STATE_INACTIVE)
        {
            return true;
        }
    }

    return false;
}


static MPI_RESULT MPIDI_CH3U_VC_FinishPending( MPIDI_VCRT_t *vcrt )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    while(pending_vcs(vcrt))
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    };

fn_exit:
    return mpi_errno;

fn_fail:
    goto fn_exit;
}


/*@
  MPID_VCRT_Create - Create a table of VC references

  Notes:
  This routine only provides space for the VC references.  Those should
  be added by assigning to elements of the vc array within the 'MPID_VCRT' object.
  @*/
MPID_VCRT* MPID_VCRT_Create(int size)
{
    MPIDI_VCRT_t* vcrt;
    vcrt = (MPIDI_VCRT_t*)MPIU_Malloc(sizeof(MPIDI_VCRT_t) + (size - 1) * sizeof(MPIDI_VC_t *));
    if(vcrt == NULL)
        return NULL;

    MPIU_Object_set_ref(vcrt, 1);
    vcrt->size = size;

    return vcrt;
}

/*@
  MPID_VCRT_Add_ref - Add a reference to a VC reference table

  Notes:
  This is called when a communicator duplicates its group of processes.
  It is used in 'commutil.c' and in routines to create communicators from
  dynamic process operations.  It does not change the state of any of the
  virtural connections (VCs).
  @*/
void MPID_VCRT_Add_ref(MPID_VCRT* vcrt)
{
    MPIU_Object_add_ref(vcrt);
}

/* FIXME: What should this do?  See proc group and vc discussion */

/*@
  MPID_VCRT_Release - Release a reference to a VC reference table

  Notes:

  @*/
MPI_RESULT MPID_VCRT_Release(MPID_VCRT* vcrt, int isDisconnect )
{
    int in_use;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    MPIU_Object_release_ref(vcrt, &in_use);

    /* If this VC reference table is no longer in use, we can
       decrement the reference count of each of the VCs.  If the
       count on the VCs goes to zero, then we can decrement the
       ref count on the process group and so on.
    */
    if (!in_use)
    {
        int i;

        /* FIXME: Need a better way to define how vc's are closed that
         takes into account pending operations on vcs, including
         close events received from other processes. */
        /*
        mpi_errno = MPIDI_CH3U_VC_FinishPending( vcrt );
        ON_ERROR_FAIL(mpi_errno);
        */

        for (i = 0; i < vcrt->size; i++)
        {
            MPIDI_VC_t* vc = vcrt->vcr_table[i];

            MPIDI_VC_release_ref(vc, &in_use);
            /* The rule for disconnect that we use is that if
               MPI_Comm_disconnect removes the last reference to this
               VC, we fully remove the VC.  This is not quite what the
               MPI standard says, but this is sufficient to give the
               expected behavior for most user programs that
               use MPI_Comm_disconnect */
            if (isDisconnect && vc->ref_count == 1)
            {
                MPIDI_VC_release_ref(vc, &in_use);
            }

            if (!in_use)
            {
                /* If the VC is myself then skip the close message */
                if (vc->pg == MPIDI_Process.my_pg &&
                    vc->pg_rank == MPIDI_Process.my_pg_rank)
                {
                    MPIDI_PG_release_ref(vc->pg);
                    continue;
                }

                /* FIXME: the correct test is ACTIVE or REMOTE_CLOSE */
                if (vc->state != MPIDI_VC_STATE_INACTIVE)
                {
                    mpi_errno = MPIDI_CH3U_VC_SendClose( vc, i );
                    if( mpi_errno != MPI_SUCCESS )
                    {
                        return mpi_errno;
                    }
                }
                else
                {
                    MPIDI_PG_release_ref(vc->pg);
                }
            }
        }

        MPIU_Free(vcrt);
    }

    return MPI_SUCCESS;
}

/*@
  MPID_VCRT_Get_ptr - Return a pointer to the array of VCs for this
  reference table

  Notes:
  This routine is always used with MPID_VCRT_Create and should be
  combined with it.

  @*/
MPID_VCR* MPID_VCRT_Get_ptr(MPID_VCRT* vcrt)
{
    return vcrt->vcr_table;
}

/*@
  MPID_VCR_Dup - Duplicate a virtual connection reference

  Notes:
  If the VC is being used for the first time in a VC reference
  table, the reference count is set to two, not one, in order to
  distinquish between freeing a communicator with 'MPI_Comm_free' and
  'MPI_Comm_disconnect', and the reference count on the process group
  is incremented (to indicate that the process group is in use).
  While this has no effect on the process group of 'MPI_COMM_WORLD',
  it is important for process groups accessed through 'MPI_Comm_spawn'
  or 'MPI_Comm_connect/MPI_Comm_accept'.

  @*/
_Ret_notnull_
MPID_VCR MPID_VCR_Dup( _Inout_ MPID_VCR orig_vcr )
{

    /* We are allowed to create a vc that belongs to no process group
     as part of the initial connect/accept action, so in that case,
     ignore the pg ref count update */
    if (orig_vcr->ref_count == 0 && orig_vcr->pg)
    {
        MPIU_Object_set_ref( orig_vcr, 2 );
        MPIDI_PG_add_ref( orig_vcr->pg );
    }
    else
    {
        MPIU_Object_add_ref(orig_vcr);
    }

    return orig_vcr;
}

/*@
  MPID_VCR_Get_lpid - Get the local process ID for a given VC reference
  @*/
int MPID_VCR_Get_lpid(const MPIDI_VC_t * vcr)
{
    return vcr->lpid;
}


/*
 * MPIDI_CH3U_Comm_FinishPending - Complete any pending operations on the
 * communicator.
 *
 * Notes:
 * This should be used before freeing or disconnecting a communicator.
 *
 * For better scalability, we might want to form a list of VC's with
 * pending operations.
 */
MPI_RESULT MPIDI_CH3U_Comm_FinishPending( MPID_Comm *comm_ptr )
{
    int mpi_errno = MPI_SUCCESS;

    mpi_errno = MPIDI_CH3U_VC_FinishPending( comm_ptr->GetVcrt() );
    if (mpi_errno == MPI_SUCCESS && comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        mpi_errno = MPIDI_CH3U_Comm_FinishPending( comm_ptr->inter.local_comm );
    }

    return mpi_errno;
}

/* ----------------------------------------------------------------------- */
/* Routines to initialize a VC */

/*
 * The lpid counter counts new processes that this process knows about.
 */
static int lpid_counter = 0;

/* Fully initialize a VC.  This invokes the channel-specific
   VC initialization routine MPIDI_CH3_VC_Init . */
void MPIDI_VC_Init( MPIDI_VC_t *vc, MPIDI_PG_t *pg, int rank )
{
    vc->state = MPIDI_VC_STATE_INACTIVE;
    MPIU_Object_set_ref(vc, 0);
    vc->handle  = MPID_VCONN;
    vc->pg      = pg;
    vc->pg_rank = rank;
    vc->lpid    = lpid_counter++;
    vc->eager_max_msg_sz      = MPIDI_CH3_EAGER_LIMIT_DEFAULT;
    MPIDI_CH3_VC_Init( vc );
}


/* Count the number of outstanding close requests */
static volatile int MPIDI_Outstanding_close_ops = 0;

/* FIXME: What is this routine for?
   It appears to be used only in ch3_progress, ch3_progress_connect, or
   ch3_progress_sock files.  Is this a general operation, or does it
   belong in util/sock ? It appears to be used in multiple channels,
   but probably belongs in mpid_vc, along with the vc exit code that
   is currently in MPID_Finalize */

/* FIXME: The only event is event_terminated.  Should this have
   a different name/expected function? */

/*@
  MPIDI_CH3U_Handle_connection - handle connection event

  Input Parameters:
+ vc - virtual connection
. event - connection event

  NOTE:
  At present this function is only used for connection termination
@*/
void MPIDI_CH3U_Handle_connection(MPIDI_VC_t * vc, MPIDI_VC_Event_t event)
{
    switch (event)
    {
        case MPIDI_VC_EVENT_TERMINATED:
        {
            switch (vc->state)
            {
                case MPIDI_VC_STATE_CLOSE_ACKED:
                {
                    vc->state = MPIDI_VC_STATE_INACTIVE;
                    /* FIXME: Decrement the reference count?  Who increments? */
                    /* FIXME: The reference count is often already 0.  But
                       not always */
                    /* MPIU_Object_set_ref(vc, 0); ??? */

                    /*
                     * FIXME: The VC used in connect accept has a NULL
                     * process group
                     */
                    if (vc->pg != NULL && vc->ref_count == 0)
                    {
                        /* FIXME: Who increments the reference count that
                           this is decrementing? */
                        /* When the reference count for a vc becomes zero,
                           decrement the reference count
                           of the associated process group.  */
                        /* FIXME: This should be done when the reference
                           count of the vc is first decremented */
                        MPIDI_PG_release_ref(vc->pg);
                    }

                    /* MT: this is not thread safe */
                    MPIDI_Outstanding_close_ops -= 1;
                    Mpi.SignalProgress();
                    MPIDI_CH3_Channel_close();
                    break;
                }

                default:
                {
                    break;
                }
            }

            break;
        }

        default:
        {
            break;
        }
    }
}


/*@
  MPIDI_CH3U_VC_SendClose - Initiate a close on a virtual connection

  Input Parameters:
+ vc - Virtual connection to close
- i  - rank of virtual connection within a process group (used for debugging)

  Notes:
  The current state of this connection must be either 'MPIDI_VC_STATE_ACTIVE'
  or 'MPIDI_VC_STATE_REMOTE_CLOSE'.
  @*/
MPI_RESULT MPIDI_CH3U_VC_SendClose( MPIDI_VC_t *vc, int /*rank*/ )
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_close_t * close_pkt = &upkt.close;
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    /* FIXME: Remove this IFDEF */
#ifdef MPIDI_CH3_USES_SSHM
    MPIU_Assert( vc->state == MPIDI_VC_STATE_ACTIVE ||
                 vc->state == MPIDI_VC_STATE_REMOTE_CLOSE
                 /* sshm queues are uni-directional.  A VC that is connected
                 * in the read direction is marked MPIDI_VC_STATE_INACTIVE
                 * so that a connection will be formed on the first write.
                 * Since the other side is marked MPIDI_VC_STATE_ACTIVE for
                 * writing
                 * we need to initiate the close protocol on the read side
                 * even if the write state is MPIDI_VC_STATE_INACTIVE. */
                 //
                 // leonidm-01/23/2008: This case is not handled in the close
                 // logic below, and the Assert in the else clause will specifically
                 // override the condition below.
                 //
                || ((vc->state == MPIDI_VC_STATE_INACTIVE) && vc->ch.shm.recv.connected) );
#else
    MPIU_Assert( vc->state == MPIDI_VC_STATE_ACTIVE ||
                 vc->state == MPIDI_VC_STATE_REMOTE_CLOSE );
#endif


    MPIDI_Pkt_init(close_pkt, MPIDI_CH3_PKT_CLOSE);
    close_pkt->ack = (vc->state == MPIDI_VC_STATE_ACTIVE) ? FALSE : TRUE;

    /* MT: this is not thread safe */
    MPIDI_Outstanding_close_ops += 1;

    /*
     * A close packet acknowledging this close request could be
     * received during iStartMsg, therefore the state must
     * be changed before the close packet is sent.
     */
    if (vc->state == MPIDI_VC_STATE_ACTIVE)
    {
        vc->state = MPIDI_VC_STATE_LOCAL_CLOSE;
    }
    else
    {
        MPIU_Assert( vc->state == MPIDI_VC_STATE_REMOTE_CLOSE );
        vc->state = MPIDI_VC_STATE_CLOSE_ACKED;
    }

    mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
    ON_ERROR_FAIL(mpi_errno);

fn_fail:
    return mpi_errno;
}


/*
 * This routine can be called to progress until all pending close operations
 * (initiated in the SendClose routine above) are completed.  It is
 * used in MPID_Finalize and MPID_Comm_disconnect.
 */
/*@
  MPIDI_CH3U_VC_WaitForClose - Wait for all virtual connections to close
  @*/
MPI_RESULT MPIDI_CH3U_VC_WaitForClose( void )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    while(MPIDI_Outstanding_close_ops > 0)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

fn_fail:
    return mpi_errno;
}
