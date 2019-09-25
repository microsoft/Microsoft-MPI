// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"
#include "ad_ntfs.h"
#include "adio_extern.h"
#include "adio_cb_config_list.h"


static int is_aggregator(int rank, MPI_File fd);

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
    )
{
    ADIO_cb_name_array array = NULL;
    int orig_amode_excl, orig_amode_wronly, err, rank, procs;
    char *value;
    int rank_ct, max_error_code;
    int *tmp_ranklist;
    MPI_Comm aggregator_comm = MPI_COMM_NULL; /* just for deferred opens */
    int mpi_errno;

    /* obtain MPI_File handle */
    ADIOI_FileD* fd = static_cast<ADIOI_FileD*>(ADIOI_Malloc( sizeof(ADIOI_FileD) ));
    if( fd == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    fd->fp_ind = disp;
    fd->comm = comm;       /* dup'ed in MPI_File_open */

    fd->filename = filename;
    fd->shared_fp_fname = NULL;
    fd->hints = NULL;
    fd->info = NULL;

    fd->disp = disp;
    fd->split_coll_count = 0;
    fd->shared_fp_fd = MPI_FILE_NULL;
    fd->atomicity = 0;
    fd->etype = etype;          /* MPI_BYTE by default */
    fd->filetype = filetype;    /* MPI_BYTE by default */
    fd->etype_size = 1;  /* default etype is MPI_BYTE */

    fd->fortran_handle = -1;

    fd->err_handler = ADIOI_DFLT_ERR_HANDLER;

    /* create and initialize info object */
    fd->hints = (ADIOI_Hints *)ADIOI_Malloc(sizeof(ADIOI_Hints));
    if( fd->hints == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    fd->hints->cb_config_list = NULL;
    fd->hints->ranklist = NULL;
    fd->hints->initialized = 0;
    fd->info = MPI_INFO_NULL;
    err = ADIO_SetInfo(fd, info);

    /* gather the processor name array if we don't already have it */

    /* this has to be done here so that we can cache the name array in both
     * the dup'd communicator (in case we want it later) and the original
     * communicator
     */
    ADIOI_cb_gather_name_array(orig_comm, comm, &array);

    /* parse the cb_config_list and create a rank map on rank 0 */
    NMPI_Comm_rank(comm, &rank);
    if (rank == 0)
    {
        NMPI_Comm_size(comm, &procs);
        tmp_ranklist = (int *) ADIOI_Malloc(sizeof(int) * procs);
        if (tmp_ranklist == NULL)
        {
            /* NEED TO HANDLE ENOMEM ERRORS */
        }

        rank_ct = ADIOI_cb_config_list_parse(fd->hints->cb_config_list,
                                             array, tmp_ranklist,
                                             fd->hints->cb_nodes);

        /* store the ranklist using the minimum amount of memory */
        if (rank_ct > 0)
        {
            fd->hints->ranklist = (int *) ADIOI_Malloc(sizeof(int) * rank_ct);
            memcpy(fd->hints->ranklist, tmp_ranklist, sizeof(int) * rank_ct);
        }
        ADIOI_Free(tmp_ranklist);
        fd->hints->cb_nodes = rank_ct;
        /* TEMPORARY -- REMOVE WHEN NO LONGER UPDATING INFO FOR FS-INDEP. */
        value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));
        MPIU_Snprintf(value, MPI_MAX_INFO_VAL+1, "%d", rank_ct);
        value[MPI_MAX_INFO_VAL] = 0;
        NMPI_Info_set(fd->info, const_cast<char*>("cb_nodes"), value);
        ADIOI_Free(value);
    }

    ADIOI_cb_bcast_rank_map(fd);
    if (fd->hints->cb_nodes <= 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_IO, "**ioagnomatch");
        goto fn_fail;
    }


     /* deferred open: if we are an aggregator, create a new communicator.
      * we'll use this aggregator communicator for opens and closes.
      * otherwise, we have a NULL communicator until we try to do independent
      * IO */
    fd->agg_comm = MPI_COMM_NULL;
    fd->is_open = 0;
    if (fd->hints->deferred_open )
    {
        /* MPI_Comm_split will create a communication group of aggregators.
         * for non-aggregators it will return MPI_COMM_NULL .  we rely on
         * fd->agg_comm == MPI_COMM_NULL for non-aggregators in several
         * tests in the code
         * is_aggregator returns 1 if found, otherwise MPI_UNDEFINED */
        NMPI_Comm_split(fd->comm, is_aggregator(rank, fd), 0, &aggregator_comm);
        fd->agg_comm = aggregator_comm;
    }

    orig_amode_excl = access_mode;

    /* optimization: by having just one process create a file, close it, then
     * have all N processes open it, we can possibly avoid contention for write
     * locks on a directory for some file systems.
     *
     * we used to special-case EXCL|CREATE, since when N processes are trying
     * to create a file exclusively, only 1 will succeed and the rest will
     * (spuriously) fail.   Since we are now carrying out the CREATE on one
     * process anyway, the EXCL case falls out and we don't need to explicitly
     * worry about it, other than turning off both the EXCL and CREATE flags
     */
    MPIU_Assert(fd->hints->ranklist != NULL);
    /* the actual optimized create on one, open on all */
    if (access_mode & MPI_MODE_CREATE)
    {
       if(rank == fd->hints->ranklist[0])
       {
            fd->access_mode = access_mode;
            if (access_mode & MPI_MODE_DELETE_ON_CLOSE)    /* remove delete_on_close flag if set */
            {
                fd->access_mode ^= MPI_MODE_DELETE_ON_CLOSE;
            }

            mpi_errno = ADIO_FS_Open(fd);
            NMPI_Bcast(&mpi_errno, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);

            /* if no error, close the file and reopen normally below */
            if (mpi_errno == MPI_SUCCESS)
            {
                ADIO_FS_Close(fd);
            }

            fd->access_mode = access_mode;    /* back to original */
       }
       else
       {
           NMPI_Bcast(&mpi_errno, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);
       }

       if (mpi_errno != MPI_SUCCESS)
           goto fn_fail;

       /* turn off CREAT (and EXCL if set) for real multi-processor open */
       access_mode &= ~(MPI_MODE_CREATE | MPI_MODE_EXCL);
    }

    /* if we are doing deferred open, non-aggregators should return now */
    if (fd->hints->deferred_open )
    {
        if (fd->agg_comm == MPI_COMM_NULL)
        {
            /* we might have turned off EXCL for the aggregators.
             * restore access_mode that non-aggregators get the right
             * value from get_amode */
            fd->access_mode = orig_amode_excl;
            mpi_errno = MPI_SUCCESS;
            goto fn_exit;
        }
    }

