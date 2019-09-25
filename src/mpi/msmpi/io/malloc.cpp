// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

/* These are routines for allocating and deallocating memory.
   They should be called as ADIOI_Malloc(size) and
   ADIOI_Free(ptr). In adio.h, they are macro-replaced to
   ADIOI_Malloc(size,__LINE__,__FILE__) and
   ADIOI_Free(ptr,__LINE__,__FILE__).

   Later on, add some tracing and error checking, similar to
   MPID_trmalloc. */

/* can't include adio.h here, because of the macro, so
 * include romioconf.h to make sure config-time defines get included */

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <mpi.h>
#include "mpiutil.h"

#pragma warning(push)
#pragma warning(disable:4702)

_Ret_notnull_
void*
ADIOI_Malloc(
    _In_ size_t size
    )
{
    void *new_ptr;

    new_ptr = (void *) malloc(size);

    if (!new_ptr)
    {
        MPID_Abort(NULL, TRUE, 1, "out of memory");
    }

    return new_ptr;
}


_Ret_notnull_
void*
ADIOI_Calloc(
    _In_ size_t nelem,
    _In_ size_t elsize
    )
{
    void *new_ptr;

    new_ptr = (void *) calloc(nelem, elsize);
    if (!new_ptr)
    {
        MPID_Abort(NULL, TRUE, 1, "out of memory");
    }

    return new_ptr;
}


_Ret_notnull_
void*
ADIOI_Realloc(
    _In_ void* ptr,
    _In_ size_t size
    )
{
    void *new_ptr;

    new_ptr = (void *) realloc(ptr, size);
    if (!new_ptr)
    {
        MPID_Abort(NULL, TRUE, 1, "out of memory");
    }
    return new_ptr;
}


void
ADIOI_Free(
    _In_opt_ _Post_ptr_invalid_ void* ptr
    )
{
    free(ptr);
}
#pragma warning(pop)
