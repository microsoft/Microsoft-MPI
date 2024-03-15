// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */


/* contains general definitions, declarations, and macros internal to
   the ADIO implementation, though independent of the underlying file
   system. This file is included in adio.h */

/* Functions and datataypes that are "internal" to the ADIO implementation
   prefixed ADIOI_. Functions and datatypes that are part of the
   "externally visible" (documented) ADIO interface are prefixed ADIO_.

   An implementation of MPI-IO, or any other high-level interface, should
   not need to use any of the ADIOI_ functions/datatypes.
   Only someone implementing ADIO on a new file system, or modifying
   an existing ADIO implementation, would need to use the ADIOI_
   functions/datatypes. */


#ifndef ADIOI_INCLUDE
#define ADIOI_INCLUDE
/* used to keep track of hint/info values.
 * Note that there are a lot of int-sized values in here...they are
 * used as int-sized entities other places as well.  This would be a
 * problem on 32-bit systems using > 2GB files in some cases...
 */
struct ADIOI_Hints
{
    int initialized;
    int striping_factor;
    int striping_unit;
    int cb_read;
    int cb_write;
    int cb_nodes;
    int cb_buffer_size;
    int ds_read;
    int ds_write;
    int no_indep_rw;
    int ind_rd_buffer_size;
    int ind_wr_buffer_size;
    int deferred_open;
    char* cb_config_list;
    int* ranklist;
    union
    {
        struct
        {
            int listio_read;
            int listio_write;
        } pvfs;

        struct
        {
            int debugmask;
        } pvfs2;
    } fs_hints;

};


/* Values for use with cb_read, cb_write, ds_read, and ds_write
 * and some fs-specific hints
   (IBM xlc, Compaq Tru64 compilers object to a comma after the last item)
   (that's just wrong)
 */
enum
{
    ADIOI_HINT_AUTO    = 0,
    ADIOI_HINT_ENABLE  = 1,
    ADIOI_HINT_DISABLE = 2
};

/* flattened datatypes. Each datatype is stored as a node of a
   globally accessible linked list. Once attribute caching on a
   datatype is available (in MPI-2), that should be used instead. */

struct ADIOI_Flatlist_node
{
    MPI_Datatype type;
    int count;                   /* no. of contiguous blocks */
    int* blocklens;              /* array of contiguous block lengths (bytes)*/
    /* may need to make it MPI_Offset *blocklens */
    MPI_Offset* indices;        /* array of byte offsets of each block */
    /* the type processing code in ROMIO loops through the flattened
     * representation to tile file views.  so, we cannot simply indicate a
     * lower bound and upper bound with entries here -- those are instead
     * indicated by offset-length pairs with zero length.  In order to deal
     * with repeatedly resetting the LB and UB though (as in resized of
     * resized, or struct of resized, perhaps?), indicate where in the
     * tuple-stream the lb and ub parts are kept
     * (-1 indicates "not explicitly set") */
    MPI_Offset lb_idx;
    MPI_Offset ub_idx;
    struct ADIOI_Flatlist_node* next;  /* pointer to next node */
};


struct ADIOI_AIO_Request
{
    EXOVERLAPPED exov;
    MPI_Request req;
    int ref_count;
    HANDLE file;
};

/* optypes for ADIO_RequestD */
#define ADIOI_READ                26
#define ADIOI_WRITE               27


#define ADIOI_PREALLOC_BUFSZ      16777216    /* buffer size used to
                                                preallocate disk space */


/* default values for some hints */
    /* buffer size for collective I/O = 16 MB */
#define ADIOI_CB_BUFFER_SIZE_DFLT         "16777216"
    /* buffer size for data sieving in independent reads = 4MB */
#define ADIOI_IND_RD_BUFFER_SIZE_DFLT     "4194304"
    /* buffer size for data sieving in independent writes = 512KB. default is
       smaller than for reads, because write requires read-modify-write
       with file locking. If buffer size is large there is more contention
       for locks. */
#define ADIOI_IND_WR_BUFFER_SIZE_DFLT     "524288"
    /* use one process per processor name by default */
#define ADIOI_CB_CONFIG_LIST_DFLT "*:1"


/* structure for storing access info of this process's request
   from the file domain of other processes, and vice-versa. used
   as array of structures indexed by process number. */
struct ADIOI_Access
{
    MPI_Offset* offsets;   /* array of offsets */
    int* lens;              /* array of lengths */
    MPI_Aint* mem_ptrs;     /* array of pointers. used in the read/write
                               phase to indicate where the data
                               is stored in memory */
    int count;             /* size of above arrays */
};


/* prototypes for ADIO internal functions */

void
ADIOI_Flatten_datatype(
    MPI_Datatype type
    );

void
ADIOI_Flatten(
    MPI_Datatype type,
    ADIOI_Flatlist_node* flat,
    MPI_Offset st_offset,
    int* curr_index
    );

void
ADIOI_Delete_flattened(
    MPI_Datatype datatype
    );

int
ADIOI_Count_contiguous_blocks(
    MPI_Datatype type,
    int* curr_index
    );

_Ret_notnull_
void*
ADIOI_Malloc(
    _In_ size_t size
    );

_Ret_notnull_
void*
ADIOI_Calloc(
    _In_ size_t nelem,
    _In_ size_t elsize
    );

