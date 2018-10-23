// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef COMM_H
#define COMM_H

#include "objpool.h"

#include <initguid.h>

/* 00000000-0000-0000-0000-000000000000 */
DEFINE_GUID(GUID_NULL,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);


/* 00000000-0000-0000-0000-000000000001 */
DEFINE_GUID(MPI_WORLD_CONTEXT_ID,
            0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);


/* 00000000-0000-0000-0000-000000000002 */
DEFINE_GUID(MPI_SELF_CONTEXT_ID,
            0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02);

//
// Structure for mapping from communicator rank to sub-communicator rank.
//
// A process that is a member of both local and leader subcomms will be
// stored with 'isLocal' set to 0, and its rank in the leader subcomm.
//
struct HA_rank_mapping
{
    ULONG  rank : 31;
    ULONG  isLocal : 1;
};


//
// To ensure that internal point-to-point operations do not collide with
// application tags, internal tags all use negative tag space (which is
// not allowed in the API).
//
// Note that for collective operations, the communicator kind is
// ORed into the tag to encode the communicator type on which the
// operation is occuring in case of HA collectives.  These extra bits
// are encoded in the upper nibble of the most significant byte.
//
#define MPIR_TAG_INTERNAL_BASE          0x80000000
//
// We reserve the 4 lower bits of the tag to allow encoding communicator
// information, so that it can easilly be masked out without requiring
// shifts.  See the MPID_Comm_kind_t definition for authoritative details.
// The fifth bit is used to store info about sync/async variant of the
// operation that is being run (0 for sync, 1 for async)
//
#define MPIR_TAG_RESERVED_LSBITS        4
#define MPIR_TAG_RESERVED_LSBITS_MASK   0x0000000F

#define MPIR_INTERNAL_TAG( _x )         ( MPIR_TAG_INTERNAL_BASE | \
                                          ( _x << (MPIR_TAG_RESERVED_LSBITS + 1) ) )

#define MPIR_GET_ASYNC_TAG( tag )       ( tag | 1 << MPIR_TAG_RESERVED_LSBITS )

#define MPIR_NBC_TAG_LSBITS             12

//
// The comm kind is used in several ways:
// - identifies the type of communicator.  Note that there are several types
//   for intracommunicators, but only one type for intercommunicators.
// - used as the collective operation tag modifier to separate concurrent
//   operations on HA subcommunicators.
// - encodes the type of intracommunicator for HA collectives.
//
// Note that the MSb is always set in these values, to force all internal pt2pt
// operations for collectives to be above the highest user tag (MPI_TAG_UB == INT_MAX).
// While this is redundant with MPIR_INTERNAL_TAG_BASE, MPIR_INTERNAL_TAG_BASE is
// used with normal pt2pt operations that don't use MPIC_ routines.
//
// We reserve MPIR_TAG_RESERVED_LSBITS at the bottom of the tag space.  These are
// used as indexes into the collective switchover points array.
//
typedef enum MPID_Comm_kind_t
{
    //
    // Intercomms do not support HA.
    //
    MPID_INTERCOMM              = 0x80000000 | COLL_SWITCHOVER_FLAT,

    //
    // Intracomm that is explicitly flat (does not support HA or split_type), e.g. MPI_COMM_SELF.
    //
    MPID_INTRACOMM_FLAT         = 0x90000000 | COLL_SWITCHOVER_FLAT,

    //
    // Intracomm that is created when HA is not enabled.
    //
    MPID_INTRACOMM_SHM_AWARE    = 0xA0000000 | COLL_SWITCHOVER_FLAT,

    //
    // Intracomm that may have HA subcomms, but is used for non-HA-aware collectives.
    //
    MPID_INTRACOMM_PARENT       = 0xB0000000 | COLL_SWITCHOVER_FLAT,

    //
    // Subcommunicator representing node roots for HA collectives.
    //
    MPID_INTRACOMM_NODE_ROOT    = 0xC0000000 | COLL_SWITCHOVER_INTER_NODE,

    //
    // Subcommunicator representing all processes on the same node.
    //
    MPID_INTRACOMM_NODE         = 0xD0000000 | COLL_SWITCHOVER_INTRA_NODE,

    //
    // Subcommunicator representing socket roots on the same node, used for
    // socket-aware collectives (e.g. Bcast).
    //
    MPID_INTRACOMM_SOCKET_ROOT  = 0xE0000000 | COLL_SWITCHOVER_INTER_NODE,

    //
    // Subcommunicator represengint all processes on the same socket, used
    // for socket-aware collectives (e.g. Bcast).
    //
    MPID_INTRACOMM_SOCKET       = 0xF0000000 | COLL_SWITCHOVER_INTRA_NODE

} MPID_Comm_kind_t;


