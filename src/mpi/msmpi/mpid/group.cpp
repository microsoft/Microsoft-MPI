// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


#ifndef MPID_GROUP_PREALLOC
#define MPID_GROUP_PREALLOC 8
#endif

C_ASSERT( HANDLE_GET_TYPE(MPI_GROUP_NULL) == HANDLE_TYPE_INVALID );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_GROUP_NULL) == MPID_GROUP );

/* Preallocated group objects */
MPID_Group MPID_Group_builtin[MPID_GROUP_N_BUILTIN] = {
    { MPI_GROUP_EMPTY, 1, 0, MPI_UNDEFINED, -1, 0, } };
MPID_Group MPID_Group_direct[MPID_GROUP_PREALLOC] = { {0} };
MPIU_Object_alloc_t MPID_Group_mem = { 0, 0, 0, 0, MPID_GROUP,
                                      sizeof(MPID_Group), MPID_Group_direct,
                                       MPID_GROUP_PREALLOC};

MPI_RESULT
MPIR_Group_release(
    _Post_ptr_invalid_ MPID_Group *group_ptr
    )
{
    int mpi_errno = MPI_SUCCESS;
    int inuse;

    MPIR_Group_release_ref(group_ptr, &inuse);
    if (!inuse)
    {
        /* Only if refcount is 0 do we actually free. */
        MPIU_Free(group_ptr->lrank_to_lpid);
        MPIU_Handle_obj_free( &MPID_Group_mem, group_ptr );
    }
    return mpi_errno;
}


/*
 * Allocate a new group and the group lrank to lpid array.  Does *not*
 * initialize any arrays, but does set the reference count.
 */