/* For writing with data sieving, a read-modify-write is needed. If
   the file is opened for write_only, the read will fail. Therefore,
   if write_only, open the file as read_write, but record it as write_only
   in fd, so that get_amode returns the right answer. */

    orig_amode_wronly = access_mode;
    if (access_mode & MPI_MODE_WRONLY)
    {
        access_mode = access_mode ^ MPI_MODE_WRONLY;
        access_mode = access_mode | MPI_MODE_RDWR;
    }
    fd->access_mode = access_mode;

    mpi_errno = ADIO_FS_Open(fd);

    /* if error, may be it was due to the change in amode above.
       therefore, reopen with access mode provided by the user.*/
    fd->access_mode = orig_amode_wronly;
    if (mpi_errno != MPI_SUCCESS)
    {
        mpi_errno = ADIO_FS_Open(fd);
    }

    /* if we turned off EXCL earlier, then we should turn it back on */
    fd->access_mode = orig_amode_excl;

    /* for deferred open: this process has opened the file (because if we are
     * not an aggregaor and we are doing deferred open, we returned earlier)*/
    fd->is_open = 1;

fn_fail:
fn_exit:
    NMPI_Allreduce(&mpi_errno, &max_error_code, 1, MPI_UNSIGNED, MPI_MAX, comm);
    if (max_error_code != MPI_SUCCESS)
    {
        /* If the file was successfully opened, close it */
        if (mpi_errno == MPI_SUCCESS)
        {

            /* in the deferred open case, only those who have actually
               opened the file should close it */
            MPIU_Assert(fd);
            MPIU_Assert(fd->hints);
            if ((fd->hints->deferred_open == 0) || (fd->agg_comm != MPI_COMM_NULL))
            {
                ADIO_FS_Close(fd);
            }
        }

        if(fd->hints)
        {
            if (fd->hints->ranklist)
            {
                ADIOI_Free(fd->hints->ranklist);
            }
            if (fd->hints->cb_config_list)
            {
                ADIOI_Free(fd->hints->cb_config_list);
            }
            ADIOI_Free(fd->hints);
        }
        if (fd->info != MPI_INFO_NULL)
        {
            NMPI_Info_free(&(fd->info));
        }
        ADIOI_Free(fd);
        fd = MPI_FILE_NULL;

        delete[] filename;

        if (mpi_errno == MPI_SUCCESS)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_IO, "**oremote_fail");
        }
    }

    *pfd = fd;
    return mpi_errno;
}

/* a simple linear search. possible enhancement: add a my_cb_nodes_index member
 * ( index into cb_nodes, else -1 if not aggregator ) for faster lookups
 *
 * fd->hints->cb_nodes is the number of aggregators
 * fd->hints->ranklist[] is an array of the ranks of aggregators
 *
 * might want to move this to adio/common/cb_config_list.c
 *
 * returning 1 if found, otherwise MPI_UNDEFINED
 */
int is_aggregator(int rank, MPI_File fd )
{
    OACR_USE_PTR( fd );
    int i = fd->hints->cb_nodes;

    while ( i > 0 )
    {
        --i;
        if ( rank == fd->hints->ranklist[i] )
        {
            return 1;
        }
    }
    return MPI_UNDEFINED;
}


