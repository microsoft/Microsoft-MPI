// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef MPIMEM_H_INCLUDED
#define MPIMEM_H_INCLUDED

/* ------------------------------------------------------------------------- */
/* mpimem.h */
/* ------------------------------------------------------------------------- */

/*D
  Memory - Memory Management Routines

  Rules for memory management:

  MPICH explicity prohibits the appearence of 'malloc', 'free',
  'calloc', 'realloc', or 'strdup' in any code implementing a device or
  MPI call (of course, users may use any of these calls in their code).
  Instead, you must use 'MPIU_Malloc' etc.; if these are defined
  as 'malloc', that is allowed, but an explicit use of 'malloc' instead of
  'MPIU_Malloc' in the source code is not allowed.  This restriction is
  made to simplify the use of portable tools to test for memory leaks,
  overwrites, and other consistency checks.

  Most memory should be allocated at the time that 'MPID_Init' is
  called and released with 'MPID_Finalize' is called.  If at all possible,
  no other MPID routine should fail because memory could not be allocated
  (for example, because the user has allocated large arrays after 'MPI_Init').

  The implementation of the MPI routines will strive to avoid memory allocation
  as well; however, operations such as 'MPI_Type_index' that create a new
  data type that reflects data that must be copied from an array of arbitrary
  size will have to allocate memory (and can fail; note that there is an
  MPI error class for out-of-memory).

  Question:
  Do we want to have an aligned allocation routine?  E.g., one that
  aligns memory on a cache-line.
  D*/

/* ------------------------------------------------------------------------- */

/* No memory tracing; just use native functions */
void* MPIU_Malloc( _In_ SIZE_T size );

void* MPIU_Calloc( _In_ SIZE_T elements, _In_ SIZE_T size );

void MPIU_Free( _In_opt_ _Post_ptr_invalid_ void* pMem );

void* MPIU_Realloc( _In_ void* pMem, _In_ SIZE_T size );

#ifdef __cplusplus

//
// C++ operator new/delete overload.
//
// Normal operator new: pInt = new int;
//
void* __cdecl operator new( size_t size );

//
// Placement new: pInt = new( ptr ) int;
//
void* __cdecl operator new( size_t /*size*/, _In_/*count_(size)*/ void* pMem );

//
// Array new: pInt = new int[10];
//
void* __cdecl operator new[]( size_t size );

//
// Placement array new: pInt = new( ptr ) int[10];
//
void* __cdecl operator new[]( size_t /*size*/, _In_/*count_(size)*/ void* pMem );

//
// Normal operator delete: delete pInt;
//
void __cdecl operator delete( _In_opt_ _Post_ptr_invalid_ void* pObj );

//
// Array delete: delete[] pInt;
//
void __cdecl operator delete[]( _In_opt_ _Post_ptr_invalid_ void* pObj );

#endif  // __cplusplus


#define MPIU_Malloc_obj(type_) \
        (type_*)MPIU_Malloc(sizeof(type_))

#define MPIU_Malloc_objn(count_, type_) \
        (type_*)MPIU_Malloc((count_)*sizeof(type_))

/* ------------------------------------------------------------------------- */
/* end of mpimem.h */
/* ------------------------------------------------------------------------- */

#endif
