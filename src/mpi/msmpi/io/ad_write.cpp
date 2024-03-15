// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 2004 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"

#include "adio.h"
#include "ad_ntfs.h"
#include "adio_extern.h"


int ADIOI_NTFS_IwriteContig(MPI_File fd, const void *buf, int count,
                             MPI_Datatype datatype, int file_ptr_type,
                             MPI_Offset offset, MPI_Request *request
                             )
{
    int len, typesize;

    NMPI_Type_size(datatype, &typesize);
    len = count * typesize;

    if (file_ptr_type == ADIO_INDIVIDUAL)
    {
        offset = fd->fp_ind;
    }

    int mpi_errno;
    mpi_errno = ADIOI_NTFS_aio(fd, const_cast<void*>(buf), len, offset, 1, request);
    if (file_ptr_type == ADIO_INDIVIDUAL)
    {
        fd->fp_ind += len;
    }

    if (mpi_errno != MPI_SUCCESS)
        return MPIU_ERR_FAIL(mpi_errno);

    return MPI_SUCCESS;
}


int ADIOI_NTFS_WriteContig(MPI_File fd, const void *buf, int count,
                            MPI_Datatype datatype, int file_ptr_type,
                            MPI_Offset offset, MPI_Status *status
                            )
{
    DWORD dwNumWritten = 0;
    int err=-1, datatype_size, len;
    OVERLAPPED ov;

    /* If file pointer type in ADIO_INDIVIDUAL then offset should be
        ignored and the current location of file pointer should be used */
    if(file_ptr_type == ADIO_INDIVIDUAL)
    {
        offset = fd->fp_ind;
    }

    NMPI_Type_size(datatype, &datatype_size);
    len = datatype_size * count;

    int mpi_errno;
    mpi_errno = ADIOI_NTFS_Init_blocking_overlapped(&ov, offset);
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    err = WriteFile(fd->fd_sys, buf, len, &dwNumWritten, &ov);
    if (err == FALSE)
    {
        err = GetLastError();
        if (err != ERROR_IO_PENDING)
        {
            CloseHandle(ov.hEvent);
            return MPIO_ERR_GLE(err);
        }
    }
    err = GetOverlappedResult(fd->fd_sys, &ov, &dwNumWritten, TRUE);
    if (err == FALSE)
    {
        err = GetLastError();
        CloseHandle(ov.hEvent);
        return MPIO_ERR_GLE(err);
    }

    CloseHandle(ov.hEvent);

    if (file_ptr_type == ADIO_INDIVIDUAL)
    {
        fd->fp_ind = fd->fp_ind + dwNumWritten;
    }

    if (err != FALSE)
    {
        MPIR_Status_set_bytes(status, datatype, dwNumWritten);
    }

    if (err == FALSE)
    {
        err = GetLastError();
        return MPIO_ERR_GLE(err);
    }

    return MPI_SUCCESS;
}


/* Generic implementation of IwriteStrided calls the blocking WriteStrided
 * immediately.
 */
int ADIOI_GEN_IwriteStrided(MPI_File fd, const void *buf, int count,
                             MPI_Datatype datatype, int file_ptr_type,
                             MPI_Offset offset, MPI_Request *request
                             )
{
    /* Call the blocking function.  It will create an error code
     * if necessary.
     */
    int mpi_errno;
    MPI_Status status;
    mpi_errno = ADIO_WriteStrided(fd, buf, count, datatype, file_ptr_type,
                      offset, &status);

    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    int typesize;
    NMPI_Type_size(datatype, &typesize);
    MPI_Offset nbytes = count * typesize;
    MPIO_Completed_request_create(nbytes, request);
    return MPI_SUCCESS;
}


/* prototypes of functions used for collective writes only. */
static int ADIOI_Exch_and_write(MPI_File fd, const void *buf, MPI_Datatype
                         datatype, int nprocs, int myrank,
                         ADIOI_Access
                         *others_req, const MPI_Offset *offset_list,
                         const int *len_list, int contig_access_count, MPI_Offset
                         min_st_offset, MPI_Offset fd_size,
                         const MPI_Offset *fd_start, const MPI_Offset *fd_end,
                         _Out_cap_(nprocs) int *buf_idx);


_Success_( *mpi_errno == MPI_SUCCESS )
static void
ADIOI_W_Exchange_data(
    _In_ MPI_File fd,
    _In_ const void *buf,
    _Out_writes_bytes_opt_(size) char *write_buf,
    _In_opt_ const ADIOI_Flatlist_node *flat_buf,
    _In_reads_(contig_access_count) const MPI_Offset *offset_list,
    _In_reads_(contig_access_count) const int *len_list,
    _Out_writes_(nprocs) int *send_size,
    _In_reads_(nprocs) const int *recv_size,
    _In_ MPI_Offset off,
    _In_ int size,
    _In_reads_(nprocs) const int *count,
    _In_reads_(nprocs) const int *start_pos,
    _In_reads_(nprocs) const int *partial_recv,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int *sent_to_proc,
    _In_range_(>,0) int nprocs,
    _In_range_(>=,0) int myrank,
    _In_range_(0,1) int buftype_is_contig,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset *fd_start,
    _In_ const MPI_Offset *fd_end,
    _Inout_updates_(nprocs) ADIOI_Access *others_req,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int* send_buf_idx,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int* curr_to_proc,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int* done_to_proc,
    _Out_ bool* hole,
    _In_ int iter,
    _In_ MPI_Aint buftype_extent,
    _When_(buftype_is_contig == 1, _Out_writes_(nprocs)) int *buf_idx,
    _Out_ int* mpi_errno
    );


static void
ADIOI_Fill_send_buffer(
    _In_ MPI_File fd,
    _In_ const void* buf,
    _In_ const ADIOI_Flatlist_node* flat_buf,
    _When_(contig_access_count > 0, _Out_) char** send_buf,
    _In_reads_(contig_access_count) const MPI_Offset* offset_list,
    _In_reads_(contig_access_count) const int* len_list,
    _In_ const int* send_size,
    _When_(contig_access_count > 0, _Out_) MPI_Request* requests,
    _Out_writes_(nprocs) int* sent_to_proc,
    _In_range_(>,0) int nprocs,
    _In_range_(>=,0) int myrank,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset* fd_start,
    _In_ const MPI_Offset* fd_end,
    _Out_writes_(nprocs) int* send_buf_idx,
    _Out_writes_(nprocs) int* curr_to_proc,
    _Out_writes_(nprocs) int* done_to_proc,
    _In_ int iter,
    _In_ MPI_Aint buftype_extent
    );


static void ADIOI_Heap_merge(ADIOI_Access *others_req, const int *count,
                      _Out_cap_(total_elements) MPI_Offset *srt_off, _Out_cap_(total_elements) int *srt_len, const int *start_pos,
                      int nprocs, int nprocs_recv, int total_elements);

int ADIOI_GEN_WriteStridedColl(MPI_File fd, const void *buf, int count,
                       MPI_Datatype datatype, int file_ptr_type,
                       MPI_Offset offset, MPI_Status* status
                       )
{
/* Uses a generalized version of the extended two-phase method described
   in "An Extended Two-Phase Method for Accessing Sections of
   Out-of-Core Arrays", Rajeev Thakur and Alok Choudhary,
   Scientific Programming, (5)4:301--317, Winter 1996.
   http://www.mcs.anl.gov/home/thakur/ext2ph.ps */

    ADIOI_Access *my_req;
    /* array of nprocs access structures, one for each other process in
       whose file domain this process's request lies */

    ADIOI_Access *others_req;
    /* array of nprocs access structures, one for each other process
       whose request lies in this process's file domain. */

    int i, filetype_is_contig, nprocs, nprocs_for_coll, myrank;
    int contig_access_count=0, interleave_count = 0, buftype_is_contig;
    int *count_my_req_per_proc, count_my_req_procs, count_others_req_procs;
    MPI_Offset orig_fp, start_offset, end_offset, fd_size, min_st_offset, off;
    MPI_Offset *offset_list = NULL, *st_offsets = NULL, *fd_start = NULL,
        *fd_end = NULL, *end_offsets = NULL;
    int *buf_idx = NULL, *len_list = NULL;
    int old_error, tmp_error;


    NMPI_Comm_size(fd->comm, &nprocs);
    NMPI_Comm_rank(fd->comm, &myrank);

/* the number of processes that actually perform I/O, nprocs_for_coll,
 * is stored in the hints off the MPI_File structure
 */
    nprocs_for_coll = fd->hints->cb_nodes;
    orig_fp = fd->fp_ind;

    /* only check for interleaving if cb_write isn't disabled */
    if (fd->hints->cb_write != ADIOI_HINT_DISABLE)
    {
        /* For this process's request, calculate the list of offsets and
           lengths in the file and determine the start and end offsets. */

        /* Note: end_offset points to the last byte-offset that will be accessed.
           e.g., if start_offset=0 and 100 bytes to be read, end_offset=99*/

        ADIOI_Calc_my_off_len(fd, count, datatype, file_ptr_type, offset,
                              &offset_list, &len_list, &start_offset,
                              &end_offset, &contig_access_count);

        /* each process communicates its start and end offsets to other
           processes. The result is an array each of start and end offsets stored
           in order of process rank. */

        st_offsets = (MPI_Offset *) ADIOI_Malloc(nprocs*sizeof(MPI_Offset));
        end_offsets = (MPI_Offset *) ADIOI_Malloc(nprocs*sizeof(MPI_Offset));

        NMPI_Allgather(&start_offset, 1, MPI_OFFSET, st_offsets, 1,
                      MPI_OFFSET, fd->comm);
        NMPI_Allgather(&end_offset, 1, MPI_OFFSET, end_offsets, 1,
                      MPI_OFFSET, fd->comm);

        /* are the accesses of different processes interleaved? */
        for (i=1; i<nprocs; i++)
        {
            /* This is a rudimentary check for interleaving, but should suffice
               for the moment. */
            if ((st_offsets[i] < end_offsets[i-1]) &&
                (st_offsets[i] <= end_offsets[i]))
            {
                interleave_count++;
            }
        }
    }

    ADIOI_Datatype_iscontig(datatype, &buftype_is_contig);

    if (fd->hints->cb_write == ADIOI_HINT_DISABLE ||
        (!interleave_count && (fd->hints->cb_write == ADIOI_HINT_AUTO)))
    {
        /* use independent accesses */
        if (fd->hints->cb_write != ADIOI_HINT_DISABLE)
        {
            ADIOI_Free(offset_list);
            ADIOI_Free(len_list);
            ADIOI_Free(st_offsets);
            ADIOI_Free(end_offsets);
        }

        fd->fp_ind = orig_fp;
        ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);

        if (buftype_is_contig && filetype_is_contig)
        {
            if (file_ptr_type == ADIO_EXPLICIT_OFFSET)
            {
                off = fd->disp + (fd->etype_size) * offset;
                return ADIO_WriteContig(fd, buf, count, datatype,
                                 ADIO_EXPLICIT_OFFSET,
                                 off, status);
            }
            else
            {
                return ADIO_WriteContig(fd, buf, count, datatype, ADIO_INDIVIDUAL,
                                  0, status);
            }
        }

        return ADIO_WriteStrided(fd, buf, count, datatype, file_ptr_type, offset, status);
    }

