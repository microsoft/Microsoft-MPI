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


int ADIOI_NTFS_IreadContig(MPI_File fd, void *buf, int count,
                MPI_Datatype datatype, int file_ptr_type,
                MPI_Offset offset, MPI_Request *request)
{
    int len, typesize;

    NMPI_Type_size(datatype, &typesize);
    len = count * typesize;

    if (file_ptr_type == ADIO_INDIVIDUAL)
    {
        offset = fd->fp_ind;
    }

    int mpi_errno;
    mpi_errno = ADIOI_NTFS_aio(fd, buf, len, offset, 0, request);
    if (file_ptr_type == ADIO_INDIVIDUAL)
    {
        fd->fp_ind += len;
    }

    if (mpi_errno != MPI_SUCCESS)
        return MPIU_ERR_FAIL(mpi_errno);

    return MPI_SUCCESS;
}


int ADIOI_NTFS_ReadContig(MPI_File fd, void *buf, int count,
                           MPI_Datatype datatype, int file_ptr_type,
                           MPI_Offset offset, MPI_Status *status
                           )
{
    DWORD dwNumRead = 0;
    int err=-1, datatype_size, len;
    OVERLAPPED ov;

    /* If file pointer is of type ADIO_INDIVIDUAL ignore the offset
       and use the current location of file pointer */
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

    err = ReadFile(fd->fd_sys, buf, len, &dwNumRead, &ov);
    if (err == FALSE)
    {
        err = GetLastError();
        switch (err)
        {
        case ERROR_IO_PENDING:
            break;
        case ERROR_HANDLE_EOF:
            /*printf("EOF error\n");fflush(stdout);*/
            SetEvent(ov.hEvent);
            break;
        default:
            CloseHandle(ov.hEvent);
            return MPIO_ERR_GLE(err);
        }
    }
    err = GetOverlappedResult(fd->fd_sys, &ov, &dwNumRead, TRUE);
    if (err == FALSE)
    {
        err = GetLastError();
        if (err != ERROR_HANDLE_EOF) /* Ignore EOF errors */
        {
            CloseHandle(ov.hEvent);
            return MPIO_ERR_GLE(err);
        }
    }

    CloseHandle(ov.hEvent);

    if (file_ptr_type == ADIO_INDIVIDUAL)
    {
        fd->fp_ind = fd->fp_ind + (MPI_Offset)dwNumRead;
    }

    if (err == FALSE)
    {
        err = GetLastError();
        return MPIO_ERR_GLE(err);
    }

    MPIR_Status_set_bytes(status, datatype, dwNumRead);
    return MPI_SUCCESS;
}


/* Generic implementation of IreadStrided calls the blocking ReadStrided
 * immediately.
 */
int ADIOI_GEN_IreadStrided(MPI_File fd, void *buf, int count,
                            MPI_Datatype datatype, int file_ptr_type,
                            MPI_Offset offset, MPI_Request* request
                            )
{
    /* Call the blocking function.  It will create an error code
     * if necessary.
     */
    int mpi_errno;
    MPI_Status status;
    mpi_errno = ADIO_ReadStrided(fd, buf, count, datatype, file_ptr_type,
                     offset, &status);

    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    int typesize;
    NMPI_Type_size(datatype, &typesize);
    MPI_Offset nbytes = count*typesize;
    MPIO_Completed_request_create(nbytes, request);
    return MPI_SUCCESS;
}


/* prototypes of functions used for collective reads only. */
static int ADIOI_Read_and_exch(MPI_File fd, void *buf, MPI_Datatype
                                datatype, int nprocs,
                                int myrank, ADIOI_Access
                                *others_req, const MPI_Offset *offset_list,
                                const int *len_list, int contig_access_count,
                                MPI_Offset min_st_offset, MPI_Offset fd_size,
                                const MPI_Offset *fd_start, const MPI_Offset *fd_end,
                                _Out_ int *buf_idx, _Out_ int *bytes_read);


static void
ADIOI_R_Exchange_data(
    _In_ MPI_File fd,
    _Out_ void *buf,
    _In_opt_ const ADIOI_Flatlist_node *flat_buf,
    _In_reads_(contig_access_count) const MPI_Offset *offset_list,
    _In_reads_(contig_access_count) const int *len_list,
    _In_reads_(nprocs) const int *send_size,
    _Out_writes_(nprocs) int *recv_size,
    _In_reads_(nprocs) const int *count,
    _In_reads_(nprocs) const int *start_pos,
    _In_reads_(nprocs) const int *partial_send,
    _When_(buftype_is_contig == 1, _Out_writes_(nprocs)) int *recd_from_proc,
    _In_range_(>,0) int nprocs,
    _In_range_(>=,0) int myrank,
    _In_range_(0,1) int buftype_is_contig,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset *fd_start,
    _In_ const MPI_Offset *fd_end,
    _In_reads_(nprocs) ADIOI_Access *others_req,
    _In_ int iter,
    _In_ MPI_Aint buftype_extent,
    _When_(buftype_is_contig == 1, _Out_writes_(nprocs)) int *buf_idx
    );


static void
ADIOI_Fill_user_buffer(
    _In_ MPI_File fd,
    _When_(contig_access_count > 0, _Out_) void *buf,
    _In_ const ADIOI_Flatlist_node *flat_buf,
    _In_ const char* const *recv_buf,
    _In_reads_(contig_access_count) const MPI_Offset *offset_list,
    _In_reads_(contig_access_count) const int *len_list,
    _In_ const int *recv_size,
    _Inout_updates_(nprocs) int *recd_from_proc,
    _In_ int nprocs,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset *fd_start,
    _In_ const MPI_Offset *fd_end,
    _In_ MPI_Aint buftype_extent
    );


