// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"

static smpd_process_t* g_process_list;


void
smpd_free_process_struct(
    _In_ _Post_ptr_invalid_ smpd_process_t* process
    )
{
    if( process->hStdout != INVALID_HANDLE_VALUE )
    {
        CloseHandle( process->hStdout );
    }
    if( process->hStderr != INVALID_HANDLE_VALUE )
    {
        CloseHandle( process->hStderr );
    }
    if( process->hStdin != INVALID_HANDLE_VALUE )
    {
        CloseHandle( process->hStdin );
    }

    delete process;
}


smpd_process_t*
smpd_create_process_struct(
    _In_ UINT16 rank,
    _In_ GUID kvs
    )
{
    static UINT16 cur_id = 0;
    smpd_process_t *p;

    p = new smpd_process_t;
    if(p == nullptr )
    {
        return nullptr;
    }

    p->next = nullptr;
    p->id = cur_id++;
    p->rank = rank;
    p->kvs = kvs;
    ZeroMemory(&(p->launch_affinity), sizeof(HWAFFINITY));
    p->stdin_write_queue.m_head = nullptr;

    p->in = nullptr;
    p->isRundown = 0;
    p->context_refcount = 0;
    p->exitcode = 0;
    p->dump_type = (MINIDUMP_TYPE)-1;

    p->hStdout = INVALID_HANDLE_VALUE;
    p->hStderr = INVALID_HANDLE_VALUE;
    p->hStdin = INVALID_HANDLE_VALUE;
    return p;
}


void
smpd_add_process(
    _Inout_ smpd_process_t* process
    )
{
    process->next = g_process_list;
    g_process_list = process;
}


void
smpd_remove_process(
    smpd_process_t* process
    )
{
    smpd_process_t** ppp = &g_process_list;
    for( ; *ppp != nullptr; ppp = &(*ppp)->next)
    {
        if(*ppp == process)
        {
            *ppp = process->next;
            return ;
        }
    }

    ASSERT(*ppp == process);
}


smpd_process_t*
smpd_get_first_process()
{
    return g_process_list;
}


smpd_process_t*
smpd_find_process_by_id(
    UINT16 id
    )
{
    smpd_process_t* proc = g_process_list;
    for( ; proc != nullptr; proc = proc->next)
    {
        if(proc->id == id)
        {
            return proc;
        }
    }

    return nullptr;
}


smpd_process_t*
smpd_find_process_by_rank(
    UINT16 rank
    )
{
    smpd_process_t* proc = g_process_list;
    for( ; proc != nullptr; proc = proc->next)
    {
        if(proc->rank == rank)
        {
            return proc;
        }
    }

    return nullptr;
}


static bool smpd_wait_process(smpd_pwait_t wait, int *exit_code_ptr)
{
    DWORD exit_code;

    if(wait.hProcess == INVALID_HANDLE_VALUE || wait.hProcess == NULL)
    {
        smpd_dbg_printf(L"No process to wait for.\n");
        *exit_code_ptr = -1;
        return true;
    }
    if(WaitForSingleObject(wait.hProcess, INFINITE) != WAIT_OBJECT_0)
    {
        smpd_err_printf(L"WaitForSingleObject failed, error %u\n", GetLastError());
        *exit_code_ptr = -1;
        return false;
    }

    BOOL fSucc = GetExitCodeProcess(wait.hProcess, &exit_code);
    if(!fSucc)
    {
        smpd_err_printf(L"GetExitCodeProcess failed, error %u\n", GetLastError());
        *exit_code_ptr = -1;
        return false;
    }
    CloseHandle(wait.hProcess);
    CloseHandle(wait.hThread);

    *exit_code_ptr = exit_code;

    return true;
}


static DWORD
smpd_send_exit_command(
    const smpd_process_t* proc)
{
    if( smpd_process.parent_closed )
    {
        smpd_dbg_printf(L"parent already closed, not sending exit command\n");
        return NOERROR;
    }

    SmpdCmd* pCmd = smpd_create_command(
        SMPD_EXIT,
        smpd_process.tree_id,
        SMPD_IS_ROOT );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create an exit command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        nullptr );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for exit command\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdExitCmd* pExitCmd = &pCmd->ExitCmd;
    pExitCmd->rank = static_cast<UINT16>(proc->rank);
    pExitCmd->exit_code = proc->exitcode;

    pCmd->ExitCmd.kvs = proc->kvs;

    smpd_dbg_printf(L"creating an exit command for process id=%hu  rank=%hu, pid=%d, exit code=%d.\n",
                    proc->id, proc->rank, proc->pid, proc->exitcode);

    DWORD rc = smpd_post_command(
        smpd_process.parent_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        smpd_err_printf(L"unable to post exit command error %u\n", rc );
        return rc;
    }

    return NOERROR;
}


void
smpd_close_stdin(
    _Inout_ smpd_process_t* proc
    )
{
    smpd_context_t* context = proc->in;
    proc->in = NULL;
    CloseHandle(context->pipe);
    if(proc->stdin_write_queue.head() == NULL)
    {
        smpd_free_context(context);
    }
}


int
smpd_rundown_process(
    smpd_process_t* proc
    )
{
    if( InterlockedExchange( &proc->isRundown, 1 ) != 0 )
    {
        //
        // Ensure that we only rundown the process once
        //
        return MPI_SUCCESS;
    }

    smpd_dbg_printf(
        L"process_id=%hu rank=%hu refcount=%u, waiting for the process to finish exiting.\n",
        proc->id,
        proc->rank,
        proc->context_refcount);

    if(!smpd_wait_process(proc->wait, &proc->exitcode))
    {
        smpd_err_printf(L"unable to wait for the process to exit: rank=%hu, pid=%d\n",
                        proc->rank, proc->pid);

        //
        // Fall through and continue the rundown
        //
    }

    /* remove the process structure from the global list */
    smpd_remove_process(proc);
    if(proc->in != nullptr)
    {
        smpd_close_stdin(proc);
    }

    DWORD rc = smpd_send_exit_command(proc);
    if(rc != NOERROR)
    {
        return rc;
    }

    /* free the context */
    smpd_free_process_struct(proc);
    return MPI_SUCCESS;
}
