// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "DynProcAlloc.h"


/* This is the utility file for comm that contains the basic comm items
   and storage management */
#ifndef MPID_COMM_PREALLOC
#define MPID_COMM_PREALLOC 8
#endif

C_ASSERT( HANDLE_GET_TYPE(MPI_COMM_WORLD) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_COMM_WORLD) == MPID_COMM );

C_ASSERT( HANDLE_GET_TYPE(MPI_COMM_SELF) == HANDLE_TYPE_BUILTIN );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_COMM_SELF) == MPID_COMM );

C_ASSERT( HANDLE_GET_TYPE(MPI_COMM_NULL) == HANDLE_TYPE_INVALID );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_COMM_NULL) == MPID_COMM );

bool g_SmpCollectiveEnabled;

bool g_SmpBcastEnabled;
bool g_SmpBarrierEnabled;
bool g_SmpReduceEnabled;
bool g_SmpAllreduceEnabled;

const char* mpi_comm_names[CommTraits::BUILTIN + 1] =
{
    "MPI_COMM_WORLD",
    "MPI_COMM_SELF",
    "",
    ""
};

CommPool CommPool::s_instance;

/* FIXME :
   Reusing context ids can lead to a race condition if (as is desirable)
   MPI_Comm_free does not include a barrier.  Consider the following:
   Process A frees the communicator.
   Process A creates a new communicator, reusing the just released id
   Process B sends a message to A on the old communicator.
   Process A receives the message, and believes that it belongs to the
   new communicator.
   Process B then cancels the message, and frees the communicator.

   The likelyhood of this happening can be reduced by introducing a gap
   between when a context id is released and when it is reused.  An alternative
   is to use an explicit message (in the implementation of MPI_Comm_free)
   to indicate that a communicator is being freed; this will often require
   less communication than a barrier in MPI_Comm_free, and will ensure that
   no messages are later sent to the same communicator (we may also want to
   have a similar check when building fault-tolerant versions of MPI).
 */

/* Create a new communicator with a context.
   Do *not* initialize the other fields except for the reference count.
   See MPIR_Comm_copy for a function to produce a copy of part of a
   communicator
*/

/*
    Create a communicator structure and perform basic initialization
    (mostly clearing fields and updating the reference count).
 */