int ADIO_ImmediateOpen(MPI_File fd)
{
    int rc = ADIO_FS_Open(fd);
    if(rc != MPI_SUCCESS)
        return rc;

    fd->is_open = 1;
    return MPI_SUCCESS;
}


void ADIO_Close(MPI_File fd)
{
    int i, j, k, combiner, myrank, err, is_contig;

    /* because of deferred open, this warants a bit of explaining.  First, if
     * we've done aggregation (fd->agg_comm has a non-nulll communicator ),
     * then close the file.  Then, if any process left has done independent
     * i/o, close the file.  Otherwise, we'll skip the fs-specific close and
     * just say everything is a-ok.
     *
     * XXX: is it ok for those processes with a "real" communicator and those
     * with "MPI_COMM_SELF" to both call ADIO_FS_Close at the same time ?
     * everyone who ever opened the file will close it. Is order important? Is
     * timing important?
     */
    if ((fd->agg_comm != MPI_COMM_NULL) || fd->is_open)
    {
        ADIO_FS_Close(fd);
    }

    if (fd->access_mode & MPI_MODE_DELETE_ON_CLOSE)
    {
        /* if we are doing aggregation and deferred open, then it's possible
         * that rank 0 does not have access to the file. make sure only an
         * aggregator deletes the file.*/
        NMPI_Comm_rank(fd->comm, &myrank);
        if (myrank == fd->hints->ranklist[0])
        {
            err = ADIO_FS_Delete(fd->filename);
        }
        NMPI_Barrier(fd->comm);
    }

    if (fd->fortran_handle != -1)
    {
        ADIOI_Ftable[fd->fortran_handle] = MPI_FILE_NULL;
    }

    ADIOI_Free(fd->hints->ranklist);
    ADIOI_Free(fd->hints->cb_config_list);
    ADIOI_Free(fd->hints);
    NMPI_Comm_free(&(fd->comm));
    /* deferred open: if we created an aggregator communicator, free it */
    if (fd->agg_comm != MPI_COMM_NULL)
    {
        NMPI_Comm_free(&(fd->agg_comm));
    }
    delete[] fd->filename;

    if( fd->shared_fp_fname != NULL )
    {
        delete[] fd->shared_fp_fname;
    }

    NMPI_Type_get_envelope(fd->etype, &i, &j, &k, &combiner);
    if (combiner != MPI_COMBINER_NAMED) NMPI_Type_free(&(fd->etype));

    ADIOI_Datatype_iscontig(fd->filetype, &is_contig);
    if (!is_contig) ADIOI_Delete_flattened(fd->filetype);

    NMPI_Type_get_envelope(fd->filetype, &i, &j, &k, &combiner);
    if (combiner != MPI_COMBINER_NAMED) NMPI_Type_free(&(fd->filetype));

    NMPI_Info_free(&(fd->info));

    ADIOI_Free(fd);
}


int ADIOI_GEN_Delete(const wchar_t* filename)
{
    if( DeleteFileW( filename ) != TRUE )
    {
        return MPIO_ERR_GLE(::GetLastError());
    }

    return MPI_SUCCESS;
}


int ADIOI_NTFS_Open(MPI_File fd)
{
    //
    // 1. Windows Server 2008 has a "file not found" cache for every host machine.
    // The cache is refreshed every ten seconds.  If a process creates a file that
    // is accessible over the network, the "not found" cache on remote hosts will
    // not be immediately updated.  Calling CreateFile from a remote host with
    // disposition (cmode) of OPEN_EXISTING may thus return an error, even though
    // the file exists.  There is no way to force the OS to refresh the cache
    // explicitly: however, setting the dispositon to OPEN_ALWAYS will bypass
    // the cache.
    //
    DWORD cmode = OPEN_ALWAYS;
    BOOL fOpenExisting = TRUE;

    if (fd->access_mode & MPI_MODE_EXCL)
    {
        cmode = CREATE_NEW;
        fOpenExisting = FALSE;
    }
    else if (fd->access_mode & MPI_MODE_CREATE)
    {
        cmode = OPEN_ALWAYS;
        fOpenExisting = FALSE;
    }

    DWORD amode = 0;
    if (fd->access_mode & MPI_MODE_RDONLY)
    {
        amode = GENERIC_READ;
    }
    if (fd->access_mode & MPI_MODE_WRONLY)
    {
        amode = GENERIC_WRITE;
    }
    if (fd->access_mode & MPI_MODE_RDWR)
    {
        amode = GENERIC_READ | GENERIC_WRITE;
    }

    DWORD attrib = FILE_FLAG_OVERLAPPED;
    if (fd->access_mode & MPI_MODE_DELETE_ON_CLOSE)
    {
        attrib |= FILE_FLAG_DELETE_ON_CLOSE;
    }
    if (fd->access_mode & MPI_MODE_SEQUENTIAL)
    {
        attrib |= FILE_FLAG_SEQUENTIAL_SCAN;
    }
    else
    {
        attrib |= FILE_FLAG_RANDOM_ACCESS;
    }
    if( fd->access_mode & MSMPI_MODE_HIDDEN )
    {
        attrib |= FILE_ATTRIBUTE_HIDDEN;
    }

    SetLastError(NO_ERROR);
    fd->fd_sys = CreateFileW(
                    fd->filename,
                    amode,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    cmode,
                    attrib,
                    NULL
                    );
    if (fd->fd_sys == INVALID_HANDLE_VALUE)
        goto fn_fail;

    //
    // 2. We are setting cmode=OPEN_ALWAYS, even if we want to open a file only when
    // it exists (see comment 1 above).  If the file already exists, CreateFile will
    // return successfully but will set the last error code to ERROR_ALREADY_EXISTS.
    // If we only want to open an existing file, we must check the last error code:
    // if it is not ERROR_ALREADY_EXISTS, then the file did not exist previously and
    // was just created.  In that case, we must delete the file and return an error.
    //
    if (fOpenExisting && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        //
        // Don't check return codes, as we are exiting with an error anyway.
        //
        CloseHandle(fd->fd_sys);
        DeleteFileW(fd->filename);

        SetLastError(ERROR_FILE_NOT_FOUND);
        goto fn_fail;
    }

    if (fd->access_mode & MPI_MODE_APPEND)
    {
        BOOL fSucc;
        LARGE_INTEGER FileSize;
        fSucc = GetFileSizeEx(fd->fd_sys, &FileSize);
        if(!fSucc)
            goto fn_fail;

        fd->fp_ind = FileSize.QuadPart;
    }

    //
    // Associate the handle with the MPID completion port
    //
    ExAttachHandle(MPIDI_CH3I_set, fd->fd_sys);

    return MPI_SUCCESS;

fn_fail:
    return MPIO_ERR_GLE(::GetLastError());
}