/* Divide the I/O workload among "nprocs_for_coll" processes. This is
   done by (logically) dividing the file into file domains (FDs); each
   process may directly access only its own file domain. */

    ADIOI_Calc_file_domains(st_offsets, end_offsets, nprocs,
                            nprocs_for_coll, &min_st_offset,
                            &fd_start, &fd_end, &fd_size);
    if(fd_start == NULL)
    {
        return MPIU_ERR_NOMEM();
    }

/* calculate what portions of the access requests of this process are
   located in what file domains */

    ADIOI_Calc_my_req(fd, offset_list, len_list, contig_access_count,
                      min_st_offset, fd_start, fd_end, fd_size,
                      nprocs, &count_my_req_procs,
                      &count_my_req_per_proc, &my_req,
                      &buf_idx);
    if(buf_idx == NULL)
    {
        ADIOI_Free(fd_start);
        ADIOI_Free(fd_end);
        return MPIU_ERR_NOMEM();
    }

/* based on everyone's my_req, calculate what requests of other
   processes lie in this process's file domain.
   count_others_req_procs = number of processes whose requests lie in
   this process's file domain (including this process itself)
   count_others_req_per_proc[i] indicates how many separate contiguous
   requests of proc. i lie in this process's file domain. */

    ADIOI_Calc_others_req(fd, count_my_req_procs,
                          count_my_req_per_proc, my_req,
                          nprocs, myrank,
                          &count_others_req_procs, &others_req);

    ADIOI_Free(count_my_req_per_proc);
    i = nprocs;
    do
    {
        --i;
        if (my_req[i].count != 0)
        {
            ADIOI_Free(my_req[i].offsets);
            ADIOI_Free(my_req[i].lens);
        }
    } while (i > 0);
    ADIOI_Free(my_req);

    /* exchange data and write in sizes of no more than coll_bufsize. */
    int mpi_errno;
    mpi_errno = ADIOI_Exch_and_write(fd, buf, datatype, nprocs, myrank,
                        others_req, offset_list,
                        len_list, contig_access_count, min_st_offset,
                        fd_size, fd_start, fd_end, buf_idx);

    /* If this collective write is followed by an independent write,
     * it's possible to have those subsequent writes on other processes
     * race ahead and sneak in before the read-modify-write completes.
     * We carry out a collective communication at the end here so no one
     * can start independent i/o before collective I/O completes.
     *
     * need to do some gymnastics with the error codes so that if something
     * went wrong, all processes report error, but if a process has a more
     * specific error code, we can still have that process report the
     * additional information */

    old_error = mpi_errno;
    if (mpi_errno != MPI_SUCCESS)
    {
        mpi_errno = MPI_ERR_IO;
    }

     /* optimization: if only one process performing i/o, we can perform
     * a less-expensive Bcast  */
    if (fd->hints->cb_nodes == 1)
    {
            NMPI_Bcast(&mpi_errno, 1, MPI_INT,
                            fd->hints->ranklist[0], fd->comm);
    }
    else
    {
            tmp_error = mpi_errno;
            NMPI_Allreduce(&tmp_error, &mpi_errno, 1, MPI_INT,
                            MPI_MAX, fd->comm);
    }

    if ( (old_error != MPI_SUCCESS) && (old_error != MPI_ERR_IO) )
    {
        mpi_errno = old_error;
    }


    if (!buftype_is_contig)
    {
        ADIOI_Delete_flattened(datatype);
    }

/* free all memory allocated for collective I/O */

    i = nprocs;
    do
    {
        --i;
        if (others_req[i].count != 0)
        {
            ADIOI_Free(others_req[i].offsets);
            ADIOI_Free(others_req[i].lens);
            ADIOI_Free(others_req[i].mem_ptrs);
        }
    } while (i > 0);
    ADIOI_Free(others_req);

    ADIOI_Free(buf_idx);
    ADIOI_Free(offset_list);
    ADIOI_Free(len_list);
    ADIOI_Free(st_offsets);
    ADIOI_Free(end_offsets);
    ADIOI_Free(fd_start);
    ADIOI_Free(fd_end);

/* This is a temporary way of filling in status. The right way is to
   keep track of how much data was actually written during collective I/O. */
    if (status)
    {
      int bufsize, size;
      /* Don't set status if it isn't needed */
      NMPI_Type_size(datatype, &size);
      bufsize = size * count;
      MPIR_Status_set_bytes(status, datatype, bufsize);
    }

    return mpi_errno;
}



/* If successful, mpi_errno is set to MPI_SUCCESS.  Otherwise an error
 * code is created and returned in mpi_errno.
 */
static int ADIOI_Exch_and_write(MPI_File fd, const void *buf, MPI_Datatype
                                 datatype, int nprocs,
                                 int myrank,
                                 ADIOI_Access
                                 *others_req, const MPI_Offset *offset_list,
                                 const int *len_list, int contig_access_count,
                                 MPI_Offset
                                 min_st_offset, MPI_Offset fd_size,
                                 const MPI_Offset *fd_start, const MPI_Offset *fd_end,
                                 _Out_cap_(nprocs) int *buf_idx)
{
/* Send data to appropriate processes and write in sizes of no more
   than coll_bufsize.
   The idea is to reduce the amount of extra memory required for
   collective I/O. If all data were written all at once, which is much
   easier, it would require temp space more than the size of user_buf,
   which is often unacceptable. For example, to write a distributed
   array to a file, where each local array is 8Mbytes, requiring
   at least another 8Mbytes of temp space is unacceptable. */

    bool hole;
    int i, j, m, size=0, ntimes, max_ntimes, buftype_is_contig;
    MPI_Offset st_loc=-1, end_loc=-1, off, done, req_off;
    char *write_buf=NULL;
    int *curr_offlen_ptr, *count, *send_size, req_len, *recv_size;
    int *partial_recv, *sent_to_proc, *start_pos, flag;
    int *send_buf_idx, *curr_to_proc, *done_to_proc;
    MPI_Status status;
    const ADIOI_Flatlist_node *flat_buf=NULL;
    MPI_Aint buftype_extent, buftype_lb;
    int info_flag, coll_bufsize;
    char *value;
    int mpi_errno = MPI_SUCCESS;

    /* only I/O errors are currently reported */

/* calculate the number of writes of size coll_bufsize
   to be done by each process and the max among all processes.
   That gives the no. of communication phases as well. */

    value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));

    NMPI_Info_get(fd->info, const_cast<char*>("cb_buffer_size"), MPI_MAX_INFO_VAL, value,
                 &info_flag);
    coll_bufsize = atoi(value);
    ADIOI_Free(value);


    for (i = 0; i < nprocs; i++)
    {
        if (others_req[i].count)
        {
            st_loc = others_req[i].offsets[0];
            end_loc = others_req[i].offsets[0];
            break;
        }
    }

    for (i = 0; i < nprocs; i++)
    {
        for (j = 0; j < others_req[i].count; j++)
        {
            if (st_loc > others_req[i].offsets[j])
            {
                st_loc = others_req[i].offsets[j];
            }
            if (end_loc < (others_req[i].offsets[j] + others_req[i].lens[j] - 1))
            {
                end_loc = others_req[i].offsets[j] + others_req[i].lens[j] - 1;
            }
        }
    }

