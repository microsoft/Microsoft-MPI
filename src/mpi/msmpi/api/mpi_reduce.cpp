// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


/*@

MPI_Reduce - Reduces values on all processes to a single value

Input Parameters:
+ sendbuf - address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - data type of elements of send buffer (handle)
. op - reduce operation (handle)
. root - rank of root process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice,
 significant only at 'root')

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_BUFFER_ALIAS

@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Reduce(
    _In_range_(!=, recvbuf) _In_opt_ const void* sendbuf,
    _When_(root != MPI_PROC_NULL, _Out_opt_) void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Reduce(comm,sendbuf,recvbuf,datatype,count,op,root);

    TypeHandle hType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        //
        // We check the sendbuf, despite it potentially being MPI_IN_PLACE.  This is because
        // the recvbuf could be NULL on all non-root processes where only the send buffer
        // parameters are significant.
        //
        mpi_errno = MpiaDatatypeValidate( sendbuf, count, datatype, "datatype", &hType );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if (comm_ptr->rank == root)
        {
            if( recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidateBuffer( hType, recvbuf, count );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( count > 0 && sendbuf == recvbuf )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else
        {
            if( count > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }
        }

        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Ireduce_intra(
                sendbuf,
                recvbuf,
                count,
                hType,
                const_cast<MPID_Op*>(op_ptr),
                root,
                const_cast<MPID_Comm*>(comm_ptr),
                &creq_ptr
                );
            if (mpi_errno == MPI_SUCCESS)
            {
                mpi_errno = MPIR_Wait(creq_ptr);
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Reduce_intra(
                sendbuf,
                recvbuf,
                count,
                hType,
                op_ptr,
                root,
                comm_ptr
                );
        }
    }
    else
    {
        if (root == MPI_ROOT)
        {
            mpi_errno = MpiaDatatypeValidate( recvbuf, count, datatype, "datatype", &hType );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else if (root != MPI_PROC_NULL)
        {
            mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( count > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidate( sendbuf, count, datatype, "datatype", &hType );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            goto fn_done;
        }

        if (Mpi.ForceAsyncWorkflow)
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Ireduce_inter(
                sendbuf,
                recvbuf,
                count,
                hType,
                const_cast<MPID_Op*>(op_ptr),
                root,
                const_cast<MPID_Comm*>(comm_ptr),
                &creq_ptr
                );
            if (mpi_errno == MPI_SUCCESS)
            {
                mpi_errno = MPIR_Wait(creq_ptr);
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Reduce_inter(
                sendbuf,
                recvbuf,
                count,
                hType,
                op_ptr,
                root,
                comm_ptr
                );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

fn_done:
    TraceLeave_MPI_Reduce();

fn_exit:
    MpiaExit();
    return mpi_errno;

fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_reduce %p %p %d %D %O %d %C",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            root,
            comm
            )
        );
    TraceError(MPI_Reduce, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Reduce_local - Applies a reduction operator to local arguments.

Input Parameters:
+ inbuf - address of the input buffer (choice)
. count - number of elements in each buffer (integer)
. datatype - data type of elements in the buffers (handle)
- op - reduction operation (handle)

Output Parameter:
. inoutbuf - address of input-output buffer (choice)

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_BUFFER_ALIAS
@*/

