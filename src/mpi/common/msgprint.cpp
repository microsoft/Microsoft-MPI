// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include <stdio.h>
#include <stdlib.h>

_Success_(return>=0)
int
MPIU_Error_printf(
    _Printf_format_string_ const char *str,
    ...
    )
{
    int n;
    va_list list;

    va_start(list, str);
    n = vfprintf(stderr, str, list);
    va_end(list);

    fflush(stderr);

    return n;
}

_Success_(return>=0)
int
MPIU_Internal_error_printf(
    _Printf_format_string_ const char *str,
    ...
    )
{
    int n;
    va_list list;

    va_start(list, str);
    n = vfprintf(stderr, str, list);
    va_end(list);

    fflush(stderr);

    return n;
}

