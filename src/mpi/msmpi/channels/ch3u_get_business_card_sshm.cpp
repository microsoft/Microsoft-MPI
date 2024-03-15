// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"
#include "pmi.h"


/*  MPIDI_CH3U_Get_business_card_sshm - does sshm specific portion of getting
 *                    a business card
 *     bc_val_p     - business card value buffer pointer, updated to the next
 *                    available location or freed if published.
 *     val_max_sz_p - ptr to maximum value buffer size reduced by the number
 *                    of characters written
 */
MPI_METHOD
MPIDI_CH3U_Get_business_card_sshm(
    _Deref_out_z_cap_c_(*val_max_sz_p) char** bc_val_p,
    _Inout_ int*                              val_max_sz_p
    )
{
    char queue_name[100];
    int str_errno;

    str_errno = MPIU_Str_add_string_arg(bc_val_p, val_max_sz_p,
                                        MPIDI_CH3I_SHM_HOST_KEY,
                                        MPIDI_CH3U_Hostname_sshm());
    if (str_errno != MPIU_STR_SUCCESS)
    {
        if (str_errno == MPIU_STR_NOMEM)
            return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**buscard_len");

        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**buscard");
    }

    queue_name[0] = '\0';
    MPIDI_CH3I_BootstrapQ_tostring(MPIDI_CH3U_BootstrapQ_sshm(), queue_name, _countof(queue_name));

    str_errno = MPIU_Str_add_string_arg(bc_val_p, val_max_sz_p,
                                        MPIDI_CH3I_SHM_QUEUE_KEY, queue_name);
    if (str_errno != MPIU_STR_SUCCESS)
    {
        if (str_errno == MPIU_STR_NOMEM)
            return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**buscard_len");

        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**buscard");
    }

    return MPI_SUCCESS;
}
