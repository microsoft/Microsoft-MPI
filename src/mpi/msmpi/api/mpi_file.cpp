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


/*@
    MPI_File_open - Opens a file

Input Parameters:
. comm - communicator (handle)
. filename - name of file to open (string)
. amode - file access mode (integer)
. info - info object (handle)

Output Parameters:
. fh - file handle (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_open(
    _In_ MPI_Comm comm,
    _In_z_ const char* filename,
    _In_ int amode,
    _In_ MPI_Info info,
    _Out_ MPI_File* fh
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_open(comm, filename, amode, info);

    int mpi_errno, flag, /* tmp_amode, */rank;
    MPI_Comm dupcomm;

    if (comm == MPI_COMM_NULL)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM, "**comm");
        goto fn_fail;
    }

    NMPI_Comm_test_inter(comm, &flag);
    if (flag)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM,  "**commnotintra");
        goto fn_fail;
    }

    if ( ((amode&MPI_MODE_RDONLY)?1:0) + ((amode&MPI_MODE_RDWR)?1:0) +
         ((amode&MPI_MODE_WRONLY)?1:0) != 1 )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_AMODE,  "**fileamodeone");
        goto fn_fail;
    }

    if ((amode & MPI_MODE_RDONLY) &&
            ((amode & MPI_MODE_CREATE) || (amode & MPI_MODE_EXCL)))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_AMODE,  "**fileamoderead");
        goto fn_fail;
    }

    if ((amode & MPI_MODE_RDWR) && (amode & MPI_MODE_SEQUENTIAL))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_AMODE,  "**fileamodeseq");
        goto fn_fail;
    }

    if( fh == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "fh" );
        goto fn_fail;
    }

    wchar_t* wname;
    DWORD rc = MPIU_MultiByteToWideChar( filename, &wname );
    if( rc != NOERROR )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_FILE, "**filename" );
        goto fn_fail;
    }

    /* check if amode is the same on all processes */
    NMPI_Comm_dup(comm, &dupcomm);

    /* use default values for disp, etype, filetype */
    mpi_errno = ADIO_Open(
                    comm,
                    dupcomm,
                    wname,
                    amode,
                    0,          // disp
                    MPI_BYTE,   // etype
                    MPI_BYTE,   // filetype
                    info,
                    fh
                    );

    if (mpi_errno != MPI_SUCCESS)
    {
        NMPI_Comm_free(&dupcomm);
        goto fn_fail;
    }

    /* determine name of file that will hold the shared file pointer */
    /* can't support shared file pointers on a file system that doesn't
       support file locking. */
    if (mpi_errno == MPI_SUCCESS)
    {
        NMPI_Comm_rank(dupcomm, &rank);
        ADIOI_Shfp_fname(*fh, rank);

        /* if MPI_MODE_APPEND, set the shared file pointer to end of file.
           indiv. file pointer already set to end of file in ADIO_Open.
           Here file view is just bytes. */
        if ((*fh)->access_mode & MPI_MODE_APPEND)
        {
            if (rank == (*fh)->hints->ranklist[0])
            {
                /* only one person need set the sharedfp */
                mpi_errno = ADIO_Set_shared_fp(*fh, (*fh)->fp_ind);
            }
            NMPI_Barrier(dupcomm);
        }
    }

    if(mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    TraceLeave_MPI_File_open(*fh);

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(MPI_FILE_NULL, mpi_errno);
    TraceError(MPI_File_open, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_close - Closes a file

Input Parameters:
. fh - file handle (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_close(
    _In_ MPI_File *fh
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_close(*fh);

    ADIOI_FileD* pFile;
    int mpi_errno;
    if( fh == nullptr )
    {
        pFile = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "fh" );
        goto fn_fail;
    }

    mpi_errno = MpiaFileValidateHandle( *fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* need a barrier because the file containing the shared file
    pointer is opened with COMM_SELF. We don't want it to be
    deleted while others are still accessing it. */
    NMPI_Barrier((pFile)->comm);
    if ((pFile)->shared_fp_fd != MPI_FILE_NULL)
    {
        ADIO_Close((pFile)->shared_fp_fd);
        pFile->shared_fp_fd = MPI_FILE_NULL;
    }

    ADIO_Close(pFile);
    *fh = MPI_FILE_NULL;

    TraceLeave_MPI_File_close();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_close, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_delete - Deletes a file

Input Parameters:
. filename - name of file to delete (string)
. info - info object (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_delete(
    _In_z_ const char* filename,
    _In_ MPI_Info info
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_delete(filename, info);

    /* call the fs-specific delete function */
    int mpi_errno;
    wchar_t* wname;
    DWORD rc = MPIU_MultiByteToWideChar( filename, &wname );
    if( rc != NOERROR )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_FILE, "**filename" );
        goto fn_fail;
    }

    mpi_errno = ADIO_FS_Delete(wname);

    delete[] wname;

    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    TraceLeave_MPI_File_delete();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(MPI_FILE_NULL, mpi_errno);
    TraceError(MPI_File_delete,  mpi_errno );
    goto fn_exit;
}


/*@
    MPI_File_c2f - Translates a C file handle to a Fortran file handle

Input Parameters:
. fh - C file handle (handle)

Return Value:
  Fortran file handle (integer)
@*/
EXTERN_C
MPI_Fint
MPIAPI
MPI_File_c2f(
    _In_ MPI_File file
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_c2f(file);

    int mpi_errno;
    mpi_errno = MPIO_File_c2f(file);
    TraceLeave_MPI_File_c2f(mpi_errno);
    MpiaExit();
    return mpi_errno;
}


/*@
    MPI_File_f2c - Translates a Fortran file handle to a C file handle

Input Parameters:
. fh - Fortran file handle (integer)

Return Value:
  C file handle (handle)
@*/
EXTERN_C
MPI_File
MPIAPI
MPI_File_f2c(
    _In_ MPI_Fint file
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_f2c(file);

    MPI_File p = MPIO_File_f2c(file);

    TraceLeave_MPI_File_f2c(p);
    MpiaExit();
    return p;
}


/*@
    MPI_File_sync - Causes all previous writes to be transferred
                    to the storage device

Input Parameters:
. fh - file handle (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_sync(
    _In_ MPI_File fh
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_sync(fh);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIO_OPEN_DEFERRED(pFile, mpi_errno);

    mpi_errno = ADIO_Flush(pFile);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_sync();
fn_exit:

    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_sync, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_amode - Returns the file access mode

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. amode - access mode (integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_amode(
    _In_ MPI_File fh,
    _Out_ int* amode
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_amode(fh);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *amode = pFile->access_mode;

    TraceLeave_MPI_File_get_amode(*amode);
fn_exit:

    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_amode, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_atomicity - Returns the atomicity mode

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. flag - true if atomic mode, false if nonatomic mode (logical)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_atomicity(
    _In_ MPI_File fh,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_atomicity(fh);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *flag = pFile->atomicity;
    TraceLeave_MPI_File_get_atomicity( *flag );
fn_exit:

    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_atomicity, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_set_atomicity - Sets the atomicity mode

Input Parameters:
. fh - file handle (handle)
. flag - true to set atomic mode, false to set nonatomic mode (logical)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_set_atomicity(
    _In_ MPI_File fh,
    _In_range_(0, 1) int flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_set_atomicity(fh, flag);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIO_OPEN_DEFERRED(pFile, mpi_errno);

    int tmp_flag;

    //
    // Normalize flag values.
    //
    if( flag != 0 )
    {
        flag = 1;
    }

    /* check if flag is the same on all processes */
    tmp_flag = flag;
    NMPI_Bcast(&tmp_flag, 1, MPI_INT, 0, pFile->comm);

    if (tmp_flag != flag)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG,  "**notsame");
        goto fn_fail;
    }

    if (pFile->atomicity == flag)
    {
        mpi_errno = MPI_SUCCESS;
        goto fn_exit;
    }


    ADIO_Fcntl_t fcntl_struct;
    fcntl_struct.atomicity = flag;
    mpi_errno = ADIO_Fcntl(pFile, ADIO_FCNTL_SET_ATOMICITY, &fcntl_struct);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_set_atomicity();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_set_atomicity, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_byte_offset - Returns the absolute byte position in
                the file corresponding to "offset" etypes relative to
                the current view

Input Parameters:
. fh - file handle (handle)
. offset - offset (nonnegative integer)

Output Parameters:
. disp - absolute byte position of offset (nonnegative integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_byte_offset(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _Out_ MPI_Offset* disp
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_byte_offset(fh,offset);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (offset < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadoffset");
        goto fn_fail;
    }

    if( (pFile->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    ADIOI_Get_byte_offset(pFile, offset, disp);

    TraceLeave_MPI_File_get_byte_offset(*disp);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_byte_offset, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_type_extent - Returns the extent of datatype in the file

Input Parameters:
. fh - file handle (handle)
. datatype - datatype (handle)

Output Parameters:
. extent - extent of the datatype (nonnegative integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_type_extent(
    _In_ MPI_File fh,
    _In_ MPI_Datatype datatype,
    _Out_ MPI_Aint* extent
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_type_extent(fh,datatype);

    TypeHandle hType;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidateHandle( datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* FIXME: handle other file data representations */

    MPI_Aint lb;
    mpi_errno = NMPI_Type_get_extent(hType.GetMpiHandle(), &lb, extent);

    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_get_type_extent(*extent);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_type_extent, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_group - Returns the group of processes that
                         opened the file

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. group - group that opened the file (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_group(
    _In_ MPI_File fh,
    _Out_ MPI_Group* group
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_group(fh);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* note: this will return the group of processes that called open, but
     * with deferred open this might not be the group of processes that
     * actually opened the file from the file system's perspective
     */
    mpi_errno = NMPI_Comm_group(pFile->comm, group);
    if(mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_File_get_group(*group);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_group, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_position - Returns the current position of the
                individual file pointer in etype units relative to
                the current view

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. offset - offset of individual file pointer (nonnegative integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_position(
    _In_ MPI_File fh,
    _Out_ MPI_Offset* offset
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_position(fh);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( (pFile->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    ADIOI_Get_position(pFile, offset);

    TraceLeave_MPI_File_get_position( *offset );
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_position, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_position_shared - Returns the current position of the
               shared file pointer in etype units relative to the current view

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. offset - offset of shared file pointer (nonnegative integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_position_shared(
    _In_ MPI_File fh,
    _Out_ MPI_Offset* offset
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_position_shared(fh);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( (pFile->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    MPIO_OPEN_DEFERRED(pFile, mpi_errno);

    mpi_errno = ADIO_Get_shared_fp(pFile, 0, offset);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    TraceLeave_MPI_File_get_position_shared(*offset);
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:

    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_position_shared, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_size - Returns the file size

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. size - size of the file in bytes (nonnegative integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_size(
    _In_ MPI_File fh,
    _Out_ MPI_Offset* size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_size(fh);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPIO_OPEN_DEFERRED(pFile, mpi_errno);

    ADIO_Fcntl_t fcntl_struct;
    mpi_errno = ADIO_Fcntl(pFile, ADIO_FCNTL_GET_FSIZE, &fcntl_struct);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    *size = fcntl_struct.fsize;
    TraceLeave_MPI_File_get_size( *size );
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_get_size, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_set_size - Sets the file size

Input Parameters:
. fh - file handle (handle)
. size - size to truncate or expand file (nonnegative integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_set_size(
    _In_ MPI_File fh,
    _In_ MPI_Offset size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_set_size(fh, size);

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( (pFile->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    if (size < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadsize");
        goto fn_fail;
    }

    MPI_Offset tmp_sz = size;
    NMPI_Bcast(&tmp_sz, 1, MPI_OFFSET, 0, pFile->comm);

    if (tmp_sz != size)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**notsame");
        goto fn_fail;
    }

    MPIO_OPEN_DEFERRED(pFile, mpi_errno);

    mpi_errno = ADIO_Resize(pFile, size);
    if (mpi_errno != MPI_SUCCESS)
        goto fn_fail;

    TraceLeave_MPI_File_set_size();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_set_size, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_get_view - Returns the file view

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. disp - displacement (nonnegative integer)
. etype - elementary datatype (handle)
. filetype - filetype (handle)
. datarep - data representation (string)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_get_view(
    _In_ MPI_File fh,
    _Out_ MPI_Offset* disp,
    _Out_ MPI_Datatype* etype,
    _Out_ MPI_Datatype* filetype,
    _Out_writes_z_(MPI_MAX_DATAREP_STRING) char* datarep
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_get_view(fh);

    int i, j, k, combiner;
    MPI_Datatype copy_etype, copy_filetype;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (datarep <= (char *) 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG,  "**iodatarepnomem");
        goto fn_fail;
    }

    *disp = pFile->disp;
    MPIU_Strncpy(datarep, "native", MPI_MAX_DATAREP_STRING);

    NMPI_Type_get_envelope(pFile->etype, &i, &j, &k, &combiner);
    if (combiner == MPI_COMBINER_NAMED)
    {
        *etype = pFile->etype;
    }
    else
    {
        NMPI_Type_contiguous(1, pFile->etype, &copy_etype);
        NMPI_Type_commit(&copy_etype);

        *etype = copy_etype;
    }

    NMPI_Type_get_envelope(pFile->filetype, &i, &j, &k, &combiner);
    if (combiner == MPI_COMBINER_NAMED)
    {
        *filetype = pFile->filetype;
    }
    else
    {
        NMPI_Type_contiguous(1, pFile->filetype, &copy_filetype);

        NMPI_Type_commit(&copy_filetype);
        *filetype = copy_filetype;
    }

    TraceLeave_MPI_File_get_view(*disp, *etype, *filetype, datarep );
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(fh, mpi_errno);
    TraceError(MPI_File_get_view, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_set_view - Sets the file view

Input Parameters:
. fh - file handle (handle)
. disp - displacement (nonnegative integer)
. etype - elementary datatype (handle)
. filetype - filetype (handle)
. datarep - data representation (string)
. info - info object (handle)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_set_view(
    _In_ MPI_File fh,
    _In_ MPI_Offset disp,
    _In_ MPI_Datatype etype,
    _In_ MPI_Datatype filetype,
    _In_z_ const char* datarep,
    _In_ MPI_Info info
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_set_view(fh, disp, etype, filetype, datarep, info);

    int filetype_size, etype_size;
    MPI_Offset shared_fp, byte_off;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if ((disp < 0) && (disp != MPI_DISPLACEMENT_CURRENT))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG,  "**iobaddisp");
        goto fn_fail;
    }

    /* rudimentary checks for incorrect etype/filetype.*/
    if (etype == MPI_DATATYPE_NULL)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**ioetype");
        goto fn_fail;
    }

    if (filetype == MPI_DATATYPE_NULL)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iofiletype");
        goto fn_fail;
    }

    if ((pFile->access_mode & MPI_MODE_SEQUENTIAL) &&
        (disp != MPI_DISPLACEMENT_CURRENT))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG,  "**iodispifseq");
        goto fn_fail;
    }

    if ((disp == MPI_DISPLACEMENT_CURRENT) &&
        !(pFile->access_mode & MPI_MODE_SEQUENTIAL))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG,  "**iodispifseq");
        goto fn_fail;
    }

    NMPI_Type_size(filetype, &filetype_size);
    NMPI_Type_size(etype, &etype_size);

    if (filetype_size % etype_size != 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iofiletype");
        goto fn_fail;
    }

    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        datarep,
                        -1,
                        "native",
                        -1 ) != CSTR_EQUAL &&
        CompareStringA( LOCALE_INVARIANT,
                        0,
                        datarep,
                        -1,
                        "NATIVE",
                        -1 ) != CSTR_EQUAL )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_UNSUPPORTED_DATAREP,  "**unsupporteddatarep");
        goto fn_fail;
    }

    if (disp == MPI_DISPLACEMENT_CURRENT)
    {
        NMPI_Barrier(pFile->comm);
        mpi_errno = ADIO_Get_shared_fp(pFile, 0, &shared_fp);
        /* TODO: check error code */

        NMPI_Barrier(pFile->comm);
        ADIOI_Get_byte_offset(pFile, shared_fp, &byte_off);
        /* TODO: check error code */

        disp = byte_off;
    }

    mpi_errno = ADIO_Set_view(pFile, disp, etype, filetype, info);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    /* reset shared file pointer to zero */
    if (pFile->shared_fp_fd != MPI_FILE_NULL)
    {
        /* only one process needs to set it to zero, but I don't want to
           create the shared-file-pointer file if shared file pointers have
           not been used so far. Therefore, every process that has already
           opened the shared-file-pointer file sets the shared file pointer
           to zero. If the file was not opened, the value is automatically
           zero. Note that shared file pointer is stored as no. of etypes
           relative to the current view, whereas indiv. file pointer is
           stored in bytes. */

        mpi_errno = ADIO_Set_shared_fp(pFile, 0);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
    }

    NMPI_Barrier(pFile->comm); /* for above to work correctly */
    TraceLeave_MPI_File_set_view();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_set_view, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_preallocate - Preallocates storage space for a file

Input Parameters:
. fh - file handle (handle)
. size - size to preallocate (nonnegative integer)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_preallocate(
    _In_ MPI_File fh,
    _In_ MPI_Offset size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_preallocate(fh, size);

    int mynod=0;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (size < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadsize");
        goto fn_fail;
    }

    MPI_Offset tmp_sz = size;

    NMPI_Bcast(&tmp_sz, 1, MPI_OFFSET, 0, pFile->comm);
    if (tmp_sz != size)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**notsame");
        goto fn_fail;
    }

    if (size == 0)
    {
        goto fn_exit;
    }

    MPIO_OPEN_DEFERRED(pFile, mpi_errno);

    NMPI_Comm_rank(pFile->comm, &mynod);
    if (!mynod)
    {
        ADIO_Fcntl_t fcntl_struct;
        fcntl_struct.diskspace = size;
        mpi_errno = ADIO_Fcntl(pFile, ADIO_FCNTL_SET_DISKSPACE, &fcntl_struct);
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
    }
    NMPI_Barrier(pFile->comm);

    TraceLeave_MPI_File_preallocate();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_preallocate, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_seek - Updates the individual file pointer

Input Parameters:
. fh - file handle (handle)
. offset - file offset (integer)
. whence - update mode (state)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_seek(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_ int whence
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_seek(fh, offset, whence);

    MPI_Offset curr_offset, eof_offset;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( (pFile->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    switch(whence)
    {
    case MPI_SEEK_SET:
        if (offset < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadoffset");
            goto fn_fail;
        }
        break;
    case MPI_SEEK_CUR:
        /* find offset corr. to current location of file pointer */
        ADIOI_Get_position(pFile, &curr_offset);
        offset += curr_offset;

        if (offset < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**ionegoffset");
            goto fn_fail;
        }

        break;
    case MPI_SEEK_END:
        /* we can in many cases do seeks w/o a file actually opened, but not in
         * the MPI_SEEK_END case */
        MPIO_OPEN_DEFERRED(pFile, mpi_errno);

        /* find offset corr. to end of file */
        ADIOI_Get_eof_offset(pFile, &eof_offset);
        offset += eof_offset;

        if (offset < 0)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**ionegoffset");
            goto fn_fail;
        }

        break;
    default:
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadwhence");
        goto fn_fail;
    }

    ADIO_SeekIndividual(pFile, offset);

    TraceLeave_MPI_File_seek();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_seek, mpi_errno);
    goto fn_exit;
}


/*@
    MPI_File_seek_shared - Updates the shared file pointer

Input Parameters:
. fh - file handle (handle)
. offset - file offset (integer)
. whence - update mode (state)

.N fortran
@*/
EXTERN_C
MPI_METHOD
MPI_File_seek_shared(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_ int whence
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_File_seek_shared(fh, offset, whence);

    int tmp_whence, myrank;
    MPI_Offset curr_offset, eof_offset, tmp_offset;

    ADIOI_FileD* pFile;
    int mpi_errno = MpiaFileValidateHandle( fh, &pFile );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( (pFile->access_mode & MPI_MODE_SEQUENTIAL) != 0 )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_UNSUPPORTED_OPERATION,
            "**ioamodeseq %s",
            __FUNCTION__
            );
        goto fn_fail;
    }

    tmp_offset = offset;
    NMPI_Bcast(&tmp_offset, 1, MPI_OFFSET, 0, pFile->comm);
    if (tmp_offset != offset)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**notsame");
        goto fn_fail;
    }

    tmp_whence = whence;
    NMPI_Bcast(&tmp_whence, 1, MPI_INT, 0, pFile->comm);
    if (tmp_whence != whence)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadwhence");
        goto fn_fail;
    }

    MPIO_OPEN_DEFERRED(pFile, mpi_errno);

    NMPI_Comm_rank(pFile->comm, &myrank);

    if (!myrank)
    {
        switch(whence)
        {
        case MPI_SEEK_SET:
            if (offset < 0)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadoffset");
                goto fn_fail;
            }
            break;
        case MPI_SEEK_CUR:
            /* get current location of shared file pointer */
            mpi_errno = ADIO_Get_shared_fp(pFile, 0, &curr_offset);
            ON_ERROR_FAIL(mpi_errno);

            offset += curr_offset;
            if (offset < 0)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**ionegoffset");
                goto fn_fail;
            }
            break;
        case MPI_SEEK_END:
            /* find offset corr. to end of file */
            ADIOI_Get_eof_offset(pFile, &eof_offset);
            offset += eof_offset;
            if (offset < 0)
            {
                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**ionegoffset");
                goto fn_fail;
            }
            break;
        default:
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**iobadwhence");
            goto fn_fail;
        }

        mpi_errno = ADIO_Set_shared_fp(pFile, offset);
        ON_ERROR_FAIL(mpi_errno);
    }

    /* FIXME: explain why the barrier is necessary */
    NMPI_Barrier(pFile->comm);

    TraceLeave_MPI_File_seek_shared();
fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIO_Err_return_file(pFile, mpi_errno);
    TraceError(MPI_File_seek_shared, mpi_errno);
    goto fn_exit;
}
