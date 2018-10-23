// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "mpidimpl.h" /* FIXME this is including a ch3 include file!
                         Implement these functions at the device level. */

#include <limits.h>
#include <errno.h>

/* MPIU_Find_node_local_and_leaders -- from the list of processes in comm,
   builds a list of local processes, i.e., processes on this same
   node, and a list of external processes, i.e., one process from each
   node.

   Note that this will not work correctly for spawned or attached
   processes.

   external processes: For a communicator, there is one external
                       process per node.  You can think of this as the
                       leader process for that node.

   OUT:
     local_size_p      - number of processes on this node
     local_ranks_p     - (*local_ranks_p)[i] = the rank in comm
                         of the process with local rank i.
                         This is of size (*local_size_p)
     leaders_size_p      - number of external processes
     leader_ranks_p      - (*leader_ranks_p)[i] = the rank in comm
                         of the process with external rank i.
                         This is of size (*leaders_size_p)
     mappings          - mapping in which to fill in the node local and leader
                         for each process in the input communicator.
*/
int MPIU_Find_node_local_and_leaders(
    _In_ const MPID_Comm *comm,
    _Out_ int *local_size_p,
    _Out_writes_to_(comm->remote_size, *local_size_p) int local_ranks[],
    _Out_ int *leaders_size_p,
    _Out_writes_to_(comm->remote_size, *leaders_size_p) int leader_ranks[],
    _Out_writes_(comm->remote_size) HA_rank_mapping mappings[]
    )
{
    int mpi_errno = MPI_SUCCESS;
    int leaders_size;
    int local_size;
    int i;
    int max_node_id;
    int node_id;
    int my_node_id;

    /* Scan through the list of processes in comm and add one
       process from each node to the list of "external" processes.  We
       add the first process we find from each node.  node_leaders[] is an
       array where we keep track of whether we have already added that
       node to the list. */

    MPIU_Assert(comm->comm_kind == MPID_INTRACOMM_PARENT);

    max_node_id = MPID_Get_max_node_id();

    StackGuardArray<int> node_leaders = new int[max_node_id + 1];
    if( node_leaders == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    //
    // node_leaders maps node_id to rank in leader_ranks
    //
    RtlFillMemory( node_leaders, sizeof(int) * (max_node_id + 1), -1 );

    my_node_id = MPID_Get_node_id(comm, comm->rank);
    MPIU_Assert(my_node_id >= 0);
    MPIU_Assert(my_node_id <= max_node_id);

    //
    // Build the array of leader processes.  We need to do this before building the
    // mappings array so that we can fill in the leader inforation properly.
    //
    bool isLeader = false;
    leaders_size = 0;
    for (i = 0; i < comm->remote_size; ++i)
    {
        node_id = MPID_Get_node_id(comm, i);

        /* The upper level can catch this non-fatal error and should be
           able to recover gracefully. */
        if(node_id < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**fail");
            goto fn_fail;
        }

        MPIU_Assert(node_id <= max_node_id);

        /* build list of external processes
           since we're traversing ranks from 0, this assignment ensures the comm root
           to also become the node master of its node
        */
        if (node_leaders[node_id] == -1)
        {
            //
            // This is the first rank on this node.  Store the rank of the
            // node leader for this node.
            //
            node_leaders[node_id] = leaders_size;

            //
            // Store the mapping from rank in leader_ranks to rank in comm.
            //
            leader_ranks[leaders_size] = i;
            ++leaders_size;

            //
            // Flag if we're a leader.
            //
            if( i == comm->rank )
            {
                MPIU_Assert( node_id == my_node_id );
                isLeader = true;
            }
        }
    }

    //
    // Now that we have the leader information, build the mappings array.
    //
    local_size = 0;
    for (i = 0; i < comm->remote_size; ++i)
    {
        node_id = MPID_Get_node_id(comm, i );

        MPIU_Assert( node_id >= 0 );

        if (node_id == my_node_id)
        {
            if( isLeader == true && i == comm->rank )
            {
                //
                // The leader needs to store the leader ranks.
                //
                mappings[i].rank = node_leaders[node_id];
                mappings[i].isLocal = 0;
            }
            else
            {
                //
                // Store the mapping from rank in comm to rank in local_ranks.
                //
                mappings[i].rank = local_size;
                mappings[i].isLocal = 1;
            }

            //
            // Store the mapping from rank in local_ranks to rank in comm.
            //
            local_ranks[local_size] = i;
            ++local_size;
        }
        else if (isLeader == true)
        {
            //
            // Leaders store the rank of their peers for remote processes.
            //
            mappings[i].rank = node_leaders[node_id];
            mappings[i].isLocal = 0;
        }
        else
        {
            //
            // Non-leaders always look to rank 0 for data from remote processes.
            //
            mappings[i].rank = 0;
            mappings[i].isLocal = 1;
        }
    }

    *local_size_p = local_size;

    if( isLeader == true )
    {
        MPIU_Assert( local_ranks[0] == comm->rank );
        MPIU_Assert( mappings[comm->rank].rank == static_cast<ULONG>(node_leaders[my_node_id]) );
        *leaders_size_p = leaders_size;
    }
    else
    {
        *leaders_size_p = 0;
    }

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}
