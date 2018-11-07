// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"


static bool g_exit_progress;
static int g_smpd_exit_code = MPI_SUCCESS;

void smpd_signal_exit_progress(int rc)
{
    g_smpd_exit_code = rc;
    g_exit_progress = true;
}


int smpd_progress(ExSetHandle_t set)
{
    if( g_smpd_exit_code == MPI_SUCCESS )
    {
        g_exit_progress = false;

        do
        {
            ExProcessCompletions(set, INFINITE);

        } while( g_exit_progress == false );
    }

    return g_smpd_exit_code;
}

