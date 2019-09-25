// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd_core.h - Network Direct MPI CH3 Channel

--*/

#pragma once

#ifndef CH3U_NDV1_CORE_H
#define CH3U_NDV1_CORE_H


namespace CH3_ND
{
namespace v1
{

//
// The following queue depths should be <= single byte.
//
#define NDv1_SENDQ_SIZE 16
C_ASSERT( NDv1_SENDQ_SIZE <= UCHAR_MAX );

#define NDv1_RECVQ_SIZE 64
C_ASSERT( NDv1_RECVQ_SIZE <= UCHAR_MAX );

//
// The code assumes that the read limit is equal to the MPI request IOV limit.
// It does not handle the case where the limit is less than the IOV limit.
//
#define NDv1_READ_LIMIT MPID_IOV_LIMIT
C_ASSERT( NDv1_READ_LIMIT <= UCHAR_MAX );

//
// We allocate CQEs and send WQEs for sends, RDMA read requests, MW bind, and MW unbind.
//
#define NDv1_ACTIVE_ENTRIES (NDv1_SENDQ_SIZE + (NDv1_READ_LIMIT * 3))
#define NDv1_PASSIVE_ENTRIES NDv1_RECVQ_SIZE

#define NDv1_CQ_ENTRIES_PER_EP (NDv1_ACTIVE_ENTRIES + NDv1_PASSIVE_ENTRIES)


//
// Caller data is 56 bytes max for IB.
//
struct nd_caller_data_t
{
    //
    // Version comes first, so that we can change things after it without potentially breaking things.
    //
    UINT32 Version;
    INT32 Rank;
    UINT8 Credits;
    GUID  GroupId;

};
C_ASSERT( sizeof(nd_caller_data_t) <= 56 );


}   // namespace CH3_ND::v1
}   // namespace CH3_ND

#endif // CH3U_NDV1_CORE_H
