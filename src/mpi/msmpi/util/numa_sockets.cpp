// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "mpidimpl.h"
#include "hwtree.h"
#include "pmi.h"
#include <limits.h>
#include <errno.h>
#include <windows.h>


static HWAFFINITY *numa_socket_masks = NULL;
static ULONG numa_socket_count = 0;
static bool numa_socket_info_init_done = false;

static int PopulateNumaSocketInfo()
{
    if(numa_socket_masks != NULL)
    {
        return numa_socket_count;
    }

    if(numa_socket_info_init_done == true)
    {
        return numa_socket_count;
    }

    ULONG highest_socket_id;
    if (GetNumaHighestNodeNumber(&highest_socket_id) == FALSE)
    {
        goto fn_fail;
    }

    numa_socket_count = highest_socket_id + 1;

    numa_socket_masks = (HWAFFINITY*) malloc(numa_socket_count * sizeof(HWAFFINITY));
    if(numa_socket_masks == NULL)
    {
        goto fn_fail;
    }

    ZeroMemory(numa_socket_masks, numa_socket_count * sizeof(HWAFFINITY));

    for(ULONG nodeIndex = 0; nodeIndex < numa_socket_count; nodeIndex++)
    {
        if(g_IsWin7OrGreater)
        {
            GROUP_AFFINITY groupAffinity;
            if (Kernel32::Methods.GetNumaNodeProcessorMaskEx((UCHAR) nodeIndex, &groupAffinity) == FALSE)
            {
                goto fn_fail;
            }

            numa_socket_masks[nodeIndex].GroupId = groupAffinity.Group;
            numa_socket_masks[nodeIndex].Mask = groupAffinity.Mask;
        }
        else
        {
            ULONGLONG nodeProcessorMask;
            if (GetNumaNodeProcessorMask((UCHAR) nodeIndex, &nodeProcessorMask) == FALSE)
            {
                goto fn_fail;
            }

            numa_socket_masks[nodeIndex].GroupId = 0;
            numa_socket_masks[nodeIndex].Mask = nodeProcessorMask;
        }
    }

    numa_socket_info_init_done = true;
    return numa_socket_count;

fn_fail:
    if(numa_socket_masks != NULL)
    {
        free(numa_socket_masks);
        numa_socket_masks = NULL;
        numa_socket_count = 0;
    }

    numa_socket_info_init_done = true;
    return numa_socket_count;
}


// Checks whether the provided affinity mask is contained within any of the existing sockets, and returns the socket ID if one is found
bool FindSocketForAffinityMask(HWAFFINITY affinity, ULONG *socketId)
{
    if (numa_socket_masks == NULL)
    {
        return false;
    }

    for(ULONG s=0; s<numa_socket_count; s++)
    {
        if(affinity.GroupId == numa_socket_masks[s].GroupId &&
           affinity.Mask != 0 &&
           ((affinity.Mask & (~numa_socket_masks[s].Mask)) == 0))
        {
            *socketId = s;
            return true;
        }
    }

    return false;
}

// gets the global rank of the given rank in comm
static inline int GetGlobalRank(const MPID_Comm *comm, int rank)
{
    return comm->vcr[rank]->pg_rank;
}


// Checks to see whether it makes sense to use NUMA socket aware algorithms.
// for the given comm.
//
// Here's the criteria we use:
//      - The affinity masks of all ranks must be constrained to a singled socket
//      - There must be at least 2 sockets with one or more ranks affinitized to them
bool MPIU_Should_use_socket_awareness(const MPID_Comm *comm)
{
    HWAFFINITY *rank_affinities = (HWAFFINITY *) PMI_Get_rank_affinities();

    if (PopulateNumaSocketInfo() == 0 ||
        rank_affinities == NULL)
    {
        return false;
    }

    int num_ranks_on_socket[256]; // max # of NUMA sockets on win2K8 Datacenter edition is 64, so this is safe.
    ZeroMemory(num_ranks_on_socket, sizeof(num_ranks_on_socket));

    // traverse all ranks, and checks their socket associations
    for(int r=0; r<comm->remote_size; r++)
    {
        int world_rank = GetGlobalRank(comm, r);
        if(rank_affinities[world_rank].Mask == 0)
        {
            continue;    // skip remote ranks
        }

        ULONG socketID = 0;
        if(FindSocketForAffinityMask(rank_affinities[world_rank], &socketID) == false)
        {
            return false;   // Do not allow socket aware algorithms, as we have at least one rank whose affinity spans multiple sockets
        }

        num_ranks_on_socket[socketID % _countof(num_ranks_on_socket)] += 1;  // record rank encountered for this socket
    }

    int num_sockets_with_a_rank = 0;

    for(int s=0; s<_countof(num_ranks_on_socket); s++)
    {
        if(num_ranks_on_socket[s] > 0) num_sockets_with_a_rank++;
    }

    return (num_sockets_with_a_rank > 1);   // use socket awareness only if we have 2 or more sockets with ranks on them
}