MPI_RESULT
MPIR_Group_create(
    _In_ int nproc,
    _Outptr_ MPID_Group** new_group_ptr
    )
{
    *new_group_ptr = (MPID_Group *)MPIU_Handle_obj_alloc( &MPID_Group_mem );
    if( *new_group_ptr == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    /* printf( "new group ptr is %x handle %x\n", (int)*new_group_ptr,
       (*new_group_ptr)->handle );fflush(stdout); */
    (*new_group_ptr)->lrank_to_lpid = MPIU_Malloc_objn(nproc, MPID_Group_pmap_t);
    if ((*new_group_ptr)->lrank_to_lpid == nullptr)
    {
        MPIU_Handle_obj_free( &MPID_Group_mem, *new_group_ptr );
        *new_group_ptr = nullptr;
        return MPIU_ERR_NOMEM();
    }
    (*new_group_ptr)->size = nproc;
    /* Make sure that there is no question that the list of ranks sorted
       by pids is marked as uninitialized */
    (*new_group_ptr)->idx_of_first_lpid = -1;

    return MPI_SUCCESS;
}
/*
 * return value is the first index in the list
 *
 * This sorts an lpid array by lpid value, using a simple merge sort
 * algorithm.
 *
 */

static int
MPIR_Mergesort_lpidarray(
    _Inout_updates_(n) MPID_Group_pmap_t maparray[],
    _In_ int n
    )
{
    int idx1, idx2, first_idx, cur_idx, next_lpid, idx2_offset;

    if (n == 2)
    {
        if (maparray[0].lpid > maparray[1].lpid)
        {
            first_idx = 1;
            maparray[0].next_lpid = -1;
            maparray[1].next_lpid = 0;
        }
        else
        {
            first_idx = 0;
            maparray[0].next_lpid = 1;
            maparray[1].next_lpid = -1;
        }
        return first_idx;
    }
    if (n == 1)
    {
        maparray[0].next_lpid = -1;
        return 0;
    }
    if (n == 0)
        return -1;

    /* Sort each half */
    idx2_offset = n/2;
    idx1 = MPIR_Mergesort_lpidarray( maparray, n/2 );
    idx2 = MPIR_Mergesort_lpidarray( maparray + idx2_offset, n - n/2 ) + idx2_offset;
    /* merge the results */
    /* There are three lists:
       first_idx - points to the HEAD of the sorted, merged list
       cur_idx - points to the LAST element of the sorted, merged list
       idx1    - points to the HEAD of one sorted list
       idx2    - points to the HEAD of the other sorted list

       We first identify the head element of the sorted list.  We then
       take elements from the remaining lists.  When one list is empty,
       we add the other list to the end of sorted list.

       The last wrinkle is that the next_lpid fields in maparray[idx2]
       are relative to n/2, not 0 (that is, a next_lpid of 1 is
       really 1 + n/2, relative to the beginning of maparray).
    */
    /* Find the head element */
    if (maparray[idx1].lpid > maparray[idx2].lpid)
    {
        first_idx = idx2;
        idx2      = maparray[idx2].next_lpid + idx2_offset;
    }
    else
    {
        first_idx = idx1;
        idx1      = maparray[idx1].next_lpid;
    }

    /* Merge the lists until one is empty */
    cur_idx = first_idx;
    while ( idx1 >= 0 && idx2 >= 0)
    {
        if (maparray[idx1].lpid > maparray[idx2].lpid)
        {
            next_lpid                   = maparray[idx2].next_lpid;
            if (next_lpid >= 0) next_lpid += idx2_offset;
            maparray[cur_idx].next_lpid = idx2;
            cur_idx                     = idx2;
            idx2                        = next_lpid;
        }
        else
        {
            next_lpid                   = maparray[idx1].next_lpid;
            maparray[cur_idx].next_lpid = idx1;
            cur_idx                     = idx1;
            idx1                        = next_lpid;
        }
    }
    /* Add whichever list remains */
    if (idx1 >= 0)
    {
        maparray[cur_idx].next_lpid = idx1;
    }
    else
    {
        maparray[cur_idx].next_lpid = idx2;
        /* Convert the rest of these next_lpid values to be
           relative to the beginning of maparray */
        while (idx2 >= 0)
        {
            next_lpid = maparray[idx2].next_lpid;
            if (next_lpid >= 0)
            {
                next_lpid += idx2_offset;
                maparray[idx2].next_lpid = next_lpid;
            }
            idx2 = next_lpid;
        }
    }

    return first_idx;
}

/*
 * Create a list of the lpids, in lpid order.
 *
 * Called by group_compare, group_translate_ranks, group_union
 *
 * In the case of a single master thread lock, the lock must
 * be held on entry to this routine.  This forces some of the routines
 * noted above to hold the SINGLE_CS; which would otherwise not be required.
 */

void
MPIR_Group_setup_lpid_list(
    _Inout_ MPID_Group *group_ptr
    )
{
    if (group_ptr->idx_of_first_lpid == -1)
    {
        group_ptr->idx_of_first_lpid =
            MPIR_Mergesort_lpidarray( group_ptr->lrank_to_lpid,
                                      group_ptr->size );
    }
}

void
MPIR_Group_setup_lpid_pairs(
    _Inout_ MPID_Group *group_ptr1,
    _Inout_ MPID_Group *group_ptr2
    )
{
    /* If the lpid list hasn't been created, do it now */
    if (group_ptr1->idx_of_first_lpid < 0)
    {
        MPIR_Group_setup_lpid_list( group_ptr1 );
    }
    if (group_ptr2->idx_of_first_lpid < 0)
    {
        MPIR_Group_setup_lpid_list( group_ptr2 );
    }
}

/*
 * The following routines are needed only for error checking
 */


/*
 * This routine is for error checking for a valid ranks array, used
 * by Group_incl and Group_excl.
 *
 * Note that because this uses the flag field in the group, it
 * must be used by only on thread at a time (per group).  For the SINGLE_CS
 * case, that means that the SINGLE_CS must be held on entry to this routine.
 */

MPI_RESULT
MPIR_Group_check_valid_ranks(
    _In_ MPID_Group* group_ptr,
    _In_reads_(n) const int ranks[],
    _In_ int n
    )
{
    int i;

    if (n < 0 || n > group_ptr->size)
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**rankarraysize %d %d", n, group_ptr->size);
    }

    for (i=0; i<group_ptr->size; i++)
    {
        group_ptr->lrank_to_lpid[i].flag = 0;
    }
    for (i=0; i<n; i++)
    {
        if (ranks[i] < 0 ||
            ranks[i] >= group_ptr->size)
        {
            return MPIU_ERR_CREATE( MPI_ERR_RANK,
                                    "**rankarray %d %d %d",
                                    i,
                                    ranks[i],
                                    group_ptr->size-1 );
        }
        if (group_ptr->lrank_to_lpid[ranks[i]].flag)
        {
            return MPIU_ERR_CREATE( MPI_ERR_RANK,
                                    "**rankdup %d %d %d",
                                    i,
                                    ranks[i],
                                    group_ptr->lrank_to_lpid[ranks[i]].flag-1 );
        }
        group_ptr->lrank_to_lpid[ranks[i]].flag = i+1;
    }

    return MPI_SUCCESS;
}

/* Service routine to check for valid range arguments.  This routine makes use
 of some of the internal fields in a group; in a multithreaded MPI program,
 these must ensure that only one thread is accessing the group at a time.
 In the SINGLE_CS model, this routine requires that the calling routine
 be within the SINGLE_CS (the routines are group_range_incl and
 group_range_excl) */

