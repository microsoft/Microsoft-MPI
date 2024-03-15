// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3_nd.h - Network Direct MPI CH3 Channel

--*/

#pragma once

#ifndef CH3U_ND_CORE_H
#define CH3U_ND_CORE_H


namespace CH3_ND
{

//
// The following queue depths should be <= single byte.
//
#define ND_SENDQ_DEPTH 16
C_ASSERT( ND_SENDQ_DEPTH <= MAXUINT8 );
#define ND_SENDQ_MINDEPTH 1
C_ASSERT( ND_SENDQ_MINDEPTH <= ND_SENDQ_DEPTH );

#define ND_RECVQ_DEPTH 128
C_ASSERT( ND_RECVQ_DEPTH <= MAXUINT8 );
//
// Minimum receive queue depth is 2 to prevent deadlock with credit flow control.
//
#define ND_RECVQ_MINDEPTH 2
C_ASSERT( ND_RECVQ_MINDEPTH <= ND_RECVQ_DEPTH );

//
// Maximum queue depth that is a power of 2 and can fit in a UINT8.
//
#define ND_MAX_QUEUE_DEPTH 128

//
// The code assumes that the read limit is equal to the MPI request IOV limit.
// It does not handle the case where the limit is less than the IOV limit.
//
#define ND_READ_LIMIT MPID_IOV_LIMIT
C_ASSERT( ND_READ_LIMIT <= MAXUINT8 );


//
// Caller data is 56 bytes max for IB.
//
struct nd_caller_data_t
{
    //
    // Version comes first, so that we can change things after it without
    // potentially breaking things.
    //
    INT32 Version;
    INT32 Rank;
    UINT8 Credits;
    GUID  GroupId;

};
C_ASSERT( sizeof(nd_caller_data_t) <= 56 );


struct ND_OVERLAPPED : public OVERLAPPED
{
    typedef int // mpi_error
    (FN_CompletionRoutine)(
        __in struct ND_OVERLAPPED* pOverlapped
        );

    FN_CompletionRoutine* pfnCompletion;

    ND_OVERLAPPED( FN_CompletionRoutine* pfn = NULL ) :
        pfnCompletion( pfn )
    {
        Offset = 0;
        OffsetHigh = 0;
        hEvent = NULL;
    }
};

}   // namespace CH3_ND

#endif // CH3U_ND_CORE_H
