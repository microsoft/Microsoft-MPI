// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"


DWORD
smpd_send_close_command(
    smpd_context_t* pContext,
    INT16 dest
    )
{
    StackGuardPointer<SmpdCmd> pCmd = smpd_create_command(
        SMPD_CLOSE,
        smpd_process.tree_id,
        dest );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create a 'close' command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    StackGuardPointer<SmpdResWrapper> pRes = smpd_create_result_command(
        smpd_process.tree_id,
        nullptr );
    if( pRes == nullptr )
    {
        smpd_err_printf(L"unable to create result for 'close' command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD rc = SmpdPostCommandSync( pContext, pCmd, &(pRes->Res) );
    if( rc != RPC_S_OK )
    {
        smpd_err_printf(L"Failed to post close command error %lu\n", rc);
    }

    return rc;
}


static void
smpd_handle_parent_closed(
    smpd_overlapped_t* /*pov*/
    )
{
    return;
}


static DWORD
smpd_send_closed_command(void)
{
    smpd_dbg_printf(L"sending 'closed' command to parent context\n");
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_CLOSED,
        smpd_process.tree_id,
        smpd_process.parent_id );
    if(pCmd == nullptr)
    {
        smpd_err_printf(L"unable to create the 'closed' command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        &smpd_handle_parent_closed );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for 'closed' command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD rc = smpd_post_command(
        smpd_process.parent_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"failed to post closed command error %u\n", rc);
    }
    return rc;
}


//
// Summary:
//  Handler for the close command from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
void
smpd_handle_close_command(
    _In_ EXOVERLAPPED* pexov
    )
{
    smpd_dbg_printf(L"handling command SMPD_CLOSE from parent\n");
    smpd_overlapped_t* pov      = smpd_ov_from_exov( pexov );
    smpd_free_overlapped( pov );

    smpd_process.closing = TRUE;
    if( smpd_process.left_context != nullptr )
    {
        smpd_dbg_printf(L"sending close command to left child\n");
        DWORD rc = smpd_send_close_command(
            smpd_process.left_context,
            smpd_process.left_child_id );
        if(rc != NOERROR)
        {
            smpd_post_abort_command(
                L"failed to send close command to the left child. error %d.\n", rc);
            return;
        }
    }

    if( smpd_process.right_context != nullptr )
    {
        smpd_dbg_printf(L"sending close command to right child\n");
        DWORD rc = smpd_send_close_command(
            smpd_process.right_context,
            smpd_process.right_child_id );
        if(rc != NOERROR)
        {
            smpd_post_abort_command(
                L"failed to send close command to the right child. error %u.\n", rc);
            return;
        }
    }

    if(smpd_process.left_context || smpd_process.right_context)
    {
        return;
    }

    DWORD rc = smpd_send_closed_command();
    if(rc != NOERROR)
    {
        smpd_err_printf(
            L"unable to send closed command to the parent context with error %u. exiting...\n", rc);
        smpd_signal_exit_progress(rc);
    }
}


DWORD
smpd_handle_closed_command(
    _In_ smpd_overlapped_t* pov
    )
{
    //ASSERT(smpd_process.closing);

    smpd_context_type_t context_type =
        smpd_destination_context_type( pov->pCmd->ClosedCmd.header.src );

    //
    // We complete the async op here because once the tear down starts,
    // the client side (the SMPD manager) might get an error on their
    // RpcAsyncCompleteCall because the teardown might close down
    // the rpc channel. We ignore the error on the RpcAsyncCompleteCall
    // because we're in tearing down mode and there's no recovery if this
    // call fails.
    //
    OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
    reinterpret_cast<SmpdResHdr*>(pov->pRes)->err = NOERROR;
    RpcAsyncCompleteCall( pov->pAsync, nullptr );
    OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
        "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");

    smpd_context_t** ppChildContext;
    if( context_type == SMPD_CONTEXT_LEFT_CHILD )
    {
        smpd_dbg_printf(L"closed command received from left child.\n");
        ppChildContext = &smpd_process.left_context;
    }
    else if( context_type == SMPD_CONTEXT_RIGHT_CHILD )
    {
        smpd_dbg_printf(L"closed command received from right child.\n");
        ppChildContext = &smpd_process.right_context;
    }
    else
    {
        smpd_err_printf(L"closed command received from unknown context.\n");
        return NOERROR;
    }

    ASSERT( *ppChildContext != nullptr );

    RPC_STATUS status = RPC_S_OK;
    RpcTryExcept
    {
        RpcCliDeleteSmpdMgrContext(
            &(*ppChildContext)->hServerContext
            );
    }
    RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
    {
        status = RpcExceptionCode();
    }
    RpcEndExcept;

    if( status != NOERROR )
    {
        //
        // Child likely exited the RPC server before the null context
        // and return error is marshalled back to us. This should not
        // affect the tree tear down.
        //
        // This error is unexpected but occasionally the child
        // terminates the RPC server faster than usual and the RPC
        // Server has not finished marshalling the null context and
        // the return status. It is safe to ignore the error here
        // because both sides are exiting very soon.
        //
        smpd_dbg_printf( L"closed context with error %ld.\n", status );
    }

    smpd_free_context( *ppChildContext );
    *ppChildContext = nullptr;

    if( smpd_process.left_context == nullptr &&
        smpd_process.right_context == nullptr )
    {
        if( smpd_process.parent_context == nullptr )
        {
            smpd_signal_exit_progress( NOERROR );
            return NOERROR;
        }

        DWORD rc = smpd_send_closed_command();
        if(rc != NOERROR)
        {
            smpd_err_printf(
                L"unable to send closed command to the parent context. exiting...\n");
            smpd_signal_exit_progress(rc);
            return rc;
        }

        // return NOERROR;

        // smpd_signal_exit_progress(MPI_SUCCESS);
    }
    return NOERROR;
}


DWORD
smpd_handle_ping_command(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdPingCmd* pPingCmd = &pov->pCmd->PingCmd;
    if( pPingCmd->header.src == SMPD_IS_ROOT )
    {
        //
        // Response with a pong;
        //
        if( smpd_process.parent_context == nullptr )
        {
            return NOERROR;
        }

        SmpdCmd* pCmd = smpd_create_command(
            SMPD_PING,
            SMPD_IS_TOP,
            SMPD_IS_ROOT );
        if( pCmd == nullptr )
        {
            return NOERROR;
        }

        SmpdResWrapper* pRes = smpd_create_result_command(
            smpd_process.tree_id,
            nullptr );
        if( pRes == nullptr )
        {
            delete pCmd;
            return NOERROR;
        }

        DWORD rc = smpd_post_command(
            smpd_process.parent_context,
            pCmd,
            &pRes->Res,
            nullptr );
        if( rc != RPC_S_OK )
        {
            delete pRes;
            delete pCmd;
            smpd_dbg_printf(L"failed to post ping command error %u\n", rc );
        }
    }

    return NOERROR;
}