MPI_RESULT
MPIR_Group_check_valid_ranges(
    _In_ MPID_Group *group_ptr,
    _In_reads_(n) const int ranges[][3],
    _In_ int n
    )
{
    int i, j, size, first, last, stride;

    if (n < 0 || n > group_ptr->size)
    {
        return MPIU_ERR_CREATE( MPI_ERR_ARG,
                                "**rangessize %d %d",
                                n,
                                group_ptr->size);
    }

    size = group_ptr->size;

    /* First, clear the flag */
    for (i=0; i<size; i++)
    {
        group_ptr->lrank_to_lpid[i].flag = 0;
    }
    for (i=0; i<n; i++)
    {
        int act_last;

        first = ranges[i][0]; last = ranges[i][1];
        stride = ranges[i][2];
        if (first < 0 || first >= size)
        {
            return MPIU_ERR_CREATE( MPI_ERR_ARG,
                                    "**rangestartinvalid %d %d %d",
                                    i,
                                    first,
                                    size );
        }
        if (stride == 0)
        {
            return MPIU_ERR_CREATE( MPI_ERR_ARG, "**stridezero" );
        }

        /* We must compute the actual last value, taking into account
           the stride value.  At this point, we know that the stride is
           non-zero
        */
        act_last = first + stride * ((last - first) / stride);

        if (last < 0 || act_last >= size)
        {
            /* Use last instead of act_last in the error message since
               the last value is the one that the user provided */
            return MPIU_ERR_CREATE( MPI_ERR_ARG,
                                    "**rangeendinvalid %d %d %d",
                                    i,
                                    last,
                                    size );
        }
        if ( (stride > 0 && first > last) ||
             (stride < 0 && first < last) )
        {
            return MPIU_ERR_CREATE( MPI_ERR_ARG,
                                    "**stride %d %d %d",
                                    first,
                                    last,
                                    stride );
        }

        /* range is valid.  Mark flags */
        if (stride > 0)
        {
            for (j=first; j<=last; j+=stride)
            {
                if (group_ptr->lrank_to_lpid[j].flag)
                {
                    return MPIU_ERR_CREATE( MPI_ERR_ARG,
                                            "**rangedup %d %d %d",
                                            j,
                                            i,
                                            group_ptr->lrank_to_lpid[j].flag - 1 );
                }
                group_ptr->lrank_to_lpid[j].flag = 1;
            }
        }
        else
        {
            for (j=first; j>=last; j+=stride)
            {
                if (group_ptr->lrank_to_lpid[j].flag)
                {
                    return MPIU_ERR_CREATE( MPI_ERR_ARG,
                                            "**rangedup %d %d %d",
                                            j,
                                            i,
                                            group_ptr->lrank_to_lpid[j].flag - 1 );
                }
                /* Set to i + 1 so that we can remember where it was
                first set */
                group_ptr->lrank_to_lpid[j].flag = i + 1;
            }
        }
    }

    return MPI_SUCCESS;
}

/* Given a group and a VCR, check that the group is a subset of the processes
   defined by the VCR.

   We sort the lpids for the group and the vcr.  If the group has an
   lpid that is not in the vcr, then report an error.
*/
MPI_RESULT
MPIR_GroupCheckVCRSubset(
    _In_ MPID_Group *group_ptr,
    _In_ int vsize,
    _In_ MPID_VCR *vcr
    )
{
    int g1_idx, g2_idx, l1_pid, l2_pid, i;
    StackGuardArray<MPID_Group_pmap_t> vmap;

    MPIU_Assert(group_ptr != NULL);
    MPIU_Assert(vcr != NULL);

    vmap = new MPID_Group_pmap_t[vsize];
    if( vmap == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    /* Initialize the vmap */
    for (i=0; i<vsize; i++)
    {
        vmap[i].lpid      = MPID_VCR_Get_lpid( vcr[i] );
        vmap[i].lrank     = i;
        vmap[i].next_lpid = 0;
        vmap[i].flag      = 0;
    }

    MPIR_Group_setup_lpid_list( group_ptr );
    g1_idx = group_ptr->idx_of_first_lpid;
    g2_idx = MPIR_Mergesort_lpidarray( vmap, vsize );
    while (g1_idx >= 0 && g2_idx >= 0)
    {
        l1_pid = group_ptr->lrank_to_lpid[g1_idx].lpid;
        l2_pid = vmap[g2_idx].lpid;
        if (l1_pid < l2_pid)
        {
        /* If we have to advance g1, we didn't find a match, so
           that's an error. */
            break;
        }
        else if (l1_pid > l2_pid)
        {
            g2_idx = vmap[g2_idx].next_lpid;
        }
        else
        {
            /* Equal */
            g1_idx = group_ptr->lrank_to_lpid[g1_idx].next_lpid;
            g2_idx = vmap[g2_idx].next_lpid;
        }
    }

    if (g1_idx >= 0)
    {
        return MPIR_Err_create_code(
            MPI_SUCCESS,
            MPIR_ERR_RECOVERABLE,
            MPI_ERR_GROUP,
            "**groupnotincomm %d",
            g1_idx
            );
    }

    return MPI_SUCCESS;
}
