// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef MPIERRS_H_INCLUDED
#define MPIERRS_H_INCLUDED
/* ------------------------------------------------------------------------- */
/* mpierrs.h */
/* ------------------------------------------------------------------------- */

#define FCNAME __FUNCTION__


struct MPID_Comm;
struct MPID_Win;
/*struct MPID_File;*/

/* Bindings for internal routines */
_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIR_Err_return_comm(
    _In_opt_ MPID_Comm* comm_ptr,
    _In_ PCSTR      fcname,
    _In_ MPI_RESULT errcode
    );


_Post_satisfies_( return != MPI_SUCCESS )
MPI_RESULT
MPIR_Err_return_win(
    _In_opt_ MPID_Win*  win_ptr,
    _In_ PCSTR      fcname,
    _In_ MPI_RESULT errcode
    );

/*int MPIR_Err_return_file( struct MPID_File *, const char [], int );*/
#ifdef MPI__FILE_DEFINED
/* Only define if we have MPI_File */
_Post_equals_last_error_
int MPIR_Err_return_file( MPI_File, const char [], int ); /* Romio version */
#endif


/* Valid pointer checks */
/* This test is lame.  Should eventually include cookie test
   and in-range addresses */
#define MPID_Valid_ptr_class(kind,ptr,errclass,err) \
{                                                   \
    if (!(ptr))                                     \
    {                                               \
        err = MPIU_ERR_CREATE(errclass, "**nullptrtype %s", #kind ); \
        __analysis_assume(err != MPI_SUCCESS);                       \
    }                                                                \
    else                                                             \
    {                                                                \
        __analysis_assume(ptr != NULL);                              \
    }                                                                \
}

#define MPID_Win_valid_ptr(ptr,err) MPID_Valid_ptr_class(Win,ptr,MPI_ERR_WIN,err)
#define MPID_Op_valid_ptr(ptr,err) MPID_Valid_ptr_class(Op,ptr,MPI_ERR_OP,err)

/* ------------------------------------------------------------------------- */
/* end of mpierrs.h */
/* ------------------------------------------------------------------------- */

#endif