void ADIOI_NTFS_Close(MPI_File fd)
{
    CloseHandle(fd->fd_sys);
}


static void ADIOI_NTFS_refresh_metadata_cache(MPI_File fd)
{
    //
    // Windows Server 2008 aggressively caches the metadata of files available over
    // the network, in order to reduce network traffic.  There is no explicit way
    // to refresh this cache.  However, issuing an invalid FSCTL instruction will
    // invalidate the cache without changing the file.  We do not check the error
    // code, since the function call will deliberately fail.
    //
    DWORD num_bytes_returned;
    DeviceIoControl(
         fd->fd_sys,                    // File handle
         FSCTL_WRITE_RAW_ENCRYPTED,     // Control code
         NULL,                          // Input buffer - invalid parameter
         0,                             // Input buffer size
         NULL,                          // Output buffer (unused)
         0,                             // Output buffer size
         &num_bytes_returned,           // Required by DeviceIoControl (but unused)
         NULL                           // OVERLAPPED structure pointer (unused
    );
}

int ADIOI_NTFS_Fcntl(MPI_File fd, int flag, ADIO_Fcntl_t* fcntl_struct)
{
    BOOL fSucc;
    LARGE_INTEGER FileSize;

    switch(flag)
    {
    case ADIO_FCNTL_GET_FSIZE:
        ADIOI_NTFS_refresh_metadata_cache(fd);

        fSucc = GetFileSizeEx(fd->fd_sys, &FileSize);
        if (!fSucc)
        {
            return MPIO_ERR_GLE(::GetLastError());
        }

        fcntl_struct->fsize = FileSize.QuadPart;
        return MPI_SUCCESS;

    case ADIO_FCNTL_SET_DISKSPACE:
        return ADIOI_GEN_Prealloc(fd, fcntl_struct->diskspace);

    case ADIO_FCNTL_SET_ATOMICITY:
        fd->atomicity = (fcntl_struct->atomicity == 0) ? 0 : 1;
        return MPI_SUCCESS;

    default:
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**flag %d", flag);
    }
}


int ADIOI_NTFS_Resize(MPI_File fd, MPI_Offset size)
{
    LARGE_INTEGER FileSize;
    FileSize.QuadPart = size;

    BOOL fSucc;
    fSucc = SetFilePointerEx(
                fd->fd_sys,
                FileSize,
                NULL,
                FILE_BEGIN
                );

    if(!fSucc)
        goto fn_fail;

    fSucc = SetEndOfFile(fd->fd_sys);
    if(!fSucc)
        goto fn_fail;

    return MPI_SUCCESS;

fn_fail:
    return MPIO_ERR_GLE(::GetLastError());
}


int ADIOI_NTFS_Flush(MPI_File fd)
{
    if(fd->access_mode & MPI_MODE_RDONLY)
        return MPI_SUCCESS;

    BOOL fSucc;
    fSucc = FlushFileBuffers(fd->fd_sys);
    if(fSucc)
        return MPI_SUCCESS;

    return  MPIO_ERR_GLE(::GetLastError());
}


