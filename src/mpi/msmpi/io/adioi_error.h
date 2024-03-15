// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


/* MPIO_CHECK_XXX macros are used to clean up error checking and
 * handling in many of the romio/mpi-io/ source files.
 */
#define MPIO_CHECK_FILE_HANDLE(fh, mpi_errno)                  \
if ((fh <= (MPI_File) 0)) {                      \
    mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadfh");     \
    (fh) = MPI_FILE_NULL;                                       \
    goto fn_fail;                                               \
}

/* TODO: handle the independent io case more gracefully  */
__forceinline
MPI_RESULT
MpioOpenDeferred( ADIOI_FileD* file )
{
    if( file->is_open == FALSE )
    {
        return ADIO_ImmediateOpen( file );
    }
    return MPI_SUCCESS;
}
#define MPIO_OPEN_DEFERRED(fh, mpi_errno)                      \
{mpi_errno = MpioOpenDeferred( fh );                            \
    if(mpi_errno)                                              \
        goto fn_fail;                                          \
}
/* MPIO_ERR_CREATE_CODE_XXX macros are used to clean up creation of
 * error codes for common cases in romio/adio/
 */
#define MPIO_ERR_CREATE_CODE_INFO_NOT_SAME(key)   \
    MPIU_ERR_CREATE(MPI_ERR_NOT_SAME, "**ioinfokey %s", key);

#define MPIO_ERR_GLE(gle_) \
    MPIU_ERR_CREATE(MPI_ERR_IO, "**io %s", get_error_string(gle_))


