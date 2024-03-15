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
    MPI_File_read - Read using individual file pointer

Input Parameters:
. fh - file handle (handle)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read(fh, buf, count, datatype);

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

    mpi_errno = MPIOI_File_read(
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

    TraceLeave_MPI_File_read(SENTINEL_SAFE_SIZE(status), status);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_read_all - Collective read using individual file pointer

Input Parameters:
. fh - file handle (handle)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_all(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_all(fh, buf, count, datatype);

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

    mpi_errno = MPIOI_File_read_all(
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

    TraceLeave_MPI_File_read_all(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_all, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_read_all_begin - Begin a split collective read using individual file pointer

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
MPI_File_read_all_begin(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_all_begin(fh, buf, count, datatype);

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

    mpi_errno = MPIOI_File_read_all_begin(
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

    TraceLeave_MPI_File_read_all_begin();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_all_begin, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_read_all_end - Complete a split collective read using
    individual file pointer

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_all_end(
    _In_ MPI_File fh,
    _Out_ void* buf,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_all_end(fh,buf);

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

    mpi_errno = MPIOI_File_read_all_end(pFile, buf, status);

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_read_all_end(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_all_end, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_read_at - Read using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_at(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_at(fh, offset, buf, count, datatype);

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

    mpi_errno = MPIOI_File_read(
        pFile,
        offset,
        ADIO_EXPLICIT_OFFSET,
        buf,
        count,
        hType.GetMpiHandle(),
        status
        );

    if(mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    TraceLeave_MPI_File_read_at(SENTINEL_SAFE_SIZE(status), status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_at, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_read_at_all - Collective read using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_at_all(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_at_all(fh, offset, buf, count, datatype);

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

    mpi_errno = MPIOI_File_read_all(
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

    TraceLeave_MPI_File_read_at_all(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_at_all, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_read_at_all_begin - Begin a split collective read using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_at_all_begin(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_at_all_begin(fh, offset, buf, count, datatype);

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

    mpi_errno = MPIOI_File_read_all_begin(
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

    TraceLeave_MPI_File_read_at_all_begin();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_at_all_begin, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_read_at_all_end - Complete a split collective read using
    explict offset

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_at_all_end(
    _In_ MPI_File fh,
    _Out_ void* buf,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_at_all_end(fh,buf);

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

    mpi_errno = MPIOI_File_read_all_end(pFile, buf, status);

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_read_at_all_end(SENTINEL_SAFE_SIZE(status), status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_at_all_end, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_read_ordered - Collective read using shared file pointer

Input Parameters:
. fh - file handle (handle)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_ordered(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_ordered(fh, buf, count, datatype);

    int nprocs, myrank;
    int source, dest;
    MPI_Offset shared_fp=0;

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

    mpi_errno = MpioOpenDeferred( pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    NMPI_Comm_size(pFile->comm, &nprocs);
    NMPI_Comm_rank(pFile->comm, &myrank);

    size_t datatype_size = static_cast<size_t>( hType.GetSize() );
    size_t incr = (count * datatype_size) / pFile->etype_size;

    /* Use a message as a 'token' to order the operations */
    source = myrank - 1;
    dest   = myrank + 1;
    if (source < 0) source = MPI_PROC_NULL;
    if (dest >= nprocs) dest = MPI_PROC_NULL;
    NMPI_Recv(NULL, 0, MPI_BYTE, source, 0, pFile->comm, MPI_STATUS_IGNORE);

    mpi_errno = ADIO_Get_shared_fp(pFile, incr, &shared_fp);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    NMPI_Send(NULL, 0, MPI_BYTE, dest, 0, pFile->comm);

    mpi_errno = ADIO_ReadStridedColl(pFile, buf, count, datatype, ADIO_EXPLICIT_OFFSET,
                         shared_fp, status);

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_read_ordered(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_ordered, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_read_ordered_begin - Begin a split collective read using shared file pointer

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
MPI_File_read_ordered_begin(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_ordered_begin(fh, buf, count, datatype);

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
        goto fn_fail;
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
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    NMPI_Send(NULL, 0, MPI_BYTE, dest, 0, pFile->comm);

    mpi_errno = ADIO_ReadStridedColl(
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

    TraceLeave_MPI_File_read_ordered_begin();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_ordered_begin, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_read_ordered_end - Complete a split collective read using shared file pointer

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_ordered_end(
    _In_ MPI_File fh,
    _Out_ void* buf,
    _Out_ MPI_Status* status
    )
{
    //
    // Suppress OACR C6101 for buf, status.
    //
    OACR_USE_PTR(buf);
    OACR_USE_PTR(status);

    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_ordered_end(fh, buf);

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

    TraceLeave_MPI_File_read_ordered_end(SENTINEL_SAFE_SIZE(status),status);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_ordered_end, mpi_errno);
    goto fn_exit;
}


/* status object not filled currently */

/*@
    MPI_File_read_shared - Read using shared file pointer

Input Parameters:
. fh - file handle (handle)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_read_shared(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Status* status
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_read_shared(fh, buf, count, datatype);
    //
    // fix OACR warning 6101 for buf, status
    //
    OACR_USE_PTR(buf);
    OACR_USE_PTR(status);

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
    if (count * datatype_size == 0)
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

    if( (pFile->access_mode & MPI_MODE_WRONLY) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ACCESS, "**iowronly" );
        goto fn_fail;
    }

    mpi_errno = MpioOpenDeferred( pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    int buftype_is_contig, filetype_is_contig;
    MPI_Offset off, shared_fp;

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
        /* convert count and shared_fp to bytes */
        size_t bufsize = datatype_size * count;
        off = pFile->disp + pFile->etype_size * shared_fp;

        /* if atomic mode requested, lock (exclusive) the region, because there
           could be a concurrent noncontiguous request. On NFS, locking
           is done in the ADIO_ReadContig.*/

        if ((pFile->atomicity))
        {
            ADIOI_WRITE_LOCK(pFile, off, bufsize);
        }

        mpi_errno = ADIO_ReadContig(
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
        /* For strided and atomic mode, locking is done in ADIO_ReadStrided */
        mpi_errno = ADIO_ReadStrided(
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

    TraceLeave_MPI_File_read_shared(SENTINEL_SAFE_SIZE(status), status);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_read_shared, mpi_errno);
    goto fn_exit;
}

/*@
    MPI_File_iread - Nonblocking read using individual file pointer

Input Parameters:
. fh - file handle (handle)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. request - request object (handle)

.N fortran
@*/

EXTERN_C
MPI_METHOD
MPI_File_iread(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_iread(fh, buf, count, datatype);

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

    mpi_errno = MPIOI_File_iread(
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

    TraceLeave_MPI_File_iread(*request);

fn_exit:

    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_iread, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_iread_at - Nonblocking read using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. request - request object (handle)

.N fortran
@*/

EXTERN_C
MPI_METHOD
MPI_File_iread_at(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_iread_at(fh, offset, buf, count, datatype);

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

    mpi_errno = MPIOI_File_iread(
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

    TraceLeave_MPI_File_iread_at( *request );
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_iread_at, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_iread_shared - Nonblocking read using shared file pointer

Input Parameters:
. fh - file handle (handle)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. request - request object (handle)

.N fortran
@*/

EXTERN_C
MPI_METHOD
MPI_File_iread_shared(
    _In_ MPI_File fh,
    _Out_opt_ void* buf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_iread_shared(fh, buf, count, datatype);

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

    int buftype_is_contig, filetype_is_contig;
    MPI_Status status;
    MPI_Offset off, shared_fp;
    MPI_Offset nbytes=0;

    ADIOI_Datatype_iscontig(hType.GetMpiHandle(), &buftype_is_contig);
    ADIOI_Datatype_iscontig(pFile->filetype, &filetype_is_contig);

    size_t incr = (count * datatype_size) / pFile->etype_size;
    mpi_errno = ADIO_Get_shared_fp(pFile, incr, &shared_fp);

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (buftype_is_contig && filetype_is_contig)
    {
    /* convert count and shared_fp to bytes */
        size_t bufsize = datatype_size * count;
        off = pFile->disp + pFile->etype_size * shared_fp;
        if (!(pFile->atomicity))
        {
            mpi_errno = ADIO_IreadContig(
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

            mpi_errno = ADIO_ReadContig(
                pFile,
                buf,
                count,
                hType.GetMpiHandle(),
                ADIO_EXPLICIT_OFFSET,
                off,
                &status
                );

            ADIOI_UNLOCK(pFile, off, bufsize);

            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }

            nbytes = count * datatype_size;
            MPIO_Completed_request_create(nbytes, request);
        }
    }
    else
    {
        mpi_errno = ADIO_IreadStrided(
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

    TraceLeave_MPI_File_iread_shared(*request);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_iread_shared, mpi_errno);
    goto fn_exit;
}
