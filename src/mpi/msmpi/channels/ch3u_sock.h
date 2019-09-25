// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef CH3U_SOCK_H
#define CH3U_SOCK_H

#define MPIDI_CH3_USES_SOCK

struct MPIDI_PG_t;
struct MPIDI_VC_t;

struct MPIDI_CH3I_Connection_t
{
    MPIDI_VC_t * vc;
    sock_state_t * sock;
    sock_state_t * aux_sock;
    MPID_Request * recv_active;
    GUID pg_id;
    MPID_IOV iov[2];
    BOOL disconnect;
    MPIU_Bsize_t recv_buff_end;
    union
    {
        MPIDI_CH3_Pkt_t pkt;
        char recv_buffer[0x800 + sizeof(MPIDI_CH3_Pkt_t)];
    };
};

C_ASSERT(sizeof(((MPIDI_CH3I_Connection_t*)0)->pkt) <= sizeof(((MPIDI_CH3I_Connection_t*)0)->recv_buffer));


int MPIDI_CH3I_Connection_alloc(MPIDI_CH3I_Connection_t**);
void MPIDI_CH3I_Connection_free(MPIDI_CH3I_Connection_t* conn);

/* Routines to get the socket address */
MPI_RESULT
MPIDU_Sock_get_conninfo_from_bc(
    _In_z_              const char* bc,
    _Out_z_cap_(maxlen) char*       host_description,
    _In_                size_t      maxlen,
    _Out_               int*        port
    );

/* These two routines from util/sock initialize and shutdown the
   socket used to establish connections.  */
int MPIDU_CH3I_SetupListener( ExSetHandle_t );
void MPIDU_CH3I_ShutdownListener( void );

int MPIDI_CH3U_Init_sock(void);
void MPIDI_CH3U_Finalize_sock(void);
void MPIDI_VC_Init_sock(MPIDI_VC_t* vc) ;
int MPIDI_CH3U_Get_business_card_sock(_Deref_out_z_cap_c_(*bc_len_p) char** bc_val_p, _Inout_ int* bc_len_p);

#define MPIDI_PG_Init_sock(a) ((void)0)

#endif /* CH3_USOCK_H */
