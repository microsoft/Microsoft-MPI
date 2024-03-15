// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"
#include "pmi.h"


/*  MPIDI_CH3U_Finalize_sshm - does scalable shared memory specific channel finalization
 */

/* FIXME: Should this be registered as a finalize handler?  Should there be
   a corresponding abort handler? */

void MPIDI_CH3U_Finalize_sshm()
{
    MPIDI_PG_t * pg;
    MPIDI_PG_t * pg_next;

    MPIDI_CH3I_BootstrapQ_unlink(MPIDI_CH3U_BootstrapQ_sshm());
    MPIDI_CH3I_BootstrapQ_destroy(MPIDI_CH3U_BootstrapQ_sshm());

    /* brad : added for dynamic processes in ssm.  needed because the vct's can't be freed
     *         earlier since the vc's themselves are still needed here to walk though and
     *         free their member fields.
     */

    MPIDI_PG_Iterate_reset();
    MPIDI_PG_Get_next(&pg);
    /* This Get_next causes us to skip the process group associated with
       out MPI_COMM_WORLD.  */
    MPIDI_PG_Get_next(&pg);
    while(pg)
    {
        MPIDI_PG_Get_next(&pg_next);
        MPIDI_PG_release_ref(pg);
        pg = pg_next;
    }
}
