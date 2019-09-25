// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef SPAWN_H
#define SPAWN_H

MPI_RESULT
MPID_Open_port(
    _In_opt_                       MPID_Info*,
    _Out_z_cap_(MPI_MAX_PORT_NAME) char *port_name
    );

MPI_RESULT
MPID_Close_port(
    _In_z_ const char *port_name
    );

/*@
   MPID_Comm_accept - MPID entry point for MPI_Comm_accept

   Input Parameters:
+  port_name - port name
.  info - info
.  root - root
-  comm - communicator

   Output Parameters:
.  MPI_Comm *newcomm - new communicator

  Return Value:
  'MPI_SUCCESS' or a valid MPI error code.
@*/
MPI_RESULT
MPID_Comm_accept(
    _In_z_   const char*        port_name,
    _In_opt_ const MPID_Info*   info,
    _In_     int                root,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    );

/*@
   MPID_Comm_connect - MPID entry point for MPI_Comm_connect

   Input Parameters:
+  port_name - port name
.  info - info
.  root - root
-  comm - communicator

   Output Parameters:
.  newcomm_ptr - new intercommunicator

  Return Value:
  'MPI_SUCCESS' or a valid MPI error code.
  @*/
MPI_RESULT
MPID_Comm_connect(
    _In_z_   const char*        port_name,
    _In_opt_ const MPID_Info*   info,
    _In_     int                root,
    _In_     const MPID_Comm*   comm_ptr,
    _Out_    MPID_Comm**        newcomm
    );

MPI_RESULT MPID_Comm_disconnect(MPID_Comm *);

MPI_RESULT
MPID_Comm_spawn_multiple(
    _In_   int              count,
    _In_z_ char*            commands[],
    _In_z_ char**           arguments[],
    _In_   const int*       maxprocs,
    _In_   MPID_Info**      info,
    _In_   int              root,
    _In_   MPID_Comm*       comm,
    _Out_  MPID_Comm**      intercomm,
    _Out_  int *            errcodes
);

MPI_RESULT MPID_Comm_join(_In_ SOCKET fd, _Out_ MPI_Comm *intercomm);

#endif // SPAWN_H