_Ret_notnull_
void*
ADIOI_Realloc(
    _In_ void* ptr,
    _In_ size_t size
    );

void
ADIOI_Free(
    _In_opt_ _Post_ptr_invalid_ void* ptr
    );

void
ADIOI_Datatype_iscontig(
    MPI_Datatype datatype,
    int* flag
    );

void
ADIOI_Get_position(
    MPI_File fd,
    MPI_Offset* offset
    );

void
ADIOI_Get_eof_offset(
    MPI_File fd,
    MPI_Offset* eof_offset
    );

void
ADIOI_Get_byte_offset(
    MPI_File fd,
    MPI_Offset offset,
    MPI_Offset* disp
    );

void
ADIOI_Calc_my_off_len(
    MPI_File fd,
    int bufcount,
    MPI_Datatype datatype,
    int file_ptr_type,
    MPI_Offset offset,
    MPI_Offset** offset_list_ptr,
    int** len_list_ptr,
    MPI_Offset* start_offset_ptr,
    MPI_Offset* end_offset_ptr,
    int* contig_access_count_ptr
    );

void
ADIOI_Calc_file_domains(
    const MPI_Offset* st_offsets,
    const MPI_Offset* end_offsets,
    int nprocs,
    int nprocs_for_coll,
    MPI_Offset* min_st_offset_ptr,
    MPI_Offset** fd_start_ptr,
    MPI_Offset** fd_end_ptr,
    MPI_Offset* fd_size_ptr
    );

int
ADIOI_Calc_aggregator(
    MPI_File fd,
    MPI_Offset off,
    MPI_Offset min_off,
    MPI_Offset* len,
    MPI_Offset fd_size,
    const MPI_Offset* fd_start,
    const MPI_Offset* fd_end
    );

void
ADIOI_Calc_my_req(
    MPI_File fd,
    const MPI_Offset* offset_list,
    const int* len_list,
    int contig_access_count,
    MPI_Offset min_st_offset,
    const MPI_Offset* fd_start,
    const MPI_Offset* fd_end,
    MPI_Offset fd_size,
    int nprocs,
    int* count_my_req_procs_ptr,
    int** count_my_req_per_proc_ptr,
    ADIOI_Access** my_req_ptr,
    int** buf_idx_ptr
    );

void
ADIOI_Calc_others_req(
    MPI_File fd,
    int count_my_req_procs,
    const int* count_my_req_per_proc,
    ADIOI_Access* my_req,
    int nprocs,
    int myrank,
    int* count_others_req_procs_ptr,
    ADIOI_Access** others_req_ptr
    );

int
ADIOI_Shfp_fname(
    MPI_File fd,
    int rank
    );

void
MPIR_Status_set_bytes(
    MPI_Status* status,
    MPI_Datatype datatype,
    int nbytes
    );


/* File I/O common functionality */
int
MPIOI_File_read(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    void* buf,
    int count,
    MPI_Datatype datatype,
    MPI_Status* status
    );

int
MPIOI_File_write(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    MPI_Status* status
    );

int
MPIOI_File_read_all(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    void* buf,
    int count,
    MPI_Datatype datatype,
    MPI_Status* status
    );

int
MPIOI_File_write_all(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    const void* buf,
    int count,
    MPI_Datatype datatype,
    MPI_Status* status
    );

int
MPIOI_File_read_all_begin(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    void* buf,
    int count,
    MPI_Datatype datatype
    );

int
MPIOI_File_write_all_begin(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    const void* buf,
    int count,
    MPI_Datatype datatype
    );

int
MPIOI_File_read_all_end(
    _In_ MPI_File fh,
    const void* buf,
    MPI_Status* status
    );

int
MPIOI_File_write_all_end(
    _In_ MPI_File fh,
    const void* buf,
    MPI_Status* status
    );

int
MPIOI_File_iwrite(
    _In_ MPI_File fh,
    _In_ MPI_Offset offset,
    _In_ int file_ptr_type,
    _In_reads_bytes_opt_(count) const void* buf,
    _In_ int count,
    _In_ MPI_Datatype datatype,
    _Inout_ MPI_Request* request
    );

int
MPIOI_File_iread(
    _In_ MPI_File fh,
    MPI_Offset offset,
    int file_ptr_type,
    void* buf,
    int count,
    MPI_Datatype datatype,
    MPI_Request* request
    );



#define ADIOI_LOCK_CMD          0
#define ADIOI_UNLOCK_CMD        1

#define ADIOI_WRITE_LOCK(fd, offset, len) \
          ADIOI_Set_lock((fd)->fd_sys, ADIOI_LOCK_CMD, LOCKFILE_EXCLUSIVE_LOCK, offset, len)
#define ADIOI_READ_LOCK(fd, offset, len) \
          ADIOI_Set_lock((fd)->fd_sys, ADIOI_LOCK_CMD, 0, offset, len)
#define ADIOI_UNLOCK(fd, offset, len) \
          ADIOI_Set_lock((fd)->fd_sys, ADIOI_UNLOCK_CMD, LOCKFILE_FAIL_IMMEDIATELY, offset, len)

int
ADIOI_Set_lock(
    FDTYPE fd_sys,
    int cmd,
    int type,
    MPI_Offset offset,
    MPI_Offset len
    );

#include "adioi_error.h"

#endif