int ADIOI_GEN_ReadStridedColl(MPI_File fd, void *buf, int count,
                               MPI_Datatype datatype, int file_ptr_type,
                               MPI_Offset offset, MPI_Status *status
                               )
{
/* Uses a generalized version of the extended two-phase method described
   in "An Extended Two-Phase Method for Accessing Sections of
   Out-of-Core Arrays", Rajeev Thakur and Alok Choudhary,
   Scientific Programming, (5)4:301--317, Winter 1996.
   http://www.mcs.anl.gov/home/thakur/ext2ph.ps */

    ADIOI_Access *my_req;
    /* array of nprocs structures, one for each other process in
       whose file domain this process's request lies */

    ADIOI_Access *others_req;
    /* array of nprocs structures, one for each other process
       whose request lies in this process's file domain. */

    int i, filetype_is_contig, nprocs, nprocs_for_coll, myrank;
    int contig_access_count=0, interleave_count = 0, buftype_is_contig;
    int *count_my_req_per_proc, count_my_req_procs, count_others_req_procs;
    MPI_Offset start_offset, end_offset, orig_fp, fd_size, min_st_offset, off;
    MPI_Offset *offset_list = NULL, *st_offsets = NULL, *fd_start = NULL,
        *fd_end = NULL, *end_offsets = NULL;
    int *len_list = NULL, *buf_idx = NULL;
    int bytes_read = 0;

    NMPI_Comm_size(fd->comm, &nprocs);
    NMPI_Comm_rank(fd->comm, &myrank);

    /* number of aggregators, cb_nodes, is stored in the hints */
    nprocs_for_coll = fd->hints->cb_nodes;
    orig_fp = fd->fp_ind;

    /* only check for interleaving if cb_read isn't disabled */
    if (fd->hints->cb_read != ADIOI_HINT_DISABLE)
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

    if (fd->hints->cb_read == ADIOI_HINT_DISABLE
        || (!interleave_count && (fd->hints->cb_read == ADIOI_HINT_AUTO)))
    {
        /* don't do aggregation */
        if (fd->hints->cb_read != ADIOI_HINT_DISABLE)
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
                return ADIO_ReadContig(fd, buf, count, datatype, ADIO_EXPLICIT_OFFSET,
                         off, status);
            }
            return ADIO_ReadContig(fd, buf, count, datatype, ADIO_INDIVIDUAL, 0, status);
        }

        return ADIO_ReadStrided(fd, buf, count, datatype, file_ptr_type, offset, status);
    }

    /* We're going to perform aggregation of I/O.  Here we call
     * ADIOI_Calc_file_domains() to determine what processes will handle I/O
     * to what regions.  We pass nprocs_for_coll into this function; it is
     * used to determine how many processes will perform I/O, which is also
     * the number of regions into which the range of bytes must be divided.
     * These regions are called "file domains", or FDs.
     *
     * When this function returns, fd_start, fd_end, fd_size, and
     * min_st_offset will be filled in.  fd_start holds the starting byte
     * location for each file domain.  fd_end holds the ending byte location.
     * min_st_offset holds the minimum byte location that will be accessed.
     *
     * Both fd_start[] and fd_end[] are indexed by an aggregator number; this
     * needs to be mapped to an actual rank in the communicator later.
     *
     */
    ADIOI_Calc_file_domains(st_offsets, end_offsets, nprocs,
                            nprocs_for_coll, &min_st_offset,
                            &fd_start, &fd_end, &fd_size);
    if(fd_start == NULL)
    {
        return MPIU_ERR_NOMEM();
    }

    /* calculate where the portions of the access requests of this process
     * are located in terms of the file domains.  this could be on the same
     * process or on other processes.  this function fills in:
     * count_my_req_procs - number of processes (including this one) for which
     *     this process has requests in their file domain
     * count_my_req_per_proc - count of requests for each process, indexed
     *     by rank of the process
     * my_req[] - array of data structures describing the requests to be
     *     performed by each process (including self).  indexed by rank.
     * buf_idx[] - array of locations into which data can be directly moved;
     *     this is only valid for contiguous buffer case
     */
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

    /* perform a collective communication in order to distribute the
     * data calculated above.  fills in the following:
     * count_others_req_procs - number of processes (including this
     *     one) which have requests in this process's file domain.
     * count_others_req_per_proc[] - number of separate contiguous
     *     requests from proc i lie in this process's file domain.
     */
    ADIOI_Calc_others_req(fd, count_my_req_procs,
                          count_my_req_per_proc, my_req,
                          nprocs, myrank, &count_others_req_procs,
                          &others_req);

    /* my_req[] and count_my_req_per_proc aren't needed at this point, so
     * let's free the memory
     */
    ADIOI_Free(count_my_req_per_proc);
    i = nprocs;
    do
    {
        --i;
        if (my_req[i].count)
        {
            ADIOI_Free(my_req[i].offsets);
            ADIOI_Free(my_req[i].lens);
        }
    } while (i > 0);
    ADIOI_Free(my_req);


    /* read data in sizes of no more than ADIOI_Coll_bufsize,
     * communicate, and fill user buf.
     */
    int mpi_errno;
    mpi_errno = ADIOI_Read_and_exch(fd, buf, datatype, nprocs, myrank,
                        others_req, offset_list,
                        len_list, contig_access_count, min_st_offset,
                        fd_size, fd_start, fd_end, buf_idx, &bytes_read);

    if (!buftype_is_contig)
    {
        ADIOI_Delete_flattened(datatype);
    }

    /* free all memory allocated for collective I/O */
    i = nprocs;
    do
    {
        --i;
        if (others_req[i].count)
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

    MPIR_Status_set_bytes(status, datatype, bytes_read);

    return mpi_errno;
}

