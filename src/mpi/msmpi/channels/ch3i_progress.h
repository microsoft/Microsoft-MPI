// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef CH3I_PROGRESS_H_INCLUDED
#define CH3I_PROGRESS_H_INCLUDED

int MPIDI_CH3I_SOCK_start_write(
    _In_ MPIDI_VC_t* vc,
    _In_ MPID_Request* sreq
    );


int MPIDI_CH3I_SOCK_write_progress(
        _In_ MPIDI_VC_t*    vc
        );

int MPIDI_CH3I_Post_sendv(MPIDI_CH3I_Connection_t* conn, MPID_IOV* iov, int iov_n);
int MPIDI_CH3I_Post_accept(MPIDI_CH3I_Connection_t* listener);
int MPIDI_CH3I_Post_close_connection(MPIDI_CH3I_Connection_t* conn);

int
MPIDI_CH3I_Post_connect(
    _In_   MPIDI_CH3I_Connection_t* conn,
    _In_z_ const char*              host_description,
    _In_   int                      port
    );

int MPIDI_CH3I_SHM_write_progress_vc(
    _In_ MPIDI_VC_t * firstVc,
    _Out_ BOOL* pfProgress
    );

int MPIDI_CH3I_SHM_write_progress(
    _In_ MPIDI_VC_t * vcChain,
    _Out_ BOOL* pfProgress
    );

void MPIDI_CH3I_Progress_spin_up(void);
void MPIDI_CH3I_Progress_spin_down(void);

#endif
