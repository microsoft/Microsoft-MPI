// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef MPICH_NMPI_H_INCLUDED
#define MPICH_NMPI_H_INCLUDED

/*
 * This file provides a flexible way to map MPI routines that are used
 * within the MPICH2 implementation to either the MPI or PMPI versions.
 * In normal use, it is appropriate to use PMPI, but in some cases,
 * using the MPI routines instead is desired.
 */

#define NMPI_Allgather MPI_Allgather
#define NMPI_Allreduce MPI_Allreduce
#define NMPI_Alltoall MPI_Alltoall
#define NMPI_Barrier MPI_Barrier
#define NMPI_Bcast MPI_Bcast
#define NMPI_Gather MPI_Gather
#define NMPI_Gatherv MPI_Gatherv
#define NMPI_Recv MPI_Recv
#define NMPI_Reduce MPI_Reduce
#define NMPI_Reduce_local MPI_Reduce_local
#define NMPI_Reduce_scatter_block MPI_Reduce_scatter_block
#define NMPI_Reduce_scatter MPI_Reduce_scatter
#define NMPI_Send MPI_Send
#define NMPI_Sendrecv MPI_Sendrecv
#define NMPI_Wait MPI_Wait
#define NMPI_Waitall MPI_Waitall
#define NMPI_Allgatherv MPI_Allgatherv
#define NMPI_Alltoallw MPI_Alltoallw
#define NMPI_Alltoallv MPI_Alltoallv
#define NMPI_Exscan MPI_Exscan
#define NMPI_Probe MPI_Probe
#define NMPI_Rsend MPI_Rsend
#define NMPI_Scan MPI_Scan
#define NMPI_Scatter MPI_Scatter
#define NMPI_Scatterv MPI_Scatterv
#define NMPI_Sendrecv_replace MPI_Sendrecv_replace
#define NMPI_Ssend MPI_Ssend
#define NMPI_Waitany MPI_Waitany
#define NMPI_Waitsome MPI_Waitsome
#define NMPI_Abort MPI_Abort
#define NMPI_Buffer_detach MPI_Buffer_detach
#define NMPI_Cancel MPI_Cancel
#define NMPI_Cart_map  MPI_Cart_map
#define NMPI_Cart_rank MPI_Cart_rank
#define NMPI_Close_port MPI_Close_port
#define NMPI_Comm_accept MPI_Comm_accept
#define NMPI_Comm_call_errhandler MPI_Comm_call_errhandler
#define NMPI_Comm_connect MPI_Comm_connect
#define NMPI_Comm_create_errhandler MPI_Comm_create_errhandler
#define NMPI_Comm_create_keyval MPI_Comm_create_keyval
#define NMPI_Comm_delete_attr MPI_Comm_delete_attr
#define NMPI_Comm_dup MPI_Comm_dup
#define NMPI_Comm_free MPI_Comm_free
#define NMPI_Comm_free_keyval MPI_Comm_free_keyval
#define NMPI_Comm_get_attr MPI_Comm_get_attr
#define NMPI_Comm_get_errhandler MPI_Comm_get_errhandler
#define NMPI_Comm_get_name MPI_Comm_get_name
#define NMPI_Comm_group MPI_Comm_group
#define NMPI_Comm_rank MPI_Comm_rank
#define NMPI_Comm_remote_group MPI_Comm_remote_group
#define NMPI_Comm_set_attr MPI_Comm_set_attr
#define NMPI_Comm_set_errhandler MPI_Comm_set_errhandler
#define NMPI_Comm_size MPI_Comm_size
#define NMPI_Comm_split MPI_Comm_split
#define NMPI_Comm_test_inter MPI_Comm_test_inter
#define NMPI_Get_address MPI_Get_address
#define NMPI_Get_count MPI_Get_count
#define NMPI_Get_processor_name MPI_Get_processor_name
#define NMPI_Graph_map  MPI_Graph_map
#define NMPI_Grequest_complete MPI_Grequest_complete
#define NMPI_Grequest_start MPI_Grequest_start
#define NMPI_Group_compare MPI_Group_compare
#define NMPI_Group_free MPI_Group_free
#define NMPI_Group_translate_ranks MPI_Group_translate_ranks
#define NMPI_Iallgather MPI_Iallgather
#define NMPI_Iallgatherv MPI_Iallgatherv
#define NMPI_Iallreduce MPI_Iallreduce
#define NMPI_Ialltoall MPI_Ialltoall
#define NMPI_Ialltoallv MPI_Ialltoallv
#define NMPI_Ialltoallw MPI_Ialltoallw
#define NMPI_Ibarrier MPI_Ibarrier
#define NMPI_Ibcast MPI_Ibcast
#define NMPI_Ibsend MPI_Ibsend
#define NMPI_Iexscan MPI_Iexscan
#define NMPI_Igather MPI_Igather
#define NMPI_Igatherv MPI_Igatherv
#define NMPI_Ireduce_scatter MPI_Ireduce_scatter
#define NMPI_Ireduce_scatter_block MPI_Ireduce_scatter_block
#define NMPI_Iscan MPI_Iscan
#define NMPI_Iscatter MPI_Iscatter
#define NMPI_Iscatterv MPI_Iscatterv
#define NMPI_Info_create MPI_Info_create
#define NMPI_Info_delete MPI_Info_delete
#define NMPI_Info_dup MPI_Info_dup
#define NMPI_Info_free MPI_Info_free
#define NMPI_Info_get MPI_Info_get
#define NMPI_Info_get_nkeys MPI_Info_get_nkeys
#define NMPI_Info_get_nthkey MPI_Info_get_nthkey
#define NMPI_Info_get_valuelen MPI_Info_get_valuelen
#define NMPI_Info_set MPI_Info_set
#define NMPI_Initialized MPI_Initialized
#define NMPI_Iprobe MPI_Iprobe
#define NMPI_Irecv MPI_Irecv
#define NMPI_Ireduce MPI_Ireduce
#define NMPI_Isend MPI_Isend
#define NMPI_Open_port MPI_Open_port
#define NMPI_Pack MPI_Pack
#define NMPI_Pack_size MPI_Pack_size
#define NMPI_Status_set_cancelled MPI_Status_set_cancelled
#define NMPI_Status_set_elements MPI_Status_set_elements
#define NMPI_Test MPI_Test
#define NMPI_Test_cancelled MPI_Test_cancelled
#define NMPI_Type_commit MPI_Type_commit
#define NMPI_Type_contiguous MPI_Type_contiguous
#define NMPI_Type_create_hindexed MPI_Type_create_hindexed
#define NMPI_Type_create_hvector MPI_Type_create_hvector
#define NMPI_Type_create_indexed_block MPI_Type_create_indexed_block
#define NMPI_Type_create_hindexed_block MPI_Type_create_hindexed_block
#define NMPI_Type_create_struct MPI_Type_create_struct
#define NMPI_Type_free MPI_Type_free
#define NMPI_Type_get_contents MPI_Type_get_contents
#define NMPI_Type_get_envelope MPI_Type_get_envelope
#define NMPI_Type_get_extent MPI_Type_get_extent
#define NMPI_Type_get_true_extent MPI_Type_get_true_extent
#define NMPI_Type_indexed MPI_Type_indexed
#define NMPI_Type_size MPI_Type_size
#define NMPI_Type_vector MPI_Type_vector
#define NMPI_Unpack MPI_Unpack
#define NMPI_Wtime MPI_Wtime
#define NMPI_Request_free MPI_Request_free


#endif /* MPICH_NMPI_H_INCLUDED */
