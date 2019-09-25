// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *  (C) 2009 by Microsoft Corporation.
 *      See COPYRIGHT in top-level directory.
 */

#pragma once

#ifndef GROUP_H
#define GROUP_H

/*---------------------------------------------------------------------------
 * Groups are *not* a major data structure in MPICH-2.  They are provided
 * only because they are required for the group operations (e.g.,
 * MPI_Group_intersection) and for the scalable RMA synchronization
 *---------------------------------------------------------------------------*/
/* This structure is used to implement the group operations such as
   MPI_Group_translate_ranks */
struct MPID_Group_pmap_t
{
    int          lrank;     /* Local rank in group (between 0 and size-1) */
    int          lpid;      /* local process id, from VCONN */
    int          next_lpid; /* Index of next lpid (in lpid order) */
    int          flag;      /* marker, used to implement group operations */
};

/* Any changes in the MPID_Group structure must be made to the
   predefined value in MPID_Group_builtin for MPI_GROUP_EMPTY in
   src/mpi/group/grouputil.c */
/*S
 MPID_Group - Description of the Group data structure

 The processes in the group of 'MPI_COMM_WORLD' have lpid values 0 to 'size'-1,
 where 'size' is the size of 'MPI_COMM_WORLD'.  Processes created by
 'MPI_Comm_spawn' or 'MPI_Comm_spawn_multiple' or added by 'MPI_Comm_attach'
 or
 'MPI_Comm_connect'
 are numbered greater than 'size - 1' (on the calling process). See the
 discussion of LocalPID values.

 Note that when dynamic process creation is used, the pids are `not` unique
 across the universe of connected MPI processes.  This is ok, as long as
 pids are interpreted `only` on the process that owns them.

 Only for MPI-1 are the lpid''s equal to the `global` pids.  The local pids
 can be thought of as a reference not to the remote process itself, but
 how the remote process can be reached from this process.  We may want to
 have a structure 'MPID_Lpid_t' that contains information on the remote
 process, such as (for TCP) the hostname, ip address (it may be different if
 multiple interfaces are supported; we may even want plural ip addresses for
 stripping communication), and port (or ports).  For shared memory connected
 processes, it might have the address of a remote queue.  The lpid number
 is an index into a table of 'MPID_Lpid_t'''s that contain this (device- and
 method-specific) information.

 Module:
 Group-DS

 S*/
struct MPID_Group
{
    int          handle;
    volatile long ref_count;
    int          size;           /* Size of a group */
    int          rank;           /* rank of this process relative to this
                    group */
    int          idx_of_first_lpid;
    MPID_Group_pmap_t *lrank_to_lpid; /* Array mapping a local rank to local
                     process number */
};

extern MPIU_Object_alloc_t MPID_Group_mem;
/* Preallocated group objects */
#define MPID_GROUP_N_BUILTIN 1
extern MPID_Group MPID_Group_builtin[MPID_GROUP_N_BUILTIN];
extern MPID_Group MPID_Group_direct[];

#define MPIR_Group_add_ref( _group ) \
    MPIU_Object_add_ref( _group )

#define MPIR_Group_release_ref( _group, _inuse ) \
     MPIU_Object_release_ref( _group, _inuse )


MPI_RESULT
MPIR_Group_create(
    _In_ int nproc,
    _Outptr_ MPID_Group** new_group_ptr
    );

MPI_RESULT
MPIR_Group_release(
    _Post_ptr_invalid_ MPID_Group *group_ptr
    );

//
//Create a list of local process ids (lpid), in local process id order.
//
void
MPIR_Group_setup_lpid_list(
    _Inout_ MPID_Group *group_ptr
    );

void
MPIR_Group_setup_lpid_pairs(
    _Inout_ MPID_Group *group_ptr1,
    _Inout_ MPID_Group *group_ptr2
    );

//
//Given a group and a VCR, check that the group is a subset of the processes
//defined by the VCR.
//
MPI_RESULT
MPIR_GroupCheckVCRSubset(
    _In_ MPID_Group *group_ptr,
    _In_ int vsize,
    _In_ MPID_VCR *vcr
    );

MPI_RESULT
MPIR_Group_check_valid_ranks(
    _In_ MPID_Group* group_ptr,
    _In_reads_(n) const int ranks[],
    _In_ int n
    );

MPI_RESULT
MPIR_Group_check_valid_ranges(
    _In_ MPID_Group *group_ptr,
    _In_reads_(n) const int ranges[][3],
    _In_ int n
    );


#endif // GROUP_H
