// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"

//----------------------------------------------------------------------------
//
// Create and initalize commands
//
//----------------------------------------------------------------------------
void
smpd_free_command(
    _In_ _Post_ptr_invalid_ SmpdCmd* pCmd
    )
{
    delete pCmd;
}


//
// Summary:
// Create a PMI command
//
// Parameters:
// cmdType : The type of command as defined in the enum SMPD_CMD_TYPE
// src     : The requestor
// dest    : The destination of this command
//
// Return  : nullptr if there is not enough memory to allocate a command
//           If successful, a command structure is allocated.
//
// Remarks:
//
SmpdCmd*
smpd_create_command(
    _In_ SMPD_CMD_TYPE cmdType,
    _In_ INT16         src,
    _In_ INT16         dest
    )
{
    SmpdCmd* pCmd = new SmpdCmd;
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to allocate memory for a command.\n");
        return nullptr;
    }

    SmpdCmdHdr* pHeader = reinterpret_cast<SmpdCmdHdr*>( pCmd );
    pHeader->cmdType = cmdType;
    pHeader->src = src;
    pHeader->dest = dest;

    return pCmd;

}

//
// Summary:
// Initialize PMI result command
//
// Parameters:
// pRes    : Pointer to the result command to initialize
// dest    : The destination of this command
//
void
smpd_init_result_command(
    _Out_ SmpdRes* pRes,
    _In_  INT16    dest
    )
{
    SmpdResHdr* pHeader = reinterpret_cast<SmpdResHdr*>( pRes );
    pHeader->src = static_cast<INT16>( smpd_process.tree_id );
    pHeader->dest = dest;
    return;
}


//
// Summary:
// Create a PMI result command
//
// Parameters:
// dest    : The destination of this command
// resHandler: The function to invoke when this result command is processed
//
// Return  : nullptr if there is not enough memory to allocate a command
//           If successful, a command structure is allocated.
//
// Remarks:
//
SmpdResWrapper*
smpd_create_result_command(
    _In_     INT16             dest,
    _In_opt_ SmpdResHandler*   resHandler
    )
{
    SmpdResWrapper* pResWrapper = new SmpdResWrapper;
    if( pResWrapper == nullptr )
    {
        smpd_err_printf(L"unable to allocate memory for a result command.\n");
        return nullptr;
    }

    reinterpret_cast<SmpdResHdr*>(&pResWrapper->Res)->err = 0xFFFFFFFF;
    smpd_init_result_command( &pResWrapper->Res, dest );

    pResWrapper->OnResultFn = resHandler;

    return pResWrapper;
}


const wchar_t*
CmdTypeToString(
    _In_ SMPD_CMD_TYPE cmdType
    )
{
    C_ASSERT(SMPD_ABORT == 0);
    static wchar_t* cmdStrArray[SMPD_CMD_MAX] = {
        L"SMPD_ABORT",
        L"SMPD_ABORT_JOB",
        L"SMPD_BARRIER",
        L"SMPD_CLOSE",
        L"SMPD_CLOSED",
        L"SMPD_COLLECT",
        L"SMPD_CONNECT",
        L"SMPD_BCGET",
        L"SMPD_BCPUT",
        L"SMPD_DBGET",
        L"SMPD_DBPUT",
        L"SMPD_DONE",
        L"SMPD_EXIT",
        L"SMPD_FINALIZE",
        L"SMPD_INIT",
        L"SMPD_KILL",
        L"SMPD_LAUNCH",
        L"SMPD_STARTDBS",
        L"SMPD_STDIN",
        L"SMPD_STDIN_CLOSE",
        L"SMPD_STDOUT",
        L"SMPD_STDERR",
        L"SMPD_SUSPEND",
        L"SMPD_PING",
        L"SMPD_ADDDBS",
        L"SMPD_SPAWN"
    };

    if( cmdType >= SMPD_CMD_MAX )
    {
        return L"Invalid command";
    }
    return cmdStrArray[cmdType];
}


