// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiexec.h"
#include "util.h"
#include "PmiDbgImpl.h"

/*
 * This routine is a special dummy routine that is used to provide a
 * location for a debugger to set a breakpoint on, allowing a user (and the
 * debugger) to attach to MPI processes after MPI_Init succeeds but before
 * MPI_Init returns control to the user.
 *
 * This routine can also initialize any datastructures that are required
 *
 */
extern "C" __declspec(dllexport)
void* __stdcall
MPIR_Breakpoint( int n, ... )
{
    va_list args;
    va_start( args, n );
    va_end( args );
    return &MPIR_Proctable_size;
}


//
// This routine is a special dummy routine that is used for the debugger
// to tell whether this mpiexec belongs to MSMPI
//
// This routine also serves as a dummy address load for MPIR_dll_name.
// Without "using" MPIR_dll_name, the debugger lib does not get pulled in
// and the export will not be made available to mpiexec
//
extern "C" __declspec(dllexport)
void* __stdcall
MSMPI_MpiexecDbg( int n, ... )
{
    va_list args;
    va_start( args, n );
    va_end( args );
    return &MPIR_dll_name;
}


smpd_process_group_t*
find_pg(
    const GUID& kvs
    )
{
    smpd_process_group_t* pg;
    for(pg = smpd_process.pg_list; pg != nullptr; pg = pg->next)
    {
        if( IsEqualGUID( pg->kvs, kvs ) )
        {
            return pg;
        }
    }

    return nullptr;
}


static DWORD
RetrieveAffinityForLaunchNodes(
    _In_ const SmpdCollectRes* pCollectRes,
    _Inout_ smpd_host_t*       pHost
    )
{
    if(!smpd_process.affinityOptions.isSet
       || (smpd_process.affinityOptions.affinityTableStyle == 0 &&
           smpd_process.affinityOptions.hwTableStyle == 0) )
    {
        return NOERROR;
    }

    pHost->Summary = static_cast<HWSUMMARY*>( malloc( pCollectRes->cbHwSummary ) );
    if( pHost->Summary == nullptr )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    CopyMemory( pHost->Summary, pCollectRes->hwsummary, pCollectRes->cbHwSummary );

    HWAFFINITY*     pAffinity = reinterpret_cast<HWAFFINITY*>(pCollectRes->affList);
    for( smpd_rank_data_t* pNode =
             next_rank_data(nullptr, pHost);
         pNode != nullptr;
         pNode = next_rank_data(pNode, pHost)
       )
    {
        pNode->affinity = pAffinity[pNode->nodeProcOrder];
    }

    return NOERROR;
}


static DWORD
RetrieveHwTreeForHost(
    _In_ const SmpdCollectRes* pCollectRes,
    _Inout_ smpd_host_t*       pHost
    )
{
    if(!smpd_process.affinityOptions.isSet
       || smpd_process.affinityOptions.hwTableStyle == 0)
    {
        return NOERROR;
    }

    pHost->hwTree = static_cast<HWTREE*>( malloc( pCollectRes->cbHwTree ) );
    if( pHost->hwTree == nullptr )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    CopyMemory( pHost->hwTree, pCollectRes->hwtree, pCollectRes->cbHwTree );

    return S_OK;
}


DWORD
mpiexec_handle_invalid_command(
    _In_ smpd_overlapped_t* pov
    )
{
    UNREFERENCED_PARAMETER( pov );
    smpd_err_printf(L"Internal error: Invalid command received by mpiexec.\n");
    smpd_post_abort_command(L"Internal error: Invalid command received by mpiexec.\n");
    return ERROR_INVALID_DATA;
}


//
// Summary:
// Process the hardware information returned from the collect command
//
// Parameters:
// pContext:  Pointer to the context of the client that is returning
//            the result
// pRes    :  The result structure that has the hardware information
//
void
mpiexec_handle_collect_result(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdCollectRes* pCollectRes = &pov->pRes->CollectRes;
    UINT16          HostId = pCollectRes->HostId;

    if( HostId == 0 )
    {
        smpd_post_abort_command(L"invalid hostid in collect result.\n");
        goto CleanUp;

    }

    bool         allCollected = true;
    smpd_host_t* host      = nullptr;
    for( smpd_host_t* p = smpd_process.host_list;
         p != nullptr;
         p = static_cast<smpd_host_t*>( p->Next ) )
    {
        if( p->HostId == pCollectRes->HostId )
        {
            ASSERT( host == nullptr );

            if( p->collected != FALSE )
            {
                smpd_post_abort_command(
                    L"Collect result already recieved from host %d.\n", p->HostId);
                goto CleanUp;
            }

            if( RetrieveAffinityForLaunchNodes(pCollectRes, p) != NOERROR )
            {
                goto CleanUp;
            }

            if( RetrieveHwTreeForHost(pCollectRes, p) != NOERROR )
            {
                goto CleanUp;
            }

            p->collected = TRUE;
            host = p;
        }
        else
        {
            allCollected &= (p->collected != FALSE);
        }
    }

    if( host == nullptr )
    {
        smpd_post_abort_command(
            L"value of hostid parameter in collect result is invalid.\n");
        goto CleanUp;
    }

    if( allCollected )
    {
        //
        // If all results are collected, post the startdbs command
        //
        smpd_dbg_printf(L"Finished collecting hardware summary.\n");
        DWORD rc = mpiexec_send_start_dbs_command( pov->pContext );
        if( rc != RPC_S_OK )
        {
            smpd_post_abort_command(L"unable to post startdbs.\n");
            goto CleanUp;
        }

        mpiexec_print_hwtree(smpd_process.host_list);
        mpiexec_print_affinity_table(smpd_process.launch_list, smpd_process.host_list);
    }

CleanUp:
    midl_user_free( pCollectRes->hwsummary );
    midl_user_free( pCollectRes->hwtree );
    midl_user_free( pCollectRes->affList );

}