static int ADIOI_NTFS_aio_query_fn(void *extra_state, MPI_Status *status)
{
    ADIOI_AIO_Request* aio_req;
    aio_req = (ADIOI_AIO_Request *)extra_state;

    BOOL fSucc;
    DWORD BytesTransferred;
    fSucc = GetOverlappedResult(
                aio_req->file,
                &aio_req->exov.ov,
                &BytesTransferred,
                FALSE       // bWait
                );
    if(!fSucc)
    {
        //
        // Note that MPI calls the Grequest query function only after the request
        // is signalled as complete.
        //
        MPIU_Assert(::GetLastError() != ERROR_IO_PENDING);
        return MPIO_ERR_GLE(::GetLastError());
    }

    MPIR_Status_set_empty(status);
    MPIR_Status_set_bytes(status, MPI_BYTE, BytesTransferred);
    return MPI_SUCCESS;
}


static int ADIOI_NTFS_aio_free_fn(void *extra_state)
{
    ADIOI_AIO_Request *aio_req;
    aio_req = (ADIOI_AIO_Request*)extra_state;

    //
    // Don't free the request until both the async read/write completes
    // and the user called MPI_Request_free or is freed by Wait/Test.
    //
    aio_req->ref_count--;
    if(aio_req->ref_count == 0)
    {
        ADIOI_Free(aio_req);
    }
    return MPI_SUCCESS;
}


static int ADIOI_NTFS_aio_cancel_fn(void* /*extra_state*/, int /*complete*/)
{
    return MPI_SUCCESS;
}


static int WINAPI NTFSReadWriteComplete(EXOVERLAPPED* pov)
{
    ADIOI_AIO_Request* aio_req = CONTAINING_RECORD(pov, ADIOI_AIO_Request, exov);

    int mpi_errno = NMPI_Grequest_complete(aio_req->req);
    ADIOI_NTFS_aio_free_fn(aio_req);
    return mpi_errno;
}


/* This function is for implementation convenience. It is not user-visible.
 * If wr==1 write, wr==0 read.
 *
 * Returns MPI_SUCCESS on success, mpi_errno on failure.
 */
int
ADIOI_NTFS_aio(
    MPI_File fd,
    void* buf,
    int len,
    MPI_Offset offset,
    int wr,
    MPI_Request* request
    )
{
    int mpi_errno;
    ADIOI_AIO_Request *aio_req;
    aio_req = (ADIOI_AIO_Request *)ADIOI_Calloc(sizeof(ADIOI_AIO_Request), 1);
    if( aio_req == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    aio_req->ref_count = 2;

    aio_req->file = fd->fd_sys;

    ExInitOverlapped(&aio_req->exov, NTFSReadWriteComplete, NTFSReadWriteComplete);
    aio_req->exov.ov.Offset = DWORDLOW(offset);
    aio_req->exov.ov.OffsetHigh = DWORDHIGH(offset);

    mpi_errno = NMPI_Grequest_start(
                    ADIOI_NTFS_aio_query_fn,
                    ADIOI_NTFS_aio_free_fn,
                    ADIOI_NTFS_aio_cancel_fn,
                    aio_req,
                    request
                    );
    if(mpi_errno != MPI_SUCCESS)
        goto fn_fail2;

    aio_req->req = *request;

    /* XXX: initiate async I/O  */
    BOOL fSucc;
    if (wr)
    {
        fSucc = WriteFile(fd->fd_sys, buf, len, NULL, &aio_req->exov.ov);
    }
    else
    {
        fSucc = ReadFile(fd->fd_sys, buf, len, NULL, &aio_req->exov.ov);
    }

    if (!fSucc)
    {
        int gle = GetLastError();
        if (gle != ERROR_IO_PENDING)
        {
            mpi_errno = MPIO_ERR_GLE(gle);
            goto fn_fail3;
        }
    }

    return MPI_SUCCESS;

fn_fail3:
    NMPI_Request_free(request);
fn_fail2:
    ADIOI_Free(aio_req);
    return mpi_errno;
}


int ADIOI_NTFS_Init_blocking_overlapped(OVERLAPPED* pov, MPI_Offset offset)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, TRUE, NULL);
    if(hEvent == NULL)
    {
        return MPIO_ERR_GLE(::GetLastError());
    }

    //
    //  Set the Event first bit to disable completion port notifications
    //
    pov->hEvent = (HANDLE)((ULONG_PTR)hEvent | (ULONG_PTR)0x1);
    pov->Offset = DWORDLOW(offset);
    pov->OffsetHigh = DWORDHIGH(offset);
    return MPI_SUCCESS;
}


/* This assumes that lock will always remain in the common directory and
 * that the ntfs directory will always be called ad_ntfs. */