bool
CmdTypeIsPMI(
    _In_ SMPD_CMD_TYPE cmdType
    )
{
    return ( cmdType == SMPD_INIT ||
             cmdType == SMPD_FINALIZE ||
             cmdType == SMPD_DONE ||
             cmdType == SMPD_BARRIER ||
             cmdType == SMPD_DBPUT ||
             cmdType == SMPD_DBGET ||
             cmdType == SMPD_BCGET ||
             cmdType == SMPD_BCPUT ||
             cmdType == SMPD_SPAWN ||
             cmdType == SMPD_ADDDBS);
}
//---------------------------------------------------------------------------
//
// Create and initalize context
//
//----------------------------------------------------------------------------

void
smpd_free_context(
    _In_ _Post_ptr_invalid_ smpd_context_t *pContext
    )
{
    free( pContext->affinityList );
    delete[] pContext->jobObjName;
    delete[] pContext->job_context;
    delete[] pContext->pPwd;
    delete pContext->pAutoAffinityRegion;
    delete pContext;
}


static
bool
smpd_init_context(
    _Out_      smpd_context_t*     context,
    _In_       smpd_context_type_t type,
    _In_       ExSetHandle_t       set,
    _In_opt_z_ PCWSTR              pJobObjName,
    _In_opt_z_ PCWSTR              pPwd,
    _In_       BOOL                saveCreds
    )
{
    context->type                = type;
    context->set                 = set;
    context->on_cmd_error        = nullptr;
    context->name[0]             = L'\0';
    context->port_str[0]         = L'\0';
    context->job_context         = nullptr;
    context->stdioBuffer[0]      = '\0';
    context->process_id          = USHRT_MAX;
    context->process_rank        = -1;
    context->kvs                 = GUID_NULL;
    context->pipe                = INVALID_HANDLE_VALUE;
    context->processCount        = USHRT_MAX;
    context->affinityList        = nullptr;
    context->pAutoAffinityRegion = nullptr;

    if (pJobObjName != nullptr)
    {
        if( MPIU_WideCharToMultiByte( pJobObjName, &context->jobObjName ) != NOERROR )
        {
            return false;
        }
    }
    else
    {
        context->jobObjName = nullptr;
    }

    if (pPwd == nullptr)
    {
        context->pPwd = nullptr;
    }
    else
    {
        if( MPIU_WideCharToMultiByte( pPwd, &context->pPwd ) != NOERROR )
        {
            return false;
        }
        context->saveCreds = saveCreds;
    }

    return true;
}


smpd_context_t*
smpd_create_context(
    _In_       smpd_context_type_t type,
    _In_       ExSetHandle_t       set,
    _In_opt_z_ PCWSTR              pJobObjName,
    _In_opt_z_ PCWSTR              pPwd,
    _In_       BOOL                saveCreds
    )
{
    smpd_context_t *pContext = new smpd_context_t;
    if( pContext == nullptr )
    {
        return nullptr;
    }

    if (!smpd_init_context(pContext, type, set, pJobObjName, pPwd, saveCreds))
    {
        delete pContext;
        pContext = nullptr;
    }
    return pContext;
}


smpd_context_type_t
smpd_destination_context_type(
    _In_ int dest
    )
{
    INT16 src = smpd_process.tree_id;
    if( src == 0 && dest == 1 )
    {
        //
        // The special case of mpiexec and its only child (smpd top of tree)
        //
        return SMPD_CONTEXT_LEFT_CHILD;
    }

    ASSERT( smpd_process.tree_level >= 0 );

    int level_bit = 0x1 << smpd_process.tree_level;
    int sub_tree_mask = (level_bit << 1) - 1;
    smpd_context_type_t context_type = (smpd_context_type_t)0;

    if(( src ^ level_bit ) == ( dest & sub_tree_mask ))
    {
        context_type = SMPD_CONTEXT_LEFT_CHILD;
        smpd_dbg_printf(L"%d -> %d : returning SMPD_CONTEXT_LEFT_CHILD\n", src, dest);
    }
    else if( src == ( dest & sub_tree_mask ) )
    {
        context_type = SMPD_CONTEXT_RIGHT_CHILD;
        smpd_dbg_printf(L"%d -> %d : returning SMPD_CONTEXT_RIGHT_CHILD\n", src, dest);
    }
    else
    {
        smpd_dbg_printf(L"%d -> %d : returning 0, not a child. \n", src, dest);
    }

    return context_type;
}



//----------------------------------------------------------------------------
//
// Call command processor error handler
//
//----------------------------------------------------------------------------