MPI_RESULT MPIR_Comm_create( MPID_Comm **newcomm_ptr, MPID_Comm_kind_t kind )
{
    MPID_Comm *newptr = CommPool::Alloc();
    if( newptr == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    /* Clear many items (empty means to use the default; some of these
       may be overridden within the communicator initialization) */
    newptr->errhandler      = 0;
    newptr->attributes      = 0;
    newptr->group           = nullptr;
    newptr->name            = mpi_comm_names[CommTraits::BUILTIN];
    newptr->comm_kind       = kind;

    //
    // Initialize tag for NBC. This needs to be done here instead of in
    // the ctor because of the pool design.
    //
    newptr->nbc_tag_seq     = MPIR_NBC_BEGIN_TAG_SEQUENCE;

    if( kind != MPID_INTERCOMM )
    {
        newptr->intra.local_subcomm   = nullptr;
        newptr->intra.leaders_subcomm = nullptr;
        newptr->intra.ha_mappings     = nullptr;
    }
    else
    {
        newptr->inter.local_comm = nullptr;
    }

    /* Fields not set include context_id, remote and local size
       since different communicator construction routines need
       different values */

#ifdef HAVE_DEBUGGER_SUPPORT
    newptr->comm_next = nullptr;
#endif

    /* Insert this new communicator into the list of known communicators.
       Make this conditional on debugger support to match the test in
       MPIR_Comm_release . */
    MPIR_COMML_REMEMBER( newptr );

    *newcomm_ptr = newptr;

    return MPI_SUCCESS;
}


static MPI_RESULT MPIR_Comm_commit_socket_level(MPID_Comm *comm)
{
    int rank;
    int num_local = -1, num_external = -1;      // number of ranks on the same socket as this process, and of those on other sockets

    MPIU_Assert(comm->intra.local_subcomm == NULL);
    MPIU_Assert(comm->intra.leaders_subcomm == NULL);
    MPIU_Assert(comm->comm_kind == MPID_INTRACOMM_PARENT || comm->comm_kind == MPID_INTRACOMM_NODE);
    MPIU_Assert(comm->intra.ha_mappings != nullptr);

    if (MPIU_Should_use_socket_awareness(comm) == false)
    {
        return MPI_SUCCESS;
    }

    StackGuardArray<int> local_procs = new int[comm->remote_size * 2];
    if( local_procs == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    int* external_procs = local_procs + comm->remote_size;

    MPI_RESULT mpi_errno = MPIU_Find_socket_local_and_leaders(
        comm,
        &num_local,
        local_procs,
        &num_external,
        external_procs,
        comm->intra.ha_mappings
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    MPIU_Assert(num_local > 0);

    /* if the socket_roots_comm and comm would be the same size, then creating
        the second communicator is useless and wasteful. */
    if (num_external == comm->remote_size)  // @TODO: Revisit. We may need a stricter check here,
                                            // as we now constrain  num_external
                                            // to nodelocal ranks (but on other socket)
                                            // Although MPIU_Should_use_socket_awareness()
                                            // seems to guard against unwanted cases
    {
        MPIU_Assert(num_local == 1);
        goto fn_exit;
    }

    /* we don't need a local comm if this process is the only one on this socket */
    if (num_local > 1)
    {
        mpi_errno = MPIR_Comm_create(&comm->intra.local_subcomm, MPID_INTRACOMM_SOCKET);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        comm->intra.local_subcomm->context_id = comm->context_id;
        comm->intra.local_subcomm->recvcontext_id = comm->intra.local_subcomm->context_id;
#if DBG
        //
        // For the assertion below.
        //
        comm->intra.local_subcomm->rank = -1;
#endif
        comm->intra.local_subcomm->remote_size  = num_local;

        MPID_VCRT* vcrt = MPID_VCRT_Create( num_local );
        if( vcrt == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail2;
        }
        comm->intra.local_subcomm->vcr = MPID_VCRT_Get_ptr( vcrt );
        MPIU_Assert( local_procs != nullptr );

        rank = num_local;
        do
        {
            --rank;
            if (local_procs[rank] == comm->rank)
            {
                MPIU_Assert(
                    num_external != 0 ||
                    comm->intra.ha_mappings[comm->rank].rank == static_cast<ULONG>(rank)
                    );
                comm->intra.local_subcomm->rank = rank;
            }

            /* For rank i in the new communicator, find the corresponding rank in the input communicator */
            comm->intra.local_subcomm->vcr[rank] =
                MPID_VCR_Dup( comm->vcr[local_procs[rank]] );

        } while( rank != 0 );

        MPIU_Assert( comm->intra.local_subcomm->rank != -1 );
        /* don't call MPIR_Comm_commit here */
    }

    /* this process may not be a member of the socket_roots_comm */
    if (num_external > 0)
    {
        mpi_errno = MPIR_Comm_create(&comm->intra.leaders_subcomm, MPID_INTRACOMM_SOCKET_ROOT);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail2;
        }

        comm->intra.leaders_subcomm->context_id = comm->context_id;
        comm->intra.leaders_subcomm->recvcontext_id = comm->intra.leaders_subcomm->context_id;
#if DBG
        //
        // For the assertion below.
        //
        comm->intra.leaders_subcomm->rank = -1;
#endif
        comm->intra.leaders_subcomm->remote_size  = num_external;

        MPID_VCRT* vcrt = MPID_VCRT_Create( num_external );
        if( vcrt == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail3;
        }
        comm->intra.leaders_subcomm->vcr = MPID_VCRT_Get_ptr( vcrt );
        MPIU_Assert( external_procs != nullptr );

        rank = num_external;
        do
        {
            --rank;
            if (external_procs[rank] == comm->rank)
            {
                comm->intra.leaders_subcomm->rank = rank;
            }
            /* For rank i in the new communicator, find the corresponding
               rank in the input communicator */
            comm->intra.leaders_subcomm->vcr[rank] =
                MPID_VCR_Dup( comm->vcr[external_procs[rank]] );

        } while( rank != 0 );

        MPIU_Assert( comm->intra.leaders_subcomm->rank != -1 );
        /* don't call MPIR_Comm_commit here */
    }

fn_exit:
    return mpi_errno;

fn_fail3:
    MPIR_Comm_release( comm->intra.leaders_subcomm, 0 );
    comm->intra.leaders_subcomm = nullptr;

fn_fail2:
    MPIR_Comm_release( comm->intra.local_subcomm, 0 );
    comm->intra.local_subcomm = nullptr;

fn_fail:
    goto fn_exit;
}


/* Provides a hook for the top level functions to perform some manipulation on a
   communicator just before it is given to the application level.

   For example, we create sub-communicators for SMP-aware collectives at this
   step. */
static MPI_RESULT MPIR_Comm_commit_node_level(MPID_Comm *comm)
{
    int rank;
    int num_local = -1, num_external = -1;

    //
    // It's OK to relax these assertions, but we should do so very
    // intentionally.  For now this function is the only place that we create
    // our hierarchy of communicators
    //
    MPIU_Assert(comm->intra.local_subcomm == nullptr);
    MPIU_Assert(comm->intra.leaders_subcomm == nullptr);
    MPIU_Assert(comm->comm_kind == MPID_INTRACOMM_PARENT);
    MPIU_Assert(comm->intra.ha_mappings != nullptr);

    StackGuardArray<int> local_procs = new int[comm->remote_size * 2];
    if( local_procs == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    int* external_procs = local_procs + comm->remote_size;

    MPI_RESULT mpi_errno = MPIU_Find_node_local_and_leaders(
        comm,
        &num_local,
        local_procs,
        &num_external,
        external_procs,
        comm->intra.ha_mappings
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    MPIU_Assert(num_local > 0);

    //
    // If there is only one process per node, there's nothing to optimize.
    //
    if( num_external == comm->remote_size )
    {
        return MPI_SUCCESS;
    }

    //
    // If all processes are local optimize at the socket level.
    //
    if( num_local == comm->remote_size )
    {
        return MPIR_Comm_commit_socket_level( comm );
    }

    //
    // Only create the local subcomm if there are more than one processes on this node.
    //
    if (num_local > 1)
    {
        mpi_errno = MPIR_Comm_create(&comm->intra.local_subcomm, MPID_INTRACOMM_NODE);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        comm->intra.local_subcomm->context_id = comm->context_id;
        comm->intra.local_subcomm->recvcontext_id = comm->recvcontext_id;
#if DBG
        //
        // For the assertion below.
        //
        comm->intra.local_subcomm->rank = -1;
#endif
        comm->intra.local_subcomm->remote_size  = num_local;

        MPID_VCRT* vcrt = MPID_VCRT_Create( num_local );
        if( vcrt == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail2;
        }
        comm->intra.local_subcomm->vcr = MPID_VCRT_Get_ptr( vcrt );
        MPIU_Assert( local_procs != nullptr );

        rank = num_local;
        do
        {
            --rank;

            if (local_procs[rank] == comm->rank)
            {
                //
                // If we're rank 0 in the local subcomm, we are part of the leader subcomm.
                //
                MPIU_Assert(
                    rank == 0
                    || comm->intra.ha_mappings[comm->rank].rank == static_cast<ULONG>(rank)
                    );
                MPIU_Assert( rank == 0 || comm->intra.ha_mappings[comm->rank].isLocal == 1 );
                MPIU_Assert( rank != 0 || comm->intra.ha_mappings[comm->rank].isLocal == 0 );
                comm->intra.local_subcomm->rank = rank;
            }
            /* For rank i in the new communicator, find the corresponding
               rank in the input communicator */
            comm->intra.local_subcomm->vcr[rank] = MPID_VCR_Dup( comm->vcr[local_procs[rank]] );

        } while( rank != 0 );

        MPIU_Assert( comm->intra.local_subcomm->rank != -1 );

        //
        // Allocate a mapping table for socket-awareness.
        //
        comm->intra.local_subcomm->intra.ha_mappings =
            new HA_rank_mapping[comm->intra.local_subcomm->remote_size];
        if( comm->intra.local_subcomm->intra.ha_mappings == nullptr )
        {
            goto fn_fail2;
        }

        mpi_errno = MPIR_Comm_commit_socket_level( comm->intra.local_subcomm );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail2;
        }
    }

    //
    // Only create the leaders subcomm if we are a leader.
    //
    if (num_external != 0)
    {
        mpi_errno = MPIR_Comm_create(&comm->intra.leaders_subcomm, MPID_INTRACOMM_NODE_ROOT);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail2;
        }

        comm->intra.leaders_subcomm->context_id = comm->context_id;
        comm->intra.leaders_subcomm->recvcontext_id = comm->recvcontext_id;
#if DBG
        //
        // For the assertion below.
        //
        comm->intra.leaders_subcomm->rank = -1;
#endif
        comm->intra.leaders_subcomm->remote_size  = num_external;

        MPID_VCRT* vcrt = MPID_VCRT_Create( num_external );
        if( vcrt == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail3;
        }
        comm->intra.leaders_subcomm->vcr = MPID_VCRT_Get_ptr( vcrt );
        MPIU_Assert( external_procs != nullptr );

        rank = num_external;
        do
        {
            --rank;

            if (external_procs[rank] == comm->rank)
            {
                comm->intra.leaders_subcomm->rank = rank;
            }

            /* For rank i in the new communicator, find the corresponding
                rank in the input communicator */
            comm->intra.leaders_subcomm->vcr[rank] =
                MPID_VCR_Dup( comm->vcr[external_procs[rank]] );

        } while( rank != 0 );

        MPIU_Assert( comm->intra.leaders_subcomm->rank != -1 );
        /* don't call MPIR_Comm_commit here */
    }

fn_exit:
    return mpi_errno;

fn_fail3:
    MPIR_Comm_release( comm->intra.leaders_subcomm, 0 );
    comm->intra.leaders_subcomm = nullptr;

fn_fail2:
    MPIR_Comm_release( comm->intra.local_subcomm, 0 );
    comm->intra.local_subcomm = nullptr;

fn_fail:
    goto fn_exit;
}


MPI_RESULT MPIR_Comm_commit(MPID_Comm *comm)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    //
    // TODO: If we have errors committing the subcomms, we could flip the parent to FLAT and
    // be a bit more resilient.
    //
    if (comm->comm_kind == MPID_INTRACOMM_PARENT)
    {
        //
        // Allocate and initialize the mappings table.
        //
        comm->intra.ha_mappings = new HA_rank_mapping[comm->remote_size];
        if( comm->intra.ha_mappings == nullptr )
        {
            //
            // Flip the communicator kind to indicate that it is not HA capable.
            //
            comm->comm_kind = MPID_INTRACOMM_FLAT;
            goto fn_exit;
        }

        mpi_errno = MPIR_Comm_commit_node_level(comm);
        ON_ERROR_FAIL(mpi_errno);
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


//
// The MPI_CONTEXT_ID contains 127 bits of random data.
//  the last bit is set only for temporary use during merge operations.
//
MPI_RESULT
MPIR_GenerateContextID(
    _Out_ MPI_CONTEXT_ID*   contextId
    )
{
    RPC_STATUS status = UuidCreate(contextId);
    if (RPC_S_OK != status)
    {
        return MPIU_ERR_CREATE(MPI_ERR_INTERN, "**uuidgenfailed");
    }
    contextId->Data1 &= ~0x80000000;
    return MPI_SUCCESS;
}


VOID
MPIR_Create_temp_contextid(
    _Out_ MPI_CONTEXT_ID*   new_contextid,
    _In_ const MPI_CONTEXT_ID& contextid
    )
{
    *new_contextid = contextid;
    new_contextid->Data1 |= 0x80000000;
}


MPI_RESULT
MPIR_Get_contextid(
    _In_  const MPID_Comm* comm_ptr,
    _Out_ MPI_CONTEXT_ID*  contextid,
    _In_  bool             abort )
{
    MPI_RESULT mpi_errno;

    MPIU_Assert( comm_ptr->comm_kind != MPID_INTERCOMM);

    *contextid = GUID_NULL;

    if (comm_ptr->rank == 0)
    {
        if (abort == false)
        {
            mpi_errno = MPIR_GenerateContextID(contextid);
            if (mpi_errno != MPI_SUCCESS)
            {
                //
                // If we failed to create an ID, we will send GUID_NULL
                // out to the rest to indicate failure.
                //
                *contextid = GUID_NULL;
            }
        }
    }

    mpi_errno = MPIR_Bcast_intra(
        contextid,
        sizeof(*contextid),
        g_hBuiltinTypes.MPI_Byte,
        0,
        comm_ptr
        );
    if (MPI_SUCCESS == mpi_errno)
    {
        if (*contextid == GUID_NULL)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_INTERN, "**errcontextid");
        }
    }
    return mpi_errno;
}


/* Get a context for a new intercomm.  There are two approaches
   here (for MPI-1 codes only)
   (a) Each local group gets a context; the groups exchange, and
       the low value is accepted and the high one returned.  This
       works because the context ids are taken from the same pool.
   (b) Form a temporary intracomm over all processes and use that
       with the regular algorithm.

   In some ways, (a) is the better approach because it is the one that
   extends to MPI-2 (where the last step, returning the context, is
   not used and instead separate send and receive context id value
   are kept).  For this reason, we'll use (a).

   Even better is to separate the local and remote context ids.  Then
   each group of processes can manage their context ids separately.
*/
/*
 * This uses the thread-safe (if necessary) routine to get a context id
 * and does not need its own thread-safe version.
 */
MPI_RESULT MPIR_Get_intercomm_contextid(
    _In_ const MPID_Comm* comm_ptr,
    _Out_ MPI_CONTEXT_ID* context_id,
    _Out_ MPI_CONTEXT_ID* recvcontext_id)
{
    MPI_CONTEXT_ID mycontext_id, remote_context_id;

    /*printf( "local comm size is %d and intercomm local size is %d\n",
      comm_ptr->inter.local_comm->remote_size, comm_ptr->inter.local_size );*/
    MPI_RESULT mpi_errno = MPIR_Get_contextid(comm_ptr->inter.local_comm, &mycontext_id);
    ON_ERROR_FAIL(mpi_errno);

    *context_id = GUID_NULL;
    *recvcontext_id = GUID_NULL;

    /* MPIC routine uses an internal context id.  The local leads (process 0)
       exchange data */
    remote_context_id = GUID_NULL;
    if (comm_ptr->rank == 0)
    {
        mpi_errno = MPIC_Sendrecv( &mycontext_id, sizeof(mycontext_id), g_hBuiltinTypes.MPI_Byte, 0, MPIR_TAG_GET_CONTEXT_ID,
                       &remote_context_id, sizeof(remote_context_id), g_hBuiltinTypes.MPI_Byte, 0, MPIR_TAG_GET_CONTEXT_ID,
                       comm_ptr, MPI_STATUS_IGNORE );
    }

    /* Make sure that all of the local processes now have this id */
    mpi_errno = MPIR_Bcast_intra( &remote_context_id, sizeof(remote_context_id), g_hBuiltinTypes.MPI_Byte,
                            0, comm_ptr->inter.local_comm );

    *context_id     = remote_context_id;
    *recvcontext_id = mycontext_id;
fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}

/*
 * Copy a communicator, including creating a new context and copying the
 * virtual connection tables and clearing the various fields.
 * Does *not* copy attributes.  If size is < the size of the (local group
 * in the ) input communicator, copy only the first size elements.
 * If this process is not a member, return a null pointer in outcomm_ptr.
 * This is only supported in the case where the communicator is in
 * Intracomm (not an Intercomm).  Note that this is all that is required
 * for cart_create and graph_create.
 *
 * Used by cart_create, graph_create, and dup_create
 */
MPI_RESULT
MPIR_Comm_copy(
    _In_ const MPID_Comm* comm_ptr,
    _In_range_(>, 0) int size,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    )
{
    MPI_RESULT mpi_errno;
    MPI_CONTEXT_ID new_context_id, new_recvcontext_id;
    MPID_Comm *newcomm_ptr;

    //
    // We only allow "shrinking" for intracomms.
    //
    MPIU_Assert( comm_ptr->comm_kind != MPID_INTERCOMM ||
                 comm_ptr->inter.local_size == size );

    Mpi.AcquireCommLock();

    /* Get a new context first.  We need this to be collective over the
       input communicator */
    /* If there is a context id cache in oldcomm, use it here.  Otherwise,
       use the appropriate algorithm to get a new context id.  Be careful
       of intercomms here */
    if (comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        mpi_errno =
            MPIR_Get_intercomm_contextid(
                 comm_ptr, &new_context_id, &new_recvcontext_id );
    }
    else
    {
        mpi_errno = MPIR_Get_contextid( comm_ptr, &new_context_id);
        new_recvcontext_id = new_context_id;
    }

    ON_ERROR_FAIL(mpi_errno);

    /* This is the local size, not the remote size, in the case of
       an intercomm */
    if (comm_ptr->rank >= size)
    {
        *outcomm_ptr = 0;
        goto fn_exit;
    }

    /* We're left with the processes that will have a non-null communicator.
       Create the object, initialize the data, and return the result */

    mpi_errno = MPIR_Comm_create( &newcomm_ptr, comm_ptr->comm_kind );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    newcomm_ptr->context_id = new_context_id;
    newcomm_ptr->recvcontext_id = new_recvcontext_id;

    /* There are two cases here - size is the same as the old communicator,
       or it is smaller.  If the size is the same, we can just add a reference.
       Otherwise, we need to create a new VCRT.  Note that this is the
       test that matches the test on rank above. */
    if (comm_ptr->comm_kind == MPID_INTERCOMM || size == comm_ptr->remote_size)
    {
        /* Duplicate the VCRT references */
        MPID_VCRT_Add_ref( comm_ptr->GetVcrt() );
        newcomm_ptr->vcr  = comm_ptr->vcr;
    }
    else
    {
        int i;
        /* The "remote" vcr gets the shortened vcrt */
        MPID_VCRT* vcrt = MPID_VCRT_Create( size );
        if( vcrt == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        newcomm_ptr->vcr = MPID_VCRT_Get_ptr( vcrt );
        for (i=0; i<size; i++)
        {
            /* For rank i in the new communicator, find the corresponding
               rank in the input communicator */
            newcomm_ptr->vcr[i] = MPID_VCR_Dup( comm_ptr->vcr[i] );
        }
    }

    /* Set the sizes and ranks */
    newcomm_ptr->rank        = comm_ptr->rank;
    if (comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        newcomm_ptr->inter.local_size   = comm_ptr->inter.local_size;
        newcomm_ptr->remote_size  = comm_ptr->remote_size;
        newcomm_ptr->inter.is_low_group = comm_ptr->inter.is_low_group;

        mpi_errno = MPIR_Comm_copy(
            comm_ptr->inter.local_comm,
            comm_ptr->inter.local_comm->remote_size,
            &newcomm_ptr->inter.local_comm
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }
    else
    {
        newcomm_ptr->remote_size  = size;
        //
        // Inherit HA collective property from the parent.
        //
        newcomm_ptr->comm_kind = comm_ptr->comm_kind;
    }

    /* Inherit the error handler (if any) */
    newcomm_ptr->errhandler = comm_ptr->errhandler;
    if (comm_ptr->errhandler)
    {
        MPIR_Errhandler_add_ref( comm_ptr->errhandler );
    }

    mpi_errno = MPIR_Comm_commit(newcomm_ptr);
    ON_ERROR_FAIL(mpi_errno);

    /* Start with no attributes on this communicator */
    newcomm_ptr->attributes = 0;
    *outcomm_ptr = newcomm_ptr;

fn_exit:
    Mpi.ReleaseCommLock();
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


MPI_RESULT MPID_Comm::Destroy(MPID_Comm * comm_ptr, int isDisconnect)
{
    MPIU_Assert( comm_ptr->attributes == NULL );

    /* If this communicator is our parent, and we're disconnecting
        from the parent, mark that fact */
    if (Mpi.CommParent == comm_ptr)
    {
        Mpi.CommParent = nullptr;
    }

    /* Free the VCRT */
    MPI_RESULT mpi_errno = MPID_VCRT_Release(comm_ptr->GetVcrt(), isDisconnect);
    ON_ERROR_FAIL(mpi_errno);

    if (comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        if (comm_ptr->inter.local_comm != nullptr )
        {
            MPIR_Comm_release(comm_ptr->inter.local_comm, isDisconnect );
        }
    }
    else
    {
        /* free the intra/inter-node/socket communicators, if they exist */
        if (comm_ptr->intra.local_subcomm != nullptr)
        {
            MPIR_Comm_release(comm_ptr->intra.local_subcomm, isDisconnect);
        }
        if (comm_ptr->intra.leaders_subcomm != nullptr )
        {
            MPIR_Comm_release(comm_ptr->intra.leaders_subcomm, isDisconnect);
        }
        if (comm_ptr->intra.ha_mappings != nullptr )
        {
            delete[] comm_ptr->intra.ha_mappings;
        }
    }

    /* Free the local and remote groups, if they exist */
    if (comm_ptr->group)
    {
        MPIR_Group_release(comm_ptr->group);
    }

    /* Remove from the list of active communicators if
        we are supporting message-queue debugging.  We make this
        conditional on having debugger support since the
        operation is not constant-time */
    MPIR_COMML_FORGET( comm_ptr );

    CommPool::Free( comm_ptr );

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


const char* MPID_Comm::GetDefaultName() const
{
    if( HANDLE_GET_TYPE(handle) == HANDLE_TYPE_BUILTIN )
    {
        return mpi_comm_names[HANDLE_BUILTIN_INDEX(handle)];
    }

    return mpi_comm_names[CommTraits::BUILTIN];
}


/* This function allocates and calculates an array (*mapping_out) such that
 * (*mapping_out)[i] is the rank in (*mapping_vcr_out) corresponding to local
 * rank i in the given group_ptr.
 *
 * Ownership of the (*mapping_out) array is transferred to the caller who is
 * responsible for freeing it. */
static MPI_RESULT
MPIR_Comm_create_calculate_mapping(
    _In_ MPID_Comm   *comm_ptr,
    _In_ MPID_Group  *group_ptr,
    _Out_cap_(group_ptr->size) int mapping[],
    _Outptr_ MPID_VCR   **mapping_vcr_out
)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int subsetOfWorld = 0;
    int i, j;
    int vcr_size;
    MPID_VCR *vcr;

    MPIU_Assert(mapping != NULL);

    *mapping_vcr_out = NULL;

    if (comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        vcr = comm_ptr->inter.local_comm->vcr;
        vcr_size = comm_ptr->inter.local_size;
    }
    else
    {
        vcr = comm_ptr->vcr;
        vcr_size = comm_ptr->remote_size;
    }

    /* Make sure that the processes for this group are contained within
       the input communicator.  Also identify the mapping from the ranks of
       the old communicator to the new communicator.
       We do this by matching the lpids of the members of the group
       with the lpids of the members of the input communicator.
       It is an error if the group contains a reference to an lpid that
       does not exist in the communicator.

       An important special case is groups (and communicators) that
       are subsets of MPI_COMM_WORLD.  In this case, the lpids are
       exactly the same as the ranks in comm world.
    */

    /* we examine the group's lpids in both the intracomm and non-comm_world cases */
    MPIR_Group_setup_lpid_list( group_ptr );

    /* Optimize for groups contained within MPI_COMM_WORLD. */
    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        int wsize;
        subsetOfWorld = 1;
        wsize         = Mpi.CommWorld->remote_size;
        for (i=0; i<group_ptr->size; i++)
        {
            int g_lpid = group_ptr->lrank_to_lpid[i].lpid;

            /* This mapping is relative to comm world */
            if (g_lpid < wsize)
            {
                mapping[i] = g_lpid;
            }
            else
            {
                subsetOfWorld = 0;
                break;
            }
        }
    }
    if (subsetOfWorld)
    {
        mpi_errno = MPIR_GroupCheckVCRSubset( group_ptr, vcr_size, vcr );
        ON_ERROR_FAIL(mpi_errno);

        /* Override the vcr to be used with the mapping array. */
        vcr = Mpi.CommWorld->vcr;
        vcr_size = Mpi.CommWorld->remote_size;
    }
    else
    {
        for (i=0; i<group_ptr->size; i++)
        {
            /* mapping[i] is the rank in the communicator of the process
               that is the ith element of the group */
            /* FIXME : BUBBLE SORT */
            mapping[i] = -1;
            for (j=0; j<vcr_size; j++)
            {
                int comm_lpid;
                comm_lpid = MPID_VCR_Get_lpid( vcr[j] );
                if (comm_lpid == group_ptr->lrank_to_lpid[i].lpid)
                {
                    mapping[i] = j;
                    break;
                }
            }
            if(mapping[i] == -1)
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_GROUP, "**groupnotincomm %d", i );
                goto fn_fail;
            }
        }
    }

    MPIU_Assert(vcr != NULL);
    *mapping_vcr_out = vcr;

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


/* This function creates a new VCRT and assigns it to out_vcrt, creates a new
 * vcr and assigns it to out_vcr, and then populates it from the mapping_vcr
 * array according to the rank mapping table provided.
 *
 * mapping[i] is the index in the old vcr of index i in the new vcr */
static MPI_RESULT
MPIR_Comm_create_create_and_map_vcrt(
    _In_ int         nproc,
    _In_count_(nproc) const int mapping[],
    _In_ MPID_VCR   *mapping_vcr,
    _Outptr_ MPID_VCR  **out_vcr
    )
{
    int i;
    MPID_VCR *vcr = NULL;

    MPID_VCRT* vcrt = MPID_VCRT_Create( nproc );
    if( vcrt == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    *out_vcr = MPID_VCRT_Get_ptr( vcrt );
    vcr = *out_vcr;
    for (i=0; i<nproc; i++)
    {
        vcr[i] = MPID_VCR_Dup( mapping_vcr[mapping[i]] );
    }

    return MPI_SUCCESS;
}


MPI_RESULT
MPIR_Comm_create_intra(
    _In_ MPID_Comm *comm_ptr,
    _In_ MPID_Group *group_ptr,
    _In_reads_opt_(group_ptr->size) int mapping[],
    _In_opt_ MPID_VCR* mapping_vcr,
    _When_(group_ptr->rank == MPI_UNDEFINED, _Outptr_result_maybenull_)
    _When_(group_ptr->rank != MPI_UNDEFINED, _Outptr_) MPID_Comm** outcomm_ptr
    )
{
    MPI_CONTEXT_ID new_context_id = GUID_NULL;
    MPID_Comm* newcomm_ptr = NULL;

    MPIU_Assert(comm_ptr->comm_kind != MPID_INTERCOMM);

    /* Create a new communicator from the specified group members */

    /* Creating the context id is collective over the *input* communicator,
       so it must be created before we decide if this process is a
       member of the group */

    MPI_RESULT mpi_errno = MPIR_Get_contextid( comm_ptr, &new_context_id );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    MPIU_Assert(new_context_id != GUID_NULL);

    if(group_ptr->rank == MPI_UNDEFINED)
    {
        /* This process is not in the group */
        *outcomm_ptr = nullptr;
        return MPI_SUCCESS;
    }

    StackGuardArray<int> local_mapping;
    if( mapping == nullptr )
    {
        local_mapping = new int[group_ptr->size];
        if( local_mapping == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        mapping = local_mapping;

        //
        // Map ranks such that for any given rank i in the new communicator,
        // mapping[i] = index into mapping_vcr of the corresponding process.
        //
        mpi_errno = MPIR_Comm_create_calculate_mapping(comm_ptr, group_ptr, mapping, &mapping_vcr);
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }
    }

    MPIU_Assert( mapping != nullptr );
    MPIU_Assert( mapping_vcr != nullptr );

    mpi_errno = MPIR_Comm_create( &newcomm_ptr, MPID_INTRACOMM_FLAT );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    newcomm_ptr->recvcontext_id = new_context_id;
    newcomm_ptr->rank           = group_ptr->rank;
    /* Since the group has been provided, let the new communicator know
        about the group */
    newcomm_ptr->group    = group_ptr;
    MPIR_Group_add_ref( group_ptr );

    newcomm_ptr->context_id = newcomm_ptr->recvcontext_id;
    newcomm_ptr->remote_size = group_ptr->size;

    //
    // Inherit HA collective property from the parent.
    //
    newcomm_ptr->comm_kind = comm_ptr->comm_kind;

    /* Setup the communicator's vc table.  This is for the remote group,
        which is the same as the local group for intracommunicators */
    mpi_errno = MPIR_Comm_create_create_and_map_vcrt(group_ptr->size,
                                                     mapping,
                                                     mapping_vcr,
                                                     &newcomm_ptr->vcr);
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = MPIR_Comm_commit(newcomm_ptr);
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        MPIR_Comm_release(newcomm_ptr, 0/*isDisconnect*/);
    }
    else
    {
        *outcomm_ptr = newcomm_ptr;
    }

    return mpi_errno;
}


/* comm create impl for intercommunicators, assumes that the standard error
 * checking has already taken place in the calling function */
MPI_RESULT
MPIR_Comm_create_inter(
    _In_ MPID_Comm* comm_ptr,
    _In_ MPID_Group* group_ptr,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    )
{
    StackGuardArray<int> mapping;
    StackGuardArray<int> remote_mapping;
    MPID_VCR *mapping_vcr = NULL;

    MPIU_Assert(comm_ptr->comm_kind == MPID_INTERCOMM);

    /* Creating the context id is collective over the *input* communicator,
       so it must be created before we decide if this process is a
       member of the group */
    MPI_CONTEXT_ID new_context_id;
    MPI_RESULT mpi_errno = MPIR_Get_contextid( comm_ptr->inter.local_comm, &new_context_id);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    MPIU_Assert(new_context_id != GUID_NULL);
    MPIU_Assert(new_context_id != comm_ptr->recvcontext_id);

    //
    // Map the ranks between the old and new communicators if we are either:
    // - a member of the group
    // - responsible for exchanging group information between
    //   the two process groups (rank 0).
    //
    // TODO: It might be nice to find a better way of selecting the leaders in the case
    // of intercomms, so that rank 0 isn't required to stick around if it isn't part of
    // the group.
    //
    if( group_ptr->rank != MPI_UNDEFINED || comm_ptr->rank == 0 )
    {
        mapping = new int[group_ptr->size];
        if( mapping == nullptr )
        {
            goto fn_cleanup;
        }

        //
        // Map ranks such that for any given rank i in the new communicator,
        // mapping[i] = index into mapping_vcr of the corresponding process.
        //
        mpi_errno = MPIR_Comm_create_calculate_mapping(comm_ptr, group_ptr, mapping, &mapping_vcr);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_cleanup;
        }
    }

    /* There is an additional step.  We must communicate the information
        on the local context id and the group members, given by the ranks
        so that the remote process can construct the appropriate VCRT
        First we exchange group sizes and context ids.  Then the
        ranks in the remote group, from which the remote VCRT can
        be constructed.  We can't use NMPI_Sendrecv since we need to
        use the "collective" context in the original intercommunicator */
    struct _rinfo{
        MPI_CONTEXT_ID contextId;
        int            size;
    } rinfo;

    rinfo.contextId = GUID_NULL;
    rinfo.size = 0;

    if (comm_ptr->rank == 0)
    {
        struct _info{
            MPI_CONTEXT_ID contextId;
            int            size;
        } info;

        info.contextId = new_context_id;
        info.size = group_ptr->size;

        //
        // Exchange the context ID and new group size between the leaders of
        // the local and remote group.
        //
        mpi_errno = MPIC_Sendrecv(
            &info,
            sizeof(info),
            g_hBuiltinTypes.MPI_Byte,
            0,
            MPIR_TAG_COMM_CREATE_INTER,
            &rinfo,
            sizeof(rinfo),
            g_hBuiltinTypes.MPI_Byte,
            0,
            MPIR_TAG_COMM_CREATE_INTER,
            comm_ptr,
            MPI_STATUS_IGNORE
            );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_cleanup;
        }
    }

    //
    // Broadcast remote context ID and remote group size to the local group.
    //
    mpi_errno = MPIR_Bcast_intra(
        &rinfo, sizeof(rinfo), g_hBuiltinTypes.MPI_Byte, 0, comm_ptr->inter.local_comm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_cleanup;
    }

    int remote_size                 = rinfo.size;

    MPIU_Assert(remote_size >= 0);

    //
    // Note that we must continue through to the collectives below even if we
    // have group_ptr->rank == MPI_UNDEFINED.
    //
    if( remote_size == 0 || group_ptr->size == 0 )
    {
        MPIU_Assert( mpi_errno == MPI_SUCCESS );
        goto fn_cleanup;
    }

    remote_mapping = new int[remote_size];
    if( remote_mapping == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_cleanup;
    }

    if( comm_ptr->rank == 0 )
    {
        //
        // Exchange rank mappings between group leaders.
        //
        mpi_errno = MPIC_Sendrecv( mapping, group_ptr->size, g_hBuiltinTypes.MPI_Int,
                                    0, MPIR_TAG_COMM_CREATE_INTER,
                                    remote_mapping, remote_size, g_hBuiltinTypes.MPI_Int,
                                    0, MPIR_TAG_COMM_CREATE_INTER, comm_ptr, MPI_STATUS_IGNORE );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_cleanup;
        }
    }

    //
    // Broadcast the remote mapping to the local group.
    //
    mpi_errno = MPIR_Bcast_intra( remote_mapping, remote_size, g_hBuiltinTypes.MPI_Int, 0,
                comm_ptr->inter.local_comm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_cleanup;
    }

    //
    // Create a local_comm for the new intercomm.  This is a collective operation
    // over the input communicator.  It was delayed until now to simplify error
    // handling.
    //
    MPID_Comm* local_comm;
    mpi_errno = MPIR_Comm_create_intra(
        comm_ptr->inter.local_comm,
        group_ptr,
        mapping,
        mapping_vcr,
        &local_comm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_cleanup;
    }

    //
    // Now that all information has been exchanged, we can exit if we aren't a member of the group.
    //
    if (local_comm == nullptr)
    {
        MPIU_Assert( group_ptr->rank == MPI_UNDEFINED );
        goto fn_cleanup;
    }

    MPID_Comm* newcomm_ptr;
    mpi_errno = MPIR_Comm_create( &newcomm_ptr, MPID_INTERCOMM );
    if( mpi_errno != MPI_SUCCESS )
    {
        MPIR_Comm_release(local_comm, FALSE);
        goto fn_cleanup;
    }

    newcomm_ptr->recvcontext_id = new_context_id;
    newcomm_ptr->inter.local_comm = local_comm;
    newcomm_ptr->group    = nullptr;

    newcomm_ptr->inter.local_size   = local_comm->remote_size;
    newcomm_ptr->inter.is_low_group = comm_ptr->inter.is_low_group;
    newcomm_ptr->rank           = local_comm->rank;

    newcomm_ptr->context_id     = rinfo.contextId;
    newcomm_ptr->remote_size    = remote_size;
    /* Now, everyone has the remote_mapping, and can apply that to
        the vcr table.*/

    /* Setup the communicator's vc table.  This is for the remote group */
    mpi_errno = MPIR_Comm_create_create_and_map_vcrt(remote_size,
                                                     remote_mapping,
                                                     comm_ptr->vcr,
                                                     &newcomm_ptr->vcr);
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = MPIR_Comm_commit( newcomm_ptr );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        MPIR_Comm_release(newcomm_ptr, /*isDisconnect=*/FALSE);
        *outcomm_ptr = nullptr;
    }
    else
    {
        *outcomm_ptr = newcomm_ptr;
    }