struct CommTraits : public ObjectTraits<MPI_Comm>
{
    static const MPID_Object_kind KIND = MPID_COMM;
    static const int DIRECT = 8;
    static const int BUILTINMASK = 0xFFFFFFFF;
/* Preallocated comm objects.  There are 3: comm_world, comm_self, and
   a private (non-user accessible) dup of comm world that is provided
   if needed in MPI_Finalize.  Having a separate version of comm_world
   avoids possible interference with User code */
    //TODO: Looks like the code only uses the first 2 builtin comms.
    static const int BUILTIN = 3;
};

class CommPool;


/* Communicators */

/*S
  MPID_Comm - Description of the Communicator data structure

  Notes:
  Note that the size and rank duplicate data in the groups that
  make up this communicator.  These are used often enough that this
  optimization is valuable.

  This definition provides only a 16-bit integer for context id''s .
  This should be sufficient for most applications.  However, extending
  this to a 32-bit (or longer) integer should be easy.

  There are two context ids.  One is used for sending and one for
  receiving.  In the case of an Intracommunicator, they are the same
  context id.  They differ in the case of intercommunicators, where
  they may come from processes in different comm worlds (in the
  case of MPI-2 dynamic process intercomms).

  The virtual connection table is an explicit member of this structure.
  This contains the information used to contact a particular process,
  indexed by the rank relative to this communicator.

  Groups are allocated lazily.  That is, the group pointers may be
  null, created only when needed by a routine such as 'MPI_Comm_group'.
  The local process ids needed to form the group are available within
  the virtual connection table.
  For intercommunicators, we may want to always have the groups.  If not,
  we either need the 'group' or we need a virtual connection table
  corresponding to the 'group' (we may want this anyway to simplify
  the implementation of the intercommunicator collective routines).

  The pointer to the structure 'MPID_Collops' containing pointers to the
  collective
  routines allows an implementation to replace each routine on a
  routine-by-routine basis.  By default, this pointer is null, as are the
  pointers within the structure.  If either pointer is null, the implementation
  uses the generic provided implementation.  This choice, rather than
  initializing the table with pointers to all of the collective routines,
  is made to reduce the space used in the communicators and to eliminate the
  need to include the implementation of all collective routines in all MPI
  executables, even if the routines are not used.

  Module:
  Communicator-DS

  Question:
  For fault tolerance, do we want to have a standard field for communicator
  health?  For example, ok, failure detected, all (live) members of failed
  communicator have acked.
  S*/
struct MPID_Comm
{
    friend class ObjectPool<CommPool, MPID_Comm, CommTraits>;

public:
    int           handle;        /* value of MPI_Comm for this structure */
private:
    volatile long ref_count;
public:
    MPI_CONTEXT_ID  context_id;    /* Send context id.  See notes */
    MPI_CONTEXT_ID  recvcontext_id;/* Assigned context id */
    int           remote_size;   /* Value of MPI_Comm_remote_size for intercomms,
                                    MPI_Comm_size for intracomms */
    int           rank;          /* Value of MPI_Comm_rank */
    union
    {
        MPID_VCR*  vcr;          /* virtual connections reference table */
        MPID_Comm* next;         /* link while objects are in the pool. */
    };
    MPID_Comm_kind_t comm_kind;  /* communicator kind, and collective tag modifier. */
    unsigned nbc_tag_seq;

    union
    {
        struct
        {
            struct MPID_Comm    *local_comm; /* Defined only for intercomms, holds
                                             an intracomm for the local group */
            int           is_low_group;   /* For intercomms only, this boolean is
                                             set for all members of one of the
                                             two groups of processes and clear for
                                             the other.  It enables certain
                                             intercommunicator collective operations
                                             that wish to use half-duplex operations
                                             to implement a full-duplex operation */
            int           local_size;    /* Value of MPI_Comm_size for intercomms */
        } inter;
        struct
        {
            //
            // The following subcommunicators represent the next level of local vs. remote
            // processes.  A leaf subcommunicator will have both set to NULL.  It is possible
            // for local to be non-NULL while remote is NULL (single node with offload).
            //
            struct MPID_Comm* local_subcomm;
            struct MPID_Comm* leaders_subcomm;
            HA_rank_mapping*  ha_mappings;
        } intra;
    };

    MPID_Attribute *attributes;  /* List of attributes */
    MPID_Group   *group;         /* Group in communicator. For intercomms, the group
                                    is the remote group, and the local group is the group
                                    of inter.local_comm. */
    const char*          name;  /* Required for MPI-2 */
    MPID_Errhandler *errhandler; /* Pointer to the error handler structure */

#ifdef HAVE_DEBUGGER_SUPPORT
    struct MPID_Comm     *comm_next;/* Provides a chain through all active communicators */
#endif

public:
    //
    // This constructor is public so that the debugger extension
    // can instantiate a temporary object to store the data it reads
    // from the MPI process
    //
    MPID_Comm();