static inline
void smpd_cmd_raise_error(
    _In_ smpd_overlapped_t* pov,
    _In_ int                error
    )
{
    pov->pContext->on_cmd_error(pov, error);
}


static inline 
HRESULT FindBizCardsNode(
    _In_                       GUID                        kvs,
    _In_                       bool                        add,
    _Outptr_result_maybenull_  smpd_process_biz_cards_t**  ppBizCardsNode,
    _In_                       UINT16                      nprocs = 0
    )
{
    EnterCriticalSection(&smpd_process.svcCriticalSection);

    //
    // find the node that has the right cache
    //
    smpd_process_biz_cards_t* pBizCardsNode = smpd_process.pBizCardsList;
    while (pBizCardsNode != nullptr)
    {
        if (IsEqualGUID(pBizCardsNode->kvs, kvs))
        {
            break;
        }
        pBizCardsNode = pBizCardsNode->pNext;
    }

    //
    // If the cache node for the given process group doesn't exist
    // create new node
    //
    if (pBizCardsNode == nullptr && add)
    {
        pBizCardsNode = new smpd_process_biz_cards_t;
        if (pBizCardsNode == nullptr)
        {
            return E_OUTOFMEMORY;
        }
        pBizCardsNode->kvs = kvs;
        pBizCardsNode->nproc = nprocs;
        pBizCardsNode->ppBizCards = new char*[nprocs];
        if (pBizCardsNode->ppBizCards == nullptr)
        {
            delete pBizCardsNode;
            return E_OUTOFMEMORY;
        }
        ZeroMemory(pBizCardsNode->ppBizCards, nprocs * sizeof(char*));

        pBizCardsNode->pNext = smpd_process.pBizCardsList;
        smpd_process.pBizCardsList = pBizCardsNode;
    }

    (*ppBizCardsNode) = pBizCardsNode;

    LeaveCriticalSection(&smpd_process.svcCriticalSection);
    return S_OK;
}


HRESULT
SmpdProcessBizCard(
    _In_    bool     isResult,
    _In_    SmpdCmd* pCmd,
    _Inout_ SmpdRes* pRes
    )
{
    SmpdCmdHdr* pHeader = reinterpret_cast<SmpdCmdHdr*>( pCmd );
    MPIU_Assert( pHeader->cmdType == SMPD_BCPUT || pHeader->cmdType == SMPD_BCGET );

    UINT16 rank;
    UINT16 nprocs = 0;
    GUID kvs;
    HRESULT hr;

    if( pHeader->cmdType == SMPD_BCPUT )
    {
        rank = pCmd->BcputCmd.rank;
        kvs = pCmd->BcputCmd.kvs;
        nprocs = pCmd->BcputCmd.nprocs;
    }
    else
    {
        rank = pCmd->BcgetCmd.rank;
        kvs = pCmd->BcgetCmd.kvs;
        nprocs = pCmd->BcgetCmd.nprocs;
    }

    if (pHeader->cmdType == SMPD_BCPUT)
    {
        if( isResult )
        {
            //
            // Take no action on the response since we already
            // copied the business card on the request.
            //
            return S_OK;
        }

        smpd_process_biz_cards_t* pBizCardsNode;
        hr = FindBizCardsNode(kvs, true, &pBizCardsNode, nprocs);
        if (hr != S_OK)
        {
            return hr;
        }

        MPIU_Assert(pBizCardsNode->ppBizCards[rank] == nullptr);

        //
        // It is possible that we have not even launched a single process
        // (and thus have not initialized smpd_process.ppBizcards)
        //
        char* p = new char[_countof(pCmd->BcputCmd.value)];
        if( p == nullptr )
        {
            return E_OUTOFMEMORY;
        }

        MPIU_Strcpy( p, _countof(pCmd->BcputCmd.value), pCmd->BcputCmd.value );
        smpd_dbg_printf(L"Caching business card for rank %hu\n", rank );
        pBizCardsNode->ppBizCards[rank] = p;

        return S_OK;
    }

    //
    // SMPD_BCGET response
    //
    smpd_process_biz_cards_t* pBizCardsNode;

    if (isResult)
    {
        //
        // It is possible that there were more than one
        // BCGET requests to obtain the same business card.
        // If this request is handled after the previous one
        // the business card has been copied already.
        //
        hr = FindBizCardsNode(kvs, true, &pBizCardsNode, nprocs);
        if (hr != S_OK)
        {
            return hr;
        }

        if (pBizCardsNode->ppBizCards[rank] != nullptr)
        {
            return S_OK;
        }

        //
        // Copy the business card into our cache
        //
        char* p = new char[_countof(pRes->BcgetRes.value)];
        if( p == nullptr )
        {
            return E_OUTOFMEMORY;
        }

        smpd_dbg_printf(L"Caching business card for rank %hu\n", rank );
        MPIU_Strcpy( p, _countof(pRes->BcgetRes.value), pRes->BcgetRes.value );

        pBizCardsNode->ppBizCards[rank] = p;
        return S_OK;
    }

    //
    // SMPD_BCGET request
    //
    hr = FindBizCardsNode(kvs, false, &pBizCardsNode);
    if (hr != S_OK)
    {
        return hr;
    }
    if (pBizCardsNode && pBizCardsNode->ppBizCards[rank] != nullptr)
    {
        smpd_dbg_printf(L"Found cached business card for rank %hu\n", rank );
        MPIU_Strcpy(
            pRes->BcgetRes.value,
            _countof(pRes->BcgetRes.value),
            pBizCardsNode->ppBizCards[rank]);

        pRes->BcgetRes.header.err = NOERROR;

        return S_OK;
    }

    return S_FALSE;
}


