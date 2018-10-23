// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef DATALOOPP_H
#define DATALOOPP_H


#define DLOOP_Handle_get_loopptr_macro(handle_,lptr_,flag_) \
    lptr_ = TypePool::Get( handle_ )->GetDataloop( flag_ == DLOOP_DATALOOP_HETEROGENEOUS )

#define DLOOP_Handle_get_loopdepth_macro(handle_,depth_,flag_) \
    depth_ = TypePool::Get( handle_ )->GetDataloopDepth( flag_ == DLOOP_DATALOOP_HETEROGENEOUS )

#define DLOOP_Handle_get_loopsize_macro(handle_,size_,flag_) \
    size_ = TypePool::Get( handle_ )->GetDataloopSize( flag_ == DLOOP_DATALOOP_HETEROGENEOUS )

#define DLOOP_Handle_set_loopptr_macro(handle_,lptr_,flag_) \
    TypePool::Get( handle_ )->SetDataloop( flag_ == DLOOP_DATALOOP_HETEROGENEOUS, lptr_ )

#define DLOOP_Handle_set_loopdepth_macro(handle_,depth_,flag_) \
    TypePool::Get( handle_ )->SetDataloopDepth( flag_ == DLOOP_DATALOOP_HETEROGENEOUS, depth_ )

#define DLOOP_Handle_set_loopsize_macro(handle_,size_,flag_) \
    TypePool::Get( handle_ )->SetDataloopSize( flag_ == DLOOP_DATALOOP_HETEROGENEOUS, size_ )

#define DLOOP_Handle_get_size_macro MPID_Datatype_get_size

#define DLOOP_Handle_get_basic_type_macro MPID_Datatype_get_basic_type

#define DLOOP_Handle_get_extent_macro MPID_Datatype_get_extent

#define DLOOP_Handle_hasloop_macro(handle_)                           \
    ((HANDLE_GET_TYPE(handle_) == HANDLE_TYPE_BUILTIN) ? 0 : 1)


/* Dataloop construction functions */
MPI_RESULT
MPID_Dataloop_create_contiguous(
    _In_                                int              count,
    _In_                                MPI_Datatype     oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    );


MPI_RESULT
MPID_Dataloop_create_vector(
    _In_                                int              count,
    _In_                                int              blocklength,
    _In_                                MPI_Aint         stride,
    _In_                                int              strideinbytes,
    _In_                                MPI_Datatype     oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    );


MPI_RESULT
MPID_Dataloop_create_blockindexed(
    _In_                                int              icount,
    _In_                                int              iblklen,
    _In_                                const void*      disp_array,
    _In_                                int              dispinbytes,
    _In_                                DLOOP_Type       oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    );


MPI_RESULT
MPID_Dataloop_create_indexed(
    _In_                                int              icount,
    _In_reads_(icount)                  const int*       blocklength_array,
    _In_                                const void*      displacement_array,
    _In_                                int              dispinbytes,
    _In_                                MPI_Datatype     oldtype,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop** dlp_p,
    _Out_                               int*             dlsz_p,
    _Out_                               int*             dldepth_p,
    _In_                                int              flag
    );


MPI_RESULT
MPID_Dataloop_create_struct(
    _In_                                int               count,
    _In_reads_(count)                   const int*        blklens,
    _In_reads_(count)                   const MPI_Aint*   disps,
    _In_                                const DLOOP_Type* oldtypes,
    _Outptr_result_bytebuffer_(*dlsz_p) DLOOP_Dataloop**  dlp_p,
    _Out_                               int*              dlsz_p,
    _Out_                               int*              dldepth_p,
    _In_                                int               flag
    );


/* Helper functions for dataloop construction */
MPI_RESULT
MPID_Type_convert_subarray(
    int ndims,
    const int *array_of_sizes,
    const int *array_of_subsizes,
    const int *array_of_starts,
    int order,
    MPI_Datatype oldtype,
    MPI_Datatype *newtype
    );


MPI_RESULT
MPID_Type_convert_darray(
    int size,
    int rank,
    int ndims,
    const int *array_of_gsizes,
    const int *array_of_distribs,
    const int *array_of_dargs,
    const int *array_of_psizes,
    int order,
    MPI_Datatype oldtype,
    MPI_Datatype *newtype
    );


void
MPID_Dataloop_copy(
    _Out_writes_bytes_all_(size) DLOOP_Dataloop*       dest,
    _In_reads_bytes_(size)       const DLOOP_Dataloop* src,
    _In_                         int                   size
    );


MPI_RESULT
MPID_Dataloop_alloc(
    _In_                                       int              kind,
    _In_                                       DLOOP_Count      count,
    _Outptr_result_bytebuffer_(*new_loop_sz_p) DLOOP_Dataloop** new_loop_p,
    _Out_                                      int*             new_loop_sz_p
    );


MPI_RESULT
MPID_Dataloop_dup(
    _In_reads_bytes_(old_loop_sz)           const DLOOP_Dataloop* old_loop,
    _In_                                    int                   old_loop_sz,
    _Outptr_result_bytebuffer_(old_loop_sz) DLOOP_Dataloop**      new_loop_p
    );


MPI_RESULT
MPID_Dataloop_alloc_and_copy(
    _In_                                       int                   kind,
    _In_                                       DLOOP_Count           count,
    _In_reads_bytes_opt_(old_loop_sz)          const DLOOP_Dataloop* old_loop,
    _In_                                       int                   old_loop_sz,
    _Outptr_result_bytebuffer_(*new_loop_sz_p) DLOOP_Dataloop**      new_loop_p,
    _Out_                                      int*                  new_loop_sz_p
    );


MPI_RESULT
MPID_Dataloop_struct_alloc(
    _In_                                       DLOOP_Count count,
    _In_                                       int old_loop_sz,
    _In_                                       int basic_ct,
    _Outptr_                                   DLOOP_Dataloop** old_loop_p,
    _Outptr_result_bytebuffer_(*new_loop_sz_p) DLOOP_Dataloop** new_loop_p,
    _Out_                                      int* new_loop_sz_p
    );



/* Common segment operations (segment_ops.c) */
void MPID_Segment_count_contig_blocks(DLOOP_Segment *segp,
                                                 DLOOP_Offset first,
                                                 DLOOP_Offset *lastp,
                                                 DLOOP_Count *countp);
void MPID_Segment_mpi_flatten(DLOOP_Segment *segp,
                                         DLOOP_Offset first,
                                         DLOOP_Offset *lastp,
                                         int *blklens,
                                         MPI_Aint *disps,
                                         int *lengthp);

#define DLOOP_M2M_TO_USERBUF   0
#define DLOOP_M2M_FROM_USERBUF 1

struct MPID_m2m_params
{
    int direction; /* M2M_TO_USERBUF or M2M_FROM_USERBUF */
    char *streambuf;
    char *userbuf;
};

#endif //DATALOOPP_H
