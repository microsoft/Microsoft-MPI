// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef CH3U_SSHM_H
#define CH3U_SSHM_H

//
// TODO: Put this back.
//
//#define MPIDI_CH3_USES_SSHM

/*
 * Define the maximum host name length for the SHM hostname used in the business card,
 * as well as in the connectivity table.
 */
#define MAXHOSTNAMELEN 256

struct MPIDI_PG_t;
struct MPIDI_VC_t;
typedef HANDLE MPIDI_CH3I_BootstrapQ;
typedef ExCompletionProcessor ch3u_bootstrapq_routine;

void MPIDI_CH3U_Init_sshm(int has_parent, int pg_rank);
void MPIDI_CH3U_Finalize_sshm(void);

void MPIDI_VC_Init_sshm(MPIDI_VC_t* vc) ;
int MPIDI_CH3I_Connect_to_root_sshm(const char* port_name, MPIDI_VC_t** new_vc);

MPI_METHOD
MPIDI_CH3U_Get_business_card_sshm(
    _Deref_out_z_cap_c_(*val_max_sz_p) char** bc_val_p,
    _Inout_ int*                              val_max_sz_p
    );


void MPIDI_PG_Init_sshm(MPIDI_PG_t* pg);


MPIDI_CH3I_BootstrapQ MPIDI_CH3U_BootstrapQ_sshm();
const char* MPIDI_CH3U_Hostname_sshm();

void MPIDI_CH3I_BootstrapQ_create(MPIDI_CH3I_BootstrapQ *queue_ptr);
void MPIDI_CH3I_BootstrapQ_tostring(_In_ MPIDI_CH3I_BootstrapQ queue, _Out_writes_z_(length) char *name, _In_range_(>, 20) size_t length);
void MPIDI_CH3I_BootstrapQ_destroy(MPIDI_CH3I_BootstrapQ queue);
void MPIDI_CH3I_BootstrapQ_unlink(MPIDI_CH3I_BootstrapQ queue);
int  MPIDI_CH3I_BootstrapQ_attach(const char* name, MPIDI_CH3I_BootstrapQ * queue_ptr);
void MPIDI_CH3I_BootstrapQ_detach(MPIDI_CH3I_BootstrapQ queue);

int  MPIDI_CH3I_Notify_connect(MPIDI_CH3I_BootstrapQ queue, HANDLE hShm, int pid);
void MPIDI_CH3I_Notify_accept_connect(ch3u_bootstrapq_routine pfnAcceptConnection);

int  MPIDI_CH3I_Notify_message(MPIDI_CH3I_BootstrapQ queue);
void MPIDI_CH3I_Notify_accept_message(ch3u_bootstrapq_routine pfnAcceptMessage);


#endif /* CH3U_SSHM_H */