fn_exit:
    return mpi_errno;

fn_cleanup:
    *outcomm_ptr = nullptr;
    goto fn_exit;
}


MPI_RESULT
MPIR_Comm_group(
    _In_ MPID_Comm* comm_ptr,
    _Outptr_ MPID_Group** group_ptr
    )
{
    MPIU_Assert( comm_ptr->comm_kind != MPID_INTERCOMM );

    if (comm_ptr->group == nullptr)
    {
        int n = comm_ptr->remote_size;
        MPID_Group* group;
        MPI_RESULT mpi_errno = MPIR_Group_create( n, &group );
        if( mpi_errno != MPI_SUCCESS )
        {
            return mpi_errno;
        }

        for (int i=0; i<n; i++)
        {
            int lpid = MPID_VCR_Get_lpid( comm_ptr->vcr[i] );
            group->lrank_to_lpid[i].lrank = i;
            group->lrank_to_lpid[i].lpid  = lpid;
        }
        group->size          = n;
        group->rank          = comm_ptr->rank;
        group->idx_of_first_lpid = -1;

        comm_ptr->group = group;
    }

    *group_ptr = comm_ptr->group;
    return MPI_SUCCESS;
}


typedef struct splittype_t
{
    int color, key;
} splittype_t;

//
// Sort the entries in keytable into increasing order by key.  A stable
// sort should be used in case the key values are not unique.
//
static void MPIU_Sort_inttable(splittype_t *keytable, int size)
{
    if (size <= 1)
    {
        return;
    }

    int leftStart, leftEnd, rightStart, rightEnd, current, i, j, branch;
    int startIndex, midIndex, endIndex, segmentIndex;
    splittype_t *tmp_storage;
    StackGuardArray<int> naturalIndex(new int[(size + 1) / 2]);
    if (naturalIndex == nullptr)
    {
        return;
    }
    //
    // Find the index points of natural segments
    // either all ascending or all descending
    // Equal is considered within ascending
    //
    j = segmentIndex = 0;
    do
    {
        naturalIndex[segmentIndex] = j;
        if (++j == size)
        {
            //
            // Last object in the array did not match the previous
            // segment; it is the only possible single object segment
            //
            segmentIndex++;
            break;
        }
        if (keytable[j-1].key <= keytable[j].key)
        {
            //
            // Ascending
            //
            do
            {
                j++;
                if (j == size)
                {
                    break;
                }
            } while (keytable[j-1].key <= keytable[j].key);
        }
        else
        {
            //
            // Descending
            //
            do
            {
                j++;
                if (j == size)
                {
                    break;
                }
            } while (keytable[j-1].key > keytable[j].key);
            //
            // Mirror swap to make it ascending
            // Because equal is included in the ascending segments,
            // mirrorswap is still a stable sort
            //
            current = naturalIndex[segmentIndex];
            i = j - 1;
            do
            {
                splittype_t mirrorSwap;

                mirrorSwap = keytable[i];
                keytable[i--] = keytable[current];
                keytable[current++] = mirrorSwap;
            } while (current < i);
        }
        segmentIndex++;
    } while (j < size);  // Finding segment indexes

    if (segmentIndex == 1)
    {
        //
        // Either it was already sorted, or was completely
        // descending, now ascending due to the mirrorswap
        //
        return;
    }

    tmp_storage = (splittype_t*)MPIU_Malloc((size / 2) * sizeof(splittype_t));
    //
    // All segments are now ascending
    // Merge segments from the left, binary tree doubling indexes in each loop
    //
    for (branch = 1; branch < segmentIndex; branch <<= 1)
    {
        startIndex = 0;
        midIndex = branch;
        do
        {
            endIndex = midIndex + branch;

            if (endIndex < segmentIndex)
            {
                rightEnd = naturalIndex[endIndex];
            }
            else
            {
                rightEnd = size;
            }
            --rightEnd;
            rightStart = naturalIndex[midIndex];
            leftEnd = rightStart - 1;
            leftStart = naturalIndex[startIndex];
            //
            // Leave alone all objects on the right that are already
            // higher than all objects in the left
            //
            while (keytable[leftEnd].key <= keytable[rightEnd].key)
            {
                rightEnd--;
                if (rightEnd < rightStart)
                {
                    break;
                }
            }
            //
            // If not all objects are in place, merge the remaining objects
            //
            if (rightEnd >= rightStart)
            {
                //
                // Leave alone all objects on the left that are already
                // lower or equal to all objects in the right
                //
                while (keytable[leftStart].key <= keytable[rightStart].key)
                {
                    //
                    // No need to check if it reaches the end of the left
                    // segment as it is already known that there is
                    // overlap between the left & right segments
                    //
                    leftStart++;
                }
                //
                // Determine if left or right is the smaller segment and
                // put the smaller one into temporary storage
                //
                if ((rightEnd - rightStart) <= (leftEnd - leftStart))
                {
                    //
                    // Move remaining objects on the right to temporary storage,
                    // making space for those being merged from the left
                    //
                    i = rightEnd - rightStart;
                    current = j = rightEnd;
                    do
                    {
                        tmp_storage[i--] = keytable[j--];
                    } while (i >= 0);
                    //
                    // Merge the two lists
                    // The left segment will finish first because all objects already
                    // in place have been accounted for.
                    //
                    i = rightEnd - rightStart;
                    keytable[current--] = keytable[leftEnd--];
                    while (leftEnd >= leftStart)
                    {
                        //
                        // Stable sort, so right side if they are equal
                        //
                        if (keytable[leftEnd].key <= tmp_storage[i].key)
                        {
                            keytable[current--] = tmp_storage[i--];
                        }
                        else
                        {
                            keytable[current--] = keytable[leftEnd--];
                        }
                    }
                    //
                    // Move remaining objects in temporary storage
                    //
                    do
                    {
                        keytable[current--] = tmp_storage[i--];
                    } while (i >= 0);
                } // Right segment smaller
                else
                {
                    //
                    // Left segment is smaller than the right segment
                    // Move remaining objects on the left to temporary storage,
                    // making space for those being merged
                    //
                    i = 0;
                    j = leftStart;
                    do
                    {
                        tmp_storage[i++] = keytable[j++];
                    } while (j < rightStart);
                    //
                    // Merge remaining segments
                    // Because both ends are known to overlap, right segment
                    // will finish before left
                    //
                    current = 0;
                    keytable[leftStart++] = keytable[j++];
                    while (j <= rightEnd)
                    {
                        //
                        // Stable sort, so left side if they are equal
                        //
                        if (keytable[j].key >= tmp_storage[current].key)
                        {
                            keytable[leftStart++] = tmp_storage[current++];
                        }
                        else
                        {
                            keytable[leftStart++] = keytable[j++];
                        }
                    }
                    //
                    // Right segment has been moved
                    // Copy remaining left segment from temporary storage
                    //
                    do
                    {
                        keytable[leftStart++] = tmp_storage[current++];
                    } while (current < i);
                } // left segment smaller
            } // merge overlapping segments

            startIndex = endIndex;
            midIndex = startIndex + branch;
        } while (midIndex < segmentIndex); // binary tree climb
    }

    MPIU_Free(tmp_storage);
}


