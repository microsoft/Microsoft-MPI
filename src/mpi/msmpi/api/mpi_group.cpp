// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@

MPI_Group_compare - Compares two groups

Input Parameters:
+ group1 - group1 (handle)
- group2 - group2 (handle)

Output Parameter:
. result - integer which is 'MPI_IDENT' if the order and members of
the two groups are the same, 'MPI_SIMILAR' if only the members are the same,
and 'MPI_UNEQUAL' otherwise

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Group_compare(
    _In_ MPI_Group group1,
    _In_ MPI_Group group2,
    _Out_ int* result
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_compare(group1, group2);

    int g1_idx, g2_idx, size, i;

    MPID_Group *group_ptr1;
    int mpi_errno = MpiaGroupValidateHandle( group1, &group_ptr1 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Group *group_ptr2;
    mpi_errno = MpiaGroupValidateHandle( group2, &group_ptr2 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( result == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "result" );
        goto fn_fail;
    }

    /* See if their sizes are equal */
    if (group_ptr1->size != group_ptr2->size)
    {
        *result = MPI_UNEQUAL;
        goto fn_exit;
    }

    /* Run through the lrank to lpid lists of each group in lpid order
       to see if the same processes are involved */
    g1_idx = group_ptr1->idx_of_first_lpid;
    g2_idx = group_ptr2->idx_of_first_lpid;
    /* If the lpid list hasn't been created, do it now */
    if (g1_idx < 0)
    {
        MPIR_Group_setup_lpid_list( group_ptr1 );
        g1_idx = group_ptr1->idx_of_first_lpid;
    }
    if (g2_idx < 0)
    {
        MPIR_Group_setup_lpid_list( group_ptr2 );
        g2_idx = group_ptr2->idx_of_first_lpid;
    }
    while (g1_idx >= 0 && g2_idx >= 0)
    {
        if (group_ptr1->lrank_to_lpid[g1_idx].lpid !=
            group_ptr2->lrank_to_lpid[g2_idx].lpid)
        {
            *result = MPI_UNEQUAL;
            goto fn_exit;
        }
        g1_idx = group_ptr1->lrank_to_lpid[g1_idx].next_lpid;
        g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
    }

    /* See if the processes are in the same order by rank */
    size = group_ptr1->size;
    for (i=0; i<size; i++)
    {
        if (group_ptr1->lrank_to_lpid[i].lpid !=
            group_ptr2->lrank_to_lpid[i].lpid)
        {
            *result = MPI_SIMILAR;
            goto fn_exit;
        }
    }

    /* If we reach here, the groups are identical */
    *result = MPI_IDENT;

  fn_exit:
    TraceLeave_MPI_Group_compare(*result);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_compare %G %G %p",
            group1,
            group2,
            result
            )
        );
    TraceError(MPI_Group_compare, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Group_difference - Makes a group from the difference of two groups

Input Parameters:
+ group1 - first group (handle)
- group2 - second group (handle)

Output Parameter:
. newgroup - difference group (handle)

Notes:
The generated group containc the members of 'group1' that are not in 'group2'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Group_difference(
    _In_ MPI_Group group1,
    _In_ MPI_Group group2,
    _Out_ MPI_Group* newgroup
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_difference(group1, group2);

    MPID_Group *new_group_ptr;
    int size1, size2, i, k, g1_idx, g2_idx, l1_pid, l2_pid, nnew;

    MPID_Group *group_ptr1;
    int mpi_errno = MpiaGroupValidateHandle( group1, &group_ptr1 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Group *group_ptr2;
    mpi_errno = MpiaGroupValidateHandle( group2, &group_ptr2 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newgroup == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newgroup" );
        goto fn_fail;
    }

    /* Return a group consisting of the members of group1 that are *not*
       in group2 */
    size1 = group_ptr1->size;
    size2 = group_ptr2->size;
    /* Insure that the lpid lists are setup */
    MPIR_Group_setup_lpid_pairs( group_ptr1, group_ptr2 );

    for (i=0; i<size1; i++)
    {
        group_ptr1->lrank_to_lpid[i].flag = 0;
    }
    g1_idx = group_ptr1->idx_of_first_lpid;
    g2_idx = group_ptr2->idx_of_first_lpid;

    nnew = size1;
    while (g1_idx >= 0 && g2_idx >= 0)
    {
        l1_pid = group_ptr1->lrank_to_lpid[g1_idx].lpid;
        l2_pid = group_ptr2->lrank_to_lpid[g2_idx].lpid;
        if (l1_pid < l2_pid)
        {
            g1_idx = group_ptr1->lrank_to_lpid[g1_idx].next_lpid;
        }
        else if (l1_pid > l2_pid)
        {
            g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
        }
        else
        {
            /* Equal */
            group_ptr1->lrank_to_lpid[g1_idx].flag = 1;
            g1_idx = group_ptr1->lrank_to_lpid[g1_idx].next_lpid;
            g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
            nnew --;
        }
    }
    /* Create the group */
    if (nnew == 0)
    {
        /* See 5.3.2, Group Constructors.  For many group routines,
           the standard explicitly says to return MPI_GROUP_EMPTY;
           for others it is implied */
        *newgroup = MPI_GROUP_EMPTY;
        goto fn_exit;
    }
    else
    {
        mpi_errno = MPIR_Group_create( nnew, &new_group_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
        new_group_ptr->rank = MPI_UNDEFINED;
        k = 0;
        for (i=0; i<size1; i++)
        {
            if (!group_ptr1->lrank_to_lpid[i].flag)
            {
                new_group_ptr->lrank_to_lpid[k].lrank = k;
                new_group_ptr->lrank_to_lpid[k].lpid =
                    group_ptr1->lrank_to_lpid[i].lpid;
                if (i == group_ptr1->rank)
                    new_group_ptr->rank = k;
                k++;
            }
        }
    }

    *newgroup = new_group_ptr->handle;

  fn_exit:
    TraceLeave_MPI_Group_difference(*newgroup);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_difference %G %G %p",
            group1,
            group2,
            newgroup
            )
        );
    TraceError(MPI_Group_difference, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Group_excl - Produces a group by reordering an existing group and taking
        only unlisted members

Input Parameters:
+ group - group (handle)
. n - number of elements in array 'ranks' (integer)
- ranks - array of integer ranks in 'group' not to appear in 'newgroup'

Output Parameter:
. newgroup - new group derived from above, preserving the order defined by
 'group' (handle)

Note:
The MPI standard requires that each of the ranks to excluded must be
a valid rank in the group and all elements must be distinct or the
function is erroneous.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_EXHAUSTED
.N MPI_ERR_ARG
.N MPI_ERR_RANK

.seealso: MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Group_excl(
    _In_ MPI_Group group,
    _In_range_(>=, 0) int n,
    _In_reads_opt_(n) const int ranks[],
    _Out_ MPI_Group* newgroup
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_excl(group, n, TraceArrayLength(n), ranks);

    MPID_Group *new_group_ptr;
    int size, i, newi;

    MPID_Group *group_ptr;
    int mpi_errno = MpiaGroupValidateHandle( group, &group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( n < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "n", n );
        goto fn_fail;
    }

    if( ranks == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "ranks" );
        goto fn_fail;
    }

    if( newgroup == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newgroup" );
        goto fn_fail;
    }

    mpi_errno = MPIR_Group_check_valid_ranks( group_ptr, ranks, n );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Allocate a new group and lrank_to_lpid array */
    size = group_ptr->size;
    if (size == n)
    {
        *newgroup = MPI_GROUP_EMPTY;
        goto fn_exit;
    }
    mpi_errno = MPIR_Group_create( size - n, &new_group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    new_group_ptr->rank = MPI_UNDEFINED;
    /* Use flag fields to mark the members to *exclude* . */

    for (i=0; i<size; i++)
    {
        group_ptr->lrank_to_lpid[i].flag = 0;
    }
    for (i=0; i<n; i++)
    {
        group_ptr->lrank_to_lpid[ranks[i]].flag = 1;
    }

    newi = 0;
    for (i=0; i<size; i++)
    {
        if (group_ptr->lrank_to_lpid[i].flag == 0)
        {
            new_group_ptr->lrank_to_lpid[newi].lrank = newi;
            new_group_ptr->lrank_to_lpid[newi].lpid =
                group_ptr->lrank_to_lpid[i].lpid;
            if (group_ptr->rank == i) new_group_ptr->rank = newi;
            newi++;
        }
    }

    new_group_ptr->size = size - n;
    new_group_ptr->idx_of_first_lpid = -1;

    *newgroup = new_group_ptr->handle;

  fn_exit:
    TraceLeave_MPI_Group_excl(*newgroup);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_excl %G %d %p %p",
            group,
            n,
            ranks,
            newgroup
            )
        );
    TraceError(MPI_Group_excl, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Group_free - Frees a group

Input Parameter:
. group - group to free (handle)

Notes:
On output, group is set to 'MPI_GROUP_NULL'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_PERM_GROUP
@*/
EXTERN_C
MPI_METHOD
MPI_Group_free(
    _Inout_ MPI_Group* group
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_free(*group);

    int mpi_errno;
    if( group == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "group" );
        goto fn_fail;
    }

    MPID_Group *group_ptr;
    mpi_errno = MpiaGroupValidateHandle( *group, &group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Do not free MPI_GROUP_EMPTY */
    if( *group != MPI_GROUP_EMPTY )
    {
        mpi_errno = MPIR_Group_release(group_ptr);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    *group = MPI_GROUP_NULL;

    TraceLeave_MPI_Group_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_free %p",
            group
            )
        );
    TraceError(MPI_Group_free, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Group_incl - Produces a group by reordering an existing group and taking
        only listed members

Input Parameters:
+ group - group (handle)
. n - number of elements in array 'ranks' (and size of newgroup ) (integer)
- ranks - ranks of processes in 'group' to appear in 'newgroup' (array of
integers)

Output Parameter:
. newgroup - new group derived from above, in the order defined by 'ranks'
(handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_ARG
.N MPI_ERR_EXHAUSTED
.N MPI_ERR_RANK

.seealso: MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Group_incl(
    _In_ MPI_Group group,
    _In_range_(>=, 0) int n,
    _In_reads_opt_(n) const int ranks[],
    _Out_ MPI_Group* newgroup
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_incl(group, n, TraceArrayLength(n), ranks);

    MPID_Group *group_ptr;
    int mpi_errno = MpiaGroupValidateHandle( group, &group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( n < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "n", n );
        goto fn_fail;
    }

    if( n > 0 && ranks == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "ranks" );
        goto fn_fail;
    }

    if( newgroup == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newgroup" );
        goto fn_fail;
    }

    mpi_errno = MPIR_Group_check_valid_ranks( group_ptr, ranks, n );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (n == 0)
    {
        *newgroup = MPI_GROUP_EMPTY;
        goto fn_exit;
    }

    /* Allocate a new group and lrank_to_lpid array */
    MPID_Group *new_group_ptr = NULL;
    mpi_errno = MPIR_Group_create( n, &new_group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    new_group_ptr->rank = MPI_UNDEFINED;
    for( int i=0; i < n; i++ )
    {
        new_group_ptr->lrank_to_lpid[i].lrank = i;
        new_group_ptr->lrank_to_lpid[i].lpid  =
            group_ptr->lrank_to_lpid[ranks[i]].lpid;
        if (ranks[i] == group_ptr->rank) new_group_ptr->rank = i;
    }
    new_group_ptr->size = n;
    new_group_ptr->idx_of_first_lpid = -1;

    *newgroup = new_group_ptr->handle;

  fn_exit:
    TraceLeave_MPI_Group_incl(*newgroup);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_incl %G %d %p %p",
            group,
            n,
            ranks,
            newgroup
            )
        );
    TraceError(MPI_Group_incl, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Group_intersection - Produces a group as the intersection of two existing
                         groups

Input Parameters:
+ group1 - first group (handle)
- group2 - second group (handle)

Output Parameter:
. newgroup - intersection group (handle)

Notes:
The output group contains those processes that are in both 'group1' and
'group2'.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Group_intersection(
    _In_ MPI_Group group1,
    _In_ MPI_Group group2,
    _Out_ MPI_Group* newgroup
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_intersection(group1, group2);

    int size1, size2, i, k, g1_idx, g2_idx, l1_pid, l2_pid, nnew;

    MPID_Group *group_ptr1;
    int mpi_errno = MpiaGroupValidateHandle( group1, &group_ptr1 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Group *group_ptr2;
    mpi_errno = MpiaGroupValidateHandle( group2, &group_ptr2 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newgroup == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newgroup" );
        goto fn_fail;
    }

    /* Return a group consisting of the members of group1 that are
       in group2 */
    size1 = group_ptr1->size;
    size2 = group_ptr2->size;
    /* Insure that the lpid lists are setup */
    MPIR_Group_setup_lpid_pairs( group_ptr1, group_ptr2 );

    for (i=0; i<size1; i++)
    {
        group_ptr1->lrank_to_lpid[i].flag = 0;
    }
    g1_idx = group_ptr1->idx_of_first_lpid;
    g2_idx = group_ptr2->idx_of_first_lpid;

    nnew = 0;
    while (g1_idx >= 0 && g2_idx >= 0)
    {
        l1_pid = group_ptr1->lrank_to_lpid[g1_idx].lpid;
        l2_pid = group_ptr2->lrank_to_lpid[g2_idx].lpid;
        if (l1_pid < l2_pid)
        {
            g1_idx = group_ptr1->lrank_to_lpid[g1_idx].next_lpid;
        }
        else if (l1_pid > l2_pid)
        {
            g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
        }
        else
        {
            /* Equal */
            group_ptr1->lrank_to_lpid[g1_idx].flag = 1;
            g1_idx = group_ptr1->lrank_to_lpid[g1_idx].next_lpid;
            g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
            nnew ++;
        }
    }
    /* Create the group.  Handle the trivial case first */
    if (nnew == 0)
    {
        *newgroup = MPI_GROUP_EMPTY;
        goto fn_exit;
    }

    MPID_Group *new_group_ptr;
    mpi_errno = MPIR_Group_create( nnew, &new_group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    new_group_ptr->rank = MPI_UNDEFINED;
    k = 0;
    for (i=0; i<size1; i++)
    {
        if (group_ptr1->lrank_to_lpid[i].flag)
        {
            new_group_ptr->lrank_to_lpid[k].lrank = k;
            new_group_ptr->lrank_to_lpid[k].lpid =
                group_ptr1->lrank_to_lpid[i].lpid;
            if (i == group_ptr1->rank)
            {
                new_group_ptr->rank = k;
            }
            k++;
        }
    }

    *newgroup = new_group_ptr->handle;

  fn_exit:
    TraceLeave_MPI_Group_intersection(*newgroup);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_intersection %G %G %p",
            group1,
            group2,
            newgroup
            )
        );
    TraceError(MPI_Group_intersection, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Group_range_excl - Produces a group by excluding ranges of processes from
       an existing group

Input Parameters:
+ group - group (handle)
. n - number of elements in array 'ranks' (integer)
- ranges - a one-dimensional array of integer triplets of the
form (first rank, last rank, stride), indicating the ranks in
'group'  of processes to be excluded from the output group 'newgroup' .

Output Parameter:
. newgroup - new group derived from above, preserving the
order in 'group'  (handle)

Note:
The MPI standard requires that each of the ranks to be excluded must be
a valid rank in the group and all elements must be distinct or the
function is erroneous.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_EXHAUSTED
.N MPI_ERR_RANK
.N MPI_ERR_ARG

.seealso: MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Group_range_excl(
    _In_ MPI_Group group,
    _In_range_(>=, 0) int n,
    _In_reads_opt_(n) int ranges[][3],
    _Out_ MPI_Group* newgroup
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_range_excl(group, n, TraceArrayLength(n), sizeof(ranges[0]), (void*)ranges);

    int size, i, j, k, nnew, first, last, stride;

    MPID_Group *group_ptr;
    int mpi_errno = MpiaGroupValidateHandle( group, &group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( n < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "n", n );
        goto fn_fail;
    }

    if( ranges == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "ranges" );
        goto fn_fail;
    }

    if( newgroup == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newgroup" );
        goto fn_fail;
    }

    mpi_errno = MPIR_Group_check_valid_ranges( group_ptr, ranges, n );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Compute size, assuming that included ranks are valid (and distinct) */
    size = group_ptr->size;
    nnew = 0;
    for (i=0; i<n; i++)
    {
        first = ranges[i][0]; last = ranges[i][1]; stride = ranges[i][2];
        /* works for stride of either sign.  Error checking above
           has already guaranteed stride != 0 */
        nnew += 1 + (last - first) / stride;
    }
    nnew = size - nnew;

    if (nnew == 0)
    {
        *newgroup = MPI_GROUP_EMPTY;
        goto fn_exit;
    }

    /* Allocate a new group and lrank_to_lpid array */
    MPID_Group *new_group_ptr;
    mpi_errno = MPIR_Group_create( nnew, &new_group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    new_group_ptr->rank = MPI_UNDEFINED;

    /* Group members are taken in rank order from the original group,
       with the specified members removed. Use the flag array for that
       purpose.  If this was a critical routine, we could use the
       flag values set in the error checking part, if the error checking
       was enabled *and* we are not MPI_THREAD_MULTIPLE, but since this
       is a low-usage routine, we haven't taken that optimization.  */

    /* First, mark the members to exclude */
    for (i=0; i<size; i++)
    {
        group_ptr->lrank_to_lpid[i].flag = 0;
    }

    for (i=0; i<n; i++)
    {
        first = ranges[i][0]; last = ranges[i][1]; stride = ranges[i][2];
        if (stride > 0)
        {
            for (j=first; j<=last; j += stride)
            {
                group_ptr->lrank_to_lpid[j].flag = 1;
            }
        }
        else
        {
            for (j=first; j>=last; j += stride)
            {
                group_ptr->lrank_to_lpid[j].flag = 1;
            }
        }
    }
    /* Now, run through the group and pick up the members that were
       not excluded */
    k = 0;
    for (i=0; i<size; i++)
    {
        if (!group_ptr->lrank_to_lpid[i].flag)
        {
            new_group_ptr->lrank_to_lpid[k].lrank = k;
            new_group_ptr->lrank_to_lpid[k].lpid =
                group_ptr->lrank_to_lpid[i].lpid;
            if (group_ptr->rank == i)
            {
                new_group_ptr->rank = k;
            }
            k++;
        }
    }

    *newgroup = new_group_ptr->handle;

  fn_exit:
    TraceLeave_MPI_Group_range_excl(*newgroup);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_range_excl %G %d %p %p",
            group,
            n,
            ranges,
            newgroup
            )
        );
    TraceError(MPI_Group_range_excl, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Group_range_incl - Creates a new group from ranges of ranks in an
        existing group

Input Parameters:
+ group - group (handle)
. n - number of triplets in array  'ranges' (integer)
- ranges - a one-dimensional array of integer triplets, of the
form (first rank, last rank, stride) indicating ranks in
'group'  or processes to be included in 'newgroup'.

Output Parameter:
. newgroup - new group derived from above, in the
order defined by  'ranges' (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_EXHAUSTED
.N MPI_ERR_ARG
.N MPI_ERR_RANK

.seealso: MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Group_range_incl(
    _In_ MPI_Group group,
    _In_range_(>=, 0) int n,
    _In_reads_opt_(n) int ranges[][3],
    _Out_ MPI_Group* newgroup
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_range_incl(group, n, TraceArrayLength(n), sizeof(ranges[0]), (void*)ranges);

    int first, last, stride, nnew, i, j, k;

    MPID_Group *group_ptr;
    int mpi_errno = MpiaGroupValidateHandle( group, &group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( n < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "n", n );
        goto fn_fail;
    }

    if( ranges == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "ranges" );
        goto fn_fail;
    }

    if( newgroup == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newgroup" );
        goto fn_fail;
    }

    mpi_errno = MPIR_Group_check_valid_ranges( group_ptr, ranges, n );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Compute size, assuming that included ranks are valid (and distinct) */
    nnew = 0;
    for (i=0; i<n; i++)
    {
        first = ranges[i][0]; last = ranges[i][1]; stride = ranges[i][2];
        /* works for stride of either sign.  Error checking above
           has already guaranteed stride != 0 */
        nnew += 1 + (last - first) / stride;
    }

    if (nnew == 0)
    {
        *newgroup = MPI_GROUP_EMPTY;
        goto fn_exit;
    }

    /* Allocate a new group and lrank_to_lpid array */
    MPID_Group *new_group_ptr;
    mpi_errno = MPIR_Group_create( nnew, &new_group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    new_group_ptr->rank = MPI_UNDEFINED;

    /* Group members taken in order specified by the range array */
    /* This could be integrated with the error checking, but since this
       is a low-usage routine, we haven't taken that optimization */
    k = 0;
    for (i=0; i<n; i++)
    {
        first = ranges[i][0]; last = ranges[i][1]; stride = ranges[i][2];
        if (stride > 0)
        {
            for (j=first; j<=last; j += stride)
            {
                new_group_ptr->lrank_to_lpid[k].lrank = k;
                new_group_ptr->lrank_to_lpid[k].lpid =
                    group_ptr->lrank_to_lpid[j].lpid;
                if (j == group_ptr->rank)
                    new_group_ptr->rank = k;
                k++;
            }
        }
        else
        {
            for (j=first; j>=last; j += stride)
            {
                new_group_ptr->lrank_to_lpid[k].lrank = k;
                new_group_ptr->lrank_to_lpid[k].lpid =
                    group_ptr->lrank_to_lpid[j].lpid;
                if (j == group_ptr->rank)
                    new_group_ptr->rank = k;
                k++;
            }
        }
    }
    *newgroup = new_group_ptr->handle;

  fn_exit:
    TraceLeave_MPI_Group_range_incl(*newgroup);
  fn_exit1:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_range_incl %G %d %p %p",
            group,
            n,
            ranges,
            newgroup
            )
        );
    TraceError(MPI_Group_range_incl, mpi_errno);
    goto fn_exit1;
}


/*@

MPI_Group_rank - Returns the rank of this process in the given group

Input Parameters:
. group - group (handle)

Output Parameter:
. rank - rank of the calling process in group, or 'MPI_UNDEFINED'  if the
process is not a member (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Group_rank(
    _In_ MPI_Group group,
    _Out_ _Deref_out_range_(>=, MPI_UNDEFINED) int* rank
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_rank(group);

    const MPID_Group *group_ptr;
    int mpi_errno = MpiaGroupValidateHandle( group, const_cast<MPID_Group**>(&group_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( rank == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "rank" );
        goto fn_fail;
    }

    *rank = group_ptr->rank;

    TraceLeave_MPI_Group_rank(*rank);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_rank %G %p",
            group,
            rank
            )
        );
    TraceError(MPI_Group_rank, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Group_size - Returns the size of a group

Input Parameters:
+ group - group (handle)
Output Parameter:
- size - number of processes in the group (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Group_size(
    _In_ MPI_Group group,
    _Out_ _Deref_out_range_(>, 0) int* size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_size(group);

    const MPID_Group *group_ptr;
    int mpi_errno = MpiaGroupValidateHandle( group, const_cast<MPID_Group**>(&group_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "size" );
        goto fn_fail;
    }

    *size = group_ptr->size;

    TraceLeave_MPI_Group_size(*size);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_size %G %p",
            group,
            size
            )
        );
    TraceError(MPI_Group_size, mpi_errno);
    goto fn_exit;
}


/*@
 MPI_Group_translate_ranks - Translates the ranks of processes in one group to
                             those in another group

  Input Parameters:
+ group1 - group1 (handle)
. n - number of ranks in  'ranks1' and 'ranks2'  arrays (integer)
. ranks1 - array of zero or more valid ranks in 'group1'
- group2 - group2 (handle)

  Output Parameter:
. ranks2 - array of corresponding ranks in group2,  'MPI_UNDEFINED'  when no
  correspondence exists.

  As a special case (see the MPI-2 errata), if the input rank is
  'MPI_PROC_NULL', 'MPI_PROC_NULL' is given as the output rank.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
@*/
EXTERN_C
MPI_METHOD
MPI_Group_translate_ranks(
    _In_ MPI_Group group1,
    _In_ int n,
    _In_reads_opt_(n) const int ranks1[],
    _In_ MPI_Group group2,
    _Out_writes_opt_(n) int ranks2[]
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_translate_ranks(group1, n, TraceArrayLength(n), ranks1, group2);

    int i, g2_idx, l1_pid, l2_pid;

    const MPID_Group *group_ptr1;
    int mpi_errno = MpiaGroupValidateHandle( group1, const_cast<MPID_Group**>(&group_ptr1) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Group *group_ptr2;
    mpi_errno = MpiaGroupValidateHandle( group2, &group_ptr2 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( n < 0 )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**argneg %s %d", "n", n );
        goto fn_fail;
    }

    if( n == 0 )
    {
        return MPI_SUCCESS;
    }

    if( ranks1 == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "ranks1" );
        goto fn_fail;
    }

    if( ranks2 == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "ranks2" );
        goto fn_fail;
    }

    /* Check that the rank entries are valid */
    int size1 = group_ptr1->size;
    for (i=0; i< n ; i++)
    {
        if ( (ranks1[i] < 0 && ranks1[i] != MPI_PROC_NULL) ||
            ranks1[i] >= size1)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_RANK, "**rank %d %d",  ranks1[i], size1 );
            goto fn_fail;
        }
    }

    /* Initialize the output ranks */
    for (i=0; i<n; i++)
    {
        ranks2[i] = MPI_UNDEFINED;
    }

    /* We may want to optimize for the special case of group2 is
       a dup of MPI_COMM_WORLD, or more generally, has rank == lpid
       for everything within the size of group2.  NOT DONE YET */
    g2_idx = group_ptr2->idx_of_first_lpid;
    if (g2_idx < 0)
    {
        MPIR_Group_setup_lpid_list( group_ptr2 );
        g2_idx = group_ptr2->idx_of_first_lpid;
    }
    if (g2_idx >= 0)
    {
        /* g2_idx can be < 0 if the g2 group is empty */
        l2_pid = group_ptr2->lrank_to_lpid[g2_idx].lpid;
        for (i=0; i<n; i++)
        {
            if (ranks1[i] == MPI_PROC_NULL)
            {
                ranks2[i] = MPI_PROC_NULL;
                continue;
            }
            l1_pid = group_ptr1->lrank_to_lpid[ranks1[i]].lpid;
            /* Search for this l1_pid in group2.  Use the following
               optimization: start from the last position in the lpid list
               if possible.  A more sophisticated version could use a
               tree based or even hashed search to speed the translation. */
            if (l1_pid < l2_pid || g2_idx < 0)
            {
                /* Start over from the beginning */
                g2_idx = group_ptr2->idx_of_first_lpid;
                l2_pid = group_ptr2->lrank_to_lpid[g2_idx].lpid;
            }
            while (g2_idx >= 0 && l1_pid > l2_pid)
            {
                g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
                if (g2_idx >= 0)
                {
                    l2_pid = group_ptr2->lrank_to_lpid[g2_idx].lpid;
                }
                else
                {
                    l2_pid = -1;
                }
            }
            if (l1_pid == l2_pid)
            {
                ranks2[i] = group_ptr2->lrank_to_lpid[g2_idx].lrank;
            }
        }
    }

    TraceLeave_MPI_Group_translate_ranks(n, TraceArrayLength(n), ranks2);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_translate_ranks %G %d %p %G %p",
            group1,
            n,
            ranks1,
            group2,
            ranks2
            )
        );
    TraceError(MPI_Group_translate_ranks, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Group_union - Produces a group by combining two groups

Input Parameters:
+ group1 - first group (handle)
- group2 - second group (handle)

Output Parameter:
. newgroup - union group (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_GROUP
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Group_union(
    _In_ MPI_Group group1,
    _In_ MPI_Group group2,
    _Out_ MPI_Group* newgroup
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Group_union(group1, group2);

    int g1_idx, g2_idx, nnew, i, k, size1, size2, mylpid;

    MPID_Group *group_ptr1;
    int mpi_errno = MpiaGroupValidateHandle( group1, &group_ptr1 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Group *group_ptr2;
    mpi_errno = MpiaGroupValidateHandle( group2, &group_ptr2 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newgroup == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newgroup" );
        goto fn_fail;
    }

    /* Determine the size of the new group.  The new group consists of all
       members of group1 plus the members of group2 that are not in group1.
    */
    g1_idx = group_ptr1->idx_of_first_lpid;
    g2_idx = group_ptr2->idx_of_first_lpid;

    /* If the lpid list hasn't been created, do it now */
    if (g1_idx < 0)
    {
        MPIR_Group_setup_lpid_list( group_ptr1 );
        g1_idx = group_ptr1->idx_of_first_lpid;
    }
    if (g2_idx < 0)
    {
        MPIR_Group_setup_lpid_list( group_ptr2 );
        g2_idx = group_ptr2->idx_of_first_lpid;
    }
    nnew = group_ptr1->size;

    /* Clear the flag bits on the second group.  The flag is set if
       a member of the second group belongs to the union */
    size2 = group_ptr2->size;
    for (i=0; i<size2; i++)
    {
        group_ptr2->lrank_to_lpid[i].flag = 0;
    }
    /* Loop through the lists that are ordered by lpid (local process
       id) to detect which processes in group 2 are not in group 1
    */
    while (g1_idx >= 0 && g2_idx >= 0)
    {
        int l1_pid, l2_pid;
        l1_pid = group_ptr1->lrank_to_lpid[g1_idx].lpid;
        l2_pid = group_ptr2->lrank_to_lpid[g2_idx].lpid;
        if (l1_pid > l2_pid)
        {
            nnew++;
            group_ptr2->lrank_to_lpid[g2_idx].flag = 1;
            g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
        }
        else if (l1_pid == l2_pid)
        {
            g1_idx = group_ptr1->lrank_to_lpid[g1_idx].next_lpid;
            g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
        }
        else
        {
            /* l1 < l2 */
            g1_idx = group_ptr1->lrank_to_lpid[g1_idx].next_lpid;
        }
    }
    /* If we hit the end of group1, add the remaining members of group 2 */
    while (g2_idx >= 0)
    {
        nnew++;
        group_ptr2->lrank_to_lpid[g2_idx].flag = 1;
        g2_idx = group_ptr2->lrank_to_lpid[g2_idx].next_lpid;
    }

    /* Allocate a new group and lrank_to_lpid array */
    MPID_Group *new_group_ptr;
    mpi_errno = MPIR_Group_create( nnew, &new_group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }
    /* If this process is in group1, then we can set the rank now.
       If we are not in this group, this assignment will set the
       current rank to MPI_UNDEFINED */
    new_group_ptr->rank = group_ptr1->rank;

    /* Add group1 */
    size1 = group_ptr1->size;
    for (i=0; i<size1; i++)
    {
        new_group_ptr->lrank_to_lpid[i].lrank = i;
        new_group_ptr->lrank_to_lpid[i].lpid  =
            group_ptr1->lrank_to_lpid[i].lpid;
    }

    /* Add members of group2 that are not in group 1 */

    if (group_ptr1->rank == MPI_UNDEFINED &&
        group_ptr2->rank >= 0)
    {
        mylpid = group_ptr2->lrank_to_lpid[group_ptr2->rank].lpid;
    }
    else
    {
        mylpid = -2;
    }
    k = size1;
    for (i=0; i<size2; i++)
    {
        if (group_ptr2->lrank_to_lpid[i].flag)
        {
            new_group_ptr->lrank_to_lpid[k].lrank = k;
            new_group_ptr->lrank_to_lpid[k].lpid =
                group_ptr2->lrank_to_lpid[i].lpid;
            if (new_group_ptr->rank == MPI_UNDEFINED &&
                group_ptr2->lrank_to_lpid[i].lpid == mylpid)
                new_group_ptr->rank = k;
            k++;
        }
    }
    *newgroup = new_group_ptr->handle;

    TraceLeave_MPI_Group_union(*newgroup);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_group_union %G %G %p",
            group1,
            group2,
            newgroup
            )
        );
    TraceError(MPI_Group_union, mpi_errno);
    goto fn_exit;
}