//
// Summary:
// Sends collect commands to each host in the host list to obtain
// hardware information
//
DWORD
mpiexec_send_collect_commands()
{
    for( smpd_host_t* p = smpd_process.host_list;
         p != nullptr;
         p = static_cast<smpd_host_t*>( p->Next ) )
    {

        smpd_rank_data_t* pNode = next_rank_data( nullptr, p );
        SmpdCmd* pCmd = smpd_create_command(
            SMPD_COLLECT,
            SMPD_IS_ROOT,
            static_cast<INT16>(p->HostId) );
        if( pCmd == nullptr )
        {
            smpd_err_printf(L"unable to create a collect command.\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        pCmd->CollectCmd.nodeProcCount = p->nodeProcCount;
        pCmd->CollectCmd.HostId = static_cast<UINT16>(p->HostId);
        pCmd->CollectCmd.affOptions = smpd_process.affinityOptions;

        if( smpd_process.affinityOptions.isExplicit )
        {
            pCmd->CollectCmd.cbExplicitAffinity = sizeof(HWAFFINITY) * p->nodeProcCount;

            HWAFFINITY* pExplicitAffinity = new HWAFFINITY[p->nodeProcCount];
            if(pExplicitAffinity == nullptr)
            {
                smpd_err_printf(L"unable to allocate memory for explicit affinity\n");
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            while(pNode != nullptr)
            {
                pExplicitAffinity[pNode->nodeProcOrder] = pNode->affinity;
                pNode = next_rank_data(pNode, p);
            }

            pCmd->CollectCmd.ExplicitAffinity = reinterpret_cast<BYTE*>(pExplicitAffinity);
        }
        else
        {
            pCmd->CollectCmd.cbExplicitAffinity = 0;
            pCmd->CollectCmd.ExplicitAffinity = nullptr;
        }

        SmpdResWrapper* pRes = smpd_create_result_command(
            smpd_process.tree_id,
            &mpiexec_handle_collect_result );
        if( pRes == nullptr )
        {
            delete[] pCmd->CollectCmd.ExplicitAffinity;
            delete pCmd;
            smpd_err_printf(L"unable to create result for collect command.\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        //
        // Post the async RPC request and the progress engine will handle it
        // when the result comes back
        //
        DWORD rc = smpd_post_command(
            smpd_process.left_context,
            pCmd,
            &pRes->Res,
            nullptr );
        if( rc != RPC_S_OK )
        {
            delete pRes;
            delete[] pCmd->CollectCmd.ExplicitAffinity;
            delete pCmd;
            smpd_err_printf(L"Unable to post collect command error %u\n", rc);
            return rc;
        }
    }

    return NOERROR;
}


void
mpiexec_handle_kill_result(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdKillCmd*    pKillCmd = &pov->pCmd->KillCmd;

    //
    // Look up the PG
    //
    smpd_process_group_t* pg = find_pg( pKillCmd->kvs );
    ASSERT( pg != nullptr );

    pg->num_pending_kills--;
    if( pg->num_pending_kills == 0 && pg->num_pending_suspends == 0 )
    {
        mpiexec_tear_down_tree();
    }
}


static DWORD
mpiexec_send_kill_command(
    const GUID& kvs,
    INT16 dest,
    int exit_code,
    UINT16 ctx_key
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_KILL,
        smpd_process.tree_id,
        dest );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create kill command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pCmd->KillCmd.kvs = kvs;
    pCmd->KillCmd.header.ctx_key = ctx_key;
    pCmd->KillCmd.exit_code = exit_code;

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        &mpiexec_handle_kill_result );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for kill command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    smpd_dbg_printf(L"posting kill command smpd id=%hd, ctx_key=%hu\n",
                    dest, ctx_key );

    DWORD rc = smpd_post_command(
        smpd_process.left_context,
        pCmd,
        &pRes->Res,
        nullptr
        );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"Unable to post kill command error %u\n", rc);
        return rc;
    }

    return NOERROR;
}


void
mpiexec_handle_suspend_result(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdSuspendCmd* pSuspendCmd = &pov->pCmd->SuspendCmd;
    UINT16          rank        = pSuspendCmd->rank;

    //
    // Look up the PG
    //
    smpd_process_group_t* pg = find_pg( pSuspendCmd->kvs );

    VERIFY( pg != nullptr );
    VERIFY( rank < pg->num_procs );

    pg->processes[rank].suspended = TRUE;
    pg->num_pending_suspends--;

    //
    // We delay killing the processes until all processes
    // have been suspended
    //
    if(pg->num_pending_suspends != 0)
    {
        return;
    }

    for( UINT i = 0; i < pg->num_procs; i++ )
    {
        const smpd_exit_process_t* p = &pg->processes[i];

        if(p->exited)
        {
            smpd_dbg_printf(L"suspended rank %u already exited, no need to kill it.\n", i);
            continue;
        }

        int err_code;
        if( p->abortreason == arFatalError )
        {
            err_code = -arFatalError;
        }
        else
        {
            err_code = -1;
        }

        DWORD rc = mpiexec_send_kill_command(pg->kvs, p->node_id, err_code, p->ctx_key);
        if( rc != NOERROR )
        {
            smpd_post_abort_command(
                L"failed to send kill command to rank %d. error %d\n", i, rc);
            return;
        }
        pg->num_pending_kills++;
    }
}


static DWORD
mpiexec_send_suspend_command(
    const GUID& kvs,
    UINT16 rank,
    const smpd_exit_process_t* proc
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_SUSPEND,
        SMPD_IS_ROOT,
        proc->node_id );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create a suspend command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        &mpiexec_handle_suspend_result );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for suspend command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pCmd->SuspendCmd.kvs = kvs;
    pCmd->SuspendCmd.rank = rank;
    pCmd->SuspendCmd.header.ctx_key = proc->ctx_key;

    smpd_dbg_printf(L"Suspending rank %hu, smpd id = %hu, ctx_key=%hu\n",
                    rank, proc->node_id, proc->ctx_key);

    /* send the suspend command */
    DWORD rc = smpd_post_command(
        smpd_process.left_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK)
    {
        smpd_err_printf(L"unable to send the suspend command for rank %hu error %u.\n", rank, rc);
        return rc;
    }

    return NOERROR;
}


void
mpiexec_handle_launch_result(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdLaunchCmd* pLaunchCmd = &pov->pCmd->LaunchCmd;
    SmpdLaunchRes* pLaunchRes = &pov->pRes->LaunchRes;
    if( pLaunchRes->header.err != NOERROR )
    {
        smpd_process.nproc_exited++;
        wchar_t err_msg[SMPD_MAX_ERROR_LEN];

        if( StringCchCopyW(
                err_msg,
                _countof(err_msg),
                pLaunchRes->error_msg) == S_OK )
        {
            smpd_post_abort_command(L"%s\n", err_msg);
        }
        else
        {
            smpd_post_abort_command(L"launch failed\n");
        }
        midl_user_free( pLaunchRes->error_msg );
        return;
    }

    for( unsigned i = 0; i < pLaunchCmd->rankCount; ++i )
    {
        UINT16 pg_rank = pLaunchCmd->rankArray[i];
        UINT16 pg_ctx = pLaunchRes->ctxkeyArray[i];

        smpd_process_group_t* pg = find_pg( pLaunchCmd->kvs );
        VERIFY(pg != NULL);
        VERIFY(pg_rank < pg->num_procs);

        pg->processes[pg_rank].launched = TRUE;
        pg->processes[pg_rank].ctx_key = pg_ctx;

        //
        // After all the processes have been launched, let the PMI extension know.
        // At this point, mpiexec can inquire about the rank information
        //
        pg->num_results++;
        if( pg->num_procs == pg->num_results )
        {
            NotifyPmiDbgExtensions(MpiexecNotifyAfterCreateProcesses);
            MPIR_debug_state = MPIR_DEBUG_SPAWNED;
            MPIR_Breakpoint( MPIR_Proctable_size );
        }

        if(pg->processes[pg_rank].suspend_pending)
        {
            /* send the delayed suspend command */
            DWORD rc = mpiexec_send_suspend_command(
                pg->kvs,
                pg_rank,
                &pg->processes[pg_rank]);
            if( rc != NOERROR )
            {
                smpd_post_abort_command(
                    L"failed to send the pending suspend command to rank %hu. error %u.\n",
                    pg_rank,
                    rc );
            }
            continue;
        }

        smpd_dbg_printf(L"successfully launched process %u\n", pg_rank);
        if(smpd_process.stdin_redirecting == FALSE)
        {
            UINT16 rank = pLaunchCmd->rankArray[i];
            if( rank == 0 )
            {
                smpd_dbg_printf(L"root process launched, starting stdin redirection.\n");

                DWORD rc = mpiexec_redirect_stdin();
                if( rc != NOERROR )
                {
                    smpd_post_abort_command(
                        L"failed to redirect stdin to rank 0. error %u.\n", rc );
                    return;
                }
            }
        }
    }

    midl_user_free( pLaunchRes->ctxkeyArray );

}


