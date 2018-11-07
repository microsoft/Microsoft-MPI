// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"


/* The following function selects the name of the file to be used to
   store the shared file pointer. The shared-file-pointer file is a
   hidden file in the same directory as the real file being accessed.
   If the real file is /tmp/thakur/testfile, the shared-file-pointer
   file will be /tmp/thakur/testfile.shfp.xxxx, where xxxx is
   a UUID. This file is created only if the shared
   file pointer functions are used and is deleted when the real
   file is closed. */

#define UUID_STRING_LEN 36

int ADIOI_Shfp_fname(MPI_File fd, int rank)
{
    size_t wlen;
    HRESULT hr = StringCchLengthW( fd->filename,
                                   STRSAFE_MAX_CCH,
                                   &wlen );
    if( FAILED(hr) )
    {
        return MPIU_ERR_CREATE(MPI_ERR_FILE,"**filename");
    }

    wlen += (_countof(L".shfp.") - 1) + UUID_STRING_LEN + 1;
    if( wlen > INT_MAX )
    {
        return MPIU_ERR_CREATE(MPI_ERR_FILE,"**filename");
    }

    int len = static_cast<int>(wlen);

    fd->shared_fp_fname = new wchar_t[wlen];
    if( fd->shared_fp_fname == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    int mpi_errno;
    if (!rank)
    {
        UUID id;
        RPC_STATUS status = UuidCreate( &id );
        if( status != RPC_S_OK )
        {
            mpi_errno = MPIO_ERR_GLE( status );
            goto fn_fail;
        }

        RPC_WSTR strId;
        if( UuidToStringW( &id, &strId ) != RPC_S_OK )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        hr = StringCchPrintfW(
            fd->shared_fp_fname,
            wlen,
            L"%s.shfp.%s",
            fd->filename,
            strId
            );

        RpcStringFreeW( &strId );

        if( FAILED( hr ) )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
    }

    mpi_errno = NMPI_Bcast(&len, 1, MPI_INT, 0, fd->comm);
    if( mpi_errno != MPI_SUCCESS )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    mpi_errno = NMPI_Bcast(fd->shared_fp_fname, len, MPI_WCHAR, 0, fd->comm);
    if( mpi_errno != MPI_SUCCESS )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

fn_exit:
    return mpi_errno;

fn_fail:
    delete[] fd->shared_fp_fname;
    fd->shared_fp_fname = NULL;
    goto fn_exit;
}


/* returns the current location of the shared_fp in terms of the
   no. of etypes relative to the current view, and also increments the
   shared_fp by the number of etypes to be accessed (incr) in the read
   or write following this function. */

void ADIOI_NFS_Get_shared_fp(MPI_File fd, int incr, MPI_Offset *shared_fp,
                             int *mpi_errno);

int ADIO_Get_shared_fp(MPI_File fd, size_t incr, MPI_Offset *shared_fp)
{
    MPI_Status status;
    MPI_Offset new_fp;
    MPI_Comm dupcommself;
    int mpi_errno;

    if (fd->shared_fp_fd == MPI_FILE_NULL)
    {
        MPIU_Assert( fd->shared_fp_fname != NULL );

        NMPI_Comm_dup(MPI_COMM_SELF, &dupcommself);
        mpi_errno = ADIO_Open(
            MPI_COMM_SELF,
            dupcommself,
            fd->shared_fp_fname,
            MPI_MODE_CREATE | MPI_MODE_RDWR | MPI_MODE_DELETE_ON_CLOSE | MSMPI_MODE_HIDDEN,
            0,          // disp
            MPI_BYTE,   // etype
            MPI_BYTE,   // filetype
            MPI_INFO_NULL,
            &fd->shared_fp_fd
            );

        fd->shared_fp_fname = NULL;

        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        *shared_fp = 0;
        ADIOI_WRITE_LOCK(fd->shared_fp_fd, 0, sizeof(MPI_Offset));
retry:
        mpi_errno = ADIO_ReadContig(fd->shared_fp_fd, shared_fp, sizeof(MPI_Offset),
                       MPI_BYTE, ADIO_EXPLICIT_OFFSET, 0, &status);
        //
        // jvert 3/34/2006
        //    There looks to be a bug in the remote file locking. Sometimes
        //    the above read file returns ERROR_LOCK_VIOLATION even though
        //    we have already locked the range we are reading. This seems
        //    to be some kind of race condition as retrying fixes the problem.
        //      N.B. the comment below is WRONG - EOF errors are actually
        //           silently swallowed by ADIOI_NTFS_ReadContig. This seems
        //           pretty dubious to me.
        //
        if (mpi_errno != MPI_SUCCESS)
        {
            Sleep(10);
            goto retry;
        }
        /* if the file is empty, the above function may return error
           (reading beyond end of file). In that case, shared_fp = 0,
           set above, is the correct value. */
    }
    else
    {
        ADIOI_WRITE_LOCK(fd->shared_fp_fd, 0, sizeof(MPI_Offset));
        mpi_errno = ADIO_ReadContig(fd->shared_fp_fd, shared_fp, sizeof(MPI_Offset),
                       MPI_BYTE, ADIO_EXPLICIT_OFFSET, 0, &status);
        if (mpi_errno != MPI_SUCCESS)
        {
            ADIOI_UNLOCK(fd->shared_fp_fd, 0, sizeof(MPI_Offset));
            return mpi_errno;
        }
    }

    new_fp = *shared_fp + incr;

    mpi_errno = ADIO_WriteContig(
                        fd->shared_fp_fd,
                        &new_fp,
                        sizeof(MPI_Offset),
                        MPI_BYTE,
                        ADIO_EXPLICIT_OFFSET,
                        0,
                        &status
                        );

    ADIOI_UNLOCK(fd->shared_fp_fd, 0, sizeof(MPI_Offset));
    return mpi_errno;
}


/* set the shared file pointer to "offset" etypes relative to the current
   view */

int ADIO_Set_shared_fp(MPI_File fd, MPI_Offset offset)
{
    MPI_Status status;
    MPI_Comm dupcommself;
    int mpi_errno;

    if (fd->shared_fp_fd == MPI_FILE_NULL)
    {
        MPIU_Assert( fd->shared_fp_fname != NULL );

        NMPI_Comm_dup(MPI_COMM_SELF, &dupcommself);
        mpi_errno = ADIO_Open(
                        MPI_COMM_SELF,
                        dupcommself,
                        fd->shared_fp_fname,
                        MPI_MODE_CREATE | MPI_MODE_RDWR | MPI_MODE_DELETE_ON_CLOSE,
                        0,          // disp
                        MPI_BYTE,   // etype
                        MPI_BYTE,   // filetype
                        MPI_INFO_NULL,
                        &fd->shared_fp_fd
                        );

        fd->shared_fp_fname = NULL;

        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }

    ADIOI_WRITE_LOCK(fd->shared_fp_fd, 0, sizeof(MPI_Offset));

    mpi_errno = ADIO_WriteContig(
                    fd->shared_fp_fd,
                    &offset,
                    sizeof(MPI_Offset),
                    MPI_BYTE,
                    ADIO_EXPLICIT_OFFSET,
                    0,
                    &status
                    );

    ADIOI_UNLOCK(fd->shared_fp_fd, 0, sizeof(MPI_Offset));
    return mpi_errno;
}

