// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"
MPIDI_CH3I_Process_t MPIDI_CH3I_Process = {NULL};

/* Define the ABI version of this channel.  Change this if the channel
   interface (not just the implementation of that interface) changes */
char MPIDI_CH3_ABIVersion[] = "1.1";

int MPIDI_CH3_Init(int has_parent, int pg_rank, const MPIDI_PG_t* worldpg)
{
    int mpi_errno;
    int pg_size;

    MPIDI_CH3I_Process.disable_shm = env_is_on_ex(
        L"MSMPI_DISABLE_SHM",
        L"MPICH_DISABLE_SHM",
        FALSE
        );

    MPIDI_CH3I_Process.disable_sock = env_is_on_ex(
        L"MSMPI_DISABLE_SOCK",
        L"MPICH_DISABLE_SOCK",
        FALSE
        );

    MPIDI_CH3I_Process.progress_fixed_spin = env_to_int_ex(
        L"MSMPI_PROGRESS_SPIN_LIMIT",
        L"MPICH_PROGRESS_SPIN_LIMIT",
        0,
        0
        );

    MPIDI_CH3I_Process.shm_eager_limit = env_to_int_ex(
        L"MSMPI_SHM_EAGER_LIMIT",
        L"MPICH_SHM_EAGER_LIMIT",
        MPIDI_CH3_EAGER_LIMIT_DEFAULT,
        MPIDI_CH3_EAGER_LIMIT_MIN
        );

    MPIDI_CH3I_Process.sock_eager_limit = env_to_int_ex(
        L"MSMPI_SOCK_EAGER_LIMIT",
        L"MPICH_SOCK_EAGER_LIMIT",
        MPIDI_CH3_EAGER_LIMIT_DEFAULT,
        MPIDI_CH3_EAGER_LIMIT_MIN
        );

    mpi_errno = MPIDI_CH3I_Progress_init();
    ON_ERROR_FAIL(mpi_errno);

    /* initialize aspects specific to sockets. */
    mpi_errno = MPIDI_CH3U_Init_sock();
    ON_ERROR_FAIL(mpi_errno);

    /* initialize aspects specific to sshm. */
    MPIDI_CH3U_Init_sshm(has_parent, pg_rank);

    mpi_errno = MPIDI_CH3I_Nd_init(MPIDI_CH3I_set);
    ON_ERROR_FAIL(mpi_errno);

    /* Obtain and publish the business card */
    mpi_errno = MPIDI_CH3_Get_business_card(
        MPIDI_Process.my_businesscard,
        _countof(MPIDI_Process.my_businesscard)
        );
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_PG_SetConnInfo( worldpg,
                                      pg_rank,
                                      MPIDI_Process.my_businesscard );
    ON_ERROR_FAIL(mpi_errno);

    //
    // Register a completion processor for receiving async
    // complete notification from the RPC layer that handles
    // MPI_Comm_connect/MPI_Comm_accept
    //
    ExRegisterCompletionProcessor( EX_KEY_CONNECT_COMPLETE, RpcAsyncNotificationHandler );

    pg_size = PMI_Get_size();

fn_exit:
    return mpi_errno;

fn_fail:
    goto fn_exit;
}


int MPIDI_CH3_Get_business_card( _Out_writes_z_( len ) char* bc, int len)
{
    int mpi_errno;
    mpi_errno = MPIDI_CH3U_Get_business_card_sock(&bc, &len);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_CH3U_Get_business_card_sshm(&bc, &len);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIDI_CH3I_Nd_get_business_card(&bc, &len);
    ON_ERROR_FAIL(mpi_errno);

fn_fail:
    return mpi_errno;
}


/* Perform the channel-specific vc initialization */
void MPIDI_CH3_VC_Init( MPIDI_VC_t *vc )
{
    vc->ch.sendq.head = NULL;
    vc->ch.sendq.tail = NULL;
    MpiLockInitialize(&vc->ch.sendq.lock);

    vc->ch.state = MPIDI_CH3I_VC_STATE_UNCONNECTED;
    vc->ch.channel = MPIDI_CH3I_CH_TYPE_SOCK;
    vc->ch.n_recv = 0;
    vc->ch.n_sent = 0;

    MPIDI_VC_Init_sock( vc );
    MPIDI_VC_Init_sshm( vc );
    MPIDI_VC_Init_nd( vc );
}


/* This routine is a hook for initializing information for a process
   group before the MPIDI_CH3_VC_Init routine is called */
void MPIDI_CH3_PG_Init(MPIDI_PG_t* pg)
{
    //
    // pBusinessCards is non-NULL in the case connect/accept
    //
    pg->pBusinessCards = NULL;
    MPIDI_PG_Init_sock(pg);
    MPIDI_PG_Init_sshm(pg);
}

/* This routine is a hook for any operations that need to be performed before
   freeing a process group */
int MPIDI_CH3_PG_Destroy( MPIDI_PG_t* pg )
{
    //
    // If this was a PG created during the process of connect/accept
    // we need to free the business cards of the remote processes
    //
    if( pg->pBusinessCards != NULL )
    {
        for( int i = 0; i < pg->size; i++ )
        {
            if( pg->pBusinessCards[i] != NULL )
            {
                MPIU_Free( pg->pBusinessCards[i] );
                pg->pBusinessCards[i] = NULL;
            }
        }
        MPIU_Free( pg->pBusinessCards );
        pg->pBusinessCards = NULL;
    }
    return MPI_SUCCESS;
}