static
void
FindNodeIdArray(
    GUID kvs,
    UINT16** node_id_array
    )
{
    smpd_node_id_node_t* pNode = smpd_process.node_id_list;
    while (pNode != nullptr)
    {
        if (IsEqualGUID(kvs, pNode->kvs))
        {
            break;
        }
        pNode = pNode->next;
    }
    if (pNode == nullptr)
    {
        (*node_id_array) = nullptr;
    }
    else
    {
        (*node_id_array) = pNode->node_id_array;
    }
}


static DWORD
mpiexec_send_launch_command(
    smpd_launch_block_t* node,
    smpd_process_group_t* pg,
    bool fSpawn,
    UINT16 totalRankCount
    )
{
    DWORD rc = ERROR_NOT_ENOUGH_MEMORY;
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_LAUNCH,
        SMPD_IS_ROOT,
        static_cast<UINT16>(node->host_id) );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create a launch command.\n");
        return rc;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        &mpiexec_handle_launch_result );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for launch command\n");
        return rc;
    }

    SmpdLaunchCmd* pLaunchCmd = &pCmd->LaunchCmd;

    pLaunchCmd->appExe = const_cast<wchar_t*>(node->pExe);
    pLaunchCmd->appArgs = node->pArgs;
    pLaunchCmd->appNum = static_cast<UINT8>(node->appnum);
    pLaunchCmd->env = node->ppEnv;
    pLaunchCmd->envCount = node->envCount;
    pLaunchCmd->wdir = const_cast<wchar_t*>(node->pDir);
    pLaunchCmd->path = const_cast<wchar_t*>(node->pPath);
    pLaunchCmd->kvs = node->kvs;
    pLaunchCmd->domain = node->domain;
    pLaunchCmd->priorityClass = node->priority_class;

    pLaunchCmd->fSpawned = fSpawn;
    pLaunchCmd->parentPortName = node->parentPortName;

    //
    // Convert the launch block's linked list into an array
    //
    pLaunchCmd->rankCount = 0;
    for( smpd_rank_data_t* pRankNode = node->pRankData;
         pRankNode != nullptr;
         pRankNode = pRankNode->next )
    {
        pLaunchCmd->rankCount++;
    }

    pLaunchCmd->rankArray = new UINT16[pLaunchCmd->rankCount];
    if( pLaunchCmd->rankArray == nullptr )
    {
        smpd_err_printf(L"unable to populate launching command\n");
        delete pRes;
        delete pCmd;
        return rc;
    }

    unsigned int i = 0;
    for( smpd_rank_data_t* pRankNode = node->pRankData;
         pRankNode != nullptr;
         pRankNode = pRankNode->next )
    {
        pLaunchCmd->rankArray[i] = pRankNode->rank;
        pg->processes[pRankNode->rank].node_id = static_cast<INT16>(node->host_id);
        MPIU_Strcpy(
            pg->processes[pRankNode->rank].host,
            _countof(pg->processes[pRankNode->rank].host),
            node->hostname );

        i++;
    }

    pLaunchCmd->totalRankCount = (totalRankCount == 0) ? pLaunchCmd->rankCount : totalRankCount;
    pLaunchCmd->nproc = static_cast<UINT16>(pg->num_procs);

    UINT16* node_id_array = nullptr;
    FindNodeIdArray(node->kvs, &node_id_array);
    ASSERT(node_id_array != nullptr);
    pLaunchCmd->node_ids = node_id_array;

    rc = smpd_post_command(
        smpd_process.left_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if(rc != RPC_S_OK)
    {
        smpd_err_printf(
            L"unable to post launch command for block starting with :\n id: %d\n rank: %hu\n cmd: '%s'\n",
            node->host_id, node->pRankData[0].rank, node->pExe );
        delete [] pLaunchCmd->rankArray;
        delete pRes;
        delete pCmd;
        return rc;
    }

    node->launched = true;

    return NOERROR;
}


static smpd_process_group_t*
create_process_group(
    UINT16 nproc,
    const GUID& kvs
    )
{
    int i;
    smpd_process_group_t *pg;

    /* initialize a new process group structure */
    pg = static_cast<smpd_process_group_t*>(
        malloc( sizeof(smpd_process_group_t) + nproc * sizeof(smpd_exit_process_t) ) );
    if( pg == nullptr )
    {
        smpd_err_printf(L"unable to allocate memory for a process group structure.\n");
        return nullptr;
    }
    pg->aborted                   = FALSE;
    pg->any_init_received         = FALSE;
    pg->any_noinit_process_exited = FALSE;
    pg->kvs                       = kvs;
    pg->num_procs                 = nproc;
    pg->num_exited                = 0;
    pg->num_pending_suspends      = 0;
    pg->num_pending_kills         = 0;
    pg->num_results               = 0;
    pg->processes                 = (smpd_exit_process_t*)(pg + 1);

    for(i = 0; i < nproc; i++)
    {
        pg->processes[i].launched        = FALSE;
        pg->processes[i].failed_launch   = FALSE;
        pg->processes[i].init_called     = FALSE;
        pg->processes[i].finalize_called = FALSE;
        pg->processes[i].suspended       = FALSE;
        pg->processes[i].suspend_pending = FALSE;
        pg->processes[i].exited          = FALSE;
        pg->processes[i].node_id         = 0;
        pg->processes[i].exitcode        = 0;
        pg->processes[i].ctx_key         = 0;
        pg->processes[i].abortreason     = arTerminated;
        pg->processes[i].errmsg          = NULL;
        pg->processes[i].host[0]         = '\0';
    }

    return pg;
}