void ADIOI_Calc_my_off_len(MPI_File fd, int bufcount, MPI_Datatype
                            datatype, int file_ptr_type, MPI_Offset
                            offset, MPI_Offset **offset_list_ptr, int
                            **len_list_ptr, MPI_Offset *start_offset_ptr,
                            MPI_Offset *end_offset_ptr, int
                           *contig_access_count_ptr)
{
    int filetype_size, buftype_size, etype_size;
    int i, j, k, frd_size=0, old_frd_size=0, st_index=0;
    int n_filetypes, etype_in_filetype;
    MPI_Offset abs_off_in_filetype=0;
    int bufsize, sum, n_etypes_in_filetype, size_in_filetype;
    int contig_access_count, *len_list, filetype_is_contig;
    MPI_Aint filetype_extent, filetype_lb;
    const ADIOI_Flatlist_node *flat_file;
    MPI_Offset *offset_list, off, end_offset=0, disp;

/* For this process's request, calculate the list of offsets and
   lengths in the file and determine the start and end offsets. */

    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);

    NMPI_Type_size(fd->filetype, &filetype_size);
    NMPI_Type_get_extent(fd->filetype, &filetype_lb, &filetype_extent);
    NMPI_Type_size(datatype, &buftype_size);
    etype_size = fd->etype_size;

    if ( ! filetype_size )
    {
        *contig_access_count_ptr = 0;
        *offset_list_ptr = (MPI_Offset *) ADIOI_Malloc(2*sizeof(MPI_Offset));
        *len_list_ptr = (int *) ADIOI_Malloc(2*sizeof(int));
        /* 2 is for consistency. everywhere I malloc one more than needed */

        offset_list = *offset_list_ptr;
        len_list = *len_list_ptr;
        offset_list[0] = (file_ptr_type == ADIO_INDIVIDUAL) ? fd->fp_ind :
                 fd->disp + etype_size * offset;
        len_list[0] = 0;
        *start_offset_ptr = offset_list[0];
        *end_offset_ptr = offset_list[0] + len_list[0] - 1;

        return;
    }

    if (filetype_is_contig)
    {
        *contig_access_count_ptr = 1;
        *offset_list_ptr = (MPI_Offset *) ADIOI_Malloc(2*sizeof(MPI_Offset));
        *len_list_ptr = (int *) ADIOI_Malloc(2*sizeof(int));
        /* 2 is for consistency. everywhere I malloc one more than needed */

        offset_list = *offset_list_ptr;
        len_list = *len_list_ptr;
        offset_list[0] = (file_ptr_type == ADIO_INDIVIDUAL) ? fd->fp_ind :
                 fd->disp + etype_size * offset;
        len_list[0] = bufcount * buftype_size;
        *start_offset_ptr = offset_list[0];
        *end_offset_ptr = offset_list[0] + len_list[0] - 1;

        /* update file pointer */
        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            fd->fp_ind = *end_offset_ptr + 1;
        }
    }
    else
    {

       /* First calculate what size of offset_list and len_list to allocate */

       /* filetype already flattened in ADIO_Open or ADIO_Fcntl */
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

            /* find the block where offset is located, skip blocklens[i] == 0 */
            for (i=0; i<flat_file->count; i++)
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
                    frd_size = flat_file->blocklens[i];
                    break;
                }
                if(dist > 0)
                {
                    frd_size = static_cast<int>(dist);
                    break;
                }
            }
            st_index = i;
            offset += disp + (MPI_Offset)n_filetypes * filetype_extent;
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
                    frd_size = sum - size_in_filetype;
                    abs_off_in_filetype = flat_file->indices[i] +
                        size_in_filetype - (sum - flat_file->blocklens[i]);
                    break;
                }
            }

            /* abs. offset in bytes in the file */
            offset = disp + (MPI_Offset) n_filetypes*filetype_extent +
                abs_off_in_filetype;
        }

         /* calculate how much space to allocate for offset_list, len_list */

        old_frd_size = frd_size;
        contig_access_count = i = 0;
        j = st_index;
        bufsize = buftype_size * bufcount;
        frd_size = min(frd_size, bufsize);
        while (i < bufsize)
        {
            if (frd_size)
            {
                contig_access_count++;
            }
            i += frd_size;
            j = (j + 1) % flat_file->count;
            frd_size = min(flat_file->blocklens[j], bufsize-i);
        }

        /* allocate space for offset_list and len_list */

        *offset_list_ptr = (MPI_Offset *)
                 ADIOI_Malloc((contig_access_count+1)*sizeof(MPI_Offset));
        *len_list_ptr = (int *) ADIOI_Malloc((contig_access_count+1)*sizeof(int));
        /* +1 to avoid a 0-size malloc */

        offset_list = *offset_list_ptr;
        len_list = *len_list_ptr;

      /* find start offset, end offset, and fill in offset_list and len_list */

        *start_offset_ptr = offset; /* calculated above */

        i = k = 0;
        j = st_index;
        off = offset;
        frd_size = min(old_frd_size, bufsize);
        while (i < bufsize)
        {
            if (frd_size)
            {
                offset_list[k] = off;
                len_list[k] = frd_size;
                k++;
            }
            i += frd_size;
            end_offset = off + frd_size - 1;

     /* Note: end_offset points to the last byte-offset that will be accessed.
         e.g., if start_offset=0 and 100 bytes to be read, end_offset=99*/

            if (off + frd_size < disp + flat_file->indices[j] +
                flat_file->blocklens[j] +
                (MPI_Offset) n_filetypes*filetype_extent)
            {
                off += frd_size;
                /* did not reach end of contiguous block in filetype.
                 * no more I/O needed. off is incremented by frd_size.
                 */
            }
            else
            {
                do
                {
                    if (++j == flat_file->count)
                    {
                        j = 0;
                        n_filetypes++;
                    }
                } while(flat_file->blocklens[j] == 0);
                off = disp + flat_file->indices[j] +
                    (MPI_Offset) n_filetypes*filetype_extent;
                frd_size = min(flat_file->blocklens[j], bufsize-i);
            }
        }

        /* update file pointer */
        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            fd->fp_ind = off;
        }

        *contig_access_count_ptr = contig_access_count;
        *end_offset_ptr = end_offset;
    }
}

