// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"


MPI_RESULT
MPIOI_File_write(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    const void *buf,
    int count,
    MPI_Datatype datatype,
    MPI_Status *status)
{
    MPI_RESULT mpi_errno;
    int bufsize, buftype_is_contig, filetype_is_contig;
    int datatype_size;
    MPI_Offset off;

    if (file_ptr_type == ADIO_EXPLICIT_OFFSET && offset < 0)
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadoffset");
    }

    NMPI_Type_size(datatype, &datatype_size);
    if (count*datatype_size == 0)
    {
        MPIR_Status_set_bytes(status, datatype, 0);
        return MPI_SUCCESS;
    }

    if( ((count * datatype_size) % fh->etype_size) != 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
    }

    if( (fh->access_mode & MPI_MODE_RDONLY) != 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_READ_ONLY, "**iordonly" );
    }

    if( (fh->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
    }

    mpi_errno = MpioOpenDeferred( fh );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    ADIOI_Datatype_iscontig(datatype, &buftype_is_contig);
    ADIOI_Datatype_iscontig(fh->filetype, &filetype_is_contig);

    if (buftype_is_contig && filetype_is_contig)
    {
        /* convert bufcount and offset to bytes */
        bufsize = datatype_size * count;
        if (file_ptr_type == ADIO_EXPLICIT_OFFSET)
        {
            off = fh->disp + fh->etype_size * offset;
        }
        else /* ADIO_INDIVIDUAL */
        {
            off = fh->fp_ind;
        }

        /* if atomic mode requested, lock (exclusive) the region, because
           there could be a concurrent noncontiguous request. Locking doesn't
           work on PIOFS and PVFS, and on NFS it is done in the
           ADIO_WriteContig.
         */

        if ((fh->atomicity))
        {
            ADIOI_WRITE_LOCK(fh, off, bufsize);
        }

        mpi_errno = ADIO_WriteContig(fh, buf, count, datatype, file_ptr_type,
                     off, status);

        if ((fh->atomicity))
        {
            ADIOI_UNLOCK(fh, off, bufsize);
        }
    }
    else
    {
        /* For strided and atomic mode, locking is done in ADIO_WriteStrided */
        mpi_errno = ADIO_WriteStrided(fh, buf, count, datatype, file_ptr_type,
                          offset, status);
    }

    return mpi_errno;
}


int MPIOI_File_write_all(_In_ MPI_File fh,
                         MPI_Offset offset,
                         int file_ptr_type,
                         const void *buf,
                         int count,
                         MPI_Datatype datatype,
                         MPI_Status *status)
{
    if (file_ptr_type == ADIO_EXPLICIT_OFFSET && offset < 0)
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadoffset");
    }

    int datatype_size;
    int mpi_errno = NMPI_Type_size(datatype, &datatype_size);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( ((count * datatype_size) % fh->etype_size) != 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
    }

    if( (fh->access_mode & MPI_MODE_RDONLY) != 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_READ_ONLY, "**iordonly" );
    }

    if( (fh->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
    }

    return ADIO_WriteStridedColl(
        fh,
        buf,
        count,
        datatype,
        file_ptr_type,
        offset,
        status
        );
}


MPI_RESULT
MPIOI_File_write_all_begin(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    const void *buf,
    int count,
    MPI_Datatype datatype
    )
{

    if( (fh->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
    }

    if (file_ptr_type == ADIO_EXPLICIT_OFFSET && offset < 0)
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadoffset");
    }

    if (fh->split_coll_count)
    {
        return MPIU_ERR_CREATE(MPI_ERR_IO,  "**iosplitcoll");
    }

    fh->split_coll_count = 1;

    int datatype_size;
    MPI_RESULT mpi_errno = NMPI_Type_size(datatype, &datatype_size);
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    if( ((count * datatype_size) % fh->etype_size) != 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
    }

    fh->split_datatype = datatype;
    return ADIO_WriteStridedColl(
        fh,
        buf,
        count,
        datatype,
        file_ptr_type,
        offset,
        &fh->split_status
        );
}


MPI_RESULT
MPIOI_File_write_all_end(
    _In_ MPI_File fh,
    const void* /*buf*/,
    MPI_Status *status)
{
    if (!(fh->split_coll_count))
    {
        return MPIU_ERR_CREATE(MPI_ERR_IO,  "**iosplitcollnone");
    }

    /* FIXME - we should really ensure that the split_datatype remains
       valid by incrementing the ref count in the write_allb.c routine
       and decrement it here after setting the bytes */
    if (status != MPI_STATUS_IGNORE)
    {
       *status = fh->split_status;
    }

    fh->split_coll_count = 0;

    return MPI_SUCCESS;
}


MPI_RESULT
MPIOI_File_iwrite(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_ int file_ptr_type,
    _In_reads_bytes_opt_(count) const void *buf,
    _In_ int count,
    _In_ MPI_Datatype datatype,
    _Inout_ MPI_Request *request)
{
    MPI_RESULT mpi_errno;
    int bufsize, buftype_is_contig, filetype_is_contig;
    int datatype_size;
    MPI_Status status;
    MPI_Offset off;
    MPI_Offset nbytes=0;

    if (file_ptr_type == ADIO_EXPLICIT_OFFSET && offset < 0)
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadoffset");
    }

    NMPI_Type_size(datatype, &datatype_size);

    if( ((count * datatype_size) % fh->etype_size) != 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
    }

    if( (fh->access_mode & MPI_MODE_RDONLY) != 0 )
    {
        return MPIU_ERR_CREATE( MPI_ERR_READ_ONLY, "**iordonly" );
    }

    if( (fh->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
    }

    mpi_errno = MpioOpenDeferred( fh );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    ADIOI_Datatype_iscontig(datatype, &buftype_is_contig);
    ADIOI_Datatype_iscontig(fh->filetype, &filetype_is_contig);

    if (buftype_is_contig && filetype_is_contig)
    {
        /* convert sizes to bytes */
        bufsize = datatype_size * count;
        if (file_ptr_type == ADIO_EXPLICIT_OFFSET)
        {
            off = fh->disp + fh->etype_size * offset;
        }
        else
        {
            off = fh->fp_ind;
        }

        if (!(fh->atomicity))
        {
            mpi_errno = ADIO_IwriteContig(
                fh,
                buf,
                count,
                datatype,
                file_ptr_type,
                off,
                request
                );
        }
        else
        {
            /* to maintain strict atomicity semantics with other concurrent
              operations, lock (exclusive) and call blocking routine */
            ADIOI_WRITE_LOCK(fh, off, bufsize);

            mpi_errno = ADIO_WriteContig(
                fh,
                buf,
                count,
                datatype,
                file_ptr_type,
                off,
                &status
                );

            ADIOI_UNLOCK(fh, off, bufsize);

            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }

            nbytes = count * datatype_size;
            MPIO_Completed_request_create(nbytes, request);
        }
    }
    else
    {
        mpi_errno = ADIO_IwriteStrided(
            fh,
            buf,
            count,
            datatype,
            file_ptr_type,
            offset,
            request
            );
    }

    return mpi_errno;
}