static
DWORD
mpiexec_compute_node_ids(
    GUID kvs
    )
{
    smpd_node_id_node_t* pNodeIdNode = new smpd_node_id_node_t;
    if (pNodeIdNode == nullptr)
    {
        smpd_err_printf(L"unable to allocate node_id_array node.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pNodeIdNode->nproc = smpd_process.nproc;
    pNodeIdNode->kvs = kvs;

    pNodeIdNode->node_id_array = new UINT16[smpd_process.nproc];
    if (pNodeIdNode->node_id_array == nullptr)
    {
        smpd_err_printf(L"unable to allocate node_id array.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    UINT16 r = 0;
    for( smpd_host_t* pHost = smpd_process.host_list;
         pHost != nullptr;
         pHost = static_cast<smpd_host_t*>(pHost->Next) )
    {
        for( smpd_rank_data_t* pNode = next_rank_data( nullptr, pHost );
             pNode != nullptr;
             pNode = next_rank_data( pNode, pHost ) )
        {
            pNodeIdNode->node_id_array[pNode->rank] = static_cast<UINT16>(pHost->HostId - 1);
            r++;
        }
    }
    
    //
    // Make sure we did not miss any rank
    //
    VERIFY(r == smpd_process.nproc);

    //
    // Add the created node to the list
    //
    pNodeIdNode->next = smpd_process.node_id_list;
    smpd_process.node_id_list = pNodeIdNode;

    return NOERROR;
}


static
UINT16
GetNumberOfRanksPerHost(
    smpd_host_t* pHost
    )
{
    UINT16 totalRankCount = 0;
    for (smpd_launch_block_t* pBlock = pHost->pLaunchBlockList;
        pBlock != nullptr;
        pBlock = pBlock->next)
    {
        if (pBlock->launched == false)
        {
            for (smpd_rank_data_t* pRankNode = pBlock->pRankData;
                pRankNode != nullptr;
                pRankNode = pRankNode->next)
            {
                totalRankCount++;
            }
        }
    }
    return totalRankCount;
}


static
DWORD
mpiexec_launch_processes(
    const GUID& kvs,
    const GUID& domain
    )
{
    int rc;
    smpd_process_group_t* pg;

    wchar_t kvs_name[GUID_STRING_LENGTH + 1];
    GuidToStr( kvs, kvs_name, _countof(kvs_name) );
    smpd_dbg_printf(L"creating a process group of size %hu on node %d called %s\n",
                    smpd_process.nproc,
                    smpd_process.tree_id,
                    kvs_name);

    pg = create_process_group( static_cast<UINT16>(smpd_process.nproc), kvs );
    if(pg == nullptr)
    {
        smpd_err_printf(L"unable to create a process group.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    /* add the process group to the global list */
    pg->next = smpd_process.pg_list;
    smpd_process.pg_list = pg;

    NotifyPmiDbgExtensions( MpiexecNotifyBeforeCreateProcesses,
                            smpd_process,
                            smpd_process.host_list );

    rc = mpiexec_compute_node_ids(kvs);
    if( rc != NOERROR )
    {
        return rc;
    }


    smpd_dbg_printf(L"launching the processes.\n");
    for( smpd_host_t* pHost = smpd_process.host_list;
         pHost != nullptr;
         pHost = static_cast<smpd_host_t*>(pHost->Next) )
    {
        UINT16 totalRankCount = GetNumberOfRanksPerHost(pHost);

        for (smpd_launch_block_t* pBlock = pHost->pLaunchBlockList;
             pBlock != nullptr;
             pBlock = pBlock->next )
        {
            pBlock->kvs = kvs;
            pBlock->domain = domain;
            rc = mpiexec_send_launch_command(pBlock, pg, false, totalRankCount);
            if( rc != NOERROR )
            {
                smpd_err_printf(L"unable to send launch command to host %s\n", pHost->name);
                return rc;
            }
        }
    }

    return NOERROR;
}


void
mpiexec_handle_start_dbs_result(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdStartDbsRes* pStartDbsRes = &pov->pRes->StartDbsRes;

    wchar_t kvs_name[GUID_STRING_LENGTH + 1];
    GuidToStr( pStartDbsRes->kvs, kvs_name, _countof(kvs_name) );

    smpd_process.domain = pStartDbsRes->domain;

    wchar_t domain_name[GUID_STRING_LENGTH + 1];
    GuidToStr( pStartDbsRes->domain,
               domain_name,
               _countof(domain_name) );

    smpd_dbg_printf(L"start_dbs succeeded, kvs_name: '%s', domain_name: '%s'\n",
                    kvs_name,
                    domain_name);

    DWORD rc = mpiexec_launch_processes( pStartDbsRes->kvs,
                                         smpd_process.domain );
    if( rc != RPC_S_OK )
    {
        smpd_post_abort_command(L"failed to launch processes. error %d\n", rc);
    }
}


void
mpiexec_handle_add_dbs_result(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdAddDbsCmd* pAddDbsCmd = &pov->pCmd->AddDbsCmd;

    wchar_t kvs_name[GUID_STRING_LENGTH + 1];
    GuidToStr(pAddDbsCmd->kvs, kvs_name, _countof(kvs_name));

    wchar_t domain_name[GUID_STRING_LENGTH + 1];
    GuidToStr(smpd_process.domain,
        domain_name,
        _countof(domain_name));

    smpd_dbg_printf(L"add_dbs succeeded, kvs_name: '%s', domain_name: '%s'\n",
        kvs_name,
        domain_name);

    smpd_process_group_t* pg;
    pg = create_process_group(pAddDbsCmd->nproc, pAddDbsCmd->kvs);
    if (pg == nullptr)
    {
        smpd_post_abort_command(L"unable to create a process group.\n");
        return;
    }

    //
    // add the process group to the global list 
    //
    pg->next = smpd_process.pg_list;
    smpd_process.pg_list = pg;

    for (smpd_host_t* pHost = smpd_process.host_list;
        pHost != nullptr;
        pHost = static_cast<smpd_host_t*>(pHost->Next))
    {
        UINT16 totalRankCount = GetNumberOfRanksPerHost(pHost);

        for (smpd_launch_block_t* pBlock = pHost->pLaunchBlockList;
            pBlock != nullptr;
            pBlock = pBlock->next)
        {
            if (pBlock->launched == false)
            {
                DWORD rc = mpiexec_send_launch_command(pBlock, pg, true, totalRankCount);
                if (rc != NOERROR)
                {
                    smpd_post_abort_command(L"unable to send launch command to host %s: %d.\n", pHost->name, rc);
                    return;
                }
            }
        }
    }

    smpd_process.nproc += pAddDbsCmd->nproc;
}


DWORD
mpiexec_send_start_dbs_command(
    smpd_context_t* pContext
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_STARTDBS,
        SMPD_IS_ROOT,
        SMPD_IS_TOP );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create a start_dbs command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        &mpiexec_handle_start_dbs_result );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for start_dbs command\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD rc = smpd_post_command(
        pContext,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"Unable to post start_dbs command error %u\n", rc);
        return rc;
    }

    return NOERROR;
}


DWORD
mpiexec_send_add_dbs_command(
    UINT16 nproc,
    smpd_context_t* pContext
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_ADDDBS,
        SMPD_IS_ROOT,
        SMPD_IS_TOP);
    if (pCmd == nullptr)
    {
        smpd_err_printf(L"unable to create a add_dbs command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    pCmd->AddDbsCmd.nproc = nproc;

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        &mpiexec_handle_add_dbs_result);
    if (pRes == nullptr)
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for add_dbs command\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD rc = smpd_post_command(
        pContext,
        pCmd,
        &pRes->Res,
        nullptr);
    if (rc != RPC_S_OK)
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"Unable to post add_dbs command error %u\n", rc);
        return rc;
    }

    return NOERROR;
}


DWORD
mpiexec_abort_job(
    smpd_process_group_t* pg,
    UINT16                rank,
    abort_reason_t        abort_reason,
    int                   exit_code,
    const wchar_t*        err_msg
    )
{
    ASSERT(pg != nullptr);
    ASSERT(rank < pg->num_procs);

    /* save the abort message. Okay if strdup fails, errmsg will not be ref'd */

    if( pg->processes[rank].abortreason == arTerminated )
    {
        pg->processes[rank].abortreason = abort_reason;
    }

    pg->processes[rank].exitcode = exit_code;
    pg->processes[rank].errmsg = err_msg;
    if(pg->aborted)
    {
        smpd_dbg_printf(L"job already aborted.\n");
        return NOERROR;
    }

    if(abort_reason == arAppAbort)
    {
        smpd_process.mpiexec_exit_code = exit_code;
    }
    else
    {
        smpd_process.mpiexec_exit_code = -(static_cast<int>(abort_reason));
    }

    pg->aborted = TRUE;
    DWORD rc = NOERROR;
    for(UINT i = 0; i < pg->num_procs; i++)
    {
        if(pg->processes[i].exited || pg->processes[i].failed_launch)
        {
            continue;
        }

        if(!pg->processes[i].launched)
        {
            pg->processes[i].suspend_pending = TRUE;
            pg->num_pending_suspends++;
            continue;
        }

        rc = mpiexec_send_suspend_command(
            pg->kvs,
            static_cast<UINT16>(i),
            &pg->processes[i]);
        if( rc != NOERROR )
        {
            smpd_err_printf(
                L"unable to send suspend command for rank %hu\n",
                rank );
            continue;
        }

        pg->num_pending_suspends++;
    }

    if( rc != NOERROR && pg->num_pending_suspends == 0 )
    {
        return rc;
    }

    if( pg->num_pending_suspends == 0 )
    {
        mpiexec_tear_down_tree();
    }

    return NOERROR;
}


void mpiexec_tear_down_tree()
{
    if(smpd_process.closing)
    {
        return;
    }

    smpd_process.closing = TRUE;

    DWORD rc = smpd_send_close_command(smpd_process.left_context, 1);
    if(rc != NOERROR)
    {
        smpd_err_printf(L"unable to tear down the job tree. exiting...\n");
        exit(-1);
    }
}


DWORD
mpiexec_handle_abort_command(
    smpd_overlapped_t* pov
    )
{
    const SmpdAbortCmd* pAbortCmd = &pov->pCmd->AbortCmd;
    if( pAbortCmd->error_msg[0] == L'\0')
    {
        smpd_post_abort_command(L"unexpected error\n");
    }
    else
    {
        smpd_post_abort_command( L"%s", pAbortCmd->error_msg );
    }

    return NOERROR;
}


DWORD
mpiexec_handle_abort_job_command(
    smpd_overlapped_t* pov
    )
{
    const SmpdAbortJobCmd* pAbortJobCmd = &pov->pCmd->AbortJobCmd;

    smpd_process_group_t* pg = find_pg( pAbortJobCmd->kvs );
    VERIFY( pg != nullptr );

    abort_reason_t abort_reason = pAbortJobCmd->intern ? arFatalError : arAppAbort;
    DWORD rc = mpiexec_abort_job(
        pg,
        pAbortJobCmd->rank,
        abort_reason,
        pAbortJobCmd->exit_code,
        MPIU_Strdup( pAbortJobCmd->error_msg ));

    if( rc != NOERROR )
    {
        smpd_post_abort_command(L"failed to abort the job. Rank %u reported %s\n",
                                pAbortJobCmd->rank,
                                pAbortJobCmd->error_msg );
    }

    return rc;
}


//
// Summary:
//  Handler for the "INIT" command from MPI processes
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
mpiexec_handle_init_command(
    smpd_overlapped_t* pov
    )
{
    const SmpdInitCmd*    pInitCmd = &pov->pCmd->InitCmd;
    smpd_process_group_t* pg       = find_pg( pInitCmd->kvs );
    UINT16                ctx_key  = pInitCmd->header.ctx_key;
    UINT16                rank     = pInitCmd->rank;
    UINT16                size     = pInitCmd->size;
    UINT16                node_id  = pInitCmd->node_id;

    wchar_t name[GUID_STRING_LENGTH + 1];
    GuidToStr( pInitCmd->kvs, name, _countof(name) );
    smpd_dbg_printf(L"init: %hu:%hu:%s\n", rank, size, name);

    VERIFY( pg != nullptr );
    VERIFY( size == pg->num_procs );
    VERIFY( rank < size );

    pg->any_init_received = TRUE;
    pg->processes[rank].init_called = TRUE;
    pg->processes[rank].node_id = node_id;
    pg->processes[rank].ctx_key = ctx_key;

    if(pg->any_noinit_process_exited)
    {
        return ERROR_INVALID_DATA;
    }

    return NOERROR;
}


static DWORD
mpiexec_parse_env_info(
    _Inout_ smpd_env_node_t** list,
    _Inout_ wchar_t* envlist
)
{
    const smpd_env_node_t* env_node;

    const wchar_t* token;
    wchar_t* next_token = nullptr;

    const wchar_t* name = nullptr;
    const wchar_t* value = nullptr;

    token = wcstok_s(envlist, L";,=", &next_token);
    UINT16 curr = 0;
    while (token)
    {
        if (curr % 2)
        {
            value = token;
            env_node = add_new_env_node(list, name, value);
            if (env_node == nullptr)
            {
                return ERROR_NOT_ENOUGH_MEMORY;
            }
        }
        else
        {
            name = token;
        }
        curr++;
        token = wcstok_s(nullptr, L";,=", &next_token);
    }
    if (curr % 2)
    {
        return ERROR_INVALID_PARAMETER;
    }
    return NOERROR;
}


static inline bool
IsNonEmptyString(
    _In_opt_z_ const wchar_t* str
    )
{
    return (str != nullptr && str[0] != L'\0');
}


DWORD
mpiexec_handle_spawn_command(
    smpd_overlapped_t* pov
    )
{
    DWORD gle;
    SmpdSpawnCmd* pSpawnCmd = &pov->pCmd->SpawnCmd;
    UINT16 numproc = pSpawnCmd->rankCount;

    wchar_t* appExe = nullptr;
    wchar_t * appArgv = nullptr;
    wchar_t* path = nullptr;
    wchar_t* wDir = nullptr;
    char* parentPortName = nullptr;
    wchar_t** envBlock = nullptr;
    smpd_env_node_t* envList = nullptr;
    mp_host_pool_t* pool = nullptr;

    UINT16* node_id_array = nullptr;
    FindNodeIdArray(pSpawnCmd->kvs, &node_id_array);
    if (node_id_array == nullptr)
    {
        smpd_node_id_node_t* pNodeIdNode = new smpd_node_id_node_t;
        if (pNodeIdNode == nullptr)
        {
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        pNodeIdNode->nproc = pSpawnCmd->totalRankCount;
        pNodeIdNode->kvs = pSpawnCmd->kvs;
        pNodeIdNode->node_id_array = new UINT16[pNodeIdNode->nproc];
        if (pNodeIdNode->node_id_array == nullptr)
        {
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        pNodeIdNode->next = smpd_process.node_id_list;
        smpd_process.node_id_list = pNodeIdNode;

        node_id_array = pNodeIdNode->node_id_array;
    }

    //
    // calculate the number of hosts
    //
    UINT16 numHosts = 0;
    for (smpd_host_t* pHost = smpd_process.host_list;
        pHost != nullptr;
        pHost = static_cast<smpd_host_t*>(pHost->Next))
    {
        numHosts++;
    }

    UINT16 rank = pSpawnCmd->startRank;

    appExe = new wchar_t[UNICODE_STRING_MAX_CHARS];
    if (appExe == nullptr)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    MPIU_Strcpy(appExe, UNICODE_STRING_MAX_CHARS, pSpawnCmd->appExe);

    appArgv = new wchar_t[UNICODE_STRING_MAX_CHARS];
    if (appArgv == nullptr)
    {
        gle = ERROR_NOT_ENOUGH_MEMORY;
        goto CleanUp;
    }
    MPIU_Snprintf(appArgv, UNICODE_STRING_MAX_CHARS, L"%s %s", appExe, pSpawnCmd->appArgv);

    if (IsNonEmptyString(pSpawnCmd->wDir))
    {
        wDir = new wchar_t[MAX_PATH];
        if (wDir == nullptr)
        {
            gle =  ERROR_NOT_ENOUGH_MEMORY;
            goto CleanUp;
        }
        MPIU_Strcpy(wDir, MAX_PATH, pSpawnCmd->wDir);
    }

    if (IsNonEmptyString(pSpawnCmd->path))
    {
        path = new wchar_t[MAX_PATH];
        if (path == nullptr)
        {
            gle = ERROR_NOT_ENOUGH_MEMORY;
            goto CleanUp;
        }
        MPIU_Strcpy(path, MAX_PATH, pSpawnCmd->path);
    }

    parentPortName = new char[UNICODE_STRING_MAX_CHARS];
    if (parentPortName == nullptr)
    {
        gle = ERROR_NOT_ENOUGH_MEMORY;
        goto CleanUp;
    }
    MPIU_Strcpy(parentPortName, UNICODE_STRING_MAX_CHARS, pSpawnCmd->parentPortName);

    //
    // process the list of environment variables
    // environment variables are passed to Spawn in the folowing format:
    // "name1=value1,name2=value2,..."
    //
    UINT16 envCount = 0;
    if (IsNonEmptyString(pSpawnCmd->envList))
    {
        gle = mpiexec_parse_env_info(&envList, pSpawnCmd->envList);
        if (gle == ERROR_INVALID_PARAMETER)
        {
            smpd_dbg_printf(L"Warning: unable to parse environment variables passed as INFO argument for MPI_Comm_spawn. Expected format is: env1=value1,env2=value2,env3=value3.\n");
            gle = NOERROR;
        }
        else if (gle == ERROR_NOT_ENOUGH_MEMORY)
        {
            smpd_err_printf(L"Not enough memory to parse environment variables\n");
            goto CleanUp;
        }
        else
        {
            smpd_env_node_t* pEnvNode;
            for (pEnvNode = envList; pEnvNode != nullptr; pEnvNode = pEnvNode->next)
            {
                envCount++;
            }

            if (envCount != 0)
            {
                //
                // Need two strings per environment variable
                // One for the name of the env var, one for the value
                //
                envBlock = new wchar_t*[envCount * 2];
                if (envBlock == nullptr)
                {
                    smpd_err_printf(L"insufficient memory for environment variables\n");
                    gle = ERROR_NOT_ENOUGH_MEMORY;
                    goto CleanUp;
                }
            }

            size_t envIndex = 0;
            pEnvNode = envList;
            while (pEnvNode != nullptr)
            {
                envBlock[envIndex++] = pEnvNode->name;
                envBlock[envIndex++] = pEnvNode->value;
                pEnvNode = pEnvNode->next;

            }
            ASSERT(envIndex == static_cast<size_t>(envCount * 2));
        }
    }

    //
    // If the list of hosts to start the spawned processes is
    // not provided as part of INFO argument, we will distribute
    // the new processes evenly across available hosts.
    // When the list of hosts is provided with INFO we would
    // parse the list of the hosts, match up with the smpd host
    // list and ignore the ones that we don't see.
    //
    UINT16 processesToStart = numproc;
    UINT16 processesPerHost = (processesToStart + numHosts - 1) / numHosts;
    UINT16 startedProcesses = 0;
    bool fHostsPassed = IsNonEmptyString(pSpawnCmd->hosts);

    //
    // parse the host list passed as part of INFO argument
    //
    if (fHostsPassed)
    {
        pool = new mp_host_pool_t();
        const wchar_t* error = mp_parse_hosts_string(pSpawnCmd->hosts, pool);
        if (error != nullptr)
        {
            smpd_dbg_printf(L"Warning: Unable to parse host list '%s': %s. Host list ignored.\n", pSpawnCmd->hosts, error);
            fHostsPassed = false;
        }
    }

    for (smpd_host_t* pHost = smpd_process.host_list;
        pHost != nullptr;
        pHost = static_cast<smpd_host_t*>(pHost->Next))
    {
        //
        // see if we want to start any processes on this host
        //
        if (fHostsPassed)
        {
            mp_host_t* pRequestedHost;
            for (pRequestedHost = pool->hosts;
                pRequestedHost != nullptr;
                pRequestedHost = static_cast<mp_host_t*>(pRequestedHost->Next))
            {
                if (_wcsicmp(pHost->name, pRequestedHost->name) == 0)
                {
                    break;
                }
            }
            if (pRequestedHost != nullptr)
            {
                processesPerHost = static_cast<UINT16>(pRequestedHost->nproc);
            }
            else
            {
                //
                // go to the next host
                //
                processesPerHost = 0;
            }
        }
        else
        {
            if (processesToStart >= processesPerHost)
            {
                processesToStart -= processesPerHost;
            }
            else
            {
                processesPerHost = processesToStart;
            }
        }

        if (processesPerHost == 0)
        {
            continue;
        }

        smpd_launch_block_t* pLaunchBlock = new smpd_launch_block_t;
        if (pLaunchBlock == nullptr)
        {
            smpd_err_printf(L"unable to allocate launching block\n");
            gle = ERROR_NOT_ENOUGH_MEMORY;
            goto CleanUp;
        }

        pLaunchBlock->host_id = pHost->HostId;
        MPIU_Strcpy(
            pLaunchBlock->hostname,
            _countof(pLaunchBlock->hostname),
            pHost->name);

        pLaunchBlock->appnum = pSpawnCmd->appNum;
        pLaunchBlock->priority_class = 0;

        pLaunchBlock->envCount = envCount*2;
        pLaunchBlock->ppEnv = envBlock;

        pLaunchBlock->pPath = (path != nullptr) ? path : L"";
        pLaunchBlock->pDir = (wDir != nullptr) ? wDir : L"";

        pLaunchBlock->kvs = pSpawnCmd->kvs;
        pLaunchBlock->domain = smpd_process.domain;

        pLaunchBlock->pExe = appExe;
        pLaunchBlock->pArgs = appArgv;
        pLaunchBlock->parentPortName = parentPortName;

        //
        // Insert this launch block to the host's list of
        // launch blocks
        //
        pLaunchBlock->next = pHost->pLaunchBlockList;
        pHost->pLaunchBlockList = pLaunchBlock;

        for (UINT16 q = 0; q < processesPerHost; q++)
        {
            smpd_rank_data_t* pRank = new smpd_rank_data_t;
            if (pRank == nullptr)
            {
                smpd_err_printf(L"unable to allocate a rank data structure.\n");
                gle = ERROR_NOT_ENOUGH_MEMORY;
                goto CleanUp;
            }

            node_id_array[rank] = static_cast<UINT16>(pHost->HostId - 1);

            pRank->parent = pLaunchBlock;
            pRank->rank = static_cast<UINT16>(rank++);
            pRank->nodeProcOrder = pHost->nodeProcCount++;

            //
            // Insert this rank's data to the launch's block rank data
            // list
            //
            pRank->next = pLaunchBlock->pRankData;
            pLaunchBlock->pRankData = pRank;
        }
        startedProcesses += processesPerHost;
    }

    if (startedProcesses != pSpawnCmd->rankCount)
    {
        gle = ERROR_INVALID_PARAMETER;
        smpd_err_printf(
            L"Error: the number of processes passed as maxprocs argument of MPI_Comm_spawn %u is not equal to the number of processes on valid hosts passed in hosts INFO node %u\n",
            pSpawnCmd->rankCount,
            startedProcesses
            );
        goto CleanUp;
    }

    if (pSpawnCmd->done)
    {
        SmpdCmd* pCmd = smpd_create_command(
            SMPD_ADDDBS,
            SMPD_IS_ROOT,
            SMPD_IS_TOP);
        if (pCmd == nullptr)
        {
            smpd_err_printf(L"unable to create a add_dbs command.\n");
            gle = ERROR_NOT_ENOUGH_MEMORY;
            goto CleanUp;
        }

        SmpdAddDbsCmd* pAddDbsCmd = &pCmd->AddDbsCmd;
        pAddDbsCmd->nproc = rank;
        pAddDbsCmd->kvs = pSpawnCmd->kvs;

        SmpdResWrapper* pRes = smpd_create_result_command(
            smpd_process.tree_id,
            &mpiexec_handle_add_dbs_result);
        if (pRes == nullptr)
        {
            delete pCmd;
            smpd_err_printf(L"unable to create result for add_dbs command\n");
            gle = ERROR_NOT_ENOUGH_MEMORY;
            goto CleanUp;
        }

        DWORD rc = smpd_post_command(
            smpd_process.left_context,
            pCmd,
            &pRes->Res,
            nullptr);
        if (rc != RPC_S_OK)
        {
            delete pRes;
            delete pCmd;
            smpd_err_printf(L"Unable to post add_dbs command error %u\n", rc);
            gle = rc;
            goto CleanUp;
        }
    }

    while (nullptr != pool && nullptr != pool->hosts)
    {
        mp_host_t* p = static_cast<mp_host_t*>(pool->hosts->Next);
        delete pool->hosts;
        pool->hosts = p;
    }
    delete pool;

    return NOERROR;

CleanUp:
    delete[] wDir;
    delete[] appExe;
    delete[] appArgv;
    delete[] parentPortName;
    delete[] path;

    while (envList != nullptr)
    {
        smpd_env_node_t* pDelete = envList;
        envList = envList->next;
        delete pDelete;
    }
    delete[] envBlock;

    while (nullptr != pool && nullptr != pool->hosts)
    {
        mp_host_t* p = static_cast<mp_host_t*>(pool->hosts->Next);
        delete pool->hosts;
        pool->hosts = p;
    }
    delete pool;

    return gle;
}

//
// Summary:
//  Handler for the "FINALIZE" command from MPI processes
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
mpiexec_handle_finalize_command(
    _In_ smpd_overlapped_t* pov
    )
{
    const SmpdFinalizeCmd* pFinalizeCmd = &pov->pCmd->FinalizeCmd;

    wchar_t name[GUID_STRING_LENGTH + 1];
    GuidToStr( pFinalizeCmd->kvs, name, _countof(name) );

    UINT16 rank = pFinalizeCmd->rank;
    smpd_dbg_printf(L"finalize: %hu:%s\n", pFinalizeCmd->rank, name);

    smpd_process_group_t* pg = find_pg( pFinalizeCmd->kvs );
    VERIFY(pg != nullptr);
    VERIFY(rank < pg->num_procs);

    pg->processes[rank].finalize_called = TRUE;

    return NOERROR;
}


static const wchar_t* get_exit_code_string(int errcode)
{
    static wchar_t text[128];

    /* check for 0x8xxxxxxx and 0xCxxxxxxx error codes range */
    if(((errcode & 0xC0000000) != 0) && ((errcode & 0x30000000) == 0))
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Failure improbable, no good recovery anyway");
        StringCchPrintfW(text, _countof(text), L"%#x", errcode);
    }
    else
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Failure improbable, no good recovery anyway");
        StringCchPrintfW(text, _countof(text), L"%d", errcode);
    }

    return text;
}


static smpd_launch_block_t*
find_launch_block(
    UINT16 rank,
    GUID kvs
    )
{
    for( smpd_host_t* pHost = smpd_process.host_list;
         pHost != nullptr;
         pHost = static_cast<smpd_host_t*>(pHost->Next) )
    {
        for( smpd_rank_data_t* pRankNode = next_rank_data( nullptr, pHost );
             pRankNode != nullptr;
             pRankNode = next_rank_data( pRankNode, pHost ) )
        {
            if ((pRankNode->rank == rank) && (IsEqualGUID(pRankNode->parent->kvs, kvs)))
            {
                return pRankNode->parent;
            }
        }

    }
    return nullptr;
}


static inline const wchar_t*
get_rank_exe_name(
    UINT16 rank,
    GUID kvs
    )
{
    const smpd_launch_block_t* node = find_launch_block( rank, kvs );
    if( node != nullptr )
    {
        return node->pExe;
    }

    return L"process";
}


const wchar_t* const g_abort_reason_format[] = {
    L"",
    L"ctrl-c was hit. job aborted by the user.\n",
    L"%s ended before calling init and may have crashed. exit code %s\n",
    L"%s ended prematurely and may have crashed. exit code %s\n",
    L"mpi has detected a fatal error and aborted %s\n",
    L"%s aborted the job. abort code %s\n",
};


static inline const wchar_t* get_abort_code_format(abort_reason_t abort_reason)
{
    return g_abort_reason_format[abort_reason];
}


static void print_analysis_tail(int first, int last, const smpd_exit_process_t* p, const wchar_t* exe)
{
    if(last != first)
    {
        wprintf(L"-%d", last);
    }

    wprintf(L"] on %s\n", p->host);
    OACR_REVIEWED_CALL(mpicr, wprintf(get_abort_code_format(p->abortreason), exe, get_exit_code_string(p->exitcode)));
}


static BOOL analyze_abort_code(const smpd_process_group_t* pg, abort_reason_t abort_reason)
{
    BOOL reported = FALSE;
    int first = -1;
    int last = 0;
    const wchar_t* last_exe = NULL;

    for(int i = 0; i < pg->num_procs; i++)
    {
        const smpd_exit_process_t* p = &pg->processes[i];
        if(p->abortreason == abort_reason)
        {
            const wchar_t* exe = get_rank_exe_name( static_cast<UINT16>(i), pg->kvs);
            if(first == -1)
            {
                first = i;
                wprintf(L"\n[%d", first);
            }
            else
            {
                const smpd_exit_process_t* lp = &pg->processes[last];
                if( p->exitcode != lp->exitcode ||
                    CompareStringW( LOCALE_INVARIANT,
                                    0,
                                    p->host,
                                    -1,
                                    lp->host,
                                    -1 ) != CSTR_EQUAL ||
                    CompareStringW( LOCALE_INVARIANT,
                                    0,
                                    last_exe,
                                    -1,
                                    exe,
                                    -1 ) != CSTR_EQUAL )
                {
                    print_analysis_tail(first, last, lp, last_exe);

                    first = i;
                    wprintf(L"\n[%d", first);
                }
                else if(i != last + 1)
                {
                    if(first != last)
                    {
                       wprintf(L"-%d", last);
                    }
                    wprintf(L",%d", i);
                    first = i;
                }
            }

            last = i;
            last_exe = exe;
            reported = TRUE;
        }
    }

    if(reported)
    {
        print_analysis_tail(first, last, &pg->processes[last], last_exe);
    }

    return reported;
}


static inline BOOL analyze_mpi_aborted(const smpd_process_group_t* pg)
{
    return analyze_abort_code(pg, arFatalError);
}


static inline BOOL analyze_app_crash(const smpd_process_group_t* pg)
{
    BOOL reported = analyze_abort_code(pg, arExitNoInit);
    reported |= analyze_abort_code(pg, arExitNoFinalize);
    return reported;
}


static inline BOOL analyze_ctrl_c(const smpd_process_group_t* pg)
{
    return analyze_abort_code(pg, arCtrlC);
}


static inline BOOL analyze_app_aborted(const smpd_process_group_t* pg)
{
    return analyze_abort_code(pg, arAppAbort);
}


static void print_error_analysis(const smpd_process_group_t* pg)
{
    if(analyze_ctrl_c(pg))
        return;

    if(analyze_app_crash(pg))
        return;

    if(analyze_mpi_aborted(pg))
        return;

    analyze_app_aborted(pg);
}


const wchar_t* const g_exit_code_text[] = {
    L"terminated",
    L"job terminated by the user",
    L"process exited without calling init",
    L"process exited without calling finalize",
    L"fatal error",
    L"application aborted",
};


static const wchar_t* get_err_code_string(const smpd_exit_process_t* p)
{
    if(p->abortreason != arTerminated || p->exitcode == 0)
        return g_exit_code_text[p->abortreason];

    return get_exit_code_string(p->exitcode);
}



static void
print_aborted_message(
    const smpd_exit_process_t* first,
    const smpd_exit_process_t* last,
    const smpd_process_group_t* pg
    )
{
    UINT16 f = static_cast<UINT16>(first - &pg->processes[0]);
    UINT16 l = static_cast<UINT16>(last  - &pg->processes[0]);
    if(f == l)
    {
        wprintf(L"\n[%hu] %s\n", f, get_err_code_string(first));
    }
    else
    {
        wprintf(L"\n[%hu-%hu] %s\n", f, l, get_err_code_string(first));
    }

    if(first->errmsg != NULL)
    {
        wprintf(L"%s\n", first->errmsg);
    }
}


static inline bool equal_error(const smpd_exit_process_t* p1, const smpd_exit_process_t* p2)
{
    if(p1->abortreason!= p2->abortreason)
        return false;

    if(p1->exitcode != p2->exitcode)
        return false;

    if(p1->errmsg == NULL)
    {
        if(p2->errmsg == NULL)
            return true;

        return false;
    }

    if(p2->errmsg == NULL)
        return false;

    return CompareStringW( LOCALE_INVARIANT,
                           0,
                           p1->errmsg,
                           -1,
                           p2->errmsg,
                           -1 ) == CSTR_EQUAL;

}


static void print_aborted_messages(const smpd_process_group_t* pg)
{
    wprintf(L"\njob aborted:\n");
    wprintf(L"[ranks] message\n");
    const smpd_exit_process_t* first = &pg->processes[0];
    for(int i = 0; i < pg->num_procs; i++)
    {
        const smpd_exit_process_t* p = &pg->processes[i];

        if(equal_error(p, first))
            continue;

        print_aborted_message(first, p - 1 , pg);
        first = p;
    }

    print_aborted_message(first, &pg->processes[pg->num_procs - 1] , pg);

    wprintf(L"\n---- error analysis -----\n");
    print_error_analysis(pg);
    wprintf(L"\n---- error analysis -----\n");
    fflush(stdout);
}


static void print_exit_codes(const smpd_process_group_t* pg)
{
    wprintf(L"\n---- exit code ----\n\n");
    for(int i = 0; i < pg->num_procs; i++)
    {
        wprintf(L"[%d] on %s: %s\n", i, pg->processes[i].host, get_exit_code_string(pg->processes[i].exitcode));
    }
    wprintf(L"\n---- exit code ----\n");
    fflush(stdout);
}


static void print_exit_messages(const smpd_process_group_t* pg)
{
    if(pg->aborted)
    {
        print_aborted_messages(pg);
    }
    else if(smpd_process.output_exit_codes)
    {
        print_exit_codes(pg);
    }
}


DWORD
mpiexec_handle_exit_command(
    _In_ smpd_overlapped_t* pov
    )
{
    SmpdExitCmd*   pExitCmd = &pov->pCmd->ExitCmd;
    UINT16         rank     = pExitCmd->rank;
    int            exitcode = pExitCmd->exit_code;

    smpd_process_group_t* pg = find_pg( pExitCmd->kvs );
    VERIFY( pg != nullptr );
    VERIFY( rank < pg->num_procs );

    smpd_exit_process_t* p = &pg->processes[rank];
    VERIFY(!p->exited);

    p->exited = TRUE;
    pg->num_exited++;

    if(!p->suspended)
    {
        if(p->abortreason == arTerminated)
        {
            /* rank was not aborted, it is okay to set the exit code
             * (not overwriting the abort-time exit code) */
            wchar_t name[GUID_STRING_LENGTH + 1];
            GuidToStr( pExitCmd->kvs, name, _countof(name) );

            smpd_dbg_printf(L"saving exit code: rank %hu, exitcode %d, pg <%s>\n",
                            rank, exitcode, name);
            p->exitcode = exitcode;

            if(smpd_process.mpiexec_exit_code == 0)
            {
                /* mpiexec will return the first non-zero exit code
                 * returned by a process. */
                smpd_process.mpiexec_exit_code = exitcode;
            }
        }

        if(p->init_called && !p->finalize_called)
        {
            /* process exited after init but before finalize */
            mpiexec_abort_job(pg, rank, arExitNoFinalize, exitcode, NULL);
        }

        if(!p->init_called)
        {
            smpd_dbg_printf(L"process exited without calling init.\n");
            /* this process never called init or finalize, check to
             * make sure no other process has called init */
            if(pg->any_init_received)
            {
                mpiexec_abort_job(pg, rank, arExitNoInit, exitcode, NULL);
            }
            else
            {
                pg->any_noinit_process_exited = TRUE;
                smpd_dbg_printf(L"process exited before anyone has called init.\n");
            }
        }
    }

    if(pg->num_exited == pg->num_procs)
    {
        print_exit_messages(pg);
    }

    smpd_process.nproc_exited++;
    if(smpd_process.nproc == smpd_process.nproc_exited)
    {
        smpd_dbg_printf(L"last process exited, tearing down the job tree num_exited=%d num_procs=%hu.\n",
                        pg->num_exited, pg->num_procs);
        mpiexec_tear_down_tree();
    }
    return NOERROR;
}
