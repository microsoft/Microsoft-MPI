// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"


/*@

MPI_Comm_compare - Compares two communicators

Input Parameters:
+ comm1 - comm1 (handle)
- comm2 - comm2 (handle)

Output Parameter:
. result - integer which is 'MPI_IDENT' if the contexts and groups are the
same, 'MPI_CONGRUENT' if different contexts but identical groups, 'MPI_SIMILAR'
if different contexts but similar groups, and 'MPI_UNEQUAL' otherwise

Using 'MPI_COMM_NULL' with 'MPI_Comm_compare':

It is an error to use 'MPI_COMM_NULL' as one of the arguments to
'MPI_Comm_compare'.  The relevant sections of the MPI standard are

$(2.4.1 Opaque Objects)
A null handle argument is an erroneous 'IN' argument in MPI calls, unless an
exception is explicitly stated in the text that defines the function.

$(5.4.1. Communicator Accessors)
where there is no text in 'MPI_COMM_COMPARE' allowing a null handle.

.N ThreadSafe
(To perform the communicator comparisions, this routine may need to
allocate some memory.  Memory allocation is not interrupt-safe, and hence
this routine is only thread-safe.)

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_compare(
    _In_ MPI_Comm comm1,
    _In_ MPI_Comm comm2,
    _Out_ int* result
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_compare(comm1, comm2);

    MPID_Comm *comm_ptr1;
    MPID_Comm *comm_ptr2;
    int mpi_errno = MpiaCommValidateHandle( comm1, &comm_ptr1 );
    if( mpi_errno != MPI_SUCCESS )
    {
        comm_ptr2 = nullptr;
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateHandle( comm2, &comm_ptr2 );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( result == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "result" );
        goto fn_fail;
    }

    if ((comm_ptr1->comm_kind == MPID_INTERCOMM) ^ (comm_ptr2->comm_kind == MPID_INTERCOMM))
    {
        *result = MPI_UNEQUAL;
    }
    else if (comm1 == comm2)
    {
        *result = MPI_IDENT;
    }
    else if (comm_ptr1->comm_kind != MPID_INTERCOMM)
    {
        MPI_Group group1, group2;

        NMPI_Comm_group( comm1, &group1 );
        NMPI_Comm_group( comm2, &group2 );
        NMPI_Group_compare( group1, group2, result );
        /* If the groups are the same but the contexts are different, then
           the communicators are congruent */
        if (*result == MPI_IDENT)
            *result = MPI_CONGRUENT;
        NMPI_Group_free( &group1 );
        NMPI_Group_free( &group2 );
    }
    else
    {
        /* INTER_COMM */
        int       lresult, rresult;
        MPI_Group group1, group2;
        MPI_Group rgroup1, rgroup2;

        /* Get the groups and see what their relationship is */
        NMPI_Comm_group (comm1, &group1);
        NMPI_Comm_group (comm2, &group2);
        NMPI_Group_compare ( group1, group2, &lresult );

        NMPI_Comm_remote_group (comm1, &rgroup1);
        NMPI_Comm_remote_group (comm2, &rgroup2);
        NMPI_Group_compare ( rgroup1, rgroup2, &rresult );

        /* Choose the result that is "least" strong. This works
           due to the ordering of result types in mpi.h */
        (*result) = (rresult > lresult) ? rresult : lresult;

        /* They can't be identical since they're not the same
           handle, they are congruent instead */
        if ((*result) == MPI_IDENT)
          (*result) = MPI_CONGRUENT;

        /* Free the groups */
        NMPI_Group_free (&group1);
        NMPI_Group_free (&group2);
        NMPI_Group_free (&rgroup1);
        NMPI_Group_free (&rgroup2);
    }

    TraceLeave_MPI_Comm_compare(*result);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    /* Use whichever communicator is non-null if possible */
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr1 ? comm_ptr1 : comm_ptr2,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_compare %C %C %p",
            comm1,
            comm2,
            result
            )
        );
    TraceError(MPI_Comm_compare, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_create - Creates a new communicator

Input Parameters:
+ comm - communicator (handle)
- group - group, which is a subset of the group of 'comm'  (handle)

Output Parameter:
. comm_out - new communicator (handle)

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_GROUP

.seealso: MPI_Comm_free
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_create(
    _In_ MPI_Comm comm,
    _In_ MPI_Group group,
    _Out_ MPI_Comm* newcomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_create(comm, group);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Group *group_ptr;
    mpi_errno = MpiaGroupValidateHandle( group, &group_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Comm* newcomm_ptr;
    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MPIR_Comm_create_intra(comm_ptr, group_ptr, nullptr, nullptr, &newcomm_ptr);
    }
    else
    {
        mpi_errno = MPIR_Comm_create_inter(comm_ptr, group_ptr, &newcomm_ptr);
    }
    ON_ERROR_FAIL(mpi_errno);

    if( newcomm_ptr == nullptr )
    {
        *newcomm = MPI_COMM_NULL;
    }
    else
    {
        *newcomm = newcomm_ptr->handle;
    }

    TraceLeave_MPI_Comm_create(*newcomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_create %C %G %p",
            comm,
            group,
            newcomm
            )
        );
    TraceError(MPI_Comm_create, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_dup - Duplicates an existing communicator with all its cached
               information

Input Parameter:
. comm - Communicator to be duplicated (handle)

Output Parameter:
. newcomm - A new communicator over the same group as 'comm' but with a new
  context. See notes.  (handle)

Notes:
  This routine is used to create a new communicator that has a new
  communication context but contains the same group of processes as
  the input communicator.  Since all MPI communication is performed
  within a communicator (specifies as the group of processes `plus`
  the context), this routine provides an effective way to create a
  private communicator for use by a software module or library.  In
  particular, no library routine should use 'MPI_COMM_WORLD' as the
  communicator; instead, a duplicate of a user-specified communicator
  should always be used.  For more information, see Using MPI, 2nd
  edition.

  Because this routine essentially produces a copy of a communicator,
  it also copies any attributes that have been defined on the input
  communicator, using the attribute copy function specified by the
  'copy_function' argument to 'MPI_Keyval_create'.  This is
  particularly useful for (a) attributes that describe some property
  of the group associated with the communicator, such as its
  interconnection topology and (b) communicators that are given back
  to the user; the attibutes in this case can track subsequent
  'MPI_Comm_dup' operations on this communicator.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM

.seealso: MPI_Comm_free, MPI_Keyval_create, MPI_Attr_put, MPI_Attr_delete,
 MPI_Comm_create_keyval, MPI_Comm_set_attr, MPI_Comm_delete_attr
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_dup(
    _In_ MPI_Comm comm,
    _Out_ MPI_Comm* newcomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_dup(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newcomm" );
        goto fn_fail;
    }

    /* Copy attributes, executing the attribute copy functions */
    MPID_Attribute *new_attributes = nullptr;
    mpi_errno = MPIR_Attr_dup_list(
        comm_ptr->handle,
        comm_ptr->attributes,
        &new_attributes
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        /* Note: The error code returned here should reflect the error code
           determined by the user routine called during the
           attribute duplication step.  Adding additional text to the
           message associated with the code is allowable; changing the
           code is not */
        *newcomm = MPI_COMM_NULL;
        goto fn_fail;
    }

    /* Generate a new context value and a new communicator structure */
    /* We must use the local size, because this is compared to the
       rank of the process in the communicator.  For intercomms,
       this must be the local size */
    int size;
    if( comm_ptr->comm_kind == MPID_INTERCOMM )
    {
        size = comm_ptr->inter.local_size;
    }
    else
    {
        size = comm_ptr->remote_size;
    }
    MPID_Comm *newcomm_ptr;
    __analysis_assert( comm_ptr->rank < size );
    mpi_errno = MPIR_Comm_copy( comm_ptr, size, &newcomm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (newcomm_ptr != nullptr)
    {
        newcomm_ptr->attributes = new_attributes;
        *newcomm = newcomm_ptr->handle;
        TraceLeave_MPI_Comm_dup(*newcomm);
    }
    else
    {
        newcomm = nullptr;
        TraceLeave_MPI_Comm_dup(0);
    }

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_dup %C %p",
            comm,
            newcomm
            )
        );
    TraceError(MPI_Comm_dup, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Comm_free - Marks the communicator object for deallocation

Input Parameter:
. comm - Communicator to be destroyed (handle)

Notes:
This routine `frees` a communicator.  Because the communicator may still
be in use by other MPI routines, the actual communicator storage will not
be freed until all references to this communicator are removed.  For most
users, the effect of this routine is the same as if it was in fact freed
at this time of this call.

Null Handles:
The MPI 1.1 specification, in the section on opaque objects, explicitly
disallows freeing a null communicator.  The text from the standard is:
.vb
 A null handle argument is an erroneous IN argument in MPI calls, unless an
 exception is explicitly stated in the text that defines the function. Such
 exception is allowed for handles to request objects in Wait and Test calls
 (sections Communication Completion and Multiple Completions ). Otherwise, a
 null handle can only be passed to a function that allocates a new object and
 returns a reference to it in the handle.
.ve

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_free(
    _Inout_ MPI_Comm* comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_free(*comm);

    MPID_Comm *comm_ptr;

    int mpi_errno;
    if( comm == nullptr )
    {
        comm_ptr = nullptr;
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm" );
        goto fn_fail;
    }

    mpi_errno = MpiaCommValidateHandle( *comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* Cannot free the predefined communicators */
    if (HANDLE_GET_TYPE(*comm) == HANDLE_TYPE_BUILTIN)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM, "**commperm %s",  comm_ptr->name );
        goto fn_fail;
    }

    if (comm_ptr->attributes != NULL)
    {
        mpi_errno = MPIR_Attr_delete_list( comm_ptr->handle, &comm_ptr->attributes );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    mpi_errno = MPIR_Comm_release(comm_ptr, 0);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *comm = MPI_COMM_NULL;

    TraceLeave_MPI_Comm_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_free %p",
            comm
            )
        );
    TraceError(MPI_Comm_free, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Comm_get_name - Return the print name from the communicator

  Input Parameter:
. comm - Communicator to get name of (handle)

  Output Parameters:
+ comm_name - On output, contains the name of the communicator.  It must
  be an array of size at least 'MPI_MAX_OBJECT_NAME'.
- resultlen - Number of characters in name

 Notes:

.N COMMNULL

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_get_name(
    _In_ MPI_Comm comm,
    _Out_writes_z_(MPI_MAX_OBJECT_NAME) char* comm_name,
    _Out_ int* resultlen
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_get_name(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( comm_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm_name" );
        goto fn_fail;
    }
    if( resultlen == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "resultlen" );
        goto fn_fail;
    }

    /* The user must allocate a large enough section of memory */
    MPIU_Strncpy( comm_name, comm_ptr->name, MPI_MAX_OBJECT_NAME );

    //
    // comm_name must be less than MPI_MAX_OBJECT_NAME characters which is
    // currently defined to be 128.
    //
    C_ASSERT(MPI_MAX_OBJECT_NAME <= INT_MAX);
    *resultlen = static_cast<int>( MPIU_Strlen( comm_name, MPI_MAX_OBJECT_NAME ) );

    TraceLeave_MPI_Comm_get_name(*resultlen, comm_name);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_get_name %C %p %p",
            comm,
            comm_name,
            resultlen
            )
        );
    TraceError(MPI_Comm_get_name, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_get_parent - Return the parent communicator for this process

   Output Parameter:
. parent - the parent communicator (handle)

   Notes:

 If a process was started with 'MPI_Comm_spawn' or 'MPI_Comm_spawn_multiple',
 'MPI_Comm_get_parent' returns the parent intercommunicator of the current
  process. This parent intercommunicator is created implicitly inside of
 'MPI_Init' and is the same intercommunicator returned by 'MPI_Comm_spawn'
  in the parents.

  If the process was not spawned, 'MPI_Comm_get_parent' returns
  'MPI_COMM_NULL'.

  After the parent communicator is freed or disconnected, 'MPI_Comm_get_parent'
  returns 'MPI_COMM_NULL'.

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_get_parent(
    _Out_ MPI_Comm* parent
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_get_parent();

    int mpi_errno = MPI_SUCCESS;

    if( parent == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "parent" );
        goto fn_fail;
    }

    /* Note that MPIU_DBG_OpenFile also uses this code (so as to avoid
       calling an MPI routine while logging it */
    *parent = (Mpi.CommParent == nullptr) ? MPI_COMM_NULL :
               (Mpi.CommParent)->handle;

    TraceLeave_MPI_Comm_get_parent(*parent);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_get_parent %p",
            parent
            )
        );
    TraceError(MPI_Comm_get_parent, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_group - Accesses the group associated with given communicator

Input Parameter:
. comm - Communicator (handle)

Output Parameter:
. group - Group in communicator (handle)

Notes:
.N COMMNULL

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_group(
    _In_ MPI_Comm comm,
    _Out_ MPI_Group* group
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_group(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( group == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "group" );
        goto fn_fail;
    }

    MPID_Group *group_ptr;

    /* Create a group if necessary and populate it with the
       local process ids */
    if( comm_ptr->comm_kind == MPID_INTERCOMM )
    {
        MPIU_Assert( comm_ptr->inter.local_comm != nullptr );
        MPIU_Assert( comm_ptr->inter.local_comm->rank == comm_ptr->rank );

        mpi_errno = MPIR_Comm_group( comm_ptr->inter.local_comm, &group_ptr );
    }
    else
    {
        mpi_errno = MPIR_Comm_group( comm_ptr, &group_ptr );
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *group = group_ptr->handle;
    MPIR_Group_add_ref( group_ptr );

    TraceLeave_MPI_Comm_group(*group);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_group %C %p",
            comm,
            group
            )
        );
    TraceError(MPI_Comm_group, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_rank - Determines the rank of the calling process in the communicator

Input Argument:
. comm - communicator (handle)

Output Argument:
. rank - rank of the calling process in the group of 'comm'  (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_rank(
    _In_ MPI_Comm comm,
    _Out_ _Deref_out_range_(>=, 0)  int* rank
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_rank(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( rank == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "rank" );
        goto fn_fail;
    }

    *rank = comm_ptr->rank;

    TraceLeave_MPI_Comm_rank(*rank);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_rank %C %p",
            comm,
            rank)
        );
    TraceError(MPI_Comm_rank, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_remote_group - Accesses the remote group associated with
                        the given inter-communicator

Input Parameter:
. comm - Communicator (must be an intercommunicator) (handle)

Output Parameter:
. group - remote group of communicator (handle)

Notes:
The user is responsible for freeing the group when it is no longer needed.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM

.seealso MPI_Group_free
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_remote_group(
    _In_ MPI_Comm comm,
    _Out_ MPI_Group* group
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_remote_group(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( group == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "group" );
        goto fn_fail;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM,  "**commnotinter" );
        goto fn_fail;
    }

    int i, lpid, n;
    MPID_Group *group_ptr;

    /* Create a group and populate it with the local process ids */
    if (!comm_ptr->group)
    {
        n = comm_ptr->remote_size;
        mpi_errno = MPIR_Group_create( n, &group_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        for (i=0; i<n; i++)
        {
            group_ptr->lrank_to_lpid[i].lrank = i;
            lpid = MPID_VCR_Get_lpid( comm_ptr->vcr[i] );
            group_ptr->lrank_to_lpid[i].lpid  = lpid;
        }
        group_ptr->size          = n;
        group_ptr->rank          = MPI_UNDEFINED;
        group_ptr->idx_of_first_lpid = -1;
        comm_ptr->group   = group_ptr;
    }
    *group = comm_ptr->group->handle;
    MPIR_Group_add_ref( comm_ptr->group );

    TraceLeave_MPI_Comm_remote_group(*group);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_remote_group %C %p",
            comm,
            group
            )
        );
    TraceError(MPI_Comm_remote_group, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_remote_size - Determines the size of the remote group
                       associated with an inter-communictor

Input Parameter:
. comm - communicator (handle)

Output Parameter:
. size - number of processes in the remote group of 'comm'  (integer)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_remote_size(
    _In_ MPI_Comm comm,
    _Out_ int* size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_remote_size(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "size" );
        goto fn_fail;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM,  "**commnotinter" );
        goto fn_fail;
    }

    *size = comm_ptr->remote_size;

    TraceLeave_MPI_Comm_remote_size(*size);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_remote_size %C %p",
            comm,
            size
            )
        );
    TraceError(MPI_Comm_remote_size, mpi_errno);
    goto fn_exit;
}