void
smpd_handle_result(
    _In_ EXOVERLAPPED* pexov
    )
{
    smpd_overlapped_t* pov = smpd_ov_from_exov( pexov );
    SmpdCmd* pCmd = pov->pCmd;
    SmpdCmdHdr* pHeader = reinterpret_cast<SmpdCmdHdr*>(pCmd);
    SmpdResWrapper* pRes =
        CONTAINING_RECORD(pov->pRes, SmpdResWrapper, Res);

    RPC_STATUS status;
    UINT retries = 0;

    //
    // This function is invoked on a completion and RPC completions do
    // not use the ov.Internal since the overlapped structure is
    // passed through from the caller
    //
    ASSERT( smpd_get_overlapped_result(pov) == NOERROR );

    smpd_dbg_printf(L"Handling cmd=%s result\n",
                    CmdTypeToString(pHeader->cmdType));

    //
    // Figure out if this result should be processed locally or forwarded.
    // If there is a need to forward this result, there should be a pending PRC
    // stored in this result's async RPC request
    //
    SmpdPendingRPC_t* pPendingRPC =
        static_cast<SmpdPendingRPC_t*>( pov->pAsync->UserInfo );
    if( pPendingRPC == nullptr )
    {
        //
        // Process the result locally
        //
        smpd_dbg_printf(L"cmd=%s result will be handled locally\n",
                        CmdTypeToString(pHeader->cmdType));

        do
        {
            //
            // Complete the Async so that we get the results back
            // from the server
            //
            status = RpcAsyncCompleteCall( pov->pAsync, nullptr );
            if( status == RPC_S_OK )
            {
                free(pov->pAsync);
                //
                // Check the result before invoking the result handler
                //
                DWORD remoteErr = reinterpret_cast<SmpdResHdr*>(pRes)->err;

                //
                // For results that are to be handled by PMI clients we let
                // them deal with the error
                //
                if( remoteErr != NOERROR &&
                    CmdTypeIsPMI( pHeader->cmdType ) == false )
                {
                    smpd_dbg_printf(
                        L"smpd id %hd failed to process cmd=%s error=%u.\n",
                        pHeader->dest,
                        CmdTypeToString( pHeader->cmdType ),
                        remoteErr );

                    //
                    // This will invoke the error handler assigned to
                    // this invocation's context.
                    // More specifically:
                    // 1) for mpiexec, it will invoke MpiexecHandleCmdError
                    // 2) for smpd, it will invoke SmpdHandleCmdError,
                    // 3) for pmi, it will invoke PmiHandleCmdError
                    //
                    smpd_cmd_raise_error( pov, remoteErr );

                    delete pRes;
                    delete pCmd;
                    smpd_free_overlapped( pov );

                    return;
                }

                //
                // Invoke the result handler. The result handler
                // needs to free any memory allocated by the RPC stub.
                // Note that we want to delay the deallocation of pCmd
                // because some result handlers will need the information
                // in pCmd
                //
                if( pRes->OnResultFn != nullptr )
                {
                    pRes->OnResultFn( pov );
                }
                break;
            }
            else if( status != RPC_S_ASYNC_CALL_PENDING )
            {
                //
                // The other side most likely crashed or exited ungracefully
                //

                //
                // Give the RPC runtime the chance to clean up the resource
                //
                if( pov->pContext->hServerContext != nullptr )
                {
                    RpcSsDestroyClientContext( &pov->pContext->hServerContext );
                }
                smpd_cmd_raise_error( pov, status );
                break;
            }
        } while( ++retries < 3 &&
                 !SleepEx( 1000, FALSE ) );

        if( status != RPC_S_OK )
        {
            free(pov->pAsync);
            smpd_err_printf(L"Exceeded maximum number of retries, failing with error %ld.\n", status);
        }

        delete pRes;
        delete pCmd;
        smpd_free_overlapped( pov );
        return;
    }

    //
    // The case where we need to forward the result. First we need to
    // complete this existing Async request to get the result data;
    // then we complete the original async request
    //
    // We do not free pCmd and pRes because those are pointers to memory
    // allocated from the previous command.
    //

    do
    {
        status = RpcAsyncCompleteCall( pov->pAsync, nullptr );
        if( status == RPC_S_OK )
        {
            //
            // Since we're acting as the forwarder we will just
            // forward the result
            //
            if( CmdTypeIsPMI( pHeader->cmdType) )
            {
                smpd_dbg_printf(
                    L"forward %s result to dest=%hd ctx_key=%hu\n",
                    CmdTypeToString(pHeader->cmdType),
                    pHeader->src,
                    pHeader->ctx_key);
            }
            else
            {
                smpd_dbg_printf(
                    L"forward result %s to dest=%hd \n",
                    CmdTypeToString(pHeader->cmdType),
                    pHeader->src);
            }

            if( pHeader->cmdType == SMPD_BCPUT || pHeader->cmdType == SMPD_BCGET )
            {
                HRESULT hr = SmpdProcessBizCard( true, pov->pCmd, pov->pRes );

                //
                // Errors from SmpdProcessBizCard isn't fatal. We only
                // trap it in debug.
                //
                MPIU_Assert( SUCCEEDED(hr) );

                //
                // In retail build hr isn't used.
                //
                UNREFERENCED_PARAMETER( hr );
            }

            SmpdResHdr* pResHeader = reinterpret_cast<SmpdResHdr*>( pov->pRes );
            pPendingRPC->UpdateStatus( pResHeader->err );
            pPendingRPC->Release();
            break;
        }
        else if( status != RPC_S_ASYNC_CALL_PENDING )
        {
            smpd_dbg_printf(L"Error %d detected while trying to forward result of %s\n",
                            status, CmdTypeToString(pHeader->cmdType));
            //
            // Either server crashed or some other errors in the RPC runtime
            //
            RpcSsDestroyClientContext( &pov->pContext->hServerContext );
            break;
        }
        else
        {
            smpd_dbg_printf(L"RPC Pending, trying again\n");
        }

    } while( ++retries < 3 &&
             status == RPC_S_ASYNC_CALL_PENDING &&
             !SleepEx( 1000, FALSE ));

    if( status != RPC_S_OK )
    {
        pPendingRPC->UpdateStatus( status );
        pPendingRPC->Release();

        if( status == RPC_S_ASYNC_CALL_PENDING )
        {
            smpd_err_printf(L"Exceeded maximum number of retries. Aborting...\n");
        }

        smpd_cmd_raise_error( pov, status );
    }

    smpd_free_overlapped( pov );
    return;
}