int ADIOI_Set_lock(FDTYPE fd, int cmd, int type, MPI_Offset offset, MPI_Offset len)
{
    int ret_val;
    OVERLAPPED Overlapped;
    DWORD dwFlags;

    if (len == 0) return MPI_SUCCESS;

    dwFlags = type;

    int mpi_errno;
    mpi_errno = ADIOI_NTFS_Init_blocking_overlapped(&Overlapped, offset);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    if (cmd == ADIOI_LOCK_CMD)
    {
        /*printf("locking %d\n", (int)fd);fflush(stdout);*/
        ret_val = LockFileEx(fd, dwFlags, 0, DWORDLOW( len ), DWORDHIGH( len ), &Overlapped );
    }
    else
    {
        /*printf("unlocking %d\n", (int)fd);fflush(stdout);*/
        ret_val = UnlockFileEx(fd, 0, DWORDLOW( len ), DWORDHIGH( len ), &Overlapped );
    }

    if (!ret_val)
    {
        ret_val = GetLastError();
        if (ret_val == ERROR_IO_PENDING)
        {
            DWORD dummy;
            ret_val = GetOverlappedResult(fd, &Overlapped, &dummy, TRUE);
            if (ret_val)
            {
                CloseHandle(Overlapped.hEvent);
                return MPI_SUCCESS;
            }
            ret_val = GetLastError();
        }
        mpi_errno = MPIO_ERR_GLE(ret_val);
    }
    CloseHandle(Overlapped.hEvent);

    return mpi_errno;
}


/* this used to be implemented in every file system as an fcntl, but the code
 * is identical for all file systems without a real "preallocate" system call.
 * This naive approach will get the job done, but not in a terribly efficient
 * manner.
 */
int ADIOI_GEN_Prealloc(MPI_File fd, MPI_Offset diskspace)
{
    MPI_Offset curr_fsize, alloc_size, size, done;
    int len;
    MPI_Status status;
    MPI_Offset i, ntimes;
    char *buf;

    /* will be called by one process only */
    /* On file systems with no preallocation function, we have to
       explicitly write
       to allocate space. Since there could be holes in the file,
       we need to read up to the current file size, write it back,
       and then write beyond that depending on how much
       preallocation is needed.
       read/write in sizes of no more than ADIOI_PREALLOC_BUFSZ */

    /*curr_fsize = fd->fp_ind; */
    ADIO_Fcntl_t fcntl_struct;
    int mpi_errno;
    mpi_errno = ADIO_Fcntl(fd, ADIO_FCNTL_GET_FSIZE, &fcntl_struct);

    curr_fsize = fcntl_struct.fsize; /* don't rely on fd->fp_ind: might be
                                        working on a pre-existing file */
    alloc_size = diskspace;

    size = min(curr_fsize, alloc_size);

    ntimes = (size + ADIOI_PREALLOC_BUFSZ - 1)/ADIOI_PREALLOC_BUFSZ;
    buf = (char *) ADIOI_Malloc(ADIOI_PREALLOC_BUFSZ);
    done = 0;
    len = ADIOI_PREALLOC_BUFSZ;

    for (i=0; i<ntimes; i++)
    {
        if (size - done < ADIOI_PREALLOC_BUFSZ)
        {
            /* This should only occur at a fractional part at the top of the buffer */
            MPIU_Assert(i == ntimes - 1);
            len = (int)(size - done);
        }
        mpi_errno = ADIO_ReadContig(fd, buf, len, MPI_BYTE, ADIO_EXPLICIT_OFFSET, done,
                        &status);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        mpi_errno = ADIO_WriteContig(fd, buf, len, MPI_BYTE, ADIO_EXPLICIT_OFFSET,
                         done, &status);
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;

        done += len;
    }

    if (alloc_size > curr_fsize)
    {
        memset(buf, 0, ADIOI_PREALLOC_BUFSZ);
        size = alloc_size - curr_fsize;
        ntimes = (size + ADIOI_PREALLOC_BUFSZ - 1)/ADIOI_PREALLOC_BUFSZ;
        len = ADIOI_PREALLOC_BUFSZ;
        for (i=0; i<ntimes; i++)
        {
            if (alloc_size - done < ADIOI_PREALLOC_BUFSZ)
            {
                /* This should only occur at a fractional part at the top of the buffer */
                MPIU_Assert(i == ntimes - 1);
                len = (int)(alloc_size - done);
            }
            mpi_errno = ADIO_WriteContig(fd, buf, len, MPI_BYTE, ADIO_EXPLICIT_OFFSET,
                             done, &status);
            if (mpi_errno != MPI_SUCCESS)
                return mpi_errno;

            done += len;
        }
    }

    ADIOI_Free(buf);
    return MPI_SUCCESS;
}


