// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"


/* status object not filled currently */

/*@
    MPI_File_write - Write using individual file pointer

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write(fh, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write(
        pFile,
        0LL,
        ADIO_INDIVIDUAL,
        buf,
        count,
        hType.GetMpiHandle(),
        status
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_write_all - Collective write using individual file pointer

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_all(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_all(fh, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write_all(
        pFile,
        0LL,
        ADIO_INDIVIDUAL,
        buf,
        count,
        hType.GetMpiHandle(),
        status
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_all(SENTINEL_SAFE_SIZE(status), status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_all, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_write_all_begin - Begin a split collective write using
    individual file pointer

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_all_begin(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_all_begin(fh, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write_all_begin(
        pFile,
        0LL,
        ADIO_INDIVIDUAL,
        buf,
        count,
        hType.GetMpiHandle()
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_all_begin();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_all_begin, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_write_all_end - Complete a split collective write using individual file pointer

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_all_end(
    _In_ MPI_File fh,
    _In_ const void* buf,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_all_end(fh,buf);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write_all_end(pFile, buf, status);

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_all_end(SENTINEL_SAFE_SIZE(status),status);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_all_end, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_write_at - Write using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_at(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_at(fh, offset, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write(
        pFile,
        offset,
        ADIO_EXPLICIT_OFFSET,
        buf,
        count,
        hType.GetMpiHandle(),
        status
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_at(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_at, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_write_at_all - Collective write using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_at_all(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_at_all(fh, offset, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write_all(
        pFile,
        offset,
        ADIO_EXPLICIT_OFFSET,
        buf,
        count,
        hType.GetMpiHandle(),
        status
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_at_all(SENTINEL_SAFE_SIZE(status), status);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_at_all, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_write_at_all_begin - Begin a split collective write using
    explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_at_all_begin(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_at_all_begin(fh, offset, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write_all_begin(
        pFile,
        offset,
        ADIO_EXPLICIT_OFFSET,
        buf,
        count,
        hType.GetMpiHandle()
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_at_all_begin();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_at_all_begin, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_write_at_all_end - Complete a split collective write using explict offset

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)

Output Parameters:
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_at_all_end(
    _In_ MPI_File fh,
    _In_ const void* buf,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_at_all_end(fh,buf);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_write_all_end(pFile, buf, status);

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_at_all_end(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_at_all_end, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_write_ordered - Collective write using shared file pointer

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_ordered(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_ordered(fh, buf, count, datatype);

    int nprocs, myrank;
    int source, dest;
    MPI_Offset shared_fp;

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    size_t datatype_size = static_cast<size_t>( hType.GetSize() );

    if( ((count * datatype_size) % pFile->etype_size) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
        goto fn_fail;
    }

    mpi_errno = MpioOpenDeferred( pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    NMPI_Comm_size(pFile->comm, &nprocs);
    NMPI_Comm_rank(pFile->comm, &myrank);

    size_t incr = (count * datatype_size) / pFile->etype_size;
    /* Use a message as a 'token' to order the operations */
    source = myrank - 1;
    dest   = myrank + 1;
    if( source < 0 )
    {
        source = MPI_PROC_NULL;
    }
    if( dest >= nprocs )
    {
        dest = MPI_PROC_NULL;
    }
    NMPI_Recv(NULL, 0, MPI_BYTE, source, 0, pFile->comm, MPI_STATUS_IGNORE);

    mpi_errno = ADIO_Get_shared_fp(pFile, incr, &shared_fp);
    ON_ERROR_FAIL(mpi_errno);

    NMPI_Send(NULL, 0, MPI_BYTE, dest, 0, pFile->comm);

    mpi_errno = ADIO_WriteStridedColl(
        pFile,
        buf,
        count,
        hType.GetMpiHandle(),
        ADIO_EXPLICIT_OFFSET,
        shared_fp,
        status);

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_ordered(SENTINEL_SAFE_SIZE(status), status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_ordered, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_write_ordered_begin - Begin a split collective write using shared file pointer

Input Parameters:
. fh - file handle (handle)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_ordered_begin(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_ordered_begin(fh, buf, count, datatype);

    int nprocs, myrank;
    int source, dest;
    MPI_Offset shared_fp;

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (pFile->split_coll_count)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_IO,  "**iosplitcoll");
        goto fn_fail;
    }

    pFile->split_coll_count = 1;

    size_t datatype_size = static_cast<size_t>( hType.GetSize() );
    if( ((count * datatype_size) % pFile->etype_size) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
        goto fn_fail;
    }

    mpi_errno = MpioOpenDeferred( pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    NMPI_Comm_size(pFile->comm, &nprocs);
    NMPI_Comm_rank(pFile->comm, &myrank);

    size_t incr = (count * datatype_size) / pFile->etype_size;
    /* Use a message as a 'token' to order the operations */
    source = myrank - 1;
    dest   = myrank + 1;
    if( source < 0 )
    {
        source = MPI_PROC_NULL;
    }
    if( dest >= nprocs )
    {
        dest = MPI_PROC_NULL;
    }
    NMPI_Recv(NULL, 0, MPI_BYTE, source, 0, pFile->comm, MPI_STATUS_IGNORE);

    mpi_errno = ADIO_Get_shared_fp(pFile, incr, &shared_fp);
    ON_ERROR_FAIL(mpi_errno);

    NMPI_Send(NULL, 0, MPI_BYTE, dest, 0, pFile->comm);

    mpi_errno = ADIO_WriteStridedColl(
        pFile,
        buf,
        count,
        hType.GetMpiHandle(),
        ADIO_EXPLICIT_OFFSET,
        shared_fp,
        &pFile->split_status
        );

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }
    TraceLeave_MPI_File_write_ordered_begin();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_ordered_begin, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_write_ordered_end - Complete a split collective write using shared file pointer

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_ordered_end(
    _In_ MPI_File fh,
    _In_ const void* buf,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_ordered_end(fh, buf);
    //
    // fix OACR warning 6101 for status
    //
    OACR_USE_PTR(status);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    if (!(pFile->split_coll_count))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_IO,  "**iosplitcollnone");
        goto fn_fail;
    }

    if (status != MPI_STATUS_IGNORE)
    {
       *status = pFile->split_status;
    }

    pFile->split_coll_count = 0;

    TraceLeave_MPI_File_write_ordered_end(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_ordered_end, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_write_shared - Write using shared file pointer

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_write_shared(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_write_shared(fh, buf, count, datatype);

    int buftype_is_contig, filetype_is_contig;
    MPI_Offset off, shared_fp;

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* NOTE: MPI_STATUS_IGNORE != NULL */
    if( status == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "status" );
        goto fn_fail;
    }

    size_t datatype_size = static_cast<size_t>( hType.GetSize() );
    if (count*datatype_size == 0)
    {
        MPIR_Status_set_bytes(status, hType.GetMpiHandle(), 0);
        mpi_errno = MPI_SUCCESS;
        goto fn_exit;
    }

    if( ((count * datatype_size) % pFile->etype_size) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
        goto fn_fail;
    }

    mpi_errno = MpioOpenDeferred( pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        return mpi_errno;
    }

    ADIOI_Datatype_iscontig(hType.GetMpiHandle(), &buftype_is_contig);
    ADIOI_Datatype_iscontig(pFile->filetype, &filetype_is_contig);

    size_t incr = (count * datatype_size) / pFile->etype_size;

    mpi_errno = ADIO_Get_shared_fp(pFile, incr, &shared_fp);
    ON_ERROR_FAIL(mpi_errno);

    if (buftype_is_contig && filetype_is_contig)
    {
        /* convert bufcount and shared_fp to bytes */
        size_t bufsize = datatype_size * count;
        off = pFile->disp + pFile->etype_size * shared_fp;

        /* if atomic mode requested, lock (exclusive) the region, because there
           could be a concurrent noncontiguous request. On NFS, locking is
           done in the ADIO_WriteContig.*/

        if ((pFile->atomicity))
        {
            ADIOI_WRITE_LOCK(pFile, off, bufsize);
        }

        mpi_errno = ADIO_WriteContig(
            pFile,
            buf,
            count,
            hType.GetMpiHandle(),
            ADIO_EXPLICIT_OFFSET,
            off,
            status
            );

        if ((pFile->atomicity))
        {
            ADIOI_UNLOCK(pFile, off, bufsize);
        }
    }
    else
    {
        /* For strided and atomic mode, locking is done in ADIO_WriteStrided */
        mpi_errno = ADIO_WriteStrided(
            pFile,
            buf,
            count,
            hType.GetMpiHandle(),
            ADIO_EXPLICIT_OFFSET,
            shared_fp,
            status
            );
    }

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_write_shared(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_write_shared, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_iwrite - Nonblocking write using individual file pointer

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. request - request object (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_iwrite(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_iwrite(fh, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_iwrite(
        pFile,
        0LL,
        ADIO_INDIVIDUAL,
        buf,
        count,
        hType.GetMpiHandle(),
        request
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_iwrite( *request );
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_iwrite, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_iwrite_at - Nonblocking write using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. request - request object (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_iwrite_at(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_iwrite_at(fh, offset, buf, count, datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    mpi_errno = MPIOI_File_iwrite(
        pFile,
        offset,
        ADIO_EXPLICIT_OFFSET,
        buf,
        count,
        hType.GetMpiHandle(),
        request
        );

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_iwrite_at(*request);
fn_exit:
    TraceError(MPI_File_iwrite_at, mpi_errno);
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_iwrite_shared - Nonblocking write using shared file pointer

Input Parameters:
. fh - file handle (handle)
. buf - initial address of buffer (choice)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. request - request object (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_iwrite_shared(
    _In_ MPI_File fh,
    _In_opt_ const void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_iwrite_shared(fh, buf, count, datatype);

    int buftype_is_contig, filetype_is_contig;
    MPI_Status status;
    MPI_Offset off, shared_fp;

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        buf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }

    size_t datatype_size = static_cast<size_t>( hType.GetSize() );

    if( ((count * datatype_size) % pFile->etype_size) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_IO, "**ioetype" );
        goto fn_fail;
    }

    mpi_errno = MpioOpenDeferred( pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    ADIOI_Datatype_iscontig(hType.GetMpiHandle(), &buftype_is_contig);
    ADIOI_Datatype_iscontig(pFile->filetype, &filetype_is_contig);

    size_t incr = (count * datatype_size) / pFile->etype_size;
    mpi_errno = ADIO_Get_shared_fp(pFile, incr, &shared_fp);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    /* contiguous or strided? */
    if (buftype_is_contig && filetype_is_contig)
    {
    /* convert sizes to bytes */
        size_t bufsize = datatype_size * count;
        off = pFile->disp + pFile->etype_size * shared_fp;
        if (!(pFile->atomicity))
        {
            mpi_errno = ADIO_IwriteContig(
                pFile,
                buf,
                count,
                hType.GetMpiHandle(),
                ADIO_EXPLICIT_OFFSET,
                off,
                request
                );
        }
        else
        {
            /* to maintain strict atomicity semantics with other concurrent
              operations, lock (exclusive) and call blocking routine */

            ADIOI_WRITE_LOCK(pFile, off, bufsize);

            mpi_errno = ADIO_WriteContig(
                pFile,
                buf,
                count,
                hType.GetMpiHandle(),
                ADIO_EXPLICIT_OFFSET,
                off,
                &status
                );

            ADIOI_UNLOCK(pFile, off, bufsize);

            if(mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }

            MPIO_Completed_request_create(bufsize, request);
        }
    }
    else
    {
        mpi_errno = ADIO_IwriteStrided(
            pFile,
            buf,
            count,
            hType.GetMpiHandle(),
            ADIO_EXPLICIT_OFFSET,
            shared_fp,
            request
            );
    }

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_iwrite_shared(*request);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_iwrite_shared, mpi_errno);
    goto fn_exit;
}
