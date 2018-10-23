// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "mpi_fortlogical.h"


/*@
  MPI_Op_create - Creates a user-defined combination function handle

  Input Parameters:
+ function - user defined function (function)
- commute -  true if commutative;  false otherwise. (logical)

  Output Parameter:
. op - operation (handle)

  Notes on the user function:
  The calling list for the user function type is
.vb
 typedef void (MPI_User_function) ( void * a,
               void * b, int * len, MPI_Datatype * );
.ve
  where the operation is 'b[i] = a[i] op b[i]', for 'i=0,...,len-1'.  A pointer
  to the datatype given to the MPI collective computation routine (i.e.,
  'MPI_Reduce', 'MPI_Allreduce', 'MPI_Scan', or 'MPI_Reduce_scatter') is also
  passed to the user-specified routine.

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS

.seealso: MPI_Op_free
@*/
EXTERN_C
MPI_METHOD
MPI_Op_create(
    _In_ MPI_User_function* user_fn,
    _In_ int commute,
    _Out_ MPI_Op* op
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Op_create(user_fn, commute);

    MPID_Op *op_ptr;
    int mpi_errno = MPI_SUCCESS;

    if( user_fn == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "user_fn" );
        goto fn_fail;
    }

    if( op == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "op" );
        goto fn_fail;
    }

    op_ptr = OpPool::Alloc();
    if( op_ptr == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    op_ptr->user_function = user_fn;
    op_ptr->proxy = MPIR_Op_c_proxy;
    op_ptr->kind     = commute ? MPID_OP_USER : MPID_OP_USER_NONCOMMUTE;

    *op = op_ptr->handle;

    TraceLeave_MPI_Op_create(*op);

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_op_create %p %d %p",
            user_fn,
            commute,
            op
            )
        );
    TraceError(MPI_Op_create, mpi_errno);
    goto fn_exit;
}


EXTERN_C
void
MPIAPI
MPIR_Op_set_proxy(MPI_Op op, MPID_User_function_proxy* proxy)
{
    MPID_Op* op_ptr = OpPool::Lookup( op );
    if(op_ptr == nullptr)
    {
        return;
    }
    op_ptr->proxy = proxy;
}


/*@
  MPI_Op_free - Frees a user-defined combination function handle

  Input Parameter:
. op - operation (handle)

  Notes:
  'op' is set to 'MPI_OP_NULL' on exit.

.N NULL

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_PERM_OP

.seealso: MPI_Op_create
@*/
EXTERN_C
MPI_METHOD
MPI_Op_free(
    _Inout_ MPI_Op* op
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Op_free(*op);

    int mpi_errno;
    if( op == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "op" );
        goto fn_fail;
    }

    MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidateHandle( *op, &op_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if (op_ptr->kind < MPID_OP_USER_NONCOMMUTE)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OP, "**permop" );
        goto fn_fail;
    }

    op_ptr->Release();
    *op = MPI_OP_NULL;

    TraceLeave_MPI_Op_free();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_op_free %p",
            op
            )
        );
    TraceError(MPI_Op_free, mpi_errno);
    goto fn_exit;
}


/*@
  MPI_Op_commutative - Queries an MPI reduction operation for its commutativity.

Input Parameters:
. op - operation (handle)

Output Parameters:
. commute - Flag is true if 'op' is a commutative operation. (logical)

.N NULL

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG

.seealso: MPI_Op_create
@*/
EXTERN_C
MPI_METHOD
MPI_Op_commutative(
    _In_ MPI_Op op,
    _Out_ int* commute
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();

    MPID_Op* op_ptr;
    int mpi_errno = MpiaOpValidateHandle( op, &op_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( commute == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "commute" );
        goto fn_fail;
    }

    *commute = op_ptr->IsCommutative() ? 1 : 0;

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_op_commutative %O",
            op
            )
        );
    goto fn_exit;
}