EXTERN_C
_Pre_satisfies_(inbuf != MPI_IN_PLACE)
_Pre_satisfies_(inoutbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Reduce_local(
    _In_opt_ _In_range_(!=, inoutbuf) const void *inbuf,
    _Inout_opt_ void *inoutbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op
    )
{
    //TODO: Need tracing!

    MpiaIsInitializedOrExit();

    TypeHandle hType;

    int mpi_errno = MPI_SUCCESS;
    if( count == 0 )
    {
        goto fn_exit;
    }

    const MPID_Op* op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( inbuf == MPI_IN_PLACE )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
        goto fn_fail;
    }

    if( inoutbuf == MPI_IN_PLACE )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate( inbuf, count, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( inbuf == inoutbuf )
    {
        mpi_errno = MPIU_ERR_CREATE(
            MPI_ERR_BUFFER,
            "**bufalias %s %s",
            "inbuf",
            "inoutbuf"
            );
        goto fn_fail;
    }

    /* actually perform the reduction */
    MPID_Uop_call(op_ptr, inbuf, inoutbuf, &count, &datatype);

  fn_exit:
    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    mpi_errno = MPIR_Err_return_comm(
        NULL,
        FCNAME,
        MPIR_Err_create_code(
            mpi_errno,
            MPIR_ERR_RECOVERABLE,
            MPI_ERR_OTHER,
            "**mpi_reduce_local %p %p %d %D %O",
            inbuf,
            inoutbuf,
            count,
            datatype,
            op
            )
        );
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}


/*@

MPI_Reduce_scatter_block - Combines values and scatters the results

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. recvcount - integer specifying the
number of elements per block distributed to each process.
Array must be identical on all calling processes.
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_OP
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Reduce_scatter_block(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Reduce_scatter_block(
        comm,
        sendbuf,
        recvbuf,
        datatype,
        recvcount,
        op
        );

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle(comm, &comm_ptr);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (recvcount < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", recvcount);
        goto fn_fail;
    }

    //
    // We check the sendbuf, despite it potentially being MPI_IN_PLACE.  This is because
    // the recvbuf could be NULL on processes whose recvcount entry in recvcounts is zero.
    //
    mpi_errno = MpiaDatatypeValidate(sendbuf, recvcount, datatype, "datatype", &hType);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate(op, datatype, false, const_cast<MPID_Op**>(&op_ptr));
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (recvcount > 0)
    {
        if (recvbuf == MPI_IN_PLACE)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**recvbuf_inplace");
            goto fn_fail;
        }

        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MpiaDatatypeValidateBuffer(hType, recvbuf, recvcount);
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }

            if (sendbuf == recvbuf)
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else if (comm_ptr->comm_kind == MPID_INTERCOMM)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**sendbuf_inplace");
            goto fn_fail;
        }
    }

    if( Mpi.ForceAsyncWorkflow )
    {
        MPID_Request* creq_ptr;
        mpi_errno = MPIR_Ireduce_scatter_block(
            sendbuf,
            recvbuf,
            recvcount,
            hType,
            op_ptr,
            comm_ptr,
            &creq_ptr );
        if( mpi_errno == MPI_SUCCESS )
        {
            mpi_errno = MPIR_Wait( creq_ptr );
            creq_ptr->Release();
        }
    }
    else if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        /* intracommunicator */
        mpi_errno = MPIR_Reduce_scatter_block_intra(
            sendbuf,
            recvbuf,
            recvcount,
            hType,
            op_ptr,
            comm_ptr
            );
    }
    else
    {
        /* intercommunicator */
        mpi_errno = MPIR_Reduce_scatter_block_inter(
            sendbuf,
            recvbuf,
            recvcount,
            hType,
            op_ptr,
            comm_ptr
            );
    }

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Reduce_scatter_block();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_reduce_scatter_block %p %p %d %D %O %C",
            sendbuf,
            recvbuf,
            recvcount,
            datatype,
            op,
            comm
            )
        );
    TraceError(MPI_Reduce_scatter_block, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Ireduce_scatter_block - Combines values and scatters the results
in a nonblocking way

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. recvcount - integer specifying the
number of elements per block distributed to each process.
Array must be identical on all calling processes.
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)
. request - communication handle (handle)
.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_OP
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Ireduce_scatter_block(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int recvcount,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ireduce_scatter_block(
        comm,
        sendbuf,
        recvbuf,
        datatype,
        recvcount,
        op
        );

    TypeHandle hType;

    MPID_Comm* pComm;
    int mpi_errno = MpiaCommValidateHandle(comm, &pComm);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    if (recvcount < 0)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", recvcount);
        goto fn_fail;
    }

    //
    // We check the sendbuf, despite it potentially being MPI_IN_PLACE.  This is because
    // the recvbuf could be NULL on processes whose recvcount entry in recvcounts is zero.
    //
    mpi_errno = MpiaDatatypeValidate(sendbuf, recvcount, datatype, "datatype", &hType);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    const MPID_Op* pOp;
    mpi_errno = MpiaOpValidate(op, datatype, false, const_cast<MPID_Op**>(&pOp));
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if (recvcount > 0)
    {
        if (recvbuf == MPI_IN_PLACE)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**recvbuf_inplace");
            goto fn_fail;
        }

        if (sendbuf != MPI_IN_PLACE)
        {
            mpi_errno = MpiaDatatypeValidateBuffer(hType, recvbuf, recvcount);
            if (mpi_errno != MPI_SUCCESS)
            {
                goto fn_fail;
            }

            if (sendbuf == recvbuf)
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else if (pComm->comm_kind == MPID_INTERCOMM)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_BUFFER, "**sendbuf_inplace");
            goto fn_fail;
        }
    }

    MPID_Request* pReq;
    mpi_errno = MPIR_Ireduce_scatter_block(
            sendbuf,
            recvbuf,
            recvcount,
            hType,
            pOp,
            pComm,
            &pReq );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user
    //
    MPIU_Assert(pReq != nullptr);
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ireduce_scatter_block( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ireduce_scatter_block %p %p %d %D %O %C %p",
            sendbuf,
            recvbuf,
            recvcount,
            datatype,
            op,
            comm,
            request
            )
        );
    TraceError(MPI_Ireduce_scatter_block, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Reduce_scatter - Combines values and scatters the results

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. recvcounts - integer array specifying the
number of elements in result distributed to each process.
Array must be identical on all calling processes.
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_OP
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Reduce_scatter(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Reduce_scatter(
        comm,
        sendbuf,
        recvbuf,
        datatype,
        TraceArrayLength(1),
        recvcounts,
        op
        );

    TypeHandle hType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    int size;
    if( comm_ptr->comm_kind == MPID_INTERCOMM )
    {
        size = comm_ptr->inter.local_size;
    }
    else
    {
        size = comm_ptr->remote_size;
    }

    int sendcount_sentinel = 0;
    for( int i = 0; i < size; i++ )
    {
        if( recvcounts[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
            goto fn_fail;
        }
        //
        // Technically, we should check the sum of all receive counts,
        // but that could overflow an int...  The check only cares about zero
        // vs. non-zero at this point, so we simply bitwise-OR the counts.
        //
        sendcount_sentinel |= recvcounts[i];
    }

    //
    // We check the sendbuf, despite it potentially being MPI_IN_PLACE.  This is because
    // the recvbuf could be NULL on processes whose recvcount entry in recvcounts is zero.
    //
    mpi_errno = MpiaDatatypeValidate( sendbuf, sendcount_sentinel, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( recvcounts[comm_ptr->rank] > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MpiaDatatypeValidateBuffer( hType, recvbuf, recvcounts[comm_ptr->rank] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendbuf == recvbuf )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else if( comm_ptr->comm_kind == MPID_INTERCOMM )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        /* intracommunicator */
        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Ireduce_scatter_intra(
                sendbuf,
                recvbuf,
                recvcounts,
                hType,
                const_cast<MPID_Op*>(op_ptr),
                const_cast<MPID_Comm*>( comm_ptr ),
                &creq_ptr
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                mpi_errno = MPIR_Wait( creq_ptr );
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Reduce_scatter_intra(
                sendbuf,
                recvbuf,
                recvcounts,
                hType,
                op_ptr,
                comm_ptr
                );
        }
    }
    else
    {
        /* intercommunicator */
        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Ireduce_scatter_inter(
                sendbuf,
                recvbuf,
                recvcounts,
                hType,
                const_cast<MPID_Op*>(op_ptr),
                const_cast<MPID_Comm*>( comm_ptr ),
                &creq_ptr
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                mpi_errno = MPIR_Wait( creq_ptr );
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Reduce_scatter_inter(
                sendbuf,
                recvbuf,
                recvcounts,
                hType,
                op_ptr,
                comm_ptr
                );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Reduce_scatter();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_reduce_scatter %p %p %p %D %O %C",
            sendbuf,
            recvbuf,
            recvcounts,
            datatype,
            op,
            comm
            )
        );
    TraceError(MPI_Reduce_scatter, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Ireduce_scatter - Combines values and scatters the results
    in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. recvcounts - integer array specifying the
number of elements in result distributed to each process.
Array must be identical on all calling processes.
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_OP
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Ireduce_scatter(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_ const int recvcounts[],
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ireduce_scatter(
        comm,
        sendbuf,
        recvbuf,
        datatype,
        TraceArrayLength( 1 ),
        recvcounts,
        op
        );

    MPID_Comm *pComm;
    int mpi_errno = MpiaCommValidateHandle( comm, &pComm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    int size;
    if( pComm->comm_kind == MPID_INTERCOMM )
    {
        size = pComm->inter.local_size;
    }
    else
    {
        size = pComm->remote_size;
    }

    int sendcount_sentinel = 0;
    for( int i = size - 1; i >= 0; i-- )
    {
        if( recvcounts[i] < 0 )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_COUNT, "**countneg %d", recvcounts[i] );
            goto fn_fail;
        }
        //
        // Technically, we should check the sum of all receive counts,
        // but that could overflow an int...  The check only cares about zero
        // vs. non-zero at this point, so we simply bitwise-OR the counts.
        //
        sendcount_sentinel |= recvcounts[i];
    }

    //
    // We check the sendbuf, despite it potentially being MPI_IN_PLACE.  This is because
    // the recvbuf could be NULL on processes whose recvcount entry in recvcounts is zero.
    //
    TypeHandle hType;
    mpi_errno = MpiaDatatypeValidate( sendbuf, sendcount_sentinel, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, &op_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( recvcounts[pComm->rank] > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MpiaDatatypeValidateBuffer( hType, recvbuf, recvcounts[pComm->rank] );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendbuf == recvbuf )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else if( pComm->comm_kind == MPID_INTERCOMM )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }
    }

    MPID_Request* pReq;

    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        mpi_errno = MPIR_Ireduce_scatter_intra(
            sendbuf,
            recvbuf,
            recvcounts,
            hType,
            op_ptr,
            pComm,
            &pReq
            );
    }
    else
    {
        mpi_errno = MPIR_Ireduce_scatter_inter(
            sendbuf,
            recvbuf,
            recvcounts,
            hType,
            op_ptr,
            pComm,
            &pReq
            );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user.
    //
    MPIU_Assert( pReq != nullptr );
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ireduce_scatter( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ireduce_scatter %p %p %p %D %O %C %p",
            sendbuf,
            recvbuf,
            recvcounts,
            datatype,
            op,
            comm,
            request
            )
        );
    TraceError(MPI_Ireduce_scatter, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Allreduce - Combines values from all processes and distributes the result
                back to all processes

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - data type of elements of send buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_ERR_BUFFER
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_OP
.N MPI_ERR_COMM
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Allreduce(
    _In_range_(!=, recvbuf) _In_opt_ const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Allreduce(comm,sendbuf, recvbuf, datatype, count, op);

    TypeHandle hType;

    const MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, const_cast<MPID_Comm**>(&comm_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate( recvbuf, count, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MpiaDatatypeValidateBuffer( hType, sendbuf, count );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendbuf == recvbuf )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else if( comm_ptr->comm_kind == MPID_INTERCOMM )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }
    }

    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Iallreduce_intra(
                sendbuf,
                recvbuf,
                count,
                hType,
                const_cast<MPID_Op*>( op_ptr ),
                const_cast<MPID_Comm*>( comm_ptr ),
                &creq_ptr
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                mpi_errno = MPIR_Wait( creq_ptr );
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Allreduce_intra(
                reinterpret_cast<const BYTE*>(sendbuf),
                reinterpret_cast<BYTE*>(recvbuf),
                count,
                hType,
                op_ptr,
                comm_ptr
                );
        }
    }
    else
    {
        if( Mpi.ForceAsyncWorkflow )
        {
            MPID_Request* creq_ptr;

            mpi_errno = MPIR_Iallreduce_inter(
                sendbuf,
                recvbuf,
                count,
                hType,
                const_cast<MPID_Op*>( op_ptr ),
                const_cast<MPID_Comm*>( comm_ptr ),
                &creq_ptr
                );
            if( mpi_errno == MPI_SUCCESS )
            {
                mpi_errno = MPIR_Wait( creq_ptr );
                creq_ptr->Release();
            }
        }
        else
        {
            mpi_errno = MPIR_Allreduce_inter(
                reinterpret_cast<const BYTE*>(sendbuf),
                reinterpret_cast<BYTE*>(recvbuf),
                count,
                hType,
                op_ptr,
                comm_ptr
                );
        }
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    TraceLeave_MPI_Allreduce();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_allreduce %p %p %d %D %O %C",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            comm
            )
        );
    TraceError(MPI_Allreduce, mpi_errno);
    goto fn_exit;
}


/*@
MPI_Iallreduce - Combines values from all processes and distributes the result
                 back to all processes in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - data type of elements of send buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_ERR_ARG
.N MPI_ERR_BUFFER
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_OP
.N MPI_ERR_COMM
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Iallreduce(
    _In_range_(!=, recvbuf) _In_opt_ const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iallreduce(
        comm,
        sendbuf,
        recvbuf,
        datatype,
        count,
        op
        );

    MPID_Comm *pComm;
    int mpi_errno = MpiaCommValidateHandle( comm, &pComm );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    TypeHandle hType;
    mpi_errno = MpiaDatatypeValidate( recvbuf, count, datatype, "datatype", &hType );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, &op_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( sendbuf != MPI_IN_PLACE )
        {
            mpi_errno = MpiaDatatypeValidateBuffer( hType, sendbuf, count );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendbuf == recvbuf )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else if( pComm->comm_kind == MPID_INTERCOMM )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );
            goto fn_fail;
        }
    }

    MPID_Request* pReq;

    if( pComm->comm_kind != MPID_INTERCOMM )
    {
        mpi_errno = MPIR_Iallreduce_intra(
            sendbuf,
            recvbuf,
            count,
            hType,
            op_ptr,
            pComm,
            &pReq
            );
    }
    else
    {
        mpi_errno = MPIR_Iallreduce_inter(
            sendbuf,
            recvbuf,
            count,
            hType,
            op_ptr,
            pComm,
            &pReq
            );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user
    //
    MPIU_Assert( pReq != nullptr );
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Iallreduce( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        pComm,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iallreduce %p %p %d %D %O %C %p",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            comm,
            request
            )
        );
    TraceError(MPI_Iallreduce, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Scan - Computes the scan (partial reductions) of data on a collection of
           processes

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. count - number of elements in input buffer (integer)
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Scan(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Scan(comm,sendbuf,recvbuf,datatype,count,op);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        recvbuf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( sendbuf == recvbuf )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_BUFFER,
                "**bufalias %s %s",
                "sendbuf",
                "recvbuf"
                );
            goto fn_fail;
        }
    }

    MPID_Request *pReq;
    mpi_errno = MPIR_Iscan(
        sendbuf,
        recvbuf,
        count,
        hType,
        op_ptr,
        comm_ptr,
        MPIR_SCAN_TAG,
        &pReq );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPIR_Wait( pReq );
    pReq->Release();

    TraceLeave_MPI_Scan();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_scan %p %p %d %D %O %C",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            comm
            )
        );
    TraceError(MPI_Scan, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Iscan - Computes the scan (partial reductions) of data on a collection of
            processes in a nonblocking way.

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. count - number of elements in input buffer (integer)
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Iscan(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iscan(comm,sendbuf,recvbuf,datatype,count,op);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    mpi_errno = MpiaDatatypeValidate(
        recvbuf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( sendbuf == recvbuf )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_BUFFER,
                "**bufalias %s %s",
                "sendbuf",
                "recvbuf"
                );
            goto fn_fail;
        }
    }

    MPID_Request* pReq;
    mpi_errno = MPIR_Iscan(
        sendbuf,
        recvbuf,
        count,
        hType,
        op_ptr,
        comm_ptr,
        MPIR_GET_ASYNC_TAG( MPIR_SCAN_TAG ),
        &pReq );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user
    //
    MPIU_Assert(pReq != nullptr);
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Iscan( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iscan %p %p %d %D %O %C %p",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            comm,
            request
            )
        );
    TraceError(MPI_Iscan, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Exscan - Computes the exclusive scan (partial reductions) of data on a
           collection of processes

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. count - number of elements in input buffer (integer)
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)

Notes:
  'MPI_Exscan' is like 'MPI_Scan', except that the contribution from the
   calling process is not included in the result at the calling process
   (it is contributed to the subsequent processes, of course).

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Exscan(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Exscan(comm,sendbuf,recvbuf,datatype,count,op);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MpiaDatatypeValidate(
        sendbuf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( comm_ptr->rank != 0 )
        {
            mpi_errno = MpiaDatatypeValidateBuffer( hType, recvbuf, count );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( sendbuf == recvbuf )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
    }

    MPID_Request* pReq;
    mpi_errno = MPIR_Iexscan(
        sendbuf,
        recvbuf,
        count,
        hType,
        op_ptr,
        comm_ptr,
        MPIR_EXSCAN_TAG,
        &pReq );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    mpi_errno = MPIR_Wait( pReq );
    pReq->Release();

    TraceLeave_MPI_Exscan();

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_exscan %p %p %d %D %O %C",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            comm
            )
        );
    TraceError(MPI_Exscan, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Iexscan - Computes the exclusive scan (partial reductions) of data on a
           collection of processes in a nonblocking way

Input Parameters:
+ sendbuf - starting address of send buffer (choice)
. count - number of elements in input buffer (integer)
. datatype - data type of elements of input buffer (handle)
. op - operation (handle)
- comm - communicator (handle)

Output Parameter:
. recvbuf - starting address of receive buffer (choice)
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_BUFFER_ALIAS
@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Iexscan(
    _In_opt_ _In_range_(!=, recvbuf) const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Iexscan(comm,sendbuf,recvbuf,datatype,count,op);

    TypeHandle hType;

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateIntracomm( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    mpi_errno = MpiaDatatypeValidate(
        recvbuf,
        count,
        datatype,
        "datatype",
        &hType
        );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    const MPID_Op *op_ptr;
    mpi_errno = MpiaOpValidate( op, datatype, false, const_cast<MPID_Op**>(&op_ptr) );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( count > 0 )
    {
        if( recvbuf == MPI_IN_PLACE )
        {
            mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
            goto fn_fail;
        }

        if( sendbuf == recvbuf )
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_BUFFER,
                "**bufalias %s %s",
                "sendbuf",
                "recvbuf"
                );
            goto fn_fail;
        }
    }

    MPID_Request* pReq;
    mpi_errno = MPIR_Iexscan(
        sendbuf,
        recvbuf,
        count,
        hType,
        op_ptr,
        comm_ptr,
        MPIR_GET_ASYNC_TAG( MPIR_EXSCAN_TAG ),
        &pReq );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    //
    // Return the handle of the request to the user
    //
    MPIU_Assert(pReq != nullptr);
    *request = pReq->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Iexscan( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        comm_ptr,
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_iexscan %p %p %d %D %O %C %p",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            comm,
            request
            )
        );
    TraceError(MPI_Iexscan, mpi_errno);
    goto fn_exit;
}


/*@

MPI_Ireduce - Reduces values on all processes to a single value
              in a nonblocking way.

Input Parameters:
+ sendbuf - address of send buffer (choice)
. count - number of elements in send buffer (integer)
. datatype - data type of elements of send buffer (handle)
. op - reduce operation (handle)
. root - rank of root process (integer)
- comm - communicator (handle)

Output Parameter:
. recvbuf - address of receive buffer (choice, significant only at 'root')
. request - communication handle (handle)

.N ThreadSafe

.N Fortran

.N collops

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_BUFFER
.N MPI_ERR_BUFFER_ALIAS

@*/
EXTERN_C
_Pre_satisfies_(recvbuf != MPI_IN_PLACE)
MPI_METHOD
MPI_Ireduce(
    _In_range_(!=, recvbuf) _In_opt_ const void* sendbuf,
    _Out_opt_ void* recvbuf,
    _In_range_(>=, 0) int count,
    _In_ MPI_Datatype datatype,
    _In_ MPI_Op op,
    _mpi_coll_rank_(root) int root,
    _In_ MPI_Comm comm,
    _Out_ MPI_Request* request
    )
{
    MpiaIsInitializedOrExit();
    MpiaEnter();
    TraceEnter_MPI_Ireduce( comm, sendbuf, recvbuf, datatype, count, op, root );

    MPID_Comm *comm_ptr;
    int mpi_errno = MpiaCommValidateHandle( comm, &comm_ptr );
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    if( request == nullptr )
    {
        mpi_errno = MPIU_ERR_CREATE( MPI_ERR_ARG, "**nullptr %s", "request" );
        goto fn_fail;
    }
    *request = MPI_REQUEST_NULL;

    TypeHandle hType;
    MPID_Request* pRequest;
    MPID_Op *op_ptr;
    if (comm_ptr->comm_kind != MPID_INTERCOMM)
    {
        mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        //
        // We check the sendbuf, despite it potentially being MPI_IN_PLACE.  This is because
        // the recvbuf could be NULL on all non-root processes where only the send buffer
        // parameters are significant.
        //
        mpi_errno = MpiaDatatypeValidate( sendbuf, count, datatype, "datatype", &hType );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        mpi_errno = MpiaOpValidate( op, datatype, false, &op_ptr );
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_fail;
        }

        if (comm_ptr->rank == root)
        {
            if( recvbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**recvbuf_inplace" );
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidateBuffer( hType, recvbuf, count );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( count > 0 && sendbuf == recvbuf )
            {
                mpi_errno = MPIU_ERR_CREATE(
                    MPI_ERR_BUFFER,
                    "**bufalias %s %s",
                    "inbuf",
                    "inoutbuf"
                    );
                goto fn_fail;
            }
        }
        else
        {
            if( count > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }
        }

        mpi_errno = MPIR_Ireduce_intra(
            sendbuf,
            recvbuf,
            count,
            hType,
            op_ptr,
            root,
            comm_ptr,
            &pRequest
            );
    }
    else
    {
        if (root == MPI_ROOT)
        {
            mpi_errno = MpiaDatatypeValidate( recvbuf, count, datatype, "datatype", &hType );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaOpValidate( op, datatype, false, &op_ptr );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else if (root != MPI_PROC_NULL)
        {
            mpi_errno = MpiaCommValidateRoot( comm_ptr, root );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            if( count > 0 && sendbuf == MPI_IN_PLACE )
            {
                mpi_errno = MPIU_ERR_CREATE( MPI_ERR_BUFFER, "**sendbuf_inplace" );;
                goto fn_fail;
            }

            mpi_errno = MpiaDatatypeValidate( sendbuf, count, datatype, "datatype", &hType );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }

            mpi_errno = MpiaOpValidate( op, datatype, false, &op_ptr );
            if( mpi_errno != MPI_SUCCESS )
            {
                goto fn_fail;
            }
        }
        else
        {
            hType = g_hBuiltinTypes.MPI_Datatype_null;
            op_ptr = nullptr;
        }

        mpi_errno = MPIR_Ireduce_inter(
            sendbuf,
            recvbuf,
            count,
            hType,
            op_ptr,
            root,
            comm_ptr,
            &pRequest
            );
    }

    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail;
    }

    *request = pRequest->handle;
    MPID_Request_track_user_alloc();

    TraceLeave_MPI_Ireduce( *request );

  fn_exit:
    MpiaExit();
    return mpi_errno;

  fn_fail:
    mpi_errno = MPIR_Err_return_comm(
        const_cast<MPID_Comm *>(comm_ptr),
        FCNAME,
        MPIU_ERR_GET(
            mpi_errno,
            "**mpi_ireduce %p %p %d %D %O %d %C %p",
            sendbuf,
            recvbuf,
            count,
            datatype,
            op,
            root,
            comm,
            request
            )
        );
    TraceError(MPI_Ireduce, mpi_errno);
    goto fn_exit;
}
