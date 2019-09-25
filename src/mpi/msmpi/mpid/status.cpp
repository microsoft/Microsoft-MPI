// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/* Not quite correct, but much closer for MPI2 */
/* TODO: still needs to handle partial datatypes and situations where the mpi
 * implementation fills status with something other than bytes (globus2 might
 * do this) */
void MPIR_Status_set_bytes(MPI_Status *status, MPI_Datatype /*datatype*/,
                          int nbytes)
{
    /* it's ok that ROMIO stores number-of-bytes in status, not
     * count-of-copies, as long as MPI_GET_COUNT knows what to do */
    if (status != MPI_STATUS_IGNORE)
        NMPI_Status_set_elements(status, MPI_BYTE, nbytes);
}