/* ntimes=ceiling_div(end_loc - st_loc + 1, coll_bufsize)*/

    ntimes = (int) ((end_loc - st_loc + coll_bufsize)/coll_bufsize);

    if ((st_loc==-1) && (end_loc==-1))
    {
        ntimes = 0; /* this process does no writing. */
    }

    NMPI_Allreduce(&ntimes, &max_ntimes, 1, MPI_INT, MPI_MAX,
                  fd->comm);

    if (ntimes)
    {
        write_buf = (char *) ADIOI_Malloc(coll_bufsize);
    }

    curr_offlen_ptr = (int *) ADIOI_Calloc(nprocs, sizeof(int));
    /* its use is explained below. calloc initializes to 0. */

    count = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    /* to store count of how many off-len pairs per proc are satisfied
       in an iteration. */

    partial_recv = (int *) ADIOI_Calloc(nprocs, sizeof(int));
    /* if only a portion of the last off-len pair is recd. from a process
       in a particular iteration, the length recd. is stored here.
       calloc initializes to 0. */

    send_size = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    /* total size of data to be sent to each proc. in an iteration.
       Of size nprocs so that I can use MPI_Alltoall later. */

    recv_size = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    /* total size of data to be recd. from each proc. in an iteration.*/

    sent_to_proc = (int *) ADIOI_Calloc(nprocs, sizeof(int));
    /* amount of data sent to each proc so far. Used in
       ADIOI_Fill_send_buffer. initialized to 0 here. */

    send_buf_idx = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    curr_to_proc = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    done_to_proc = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    /* Above three are used in ADIOI_Fill_send_buffer*/

    start_pos = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    /* used to store the starting value of curr_offlen_ptr[i] in
       this iteration */

    ADIOI_Datatype_iscontig(datatype, &buftype_is_contig);
    if (!buftype_is_contig)
    {
        ADIOI_Flatten_datatype(datatype);
        flat_buf = ADIOI_Flatlist;
        while (flat_buf->type != datatype)
        {
            flat_buf = flat_buf->next;
        }
    }
    NMPI_Type_get_extent(datatype, &buftype_lb, &buftype_extent);