_Success_( return == true )
static bool
smpd_command_destination(
    _In_ int dest,
    _Outptr_ smpd_context_t **dest_context )
{
    ASSERT( dest_context != nullptr );
    int src, level_bit, sub_tree_mask;

    src = smpd_process.tree_id;

    ASSERT( src != dest );

    if(src == SMPD_IS_ROOT)
    {
        /* this assumes that the root uses the left context for it's only child. */
        if( smpd_process.left_context == nullptr )
        {
            return false;
        }

        *dest_context = smpd_process.left_context;
        smpd_dbg_printf(L"%d -> %d : returning left_context\n", src, dest);
        return true;
    }

    if(dest < src)
    {
        if(smpd_process.parent_context == nullptr)
        {
            return false;
        }
        *dest_context = smpd_process.parent_context;
        smpd_dbg_printf(L"%d -> %d : returning parent_context: %d < %d\n",
                        src, dest, dest, src);
        return true;
    }

    level_bit = 0x1 << smpd_process.tree_level;
    sub_tree_mask = (level_bit << 1) - 1;

    if(( src ^ level_bit ) == ( dest & sub_tree_mask ))
    {
        if(smpd_process.left_context == nullptr)
        {
            return false;
        }

        *dest_context = smpd_process.left_context;
        smpd_dbg_printf(L"%d -> %d : returning left_context\n", src, dest);
        return true;
    }

    if( src == ( dest & sub_tree_mask ) )
    {
        if(smpd_process.right_context == nullptr)
        {
            return false;
        }

        *dest_context = smpd_process.right_context;
        smpd_dbg_printf(L"%d -> %d : returning right_context\n", src, dest);
        return true;
    }

    if(smpd_process.parent_context == nullptr)
    {
        return false;
    }

    *dest_context = smpd_process.parent_context;
    smpd_dbg_printf(L"%d -> %d : returning parent_context: fall through\n", src, dest);
    return true;
}