int GetSocketIdForRank(const MPID_Comm *comm, int rank)
{
    int grank = GetGlobalRank(comm, rank);

    HWAFFINITY *rank_affinities = (HWAFFINITY *) PMI_Get_rank_affinities();
    MPIU_Assert(rank_affinities != NULL);

    HWAFFINITY affinity = rank_affinities[grank];

    ULONG socketId = 0;
    if(FindSocketForAffinityMask(affinity,&socketId) == false)
    {
        return -1;
    }

    return (int) socketId;
}


/* MPIU_Find_socket_local_and_roots -- from the list of processes in comm,
   builds a list of socket-local processes, i.e., processes on this same
   socket, and a list of external processes, i.e., one process from each
   remote socket.

   Note that this will not work correctly for spawned or attached
   processes.

   external processes: For a communicator, there is one external
                       process per socket.  This is the socket-root.

   OUT:
     local_size_p        - number of processes on this socket

     local_ranks_p       - (*local_ranks_p)[i] = the rank in comm  of the process
                         with local rank i. This is of size (*local_size_p)

     roots_size_p        - number of external processes (same machine, remote sockets)

     root_ranks_p        - (*root_ranks_p)[i] = the rank in comm
                         of the process with external rank i.
                         This is of size (*roots_size_p)

     mappings          - mapping in which to fill in the node local and root
                         for each process in the input communicator.
*/
int MPIU_Find_socket_local_and_leaders(
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
    int socket_id;
    int my_socket_id;

    MPIU_Assert( comm->comm_kind != MPID_INTERCOMM );

    int socket_count = static_cast<int>(PopulateNumaSocketInfo());
    if( socket_count == 0 )
    {
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**fail");
    }

    //
    // The input communicator is already scoped to a single node at this point,
    // so we can rely on rank 0 being the 'leader' at a higher level in the hierarchy.
    //

    //
    // Scan through the list of processes in comm and add one
    // process from each socket to the list of "external" processes.  We
    // add the first process we find from each node.  socket_leaders[] is an
    // array where we keep track of whether we have already added that
    // socket to the list.
    //
    MPIU_Assert(socket_count >= 1);
    StackGuardArray<int> socket_leaders = new int[socket_count];
    if( socket_leaders == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    //
    // socket_leaders maps socket_id to rank in leader_ranks
    //
    RtlFillMemory( socket_leaders, sizeof(int) * socket_count, -1 );

    my_socket_id = GetSocketIdForRank(comm, comm->rank);
    MPIU_Assert(my_socket_id >= 0);
    MPIU_Assert(my_socket_id < socket_count);

    //
    // Build the array of leader processes.  We need to do this before building the
    // mappings array so that we can fill in the leader inforation properly.
    //
    bool isLeader = false;
    leaders_size = 0;
    for (i = 0; i < comm->remote_size; ++i)
    {
        socket_id = GetSocketIdForRank(comm, i);

        //
        // All ranks are local, so we should never get -1 here.
        //
        MPIU_Assert( socket_id != -1 );

        MPIU_Assert(socket_id <= socket_count);

        /* build list of external processes */
        if (socket_leaders[socket_id] == -1)
        {
            //
            // This is the first rank on this socket - it is socket root.
            // Store the rank of the socket root for this socket.
            //
            socket_leaders[socket_id] = leaders_size;
            //
            // Store the mapping from rank in root_ranks to rank in comm.
            //
            __analysis_assume( leaders_size < comm->remote_size );
            leader_ranks[leaders_size] = i;
            ++leaders_size;

            //
            // Flag if we're a leader.
            //
            if( i == comm->rank )
            {
                MPIU_Assert( socket_id == my_socket_id );
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
        socket_id = GetSocketIdForRank(comm, i);

        //
        // All ranks are local, so we should never get -1 here.
        //
        MPIU_Assert( socket_id != -1 );

        MPIU_Assert(socket_id <= socket_count);

        if (socket_id == my_socket_id)
        {
            if( isLeader == true && i == comm->rank )
            {
                //
                // The leader needs to store the leader ranks.
                //
                mappings[i].rank = socket_leaders[socket_id];
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
        else if( isLeader == true )
        {
            //
            // Leaders store the rank of their peers for remote processes.
            //
            mappings[i].rank = socket_leaders[socket_id];
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
        MPIU_Assert( mappings[comm->rank].rank == static_cast<ULONG>(socket_leaders[my_socket_id]) );
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
