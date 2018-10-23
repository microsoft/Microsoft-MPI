// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_err.h - Network Direct MPI CH3 Channel error macros

--*/

#pragma once


/* --BEGIN ERROR MACROS-- */

#define MPIU_E_ERR( fmt_, ... ) \
    MPIU_ERR_FATAL_GET( MPI_SUCCESS, MPI_ERR_OTHER, fmt_, __VA_ARGS__ )

/* --END ERROR MACROS-- */
