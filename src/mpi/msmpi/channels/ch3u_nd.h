// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd.h - Network Direct MPI CH3 Channel

--*/

#pragma once

#ifndef CH3U_ND_H
#define CH3U_ND_H

struct MPIDI_VC_t;

namespace CH3_ND
{
    class Endpoint;

    namespace v1
    {
        class CEndpoint;

        int Init(
            _In_ ExSetHandle_t hExSet,
            _In_ SIZE_T cbZCopyThreshold,
            _In_ MPIDI_msg_sz_t cbEagerLimit,
            _In_ UINT64 cbMrCacheLimit,
            _In_ int fEnableFallback
            );

        void Shutdown();

        int Listen();

        int GetBusinessCard(
            _Deref_pre_cap_c_( *pcbBusinessCard ) _Deref_out_z_cap_c_( *pcbBusinessCard ) char** pszBusinessCard,
            _Inout_ int* pcbBusinessCard
            );

        int Connect(
            _Inout_ MPIDI_VC_t* pVc,
            _In_z_ const char* szBusinessCard,
            _In_ int fForceUse,
            _Out_ int* pbHandled
            );

        int Arm();

        int Poll( _Out_ BOOL* pfProgress );
    }
}

//
// MPIDI_VC_Init_nd
//
// Initialize the ND member of the VC
//
void
MPIDI_VC_Init_nd(
    __inout MPIDI_VC_t* vc
    );

//
// MPIDI_CH3I_Nd_init
//
// Initialize the ND channel
//
int
MPIDI_CH3I_Nd_init(
    __in ExSetHandle_t hExSet
    );

void
MPIDI_CH3I_Nd_finalize(
    void
    );

//
// MPIDI_CH3I_Nd_get_business_card
//
// Add the ND channel business card information
//
// Parameters:
//
//  pBusinessCard
//      pointer to the business card output buffer. On output the pointer is advanced to point to
//      the end of the buffer.
//
//  pBusinessCardLength
//      On input the size of the output buffer.
//      On output the size left in the buffer (including the size of the added \0 char)
//
int
MPIDI_CH3I_Nd_get_business_card(
    _Deref_pre_cap_c_( *pcbBusinessCard ) _Deref_out_z_cap_c_( *pcbBusinessCard ) char** pszBusinessCard,
    _Inout_ int* pcbBusinessCard
    );

//
// MPIDI_CH3I_Nd_connect
//
// Connect to the targeted using the business card
//
// Parameters:
//
//  pVc
//      The virtual connection object to use
//
//  BusinessCard
//      The business card desribing the target
//
//  pbHandled
//      On output indicates whether the connection is handled by the ND channel
//
int
MPIDI_CH3I_Nd_connect(
    __inout MPIDI_VC_t* pVc,
    __in_z const char* szBusinessCard,
    __in int fForceUse,
    __out int* pbHandled
    );

void
MPIDI_CH3I_Nd_disconnect(
    __inout MPIDI_VC_t* pVc
    );

void
MPIDI_CH3I_Ndv1_disconnect(
    __inout MPIDI_VC_t* pVc
    );

//
// MPIDI_CH3I_Nd_start_write
//
// Initiates sending a request.
//
int
MPIDI_CH3I_Nd_start_write(
    __inout MPIDI_VC_t* vc,
    __inout MPID_Request* sreq
    );

int
MPIDI_CH3I_Ndv1_start_write(
    __inout MPIDI_VC_t* vc,
    __inout MPID_Request* sreq
    );

//
// TODO: pnBytes should be replaced with a 'current offset' value that gets updated
// to the index of the first IOV in the pIov array of any remaining data to be sent.
int
MPIDI_CH3I_Nd_writev(
    __inout MPIDI_VC_t* pVc,
    __inout const MPID_IOV* pIov,
    __in int nIov,
    __out MPIU_Bsize_t* pnBytes
    );

int
MPIDI_CH3I_Ndv1_writev(
    __inout MPIDI_VC_t* pVc,
    __inout const MPID_IOV* pIov,
    __in int nIov,
    __out MPIU_Bsize_t* pnBytes
    );

//
// MPIDI_CH3I_Nd_write_progress
//
// Make progress using ND sending the message queue
//
int
MPIDI_CH3I_Nd_write_progress(
    __inout MPIDI_VC_t* pVc
    );

int
MPIDI_CH3I_Ndv1_write_progress(
    __inout MPIDI_VC_t* pVc
    );

//
// MPIDI_CH3I_Nd_progress
//
// Process ND channel events including, send and receive requests, and connection setup/teardown.
//
int
MPIDI_CH3I_Nd_progress(
    __out BOOL* pfProgress
    );

//
// MPIDI_CH3I_Nd_enable_notification
//
// Request notification of send/recv request completion to the ExHandleSet_t
// specified in the channel initialization function.
//
MPI_RESULT
MPIDI_CH3I_Nd_enable_notification(
    _Out_ BOOL* pfProgress
    );

#endif // CH3U_ND_H