MPI_RESULT
MPIR_Comm_split_intra(
    _In_ MPID_Comm* comm_ptr,
    _Pre_satisfies_(color == MPI_UNDEFINED || color >= 0) int color,
    _In_ int key,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    )
{
    MPID_Comm *newcomm_ptr;
    StackGuardArray<splittype_t> table;
    StackGuardArray<splittype_t> keytable;
    int       i, new_size, first_entry = 0, *last_ptr;
    MPI_CONTEXT_ID new_context_id;

    MPIU_Assert( comm_ptr->comm_kind != MPID_INTERCOMM );

    int rank        = comm_ptr->rank;
    int size        = comm_ptr->remote_size;

    /* Step 1: Find out what color and keys all of the processes have */
    table = new splittype_t[size];
    if( table == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }
    table[rank].color = color;
    table[rank].key   = key;

    /* Gather information on the local group of processes */
    MPI_RESULT mpi_errno = MPIR_Allgather_intra( MPI_IN_PLACE,
                                                 2,
                                                 g_hBuiltinTypes.MPI_Int,
                                                 table,
                                                 2,
                                                 g_hBuiltinTypes.MPI_Int,
                                                 comm_ptr );
    ON_ERROR_FAIL(mpi_errno);

    /* Step 2: How many processes have our same color? */
    new_size = 0;
    if (color != MPI_UNDEFINED)
    {
        /* Also replace the color value with the index of the *next* value
           in this set.  The integer first_entry is the index of the
           first element */
        last_ptr = &first_entry;
        for (i=0; i<size; i++)
        {
            /* Replace color with the index in table of the next item
               of the same color.  We use this to efficiently populate
               the keyval table */
            if (table[i].color == color)
            {
                new_size++;
                *last_ptr = i;
                last_ptr  = &table[i].color;
            }
        }
        _Analysis_assume_( new_size > 0 );
    }

    /* We don't need to set the last value to -1 because we loop through
       the list for only the known size of the group */

    if (color != MPI_UNDEFINED && new_size > 0)
    {
        //
        // We will order the processes by their key values.  To simplify
        // the sort, we extract the table into a smaller array and sort that.
        //
        // We pre-allocate the key table here, so that the implicit goto happens
        // before we allocate the context ID.  This allows us to have rational
        // error recovery.
        //
        keytable = new splittype_t[new_size];
        if( keytable == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
    }
    else
    {
        keytable = nullptr;
    }

    /* Step 3: Create the communicator */
    /* Collectively create a new context id.  The same context id will
       be used by each (disjoint) collections of processes.  The
       processes whose color is MPI_UNDEFINED will return the
       context id to the pool */
    mpi_errno = MPIR_Get_contextid( comm_ptr, &new_context_id );
    ON_ERROR_FAIL(mpi_errno);

    /* Now, create the new communicator structure if necessary */
    if (keytable != nullptr)
    {
        /* Step 4: Order the processes by their key values.  Sort the
           list that is stored in table.  To simplify the sort, we
           extract the table into a smaller array and sort that.
           Also, store in the "color" entry the rank in the input communicator
           of the entry. */
        for (i=0; i<new_size; i++)
        {
            keytable[i].key   = table[first_entry].key;
            keytable[i].color = first_entry;
            first_entry       = table[first_entry].color;
        }

        /* sort key table.  The "color" entry is the rank of the corresponding
           process in the input communicator */
        MPIU_Sort_inttable( keytable, new_size );

        mpi_errno = MPIR_Comm_create( &newcomm_ptr, MPID_INTRACOMM_FLAT );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail2;
        }

        newcomm_ptr->context_id     = new_context_id;
        newcomm_ptr->remote_size    = new_size;
        newcomm_ptr->recvcontext_id = new_context_id;
        MPID_VCRT* vcrt = MPID_VCRT_Create( new_size );
        if( vcrt == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail3;
        }
        newcomm_ptr->vcr = MPID_VCRT_Get_ptr( vcrt );
        for (i=0; i<new_size; i++)
        {
            newcomm_ptr->vcr[i] = MPID_VCR_Dup( comm_ptr->vcr[keytable[i].color] );

            if (keytable[i].color == comm_ptr->rank)
            {
                newcomm_ptr->rank = i;
            }
            //
            // We inherit HA collective property from the parent.
            //
            newcomm_ptr->comm_kind = comm_ptr->comm_kind;
        }

        /* Inherit the error handler (if any) */
        newcomm_ptr->errhandler = comm_ptr->errhandler;
        if (comm_ptr->errhandler)
        {
            MPIR_Errhandler_add_ref( comm_ptr->errhandler );
        }

        mpi_errno = MPIR_Comm_commit(newcomm_ptr);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail3;
        }

        *outcomm_ptr = newcomm_ptr;
    }
    else
    {
        /* color was MPI_UNDEFINED.  Free the context id */
        *outcomm_ptr = NULL;
    }

