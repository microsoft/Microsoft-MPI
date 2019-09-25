// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2015 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef TOPO_H
#define TOPO_H

typedef struct MPIR_Graph_topology
{
  int nnodes;
  int nedges;
  int *index;
  int *edges;
} MPIR_Graph_topology;

typedef struct MPIR_Cart_topology
{
  int nnodes;     /* Product of dims[*], gives the size of the topology */
  int ndims;
  int *dims;
  int *periodic;
  int *position;
} MPIR_Cart_topology;

typedef struct MPIR_Dist_graph_topology
{
  int indegree;
  int *in;
  int *in_weights;
  int outdegree;
  int *out;
  int *out_weights;
  bool is_weighted;
} MPIR_Dist_graph_topology;

typedef struct MPIR_Topology
{
  int kind;
  union topo
  {
    MPIR_Graph_topology graph;
    MPIR_Cart_topology  cart;
    MPIR_Dist_graph_topology dist_graph;
  } topo;
} MPIR_Topology;

_Success_(return != nullptr)
MPIR_Topology*
MPIR_Topology_get(
    _In_ const MPID_Comm* comm_ptr
    );

MPI_RESULT
MPIR_Topology_put(
    _In_opt_ const MPID_Comm* comm_ptr,
    _In_ MPIR_Topology* topo_ptr
    );

MPI_RESULT
MPIR_Cart_create(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int ndims,
    _In_reads_(ndims) const int dims[],
    _In_reads_(ndims) const int periods[],
    _In_ int reorder,
    _Out_ MPI_Comm* comm_cart
    );

MPI_RESULT
MPIR_Graph_create(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int nnodes,
    _In_reads_(nnodes) const int index[],
    _In_reads_(nnodes) const int edges[],
    _In_ int reorder,
    _Out_ MPI_Comm* comm_graph
    );

MPI_RESULT
MPIR_Dims_create(
    _In_range_(>, 0) int nnodes,
    _In_range_(>, 0) int ndims,
    _Inout_updates_(ndims) int dims[]
    );

MPI_RESULT
MPIR_Graph_map(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int nnodes,
    _In_reads_(nnodes) const int /*index*/[],
    _In_opt_ const int /*edges*/[],
    _Out_ int *newrank
    );

MPI_RESULT
MPIR_Cart_map(
    _In_ const MPID_Comm *comm_ptr,
    _In_ int ndims,
    _In_reads_(ndims) const int dims[],
    _In_opt_ const int periodic[],
    _Out_ int* newrank
    );

MPI_RESULT
MPIR_Dist_graph_create_adjacent(
    _In_ const MPID_Comm *comm_ptr,
    _In_range_(>=, 0) int indegree,
    _In_reads_(indegree) const int sources[],
    _In_reads_(indegree) const int sourceweights[],
    _In_range_(>=, 0) int outdegree,
    _In_reads_(outdegree) const int destinations[],
    _In_reads_(outdegree) const int destweights[],
    _In_opt_ MPID_Info* /*info_ptr*/,
    _In_opt_ int /*reorder*/,
    _Out_ MPI_Comm *comm_dist_graph
    );

MPI_RESULT
MPIR_Dist_graph_create(
    _In_ const MPID_Comm *comm_ptr,
    _In_range_(>=, 0) const int n,
    _In_reads_opt_(n) const int sources[],
    _In_reads_opt_(n) const int degrees[],
    _In_opt_ const int destinations[],
    _In_opt_ const int weights[],
    _In_opt_ MPID_Info* /*info_ptr*/,
    _In_opt_ int /*reorder*/,
    _Out_ MPI_Comm *comm_dist_graph
    );

#endif // TOPO_H