    __forceinline void SetRef(long value);

    __forceinline void AddRef();

    __forceinline MPI_RESULT Release( int disconnect );

    __forceinline bool Busy() const;

    MPID_VCRT* GetVcrt() const;

    //
    // Returns true if the given communicator is aware of node topology information,
    // false otherwise.  Such information could be used to implement more efficient
    // collective communication, for example.
    //
    __forceinline bool IsNodeAware() const;

    __forceinline bool IsHaAware() const;

    __forceinline bool IsIntraSocketComm() const;

    __forceinline CollectiveSwitchover* SwitchPoints() const;

    __forceinline
    unsigned
    GetNextNBCTag(
        _In_ unsigned baseTag
    );

    const char* GetDefaultName() const;

    __forceinline
    unsigned
    MPID_Comm::RankAdd(
        _In_range_( <, remote_size ) unsigned distance,
        _In_range_( <, remote_size ) unsigned from
        );

    __forceinline
    unsigned
    MPID_Comm::RankSub(
        _In_range_( <, remote_size ) unsigned distance,
        _In_range_( <, remote_size ) unsigned from
        );

    __forceinline
    unsigned
    MPID_Comm::RankAdd(
        _In_range_( <, remote_size ) unsigned distance
        );

    __forceinline
    unsigned
    MPID_Comm::RankSub(
        _In_range_( <, remote_size ) unsigned distance
        );

private:
    /* MPIR_Comm_destroy is a helper routine that destroys the comm.
       The second arg is false unless this is called as part of MPI_Comm_disconnect.
    */
    static int Destroy(MPID_Comm* comm, int disconnect);
};


class CommPool : public ObjectPool<CommPool, MPID_Comm, CommTraits>
{
};


static inline MPI_RESULT MPIR_Comm_release( MPID_Comm* comm, int disconnect );


MPI_RESULT
MPIR_Comm_copy(
    _In_ const MPID_Comm* comm_ptr,
    _In_range_(>, 0) int size,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    );

MPI_RESULT
MPIR_Comm_create(
    MPID_Comm** comm_pptr,
    MPID_Comm_kind_t kind
    );


MPI_RESULT MPIR_Get_contextid( _In_ const MPID_Comm *, _Out_ MPI_CONTEXT_ID* contextid, _In_ bool abort = false );

VOID
MPIR_Create_temp_contextid(
    _Out_ MPI_CONTEXT_ID*   new_contextid,
    _In_ const MPI_CONTEXT_ID& contextid
    );

//
// Topology aware communicator utilities.
//
int MPID_Get_max_node_id();
int MPID_Get_node_id(const MPID_Comm *comm, int rank);

MPI_RESULT MPIR_Comm_commit( MPID_Comm *comm);

MPI_RESULT
MPIU_Find_node_local_and_leaders(
    _In_ const MPID_Comm *comm,
    _Out_ int *local_size_p,
    _Out_writes_to_(comm->remote_size, *local_size_p) int local_ranks[],
    _Out_ int *leaders_size_p,
    _Out_writes_to_(comm->remote_size, *leaders_size_p) int leader_ranks[],
    _Out_writes_(comm->remote_size) HA_rank_mapping mappings[]
    );

bool MPIU_Should_use_socket_awareness(const MPID_Comm *comm);

MPI_RESULT
MPIU_Find_socket_local_and_leaders(
    _In_ const MPID_Comm *comm,
    _Out_ int *local_size_p,
    _Out_writes_to_(comm->remote_size, *local_size_p) int local_ranks[],
    _Out_ int *leaders_size_p,
    _Out_writes_to_(comm->remote_size, *leaders_size_p) int leader_ranks[],
    _Out_writes_(comm->remote_size) HA_rank_mapping mappings[]
    );

MPI_RESULT
MPIR_Comm_create_intra(
    _In_ MPID_Comm *comm_ptr,
    _In_ MPID_Group *group_ptr,
    _In_reads_opt_(group_ptr->size) int mapping[],
    _In_opt_ MPID_VCR* mapping_vcr,
    _When_(group_ptr->rank == MPI_UNDEFINED, _Outptr_result_maybenull_)
    _When_(group_ptr->rank != MPI_UNDEFINED, _Outptr_) MPID_Comm** outcomm_ptr
    );

MPI_RESULT
MPIR_Comm_create_inter(
    _In_ MPID_Comm* comm_ptr,
    _In_ MPID_Group* group_ptr,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    );