/* I need to check if there are any outstanding nonblocking writes to
   the file, which could potentially interfere with the writes taking
   place in this collective write call. Since this is not likely to be
   common, let me do the simplest thing possible here: Each process
   completes all pending nonblocking operations before completing. */

    /*ADIOI_Complete_async(mpi_errno);
    if (*mpi_errno != MPI_SUCCESS) return;
    MPI_Barrier(fd->comm);
    */

    done = 0;
    off = st_loc;

    for (m=0; m < ntimes; m++)
    {
       /* go through all others_req and check which will be satisfied
          by the current write */

       /* Note that MPI guarantees that displacements in filetypes are in
          monotonically nondecreasing order and that, for writes, the
          filetypes cannot specify overlapping regions in the file. This
          simplifies implementation a bit compared to reads. */

          /* off = start offset in the file for the data to be written in
                   this iteration
             size = size of data written (bytes) corresponding to off
             req_off = off in file for a particular contiguous request
                       minus what was satisfied in previous iteration
             req_size = size corresponding to req_off */

        /* first calculate what should be communicated */

        i = nprocs;
        do
        {
            --i;
            count[i] = recv_size[i] = 0;
        } while (i > 0);

        size = (int) (min(coll_bufsize, end_loc-st_loc+1-done));

        for (i=0; i < nprocs; i++)
        {
            if (others_req[i].count)
            {
                start_pos[i] = curr_offlen_ptr[i];
                for (j=curr_offlen_ptr[i]; j<others_req[i].count; j++)
                {
                    if (partial_recv[i])
                    {
                        /* this request may have been partially
                           satisfied in the previous iteration. */
                        req_off = others_req[i].offsets[j] +
                            partial_recv[i];
                        req_len = others_req[i].lens[j] -
                            partial_recv[i];
                        partial_recv[i] = 0;
                        /* modify the off-len pair to reflect this change */
                        others_req[i].offsets[j] = req_off;
                        others_req[i].lens[j] = req_len;
                    }
                    else
                    {
                        req_off = others_req[i].offsets[j];
                        req_len = others_req[i].lens[j];
                    }
                    if (req_off < off + size)
                    {
                        count[i]++;
                        NMPI_Get_address(write_buf+req_off-off,
                               &(others_req[i].mem_ptrs[j]));
                        recv_size[i] += (int)(min(off + (MPI_Offset)size -
                                                  req_off, req_len));

                        if (off+size-req_off < req_len)
                        {
                            partial_recv[i] = (int) (off + size - req_off);

                            if ((j+1 < others_req[i].count) &&
                                 (others_req[i].offsets[j+1] < off+size))
                            {
                                mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**ioverlapping");
                                /* allow to continue since additional
                                 * communication might have to occur
                                 */
                            }
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                curr_offlen_ptr[i] = j;
            }
        }

        ADIOI_W_Exchange_data(fd, buf, write_buf, flat_buf, offset_list,
                            len_list, send_size, recv_size, off, size, count,
                            start_pos, partial_recv,
                            sent_to_proc, nprocs, myrank,
                            buftype_is_contig, contig_access_count,
                            min_st_offset, fd_size, fd_start, fd_end,
                            others_req, send_buf_idx, curr_to_proc,
                            done_to_proc, &hole, m, buftype_extent, buf_idx,
                            &mpi_errno);
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }

        flag = 0;
        for (i=0; i<nprocs; i++)
        {
            if (count[i])
            {
                flag = 1;
            }
        }

        if (flag)
        {
            mpi_errno = ADIO_WriteContig(fd, write_buf, size, MPI_BYTE, ADIO_EXPLICIT_OFFSET,
                        off, &status);
            if (mpi_errno != MPI_SUCCESS)
                return mpi_errno;
        }

        off += size;
        done += size;
    }

    i = nprocs;
    do
    {
        --i;
        count[i] = recv_size[i] = 0;
    } while (i > 0);

    for (m=ntimes; m<max_ntimes; m++)
    {
        /* nothing to recv, but check for send. */
        ADIOI_W_Exchange_data(fd, buf, write_buf, flat_buf, offset_list,
                            len_list, send_size, recv_size, off, size, count,
                            start_pos, partial_recv,
                            sent_to_proc, nprocs, myrank,
                            buftype_is_contig, contig_access_count,
                            min_st_offset, fd_size, fd_start, fd_end,
                            others_req, send_buf_idx,
                            curr_to_proc, done_to_proc, &hole, m,
                            buftype_extent, buf_idx, &mpi_errno);
    }

    if (ntimes)
    {
        ADIOI_Free(write_buf);
    }
    ADIOI_Free(curr_offlen_ptr);
    ADIOI_Free(count);
    ADIOI_Free(partial_recv);
    ADIOI_Free(send_size);
    ADIOI_Free(recv_size);
    ADIOI_Free(sent_to_proc);
    ADIOI_Free(start_pos);
    ADIOI_Free(send_buf_idx);
    ADIOI_Free(curr_to_proc);
    ADIOI_Free(done_to_proc);
    return mpi_errno;
}


/* Sets mpi_errno to MPI_SUCCESS if successful, or creates an error code
 * in the case of error.
 */
_Success_( *mpi_errno == MPI_SUCCESS )
static void
ADIOI_W_Exchange_data(
    _In_ MPI_File fd,
    _In_ const void *buf,
    _Out_writes_bytes_opt_(size) char *write_buf,
    _In_opt_ const ADIOI_Flatlist_node *flat_buf,
    _In_reads_(contig_access_count) const MPI_Offset *offset_list,
    _In_reads_(contig_access_count) const int *len_list,
    _Out_writes_(nprocs) int *send_size,
    _In_reads_(nprocs) const int *recv_size,
    _In_ MPI_Offset off,
    _In_ int size,
    _In_reads_(nprocs) const int *count,
    _In_reads_(nprocs) const int *start_pos,
    _In_reads_(nprocs) const int *partial_recv,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int *sent_to_proc,
    _In_range_(>,0) int nprocs,
    _In_range_(>=,0) int myrank,
    _In_range_(0,1) int buftype_is_contig,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset *fd_start,
    _In_ const MPI_Offset *fd_end,
    _Inout_updates_(nprocs) ADIOI_Access *others_req,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int* send_buf_idx,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int* curr_to_proc,
    _When_(buftype_is_contig == 0, _Out_writes_(nprocs)) int* done_to_proc,
    _Out_ bool* hole,
    _In_ int iter,
    _In_ MPI_Aint buftype_extent,
    _When_(buftype_is_contig == 1, _Out_writes_(nprocs)) int *buf_idx,
    _Out_ int* mpi_errno
    )
{
    int i, j, k, *tmp_len, nprocs_recv, nprocs_send, err;
    char **send_buf = NULL;
    MPI_Request *requests, *send_req;
    MPI_Datatype *recv_types;
    MPI_Status *statuses, status;
    int *srt_len, sum;
    MPI_Offset *srt_off;

/* exchange recv_size info so that each process knows how much to
   send to whom. */

    NMPI_Alltoall(recv_size, 1, MPI_INT, send_size, 1, MPI_INT, fd->comm);

    /* create derived datatypes for recv */

    i = nprocs;
    nprocs_recv = nprocs;
    do
    {
        --i;
        if (recv_size[i] == 0)
        {
            --nprocs_recv;
        }
    } while (i > 0);

    recv_types = (MPI_Datatype *) ADIOI_Malloc((nprocs_recv+1)*sizeof(MPI_Datatype));
    /* +1 to avoid a 0-size malloc */

    tmp_len = (int *) ADIOI_Malloc(nprocs*sizeof(int));
    j = 0;
    for (i = 0; j < nprocs_recv; i++)
    {
        if (recv_size[i])
        {
            /* take care if the last off-len pair is a partial recv */
            if (partial_recv[i])
            {
                k = start_pos[i] + count[i] - 1;
                tmp_len[i] = others_req[i].lens[k];
                others_req[i].lens[k] = partial_recv[i];
            }
            NMPI_Type_create_hindexed(count[i],
                 &(others_req[i].lens[start_pos[i]]),
                     &(others_req[i].mem_ptrs[start_pos[i]]),
                         MPI_BYTE, recv_types+j);
            /* absolute displacements; use MPI_BOTTOM in recv */
            NMPI_Type_commit(recv_types+j);
            j++;
        }
    }

    /* To avoid a read-modify-write, check if there are holes in the
       data to be written. For this, merge the (sorted) offset lists
       others_req using a heap-merge. */

    sum = 0;
    for (i=0; i<nprocs; i++)
    {
        sum += count[i];
    }

    if(sum > 0)
    {
        srt_off = (MPI_Offset *) ADIOI_Malloc((sum+1)*sizeof(MPI_Offset));
        srt_len = (int *) ADIOI_Malloc(sum*sizeof(int));

        ADIOI_Heap_merge(others_req, count, srt_off, srt_len, start_pos,
                         nprocs, nprocs_recv, sum);
    }
    else
    {
        srt_off = NULL;
        srt_len = NULL;
    }

/* for partial recvs, restore original lengths */
    for (i=0; i<nprocs; i++)
    {
        if (partial_recv[i])
        {
            k = start_pos[i] + count[i] - 1;
            others_req[i].lens[k] = tmp_len[i];
        }
    }
    ADIOI_Free(tmp_len);

    /* check if there are any holes If yes, must do read-modify-write.
     * holes can be in three places.  'middle' is what you'd expect: the
     * processes are operating on noncontiguous data.  But holes can also show
     * up at the beginning or end of the file domain (see John Bent ROMIO REQ
     * #835). Missing these holes would result in us writing more data than
     * received by everyone else. */

    *hole = false;
    if(srt_off != NULL && srt_len != NULL) // implied: sum > 0
    {
        for (i=0; i < sum - 1; i++)
        {
            if (srt_off[i]+srt_len[i] < srt_off[i+1])
            {
                *hole = true;
                break;
            }
        }
    /* In some cases (see John Bent ROMIO REQ # 835), an odd interaction
     * between aggregation, nominally contiguous regions, and cb_buffer_size
     * should be handled with a read-modify-write (otherwise we will write out
     * more data than we receive from everyone else (inclusive), so override
     * hole detection
     */
        if(off != srt_off[0]) /* hole at the front */
        {
            *hole = true;
        }
        else
        {
            /* coalesce the sorted offset-length pairs */
            for(i = 1; i < sum; i++)
            {
                if(srt_off[i] <= srt_off[0] + srt_len[0])
                {
                    int new_len = static_cast<int>(srt_off[i] + srt_len[i] - srt_off[0]);
                    if(new_len > srt_len[0])
                    {
                        srt_len[0] = new_len;
                    }
                }
                else
                {
                    break;
                }
            }
            if(i < sum || size != srt_len[0])
            {
                *hole = true;
            }
        }
        ADIOI_Free(srt_off);
        ADIOI_Free(srt_len);
    }

    if (nprocs_recv != 0)
    {
        if (*hole)
        {
            err = ADIO_ReadContig(fd, write_buf, size, MPI_BYTE,
                            ADIO_EXPLICIT_OFFSET, off, &status);
            if (err != MPI_SUCCESS)
            {
                *mpi_errno = MPIU_ERR_FAIL(err);
                return;
            }
        }
    }

    i = nprocs_send = nprocs;
    do
    {
        --i;
        if (send_size[i] == 0)
        {
            --nprocs_send;
        }
    } while (i > 0);

    if (fd->atomicity)
    {
        /* bug fix from Wei-keng Liao and Kenin Coloma */
        requests = (MPI_Request *)
            ADIOI_Malloc((nprocs_send+1)*sizeof(MPI_Request));
        send_req = requests;
    }
    else
    {
        requests = (MPI_Request *)
            ADIOI_Malloc((nprocs_send+nprocs_recv+1)*sizeof(MPI_Request));
        /* +1 to avoid a 0-size malloc */

        /* post receives */
        j = 0;
        for (i=0; i<nprocs; i++)
        {
            if (recv_size[i])
            {
                NMPI_Irecv(MPI_BOTTOM, 1, recv_types[j], i, myrank+i+100*iter,
                          fd->comm, requests+j);
                j++;
            }
        }
        send_req = requests + nprocs_recv;
    }

/* post sends. if buftype_is_contig, data can be directly sent from
   user buf at location given by buf_idx. else use send_buf. */

    if (buftype_is_contig)
    {
        j = 0;
        for (i=0; i < nprocs; i++)
        {
            if (send_size[i])
            {
                NMPI_Isend(((char *) buf) + buf_idx[i], send_size[i],
                            MPI_BYTE, i,  myrank+i+100*iter, fd->comm,
                                  send_req+j);
                j++;
                buf_idx[i] += send_size[i];
            }
        }
    }
    else if (nprocs_send)
    {
        /* buftype is not contig */
        send_buf = (char **) ADIOI_Malloc(nprocs*sizeof(char*));
        for (i=0; i < nprocs; i++)
        {
            if (send_size[i])
            {
                send_buf[i] = (char *) ADIOI_Malloc(send_size[i]);
            }
        }

        ADIOI_Fill_send_buffer(fd, buf, flat_buf, send_buf,
                           offset_list, len_list, send_size,
                           send_req,
                           sent_to_proc, nprocs, myrank,
                           contig_access_count,
                           min_st_offset, fd_size, fd_start, fd_end,
                           send_buf_idx, curr_to_proc, done_to_proc, iter,
                           buftype_extent);
        /* the send is done in ADIOI_Fill_send_buffer */
    }

    if (fd->atomicity)
    {
        /* bug fix from Wei-keng Liao and Kenin Coloma */
        j = 0;
        for (i=0; i<nprocs; i++)
        {
            MPI_Status wkl_status;
            if (recv_size[i])
            {
                NMPI_Recv(MPI_BOTTOM, 1, recv_types[j], i, myrank+i+100*iter,
                          fd->comm, &wkl_status);
                j++;
            }
        }
    }

    for (i=0; i<nprocs_recv; i++)
    {
        NMPI_Type_free(recv_types+i);
    }
    ADIOI_Free(recv_types);

    if (fd->atomicity)
    {
        /* bug fix from Wei-keng Liao and Kenin Coloma */
        statuses = (MPI_Status *) ADIOI_Malloc((nprocs_send+1) * \
                                         sizeof(MPI_Status));
         /* +1 to avoid a 0-size malloc */
    }
    else
    {
        statuses = (MPI_Status *) ADIOI_Malloc((nprocs_send+nprocs_recv+1) * \
                                     sizeof(MPI_Status));
        /* +1 to avoid a 0-size malloc */
    }

    if (fd->atomicity)
    {
        /* bug fix from Wei-keng Liao and Kenin Coloma */
        NMPI_Waitall(nprocs_send, send_req, statuses);
    }
    else
    {
        NMPI_Waitall(nprocs_send+nprocs_recv, requests, statuses);
    }

    ADIOI_Free(statuses);
    ADIOI_Free(requests);
    if (!buftype_is_contig && nprocs_send)
    {
        i = nprocs;
        do
        {
            --i;
            if (send_size[i])
            {
                ADIOI_Free(send_buf[i]);
            }
        } while (i > 0);
        ADIOI_Free(send_buf);
    }

    *mpi_errno = MPI_SUCCESS;
}


#define ADIOI_BUF_INCR \
{ \
    while (buf_incr) { \
        if (buf_incr < flat_buf_sz) \
        { \
            size_in_buf = buf_incr; \
            user_buf_idx += size_in_buf; \
            flat_buf_sz -= size_in_buf; \
        } \
        else \
        { \
            size_in_buf = flat_buf_sz; \
            ++flat_buf_idx; \
            if (flat_buf_idx >= flat_buf->count) \
            { \
                flat_buf_idx = 0; \
                n_buftypes++; \
            } \
            user_buf_idx = flat_buf->indices[flat_buf_idx] + \
                              n_buftypes*buftype_extent; \
            flat_buf_sz = flat_buf->blocklens[flat_buf_idx]; \
        } \
        buf_incr -= size_in_buf; \
    } \
}


#define ADIOI_BUF_COPY \
{ \
    while (size) { \
        size_in_buf = min(size, flat_buf_sz); \
        memcpy(&(send_buf[p][send_buf_idx[p]]), \
               ((char *) buf) + user_buf_idx, size_in_buf); \
        send_buf_idx[p] += size_in_buf; \
        user_buf_idx += size_in_buf; \
        flat_buf_sz -= size_in_buf; \
        if (!flat_buf_sz) { \
            ++flat_buf_idx; \
            if (flat_buf_idx >= flat_buf->count) \
            { \
                flat_buf_idx = 0; \
                n_buftypes++; \
            } \
            user_buf_idx = flat_buf->indices[flat_buf_idx] + \
                              n_buftypes*buftype_extent; \
            flat_buf_sz = flat_buf->blocklens[flat_buf_idx]; \
        } \
        size -= size_in_buf; \
        buf_incr -= size_in_buf; \
    } \
    ADIOI_BUF_INCR \
}


static void
ADIOI_Fill_send_buffer(
    _In_ MPI_File fd,
    _In_ const void* buf,
    _In_ const ADIOI_Flatlist_node* flat_buf,
    _When_(contig_access_count > 0, _Out_) char** send_buf,
    _In_reads_(contig_access_count) const MPI_Offset* offset_list,
    _In_reads_(contig_access_count) const int* len_list,
    _In_ const int* send_size,
    _When_(contig_access_count > 0, _Out_) MPI_Request* requests,
    _Out_writes_(nprocs) int* sent_to_proc,
    _In_range_(>,0) int nprocs,
    _In_range_(>=,0) int myrank,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset* fd_start,
    _In_ const MPI_Offset* fd_end,
    _Out_writes_(nprocs) int* send_buf_idx,
    _Out_writes_(nprocs) int* curr_to_proc,
    _Out_writes_(nprocs) int* done_to_proc,
    _In_ int iter,
    _In_ MPI_Aint buftype_extent
    )
{
/* this function is only called if buftype is not contig */

    OACR_USE_PTR( requests );
    OACR_USE_PTR( send_buf );

    int i, p, flat_buf_idx, size;
    int flat_buf_sz, buf_incr, size_in_buf, jj, n_buftypes;
    MPI_Offset off, len, rem_len, user_buf_idx;

/*  curr_to_proc[p] = amount of data sent to proc. p that has already
    been accounted for so far
    done_to_proc[p] = amount of data already sent to proc. p in
    previous iterations
    user_buf_idx = current location in user buffer
    send_buf_idx[p] = current location in send_buf of proc. p  */

    RtlZeroMemory(send_buf_idx, nprocs * sizeof(int));
    RtlZeroMemory(curr_to_proc, nprocs * sizeof(int));
    RtlCopyMemory(done_to_proc, sent_to_proc, nprocs * sizeof(int));

    jj = 0;

    user_buf_idx = flat_buf->indices[0];
    flat_buf_idx = 0;
    n_buftypes = 0;
    flat_buf_sz = flat_buf->blocklens[0];

    /* flat_buf_idx = current index into flattened buftype
       flat_buf_sz = size of current contiguous component in
                         flattened buf */

    for (i=0; i<contig_access_count; i++)
    {
        off     = offset_list[i];
        rem_len = (MPI_Offset) len_list[i];

        /*this request may span the file domains of more than one process*/
        while (rem_len != 0)
        {
            len = rem_len;
            /* NOTE: len value is modified by ADIOI_Calc_aggregator() to be no
             * longer than the single region that processor "p" is responsible
             * for.
             */
            p = ADIOI_Calc_aggregator(fd,
                                      off,
                                      min_st_offset,
                                      &len,
                                      fd_size,
                                      fd_start,
                                      fd_end);

            if (send_buf_idx[p] < send_size[p])
            {
                if (curr_to_proc[p]+len > done_to_proc[p])
                {
                    if (done_to_proc[p] > curr_to_proc[p])
                    {
                        size = (int)min(curr_to_proc[p] + len -
                                done_to_proc[p], send_size[p]-send_buf_idx[p]);
                        buf_incr = done_to_proc[p] - curr_to_proc[p];
                        ADIOI_BUF_INCR
                        buf_incr = (int)(curr_to_proc[p] + len - done_to_proc[p]);
                        curr_to_proc[p] = done_to_proc[p] + size;
                        ADIOI_BUF_COPY
                    }
                    else
                    {
                        size = (int)min(len,send_size[p]-send_buf_idx[p]);
                        buf_incr = (int)len;
                        curr_to_proc[p] += size;
                        ADIOI_BUF_COPY
                    }
                    if (send_buf_idx[p] == send_size[p])
                    {
                        NMPI_Isend(send_buf[p], send_size[p], MPI_BYTE, p,
                                myrank+p+100*iter, fd->comm, requests+jj);
                        jj++;
                    }
                }
                else
                {
                    curr_to_proc[p] += (int)len;
                    buf_incr = (int)len;
                    ADIOI_BUF_INCR
                }
            }
            else
            {
                buf_incr = (int)len;
                ADIOI_BUF_INCR
            }
            off     += len;
            rem_len -= len;
        }
    }
    i = nprocs;
    do
    {
        --i;
        if (send_size[i])
        {
            sent_to_proc[i] = curr_to_proc[i];
        }
    } while (i > 0);
}



static void ADIOI_Heap_merge(ADIOI_Access *others_req, const int *count,
                      _Out_cap_(total_elements) MPI_Offset *srt_off, _Out_cap_(total_elements) int *srt_len, const int *start_pos,
                      int nprocs, int nprocs_recv, int total_elements)
{
    typedef struct
    {
        MPI_Offset *off_list;
        int *len_list;
        int nelem;
    } heap_struct;

    heap_struct *a, tmp;
    int i, j, heapsize, l, r, k, smallest;

    a = (heap_struct *) ADIOI_Malloc((nprocs_recv+1)*sizeof(heap_struct));

    j = 0;
    for (i=0; i<nprocs; i++)
    {
        if (count[i])
        {
            a[j].off_list = &(others_req[i].offsets[start_pos[i]]);
            a[j].len_list = &(others_req[i].lens[start_pos[i]]);
            a[j].nelem = count[i];
            j++;
        }
    }

    /* build a heap out of the first element from each list, with
       the smallest element of the heap at the root */

    heapsize = nprocs_recv;
    for (i=heapsize/2 - 1; i>=0; i--)
    {
        /* Heapify(a, i, heapsize); Algorithm from Cormen et al. pg. 143
           modified for a heap with smallest element at root. I have
           removed the recursion so that there are no function calls.
           Function calls are too expensive. */
        k = i;
        for(;;)
        {
            r = 2*(k+1);
            l = r - 1;

            if ((l < heapsize) &&
                (*(a[l].off_list) < *(a[k].off_list)))
            {
                smallest = l;
            }
            else
            {
                smallest = k;
            }

            if ((r < heapsize) &&
                (*(a[r].off_list) < *(a[smallest].off_list)))
            {
                smallest = r;
            }

            if (smallest != k)
            {
                tmp.off_list = a[k].off_list;
                tmp.len_list = a[k].len_list;
                tmp.nelem = a[k].nelem;

                a[k].off_list = a[smallest].off_list;
                a[k].len_list = a[smallest].len_list;
                a[k].nelem = a[smallest].nelem;

                a[smallest].off_list = tmp.off_list;
                a[smallest].len_list = tmp.len_list;
                a[smallest].nelem = tmp.nelem;

                k = smallest;
            }
            else break;
        }
    }

    for (i=0; i<total_elements; i++)
    {
        /* extract smallest element from heap, i.e. the root */
        srt_off[i] = *(a[0].off_list);
        srt_len[i] = *(a[0].len_list);
        (a[0].nelem)--;

        if (!a[0].nelem)
        {
            a[0].off_list = a[heapsize-1].off_list;
            a[0].len_list = a[heapsize-1].len_list;
            a[0].nelem = a[heapsize-1].nelem;
            heapsize--;
        }
        else
        {
            (a[0].off_list)++;
            (a[0].len_list)++;
        }

        /* Heapify(a, 0, heapsize); */
        k = 0;
        for (;;)
        {
            r = 2*(k+1);
            l = r - 1;

            if ((l < heapsize) &&
                (*(a[l].off_list) < *(a[k].off_list)))
            {
                smallest = l;
            }
            else
            {
                smallest = k;
            }

            if ((r < heapsize) &&
                (*(a[r].off_list) < *(a[smallest].off_list)))
            {
                smallest = r;
            }

            if (smallest != k)
            {
                tmp.off_list = a[k].off_list;
                tmp.len_list = a[k].len_list;
                tmp.nelem = a[k].nelem;

                a[k].off_list = a[smallest].off_list;
                a[k].len_list = a[smallest].len_list;
                a[k].nelem = a[smallest].nelem;

                a[smallest].off_list = tmp.off_list;
                a[smallest].len_list = tmp.len_list;
                a[smallest].nelem = tmp.nelem;

                k = smallest;
            }
            else break;
        }
    }
    ADIOI_Free(a);
}


#define ADIOI_BUFFERED_WRITE \
{ \
    if (req_off >= writebuf_off + writebuf_len) { \
        if (writebuf_len) { \
           mpi_errno = ADIO_WriteContig(fd, writebuf, writebuf_len, MPI_BYTE, \
                  ADIO_EXPLICIT_OFFSET, writebuf_off, &status1); \
           if (!(fd->atomicity)) ADIOI_UNLOCK(fd, writebuf_off, writebuf_len); \
           if (mpi_errno != MPI_SUCCESS) { \
               return MPIU_ERR_FAIL(mpi_errno); \
           } \
        } \
        writebuf_off = req_off; \
        writebuf_len = (int) (min(max_bufsize,end_offset-writebuf_off+1));\
        if (!(fd->atomicity)) ADIOI_WRITE_LOCK(fd, writebuf_off, writebuf_len); \
        mpi_errno = ADIO_ReadContig(fd, writebuf, writebuf_len, MPI_BYTE, \
                 ADIO_EXPLICIT_OFFSET, writebuf_off, &status1); \
        if (mpi_errno != MPI_SUCCESS) { \
            return MPIU_ERR_FAIL(mpi_errno); \
        } \
    } \
    write_sz = (int) (min(req_len, writebuf_off + writebuf_len - req_off)); \
    memcpy(writebuf+req_off-writebuf_off, (char *)buf +userbuf_off, write_sz);\
    while (write_sz != req_len) { \
        mpi_errno = ADIO_WriteContig(fd, writebuf, writebuf_len, MPI_BYTE, \
                  ADIO_EXPLICIT_OFFSET, writebuf_off, &status1); \
        if (!(fd->atomicity)) ADIOI_UNLOCK(fd, writebuf_off, writebuf_len); \
        if (mpi_errno != MPI_SUCCESS) { \
            return MPIU_ERR_FAIL(mpi_errno); \
        } \
        req_len -= write_sz; \
        userbuf_off += write_sz; \
        writebuf_off += writebuf_len; \
        writebuf_len = (int) (min(max_bufsize,end_offset-writebuf_off+1));\
        if (!(fd->atomicity)) ADIOI_WRITE_LOCK(fd, writebuf_off, writebuf_len); \
        mpi_errno = ADIO_ReadContig(fd, writebuf, writebuf_len, MPI_BYTE, \
                  ADIO_EXPLICIT_OFFSET, writebuf_off, &status1); \
        if (mpi_errno != MPI_SUCCESS) { \
            return MPIU_ERR_FAIL(mpi_errno); \
        } \
        write_sz = min(req_len, writebuf_len); \
        memcpy(writebuf, (char *)buf + userbuf_off, write_sz);\
    } \
}


/* this macro is used when filetype is contig and buftype is not contig.
   it does not do a read-modify-write and does not lock*/
#define ADIOI_BUFFERED_WRITE_WITHOUT_READ \
{ \
    if (req_off >= writebuf_off + writebuf_len) { \
        mpi_errno = ADIO_WriteContig(fd, writebuf, writebuf_len, MPI_BYTE, \
                 ADIO_EXPLICIT_OFFSET, writebuf_off, &status1); \
        if (mpi_errno != MPI_SUCCESS) { \
            return MPIU_ERR_FAIL(mpi_errno); \
        } \
        writebuf_off = req_off; \
        writebuf_len = (int) (min(max_bufsize,end_offset-writebuf_off+1));\
    } \
    write_sz = (int) (min(req_len, writebuf_off + writebuf_len - req_off)); \
    memcpy(writebuf+req_off-writebuf_off, (char *)buf +userbuf_off, write_sz);\
    while (write_sz != req_len) { \
        mpi_errno = ADIO_WriteContig(fd, writebuf, writebuf_len, MPI_BYTE, \
                ADIO_EXPLICIT_OFFSET, writebuf_off, &status1); \
        if (mpi_errno != MPI_SUCCESS) { \
            return MPIU_ERR_FAIL(mpi_errno); \
        } \
        req_len -= write_sz; \
        userbuf_off += write_sz; \
        writebuf_off += writebuf_len; \
        writebuf_len = (int) (min(max_bufsize,end_offset-writebuf_off+1));\
        write_sz = min(req_len, writebuf_len); \
        memcpy(writebuf, (char *)buf + userbuf_off, write_sz);\
    } \
}


int
ADIOI_GEN_WriteStrided(MPI_File fd, const void *buf, int count,
                       MPI_Datatype datatype, int file_ptr_type,
                       MPI_Offset offset, MPI_Status *status
                       )
{
    /* offset is in units of etype relative to the filetype. */

    const ADIOI_Flatlist_node *flat_buf, *flat_file;
    int i, j, k, bwr_size, fwr_size=0, st_index=0;
    int bufsize, num, size, sum, n_etypes_in_filetype, size_in_filetype;
    int n_filetypes, etype_in_filetype;
    MPI_Offset abs_off_in_filetype=0;
    int filetype_size, etype_size, buftype_size, req_len;
    MPI_Aint filetype_extent, filetype_lb, buftype_extent, buftype_lb;
    int buf_count, buftype_is_contig, filetype_is_contig;
    MPI_Offset userbuf_off;
    MPI_Offset off, req_off, disp, end_offset=0, writebuf_off, start_off;
    char *writebuf;
    int st_fwr_size, st_n_filetypes, writebuf_len, write_sz;
    MPI_Status status1;
    int new_bwr_size, new_fwr_size, max_bufsize;
    int mpi_errno;

    if (fd->hints->ds_write == ADIOI_HINT_DISABLE)
    {
        /* if user has disabled data sieving on reads, use naive
         * approach instead.
         */
        return ADIOI_GEN_WriteStrided_naive(fd,
                                    buf,
                                    count,
                                    datatype,
                                    file_ptr_type,
                                    offset,
                                    status
                                    );
    }

    ADIOI_Datatype_iscontig(datatype, &buftype_is_contig);
    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);

    NMPI_Type_size(fd->filetype, &filetype_size);
    if ( ! filetype_size )
    {
        return MPI_SUCCESS;
    }

    NMPI_Type_get_extent(fd->filetype, &filetype_lb, &filetype_extent);
    NMPI_Type_size(datatype, &buftype_size);
    NMPI_Type_get_extent(datatype, &buftype_lb, &buftype_extent);
    etype_size = fd->etype_size;

    bufsize = buftype_size * count;

    /* get max_bufsize from the info object. */

    max_bufsize = fd->hints->ind_wr_buffer_size;

    if (!buftype_is_contig && filetype_is_contig)
    {
        /* noncontiguous in memory, contiguous in file. */

        ADIOI_Flatten_datatype(datatype);
        flat_buf = ADIOI_Flatlist;
        while (flat_buf->type != datatype)
        {
            flat_buf = flat_buf->next;
        }

        off = (file_ptr_type == ADIO_INDIVIDUAL) ? fd->fp_ind :
                 fd->disp + etype_size * offset;

        start_off = off;
        end_offset = off + bufsize - 1;
        writebuf_off = off;
        writebuf = (char *) ADIOI_Malloc(max_bufsize);
        writebuf_len = (int) (min(max_bufsize, end_offset-writebuf_off+1));

        /* if atomicity is true, lock the region to be accessed */
        if (fd->atomicity)
        {
            ADIOI_WRITE_LOCK(fd, start_off, end_offset-start_off+1);
        }

        for (j=0; j<count; j++)
        {
            for (i=0; i<flat_buf->count; i++)
            {
                userbuf_off = j*buftype_extent + flat_buf->indices[i];
                req_off = off;
                req_len = flat_buf->blocklens[i];
                ADIOI_BUFFERED_WRITE_WITHOUT_READ
                off += flat_buf->blocklens[i];
            }
        }

        /* write the buffer out finally */
        mpi_errno = ADIO_WriteContig(fd, writebuf, writebuf_len, MPI_BYTE, ADIO_EXPLICIT_OFFSET,
                        writebuf_off, &status1);

        if (fd->atomicity)
        {
            ADIOI_UNLOCK(fd, start_off, end_offset-start_off+1);
        }

        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }

        ADIOI_Free(writebuf);

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            fd->fp_ind = off;
        }
    }
    else
    {  /* noncontiguous in file */

        /* filetype already flattened in ADIO_Open */

        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype)
        {
            flat_file = flat_file->next;
        }
        disp = fd->disp;

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            offset = fd->fp_ind - disp;
            n_filetypes = static_cast<int>((offset - flat_file->indices[0]) / filetype_extent);
            offset -= static_cast<MPI_Offset>(n_filetypes * filetype_extent);
            /* now offset is local to this extent */

            /* find the block where offset is located, skip blocklens[i]==0 */
            for(i = 0; i < flat_file->count; i++)
            {
                MPI_Offset dist;
                if(flat_file->blocklens[i] == 0)
                {
                    continue;
                }
                dist = flat_file->indices[i] + flat_file->blocklens[i] - offset;
                /* frd_size is from offset to the end of block i */
                if(dist == 0)
                {
                    i++;
                    offset = flat_file->indices[i];
                    fwr_size = flat_file->blocklens[i];
                    break;
                }
                if(dist > 0)
                {
                    fwr_size = (int) dist;
                    break;
                }
            }
            st_index = i; /* starting index in flat_file->indices[] */
            offset += disp + (MPI_Offset)n_filetypes*filetype_extent;
        }
        else
        {
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
                    st_index = i;
                    fwr_size = sum - size_in_filetype;
                    abs_off_in_filetype = flat_file->indices[i] +
                        size_in_filetype - (sum - flat_file->blocklens[i]);
                    break;
                }
            }

            /* abs. offset in bytes in the file */
            offset = disp + (MPI_Offset) n_filetypes*filetype_extent + abs_off_in_filetype;
        }

        start_off = offset;

        /* Wei-keng Liao:write request is within single flat_file contig block*/
        /* this could happen, for example, with subarray types that are
           actually fairly contiguous */
        if (buftype_is_contig && bufsize <= fwr_size)
        {
            mpi_errno = ADIO_WriteContig(fd, buf, bufsize, MPI_BYTE, ADIO_EXPLICIT_OFFSET,
                             offset, status);

            if (file_ptr_type == ADIO_INDIVIDUAL)
            {
                /* update MPI-IO file pointer to point to the first byte
                   that can be accessed in the fileview. */
                fd->fp_ind = offset + bufsize;
                if (bufsize == fwr_size)
                {
                    do
                    {
                        st_index++;
                        if (st_index == flat_file->count)
                        {
                            st_index = 0;
                            n_filetypes++;
                        }
                    } while (flat_file->blocklens[st_index] == 0);
                    fd->fp_ind = disp + flat_file->indices[st_index]
                               + static_cast<MPI_Offset>(n_filetypes*filetype_extent);
                }
            }
            MPIR_Status_set_bytes(status, datatype, bufsize);
            return mpi_errno;
        }

        /* Calculate end_offset, the last byte-offset that will be accessed.
         e.g., if start_offset=0 and 100 bytes to be write, end_offset=99*/

        st_fwr_size = fwr_size;
        st_n_filetypes = n_filetypes;
        i = 0;
        j = st_index;
        off = offset;
        fwr_size = min(st_fwr_size, bufsize);
        while (i < bufsize)
        {
            i += fwr_size;
            end_offset = off + fwr_size - 1;

            if (++j == flat_file->count)
            {
                j = 0;
                n_filetypes++;
            }

            off = disp + flat_file->indices[j] + (MPI_Offset) n_filetypes*filetype_extent;
            fwr_size = min(flat_file->blocklens[j], bufsize-i);
        }

        /* if atomicity is true, lock the region to be accessed */
        if (fd->atomicity)
        {
            ADIOI_WRITE_LOCK(fd, start_off, end_offset-start_off+1);
        }

        writebuf_off = 0;
        writebuf_len = 0;
        writebuf = (char *) ADIOI_Malloc(max_bufsize);
        memset(writebuf, -1, max_bufsize);

        if (buftype_is_contig && !filetype_is_contig)
        {
            /* contiguous in memory, noncontiguous in file. should be the most
               common case. */

            i = 0;
            j = st_index;
            off = offset;
            n_filetypes = st_n_filetypes;
            fwr_size = min(st_fwr_size, bufsize);
            while (i < bufsize)
            {
                if (fwr_size)
                {
                    req_off = off;
                    req_len = fwr_size;
                    userbuf_off = i;
                    ADIOI_BUFFERED_WRITE
                }
                i += fwr_size;

                if (off + fwr_size < disp + flat_file->indices[j] +
                   flat_file->blocklens[j] + (MPI_Offset) n_filetypes*filetype_extent)
                {
                       off += fwr_size;
                }
                /* did not reach end of contiguous block in filetype.
                   no more I/O needed. off is incremented by fwr_size. */
                else
                {
                    do
                    {
                        if (++j == flat_file->count)
                        {
                            j = 0;
                            n_filetypes++;
                        }
                    } while (flat_file->blocklens[j]==0);
                    off = disp + flat_file->indices[j] +
                                        (MPI_Offset) n_filetypes*filetype_extent;
                    fwr_size = min(flat_file->blocklens[j], bufsize-i);
                }
            }
        }
        else
        {
            /* noncontiguous in memory as well as in file */

            ADIOI_Flatten_datatype(datatype);
            flat_buf = ADIOI_Flatlist;
            while (flat_buf->type != datatype)
            {
                flat_buf = flat_buf->next;
            }

            k = num = buf_count = 0;
            i = (int) (flat_buf->indices[0]);
            j = st_index;
            off = offset;
            n_filetypes = st_n_filetypes;
            fwr_size = st_fwr_size;
            bwr_size = flat_buf->blocklens[0];

            while (num < bufsize)
            {
                size = min(fwr_size, bwr_size);
                if (size)
                {
                    req_off = off;
                    req_len = size;
                    userbuf_off = i;
                    ADIOI_BUFFERED_WRITE
                }

                new_fwr_size = fwr_size;
                new_bwr_size = bwr_size;

                if (size == fwr_size)
                {
                    /* reached end of contiguous block in file */
                    do
                    {
                        if (++j == flat_file->count)
                        {
                            j = 0;
                            n_filetypes++;
                        }
                    } while(flat_file->blocklens[j] == 0);
                    off = disp +
                          flat_file->indices[j] +
                          (MPI_Offset) n_filetypes*filetype_extent;

                    new_fwr_size = flat_file->blocklens[j];
                    if (size != bwr_size)
                    {
                        i += size;
                        new_bwr_size -= size;
                    }
                }

                if (size == bwr_size)
                {
                    /* reached end of contiguous block in memory */

                    k = (k + 1)%flat_buf->count;
                    buf_count++;
                    i = (int) (buftype_extent*(buf_count/flat_buf->count) +
                        flat_buf->indices[k]);
                    new_bwr_size = flat_buf->blocklens[k];
                    if (size != fwr_size)
                    {
                        off += size;
                        new_fwr_size -= size;
                    }
                }
                num += size;
                fwr_size = new_fwr_size;
                bwr_size = new_bwr_size;
            }
        }

        /* write the buffer out finally */
        if (writebuf_len)
        {
            mpi_errno = ADIO_WriteContig(fd, writebuf, writebuf_len, MPI_BYTE, ADIO_EXPLICIT_OFFSET,
                            writebuf_off, &status1);
            if (!(fd->atomicity))
            {
                ADIOI_UNLOCK(fd, writebuf_off, writebuf_len);
            }
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }

        if (fd->atomicity)
        {
            ADIOI_UNLOCK(fd, start_off, end_offset-start_off+1);
        }

        ADIOI_Free(writebuf);

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            fd->fp_ind = off;
        }
    }

    /* This is a temporary way of filling in status. The right way is to
       keep track of how much data was actually written by ADIOI_BUFFERED_WRITE. */
    MPIR_Status_set_bytes(status, datatype, bufsize);

    if (!buftype_is_contig)
    {
        ADIOI_Delete_flattened(datatype);
    }

    return MPI_SUCCESS;
}


