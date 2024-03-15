// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"


void MPIDI_CH3I_BootstrapQ_create(MPIDI_CH3I_BootstrapQ* queue_ptr)
{
    *queue_ptr = MPIDI_CH3I_set;
    return;
}


void MPIDI_CH3I_BootstrapQ_tostring(
    _In_ MPIDI_CH3I_BootstrapQ queue,
    _Out_writes_z_(length) char *name,
    _In_range_(>, 20) size_t length
    )
{
    MPIU_Assert(length > 20);
    MPIU_Snprintf(name, length, "%u:%u", GetCurrentProcessId(), ExGetPortValue(queue));
    return;
}

void MPIDI_CH3I_BootstrapQ_unlink(MPIDI_CH3I_BootstrapQ /*queue*/)
{
}

void MPIDI_CH3I_BootstrapQ_destroy(MPIDI_CH3I_BootstrapQ /*queue*/)
{
}


int MPIDI_CH3I_BootstrapQ_attach(const char* name, MPIDI_CH3I_BootstrapQ * queue_ptr)
{
    char* p;
    int pid;
    int queue_id = 0;
    BOOL fSucc;
    HANDLE hQueue;
    HANDLE hProcess;

    pid = strtoul(name, &p, 10);
    if(*p != '\0')
    {
        queue_id = strtoul(p + 1, &p, 10);
    }

    if(pid == 0 || queue_id == 0)
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**argstr_shmq");

    hProcess = OpenProcess(
                    PROCESS_DUP_HANDLE,
                    FALSE, // bInheritHandle
                    pid
                    );

    if (hProcess == NULL)
    {
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**OpenProcess %d %d", pid, ::GetLastError());
    }

    fSucc = DuplicateHandle(
                hProcess,
                UlongToHandle(queue_id),
                GetCurrentProcess(),
                &hQueue,
                0, // dwDesiredAccess is ignored,
                FALSE, // bInheritHandle,
                DUPLICATE_SAME_ACCESS
                );
    if(!fSucc)
    {
        int mpi_errno = MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**duphandle %s %d", name, ::GetLastError());
        CloseHandle(hProcess);
        return mpi_errno;
    }

    CloseHandle(hProcess);

    *queue_ptr = hQueue;
    return MPI_SUCCESS;
}


void MPIDI_CH3I_BootstrapQ_detach(MPIDI_CH3I_BootstrapQ queue)
{
    CloseHandle(queue);
}


int
MPIDI_CH3I_BootstrapQ_send_msg(
    MPIDI_CH3I_BootstrapQ queue,
    int key,
    const void* buffer,
    int length
    )
{
    BOOL fSucc;
    fSucc = PostQueuedCompletionStatus(
                queue,
                length,
                key,
                (OVERLAPPED*)buffer
                );

    if(!fSucc)
    {
        //DWORD gle = GetLastError();
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**boot_send");
    }

    return MPI_SUCCESS;
}


int MPIDI_CH3I_Notify_connect(MPIDI_CH3I_BootstrapQ queue, HANDLE hShm, int pid)
{
    return MPIDI_CH3I_BootstrapQ_send_msg(queue, EX_KEY_SHM_NOTIFY_CONNECTION, hShm, pid);
}


int MPIDI_CH3I_Notify_message(MPIDI_CH3I_BootstrapQ queue)
{
    return MPIDI_CH3I_BootstrapQ_send_msg(queue, EX_KEY_SHM_NOTIFY_MESSAGE, NULL, 0);
}


void MPIDI_CH3I_Notify_accept_connect(ch3u_bootstrapq_routine pfnAcceptConnection)
{
    ExRegisterCompletionProcessor(EX_KEY_SHM_NOTIFY_CONNECTION, pfnAcceptConnection);
}


void MPIDI_CH3I_Notify_accept_message(ch3u_bootstrapq_routine pfnAcceptMessage)
{
    ExRegisterCompletionProcessor(EX_KEY_SHM_NOTIFY_MESSAGE, pfnAcceptMessage);
}