void ADIOI_GEN_SeekIndividual(MPI_File fd, MPI_Offset offset)
{
    /* offset is in units of etype relative to the filetype */

    MPI_Offset off;
    const ADIOI_Flatlist_node *flat_file;

    int i, n_etypes_in_filetype, n_filetypes, etype_in_filetype;
    MPI_Offset abs_off_in_filetype=0;
    int size_in_filetype, sum;
    int filetype_size, etype_size, filetype_is_contig;
    MPI_Aint filetype_extent, filetype_lb;

    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);
    etype_size = fd->etype_size;

    if (filetype_is_contig)
    {
        off = fd->disp + etype_size * offset;
    }
    else
    {
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype) flat_file = flat_file->next;

        NMPI_Type_get_extent(fd->filetype, &filetype_lb, &filetype_extent);
        NMPI_Type_size(fd->filetype, &filetype_size);
        if ( ! filetype_size )
        {
            /* Since offset relative to the filetype size, we can't
               do compute the offset when that result is zero.
             */
            return;
        }

        n_etypes_in_filetype = filetype_size/etype_size;
        n_filetypes = (int) (offset / n_etypes_in_filetype);
        etype_in_filetype = (int) (offset % n_etypes_in_filetype);
        size_in_filetype = etype_in_filetype * etype_size;

        sum = 0;
        for (i=0; i<flat_file->count; i++)
        {
            sum += flat_file->blocklens[i];
            if (sum > size_in_filetype)
            {
                abs_off_in_filetype = flat_file->indices[i] +
                    size_in_filetype - (sum - flat_file->blocklens[i]);
                break;
            }
        }

        /* abs. offset in bytes in the file */
        off = fd->disp + (MPI_Offset) n_filetypes * filetype_extent +
                abs_off_in_filetype;
    }

    /*
     * we used to call lseek here and update both fp_ind and fp_sys_posn, but now
     * we don't seek and only update fp_ind (ROMIO's idea of where we are in the
     * file).  We leave the system file descriptor and fp_sys_posn alone.
     * The fs-specifc ReadContig and WriteContig will seek to the correct place in
     * the file before reading/writing if the 'offset' parameter doesn't match
     * fp_sys_posn
     */
    fd->fp_ind = off;
}


/* this used to be implemented in every file system as an fcntl.  It makes
 * deferred open easier if we know ADIO_Fcntl will always need a file to really
 * be open. set_view doesn't modify anything related to the open files.
 */
int ADIO_Set_view(MPI_File fd, MPI_Offset disp, MPI_Datatype etype,
                MPI_Datatype filetype, MPI_Info info)
{
    int combiner, i, j, k, err, filetype_is_contig;
    MPI_Datatype copy_etype, copy_filetype;
    const ADIOI_Flatlist_node *flat_file;

    /* free copies of old etypes and filetypes and delete flattened
   version of filetype if necessary */

    NMPI_Type_get_envelope(fd->etype, &i, &j, &k, &combiner);
    if (combiner != MPI_COMBINER_NAMED)
    {
        NMPI_Type_free(&(fd->etype));
    }

    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);
    if (!filetype_is_contig)
    {
        ADIOI_Delete_flattened(fd->filetype);
    }

    NMPI_Type_get_envelope(fd->filetype, &i, &j, &k, &combiner);
    if (combiner != MPI_COMBINER_NAMED)
    {
        NMPI_Type_free(&(fd->filetype));
    }

    /* set new info */
    err = ADIO_SetInfo(fd, info);

    /* set new etypes and filetypes */

    NMPI_Type_get_envelope(etype, &i, &j, &k, &combiner);
    if (combiner == MPI_COMBINER_NAMED)
    {
        fd->etype = etype;
    }
    else
    {
        NMPI_Type_contiguous(1, etype, &copy_etype);
        NMPI_Type_commit(&copy_etype);
        fd->etype = copy_etype;
    }
    NMPI_Type_get_envelope(filetype, &i, &j, &k, &combiner);
    if (combiner == MPI_COMBINER_NAMED)
    {
        fd->filetype = filetype;
    }
    else
    {
        NMPI_Type_contiguous(1, filetype, &copy_filetype);
        NMPI_Type_commit(&copy_filetype);
        fd->filetype = copy_filetype;
        ADIOI_Flatten_datatype(fd->filetype);
        /* this function will not flatten the filetype if it turns out
           to be all contiguous. */
    }

    NMPI_Type_size(fd->etype, &(fd->etype_size));
    fd->disp = disp;

    /* reset MPI-IO file pointer to point to the first byte that can
       be accessed in this view. */

    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);
    if (filetype_is_contig)
    {
        fd->fp_ind = disp;
    }
    else
    {
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype)
            flat_file = flat_file->next;
        for (i=0; i<flat_file->count; i++)
        {
            if (flat_file->blocklens[i])
            {
                fd->fp_ind = disp + flat_file->indices[i];
                break;
            }
        }
    }

    return MPI_SUCCESS;
}


void ADIOI_Get_byte_offset(MPI_File fd, MPI_Offset offset, MPI_Offset *disp)
{
    OACR_USE_PTR( fd );
    const ADIOI_Flatlist_node *flat_file;
    int i, sum, n_etypes_in_filetype, size_in_filetype;
    int n_filetypes, etype_in_filetype;
    MPI_Offset abs_off_in_filetype=0;
    int filetype_size, etype_size, filetype_is_contig;
    MPI_Aint filetype_extent, filetype_lb;

    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);
    etype_size = fd->etype_size;

    if (filetype_is_contig)
    {
        *disp = fd->disp + etype_size * offset;
    }
    else
    {
/* filetype already flattened in ADIO_Open */
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype) flat_file = flat_file->next;

        NMPI_Type_size(fd->filetype, &filetype_size);
        n_etypes_in_filetype = filetype_size/etype_size;
        n_filetypes = (int) (offset / n_etypes_in_filetype);
        etype_in_filetype = (int) (offset % n_etypes_in_filetype);
        size_in_filetype = etype_in_filetype * etype_size;

        sum = 0;
        for (i=0; i<flat_file->count; i++)
        {
            sum += flat_file->blocklens[i];
            if (sum > size_in_filetype)
            {
                abs_off_in_filetype = flat_file->indices[i] +
                    size_in_filetype - (sum - flat_file->blocklens[i]);
                break;
            }
        }

        /* abs. offset in bytes in the file */
        NMPI_Type_get_extent(fd->filetype, &filetype_lb, &filetype_extent);
        *disp = fd->disp + (MPI_Offset) n_filetypes*filetype_extent + abs_off_in_filetype;
    }
}