void
smpd_forward_command(
    _In_ EXOVERLAPPED* pexov
    )
{
    smpd_overlapped_t* pov      = smpd_ov_from_exov( pexov );
    SmpdCmd*         pCmd       = pov->pCmd;
    SmpdRes*         pRes       = pov->pRes;
    SmpdCmdHdr*      pHeader    = reinterpret_cast<SmpdCmdHdr*>( pov->pCmd );
    PRPC_ASYNC_STATE pAsync     = pov->pAsync;

    if( CmdTypeIsPMI( pHeader->cmdType ) )
    {
        smpd_dbg_printf(
            L"forwarding command %s src=%hd ctx_key=%hu\n",
            CmdTypeToString( pHeader->cmdType ),
            pHeader->src,
            pHeader->ctx_key );
    }
    else
    {
        smpd_dbg_printf(
            L"forwarding command %s src=%d\n",
            CmdTypeToString( pHeader->cmdType ),
            pHeader->src );
    }

    StackGuardRef<SmpdPendingRPC_t> pPendingRPC(
        new SmpdPendingRPC_t( *pAsync, pRes ) );
    if( pPendingRPC.get() == nullptr )
    {
        SmpdResHdr* pResHdr = reinterpret_cast<SmpdResHdr*>(pRes);
        pResHdr->err = ERROR_NOT_ENOUGH_MEMORY;

        OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
            "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");
        RpcAsyncCompleteCall( pAsync, nullptr );
        OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
            "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no possible recovery.");

        return;
    }

    //
    // This message is intended to be forwarded
    // We can't complete this async request until we receive the result
    // from the forwardee
    //

    //
    // Find out where to forward the command
    //
    smpd_context_t* pDest;
    if( smpd_command_destination( pHeader->dest, &pDest ) != true )
    {
        smpd_err_printf(
            L"Invalid command received, unable to determine the destination %d\n",
            pHeader->dest );
        pPendingRPC->UpdateStatus( ERROR_INVALID_FUNCTION );
        return;
    }

    if (pHeader->cmdType == SMPD_LAUNCH)
    {
        SmpdLaunchCmd* pLaunchCmd = &pCmd->LaunchCmd;
        smpd_barrier_forward_t* pBarrierForwardNode = smpd_process.barrier_forward_list;
        while (pBarrierForwardNode != nullptr)
        {
            if (IsEqualGUID(pBarrierForwardNode->kvs, pLaunchCmd->kvs))
            {
                break;
            }
            pBarrierForwardNode = pBarrierForwardNode->next;
        }

        if (pBarrierForwardNode == nullptr)
        {
            pBarrierForwardNode = new smpd_barrier_forward_t(pLaunchCmd->kvs);
            if (pBarrierForwardNode == nullptr)
            {
                return;
            }
            pBarrierForwardNode->next = smpd_process.barrier_forward_list;
            smpd_process.barrier_forward_list = pBarrierForwardNode;
        }
        if (pDest == smpd_process.left_context)
        {
            pBarrierForwardNode->left = 1;
        }
        else if (pDest == smpd_process.right_context)
        {
            pBarrierForwardNode->right = 1;
        }
    }

    ASSERT( pDest != nullptr );
    pPendingRPC->AddRef();
    RPC_STATUS status = smpd_post_command( pDest,
                                           pCmd,
                                           pRes,
                                           pPendingRPC.get() );
    if( status != RPC_S_OK )
    {
        pPendingRPC->UpdateStatus( status );
        pPendingRPC->Release();
    }

}