static int ADIOI_Read_and_exch(
    MPI_File fd,
    void *buf,
    MPI_Datatype datatype,
    int nprocs,
    int myrank,
    ADIOI_Access *others_req,
    const MPI_Offset *offset_list,
    const int *len_list,
    int contig_access_count,
    MPI_Offset min_st_offset,
    MPI_Offset fd_size,
    const MPI_Offset *fd_start,
    const MPI_Offset *fd_end,
    _Out_ int *buf_idx,
    _Out_ int* bytes_read
    )
{
/* Read in sizes of no more than coll_bufsize, an info parameter.
   Send data to appropriate processes.
   Place recd. data in user buf.
   The idea is to reduce the amount of extra memory required for
   collective I/O. If all data were read all at once, which is much
   easier, it would require temp space more than the size of user_buf,
   which is often unacceptable. For example, to read a distributed
   array from a file, where each local array is 8Mbytes, requiring
   at least another 8Mbytes of temp space is unacceptable. */

    int i, j, m, size, ntimes, max_ntimes, buftype_is_contig;
    MPI_Offset st_loc=-1, end_loc=-1, off, done, real_off, req_off;
    char *read_buf = NULL, *tmp_buf;
    int *curr_offlen_ptr, *count, *send_size, *recv_size;
    int *partial_send, *recd_from_proc, *start_pos, for_next_iter;
    int real_size, req_len, flag, for_curr_iter, rank;
    MPI_Status status;
    const ADIOI_Flatlist_node *flat_buf=NULL;
    MPI_Aint buftype_extent, buftype_lb;
    int coll_bufsize;

    /* only I/O errors are currently reported */

    OACR_USE_PTR( buf_idx );
    OACR_USE_PTR( bytes_read );

/* calculate the number of reads of size coll_bufsize
   to be done by each process and the max among all processes.
   That gives the no. of communication phases as well.
   coll_bufsize is obtained from the hints object. */


    coll_bufsize = fd->hints->cb_buffer_size;

    /* grab some initial values for st_loc and end_loc */
    for (i=0; i < nprocs; i++)
    {
        if (others_req[i].count)
        {
            st_loc = others_req[i].offsets[0];
            end_loc = others_req[i].offsets[0];
            break;
        }
    }

    /* now find the real values */
    for (i=0; i < nprocs; i++)
    {
        for (j=0; j<others_req[i].count; j++)
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
    /* calculate ntimes, the number of times this process must perform I/O
     * operations in order to complete all the requests it has received.
     * the need for multiple I/O operations comes from the restriction that
     * we only use coll_bufsize bytes of memory for internal buffering.
     */
    if ((st_loc==-1) && (end_loc==-1))
    {
        /* this process does no I/O. */
        ntimes = 0;
    }
    else
    {
        /* ntimes=ceiling_div(end_loc - st_loc + 1, coll_bufsize)*/
        ntimes = (int) ((end_loc - st_loc + coll_bufsize)/coll_bufsize);
    }

    NMPI_Allreduce(&ntimes, &max_ntimes, 1, MPI_INT, MPI_MAX, fd->comm);

    if( ntimes != 0 )
    {
        read_buf = (char *) ADIOI_Malloc(coll_bufsize);
    }

    curr_offlen_ptr = (int *) ADIOI_Calloc(nprocs, sizeof(int));
    /* its use is explained below. calloc initializes to 0. */

    count = (int *) ADIOI_Malloc(nprocs * sizeof(int));
    /* to store count of how many off-len pairs per proc are satisfied
       in an iteration. */

    partial_send = (int *) ADIOI_Calloc(nprocs, sizeof(int));
    /* if only a portion of the last off-len pair is sent to a process
       in a particular iteration, the length sent is stored here.
       calloc initializes to 0. */

    send_size = (int *) ADIOI_Malloc(nprocs * sizeof(int));
    /* total size of data to be sent to each proc. in an iteration */

    recv_size = (int *) ADIOI_Malloc(nprocs * sizeof(int));
    /* total size of data to be recd. from each proc. in an iteration.
       Of size nprocs so that I can use MPI_Alltoall later. */

    recd_from_proc = (int *) ADIOI_Calloc(nprocs, sizeof(int));
    /* amount of data recd. so far from each proc. Used in
       ADIOI_Fill_user_buffer. initialized to 0 here. */

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

    done = 0;
    off = st_loc;
    for_curr_iter = for_next_iter = 0;

    NMPI_Comm_rank(fd->comm, &rank);

    for (m=0; m<ntimes; m++)
    {
       /* read buf of size coll_bufsize (or less) */
       /* go through all others_req and check if any are satisfied
          by the current read */

       /* since MPI guarantees that displacements in filetypes are in
          monotonically nondecreasing order, I can maintain a pointer
          (curr_offlen_ptr) to
          current off-len pair for each process in others_req and scan
          further only from there. There is still a problem of filetypes
          such as:  (1, 2, 3 are not process nos. They are just numbers for
          three chunks of data, specified by a filetype.)

                   1  -------!--
                   2    -----!----
                   3       --!-----

          where ! indicates where the current read_size limitation cuts
          through the filetype.  I resolve this by reading up to !, but
          filling the communication buffer only for 1. I copy the portion
          left over for 2 into a tmp_buf for use in the next
          iteration. i.e., 2 and 3 will be satisfied in the next
          iteration. This simplifies filling in the user's buf at the
          other end, as only one off-len pair with incomplete data
          will be sent. I also don't need to send the individual
          offsets and lens along with the data, as the data is being
          sent in a particular order. */

          /* off = start offset in the file for the data actually read in
                   this iteration
             size = size of data read corresponding to off
             real_off = off minus whatever data was retained in memory from
                  previous iteration for cases like 2, 3 illustrated above
             real_size = size plus the extra corresponding to real_off
             req_off = off in file for a particular contiguous request
                       minus what was satisfied in previous iteration
             req_size = size corresponding to req_off */

        size = (int) (min(coll_bufsize, end_loc-st_loc+1-done));
        real_off = off - for_curr_iter;
        real_size = size + for_curr_iter;

        for (i=0; i<nprocs; i++) count[i] = send_size[i] = 0;
        for_next_iter = 0;

        for (i=0; i<nprocs; i++)
        {
            if (others_req[i].count)
            {
                start_pos[i] = curr_offlen_ptr[i];
                for (j=curr_offlen_ptr[i]; j<others_req[i].count; j++)
                {
                    if (partial_send[i])
                    {
                        /* this request may have been partially
                           satisfied in the previous iteration. */
                        req_off = others_req[i].offsets[j] +
                            partial_send[i];
                        req_len = others_req[i].lens[j] -
                            partial_send[i];
                        partial_send[i] = 0;
                        /* modify the off-len pair to reflect this change */
                        others_req[i].offsets[j] = req_off;
                        others_req[i].lens[j] = req_len;
                    }
                    else
                    {
                        req_off = others_req[i].offsets[j];
                        req_len = others_req[i].lens[j];
                    }
                    if (req_off < real_off + real_size)
                    {
                        count[i]++;
                        NMPI_Get_address(read_buf+req_off-real_off,
                               &(others_req[i].mem_ptrs[j]));
                        send_size[i] += (int)(min(real_off + (MPI_Offset)real_size -
                                                  req_off, req_len));

                        if (real_off+real_size-req_off < req_len)
                        {
                            partial_send[i] = (int) (real_off+real_size-
                                                     req_off);
                            if ((j+1 < others_req[i].count) &&
                                 (others_req[i].offsets[j+1] <
                                     real_off+real_size))
                            {
                                /* this is the case illustrated in the
                                   figure above. */
                                for_next_iter = (int) (max(for_next_iter,
                                          real_off + real_size -
                                             others_req[i].offsets[j+1]));
                                /* max because it must cover requests
                                   from different processes */
                            }
                            break;
                        }
                    }
                    else break;
                }
                curr_offlen_ptr[i] = j;
            }
        }

        flag = 0;
        for (i=0; i<nprocs; i++)
        {
            if (count[i])
            {
                flag = 1;
                break;
            }
        }

        if (flag)
        {
            int mpi_errno;
            mpi_errno = ADIO_ReadContig(fd, read_buf+for_curr_iter, size, MPI_BYTE,
                            ADIO_EXPLICIT_OFFSET, off, &status);
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }

        for_curr_iter = for_next_iter;

        ADIOI_R_Exchange_data(fd, buf, flat_buf, offset_list, len_list,
                            send_size, recv_size, count,
                            start_pos, partial_send, recd_from_proc, nprocs,
                            myrank,
                            buftype_is_contig, contig_access_count,
                            min_st_offset, fd_size, fd_start, fd_end,
                            others_req,
                            m, buftype_extent, buf_idx);
        for(i = 0; i < nprocs; ++i)
        {
            *bytes_read += recv_size[i];
        }

        if (for_next_iter)
        {
            tmp_buf = (char *) ADIOI_Malloc(for_next_iter);
            memcpy(tmp_buf, read_buf+real_size-for_next_iter, for_next_iter);
            ADIOI_Free(read_buf);
            read_buf = (char *) ADIOI_Malloc(for_next_iter+coll_bufsize);
            memcpy(read_buf, tmp_buf, for_next_iter);
            ADIOI_Free(tmp_buf);
        }

        off += size;
        done += size;
    }

    i = nprocs;
    do
    {
        --i;
        count[i] = send_size[i] = 0;
    } while (i > 0);
    for (m=ntimes; m<max_ntimes; m++)
    {
/* nothing to send, but check for recv. */
        ADIOI_R_Exchange_data(fd, buf, flat_buf, offset_list, len_list,
                            send_size, recv_size, count,
                            start_pos, partial_send, recd_from_proc, nprocs,
                            myrank,
                            buftype_is_contig, contig_access_count,
                            min_st_offset, fd_size, fd_start, fd_end,
                            others_req, m,
                            buftype_extent, buf_idx);
        i = nprocs;
        do
        {
            --i;
            *bytes_read += recv_size[i];
        } while (i > 0);
    }

    if (ntimes)
    {
        ADIOI_Free(read_buf);
    }
    ADIOI_Free(curr_offlen_ptr);
    ADIOI_Free(count);
    ADIOI_Free(partial_send);
    ADIOI_Free(send_size);
    ADIOI_Free(recv_size);
    ADIOI_Free(recd_from_proc);
    ADIOI_Free(start_pos);
    return MPI_SUCCESS;
}


static void
ADIOI_R_Exchange_data(
    _In_ MPI_File fd,
    _Out_ void *buf,
    _In_opt_ const ADIOI_Flatlist_node *flat_buf,
    _In_reads_(contig_access_count) const MPI_Offset *offset_list,
    _In_reads_(contig_access_count) const int *len_list,
    _In_reads_(nprocs) const int *send_size,
    _Out_writes_(nprocs) int *recv_size,
    _In_reads_(nprocs) const int *count,
    _In_reads_(nprocs) const int *start_pos,
    _In_reads_(nprocs) const int *partial_send,
    _When_(buftype_is_contig == 1, _Out_writes_(nprocs)) int *recd_from_proc,
    _In_range_(>,0) int nprocs,
    _In_range_(>=,0) int myrank,
    _In_range_(0,1) int buftype_is_contig,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset *fd_start,
    _In_ const MPI_Offset *fd_end,
    _In_reads_(nprocs) ADIOI_Access *others_req,
    _In_ int iter,
    _In_ MPI_Aint buftype_extent,
    _When_(buftype_is_contig == 1, _Out_writes_(nprocs)) int *buf_idx
    )
{
    int i, j, k=0, tmp=0, nprocs_recv, nprocs_send;
    char **recv_buf = NULL;
    MPI_Request *requests;
    MPI_Datatype send_type;
    MPI_Status *statuses;

/* exchange send_size info so that each process knows how much to
   receive from whom and how much memory to allocate. */

    NMPI_Alltoall(const_cast<int *>(send_size), 1, MPI_INT, const_cast<int *>(recv_size), 1, MPI_INT, fd->comm);

    nprocs_recv = nprocs_send = nprocs;
    i = nprocs;
    do
    {
        --i;
        if (recv_size[i] == 0)
        {
            --nprocs_recv;
        }
        if (send_size[i] == 0)
        {
            --nprocs_send;
        }
    } while (i > 0);

    requests = (MPI_Request *)
        ADIOI_Malloc((nprocs_send+nprocs_recv+1)*sizeof(MPI_Request));
/* +1 to avoid a 0-size malloc */

/* post recvs. if buftype_is_contig, data can be directly recd. into
   user buf at location given by buf_idx. else use recv_buf. */

    if (buftype_is_contig)
    {
        j = 0;
        for (i=0; i < nprocs; i++)
        {
            if (recv_size[i])
            {
                NMPI_Irecv(((char *) buf) + buf_idx[i], recv_size[i],
                    MPI_BYTE, i, myrank+i+100*iter, fd->comm, requests+j);
                j++;
                buf_idx[i] += recv_size[i];
            }
        }
    }
    else
    {
/* allocate memory for recv_buf and post receives */
        recv_buf = (char **) ADIOI_Malloc(nprocs * sizeof(char*));
        for (i=0; i < nprocs; i++)
        {
            if (recv_size[i])
            {
                recv_buf[i] = (char *) ADIOI_Malloc(recv_size[i]);
            }
        }
        j = 0;
        for (i=0; i < nprocs; i++)
        {
            if (recv_size[i])
            {
                NMPI_Irecv(recv_buf[i], recv_size[i], MPI_BYTE, i,
                    myrank+i+100*iter, fd->comm, requests+j);
                j++;
            }
        }
    }

/* create derived datatypes and send data */

    j = 0;
    for (i=0; i<nprocs; i++)
    {
        if (send_size[i])
        {
/* take care if the last off-len pair is a partial send */
            if (partial_send[i])
            {
                k = start_pos[i] + count[i] - 1;
                tmp = others_req[i].lens[k];
                others_req[i].lens[k] = partial_send[i];
            }
            NMPI_Type_create_hindexed(count[i],
                 &(others_req[i].lens[start_pos[i]]),
                    &(others_req[i].mem_ptrs[start_pos[i]]),
                         MPI_BYTE, &send_type);
            /* absolute displacement; use MPI_BOTTOM in send */
            NMPI_Type_commit(&send_type);
            NMPI_Isend(MPI_BOTTOM, 1, send_type, i, myrank+i+100*iter,
                      fd->comm, requests+nprocs_recv+j);
            NMPI_Type_free(&send_type);
            if (partial_send[i])
            {
                others_req[i].lens[k] = tmp;
            }
            j++;
        }
    }

    statuses = (MPI_Status *) ADIOI_Malloc((nprocs_send+nprocs_recv+1)*sizeof(MPI_Status));
     /* +1 to avoid a 0-size malloc */

    /* wait on the receives */
    if (nprocs_recv)
    {
        NMPI_Waitall(nprocs_recv, requests, statuses);

        /* if noncontiguous, to the copies from the recv buffers */
        if (!buftype_is_contig)
        {
            ADIOI_Fill_user_buffer(fd, buf, flat_buf, recv_buf,
                                   offset_list, len_list, recv_size, recd_from_proc,
                                   nprocs, contig_access_count,
                                   min_st_offset, fd_size, fd_start, fd_end,
                                   buftype_extent);
        }
    }

    /* wait on the sends*/
    NMPI_Waitall(nprocs_send, requests+nprocs_recv, statuses+nprocs_recv);

    ADIOI_Free(statuses);
    ADIOI_Free(requests);

    if (!buftype_is_contig)
    {
        i = nprocs;
        do
        {
            --i;
            if (recv_size[i] != 0)
            {
                ADIOI_Free(recv_buf[i]);
            }
        } while (i > 0);
        ADIOI_Free(recv_buf);
    }
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
            if (++flat_buf_idx >= flat_buf->count) \
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
        memcpy(((char *) buf) + user_buf_idx, \
               &(recv_buf[p][recv_buf_idx[p]]), size_in_buf); \
        recv_buf_idx[p] += size_in_buf; \
        if (size_in_buf != flat_buf_sz) \
        { \
            user_buf_idx += size_in_buf; \
            flat_buf_sz -= size_in_buf; \
        } \
        else \
        { \
            if (++flat_buf_idx >= flat_buf->count) \
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
ADIOI_Fill_user_buffer(
    _In_ MPI_File fd,
    _When_(contig_access_count > 0, _Out_) void *buf,
    _In_ const ADIOI_Flatlist_node *flat_buf,
    _In_ const char* const *recv_buf,
    _In_reads_(contig_access_count) const MPI_Offset *offset_list,
    _In_reads_(contig_access_count) const int *len_list,
    _In_ const int *recv_size,
    _Inout_updates_(nprocs) int *recd_from_proc,
    _In_ int nprocs,
    _In_range_(>=,0) int contig_access_count,
    _In_ MPI_Offset min_st_offset,
    _In_ MPI_Offset fd_size,
    _In_ const MPI_Offset *fd_start,
    _In_ const MPI_Offset *fd_end,
    _In_ MPI_Aint buftype_extent
    )
{
/* this function is only called if buftype is not contig */

    MPIU_Assert( contig_access_count >= 0 );
    if( contig_access_count == 0 )
    {
        return;
    }

    OACR_USE_PTR( buf );

    int i, p, flat_buf_idx, size, buf_incr;
    int flat_buf_sz, size_in_buf, n_buftypes;
    MPI_Offset off, len, rem_len, user_buf_idx;
    int *curr_from_proc, *done_from_proc, *recv_buf_idx;

/*  curr_from_proc[p] = amount of data recd from proc. p that has already
                        been accounted for so far
    recv_buf_idx[p] = current location in recv_buf of proc. p
    done_from_proc[p] = amount of data already recd from proc. p and
                        filled into user buffer in previous iterations
    user_buf_idx = current location in user buffer  */

    curr_from_proc = (int *) ADIOI_Malloc(3 * nprocs * sizeof(int));
    recv_buf_idx = curr_from_proc + nprocs;
    done_from_proc = recv_buf_idx + nprocs;

    /* Zero curr_from_proc & recv_buf_idx */
    RtlZeroMemory(curr_from_proc, 2 * nprocs * sizeof(int));
    RtlCopyMemory(done_from_proc, recd_from_proc, nprocs * sizeof(int));

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

        /* this request may span the file domains of more than one process */
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

            if (recv_buf_idx[p] < recv_size[p])
            {
                if (curr_from_proc[p]+len > done_from_proc[p])
                {
                    if (done_from_proc[p] > curr_from_proc[p])
                    {
                        size = (int)min(curr_from_proc[p] + len -
                              done_from_proc[p], recv_size[p]-recv_buf_idx[p]);
                        buf_incr = done_from_proc[p] - curr_from_proc[p];
                        ADIOI_BUF_INCR
                        buf_incr = (int)(curr_from_proc[p]+len-done_from_proc[p]);
                        curr_from_proc[p] = done_from_proc[p] + size;
                    }
                    else
                    {
                        size = (int)min(len,recv_size[p]-recv_buf_idx[p]);
                        buf_incr = (int)len;
                        curr_from_proc[p] += size;
                    }
                    ADIOI_BUF_COPY
                }
                else
                {
                    curr_from_proc[p] += (int)len;
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
        if (recv_size[i])
        {
            recd_from_proc[i] = curr_from_proc[i];
        }
    } while (i > 0);

    ADIOI_Free(curr_from_proc);
}


#define ADIOI_BUFFERED_READ \
{ \
    if (req_off >= readbuf_off + readbuf_len) { \
        readbuf_off = req_off; \
        readbuf_len = (int) (min(max_bufsize, end_offset-readbuf_off+1));\
        mpi_errno = ADIO_ReadContig(fd, readbuf, readbuf_len, MPI_BYTE, \
              ADIO_EXPLICIT_OFFSET, readbuf_off, &status1); \
        if (mpi_errno != MPI_SUCCESS) return mpi_errno; \
    } \
    while (req_len > readbuf_off + readbuf_len - req_off) { \
        partial_read = (int) (readbuf_off + readbuf_len - req_off); \
        tmp_buf = (char *) ADIOI_Malloc(partial_read); \
        memcpy(tmp_buf, readbuf+readbuf_len-partial_read, partial_read); \
        ADIOI_Free(readbuf); \
        readbuf = (char *) ADIOI_Malloc(partial_read + max_bufsize); \
        memcpy(readbuf, tmp_buf, partial_read); \
        ADIOI_Free(tmp_buf); \
        readbuf_off += readbuf_len-partial_read; \
        readbuf_len = (int) (partial_read + min(max_bufsize, \
                                       end_offset-readbuf_off+1)); \
        mpi_errno = ADIO_ReadContig(fd, readbuf+partial_read, readbuf_len-partial_read, \
             MPI_BYTE, ADIO_EXPLICIT_OFFSET, readbuf_off+partial_read, \
             &status1); \
        if (mpi_errno != MPI_SUCCESS) return mpi_errno; \
    } \
    memcpy((char *)buf + userbuf_off, readbuf+req_off-readbuf_off, req_len); \
}


int
ADIOI_GEN_ReadStrided(MPI_File fd, _Out_ void *buf, int count,
                       MPI_Datatype datatype, int file_ptr_type,
                       MPI_Offset offset, MPI_Status *status
                       )
{
/* offset is in units of etype relative to the filetype. */

    const ADIOI_Flatlist_node *flat_buf, *flat_file;
    int i, j, k, brd_size, frd_size=0, st_index=0;
    int bufsize, num, size, sum, n_etypes_in_filetype, size_in_filetype;
    int n_filetypes, etype_in_filetype;
    MPI_Offset abs_off_in_filetype=0;
    int filetype_size, etype_size, buftype_size, req_len, partial_read;
    MPI_Aint filetype_extent, filetype_lb, buftype_extent, buftype_lb;
    int buf_count, buftype_is_contig, filetype_is_contig;
    MPI_Offset userbuf_off;
    MPI_Offset off, req_off, disp, end_offset=0, readbuf_off, start_off;
    char *readbuf, *tmp_buf, *value;
    int st_frd_size, st_n_filetypes, readbuf_len;
    int new_brd_size, new_frd_size, info_flag, max_bufsize;
    MPI_Status status1;
    int mpi_errno;
    int flatfilecount;

    //
    // fix OACR warning 6101 for buf
    //
    OACR_USE_PTR(buf);

    if (fd->hints->ds_read == ADIOI_HINT_DISABLE)
    {
        /* if user has disabled data sieving on reads, use naive
         * approach instead.
         */
        return ADIOI_GEN_ReadStrided_naive(
                    fd,
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

    value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));
    NMPI_Info_get(fd->info, const_cast<char*>("ind_rd_buffer_size"), MPI_MAX_INFO_VAL, value,
                 &info_flag);
    max_bufsize = atoi(value);
    ADIOI_Free(value);

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
        readbuf_off = off;
        readbuf = (char *) ADIOI_Malloc(max_bufsize);
        readbuf_len = (int) (min(max_bufsize, end_offset-readbuf_off+1));

        /* if atomicity is true, lock (exclusive) the region to be accessed */
        if ((fd->atomicity))
        {
            ADIOI_WRITE_LOCK(fd, start_off, end_offset-start_off+1);
        }

        mpi_errno = ADIO_ReadContig(fd, readbuf, readbuf_len, MPI_BYTE,
                        ADIO_EXPLICIT_OFFSET, readbuf_off, &status1);

        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }

        for (j=0; j<count; j++)
        {
            for (i=0; i<flat_buf->count; i++)
            {
                userbuf_off = j*buftype_extent + flat_buf->indices[i];
                req_off = off;
                req_len = flat_buf->blocklens[i];
                ADIOI_BUFFERED_READ
                off += flat_buf->blocklens[i];
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

        ADIOI_Free(readbuf);
    }
    else
    {
        /* noncontiguous in file */

        /* filetype already flattened in ADIO_Open */
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype)
        {
            flat_file = flat_file->next;
        }
        disp = fd->disp;
        flatfilecount = flat_file->count;

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            /* Wei-keng reworked type processing to be a bit more efficient */
            offset       = fd->fp_ind - disp;
            n_filetypes  = static_cast<int>(((offset - flat_file->indices[0]) / filetype_extent));
            offset      -= static_cast<MPI_Offset>(n_filetypes * filetype_extent);
            /* now offset is local to this extent */

            /* find the block where offset is located, skip blocklens[i]==0 */
            for (i = 0; i < flatfilecount; i++)
            {
                MPI_Offset dist;
                if (flat_file->blocklens[i] == 0)
                {
                    continue;
                }
                dist = flat_file->indices[i] + flat_file->blocklens[i] - offset;
                /* frd_size is from offset to the end of block i */
                if (dist == 0)
                {
                    i++;
                    offset   = flat_file->indices[i];
                    frd_size = flat_file->blocklens[i];
                    break;
                }
                if (dist > 0)
                {
                    frd_size = static_cast<int>(dist);
                    break;
                }
            }
            st_index = i;  /* starting index in flat_file->indices[] */
            offset += disp + static_cast<MPI_Offset>(n_filetypes*filetype_extent);
        }
        else
        {
            n_etypes_in_filetype = filetype_size/etype_size;
            n_filetypes = (int) (offset / n_etypes_in_filetype);
            etype_in_filetype = (int) (offset % n_etypes_in_filetype);
            size_in_filetype = etype_in_filetype * etype_size;

            sum = 0;
            for (i = 0; i < flatfilecount; i++)
            {
                sum += flat_file->blocklens[i];
                if (sum > size_in_filetype)
                {
                    st_index = i;
                    frd_size = sum - size_in_filetype;
                    abs_off_in_filetype = flat_file->indices[i] +
                        size_in_filetype - (sum - flat_file->blocklens[i]);
                    break;
                }
            }

            /* abs. offset in bytes in the file */
            offset = disp + (MPI_Offset) n_filetypes*filetype_extent + abs_off_in_filetype;
        }

        start_off = offset;

        /* Wei-keng Liao: read request is within a single flat_file contig
         * block e.g. with subarray types that actually describe the whole
         * array */
        if(buftype_is_contig && bufsize <= frd_size)
        {
            mpi_errno = ADIO_ReadContig(fd, buf, bufsize, MPI_BYTE, ADIO_EXPLICIT_OFFSET,
                                 offset, status);

            if(file_ptr_type == ADIO_INDIVIDUAL)
            {
                /* update MPI-IO file pointer to point to the first byte that
                 * can be accessed in the fileview. */
                fd->fp_ind = offset + bufsize;
                if (bufsize == frd_size)
                {
                    do
                    {
                        st_index++;
                        if (st_index == flatfilecount)
                        {
                            st_index = 0;
                            n_filetypes++;
                        }
                    } while (flat_file->blocklens[st_index] == 0);
                    fd->fp_ind = disp + flat_file->indices[st_index]
                                   + n_filetypes*filetype_extent;
                }
            }
            MPIR_Status_set_bytes(status, datatype, bufsize);
            return mpi_errno;
        }

       /* Calculate end_offset, the last byte-offset that will be accessed.
         e.g., if start_offset=0 and 100 bytes to be read, end_offset=99*/

        st_frd_size = frd_size;
        st_n_filetypes = n_filetypes;
        i = 0;
        j = st_index;
        off = offset;
        frd_size = min(st_frd_size, bufsize);
        while (i < bufsize)
        {
            i += frd_size;
            end_offset = off + frd_size - 1;

            do
            {
                if (++j >= flatfilecount)
                {
                    j = 0;
                    n_filetypes++;
                }
            } while (flat_file->blocklens[j] == 0);

            off = disp + flat_file->indices[j] + (MPI_Offset) n_filetypes*filetype_extent;
            frd_size = min(flat_file->blocklens[j], bufsize-i);
        }

        /* if atomicity is true, lock (exclusive) the region to be accessed */
        if ((fd->atomicity))
        {
            ADIOI_WRITE_LOCK(fd, start_off, end_offset-start_off+1);
        }

        readbuf_off = 0;
        readbuf_len = 0;
        readbuf = (char *) ADIOI_Malloc(max_bufsize);

        if (buftype_is_contig && !filetype_is_contig)
        {
            /* contiguous in memory, noncontiguous in file. should be the most
               common case. */

            i = 0;
            j = st_index;
            off = offset;
            n_filetypes = st_n_filetypes;
            frd_size = min(st_frd_size, bufsize);
            while (i < bufsize)
            {
                if (frd_size)
                {
                    req_off = off;
                    req_len = frd_size;
                    userbuf_off = i;
                    ADIOI_BUFFERED_READ
                }
                i += frd_size;

                if (off + frd_size < disp + flat_file->indices[j] +
                    flat_file->blocklens[j] + (MPI_Offset)n_filetypes*filetype_extent)
                {
                    off += frd_size;
                }
                /* did not reach end of contiguous block in filetype.
                   no more I/O needed. off is incremented by frd_size. */
                else
                {
                    do
                    {
                        ++j;
                        if (j >= flatfilecount)
                        {
                            j = 0;
                            n_filetypes++;
                        }
                    } while (flat_file->blocklens[j] == 0);

                    off = disp + flat_file->indices[j] +
                                        (MPI_Offset) n_filetypes*filetype_extent;
                    frd_size = min(flat_file->blocklens[j], bufsize-i);
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
            frd_size = st_frd_size;
            brd_size = flat_buf->blocklens[0];

            while (num < bufsize)
            {
                size = min(frd_size, brd_size);
                if (size)
                {
                    req_off = off;
                    req_len = size;
                    userbuf_off = i;
                    ADIOI_BUFFERED_READ
                }

                new_frd_size = frd_size;
                new_brd_size = brd_size;

                if (size == frd_size)
                {
                    /* reached end of contiguous block in file */
                    do
                    {
                        if (++j >= flatfilecount)
                        {
                            j = 0;
                            n_filetypes++;
                        }
                    } while (flat_file->blocklens[j] == 0);

                    off = disp + flat_file->indices[j] +
                                              (MPI_Offset) n_filetypes*filetype_extent;

                    new_frd_size = flat_file->blocklens[j];
                    if (size != brd_size)
                    {
                        i += size;
                        new_brd_size -= size;
                    }
                }

                if (size == brd_size)
                {
                    /* reached end of contiguous block in memory */

                    k = (k + 1)%flat_buf->count;
                    buf_count++;
                    i = (int) (buftype_extent*(buf_count/flat_buf->count) +
                        flat_buf->indices[k]);
                    new_brd_size = flat_buf->blocklens[k];
                    if (size != frd_size)
                    {
                        off += size;
                        new_frd_size -= size;
                    }
                }
                num += size;
                frd_size = new_frd_size;
                brd_size = new_brd_size;
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

        ADIOI_Free(readbuf); /* malloced in the buffered_read macro */
    }

    /* This is a temporary way of filling in status. The right way is to
       keep track of how much data was actually read and placed in buf
       by ADIOI_BUFFERED_READ. */
    MPIR_Status_set_bytes(status, datatype, bufsize);

    if (!buftype_is_contig)
    {
        ADIOI_Delete_flattened(datatype);
    }

    return MPI_SUCCESS;
}


int ADIOI_GEN_ReadStrided_naive(MPI_File fd, void *buf, int count,
                       MPI_Datatype buftype, int file_ptr_type,
                       MPI_Offset offset, MPI_Status *status
                       )
{
    /* offset is in units of etype relative to the filetype. */

    const ADIOI_Flatlist_node *flat_buf, *flat_file;
    int brd_size, frd_size=0, b_index;
    int bufsize, size, sum, n_etypes_in_filetype, size_in_filetype;
    int n_filetypes, etype_in_filetype;
    MPI_Offset abs_off_in_filetype=0;
    int filetype_size, etype_size, buftype_size, req_len;
    MPI_Aint filetype_extent, filetype_lb, buftype_extent, buftype_lb;
    int buf_count, buftype_is_contig, filetype_is_contig;
    MPI_Offset userbuf_off;
    MPI_Offset off, req_off, disp, end_offset=0, start_off;
    MPI_Status status1;
    int flatfilecount;

    int mpi_errno;

    ADIOI_Datatype_iscontig(buftype, &buftype_is_contig);
    ADIOI_Datatype_iscontig(fd->filetype, &filetype_is_contig);

    NMPI_Type_size(fd->filetype, &filetype_size);
    if ( ! filetype_size )
        return MPI_SUCCESS;

    NMPI_Type_get_extent(fd->filetype, &filetype_lb, &filetype_extent);
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

                mpi_errno = ADIO_ReadContig(
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

        if (file_ptr_type == ADIO_INDIVIDUAL) fd->fp_ind = off;

    }

    else  /* noncontiguous in file */
    {
        int f_index, st_frd_size, st_index = 0, st_n_filetypes;
        int flag;

        /* First we're going to calculate a set of values for use in all
         * the noncontiguous in file cases:
         * start_off - starting byte position of data in file
         * end_offset - last byte offset to be acessed in the file
         * st_n_filetypes - how far into the file we start in terms of
         *                  whole filetypes
         * st_index - index of block in first filetype that we will be
         *            starting in (?)
         * st_frd_size - size of the data in the first filetype block
         *               that we will read (accounts for being part-way
         *               into reading this block of the filetype
         *
         */

        /* filetype already flattened in ADIO_Open */
        flat_file = ADIOI_Flatlist;
        while (flat_file->type != fd->filetype) flat_file = flat_file->next;
        disp = fd->disp;

        flatfilecount = flat_file->count;

        if (file_ptr_type == ADIO_INDIVIDUAL)
        {
            start_off = fd->fp_ind; /* in bytes */
            n_filetypes = -1;
            flag = 0;
            do
            {
                n_filetypes++;
                for (f_index = 0; f_index < flatfilecount; f_index++)
                {
                    if (disp + flat_file->indices[f_index] +
                       (MPI_Offset) n_filetypes*filetype_extent +
                       flat_file->blocklens[f_index] >= start_off)
                    {
                        /* this block contains our starting position */

                        st_index = f_index;
                        frd_size = (int) (disp + flat_file->indices[f_index] +
                                   (MPI_Offset) n_filetypes*filetype_extent +
                                   flat_file->blocklens[f_index] - start_off);
                        flag = 1;
                        break;
                    }
                }
            } while (flag == 0);
        }
        else
        {
            n_etypes_in_filetype = filetype_size/etype_size;
            n_filetypes = (int) (offset / n_etypes_in_filetype);
            etype_in_filetype = (int) (offset % n_etypes_in_filetype);
            size_in_filetype = etype_in_filetype * etype_size;

            sum = 0;
            for (f_index = 0; f_index < flatfilecount; f_index++)
            {
                sum += flat_file->blocklens[f_index];
                if (sum > size_in_filetype)
                {
                    st_index = f_index;
                    frd_size = sum - size_in_filetype;
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

        st_frd_size = frd_size;
        st_n_filetypes = n_filetypes;

        /* start_off, st_n_filetypes, st_index, and st_frd_size are
         * all calculated at this point
         */

        /* Calculate end_offset, the last byte-offset that will be accessed.
         * e.g., if start_off=0 and 100 bytes to be read, end_offset=99
         */
        userbuf_off = 0;
        f_index = st_index;
        off = start_off;
        frd_size = min(st_frd_size, bufsize);
        while (userbuf_off < bufsize)
        {
            userbuf_off += frd_size;
            end_offset = off + frd_size - 1;

            ++f_index;
            if (f_index >= flatfilecount)
            {
                f_index = 0;
                n_filetypes++;
            }

            off = disp + flat_file->indices[f_index] +
                  (MPI_Offset) n_filetypes*filetype_extent;
            frd_size = min(flat_file->blocklens[f_index],
                                 bufsize-(int)userbuf_off);
        }

        /* End of calculations.  At this point the following values have
         * been calculated and are ready for use:
         * - start_off
         * - end_offset
         * - st_n_filetypes
         * - st_index
         * - st_frd_size
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
            frd_size = min(st_frd_size, bufsize);

            /* while there is still space in the buffer, read more data */
            while (userbuf_off < bufsize)
            {
                if (frd_size)
                {
                    /* TYPE_UB and TYPE_LB can result in
                       frd_size = 0. save system call in such cases */
                    req_off = off;
                    req_len = frd_size;

                    mpi_errno = ADIO_ReadContig(
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
                userbuf_off += frd_size;

                if (off + frd_size < disp + flat_file->indices[f_index] +
                   flat_file->blocklens[f_index] +
                   (MPI_Offset) n_filetypes*filetype_extent)
                {
                    /* important that this value be correct, as it is
                     * used to set the offset in the fd near the end of
                     * this function.
                     */
                    off += frd_size;
                }
                /* did not reach end of contiguous block in filetype.
                 * no more I/O needed. off is incremented by frd_size.
                 */
                else
                {
                    ++f_index;
                    if (f_index >= flatfilecount)
                    {
                        f_index = 0;
                        n_filetypes++;
                    }
                    off = disp + flat_file->indices[f_index] +
                          (MPI_Offset) n_filetypes*filetype_extent;
                    frd_size = min(flat_file->blocklens[f_index],
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
            frd_size = st_frd_size;
            brd_size = flat_buf->blocklens[0];

            /* while we haven't read size * count bytes, keep going */
            while (tmp_bufsize < bufsize)
            {
                int new_brd_size = brd_size, new_frd_size = frd_size;

                size = min(frd_size, brd_size);
                if (size)
                {
                    req_off = off;
                    req_len = size;
                    userbuf_off = i;

                    mpi_errno = ADIO_ReadContig(
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

                if (size == frd_size)
                {
                    /* reached end of contiguous block in file */
                    ++f_index;
                    if (f_index >= flatfilecount)
                    {
                        f_index = 0;
                        n_filetypes++;
                    }

                    off = disp + flat_file->indices[f_index] +
                          (MPI_Offset) n_filetypes*filetype_extent;

                    new_frd_size = flat_file->blocklens[f_index];
                    if (size != brd_size)
                    {
                        i += size;
                        new_brd_size -= size;
                    }
                }

                if (size == brd_size)
                {
                    /* reached end of contiguous block in memory */

                    b_index = (b_index + 1)%flat_buf->count;
                    buf_count++;
                    i = (int) (buftype_extent*(buf_count/flat_buf->count) +
                        flat_buf->indices[b_index]);
                    new_brd_size = flat_buf->blocklens[b_index];
                    if (size != frd_size)
                    {
                        off += size;
                        new_frd_size -= size;
                    }
                }
                tmp_bufsize += size;
                frd_size = new_frd_size;
                brd_size = new_brd_size;
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
     * keep track of how much data was actually read and placed in buf
     */
    MPIR_Status_set_bytes(status, buftype, bufsize);

    if (!buftype_is_contig)
    {
        ADIOI_Delete_flattened(buftype);
    }

    return MPI_SUCCESS;
}