fn_exit:
    return mpi_errno;

fn_fail3:
    MPIR_Comm_release(newcomm_ptr, 0);
fn_fail2:
fn_fail:
    goto fn_exit;
}


MPI_RESULT
MPIR_Comm_split_inter(
    _In_ MPID_Comm* comm_ptr,
    _Pre_satisfies_(color == MPI_UNDEFINED || color >= 0) int color,
    _In_ int key,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    )
{
    MPI_RESULT mpi_errno;
    MPID_Comm *newcomm_ptr;
    MPID_Comm *local_comm_ptr;
    StackGuardArray<splittype_t> remotetable;
    StackGuardArray<splittype_t> remotekeytable;
    int       i, new_size, new_remote_size = 0, first_remote_entry = 0, *last_ptr;
    MPI_CONTEXT_ID new_context_id, remote_context_id=GUID_NULL;

    MPIU_Assert( comm_ptr->comm_kind == MPID_INTERCOMM );

    int remote_size = comm_ptr->remote_size;

    /* Step 1: Find out what color and keys of the remote processes */
    splittype_t mypair;
    /* For the remote group, the situation is more complicated.
        We need to find the size of our "partner" group in the
        remote comm.  The easiest way (in terms of code) is for
        every process to essentially repeat the operation for the
        local group - perform an (intercommunicator) all gather
        of the color and rank information for the remote group.
    */
    remotetable = new splittype_t[remote_size];
    if( remotetable == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    //
    // We use a local splittype_t because the remotetable may not be large
    // enough to store all entries for the local group (in case the local group
    // is larger than the remote group).
    //
    mypair.color = color;
    mypair.key   = key;

    /* This is an intercommunicator allgather */
    mpi_errno = MPIR_Allgather_inter( &mypair, 2, g_hBuiltinTypes.MPI_Int, remotetable, 2, g_hBuiltinTypes.MPI_Int, comm_ptr );
    ON_ERROR_FAIL(mpi_errno);

    /* Step 2: How many processes have our same color? */
    /* Each process can now match its color with the entries in the table */
    new_remote_size = 0;
    last_ptr = &first_remote_entry;
    for (i=0; i<remote_size; i++)
    {
        /* Replace color with the index in table of the next item
            of the same color.  We use this to efficiently populate
            the keyval table */
        if (remotetable[i].color == color)
        {
            new_remote_size++;
            *last_ptr = i;
            last_ptr  = &remotetable[i].color;
        }
    }
    /* Note that we might find that there are no processes in the remote
        group with the same color.  In that case, COMM_SPLIT will
        return a null communicator */

    //
    // Collectively create a new context id.  The same context id will
    // be used by each (disjoint) collections of processes.  The
    // processes whose color is MPI_UNDEFINED will return the
    // context id to the pool
    //
    mpi_errno = MPIR_Get_contextid( comm_ptr->inter.local_comm, &new_context_id );
    ON_ERROR_FAIL(mpi_errno);

    //
    // Exchange the context ID with the remote group (root of each process group only)
    //
    if (comm_ptr->rank == 0)
    {
        mpi_errno = MPIC_Sendrecv( &new_context_id, sizeof(new_context_id), g_hBuiltinTypes.MPI_Byte, 0, MPIR_TAG_COMM_SPLIT_INTER,
                                    &remote_context_id, sizeof(remote_context_id), g_hBuiltinTypes.MPI_Byte,
                                    0, MPIR_TAG_COMM_SPLIT_INTER, comm_ptr, MPI_STATUS_IGNORE );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail2;
        }
    }

    //
    // Broadcast the remote context ID to all members of the local group.
    //
    mpi_errno = MPIR_Bcast_intra( &remote_context_id, sizeof(remote_context_id), g_hBuiltinTypes.MPI_Byte, 0, comm_ptr->inter.local_comm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail2;
    }

    /* Step 3: Create the communicator */

    //
    // Split the local subcommunicator.  All processes in the local group must
    // do this, regardless of whether the remote group is empty.
    //
    mpi_errno = MPIR_Comm_split_intra( comm_ptr->inter.local_comm, color, key, &local_comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail2;
    }

    MPIU_Assert( color == MPI_UNDEFINED || local_comm_ptr != nullptr );
    if( color == MPI_UNDEFINED )
    {
        //
        // The remote group will have calculated new_remote_size == 0.
        //
        MPIU_Assert( local_comm_ptr == nullptr );
        goto fn_fail2;
    }
    else if( new_remote_size == 0 )
    {
        //
        // There are no matching processes with the same color in the remote group.
        // MPI_COMM_SPLIT is required to return MPI_COMM_NULL instead of an intercomm
        // with an empty remote group. */
        //
        // We created a local communicator object, and need to free it.
        //
        goto fn_fail3;
    }

    new_size = local_comm_ptr->remote_size;

    /* Step 4: Order the processes by their key values.  Sort the
        list that is stored in table.  To simplify the sort, we
        extract the table into a smaller array and sort that.
        Also, store in the "color" entry the rank in the input communicator
        of the entry. */
    remotekeytable = new splittype_t[new_remote_size];
    if( remotekeytable == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail3;
    }

    for (i=0; i<new_remote_size; i++)
    {
        remotekeytable[i].key   = remotetable[first_remote_entry].key;
        remotekeytable[i].color = first_remote_entry;
        first_remote_entry      = remotetable[first_remote_entry].color;
    }

    /* sort key table.  The "color" entry is the rank of the
        corresponding process in the input communicator */
    MPIU_Sort_inttable( remotekeytable, new_remote_size );

    mpi_errno = MPIR_Comm_create( &newcomm_ptr, MPID_INTERCOMM );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail3;
    }

    newcomm_ptr->context_id     = new_context_id;
    newcomm_ptr->inter.local_size     = new_size;
    newcomm_ptr->recvcontext_id = remote_context_id;
    newcomm_ptr->remote_size    = new_remote_size;

    newcomm_ptr->inter.local_comm     = local_comm_ptr;
    newcomm_ptr->inter.is_low_group   = comm_ptr->inter.is_low_group;
    newcomm_ptr->rank           = local_comm_ptr->rank;

    MPID_VCRT* vcrt = MPID_VCRT_Create( new_remote_size );
    if( vcrt == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail3;
    }
    newcomm_ptr->vcr = MPID_VCRT_Get_ptr( vcrt );
    for (i=0; i<new_remote_size; i++)
    {
        newcomm_ptr->vcr[i] = MPID_VCR_Dup( comm_ptr->vcr[remotekeytable[i].color] );
    }

    /* Inherit the error handler (if any) */
    newcomm_ptr->errhandler = comm_ptr->errhandler;
    if (comm_ptr->errhandler)
    {
        MPIR_Errhandler_add_ref( comm_ptr->errhandler );
    }

    mpi_errno = MPIR_Comm_commit(newcomm_ptr);
    ON_ERROR_FAIL(mpi_errno);

    *outcomm_ptr = newcomm_ptr;

  fn_exit:
    return mpi_errno;

