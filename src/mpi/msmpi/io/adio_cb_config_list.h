// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *   Copyright (C) 2001 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */

/* I have no idea what the "D" stands for; it's how things are done in adio.h
 */
struct ADIO_cb_name_arrayD
{
       int refct;
       int namect;
       char **names;
};

typedef struct ADIO_cb_name_arrayD *ADIO_cb_name_array;

int ADIOI_cb_gather_name_array(MPI_Comm comm, MPI_Comm dupcomm,
                               ADIO_cb_name_array *arrayp);

int ADIOI_cb_copy_name_array(MPI_Comm comm, int keyval, void *extra,
                             void *attr_in,
                             void *attr_out, int *flag);

int ADIOI_cb_delete_name_array(MPI_Comm comm, int keyval, void *attr_val,
                               void *extra);

_Success_( return != -1 )
int
ADIOI_cb_config_list_parse(
    _In_opt_z_ PCSTR config_list,
    _In_ const ADIO_cb_name_array array,
    _In_reads_(cb_nodes) int ranklist[],
    _In_ int cb_nodes
    );

int ADIOI_cb_bcast_rank_map(MPI_File fd);
