// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

/* main include file for ADIO.
   contains general definitions, declarations, and macros independent 
   of the underlying file system */

/* Functions and datataypes that are "internal" to the ADIO implementation 
   prefixed ADIOI_. Functions and datatypes that are part of the
   "externally visible" (documented) ADIO interface are prefixed ADIO_.

   An implementation of MPI-IO, or any other high-level interface, should
   not need to use any of the ADIOI_ functions/datatypes. 
   Only someone implementing ADIO on a new file system, or modifying 
   an existing ADIO implementation, would need to use the ADIOI_
   functions/datatypes. */

#ifndef ADIO_INCLUDE
#define ADIO_INCLUDE


#define FDTYPE HANDLE

#ifndef MPI_OFFSET
/* The standard has defined MPI_OFFSET in mpi 2.2; not in our mpi header yet. */
#define MPI_OFFSET MPI_DOUBLE
#endif


struct ADIOI_Hints;

struct ADIOI_FileD
{
    FDTYPE fd_sys;              /* system file descriptor */

    MPI_Offset fp_ind;      /* individual file pointer in MPI-IO (in bytes)*/
    MPI_Comm comm;           /* communicator indicating who called open */
    MPI_Comm agg_comm;      /* deferred open: aggregators who called open */
    int is_open;            /* deferred open: 0: not open yet 1: is open */
    const wchar_t* filename;
    int access_mode;         /* Access mode (sequential, append, etc.) */
    MPI_Offset disp;        /* reqd. for MPI-IO */
    MPI_Datatype etype;      /* reqd. for MPI-IO */
    MPI_Datatype filetype;   /* reqd. for MPI-IO */
    int etype_size;          /* in bytes */
    ADIOI_Hints* hints;      /* structure containing fs-indep. info values */
    MPI_Info info;

    /* The following support the split collective operations */
    int split_coll_count;    /* count of outstanding split coll. ops. */
    MPI_Status split_status; /* status used for split collectives */
    MPI_Datatype split_datatype; /* datatype used for split collectives */

    /* The following support the shared file operations */
    wchar_t* shared_fp_fname;   /* name of file containing shared file pointer */
    struct ADIOI_FileD* shared_fp_fd;  /* file handle of file 
                                         containing shared fp */
    int atomicity;          /* true=atomic, false=nonatomic */
    int fortran_handle;     /* handle for Fortran interface if needed */
    MPI_Errhandler err_handler;
};


/* fcntl structure */
struct ADIO_Fcntl_t
{
    int atomicity;
    MPI_Offset fsize;       /* for get_fsize only */
    MPI_Offset diskspace;   /* for file preallocation */
};


/* file-pointer types */
#define ADIO_EXPLICIT_OFFSET     100
#define ADIO_INDIVIDUAL          101

#define ADIO_FCNTL_SET_ATOMICITY 180
#define ADIO_FCNTL_SET_DISKSPACE 188
#define ADIO_FCNTL_GET_FSIZE     200

/* ADIO function prototypes */
/* all these may not be necessary, as many of them are macro replaced to 
   function pointers that point to the appropriate functions for each
   different file system (in adioi.h), but anyway... */


int
ADIO_Open(
    MPI_Comm orig_comm,
    MPI_Comm comm,
    const wchar_t* filename,
    int access_mode,
    MPI_Offset disp,
    MPI_Datatype etype,
    MPI_Datatype filetype,
    MPI_Info info,
    MPI_File* pfd
    );

int
ADIO_ImmediateOpen(
    MPI_File fd
    );

void
ADIO_Close(
    MPI_File fd
    );

int
ADIO_Get_shared_fp(
    MPI_File fd,
    size_t size,
    MPI_Offset* shared_fp
    );

int
ADIO_Set_shared_fp(
    MPI_File fd,
    MPI_Offset offset
    );

int
ADIO_Set_view(
    MPI_File fd,
    MPI_Offset disp,
    MPI_Datatype etype,
    MPI_Datatype filetype,
    MPI_Info info
    );

/* functions to help deal with the array datatypes */
int
ADIO_Type_create_subarray(
    _In_ int ndims,
    _In_reads_(ndims) int* array_of_sizes,
    _In_reads_(ndims) int* array_of_subsizes,
    _In_reads_(ndims) const int* array_of_starts,
    _In_ int order,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    );

int
ADIO_Type_create_darray(
    _In_ int size,
    _In_ int rank,
    _In_ int ndims,
    _In_reads_(ndims) const int* array_of_gsizes,
    _In_reads_(ndims) const int* array_of_distribs,
    _In_reads_(ndims) int* array_of_dargs,
    _In_reads_(ndims) int* array_of_psizes,
    _In_ int order,
    _In_ MPI_Datatype oldtype,
    _Out_ MPI_Datatype* newtype
    );

/* MPI_File management functions (in mpio_file.c) */
MPI_File
MPIO_File_resolve(
    MPI_File mpi_fh
    );

MPI_File
MPIO_File_f2c(
    MPI_Fint fh
    );

MPI_Fint
MPIO_File_c2f(
    MPI_File fh
    );

/* request managment helper functions */
void
MPIO_Completed_request_create(
    MPI_Offset nbytes,
    MPI_Request* request
    );

char * AllocateString(_In_count_(len) const char *buf, size_t len);

#include "adioi.h"
#include "adioi_fs_proto.h"

#endif
