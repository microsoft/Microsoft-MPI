// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef ERRHAN_H
#define ERRHAN_H


/* Error Handlers */

/*E
  MPID_XXX_Errhandler_fn - MPID Structures to hold an error handler function

  Notes:
  The MPI-1 Standard declared only the C version of this, implicitly
  assuming that 'int' and 'MPI_Fint' were the same.

  Since Fortran does not have a C-style variable number of arguments
  interface, the Fortran interface simply accepts two arguments.  Some
  calling conventions for Fortran (particularly under Windows) require
  this.

  Module:
  ErrHand-DS

  Questions:
  What do we want to do about C++?  Do we want a hook for a routine that can
  be called to throw an exception in C++, particularly if we give C++ access
  to this structure?  Does the C++ handler need to be different (not part
  of the union)?

  E*/
struct MPID_Comm_errhandler_fn
{
    MPI_Comm_errhandler_fn* user_function;
    MPID_Comm_errhandler_proxy* proxy;
};


struct MPID_File_errhandler_fn
{
    MPI_File_errhandler_fn* user_function;
    MPID_File_errhandler_proxy* proxy;
};


struct MPID_Win_errhandler_fn
{
    MPI_Win_errhandler_fn* user_function;
    MPID_Win_errhandler_proxy* proxy;
};


union MPID_Errhandler_fn
{
    MPID_Comm_errhandler_fn comm;
    MPID_File_errhandler_fn file;
    MPID_Win_errhandler_fn win;
};

/*S
  MPID_Errhandler - Description of the error handler structure

  Notes:
  Device-specific information may indicate whether the error handler is active;
  this can help prevent infinite recursion in error handlers caused by
  user-error without requiring the user to be as careful.  We might want to
  make this part of the interface so that the 'MPI_xxx_call_errhandler'
  routines would check.

  It is useful to have a way to indicate that the errhandler is no longer
  valid, to help catch the case where the user has freed the errhandler but
  is still using a copy of the 'MPI_Errhandler' value.  We may want to
  define the 'id' value for deleted errhandlers.

  Module:
  ErrHand-DS
  S*/
struct MPID_Errhandler
{
  int                handle;
  volatile long      ref_count;
  MPID_Object_kind   kind;
  MPID_Errhandler_fn errfn;
};

extern MPIU_Object_alloc_t MPID_Errhandler_mem;
/* Preallocated errhandler objects */
extern MPID_Errhandler MPID_Errhandler_builtin[];
extern MPID_Errhandler MPID_Errhandler_direct[];

#define MPIR_Errhandler_add_ref( _errhand ) \
    MPIU_Object_add_ref( _errhand )

#define MPIR_Errhandler_release_ref( _errhand, _inuse ) \
     MPIU_Object_release_ref( _errhand, _inuse )


void MPID_Errhandler_free(MPID_Errhandler *errhan_ptr);


#define MPIR_Nest_init()

#define MPIR_Nested_call() (Mpi.CallState->nest_count > 1)

void MPIAPI MPIR_Comm_errhandler_c_proxy(MPI_Comm_errhandler_fn* fn, MPI_Comm* comm, int* errcode, ...);
void MPIAPI MPIR_Win_errhandler_c_proxy(MPI_Win_errhandler_fn* fn, MPI_Win* win, int* errcode, ...);
void MPIAPI MPIR_File_errhandler_c_proxy(MPI_File_errhandler_fn* fn, MPI_File* file, int* errcode, ...);

#endif // ERRHAN_H
