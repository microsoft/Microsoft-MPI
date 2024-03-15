// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#if !defined(MPICH_MPIDI_CH3_PRE_H_INCLUDED)
#define MPICH_MPIDI_CH3_PRE_H_INCLUDED

namespace CH3_ND
{
    class Endpoint;

    namespace v1
    {
        class CEndpoint;
    }
}

typedef HANDLE MPIDI_CH3I_BootstrapQ;

enum MPIDI_CH3I_VC_state_t
{
    MPIDI_CH3I_VC_STATE_UNCONNECTED,
    MPIDI_CH3I_VC_STATE_CONNECTING,
    MPIDI_CH3I_VC_STATE_PENDING_ACCEPT,
    MPIDI_CH3I_VC_STATE_ACCEPTED,
    MPIDI_CH3I_VC_STATE_CONNECTED,
    MPIDI_CH3I_VC_STATE_FAILED
};


struct MPIDI_CH3I_SHM_info_t
{
    struct MPIDI_CH3I_SHM_Queue_t *addr;
    unsigned int size;
    HANDLE hShm;
};


struct MPIDI_CH3I_Request_Queue_t
{
    struct MPID_Request * head;
    struct MPID_Request * tail;
    mutable MPI_LOCK lock;
};


struct MPIDI_CH3I_SHM_recv_t
{
    MPIDI_VC_t* next_vc;
    struct MPID_Request* request;
    struct MPIDI_CH3I_SHM_Queue_t* shm;
    MPIDI_CH3I_SHM_info_t shm_info;
    MPIU_Bsize_t slot_offset;
    int connected;
    HANDLE hProcess;
};


struct MPIDI_CH3I_SHM_send_t
{
    MPIDI_VC_t* next_vc;
    struct MPIDI_CH3I_SHM_Queue_t* shm;
    MPIDI_CH3I_SHM_info_t shm_info;
    BOOL wait_for_rma;
};


struct MPIDI_CH3I_SHM_VC_t
{
    MPIDI_CH3I_SHM_recv_t recv;
    MPIDI_CH3I_SHM_send_t send;
    MPIDI_CH3I_BootstrapQ queue;
    int state;
};


union MPIDI_CH3I_ND_VC_t
{
    CH3_ND::Endpoint* pEp;
    CH3_ND::v1::CEndpoint* pEpV1;
};


enum MPIDI_CH3I_CH_TYPE_t
{
    MPIDI_CH3I_CH_TYPE_SHM,
    MPIDI_CH3I_CH_TYPE_ND,
    MPIDI_CH3I_CH_TYPE_NDv1,
    MPIDI_CH3I_CH_TYPE_SOCK
};


struct MPIDI_CH3I_VC_t
{
    MPIDI_CH3I_Request_Queue_t sendq;
    MPIDI_CH3I_SHM_VC_t shm;
    MPIDI_CH3I_ND_VC_t nd;
    struct MPIDI_CH3I_Connection_t* conn;
    MPIDI_CH3I_VC_state_t state;
    MPIDI_CH3I_CH_TYPE_t channel;
    volatile long n_recv;
    volatile long n_sent;
};



#endif /* !defined(MPICH_MPIDI_CH3_PRE_H_INCLUDED) */