int ADIOI_GEN_WriteStrided_naive(MPI_File fd, const void *buf, int count,
                       MPI_Datatype buftype, int file_ptr_type,
                       MPI_Offset offset, MPI_Status *status
                       )
{
    /* offset is in units of etype relative to the filetype. */

    const ADIOI_Flatlist_node *flat_buf, *flat_file;
    /* bwr == buffer write; fwr == file write */
    int bwr_size, fwr_size=0, b_index;
    int bufsize, size, sum, n_etypes_in_filetype, size_in_filetype;
    int n_filetypes, etype_in_filetype;
    MPI_Offset abs_off_in_filetype=0;
    int filetype_size, etype_size, buftype_size, req_len;
    MPI_Aint filetype_extent, filetype_lb, buftype_extent, buftype_lb;
    int buf_count, buftype_is_contig, filetype_is_contig;
    MPI_Offset userbuf_off;
    MPI_Offset off, req_off, disp, end_offset=0, start_off;
    MPI_Status status1;

    int mpi_errno;

    ADIOI_Datatype_iscontig(buftype, &buftype_is_contig);
    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);

    NMPI_Type_size(fd->filetype, &filetype_size);
    if ( ! filetype_size )
        return MPI_SUCCESS;

    NMPI_Type_get_extent(fd->filetype, &filetype_lb,&filetype_extent);
    NMPI_Type_size(buftype, &buftype_size);
    NMPI_Type_get_extent(buftype, &buftype_lb, &buftype_extent);
    etype_size = fd->etype_size;

    bufsize = buftype_size * count;

    /* contiguous in buftype and filetype is handled elsewhere */

    if (!buftype_is_contig && filetype_is_contig)
    {
        int b_count;
        /* noncontiguous in memory, contiguous in file. */

        ADIOI_Flatten_datatype(buftype);
        flat_buf = ADIOI_Flatlist;
        while (flat_buf->type != buftype) flat_buf = flat_buf->next;

        off = (file_ptr_type == ADIO_INDIVIDUAL) ? fd->fp_ind :
              fd->disp + etype_size * offset;

        start_off = off;
        end_offset = off + bufsize - 1;

        /* if atomicity is true, lock (exclusive) the region to be accessed */
        if ((fd->atomicity))
        {
            ADIOI_WRITE_LOCK(fd, start_off, end_offset-start_off+1);
        }

        /* for each region in the buffer, grab the data and put it in
         * place
         */
        for (b_count=0; b_count < count; b_count++)
        {
            for (b_index=0; b_index < flat_buf->count; b_index++)
            {
                userbuf_off = b_count*buftype_extent +
                              flat_buf->indices[b_index];
                req_off = off;
                req_len = flat_buf->blocklens[b_index];

                mpi_errno = ADIO_WriteContig(
                                fd,
                                (char *) buf + userbuf_off,
                                req_len,
                                MPI_BYTE,
                                ADIO_EXPLICIT_OFFSET,
                                req_off,
                                &status1
                                );
                if (mpi_errno != MPI_SUCCESS)
                    return mpi_errno;

                /* off is (potentially) used to save the final offset later */
                off += flat_buf->blocklens[b_index];
            }
        }

        if ((fd->atomicity))
        {
            ADIOI_UNLOCK(fd, start_off, end_offset-start_off+1);
        }

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            fd->fp_ind = off;
        }
    }
    else
    {
        /* noncontiguous in file */

        int f_index, st_fwr_size, st_index = 0, st_n_filetypes;
        int flag;

        /* First we're going to calculate a set of values for use in all
         * the noncontiguous in file cases:
         * start_off - starting byte position of data in file
         * end_offset - last byte offset to be acessed in the file
         * st_n_filetypes - how far into the file we start in terms of
         *                  whole filetypes
         * st_index - index of block in first filetype that we will be
         *            starting in (?)
         * st_fwr_size - size of the data in the first filetype block
         *               that we will write (accounts for being part-way
         *               into writing this block of the filetype
         *
         */

        /* filetype already flattened in ADIO_Open */
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype) flat_file = flat_file->next;
        disp = fd->disp;

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            start_off = fd->fp_ind; /* in bytes */
            n_filetypes = -1;
            flag = 0;
            while (!flag)
            {
                n_filetypes++;
                for (f_index=0; f_index < flat_file->count; f_index++)
                {
                    if (disp + flat_file->indices[f_index] +
                       (MPI_Offset) n_filetypes*filetype_extent +
                       flat_file->blocklens[f_index] >= start_off)
                    {
                        /* this block contains our starting position */

                        st_index = f_index;
                        fwr_size = (int) (disp + flat_file->indices[f_index] +
                                   (MPI_Offset) n_filetypes*filetype_extent +
                                   flat_file->blocklens[f_index] - start_off);
                        flag = 1;
                        break;
                    }
                }
            }
        }
        else
        {
            n_etypes_in_filetype = filetype_size/etype_size;
            n_filetypes = (int) (offset / n_etypes_in_filetype);
            etype_in_filetype = (int) (offset % n_etypes_in_filetype);
            size_in_filetype = etype_in_filetype * etype_size;

            sum = 0;
            for (f_index=0; f_index < flat_file->count; f_index++)
            {
                sum += flat_file->blocklens[f_index];
                if (sum > size_in_filetype)
                {
                    st_index = f_index;
                    fwr_size = sum - size_in_filetype;
                    abs_off_in_filetype = flat_file->indices[f_index] +
                                          size_in_filetype -
                                          (sum - flat_file->blocklens[f_index]);
                    break;
                }
            }

            /* abs. offset in bytes in the file */
            start_off = disp + (MPI_Offset) n_filetypes*filetype_extent +
                        abs_off_in_filetype;
        }

        st_fwr_size = fwr_size;
        st_n_filetypes = n_filetypes;

        /* start_off, st_n_filetypes, st_index, and st_fwr_size are
         * all calculated at this point
         */

        /* Calculate end_offset, the last byte-offset that will be accessed.
         * e.g., if start_off=0 and 100 bytes to be written, end_offset=99
         */
        userbuf_off = 0;
        f_index = st_index;
        off = start_off;
        fwr_size = min(st_fwr_size, bufsize);
        while (userbuf_off < bufsize)
        {
            userbuf_off += fwr_size;
            end_offset = off + fwr_size - 1;

            if (++f_index >= flat_file->count)
            {
                f_index = 0;
                n_filetypes++;
            }

            off = disp + flat_file->indices[f_index] +
                  (MPI_Offset) n_filetypes*filetype_extent;
            fwr_size = min(flat_file->blocklens[f_index],
                                 bufsize-(int)userbuf_off);
        }

        /* End of calculations.  At this point the following values have
         * been calculated and are ready for use:
         * - start_off
         * - end_offset
         * - st_n_filetypes
         * - st_index
         * - st_fwr_size
         */

        /* if atomicity is true, lock (exclusive) the region to be accessed */
        if ((fd->atomicity))
        {
            ADIOI_WRITE_LOCK(fd, start_off, end_offset-start_off+1);
        }

        if (buftype_is_contig && !filetype_is_contig)
        {
            /* contiguous in memory, noncontiguous in file. should be the
             * most common case.
             */

            userbuf_off = 0;
            f_index = st_index;
            off = start_off;
            n_filetypes = st_n_filetypes;
            fwr_size = min(st_fwr_size, bufsize);

            /* while there is still space in the buffer, write more data */
            while (userbuf_off < bufsize)
            {
                if (fwr_size)
                {
                    /* TYPE_UB and TYPE_LB can result in
                       fwr_size = 0. save system call in such cases */
                    req_off = off;
                    req_len = fwr_size;

                    mpi_errno = ADIO_WriteContig(
                                    fd,
                                    (char *) buf + userbuf_off,
                                    req_len,
                                    MPI_BYTE,
                                    ADIO_EXPLICIT_OFFSET,
                                    req_off,
                                    &status1
                                    );
                    if (mpi_errno != MPI_SUCCESS)
                        return mpi_errno;
                }
                userbuf_off += fwr_size;

                if (off + fwr_size < disp + flat_file->indices[f_index] +
                   flat_file->blocklens[f_index] +
                   (MPI_Offset) n_filetypes*filetype_extent)
                {
                    /* important that this value be correct, as it is
                     * used to set the offset in the fd near the end of
                     * this function.
                     */
                    off += fwr_size;
                }
                /* did not reach end of contiguous block in filetype.
                 * no more I/O needed. off is incremented by fwr_size.
                 */
                else
                {
                    if (++f_index >= flat_file->count)
                    {
                        f_index = 0;
                        n_filetypes++;
                    }
                    off = disp + flat_file->indices[f_index] +
                          (MPI_Offset) n_filetypes*filetype_extent;
                    fwr_size = min(flat_file->blocklens[f_index],
                                         bufsize-(int)userbuf_off);
                }
            }
        }
        else
        {
            int i, tmp_bufsize = 0;
            /* noncontiguous in memory as well as in file */

            ADIOI_Flatten_datatype(buftype);
            flat_buf = ADIOI_Flatlist;
            while (flat_buf->type != buftype) flat_buf = flat_buf->next;

            b_index = buf_count = 0;
            i = (int) (flat_buf->indices[0]);
            f_index = st_index;
            off = start_off;
            n_filetypes = st_n_filetypes;
            fwr_size = st_fwr_size;
            bwr_size = flat_buf->blocklens[0];

            /* while we haven't read size * count bytes, keep going */
            while (tmp_bufsize < bufsize)
            {
                int new_bwr_size = bwr_size, new_fwr_size = fwr_size;

                size = min(fwr_size, bwr_size);
                if (size)
                {
                    req_off = off;
                    req_len = size;
                    userbuf_off = i;

                    mpi_errno = ADIO_WriteContig(
                                    fd,
                                    (char *) buf + userbuf_off,
                                    req_len,
                                    MPI_BYTE,
                                    ADIO_EXPLICIT_OFFSET,
                                    req_off,
                                    &status1
                                    );
                    if (mpi_errno != MPI_SUCCESS)
                        return mpi_errno;
                }

                if (size == fwr_size)
                {
                    /* reached end of contiguous block in file */
                    if (++f_index >= flat_file->count)
                    {
                        f_index = 0;
                        n_filetypes++;
                    }

                    off = disp + flat_file->indices[f_index] +
                          (MPI_Offset) n_filetypes*filetype_extent;

                    new_fwr_size = flat_file->blocklens[f_index];
                    if (size != bwr_size)
                    {
                        i += size;
                        new_bwr_size -= size;
                    }
                }

                if (size == bwr_size)
                {
                    /* reached end of contiguous block in memory */

                    b_index = (b_index + 1)%flat_buf->count;
                    buf_count++;
                    i = (int) (buftype_extent*(buf_count/flat_buf->count) +
                        flat_buf->indices[b_index]);
                    new_bwr_size = flat_buf->blocklens[b_index];
                    if (size != fwr_size)
                    {
                        off += size;
                        new_fwr_size -= size;
                    }
                }
                tmp_bufsize += size;
                fwr_size = new_fwr_size;
                bwr_size = new_bwr_size;
            }
        }

        /* unlock the file region if we locked it */
        if ((fd->atomicity))
        {
            ADIOI_UNLOCK(fd, start_off, end_offset-start_off+1);
        }

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            fd->fp_ind = off;
        }
    } /* end of (else noncontiguous in file) */

    /* This is a temporary way of filling in status. The right way is to
     * keep track of how much data was actually written and placed in buf
     */
    MPIR_Status_set_bytes(status, buftype, bufsize);

    if (!buftype_is_contig)
    {
        ADIOI_Delete_flattened(buftype);
    }

    return MPI_SUCCESS;
}
