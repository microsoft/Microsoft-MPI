// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "mpidi_ch3_impl.h"
#include "pmi.h"

static int getNumProcessors( void );
static int getNodeRootRank( int, int * );


/*
 * The shared memory local bootstrapQ and it's access function
 */
static MPIDI_CH3I_BootstrapQ g_sshm_bootstrapQ = NULL;

MPIDI_CH3I_BootstrapQ MPIDI_CH3U_BootstrapQ_sshm()
{
    MPIU_Assert(g_sshm_bootstrapQ != NULL);
    return g_sshm_bootstrapQ;
}


/*
 * The shared memory local hostname and it's access function
 */
static char g_sshm_hostname[MAXHOSTNAMELEN] = {0};

const char* MPIDI_CH3U_Hostname_sshm()
{
    MPIU_Assert(g_sshm_hostname[0] != '\0');
    return g_sshm_hostname;
}

/*  MPIDI_CH3U_Init_sshm - does scalable shared memory specific channel
 *  initialization
 *
 */

/* This routine is used only by channels/{sshm,ssm}/src/ch3_init.c */

void MPIDI_CH3U_Init_sshm(int /*has_parent*/, int /*pg_rank*/)
{
    MPIDI_CH3I_Process.shm_reading_list = NULL;
    MPIDI_CH3I_Process.shm_writing_list = NULL;

    gethostname(g_sshm_hostname, _countof(g_sshm_hostname));

    MPIDI_CH3I_BootstrapQ_create(&g_sshm_bootstrapQ);
    MPIDI_CH3I_Notify_accept_connect(MPIDI_CH3I_Accept_shm_connection);
    MPIDI_CH3I_Notify_accept_message(MPIDI_CH3I_Accept_shm_message);
}


void MPIDI_PG_Init_sshm(MPIDI_PG_t * pg)
{
    /*
     * Extra pg ref needed for shared memory modules because the
     * shm_XXXXing_list's
     * need to be walked though in the later stages of finalize to
     * free queue_info's.
     */
    MPIDI_PG_add_ref(pg);
}


/* This routine initializes shm-specific elements of the VC */
void MPIDI_VC_Init_sshm( MPIDI_VC_t *vc )
{
    vc->ch.shm.recv.next_vc = NULL;
    vc->ch.shm.recv.request = NULL;
    vc->ch.shm.recv.shm = NULL;
    vc->ch.shm.recv.connected = 0;
    vc->ch.shm.recv.slot_offset = 0;

    vc->ch.shm.send.next_vc = NULL;
    vc->ch.shm.send.shm = NULL;

    vc->ch.shm.queue = NULL;
    vc->ch.shm.state = 0;

    vc->ch.shm.send.wait_for_rma = FALSE;
    vc->ch.shm.recv.hProcess = NULL;
}