MPI_RESULT
MPIR_Comm_split_intra(
    _In_ MPID_Comm* comm_ptr,
    _Pre_satisfies_(color == MPI_UNDEFINED || color >= 0) int color,
    _In_ int key,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    );

MPI_RESULT
MPIR_Comm_split_inter(
    _In_ MPID_Comm* comm_ptr,
    _Pre_satisfies_(color == MPI_UNDEFINED || color >= 0) int color,
    _In_ int key,
    _Outptr_result_maybenull_ MPID_Comm** outcomm_ptr
    );

MPI_RESULT
MPIR_Intercomm_create_root(
    _In_ const MPID_Comm* comm_ptr,
    _In_ const MPID_Comm* peer_comm_ptr,
    _In_ int              remote_leader,
    _In_ int              tag,
    _Outptr_ MPID_Comm**  newcomm
    );

MPI_RESULT
MPIR_Intercomm_create_non_root(
    _In_  const MPID_Comm* comm_ptr,
    _In_  int              root,
    _Outptr_ MPID_Comm**   newcomm
    );

MPI_RESULT
MPIR_Comm_group(
    _In_ MPID_Comm* comm_ptr,
    _Outptr_ MPID_Group** group_ptr
    );


//
// This is a convenient data structure to hold metadata
// for a remote group
//
struct RemoteGroupMetadata
{
    MPI_CONTEXT_ID  remoteContextId;
    unsigned int    remoteCommSize;
    int             is_low_group;
};


//
// Summary:
// Non-root process uses this routine to send business card to
// the root process
//
// In:
// comm_ptr  : The pointer to the communicator
// root      : The root process
//
// Return:
// MPI_SUCCESS if the function succeeds, other errors otherwise.
//
// Remarks: None
//
MPI_RESULT
SendLocalPGInfo(
    _In_ const MPID_Comm*       comm_ptr,
    _In_ int                    root
    );


//
// Summary:
// Root of MPI_Comm_connect/accept uses this routine to
// receive PG's from the local group of a communicator.
//
// In:
// comm_ptr  : The pointer to the communicator
// prevStatus: Previous error leading to the function
//
// Out:
// pConnInfos: Array of PG info, only populated for root process
//             This parameter is ignored for the non-root
//
// Returns:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks: None
//
MPI_RESULT
ReceiveLocalPGInfo(
    _In_                         const MPID_Comm*     comm_ptr,
    _When_(prevStatus == MPI_SUCCESS, _Out_writes_(comm_ptr->remote_size))
                                 struct ConnInfoType  pConnInfos[],
    _In_                         MPI_RESULT                  prevStatus
    );


//
// Summary:
// Distribute the remote group's PG info to the local group
//
// In:
// comm_ptr        : The intracomm of the local group
// pGroupData      : The pointer to the metadata of the remote group
// pRemoteConnInfos: The array of the remote PG infos
//
//
// Returns:
// MPI_SUCCESS on success, other errors otherwise
//
// Remarks: None
//
MPI_RESULT
SendRemoteConnInfo(
    _In_     const MPID_Comm*              comm_ptr,
    _In_     const RemoteGroupMetadata*    pGroupData,
    _In_opt_ const struct ConnInfoType*    pRemoteConnInfos
    );


//
// Summary:
// Receive the remote group's PG info from the local root
//
// In:
// comm_ptr          : The intracomm of the local group
// root              : The root process
//
// Out:
// pGroupData        : The pointer to the metadata of the remote group
// ppRemoteConnInfos : The pointer to the array of the remote PG's
//
// Returns:
// MPI_SUCCESS on success, other errors otherwise
//s
// Remarks: ppRemoteConnInfos should be freed by non-root processes
// when it's no longer used
//
MPI_RESULT
ReceiveRemoteConnInfo(
    _In_    const MPID_Comm*               comm_ptr,
    _In_    int                            root,
    _Out_   RemoteGroupMetadata*           pGroupData,
    _Inout_ _Outptr_ struct ConnInfoType** ppRemoteConnInfos
    );


//
// Summary:
// Setup an intercomm
//
// In:
// comm_ptr        : Pointer to the intracomm
// is_low_group    : Indicates whether the local group will take the role of the 'low' group
//                   resulting intercomm.
// recvContextId  : The context id for receiving messages from the remote side
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
    _In_  struct ConnInfoType*     pConnInfos,
    _Outptr_ MPID_Comm**           newcomm
    );


//
// Retrieves the value for a given built-in attribute, and returns
// whether the value was written to the pValue parameter.
//
_Success_( return == TRUE )
BOOL
MPID_Comm_get_attr(
    _In_ const MPID_Comm* /*pComm*/,
    _In_ int hKey,
    _Outptr_result_maybenull_ void** pValue
    );

#endif // COMM_H