void
smpd_handle_command(
    _In_ EXOVERLAPPED* pexov
    )
{
    smpd_overlapped_t* pov      = smpd_ov_from_exov( pexov );
    SmpdCmdHdr*      pHeader  = reinterpret_cast<SmpdCmdHdr*>( pov->pCmd );
    ASSERT( pHeader->cmdType < SMPD_CMD_MAX );

    if( CmdTypeIsPMI( pHeader->cmdType ) )
    {
        smpd_dbg_printf(
            L"handling command %s src=%hd ctx_key=%hu\n",
            CmdTypeToString( pHeader->cmdType ),
            pHeader->src,
            pHeader->ctx_key );
    }
    else
    {
        smpd_dbg_printf(
            L"handling command %s src=%d\n",
            CmdTypeToString( pHeader->cmdType ),
            pHeader->src );

    }

    //
    // Prepare the result command and let the handler
    // add any additional information to the result as appropriate.
    //
    smpd_init_result_command( pov->pRes,
                              pHeader->src );

    //
    // Invoke the appropriate handler for this command.
    // Need to store the information from the input buffer
    // as it will become invalid after the completion of the
    // RpcAsyncCompleteCall
    //
    SMPD_CMD_TYPE type = pHeader->cmdType;
    INT16         src  = pHeader->src;

    ASSERT( type >=0 && type < SMPD_CMD_MAX );
    DWORD         err  = smpd_process.cmdHandler[type]( pov );

    //
    // These commands require special handling and they will
    // complete the async request by themselves
    //
    if( type == SMPD_BARRIER )
    {
        //
        // We do not complete the barrier command until we get
        // all the barrier signals from all MPI processes
        //
        return;
    }
    else if( type == SMPD_CLOSED )
    {
        smpd_free_overlapped( pov );
        return;
    }

    reinterpret_cast<SmpdResHdr*>(pov->pRes)->err = err;

    //
    // Send result back after finishing the command
    //
    RPC_STATUS status = RpcAsyncCompleteCall( pov->pAsync, nullptr );
    ASSERT( status == RPC_S_OK );
    if( status != RPC_S_OK )
    {
        smpd_dbg_printf( L"failed processing command %s src=%d error %u",
                         CmdTypeToString( type ),
                         src,
                         err );
    }

    smpd_free_overlapped( pov );
    return;
}


//
// Initialize the embedded pointers in the result structure
// because RPC only allocates memory for embedded [out] pointers
// that are set to null when making the RPC calls.
//
static void
SmpdResInitOutPtrs(
    _In_ SMPD_CMD_TYPE cmdType,
    _In_ SmpdRes* pRes
    )
{
    switch(cmdType)
    {
    case SMPD_COLLECT:
        pRes->CollectRes.hwsummary = nullptr;
        pRes->CollectRes.hwtree = nullptr;
        pRes->CollectRes.affList = nullptr;
        break;
    case SMPD_LAUNCH:
        pRes->LaunchRes.ctxkeyArray = nullptr;
        pRes->LaunchRes.error_msg = nullptr;
    }
}