fn_fail3:
    MPIR_Comm_release(local_comm_ptr, 0);
fn_fail2:
fn_fail:
    *outcomm_ptr = NULL;
    goto fn_exit;
}


//
// Summary:
// Setup the new intercomm
//
// In:
// comm_ptr        : Pointer to the intracomm
// recvContextId   : The context id for receiving messages from the remote side
// pGroupData      : The pointer to the metadata of the remote group
// pConnInfos      : Array of PG infos for the remote side
//
// Out:
// newcomm         : the intercomm to setup
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
MPI_RESULT
MPIR_Intercomm_create(
    _In_  const MPID_Comm*         comm_ptr,
    _In_  const MPI_CONTEXT_ID&    recvContextId,
    _In_  RemoteGroupMetadata      groupData,
    _In_  CONN_INFO_TYPE*          pConnInfos,
    _Outptr_ MPID_Comm**           newcomm
    )
{
    *newcomm = NULL;

    //
    // Setup the remote VCRT for the intercomm.
    //
    MPID_VCRT* vcrt = MPID_VCRT_Create(groupData.remoteCommSize);
    if( vcrt == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    MPID_VCR* vcr = MPID_VCRT_Get_ptr(vcrt);

    //
    // Now we need to create the process groups (pg) from
    // the connection infos
    //
    MPI_RESULT mpi_errno;
    for( unsigned int i = 0; i < groupData.remoteCommSize; i++ )
    {
        //
        // Only create a new PG if the PG does not exist
        //
        MPIDI_PG_t* pg = MPIDI_PG_Find( pConnInfos[i].pg_id );
        if( pg == NULL )
        {
            //
            // Create the PG, this will setup the VC table and all the
            // back pointers from the pg's VCs to the pg itself
            //
            mpi_errno = MPIDI_PG_Create( pConnInfos[i].pg_size,
                                         pConnInfos[i].pg_id,
                                         &pg );
            if( mpi_errno != MPI_SUCCESS )
            {
                MPID_VCRT_Release( vcrt, 0 );
                return mpi_errno;
            }

            //
            // Since this is a "fresh" PG, we will need to allocate
            // space to store the business cards
            //
            pg->pBusinessCards = static_cast<char**>(
                MPIU_Malloc( pg->size * sizeof(char*) ) );
            if( pg->pBusinessCards == NULL )
            {
                MPID_VCRT_Release( vcrt, 0 );
                return MPIU_ERR_NOMEM();
            }

            //
            // Initialize all the connection strings (business cards)
            //
            for( int j = 0; j < pg->size; j++ )
            {
                pg->pBusinessCards[j] = NULL;
            }
        }

        if( pg->pBusinessCards != NULL )
        {
            //
            // We only setup connection strings for PG that is created
            // during Connect/Accept.
            //
            int pg_rank = pConnInfos[i].pg_rank;
            if( pg->pBusinessCards[pg_rank] == NULL )
            {
                //
                // Only copy the connection string if we did not
                // have it already
                //
                size_t bc_len = MPIU_Strlen( pConnInfos[i].business_card,
                                             _countof(pConnInfos[i].business_card) );

                pg->pBusinessCards[pg_rank] = static_cast<char*>(
                    MPIU_Malloc( (bc_len + 1) * sizeof(char) ) );
                if( pg->pBusinessCards[pg_rank] == NULL )
                {
                    MPID_VCRT_Release( vcrt, 0 );
                    return MPIU_ERR_NOMEM();
                }

                HRESULT hr = StringCchCopyA(
                    pg->pBusinessCards[pg_rank],
                    bc_len + 1,
                    pConnInfos[i].business_card );
                if( FAILED(hr) )
                {
                    MPIU_Free( pg->pBusinessCards[pg_rank] );
                    pg->pBusinessCards[pg_rank] = nullptr;
                    MPID_VCRT_Release( vcrt, 0 );
                    return MPIU_ERR_NOMEM();
                }
            }
        }

        vcr[i] = MPID_VCR_Dup( &pg->vct[pConnInfos[i].pg_rank] );
    }

    MPID_Comm* newcomm_ptr;
    mpi_errno = MPIR_Comm_create( &newcomm_ptr, MPID_INTERCOMM );
    if( mpi_errno != MPI_SUCCESS )
    {
        MPID_VCRT_Release( vcrt, 0 );
        return mpi_errno;
    }

    newcomm_ptr->vcr              = vcr;
    newcomm_ptr->context_id       = groupData.remoteContextId;
    newcomm_ptr->recvcontext_id   = recvContextId;
    newcomm_ptr->remote_size      = vcrt->size;
    newcomm_ptr->inter.local_size = comm_ptr->remote_size;
    newcomm_ptr->rank             = comm_ptr->rank;

    mpi_errno = MPIR_Comm_copy( comm_ptr, comm_ptr->remote_size,
                                &newcomm_ptr->inter.local_comm );
    if( mpi_errno != MPI_SUCCESS )
    {
        MPIR_Comm_release( newcomm_ptr, 0 );
        return mpi_errno;
    }
    newcomm_ptr->inter.is_low_group   = groupData.is_low_group;

    /* Inherit the error handler (if any) */
    newcomm_ptr->errhandler = comm_ptr->errhandler;
    if (comm_ptr->errhandler)
    {
        MPIR_Errhandler_add_ref( comm_ptr->errhandler );
    }

    mpi_errno = MPIR_Comm_commit( newcomm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        MPIR_Comm_release( newcomm_ptr, 0 );
    }
    else
    {
        *newcomm = newcomm_ptr;
    }

    return mpi_errno;
}


//
// Summary:
// The routine for the root of MPI_Intercomm_create
//
// In:
// comm_ptr        : The intracomm of the processes participate in this call
// peer_comm_ptr   : The intracomm of that the two leaders share
// remote_leader   : The rank of the remote leader in peer_comm
// tag             : The tag being used for pt2pt operation within this routine
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
MPI_RESULT
MPIR_Intercomm_create_root(
    _In_ const MPID_Comm* comm_ptr,
    _In_ const MPID_Comm* peer_comm_ptr,
    _In_ int              remote_leader,
    _In_ int              tag,
    _Outptr_ MPID_Comm**  newcomm
    )
{
    MPI_RESULT mpi_errno             = MPI_SUCCESS;
    CONN_INFO_TYPE* pRemoteConnInfos = NULL;
    RemoteGroupMetadata groupData    = {GUID_NULL, 0, 0};

    //
    // Gather process group information from the local group
    //
    CONN_INFO_TYPE* pLocalConnInfos = ConnInfoAlloc( comm_ptr->remote_size );
    if( pLocalConnInfos == NULL )
    {
        mpi_errno = MPIU_ERR_NOMEM();
    }

    //
    // If the previous allocation fails, the root will use this function
    // to notify other non-root of this operation of the alloc error
    // so that we can fail gracefully.
    //
    MPI_RESULT mpi_errno2 = ReceiveLocalPGInfo( comm_ptr, pLocalConnInfos, mpi_errno );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( mpi_errno2 != MPI_SUCCESS )
    {
        mpi_errno = mpi_errno2;
        goto NotifyNonRootAndFail;
    }

    //
    // We do an exchange of ConnInfo's and verify whether the two groups are
    // disjointed. The exchange happens in 2 steps
    // 1) Exchange the size
    // 2) Allocate memory based on the received size, and exchange the ConnInfos
    //
    int local_size = comm_ptr->remote_size;
    int remote_size;

    mpi_errno = MPIC_Sendrecv( &local_size, 1, g_hBuiltinTypes.MPI_Int,
                               remote_leader, tag,
                               &remote_size, 1, g_hBuiltinTypes.MPI_Int,
                               remote_leader, tag, peer_comm_ptr,
                               MPI_STATUS_IGNORE );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto NotifyNonRootAndFail;
    }
    groupData.remoteCommSize = remote_size;

    pRemoteConnInfos = ConnInfoAlloc( remote_size );
    if( pRemoteConnInfos == NULL )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto NotifyNonRootAndFail;
    }

    mpi_errno = MPIC_Sendrecv( pLocalConnInfos,
                               sizeof(CONN_INFO_TYPE) * local_size,
                               g_hBuiltinTypes.MPI_Byte,
                               remote_leader,
                               tag,
                               pRemoteConnInfos,
                               sizeof(CONN_INFO_TYPE) * remote_size,
                               g_hBuiltinTypes.MPI_Byte,
                               remote_leader,
                               tag,
                               peer_comm_ptr,
                               MPI_STATUS_IGNORE );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto NotifyNonRootAndFail;
    }

    /*
        * Error checking for this routine requires care.  Because this
        * routine is collective over two different sets of processes,
        * it is relatively easy for the user to try to create an
        * intercommunicator from two overlapping groups of processes.
        * This is made more likely by inconsistencies in the MPI-1
        * specification (clarified in MPI-2) that seemed to allow
        * the groups to overlap.  Because of that, we first check that the
        * groups are in fact disjoint before performing any collective
        * operations.
        */

    //
    // This is an expensive check
    //
    for( int i = 0; i < local_size; i++ )
    {
        for( int j = 0; j < remote_size; j++ )
        {
            if( InlineIsEqualGUID( pLocalConnInfos[i].pg_id,
                                    pRemoteConnInfos[j].pg_id ) &&
                pLocalConnInfos[i].pg_rank == pRemoteConnInfos[j].pg_rank )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_COMM,
                    "**dupprocesses %d",
                    pLocalConnInfos[i].pg_rank );
                goto NotifyNonRootAndFail;

            }
        }
    }

    //
    // For intercomm, each side generates its own receiving context id
    // and sends the generated context id to the other side. This
    // process will expect to receive all future messages on this
    // context id
    //
    MPI_CONTEXT_ID localContextId;

    //
    // The two sides need to argee on an arbitrary is_low_group
    //
    RPC_STATUS status;
    int compareResult = UuidCompare( &pLocalConnInfos[0].pg_id,
                                     &pRemoteConnInfos[0].pg_id,
                                     &status );
    if( compareResult == -1 )
    {
        groupData.is_low_group = 0;
    }
    else if( compareResult == 1 )
    {
        groupData.is_low_group = 1;
    }
    else
    {
        //
        // The case in which first process of each group has the same
        // pg_id
        //
        MPIU_Assert( pLocalConnInfos[0].pg_rank != pRemoteConnInfos[0].pg_rank );
        groupData.is_low_group =
            pLocalConnInfos[0].pg_rank < pRemoteConnInfos[0].pg_rank ? 0 : 1;
    }

    mpi_errno = MPIR_Get_contextid( comm_ptr,
                                    &localContextId,
                                    false /* isAbort */);
    if( mpi_errno != MPI_SUCCESS )
    {
        //
        // In other failure cases we would notify the non-root processes
        // by calling MPIR_Get_contextid with abort flag turned on. But if
        // MPIR_Get_contextid is failing here, there's no point doing another
        // MPIR_Get_contextid with abort flag because it will likely fail as well.
        //
        goto CleanUp;
    }

    mpi_errno = MPIC_Sendrecv( &localContextId, sizeof(localContextId), g_hBuiltinTypes.MPI_Byte,
                               remote_leader, tag,
                               &groupData.remoteContextId, sizeof(groupData.remoteContextId), g_hBuiltinTypes.MPI_Byte,
                               remote_leader, tag, peer_comm_ptr, MPI_STATUS_IGNORE );
    if( mpi_errno != MPI_SUCCESS )
    {
        //
        // When the non-root receives 0 as the context id, it will
        // know that the root is indicating an error condition.
        //
        groupData.remoteContextId = GUID_NULL;
    }

    //
    // Distribute the remote PG infos to the non-root processes
    //
    mpi_errno2 = SendRemoteConnInfo( comm_ptr,
                                     &groupData,
                                     pRemoteConnInfos );
    if( mpi_errno == MPI_SUCCESS )
    {
        mpi_errno = mpi_errno2;
    }
    if( mpi_errno == MPI_SUCCESS )
    {
        //
        // All processes setup the new intercomm using
        // the connection infos obtained from the remote side
        //
        mpi_errno = MPIR_Intercomm_create( comm_ptr,
                                           localContextId,
                                           groupData,
                                           pRemoteConnInfos,
                                           newcomm );
    }

