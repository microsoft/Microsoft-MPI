// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "mpidimpl.h"

#pragma once

#ifndef CH3_COMPRESSION
#define CH3_COMPRESSION


//
//Messages that are larger than g_CompressionThreshold in bytes cause an
//attempt at compression.  This is set to MSMPI_COMPRESSION_OFF when
//compression should never be attempted.
//
extern int g_CompressionThreshold;

//
//Initialize the compression library.
//returns
//  MPI_ERR_OTHER: Indicates that the library containing the compression
//                 routine could not be found.
//  MPI_ERR_NO_MEM: Indicates that the compression buffers could not
//                  be allocated.
//  MPI_SUCCESS: Indicates the library was successfully initialized.
//
int InitializeCompression();

//
//Handle a receive request completing.
//parameters
//  pReq: A pointer to the request containing a compressed payload.
//
int DecompressRequest(_Inout_ MPID_Request *pReq);

#define MSMPI_COMPRESSION_OFF -1

//Compress the buffer associated with an outgoing send request.
//parameters
//  pReq: A pointer to the request that is being sent.
//
int CompressRequest(_Inout_ MPID_Request *pReq);

#endif //CH3_COMPRESSION