DWORD
smpd_post_command(
    _In_ smpd_context_t* pContext,
    _In_ SmpdCmd* pCmd,
    _In_ SmpdRes* pRes,
    _In_opt_ SmpdPendingRPC_t* pPendingRPC,
    _In_opt_ char* pPmiTmpBuffer,
    _In_opt_ DWORD* pPmiTmpErr
    )
{
    smpd_overlapped_t* pov = smpd_create_overlapped();
    if( pov == nullptr )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    RPC_ASYNC_STATE* pAsync = static_cast<RPC_ASYNC_STATE*>(
        malloc( sizeof(RPC_ASYNC_STATE) ) );
    if( pAsync == nullptr )
    {
        smpd_free_overlapped( pov );
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    RPC_STATUS status = RpcAsyncInitializeHandle( pAsync, sizeof(RPC_ASYNC_STATE) );
    if( status != RPC_S_OK )
    {
        smpd_free_overlapped( pov );
        smpd_err_printf(L"Failed to initialize async handle error %ld\n", status );
        return status;
    }

    pAsync->NotificationType = RpcNotificationTypeIoc;
    pAsync->u.IOC.hIOPort = smpd_process.set;
    pAsync->u.IOC.dwCompletionKey = 0;
    pAsync->u.IOC.dwNumberOfBytesTransferred = sizeof( smpd_overlapped_t* );
    pAsync->u.IOC.lpOverlapped = reinterpret_cast<LPOVERLAPPED>( &pov->uov );
    pov->uov.ov.Internal = NOERROR;

    //
    // pPendingRPC contains the RPC_ASYNC_STATE request that we cannot
    // complete until AFTER we finish this request. We store it here
    // so when we complete this request we can go and complete it.
    //
    pAsync->UserInfo = pPendingRPC;

    pov->pAsync = pAsync;
    pov->pCmd = pCmd;
    pov->pRes = pRes;

    SMPD_CMD_TYPE cmdType = reinterpret_cast<SmpdCmdHdr*>(pov->pCmd)->cmdType;

    SmpdResInitOutPtrs( cmdType, pRes );
    smpd_init_overlapped( pov, smpd_handle_result, pContext );

    pov->pPmiTmpBuffer = pPmiTmpBuffer;
    pov->pPmiTmpErr = pPmiTmpErr;

    if( CmdTypeIsPMI( cmdType ) )
    {
        smpd_dbg_printf(
            L"posting command %s to %s, src=%hd, ctx_key=%hu, dest=%hd.\n",
            CmdTypeToString(cmdType),
            smpd_get_context_str(pContext),
            reinterpret_cast<SmpdCmdHdr*>(pCmd)->src,
            reinterpret_cast<SmpdCmdHdr*>(pCmd)->ctx_key,
            reinterpret_cast<SmpdCmdHdr*>(pCmd)->dest );
    }
    else
    {
        smpd_dbg_printf(
            L"posting command %s to %s, src=%hd, dest=%hd.\n",
            CmdTypeToString(cmdType),
            smpd_get_context_str(pContext),
            reinterpret_cast<SmpdCmdHdr*>(pCmd)->src,
            reinterpret_cast<SmpdCmdHdr*>(pCmd)->dest );
    }

    status = RPC_S_OK;
    RpcTryExcept
    {
        RpcCliSmpdMgrCommandAsync(
            pAsync,
            pContext->hServerContext,
            cmdType,
            pCmd,
            pRes );
    }
    RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
    {
        status = RpcExceptionCode();
    }
    RpcEndExcept;

    if( status != RPC_S_OK )
    {
        smpd_err_printf(L"Failed to post smpd command error %ld\n", status);
        smpd_free_overlapped( pov );
    }

    return static_cast<DWORD>(status);
}


DWORD
SmpdPostCommandSync(
    _In_ smpd_context_t* pContext,
    _In_ SmpdCmd* pCmd,
    _In_ SmpdRes* pRes
    )
{
    SMPD_CMD_TYPE cmdType = reinterpret_cast<SmpdCmdHdr*>(pCmd)->cmdType;
    SmpdResInitOutPtrs( cmdType, pRes );

    RPC_STATUS status = RPC_S_OK;
    RpcTryExcept
    {
        RpcCliSmpdMgrCommandSync(
            pContext->hServerContext,
            cmdType,
            pCmd,
            pRes );
    }
    RpcExcept( I_RpcExceptionFilter( RpcExceptionCode() ) )
    {
        status = RpcExceptionCode();
    }
    RpcEndExcept;

    return static_cast<DWORD>( status );
}
