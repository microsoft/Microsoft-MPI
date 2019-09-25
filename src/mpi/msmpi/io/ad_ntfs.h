// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#ifndef AD_NTFS_INCLUDE
#define AD_NTFS_INCLUDE

#include "adio.h"

#define DWORDLOW(x)          ((DWORD)( x ))
#define DWORDHIGH(x)         ((DWORD)((x) >> 32))

int ADIOI_NTFS_aio(MPI_File fd, void *buf, int len, MPI_Offset offset,
                  int wr, MPI_Request *request);

int ADIOI_NTFS_Init_blocking_overlapped(OVERLAPPED* pov, MPI_Offset offset);

#endif
