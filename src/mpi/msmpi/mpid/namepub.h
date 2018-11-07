// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef NAMEPUB_H_INCLUDED
#define NAMEPUB_H_INCLUDED

struct MPID_NS_t { int dummy; };    /* unused for now */

MPI_RESULT MPID_NS_Create( const MPID_Info *, MPID_NS_t ** );
MPI_RESULT MPID_NS_Publish( const MPID_NS_t *, const MPID_Info *,
                     const char service_name[], const char port[] );
MPI_RESULT MPID_NS_Lookup( _In_opt_ const MPID_NS_t *, _In_opt_ const MPID_Info *,
                    _In_z_ const char service_name[], _Out_writes_z_ ( MPI_MAX_PORT_NAME ) char port[] );
MPI_RESULT MPID_NS_Unpublish( const MPID_NS_t *, const MPID_Info *,
                       const char service_name[] );
MPI_RESULT MPID_NS_Free( MPID_NS_t ** );

extern MPID_NS_t * MPIR_Namepub;

#endif
