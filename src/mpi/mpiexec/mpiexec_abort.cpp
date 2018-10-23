// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "smpd.h"


void smpd_post_abort_command(const wchar_t *fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    fwprintf(stderr, L"\nAborting: ");
    vfwprintf(stderr, fmt, list);
    fflush(stderr);
    va_end(list);

    smpd_signal_exit_progress(MPI_ERR_INTERN);
}


void smpd_kill_all_processes( void )
{
}
