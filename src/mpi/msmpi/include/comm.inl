// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

//
// This constructor is public so that the debugger extension
// can instantiate a temporary object to store the data it reads
// from the MPI process
//
inline MPID_Comm::MPID_Comm() :
#ifdef HAVE_DEBUGGER_SUPPORT
    comm_next(nullptr),
#endif
    handle(0),
    ref_count(0),
    context_id(GUID_NULL),
    recvcontext_id(GUID_NULL),
    remote_size(0),
    rank(0),
    comm_kind(MPID_INTRACOMM_SHM_AWARE),
    vcr(nullptr),
    attributes(nullptr),
    group(nullptr),
    name(nullptr),
    errhandler(nullptr)
{
    inter.local_size = 0;
}

__forceinline void MPID_Comm::SetRef(long value)
{
    InterlockedExchange(&ref_count, value);
}

__forceinline void MPID_Comm::AddRef()
{
    ::InterlockedIncrement(&ref_count);
}

__forceinline MPI_RESULT MPID_Comm::Release( int disconnect )
{
    MPIU_Assert( ref_count > 0 );
    long value = ::InterlockedDecrement(&ref_count);
    if( value == 0 )
    {
        return Destroy(this, disconnect);
    }
    return MPI_SUCCESS;
}

__forceinline bool MPID_Comm::Busy() const
{
    return ref_count > 1;
}

inline MPID_VCRT* MPID_Comm::GetVcrt() const
{
    return CONTAINING_RECORD( vcr, MPID_VCRT, vcr_table );
}

//
// Returns true if the given communicator is aware of node topology information,
// false otherwise.  Such information could be used to implement more efficient
// collective communication, for example.
//
__forceinline bool MPID_Comm::IsNodeAware() const
{
    return ((intra.local_subcomm != nullptr
        && intra.local_subcomm->comm_kind == MPID_INTRACOMM_NODE)
        || (intra.leaders_subcomm != nullptr
        && intra.leaders_subcomm->comm_kind == MPID_INTRACOMM_NODE_ROOT));
}

__forceinline bool MPID_Comm::IsHaAware() const
{
    return (intra.local_subcomm != nullptr || intra.leaders_subcomm != nullptr);
}

__forceinline bool MPID_Comm::IsIntraSocketComm() const
{
    return (comm_kind == MPID_INTRACOMM_SOCKET);
}

__forceinline CollectiveSwitchover* MPID_Comm::SwitchPoints() const
{
    return &Mpi.SwitchoverSettings[comm_kind & MPIR_TAG_RESERVED_LSBITS_MASK];
}


__forceinline
unsigned
MPID_Comm::GetNextNBCTag(
    _In_ unsigned baseTag
)
{
    unsigned tag = baseTag | ( nbc_tag_seq << MPIR_NBC_TAG_LSBITS );
    if( ++nbc_tag_seq > MPIR_NBC_END_TAG_SEQUENCE )
    {
        nbc_tag_seq = MPIR_NBC_BEGIN_TAG_SEQUENCE;
    }
    return tag;
}


__forceinline
unsigned
MPID_Comm::RankAdd(
    _In_range_( <, remote_size ) unsigned distance,
    _In_range_( <, remote_size ) unsigned from
    )
{
    MPIU_Assert( distance < fixme_cast<unsigned>( remote_size ) );
    MPIU_Assert( from < fixme_cast<unsigned>( remote_size ) );
    unsigned result = from + distance;
    if( result >= fixme_cast<unsigned>( remote_size ) )
    {
        result -= fixme_cast<unsigned>( remote_size );
    }
    return result;
}


__forceinline
unsigned
MPID_Comm::RankAdd(
    _In_range_( <, remote_size ) unsigned distance
    )
{
    return RankAdd( distance, fixme_cast<unsigned>( rank ) );
}


__forceinline
unsigned
MPID_Comm::RankSub(
    _In_range_( <, remote_size ) unsigned distance,
    _In_range_( <, remote_size ) unsigned from
    )
{
    MPIU_Assert( distance < fixme_cast<unsigned>( remote_size ) );
    MPIU_Assert( from < fixme_cast<unsigned>( remote_size ) );
    if( from < distance )
    {
        return from + fixme_cast<unsigned>( remote_size ) - distance;
    }
    return from - distance;
}


__forceinline
unsigned
MPID_Comm::RankSub(
    _In_range_( <, remote_size ) unsigned distance
    )
{
    return RankSub( distance, fixme_cast<unsigned>( rank ) );
}


static inline MPI_RESULT MPIR_Comm_release(MPID_Comm* comm, int disconnect)
{
    return comm->Release( disconnect );
}
