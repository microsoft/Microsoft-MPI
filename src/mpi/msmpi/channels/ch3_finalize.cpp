// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"

int MPIDI_CH3_Finalize()
{
    MPIDI_CH3U_Finalize_sock();

    MPIDI_CH3I_Progress_finalize();

    MPIDI_CH3I_Nd_finalize();

    /* Free memory allocated in ch3_progress */
    MPIDI_CH3U_Finalize_ssm_memory();

    MPIDI_CH3U_Finalize_sshm();

    return MPI_SUCCESS;
}