CleanUp:
    if( pLocalConnInfos != NULL )
    {
        ConnInfoFree( pLocalConnInfos );
    }

    if( pRemoteConnInfos != NULL )
    {
        ConnInfoFree( pRemoteConnInfos );
    }

    return mpi_errno;

NotifyNonRootAndFail:
    MPIR_Get_contextid( comm_ptr,
                        &localContextId,
                        true /* isAbort */);
    goto CleanUp;
}


//
// Summary:
// The routine for the non root processes of MPI_Intercomm_create
//
// In:
// comm_ptr        : The intracomm of the processes participate in this call
// root            : The rank of the root of this operation
//
// Out:
// newcomm         : The output intercomm
//
// Return:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks:
//
MPI_RESULT
MPIR_Intercomm_create_non_root(
    _In_  const MPID_Comm* comm_ptr,
    _In_  int              root,
    _Outptr_ MPID_Comm**   newcomm
    )
{
    //
    // Send process group information to the root
    //
    MPI_RESULT mpi_errno = SendLocalPGInfo( comm_ptr, root );

    //
    // For intercomm, each side generates its own receiving context id
    // and sends the generated context id to the other side. This
    // process will expect to receive all future messages on this
    // context id
    //
    MPI_CONTEXT_ID localContextId;
    MPI_RESULT mpi_errno2 = MPIR_Get_contextid( comm_ptr,
                                                &localContextId,
                                                mpi_errno != MPI_SUCCESS );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( mpi_errno2 != MPI_SUCCESS )
    {
        return mpi_errno2;
    }

    //
    // Information about the remote group that will be broadcast
    // to all processes from the local root
    //
    RemoteGroupMetadata groupData;
    CONN_INFO_TYPE* pRemoteConnInfos = NULL;

    //
    // This call will allocate memory to hold the remote connection
    // infos for non-root processes, which will need to be freed
    // after setting up the intercomm
    //
    mpi_errno = ReceiveRemoteConnInfo( comm_ptr,
                                       root,
                                       &groupData,
                                       &pRemoteConnInfos );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    //
    // All processes (root and non-root) setup the new intercomm using
    // the connection infos obtained from the remote side
    //
    mpi_errno = MPIR_Intercomm_create( comm_ptr,
                                       localContextId,
                                       groupData,
                                       pRemoteConnInfos,
                                       newcomm );

    ConnInfoFree( pRemoteConnInfos );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    return MPI_SUCCESS;
}