/* return the current end of file in etype units relative to the
   current view */

void ADIOI_Get_eof_offset(MPI_File fd, MPI_Offset *eof_offset)
{
    int mpi_errno, filetype_is_contig, etype_size, filetype_size;
    MPI_Offset fsize, disp, sum=0, size_in_file;
    int n_filetypes, flag, i, rem;
    MPI_Aint filetype_extent, filetype_lb;
    const ADIOI_Flatlist_node *flat_file;

    /* find the eof in bytes */
    ADIO_Fcntl_t fcntl_struct;
    mpi_errno = ADIO_Fcntl(fd, ADIO_FCNTL_GET_FSIZE, &fcntl_struct);
    fsize = fcntl_struct.fsize;

    /* Find the offset in etype units corresponding to eof.
       The eof could lie in a hole in the current view, or in the
       middle of an etype. In that case the offset will be the offset
       corresponding to the start of the next etype in the current view.*/

    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);
    etype_size = fd->etype_size;

    if (filetype_is_contig)
    {
        *eof_offset = (fsize - fd->disp + etype_size - 1)/etype_size;
    }
    /* ceiling division in case fsize is not a multiple of etype_size;*/
    else
    {
        /* filetype already flattened in ADIO_Open */
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype)
            flat_file = flat_file->next;

        NMPI_Type_size(fd->filetype, &filetype_size);
        NMPI_Type_get_extent(fd->filetype, &filetype_lb, &filetype_extent);

        disp = fd->disp;
        n_filetypes = -1;
        flag = 0;
        do
        {
            sum = 0;
            n_filetypes++;
            for (i=0; i<flat_file->count; i++)
            {
                sum += flat_file->blocklens[i];
                if (disp + flat_file->indices[i] +
                    (MPI_Offset) n_filetypes*filetype_extent +
                       flat_file->blocklens[i] >= fsize)
                {
                    if (disp + flat_file->indices[i] +
                           (MPI_Offset) n_filetypes*filetype_extent >= fsize)
                    {
                        sum -= flat_file->blocklens[i];
                    }
                    else
                    {
                        rem = (int) (disp + flat_file->indices[i] +
                                (MPI_Offset) n_filetypes*filetype_extent
                                + flat_file->blocklens[i] - fsize);
                        sum -= rem;
                    }
                    flag = 1;
                    break;
                }
            }
        } while (flag == 0);
        size_in_file = (MPI_Offset) n_filetypes*filetype_size + sum;
        *eof_offset = (size_in_file+etype_size-1)/etype_size; /* ceiling division */
    }
}


/* returns the current position of the individual file pointer
   in etype units relative to the current view. */

void ADIOI_Get_position(MPI_File fd, MPI_Offset *offset)
{
    OACR_USE_PTR( fd );
    const ADIOI_Flatlist_node *flat_file;
    int i, n_filetypes, flag, frd_size;
    int filetype_size, etype_size, filetype_is_contig;
    MPI_Aint filetype_extent, filetype_lb;
    MPI_Offset disp, byte_offset, sum=0, size_in_file;

    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);
    etype_size = fd->etype_size;

    if (filetype_is_contig)
    {
        *offset = (fd->fp_ind - fd->disp)/etype_size;
    }
    else
    {
/* filetype already flattened in ADIO_Open */
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype) flat_file = flat_file->next;

        NMPI_Type_size(fd->filetype, &filetype_size);
        NMPI_Type_get_extent(fd->filetype, &filetype_lb, &filetype_extent);

        disp = fd->disp;
        byte_offset = fd->fp_ind;
        n_filetypes = -1;
        flag = 0;
        do
        {
            sum = 0;
            n_filetypes++;
            for (i=0; i<flat_file->count; i++)
            {
                sum += flat_file->blocklens[i];
                if (disp + flat_file->indices[i] +
                    (MPI_Offset) n_filetypes*filetype_extent + flat_file->blocklens[i]
                    >= byte_offset)
                {
                    frd_size = (int) (disp + flat_file->indices[i] +
                        (MPI_Offset) n_filetypes*filetype_extent
                        + flat_file->blocklens[i] - byte_offset);
                    sum -= frd_size;
                    flag = 1;
                    break;
                }
            }
        } while (flag == 0);
        size_in_file = (MPI_Offset) n_filetypes*filetype_size + sum;
        *offset = size_in_file/etype_size;
    }
}
