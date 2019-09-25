// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"
#include "adioi.h" /* ADIOI_Get_byte_offset() prototype */
#include "adio_extern.h"


/* These next two routines are used to allow MPICH2 to access/set the
   error handers in the MPI_File structure until MPICH2 knows about the
   file structure, and to handle the errhandler structure, which
   includes a reference count.  Not currently used. */
void MPIR_ROMIO_Set_file_errhand( _In_opt_ MPI_File file_ptr, _In_ MPI_Errhandler e )
{
    if (file_ptr == MPI_FILE_NULL)
    {
        ADIOI_DFLT_ERR_HANDLER = e;
    }
    else
    {
        file_ptr->err_handler = e;
    }
}


MPI_Errhandler MPIR_ROMIO_Get_file_errhand( _In_opt_ struct ADIOI_FileD const *file_ptr )
{
    if (file_ptr == MPI_FILE_NULL)
    {
        return ADIOI_DFLT_ERR_HANDLER;
    }

    return file_ptr->err_handler;
}


MPI_File MPIO_File_f2c(MPI_Fint fh)
{
    if (!fh) return MPI_FILE_NULL;
    if ((fh < 0) || (fh > ADIOI_Ftable_ptr))
    {
        /* there is no way to return an error from MPI_File_f2c */
        return MPI_FILE_NULL;
    }
    return ADIOI_Ftable[fh];
}


MPI_Fint MPIO_File_c2f(MPI_File fh)
{
    int i;

    if ((fh <= (MPI_File) 0))
    {
        return (MPI_Fint) 0;
    }

    if (fh->fortran_handle != -1)
    {
        return fh->fortran_handle;
    }

    if (!ADIOI_Ftable)
    {
        ADIOI_Ftable_max = 1024;
        ADIOI_Ftable = (MPI_File *)
            ADIOI_Malloc(ADIOI_Ftable_max*sizeof(MPI_File));
        ADIOI_Ftable_ptr = 0;  /* 0 can't be used though, because
                                  MPI_FILE_NULL=0 */
        for (i=0; i<ADIOI_Ftable_max; i++) ADIOI_Ftable[i] = MPI_FILE_NULL;
    }
    if (ADIOI_Ftable_ptr == ADIOI_Ftable_max-1)
    {
        ADIOI_Ftable = (MPI_File *) ADIOI_Realloc(ADIOI_Ftable,
                           (ADIOI_Ftable_max+1024)*sizeof(MPI_File));
        for (i=ADIOI_Ftable_max; i<ADIOI_Ftable_max+1024; i++)
            ADIOI_Ftable[i] = MPI_FILE_NULL;
        ADIOI_Ftable_max += 1024;
    }
    ADIOI_Ftable_ptr++;
    ADIOI_Ftable[ADIOI_Ftable_ptr] = fh;
    fh->fortran_handle = ADIOI_Ftable_ptr;
    return (MPI_Fint) ADIOI_Ftable_ptr;
}


static MPI_RESULT MPIU_Greq_query_fn(void* extra_state, MPI_Status* status)
{
    OACR_USE_PTR( extra_state );
    MPIR_Status_set_empty(status);
    MPIR_Status_set_bytes(status, MPI_BYTE, MPIU_PtrToInt(extra_state));

    return MPI_SUCCESS;
}


static MPI_RESULT MPIU_Greq_free_fn(void* /*extra_state*/)
{
    return MPI_SUCCESS;
}


static MPI_RESULT MPIU_Greq_cancel_fn(void* /*extra_state*/, int /*complete*/)
{
    /* can't cancel */
    return MPI_SUCCESS;
}

/* In cases where nonblocking operation will carry out blocking version,
 * instantiate and complete a generalized request  */

void
MPIO_Completed_request_create(
    MPI_Offset bytes,
    MPI_Request* request
    )
{
    //
    // FIXME: Potentially unsafe cast from MPI_Offset to int.
    //

    NMPI_Grequest_start(
        MPIU_Greq_query_fn,
        MPIU_Greq_free_fn,
        MPIU_Greq_cancel_fn,
        MPIU_IntToPtr((int)bytes),
        request
        );

    NMPI_Grequest_complete(*request);
}
