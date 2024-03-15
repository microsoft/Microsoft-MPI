// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef ROMIO_H
#define ROMIO_H

int ADIO_Init();
void ADIO_Finalize();

MPI_Errhandler MPIR_ROMIO_Get_file_errhand( _In_opt_ struct ADIOI_FileD const *file_ptr );

void MPIR_ROMIO_Set_file_errhand( _In_opt_ MPI_File file_ptr, _In_ MPI_Errhandler e );

_Post_equals_last_error_
int MPIR_Err_return_file_helper(MPI_Errhandler eh, MPI_File file, int errcode);

_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIO_Err_return_file( MPI_File, int );

#endif // ROMIO_H
