// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

/* Handle different mechanisms for passing Fortran CHARACTER to routines */
#ifdef USE_FORT_MIXED_STR_LEN
#define FORT_MIXED_LEN_DECL   , MPI_Fint
#define FORT_END_LEN_DECL
#define FORT_MIXED_LEN(a)     , MPI_Fint a
#define FORT_END_LEN(a)
#else
#define FORT_MIXED_LEN_DECL
#define FORT_END_LEN_DECL     , MPI_Fint
#define FORT_MIXED_LEN(a)
#define FORT_END_LEN(a)       , MPI_Fint a
#endif


/* Support different calling convention */
#ifdef USE_FORT_CDECL
#define FORT_CALL __cdecl
#elif defined (USE_FORT_STDCALL)
#define FORT_CALL __stdcall
#else
#define FORT_CALL
#endif

#define FORT_IMPORT __declspec(dllimport)
#define MSMPI_FORT_CALL  __cdecl

/* ------------------------------------------------------------------------- */

/* MPI_FAint is used as the C type corresponding to the Fortran type
   used for addresses.  For now, we make this the same as MPI_Aint.
   Note that since this is defined only for this private include file,
   we can get away with calling MPI_xxx */
typedef MPI_Aint MPI_FAint;