int MPID_Get_node_id(const MPID_Comm* comm, int rank)
{
    return comm->vcr[rank]->node_id;
}


_Success_( return == TRUE )
BOOL
MPID_Comm_get_attr(
    _In_ const MPID_Comm* /*pComm*/,
    _In_ int hKey,
    _Outptr_result_maybenull_ void** pValue
    )
{
    static PreDefined_attrs attr_copy;    /* Used to provide a copy of the
                                             predefined attributes */

    int attr_idx = hKey & 0x0000000f;

    /* FIXME : We could initialize some of these here; only tag_ub is
    used in the error checking. */
    /*
    * The C versions of the attributes return the address of a
    * *COPY* of the value (to prevent the user from changing it)
    * and the Fortran versions provide the actual value (as an Fint)
    */
    attr_copy = Mpi.Attributes;
    switch (attr_idx)
    {
    case 1: /* TAG_UB */
    case 2: /* Fortran TAG_UB */
        *pValue = &attr_copy.tag_ub;
        break;

    case 3: /* HOST */
    case 4: /* Fortran HOST */
        *pValue = &attr_copy.host;
        break;

    case 5: /* IO */
    case 6: /* Fortran IO */
        *pValue = &attr_copy.io;
        break;

    case 7: /* WTIME */
    case 8: /* Fortran WTIME */
        *pValue = &attr_copy.wtime_is_global;
        break;

    case 9: /* UNIVERSE_SIZE */
    case 10: /* Fortran UNIVERSE_SIZE */
        /* This is a special case.  If universe is not set, then we
        attempt to get it from the device.  If the device doesn't
        supply a value, then we set the flag accordingly */
        if (attr_copy.universe == MPIR_UNIVERSE_SIZE_NOT_SET )
        {
            attr_copy.universe = MPID_Get_universe_size();
        }

        if (attr_copy.universe < 0)
        {
            return FALSE;
        }

        *pValue = &attr_copy.universe;
        break;

    case 11: /* LASTUSEDCODE */
    case 12: /* Fortran LASTUSEDCODE */
        *pValue = &attr_copy.lastusedcode;
        break;

    case 13: /* APPNUM */
    case 14: /* Fortran APPNUM */
        /* This is another special case.  If appnum is negative,
        we take that as indicating no value of APPNUM, and set
        the flag accordingly */
        if (attr_copy.appnum < 0)
        {
            return FALSE;
        }

        *pValue = &attr_copy.appnum;
        break;

    default:
        return FALSE;
    }

    if( (attr_idx & 1) == 0 )
    {
        //
        // Indexes for attributes are 1 more in Fortran than the C index.
        // All Fortran attribute indexes are even.
        //
        // Convert the return value to be "by value" for Fortran callers.
        //
        MPI_Aint intValue = *reinterpret_cast<int*>(*pValue);
        *pValue = reinterpret_cast<void*>(intValue);
    }
    return TRUE;
}
