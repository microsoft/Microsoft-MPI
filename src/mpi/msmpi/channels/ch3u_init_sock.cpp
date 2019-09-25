// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

/*
 * MPIDI_CH3U_Init_sock - does socket specific channel initialization
 */

int MPIDI_CH3U_Init_sock(void)
{
    int mpi_errno = MPIDU_Sock_init();
    ON_ERROR_FAIL(mpi_errno);

    if( MPIDI_CH3I_Process.disable_sock == TRUE )
    {
        return MPI_SUCCESS;
    }

    //
    // Disable sockets by default in SDK mode, except if:
    //  - SHM is disabled.
    //  - SOCK is explicitly enabled (MSMPI_DISABLE_SOCK = 0).
    //
    if( env_is_on(L"MSMPI_LOCAL_ONLY", FALSE) == TRUE &&
        env_is_on_ex(L"MSMPI_DISABLE_SOCK", L"MPICH_DISABLE_SOCK", TRUE) == TRUE &&
        MPIDI_CH3I_Process.disable_shm == FALSE )
    {
        return MPI_SUCCESS;
    }

    //
    // Establish non-blocking listener
    //
    mpi_errno = MPIDU_CH3I_SetupListener( MPIDI_CH3I_set );
    ON_ERROR_FAIL(mpi_errno);

fn_fail:
    return mpi_errno;
}


void MPIDI_CH3U_Finalize_sock(void)
{
    MPIDU_CH3I_ShutdownListener();

    /* FIXME: Cleanly shutdown other socks and MPIU_Free connection
       structures. (close protocol?) */

    MPIDU_Sock_finalize();
}


/* This routine initializes Sock-specific elements of the VC */
void MPIDI_VC_Init_sock( MPIDI_VC_t *vc )
{
    vc->ch.conn = NULL;
}

