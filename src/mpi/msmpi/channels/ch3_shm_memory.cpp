// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "mpidi_ch3_impl.h"

static int create_shm_view(unsigned int size, MPIDI_CH3I_SHM_info_t* info)
{
    PVOID pView;
    HANDLE hShm;
    hShm = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                NULL,
                PAGE_READWRITE,
                0,
                size,
                NULL
                );
    if (hShm == NULL)
    {
        return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**CreateFileMapping %d", ::GetLastError());
    }

    pView = MapViewOfFile(
                hShm,
                FILE_MAP_WRITE,
                0,
                0,
                size
                );
    if (pView == NULL)
    {
        int mpi_errno = MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**MapViewOfFileEx %d", ::GetLastError());
        CloseHandle(hShm);
        return mpi_errno;
    }

    info->hShm = hShm;
    info->addr = (MPIDI_CH3I_SHM_Queue_t*)pView;
    info->size = size;
    return MPI_SUCCESS;

}


/* MPIDI_CH3I_SHM_Get_mem - allocate and get the address and size of a
   shared memory block */
int MPIDI_CH3I_SHM_Get_mem(int size, MPIDI_CH3I_SHM_info_t* info)
{
    int mpi_errno = MPI_SUCCESS;

    if (size == 0 || size > MPIDU_MAX_SHM_BLOCK_SIZE )
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**arg");
        return mpi_errno;
    }

    /* Create the shared memory object */
    mpi_errno = create_shm_view(size, info);

    return mpi_errno;
}


static int open_shm_view(int pid, int remote_shm_id, MPIDI_CH3I_SHM_info_t* info)
{
    BOOL fSucc;
    PVOID pView;
    DWORD Size;
    HANDLE hShm;
    HANDLE hProcess;
    SIZE_T n;
    MEMORY_BASIC_INFORMATION mbi;

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
                UlongToHandle(remote_shm_id),
                GetCurrentProcess(),
                &hShm,
                0, // dwDesiredAccess is ignored,
                FALSE, // bInheritHandle,
                DUPLICATE_SAME_ACCESS
                );
    if(!fSucc)
    {
        int mpi_errno = MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**duphandle %d", ::GetLastError());
        CloseHandle(hProcess);
        return mpi_errno;
    }

    CloseHandle(hProcess);

    pView = MapViewOfFile(
                hShm,
                FILE_MAP_WRITE,
                0,
                0,
                0
                );
    if (pView == NULL)
    {
        int mpi_errno = MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**MapViewOfFileEx %d", ::GetLastError());
        CloseHandle(hShm);
        return mpi_errno;
    }

    n = VirtualQuery(pView, &mbi, sizeof(mbi));
    MPIU_Assert(n == sizeof(mbi));
    Size = (DWORD)min(mbi.RegionSize, MPIDU_MAX_SHM_BLOCK_SIZE);

    info->hShm = hShm;
    info->addr = (MPIDI_CH3I_SHM_Queue_t*)pView;
    info->size = Size;
    return MPI_SUCCESS;
}


int MPIDI_CH3I_SHM_Attach_to_mem(int pid, int remote_shm_id, MPIDI_CH3I_SHM_info_t* info)
{
    int mpi_errno;

    /* Create the shared memory object */
    mpi_errno = open_shm_view(pid, remote_shm_id, info);

    return mpi_errno;
}

int MPIDI_CH3I_SHM_Unlink_mem(MPIDI_CH3I_SHM_info_t* /*p*/)
{
    int mpi_errno = MPI_SUCCESS;

    /* Windows requires no special handling */

    return mpi_errno;
}

/*@
   MPIDI_CH3I_SHM_Release_mem -

   Notes:
@*/
int MPIDI_CH3I_SHM_Release_mem(MPIDI_CH3I_SHM_info_t *p)
{
    UnmapViewOfFile(p->addr);
    CloseHandle(p->hShm);
    return MPI_SUCCESS;
}
/* END OF WINDOWS_SHM */