/*@
   MPI_Comm_set_name - Sets the print name for a communicator

   Input Parameters:
+  MPI_Comm comm - communicator to name (handle)
-  char *comm_name - Name for communicator

.N ThreadSafeNoUpdate

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_set_name(
    _In_ MPI_Comm comm,
    _In_z_ const char* comm_name
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_set_name(comm, comm_name);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( comm_name == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "comm_name" );
        goto fn_fail;
    }

    if( SetName<MPID_Comm>( comm_ptr, comm_name ) == false )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    TraceLeave_MPI_Comm_set_name();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_set_name %C %s",
            comm,
            comm_name
            )
        );
    TraceError(MPI_Comm_set_name, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_size - Determines the size of the group associated with a communicator

Input Parameter:
. comm - communicator (handle)

Output Parameter:
. size - number of processes in the group of 'comm'  (integer)

Notes:

.N NULL

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_size(
    _In_ MPI_Comm comm,
    _Out_ _Deref_out_range_(>, 0) int* size
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_size(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( size == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "size" );
        goto fn_fail;
    }

    if( comm_ptr->comm_kind == MPID_INTERCOMM )
    {
        *size = comm_ptr->inter.local_size;
    }
    else
    {
        *size = comm_ptr->remote_size;
    }

    TraceLeave_MPI_Comm_size(*size);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_size %C %p",
            comm,
            size
            )
        );
    TraceError(MPI_Comm_size, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_split - Creates new communicators based on colors and keys

Input Parameters:
+ comm - communicator (handle)
. color - control of subset assignment (nonnegative integer).  Processes
  with the same color are in the same new communicator
- key - control of rank assignment (integer)

Output Parameter:
. newcomm - new communicator (handle)

Notes:
  The 'color' must be non-negative or 'MPI_UNDEFINED'.

.N ThreadSafe

.N Fortran

Algorithm:
.vb
  1. Use MPI_Allgather to get the color and key from each process
  2. Count the number of processes with the same color; create a
     communicator with that many processes.  If this process has
     'MPI_UNDEFINED' as the color, create a process with a single member.
  3. Use key to order the ranks
  4. Set the VCRs using the ordered key values
.ve

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Comm_free
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_split(
    _In_ MPI_Comm comm,
    _In_ int color,
    _In_ int key,
    _Out_ MPI_Comm* newcomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_split(comm, color, key);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newcomm" );
        goto fn_fail;
    }

    MPID_Comm *newcomm_ptr;
    if( comm_ptr->comm_kind == MPID_INTERCOMM )
    {
        mpi_errno = MPIR_Comm_split_inter( comm_ptr, color, key, &newcomm_ptr );
    }
    else
    {
        mpi_errno = MPIR_Comm_split_intra( comm_ptr, color, key, &newcomm_ptr );
    }
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm_ptr == NULL )
    {
        *newcomm = MPI_COMM_NULL;
    }
    else
    {
        *newcomm = newcomm_ptr->handle;
    }

    TraceLeave_MPI_Comm_split(*newcomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_split %C %d %d %p",
            comm,
            color,
            key,
            newcomm
            )
        );
    TraceError(MPI_Comm_split, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_split_type - Creates new communicators based on split type

Input Parameters:
+ comm - communicator (handle)
. split_type - Processes with the same type are in the same new communicator (integer)
- key - control of rank assignment (integer)
+ info - additional info to assist in creating communicators (handle)

Ouput Parameter


control of subset assignment (nonnegative integer).  Processes
  with the same color are in the same new communicator
- key - control of rank assignment (integer)

Output Parameter:
. newcomm - new communicator (handle)

Notes:
  This is a collective call. All processes must provide the same split_type.
  Split_type can be MPI_UNDEFINED, in which case newcomm returns MPI_COMM_NULL.
  Supported split types: MPI_COMM_TYPE_SHARED
@*/
MPI_METHOD
MPI_Comm_split_type(
    _In_ MPI_Comm comm,
    _In_ int split_type,
    _In_ int key,
    _In_ MPI_Info info,
    _Out_ MPI_Comm *newcomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_split_type(comm, split_type, key, info);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "newcomm" );
        goto fn_fail;
    }

    if(split_type != MPI_COMM_TYPE_SHARED && split_type != MPI_UNDEFINED)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG, "**comm_split_type %d", split_type);
        goto fn_fail;
    }

    if(comm_ptr->comm_kind == MPID_INTERCOMM)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM, "**commnotintra");
        goto fn_fail;
    }

    if(comm_ptr->comm_kind == MPID_INTRACOMM_FLAT)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM, "**nosplittype %d", comm_ptr->comm_kind);
        goto fn_fail;
    }

    //
    // Use the node id of rank as color for creating a new communicator that uses shared memory
    //
    int color;
    if( split_type == MPI_COMM_TYPE_SHARED )
    {
        color = comm_ptr->vcr[comm_ptr->rank]->node_id;
    }
    else
    {
        color = MPI_UNDEFINED;
    }

    MPID_Comm *newcomm_ptr;
    mpi_errno = MPIR_Comm_split_intra( comm_ptr, color, key, &newcomm_ptr );

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( newcomm_ptr == nullptr )
    {
        *newcomm = MPI_COMM_NULL;
    }
    else
    {
        *newcomm = newcomm_ptr->handle;
    }

    TraceLeave_MPI_Comm_split_type(*newcomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_split_type %C %d %d %I %p",
            comm,
            split_type,
            key,
            info,
            newcomm
            )
        );
    TraceError(MPI_Comm_split_type, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Comm_test_inter - Tests to see if a comm is an inter-communicator

Input Parameter:
. comm - communicator to test (handle)

Output Parameter:
. flag - true if this is an inter-communicator(logical)

.N SignalSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_ARG
@*/
EXTERN_C
MPI_METHOD
MPI_Comm_test_inter(
    _In_ MPI_Comm comm,
    _Out_ _Deref_out_range_(0, 1) int* flag
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Comm_test_inter(comm);

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( flag == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "flag" );
        goto fn_fail;
    }

    *flag = (comm_ptr->comm_kind == MPID_INTERCOMM);

    TraceLeave_MPI_Comm_test_inter(*flag);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_comm_test_inter %C %p",
            comm,
            flag
            )
        );
    TraceError(MPI_Comm_test_inter, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Intercomm_create - Creates an intercommuncator from two intracommunicators

Input Parameters:
+ local_comm - Local (intra)communicator
. local_leader - Rank in local_comm of leader (often 0)
. peer_comm - Communicator used to communicate between a
              designated process in the other communicator.
              Significant only at the process in 'local_comm' with
              rank 'local_leader'.
. remote_leader - Rank in peer_comm of remote leader (often 0)
- tag - Message tag to use in constructing intercommunicator; if multiple
  'MPI_Intercomm_creates' are being made, they should use different tags (more
  precisely, ensure that the local and remote leaders are using different
  tags for each 'MPI_intercomm_create').

Output Parameter:
. comm_out - Created intercommunicator

Notes:
   'peer_comm' is significant only for the process designated the
   'local_leader' in the 'local_comm'.

  The MPI 1.1 Standard contains two mutually exclusive comments on the
  input intracommunicators.  One says that their repective groups must be
  disjoint; the other that the leaders can be the same process.  After
  some discussion by the MPI Forum, it has been decided that the groups must
  be disjoint.  Note that the `reason` given for this in the standard is
  `not` the reason for this choice; rather, the `other` operations on
  intercommunicators (like 'MPI_Intercomm_merge') do not make sense if the
  groups are not disjoint.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TAG
.N MPI_ERR_EXHAUSTED
.N MPI_ERR_RANK

.seealso: MPI_Intercomm_merge, MPI_Comm_free, MPI_Comm_remote_group,
          MPI_Comm_remote_size

@*/
EXTERN_C
MPI_METHOD
MPI_Intercomm_create(
    _In_ MPI_Comm local_comm,
    _In_range_(>=, 0) int local_leader,
    _In_ MPI_Comm peer_comm,
    _In_range_(>=, 0) int remote_leader,
    _In_range_(>=, 0) int tag,
    _Out_ MPI_Comm* newintercomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Intercomm_create(local_comm, local_leader, peer_comm, remote_leader, tag);

    MPID_Comm *peer_comm_ptr;
    MPID_Comm *newcomm_ptr;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm( local_comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( (local_leader < 0 ||
        local_leader >= comm_ptr->remote_size) )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_RANK, "**ranklocal %d %d", local_leader, comm_ptr->remote_size );
        goto fn_fail;
    }

    /*
     * Error checking for this routine requires care.  Because this
     * routine is collective over two different sets of processes,
     * it is relatively easy for the user to try to create an
     * intercommunicator from two overlapping groups of processes.
     * This is made more likely by inconsistencies in the MPI-1
     * specification (clarified in MPI-2) that seemed to allow
     * the groups to overlap.  Because of that, we first check that the
     * groups are in fact disjoint before performing any collective
     * operations.
     */

    /*printf( "comm_ptr->rank = %d, local_leader = %d\n", comm_ptr->rank,
      local_leader ); fflush(stdout);*/
    if (comm_ptr->rank == local_leader)
    {
        mpi_errno = MpiaCommValidateHandle( peer_comm, &peer_comm_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
        /* Note: In MPI 1.0, peer_comm was restricted to
            intracommunicators.  In 1.1, it may be any communicator */

        /* In checking the rank of the remote leader,
            allow the peer_comm to be in intercommunicator
            by checking against the remote size */
        int remote_size = peer_comm_ptr->remote_size;

        if( (remote_leader < 0 ||
            remote_leader >= remote_size) )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_RANK,
                "**rankremote %d %d",
                remote_leader,
                remote_size
                );
            goto fn_fail;
        }

        //
        // Verify that local_leader and remote_leader are not the same
        //
        if (peer_comm_ptr->comm_kind != MPID_INTERCOMM &&
            comm_ptr->rank == local_leader &&
            peer_comm_ptr->rank == remote_leader)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_RANK,"**ranksdistinct");
            goto fn_fail;
        }

        mpi_errno = MPIR_Intercomm_create_root( comm_ptr,
                                                peer_comm_ptr,
                                                remote_leader,
                                                tag,
                                                &newcomm_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }
    else
    {
        mpi_errno = MPIR_Intercomm_create_non_root( comm_ptr,
                                                    local_leader,
                                                    &newcomm_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }
    }

    *newintercomm = newcomm_ptr->handle;

    TraceLeave_MPI_Intercomm_create(*newintercomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_intercomm_create %C %d %C %d %d %p",
            local_comm,
            local_leader,
            peer_comm,
            remote_leader,
            tag,
            newintercomm
            )
        );
    TraceError(MPI_Intercomm_create, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Intercomm_merge - Creates an intracommuncator from an intercommunicator

Input Parameters:
+ comm - Intercommunicator (handle)
- high - Used to order the groups within comm (logical)
  when creating the new communicator.  This is a boolean value; the group
  that sets high true has its processes ordered `after` the group that sets
  this value to false.  If all processes in the intercommunicator provide
  the same value, the choice of which group is ordered first is arbitrary.

Output Parameter:
. comm_out - Created intracommunicator (handle)

Notes:
 While all processes may provide the same value for the 'high' parameter,
 this requires the MPI implementation to determine which group of
 processes should be ranked first.

.N ThreadSafe

.N Fortran

Algorithm:
.Es
.i Allocate contexts
.i Local and remote group leaders swap high values
.i Determine the high value.
.i Merge the two groups and make the intra-communicator
.Ee

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_EXHAUSTED

.seealso: MPI_Intercomm_create, MPI_Comm_free
@*/
EXTERN_C
MPI_METHOD
MPI_Intercomm_merge(
    _In_ MPI_Comm intercomm,
    _In_ int high,
    _Out_ MPI_Comm* newintracomm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Intercomm_merge(intercomm, high);

    MPID_Comm *newcomm_ptr;
    int  local_high, remote_high, i, j, new_size;
    MPI_CONTEXT_ID new_context_id;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( intercomm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COMM, "**commnotinter" );
        goto fn_fail;
    }

    int acthigh;
    /* Check for consistent valus of high in each local group.
        The Intel test suite checks for this; it is also an easy
        error to make */
    acthigh = high ? 1 : 0;   /* Clamp high into 1 or 0 */

    mpi_errno = NMPI_Allreduce( MPI_IN_PLACE, &acthigh, 1, MPI_INT,
                        MPI_SUM, comm_ptr->inter.local_comm->handle );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    /* acthigh must either == 0 or the size of the local comm */
    if (acthigh != 0 && acthigh != comm_ptr->inter.local_size)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_ARG,  "**notsame %s %s", "high",  "MPI_Intercomm_merge" );
        goto fn_fail;
    }

    /* Find the "high" value of the other group of processes.  This
       will be used to determine which group is ordered first in
       the generated communicator.  high is logical */
    local_high = high;
    if (comm_ptr->rank == 0)
    {
        /* This routine allows use to use the collective communication
           context rather than the point-to-point context. */
        mpi_errno = MPIC_Sendrecv( &local_high, 1, g_hBuiltinTypes.MPI_Int, 0,
                                   MPIR_TAG_INTERCOMM_MERGE,
                                   &remote_high, 1, g_hBuiltinTypes.MPI_Int, 0,
                                   MPIR_TAG_INTERCOMM_MERGE,
                                   comm_ptr, MPI_STATUS_IGNORE );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        /* If local_high and remote_high are the same, then order is arbitrary.
           we use the gpids of the rank 0 member of the local and remote
           groups to choose an order in this case. */
        if (local_high == remote_high)
        {
            struct gpid_t
            {
                GUID gid;
                int  pg_rank;
            };

            gpid_t ingpid = {MPIDI_Process.my_pg->id,
                             MPIDI_Process.my_pg_rank};
            gpid_t outgpid;

            mpi_errno = MPIC_Sendrecv( &ingpid, sizeof(gpid_t), g_hBuiltinTypes.MPI_Byte, 0,
                                       MPIR_TAG_INTERCOMM_MERGE,
                                       &outgpid, sizeof(gpid_t), g_hBuiltinTypes.MPI_Byte, 0,
                                       MPIR_TAG_INTERCOMM_MERGE,
                                       comm_ptr, MPI_STATUS_IGNORE );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( ingpid.pg_rank < outgpid.pg_rank )
            {
                local_high = 0;
            }
            else if ( ingpid.pg_rank > outgpid.pg_rank )
            {
                local_high = 1;
            }
            else
            {
                RPC_STATUS status;
                int compareResult = UuidCompare( &ingpid.gid, &outgpid.gid, &status );
                if( FAILED( status ) )
                {
                    mpi_errno = MPIU_ERR_CREATE( MPI_ERR_INTERN,
                                                 "**intern %s",
                                                 "Failed to compare group ids");
                    goto fn_fail;
                }
                if( compareResult == -1 )
                {
                    local_high = 0;
                }
                else
                {
                    /* Note that the gpids cannot be the same because we are
                       starting from a valid intercomm */
                    MPIU_Assert( compareResult != 0 );
                    local_high = 1;
                }
            }
        }
    }

    /*
       All processes in the local group now need to get the
       value of local_high, which may have changed if both groups
       of processes had the same value for high
    */
    mpi_errno = NMPI_Bcast( &local_high, 1, MPI_INT, 0,
                            comm_ptr->inter.local_comm->handle );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // HA is disabled for merged communicators.
    //
    mpi_errno = MPIR_Comm_create( &newcomm_ptr, MPID_INTRACOMM_FLAT );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    new_size = comm_ptr->inter.local_size + comm_ptr->remote_size;

    if (local_high)
    {
        MPIR_Create_temp_contextid(
            &newcomm_ptr->context_id,
            comm_ptr->recvcontext_id
            );
    }
    else
    {
        MPIR_Create_temp_contextid(
            &newcomm_ptr->context_id,
            comm_ptr->context_id
            );
    }
    newcomm_ptr->recvcontext_id = newcomm_ptr->context_id;
    newcomm_ptr->remote_size     = new_size;
    newcomm_ptr->rank           = -1;

    /* Now we know which group comes first.  Build the new vcr
       from the existing vcrs */
    MPID_VCRT* vcrt = MPID_VCRT_Create( new_size);
    if( vcrt == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    newcomm_ptr->vcr = MPID_VCRT_Get_ptr( vcrt );
    if (local_high)
    {
        /* remote group first */
        j = 0;
        for (i=0; i<comm_ptr->remote_size; i++)
        {
            newcomm_ptr->vcr[j++] = MPID_VCR_Dup( comm_ptr->vcr[i] );
        }
        for (i=0; i<comm_ptr->inter.local_size; i++)
        {
            if (i == comm_ptr->rank) newcomm_ptr->rank = j;
            newcomm_ptr->vcr[j++] = MPID_VCR_Dup( comm_ptr->inter.local_comm->vcr[i] );
        }
    }
    else
    {
        /* local group first */
        j = 0;
        for (i=0; i<comm_ptr->inter.local_size; i++)
        {
            if (i == comm_ptr->rank) newcomm_ptr->rank = j;
            newcomm_ptr->vcr[j++] = MPID_VCR_Dup( comm_ptr->inter.local_comm->vcr[i] );
        }
        for (i=0; i<comm_ptr->remote_size; i++)
        {
            newcomm_ptr->vcr[j++] = MPID_VCR_Dup( comm_ptr->vcr[i] );
        }
    }

    /* We've setup a temporary context id, based on the context id
       used by the intercomm.  This allows us to perform the allreduce
       operations within the context id algorithm, since we already
       have a valid (almost - see comm_create_hook) communicator.
    */
    /* printf( "About to get context id \n" ); fflush( stdout ); */
    /* In the multi-threaded case, MPIR_Get_contextid assumes that the
       calling routine already holds the single criticial section */
    mpi_errno = MPIR_Get_contextid( newcomm_ptr, &new_context_id );
    ON_ERROR_FAIL(mpi_errno);

    newcomm_ptr->context_id     = new_context_id;
    newcomm_ptr->recvcontext_id = new_context_id;

    mpi_errno = MPIR_Comm_commit(newcomm_ptr);
    ON_ERROR_FAIL(mpi_errno);

    *newintracomm = newcomm_ptr->handle;

    TraceLeave_MPI_Intercomm_merge(*newintracomm);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_intercomm_merge %C %d %p",
            intercomm,
            high,
            newintracomm
            )
        );
    TraceError(MPI_Intercomm_merge, mpi_errno);
    goto fn_exit;
}
