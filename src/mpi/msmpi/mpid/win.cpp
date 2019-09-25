// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "mpidi_ch3_impl.h"
#include "mpidrma.h"


#ifndef MPID_WIN_PREALLOC
#define MPID_WIN_PREALLOC 8
#endif

C_ASSERT( HANDLE_GET_TYPE(MPI_WIN_NULL) == HANDLE_TYPE_INVALID );
C_ASSERT( HANDLE_GET_MPI_KIND(MPI_WIN_NULL) == MPID_WIN );

/* Preallocated window objects */
MPID_Win MPID_Win_direct[MPID_WIN_PREALLOC] = { {0} };
MPIU_Object_alloc_t MPID_Win_mem = { 0, 0, 0, 0, MPID_WIN,
                                      sizeof(MPID_Win), MPID_Win_direct,
                                      MPID_WIN_PREALLOC};


const char* mpi_win_name = "";

//
// Structure that keeps all creation related information for a window. 
// Used to distribute created windows' info among processes
//
typedef struct
{
    DWORD m_idSrcProcess;
    HANDLE m_hSrcShm;
    MPI_Aint m_size;
    int m_dispUnit;
    MPI_Win m_winHandle;
} WinCreationInfoNonContig;

//
// Used to distribute contig, different sized windows' info among processes
//
typedef struct
{
    MPI_Aint m_size;
    int m_dispUnit;
    MPI_Win m_winHandle;
} WinCreationInfoContig;


const char* MPID_Win::GetDefaultName() const
{
    return mpi_win_name;
}


int MPIDI_WinShmInfo::Initialize(
    _In_ bool bContig,
    _In_ int nCommSize
    )
{
    m_processCount = nCommSize;
    m_bMemBlocksContig = bContig;

    MPIU_Assert(m_pMappedShmInfo == nullptr);
    MPIU_Assert(m_pShmSrcInfo == nullptr);

    m_pMappedShmInfo = new MappedShmInfo[m_processCount];

    if(m_pMappedShmInfo == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    ZeroMemory(m_pMappedShmInfo, m_processCount * sizeof(MappedShmInfo));

    m_pShmSrcInfo = new ShmSrcInfo[GetSrcInfoCount()];
    
    if(m_pShmSrcInfo == nullptr)
    {
        CleanUpMemory();
        return MPIU_ERR_NOMEM();
    }

    return MPI_SUCCESS;
}


void MPIDI_WinShmInfo::CleanUpMemory()
{
    delete[] m_pMappedShmInfo;
    delete[] m_pShmSrcInfo;

    m_pMappedShmInfo = nullptr;
    m_pShmSrcInfo = nullptr;
}


void MPIDI_WinShmInfo::ReleaseHandles()
{
    int nSrcInfoCount = GetSrcInfoCount();

    for(int i = 0; i < nSrcInfoCount; ++i)
    {
        if(m_pMappedShmInfo[i].m_pBase == nullptr)
        {
            continue;
        }
        UnmapViewOfFile(m_pMappedShmInfo[i].m_pBase);
        m_pMappedShmInfo[i].m_pBase = nullptr;
        CloseHandle(m_pShmSrcInfo[i].m_hDestShm);
    }
}


int MPIDI_WinShmInfo::MapView(_In_range_(>=, 0) int rank)
{
    //
    // If blocks are contig, only rank-0 creates file mapping, so map view of rank-0
    //
    rank = m_bMemBlocksContig ? 0 : rank;

    DWORD idSrcProcess = m_pShmSrcInfo[rank].m_idSrcProcess;

    if(GetCurrentProcessId() == idSrcProcess)
    {
        //
        // Source handle is valid, map view directly
        //
        m_pShmSrcInfo[rank].m_hDestShm = m_pShmSrcInfo[rank].m_hSrcShm;
    }
    else
    {
        //
        // Duplicate the source handle
        //
        HANDLE hSrcProcess = OpenProcess(
            PROCESS_DUP_HANDLE,
            FALSE,
            idSrcProcess
            );

        if(hSrcProcess == nullptr)
        {
            int mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_OTHER,
                "**OpenProcess %d %d",
                idSrcProcess,
                ::GetLastError()
                );
            return mpi_errno;
        }

        BOOL fDup = DuplicateHandle(
            hSrcProcess,
            m_pShmSrcInfo[rank].m_hSrcShm,
            GetCurrentProcess(),
            &(m_pShmSrcInfo[rank].m_hDestShm),
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS
            );

        if(!fDup)
        {
            MPI_RESULT mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_OTHER,
                "**duphandle %d",
                ::GetLastError()
                );

            CloseHandle(hSrcProcess);
            return mpi_errno;
        }
        
        CloseHandle(hSrcProcess);
    }

    void *pBase = MapViewOfFile(
        m_pShmSrcInfo[rank].m_hDestShm,
        FILE_MAP_WRITE,
        0,
        0,
        0
        );
    if (pBase == nullptr)
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**MapViewOfFile %d",
            ::GetLastError()
            );
    }

    m_pMappedShmInfo[rank].m_pBase = pBase;

    if(m_bMemBlocksContig)
    {
        //
        // if memory is contiguous, can calculate all ranks according to offsets
        //
        for(int i = 1; i < m_processCount; ++i)
        {
            m_pMappedShmInfo[i].m_pBase = static_cast<char*>(m_pMappedShmInfo[i-1].m_pBase)
                + m_pMappedShmInfo[i-1].m_size;
        }
    }

    return MPI_SUCCESS;
}


MPI_RESULT
MPIDI_WinShmInfo::GetMappedView(
    _In_range_(>=, 0) int rank,
    _Out_ MPI_Aint *pSize,
    _Out_ int *pDispUnit,
    _Outptr_ void **ppBase
    )
{
    *pSize = m_pMappedShmInfo[rank].m_size;
    *pDispUnit = m_pMappedShmInfo[rank].m_dispUnit;

    if(m_pMappedShmInfo[rank].m_pBase != nullptr)
    {
        //
        // View is already mapped, return address
        //
        *ppBase = m_pMappedShmInfo[rank].m_pBase;
        return MPI_SUCCESS;
    }

    //
    // Create mappings and return address
    //
    MPI_RESULT mpi_errno = MapView(rank);
    if(mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    *ppBase = m_pMappedShmInfo[rank].m_pBase;
    return mpi_errno;
}


MPI_RESULT
MPID_Win_create_noncontig(
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_ const MPID_Comm *pComm,
    _Inout_ MPID_Win *pWin
    )
{
    //
    // Each process will create its own segment, then all-gather the shm info, sizes, 
    // disp_units, and win handles
    //
    ULARGE_INTEGER ulSize;
    ulSize.QuadPart = static_cast<ULONGLONG>(size);

    HANDLE hShm = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        ulSize.HighPart,
        ulSize.LowPart,
        NULL
        );
    if (hShm == nullptr)
    {
        return MPIU_ERR_CREATE(
            MPI_ERR_OTHER,
            "**CreateFileMapping %d",
            ::GetLastError()
            );
    }

    MPI_RESULT mpi_errno;
    WinCreationInfoNonContig* creationInfo =
        new WinCreationInfoNonContig[pComm->remote_size];
    if(creationInfo == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    int rank = pComm->rank;
    creationInfo[rank].m_idSrcProcess = GetCurrentProcessId();
    creationInfo[rank].m_hSrcShm = hShm;
    creationInfo[rank].m_size = size;
    creationInfo[rank].m_dispUnit = dispUnit;
    creationInfo[rank].m_winHandle = pWin->dev.all_win_handles[rank];

    mpi_errno = MPIR_Allgather_intra(
        MPI_IN_PLACE,
        0,
        g_hBuiltinTypes.MPI_Datatype_null,
        creationInfo,
        sizeof(WinCreationInfoNonContig),
        g_hBuiltinTypes.MPI_Byte,
        pComm
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    for(int i = 0; i < pComm->remote_size; ++i)
    {
        pWin->dev.shmInfo.m_pShmSrcInfo[i].m_idSrcProcess = creationInfo[i].m_idSrcProcess;
        pWin->dev.shmInfo.m_pShmSrcInfo[i].m_hSrcShm = creationInfo[i].m_hSrcShm;
        pWin->dev.shmInfo.m_pMappedShmInfo[i].m_size = creationInfo[i].m_size;
        pWin->dev.shmInfo.m_pMappedShmInfo[i].m_dispUnit = creationInfo[i].m_dispUnit;
        pWin->dev.all_win_handles[i] = creationInfo[i].m_winHandle;
    }

fn_exit:
    delete[] creationInfo;
    return mpi_errno;

fn_fail:
    CloseHandle(hShm);
    goto fn_exit;
}


MPI_RESULT
MPID_Win_create_contig(
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_ const MPID_Comm *pComm,
    _Inout_ MPID_Win *pWin
    )
{
    //
    // Root rank sums all sizes and allocates contig mem, then handles and sizes are
    // distributed among processes
    //
    WinCreationInfoContig* creationInfo = 
        new WinCreationInfoContig[pComm->remote_size];
    if(creationInfo == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    int rank = pComm->rank;
    creationInfo[rank].m_size = size;
    creationInfo[rank].m_dispUnit = dispUnit;
    creationInfo[rank].m_winHandle = pWin->dev.all_win_handles[rank];

    MPI_RESULT mpi_errno = MPIR_Allgather_intra(
        MPI_IN_PLACE,
        0,
        g_hBuiltinTypes.MPI_Datatype_null,
        creationInfo,
        sizeof(WinCreationInfoContig),
        g_hBuiltinTypes.MPI_Byte,
        pComm
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    for(int i = 0; i < pComm->remote_size; ++i)
    {
        pWin->dev.shmInfo.m_pMappedShmInfo[i].m_size = creationInfo[i].m_size;
        pWin->dev.shmInfo.m_pMappedShmInfo[i].m_dispUnit = creationInfo[i].m_dispUnit;
        pWin->dev.all_win_handles[i] = creationInfo[i].m_winHandle;
    }

    if(pComm->rank == 0)
    {
        ULARGE_INTEGER totalSize;
        totalSize.QuadPart = 0;

        for(int i = 0; i < pComm->remote_size; ++i)
        {
            //
            // size has to be >=0; it can be safely cast into ULONGLONG 
            //
            ULONGLONG tempSize = totalSize.QuadPart +
                static_cast<ULONGLONG>(creationInfo[i].m_size);

            //
            // Check if there is overflow: is the addition smaller after the operation
            //
            if(tempSize >= totalSize.QuadPart)
            {
                totalSize.QuadPart = tempSize;
                continue;
            }

            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_OTHER,
                "**overflow %s",
                "sum of window sizes"
                );
            goto fn_fail;
        }

        HANDLE hShm = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            totalSize.HighPart,
            totalSize.LowPart,
            NULL
            );
        if (hShm == nullptr)
        {
            mpi_errno = MPIU_ERR_CREATE(
                MPI_ERR_OTHER,
                "**CreateFileMapping %d",
                ::GetLastError()
                );

            goto fn_fail;
        }
        pWin->dev.shmInfo.m_pShmSrcInfo[0].m_idSrcProcess = GetCurrentProcessId();
        pWin->dev.shmInfo.m_pShmSrcInfo[0].m_hSrcShm = hShm;
    }

    mpi_errno = MPIR_Bcast_intra(
        pWin->dev.shmInfo.m_pShmSrcInfo,
        sizeof(MPIDI_WinShmInfo::ShmSrcInfo),
        g_hBuiltinTypes.MPI_Byte,
        0,
        pComm
        );
    if(mpi_errno != MPI_SUCCESS && pComm->rank == 0)
    {
        CloseHandle(pWin->dev.shmInfo.m_pShmSrcInfo[0].m_hSrcShm);
    }

fn_fail:
    delete[] creationInfo;
    return mpi_errno;
}


MPI_RESULT
MPID_Win_create(
    _In_opt_ void *base,
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_opt_ const MPID_Info * pInfo,
    _In_ const MPID_Comm * pComm,
    _In_ bool isAllocated,
    _In_ bool isShm,
    _Outptr_ MPID_Win **ppWin
    )
{
    MPID_Win *pWin = (MPID_Win *)MPIU_Handle_obj_alloc( &MPID_Win_mem );
    if(pWin == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    pWin->dev.isAllocated = isAllocated;
    pWin->dev.isShm = isShm;
    pWin->fence_cnt = 0;
    pWin->base = base;
    pWin->size = size;
    pWin->disp_unit = dispUnit;
    pWin->start_group_ptr = nullptr;
    pWin->start_assert = 0;
    pWin->attributes = nullptr;
    pWin->dev.rma_ops_list = nullptr;
    pWin->dev.remote_lock_state = nullptr;
    pWin->dev.remote_ops_done = nullptr;
    pWin->dev.requested_ops_count = nullptr;
    pWin->dev.lock_op_used = MPIDI_WIN_NOT_LOCKED;
    pWin->dev.error_found = false;
    pWin->dev.rma_error = MPI_SUCCESS;
    pWin->dev.lock_state = MPIDI_LOCKED_NOT_LOCKED;
    pWin->dev.shared_lock_ref_cnt = 0;
    pWin->dev.lock_queue = nullptr;
    pWin->dev.my_counter = 0;
    pWin->dev.my_pt_rma_puts_accs = 0;
    pWin->errhandler = nullptr;
    pWin->regions = nullptr;

    InitName<MPID_Win>( pWin );

    MpiLockInitialize(&pWin->winLock);

    MPI_Comm newcomm;
    MPI_RESULT mpi_errno = NMPI_Comm_dup(pComm->handle, &newcomm);
    if( mpi_errno != MPI_SUCCESS )
    {
        goto fn_fail1;
    }

    pWin->comm_ptr = CommPool::Get( newcomm );
    MPIU_Assert( pWin->comm_ptr != nullptr );

    //
    // allocate memory for window handles and completion counters of all processes
    //
    int comm_size = pComm->remote_size;

    pWin->dev.all_win_handles = new MPI_Win[comm_size];

    if( pWin->dev.all_win_handles == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail2;
    }

    pWin->dev.all_win_handles[pComm->rank] = pWin->handle;

    pWin->dev.pt_rma_puts_accs = new int[comm_size];
    if( pWin->dev.pt_rma_puts_accs == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail3;
    }

    for (int i=0; i<comm_size; i++)
    {
        pWin->dev.pt_rma_puts_accs[i] = 0;
    }

    pWin->dev.remote_lock_state = new int[comm_size];
    if (pWin->dev.remote_lock_state == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail4;
    }

    for (int i = 0; i<comm_size; i++)
    {
        pWin->dev.remote_lock_state[i] = MPIDI_LOCKED_NOT_LOCKED;
    }

    pWin->dev.remote_ops_done = new int[comm_size];
    if (pWin->dev.remote_ops_done == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail5;
    }

    for (int i = 0; i<comm_size; i++)
    {
        pWin->dev.remote_ops_done[i] = 1;
    }

    pWin->dev.requested_ops_count = new int[comm_size];
    if (pWin->dev.requested_ops_count == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail6;
    }

    for (int i = 0; i<comm_size; i++)
    {
        pWin->dev.requested_ops_count[i] = 0;
    }

    //
    // Local memory window. Used by MPI_Win_create
    //
    if(!pWin->dev.isShm)
    {
        mpi_errno = MPIR_Allgather_intra(
            MPI_IN_PLACE,
            0,
            g_hBuiltinTypes.MPI_Datatype_null,
            pWin->dev.all_win_handles,
            sizeof(MPI_Win),
            g_hBuiltinTypes.MPI_Byte,
            pComm
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }

        *ppWin = pWin;
        return MPI_SUCCESS;
    }
    
    //
    // Get hint for pWin allocation
    //
    bool bContig = true;

    if(pInfo != nullptr)
    {
        int flag;
        char value[MPI_MAX_INFO_VAL];

        NMPI_Info_get(pInfo->handle, "alloc_shared_noncontig", MPI_MAX_INFO_VAL, value, &flag);
        if(flag != 0 && CompareStringA( LOCALE_INVARIANT,
            NORM_IGNORECASE | NORM_IGNORENONSPACE,
            value,
            -1,
            "true",
            -1 )  == CSTR_EQUAL )
        {
            bContig = false;
        }
    }

    mpi_errno = pWin->dev.shmInfo.Initialize(bContig, comm_size);
    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    if(bContig)
    {
        mpi_errno = MPID_Win_create_contig(size, dispUnit, pComm, pWin);
    }
    else
    {
        mpi_errno = MPID_Win_create_noncontig(size, dispUnit, pComm, pWin);
    }

    if (mpi_errno != MPI_SUCCESS)
    {
        goto fn_fail;
    }

    *ppWin = pWin;

fn_exit:
    return mpi_errno;
fn_fail:
    delete[] pWin->dev.requested_ops_count;
fn_fail6:
    delete[] pWin->dev.remote_ops_done;
fn_fail5:
    delete[] pWin->dev.remote_lock_state;
fn_fail4:
    delete[] pWin->dev.pt_rma_puts_accs;
fn_fail3:
    delete[] pWin->dev.all_win_handles;
fn_fail2:
    MPIR_Comm_release( pWin->comm_ptr, 0 );
fn_fail1:
    MPIU_Handle_obj_free( &MPID_Win_mem, pWin );
    goto fn_exit;
}


MPI_RESULT
MPID_Win_allocate(
    _In_range_(>= , 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_opt_ MPID_Info *pInfo,
    _In_ const MPID_Comm *pComm,
    _Out_ void *ppBase,
    _Outptr_ MPID_Win **ppWin
)
{
    MPI_RESULT mpi_errno;

    //
    // in case size=0 there is no need to allocate the memory
    //
    if (size == 0)
    {
        *static_cast<void **>(ppBase) = nullptr;
        mpi_errno = MPID_Win_create(nullptr, size, dispUnit, pInfo, pComm, true, false, ppWin);
        return mpi_errno;
    }

    void *ap = MPID_Alloc_mem(size, pInfo);
    if (!ap)
    {
        return MPIU_ERR_CREATE(MPI_ERR_NO_MEM, "**allocmem");
    }

    mpi_errno = MPID_Win_create(ap, size, dispUnit, pInfo, pComm, true, false, ppWin);
    if (mpi_errno != MPI_SUCCESS)
    {
        MPID_Free_mem(ap);
    }
    else
    {
        *static_cast<void **>(ppBase) = ap;
    }
    return mpi_errno;
}


MPI_RESULT
MPID_Win_allocate_shared(
    _In_range_(>=, 0) MPI_Aint size,
    _In_range_(>, 0) int dispUnit,
    _In_opt_ const MPID_Info *pInfo,
    _In_ const MPID_Comm *pComm,
    _Outptr_ void **ppBase,
    _Outptr_ MPID_Win **ppWin
    )
{
    MPI_RESULT mpi_errno = MPID_Win_create(nullptr, size, dispUnit, pInfo, pComm, false, true, ppWin);
    if(mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    MPI_Aint temp_size;
    int temp_disp;

    mpi_errno = (*ppWin)->dev.shmInfo.GetMappedView(pComm->rank, &temp_size, &temp_disp, ppBase);
    (*ppWin)->base = *ppBase;

    if(mpi_errno != MPI_SUCCESS)
    {
        MPID_Win_free(*ppWin);
    }

    return mpi_errno;
}


MPI_RESULT
MPID_Win_shared_query(
    _In_ MPID_Win *pWin,
    _In_range_(>, MPI_PROC_NULL) int rank,
    _Out_ MPI_Aint *pSize,
    _Out_ int *pDispUnit,
    _Outptr_ void **ppBase
    )
{
    if(MPI_PROC_NULL == rank)
    {
        //
        // When rank is MPI_PROC_NULL, return the values belonging the lowest rank
        // that specified size > 0.
        //
        rank = 0;
        for(int i = 0; i < pWin->dev.shmInfo.m_processCount; ++i)
        {
            if(pWin->dev.shmInfo.m_pMappedShmInfo[i].m_size > 0)
            {
                rank = i;
                break;
            }
        }
    }

    return pWin->dev.shmInfo.GetMappedView(rank, pSize, pDispUnit, ppBase);
}


static 
MPI_RESULT
MPID_Add_new_region(
    _In_ MPID_Win* pWin,
    _In_ MPI_Aint base,
    _In_range_(>= , 0) MPI_Aint size,
    _Out_opt_ MPIDI_Win_memory_region_t* pPrevRegion,
    _In_opt_ MPIDI_Win_memory_region_t* pNextRegion
    )
{
    MPIDI_Win_memory_region_t* pNewRegion = new MPIDI_Win_memory_region_t;
    if (pNewRegion == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    pNewRegion->base = base;
    pNewRegion->size = size;
    if (pPrevRegion == nullptr)
    {
        pWin->regions = pNewRegion;
    }
    else
    {
        pPrevRegion->next = pNewRegion;
    }
    pNewRegion->next = pNextRegion;
    pWin->size += size;
    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Win_attach(
    _In_ MPID_Win* pWin,
    _In_ MPI_Aint base,
    _In_range_(>=, 0) MPI_Aint size
    )
{
    if (pWin->regions == nullptr)
    {
        return MPID_Add_new_region(pWin, base, size, nullptr, nullptr);
    }

    MPI_Aint upperBound = base + size;
    MPIDI_Win_memory_region_t* pRegion = pWin->regions;
    MPIDI_Win_memory_region_t* pPrevRegion = nullptr;

    while (pRegion != nullptr)
    {
        if (pRegion->base < base)
        {
            pPrevRegion = pRegion;
            pRegion = pRegion->next;
            continue;
        }
        if ((upperBound > pRegion->base) || (pPrevRegion && (pPrevRegion->base + pPrevRegion->size > base)))
        {
            return MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "attempting to attach intersecting memory region");
        }
        return MPID_Add_new_region(pWin, base, size, pPrevRegion, pRegion);
    }

    if (pPrevRegion->base + pPrevRegion->size > base)
    {
        return MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "attempting to attach intersecting memory region");
    }
    else
    {
        return MPID_Add_new_region(pWin, base, size, pPrevRegion, nullptr);
    }
}


MPI_RESULT
MPID_Win_detach(
    _In_ MPID_Win* pWin,
    _In_opt_ MPI_Aint base
    )
{
    MPIDI_Win_memory_region_t* pRegion = pWin->regions;
    
    if (pRegion->base > base)
    {
        goto fn_fail;
    }
    else if (pRegion->base == base)
    {
        pWin->regions = pRegion->next;
        pWin->size -= pRegion->size;
        delete pRegion;
        return MPI_SUCCESS;
    }
    
    while (pRegion->next != nullptr)
    {
        if (base < pRegion->next->base)
        {
            goto fn_fail;
        }
        else if (base == pRegion->next->base)
        {
            MPIDI_Win_memory_region_t* pDelete = pRegion->next;
            pRegion->next = pRegion->next->next;
            pWin->size -= pDelete->size;
            delete pDelete;
            return MPI_SUCCESS;
        }
        else
        {
            pRegion = pRegion->next;
        }
    }

fn_fail:
    return MPIU_ERR_CREATE(MPI_ERR_ARG, "**arg %s", "attempting to detach memory region that doesn't belong to the window");
}


MPI_RESULT  MPID_Win_free(MPID_Win* win)
{
    MPI_RESULT mpi_errno;
    int total_pt_rma_puts_accs, i, comm_size;
    StackGuardArray<int> recvcnts;

    /* set up the recvcnts array for the reduce scatter to check if all
       passive target rma operations are done */
    MPID_Comm *comm_ptr = win->comm_ptr;
    comm_size = comm_ptr->remote_size;

    recvcnts = new int[comm_size];
    if( recvcnts == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    for (i=0; i<comm_size; i++)  recvcnts[i] = 1;

    mpi_errno = NMPI_Reduce_scatter(win->dev.pt_rma_puts_accs,
                                    &total_pt_rma_puts_accs, recvcnts,
                                    MPI_INT, MPI_SUM, comm_ptr->handle);
    ON_ERROR_FAIL(mpi_errno);

    //
    // poke the progress engine until the two are equal
    //
    while (total_pt_rma_puts_accs != win->dev.my_pt_rma_puts_accs)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

    if (win->dev.error_found == true)
    {
        if (win->dev.rma_error == MPI_SUCCESS)
        {
            return MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestrmaremoteerror");
        }
        else
        {
            return win->dev.rma_error;
        }
    }

    delete[] win->dev.pt_rma_puts_accs;
    delete[] win->dev.all_win_handles;
    delete[] win->dev.remote_lock_state;
    delete[] win->dev.remote_ops_done;
    delete[] win->dev.requested_ops_count;

    if( win->dev.isShm )
    {
        win->dev.shmInfo.ReleaseHandles();
        win->dev.shmInfo.CleanUpMemory();
    }
    if (win->dev.isAllocated && win->size != 0)
    {
        MPID_Free_mem(win->base);
    }
    while (win->regions)
    {
        MPIDI_Win_memory_region_t* pDelete = win->regions;
        win->regions = win->regions->next;
        delete pDelete;
    }
    MpiLockDelete(&win->winLock);
    MPIR_Comm_release( comm_ptr, 0 );

    CleanupName<MPID_Win>( win );

    /* check whether refcount needs to be decremented here as in group_free */
    MPIU_Handle_obj_free( &MPID_Win_mem, win );

fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


static MPI_RESULT
MPID_Win_queue(
    _In_ MPID_Win* win_ptr,
    _In_ MPIDI_Rma_op_t type,
    _In_opt_ void *origin_addr,
    _In_ int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_ int target_rank,
    _In_ MPI_Aint target_disp,
    _In_ int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_opt_ MPID_Request* request
    )
{
    MPIDI_RMA_ops* new_ptr = static_cast<MPIDI_RMA_ops*>(
        MPIU_Malloc( sizeof(MPIDI_RMA_ops) )
        );
    if( new_ptr == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    new_ptr->next = nullptr;
    new_ptr->type = type;
    new_ptr->origin_addr = origin_addr;
    new_ptr->origin_count = origin_count;
    new_ptr->origin_datatype = origin_datatype;
    new_ptr->target_rank = target_rank;
    new_ptr->target_disp = target_disp;
    new_ptr->target_count = target_count;
    new_ptr->target_datatype = target_datatype;
    new_ptr->op = op;
    new_ptr->request = request;

    MpiLockEnter(&win_ptr->winLock);

    MPIDI_RMA_ops **prev_ptr = &win_ptr->dev.rma_ops_list;
    while (*prev_ptr != nullptr)
    {
        prev_ptr = &(*prev_ptr)->next;
    }
    *prev_ptr = new_ptr;

    win_ptr->dev.requested_ops_count[target_rank]++;

    MpiLockLeave(&win_ptr->winLock);

    /* if source or target datatypes are derived, increment their
        reference counts */
    if( origin_datatype.IsPredefined() == false )
    {
        origin_datatype.Get()->AddRef();
    }
    if( target_datatype.IsPredefined() == false )
    {
        target_datatype.Get()->AddRef();
    }

    return MPI_SUCCESS;
}


static MPI_RESULT
MPID_Win_queue_lock(
    _In_ MPID_Win* win_ptr,
    _In_ int lock_type,
    _In_ int dest,
    _In_ int assert
    )
{
    MPIDI_RMA_ops* new_ptr = static_cast<MPIDI_RMA_ops*>(
        MPIU_Malloc(sizeof(MPIDI_RMA_ops))
        );
    if (new_ptr == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    new_ptr->next = nullptr;
    new_ptr->type = MPIDI_RMA_OP_LOCK;
    new_ptr->target_rank = dest;
    new_ptr->lock_type = lock_type;
    new_ptr->assert = assert;

    MpiLockEnter(&win_ptr->winLock);

    if (win_ptr->dev.remote_lock_state[dest] != MPIDI_LOCKED_NOT_LOCKED)
    {
        MPIU_Free(new_ptr);
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, 
            "**rmasyncq %d %d %d %d",
            win_ptr->comm_ptr->rank,
            dest,
            win_ptr->dev.remote_lock_state[dest],
            MPIDI_LOCKED_NOT_LOCKED
        );
    }
    win_ptr->dev.remote_lock_state[dest] = MPIDI_LOCKED_LOCK_REQUESTED;

    MPIDI_RMA_ops **prev_ptr = &win_ptr->dev.rma_ops_list;
    while (*prev_ptr != nullptr)
    {
        prev_ptr = &(*prev_ptr)->next;
    }
    *prev_ptr = new_ptr;

    MpiLockLeave(&win_ptr->winLock);

    return MPI_SUCCESS;
}


static MPI_RESULT
MPID_Win_queue_complex(
    _In_ MPID_Win* win_ptr,
    _In_ MPIDI_Rma_op_t type,
    _In_opt_ void *origin_addr,
    _In_ int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_opt_ void *result_addr,
    _In_ int result_count,
    _In_ TypeHandle result_datatype,
    _In_ int target_rank,
    _In_ MPI_Aint target_disp,
    _In_ int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_opt_ MPID_Request* request
)
{
    MPIDI_RMA_ops* new_ptr = static_cast<MPIDI_RMA_ops*>(
        MPIU_Malloc(sizeof(MPIDI_RMA_ops))
        );
    if (new_ptr == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    new_ptr->next = nullptr;
    new_ptr->type = type;
    new_ptr->origin_addr = origin_addr;
    new_ptr->origin_count = origin_count;
    new_ptr->origin_datatype = origin_datatype;
    new_ptr->result_addr = result_addr;
    new_ptr->result_count = result_count;
    new_ptr->result_datatype = result_datatype;
    new_ptr->target_rank = target_rank;
    new_ptr->target_disp = target_disp;
    new_ptr->target_count = target_count;
    new_ptr->target_datatype = target_datatype;
    new_ptr->op = op;
    new_ptr->request = request;

    MpiLockEnter(&win_ptr->winLock);

    MPIDI_RMA_ops **prev_ptr = &win_ptr->dev.rma_ops_list;
    while (*prev_ptr != nullptr)
    {
        prev_ptr = &(*prev_ptr)->next;
    }
    *prev_ptr = new_ptr;
    win_ptr->dev.requested_ops_count[target_rank]++;

    MpiLockLeave(&win_ptr->winLock);

    //
    // if source, result or target datatypes are derived, increment
    // their reference counts
    //
    if (origin_datatype.IsPredefined() == false)
    {
        origin_datatype.Get()->AddRef();
    }
    if (target_datatype.IsPredefined() == false)
    {
        target_datatype.Get()->AddRef();
    }
    if (result_datatype.IsPredefined() == false)
    {
        result_datatype.Get()->AddRef();
    }

    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Win_Put(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr
    )
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo( origin_count, &dt_contig, &dt_true_lb )
        );

    if ((data_sz == 0) || (target_rank == MPI_PROC_NULL))
    {
        return MPI_SUCCESS;
    }

    /* If the put is a local operation, do it here */
    if (target_rank == win_ptr->comm_ptr->rank)
    {
        return MPIR_Localcopy(origin_addr, origin_count, origin_datatype,
                              (char *) win_ptr->base + win_ptr->disp_unit *
                              target_disp, target_count, target_datatype);
    }
    else
    {
        return MPID_Win_queue(
            win_ptr,
            MPIDI_RMA_OP_PUT,
            const_cast<void*>(origin_addr),
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            MPI_OP_NULL,
            nullptr
            );
    }
}


MPI_RESULT
MPID_Win_Rput(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
    )
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPI_RESULT mpi_errno;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo( origin_count, &dt_contig, &dt_true_lb )
        );

    if (data_sz == 0 || target_rank == MPI_PROC_NULL)
    {
        MPID_Request* preq = MPIDI_Request_create_completed_req();
        if (preq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }

        *request = preq;
        return MPI_SUCCESS;
    }

    /* If the put is a local operation, do it here */
    if (target_rank == win_ptr->comm_ptr->rank)
    {
        MPID_Request* preq = MPIDI_Request_create_completed_req();
        if (preq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        mpi_errno = MPIR_Localcopy(origin_addr, origin_count, origin_datatype,
                              (char *) win_ptr->base + win_ptr->disp_unit *
                              target_disp, target_count, target_datatype);
        preq->status.MPI_ERROR = mpi_errno;

        *request = preq;
        return mpi_errno;
    }
    else
    {
        MPID_Request* preq = MPID_Request_create(MPID_REQUEST_RMA_PUT);
        if (preq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        preq->init_pkt(MPIDI_CH3_PKT_PUT);
        preq->dev.source_win_handle = win_ptr->handle;
        preq->dev.target_rank = target_rank;

        mpi_errno = MPID_Win_queue(
            win_ptr,
            MPIDI_RMA_OP_PUT,
            const_cast<void*>(origin_addr),
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            MPI_OP_NULL,
            preq
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            preq->Release();
            return mpi_errno;
        }
        *request = preq;
        return mpi_errno;
    }
}


MPI_RESULT
MPID_Win_Get(
    _Out_opt_ void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr
    )
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo( origin_count, &dt_contig, &dt_true_lb )
        );

    if ((data_sz == 0) || (target_rank == MPI_PROC_NULL))
    {
        return MPI_SUCCESS;
    }

    /* If the get is a local operation, do it here */
    if (target_rank == win_ptr->comm_ptr->rank)
    {
        return MPIR_Localcopy((char *) win_ptr->base +
                              win_ptr->disp_unit * target_disp,
                              target_count, target_datatype,
                              origin_addr, origin_count,
                              origin_datatype);
    }
    else
    {
        return MPID_Win_queue(
            win_ptr,
            MPIDI_RMA_OP_GET,
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            MPI_OP_NULL,
            nullptr
            );
    }
}


MPI_RESULT
MPID_Win_Rget(
    _Out_opt_ void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
)
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPI_RESULT mpi_errno;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo( origin_count, &dt_contig, &dt_true_lb )
        );

    if (data_sz == 0 || target_rank == MPI_PROC_NULL)
    {
        MPID_Request* preq = MPIDI_Request_create_completed_req();
        if (preq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }

        *request = preq;
        return MPI_SUCCESS;
    }

    /* If the get is a local operation, do it here */
    if (target_rank == win_ptr->comm_ptr->rank)
    {
        MPID_Request* sreq = MPIDI_Request_create_completed_req();
        if (sreq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        mpi_errno = MPIR_Localcopy((char *) win_ptr->base +
                              win_ptr->disp_unit * target_disp,
                              target_count, target_datatype,
                              origin_addr, origin_count,
                              origin_datatype);
        sreq->status.MPI_ERROR = mpi_errno;
        *request = sreq;
        return mpi_errno;
    }
    else
    {
        MPID_Request* rreq = MPID_Request_create(MPID_REQUEST_RMA_GET);
        if (rreq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        rreq->dev.target_rank = target_rank;
        rreq->dev.source_win_handle = win_ptr->handle;

        mpi_errno = MPID_Win_queue(
            win_ptr,
            MPIDI_RMA_OP_GET,
            origin_addr,
            origin_count,
            origin_datatype,
            target_rank,
            target_disp,
            target_count,
            target_datatype,
            MPI_OP_NULL,
            rreq
            );
        if (mpi_errno != MPI_SUCCESS)
        {
            rreq->Release();
            return mpi_errno;
        }
        *request = rreq;
        return mpi_errno;
    }
}


MPI_RESULT
MPIDI_Win_local_accumulate(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_count,
    _In_ char *target_addr,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op
    )
{
    if (op == MPI_REPLACE)
    {
        return MPIR_Localcopy(
            origin_addr,
            origin_count,
            origin_datatype,
            target_addr,
            target_count, 
            target_datatype
            );
    }

    if(HANDLE_GET_TYPE(op) != HANDLE_TYPE_BUILTIN)
    {
        return MPIU_ERR_CREATE(MPI_ERR_OP, "**opnotpredefined %d", op);
    }

    /* get the function by indexing into the op table */
    MPI_User_function *uop = MPIR_Op_table[(op)%16 - 1];

    if( origin_datatype.IsBuiltin() &&
        target_datatype.IsBuiltin() )
    {
        //
        // Cast away the constness as MPI_User_function is missing the const for
        // the first parameter.
        //
        MPI_Datatype hTargetType = target_datatype.GetMpiHandle();
        (*uop)(
            const_cast<void*>(origin_addr),
            target_addr,
            &target_count,
            &hTargetType
            );
    }
    else
    {
        /* derived datatype */

        MPID_Segment *segp;
        StackGuardArray<MPID_IOV> dloop_vec;
        MPI_Aint first, last;
        int vec_len, i, type_size, count;
        MPI_Datatype type;
        MPI_Count true_lb, true_extent, extent;
        StackGuardArray<BYTE> auto_buf;
        BYTE* tmp_buf=nullptr;
        const void *source_buf;
        MPI_RESULT mpi_errno = MPI_SUCCESS;

        if (origin_datatype != target_datatype)
        {
            /* first copy the data into a temporary buffer with
                the same datatype as the target. Then do the
                accumulate operation. */
            true_extent = target_datatype.GetTrueExtentAndLowerBound( &true_lb );
            extent = target_datatype.GetExtent();

            MPI_Count cbAlloc = target_count * max( extent, true_extent );
            if( cbAlloc > SIZE_MAX )
            {
                return MPIU_ERR_NOMEM();
            }
            auto_buf = new BYTE[static_cast<size_t>(cbAlloc)];
            if( auto_buf == nullptr )
            {
                return MPIU_ERR_NOMEM();
            }
            /* adjust for potential negative lower bound in datatype */
            tmp_buf = auto_buf - true_lb;

            mpi_errno = MPIR_Localcopy(origin_addr, origin_count,
                                        origin_datatype, tmp_buf,
                                        target_count, target_datatype);
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }

        segp = MPID_Segment_alloc();
        if( segp == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        MPID_Segment_init(NULL, target_count, target_datatype.GetMpiHandle(), segp, 0);
        first = 0;
        last  = SEGMENT_IGNORE_LAST;

        const MPID_Datatype *dtp = target_datatype.Get();
        vec_len = dtp->max_contig_blocks * target_count + 1;
        /* +1 needed because Rob says so */
        dloop_vec = new MPID_IOV[vec_len];
        if( dloop_vec == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        MPID_Segment_pack_vector(segp, first, &last, dloop_vec, &vec_len);

        source_buf = (tmp_buf != NULL) ? tmp_buf : origin_addr;
        type = dtp->eltype;
        type_size = MPID_Datatype_get_basic_size(type);
        for (i=0; i<vec_len; i++)
        {
            count = (dloop_vec[i].len)/type_size;
            (*uop)((char *)source_buf + MPIU_PtrToAint(dloop_vec[i].buf),
                    target_addr + MPIU_PtrToAint(dloop_vec[i].buf),
                    &count, &type);
        }

        MPID_Segment_free(segp);
    }

    return MPI_SUCCESS;
}


MPI_RESULT
MPID_Win_Accumulate(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr
    )
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo( origin_count, &dt_contig, &dt_true_lb )
        );

    if ((data_sz == 0) || (target_rank == MPI_PROC_NULL))
    {
        return MPI_SUCCESS;
    }

    if(win_ptr->dev.isShm)
    {
        MPI_Aint size_target;
        int target_disp_unit = 0;
        void *base_target = win_ptr->base;

        MPID_Win_shared_query( win_ptr, target_rank, &size_target, &target_disp_unit, &base_target);

        char *target_addr = static_cast<char *>(base_target) + target_disp_unit * target_disp;
        return MPIDI_Win_local_accumulate(
            origin_addr,
            origin_count,
            origin_datatype,
            target_count,
            target_addr,
            target_datatype,
            op
            );
    }
    
    if (target_rank == win_ptr->comm_ptr->rank)
    {
        char *target_addr = static_cast<char *>(win_ptr->base) + win_ptr->disp_unit * target_disp;
        return MPIDI_Win_local_accumulate(
            origin_addr,
            origin_count,
            origin_datatype,
            target_count,
            target_addr,
            target_datatype,
            op
            );
    }

    return MPID_Win_queue(
        win_ptr,
        MPIDI_RMA_OP_ACCUMULATE,
        const_cast<void*>(origin_addr),
        origin_count,
        origin_datatype,
        target_rank,
        target_disp,
        target_count,
        target_datatype,
        op,
        nullptr
        );
}


MPI_RESULT
MPID_Win_Get_accumulate(
    _In_opt_ const void* origin_addr,
    _In_range_(>= , 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_opt_ void* result_addr,
    _In_range_(>= , 0) int result_count,
    _In_ TypeHandle result_datatype,
    _In_range_(>= , 0) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_range_(>= , 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr
)
{
    MPI_RESULT mpi_errno;
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo(origin_count, &dt_contig, &dt_true_lb)
        );

    if (data_sz == 0)
    {
        //
        // fix OACR warning for result_addr
        //
        OACR_USE_PTR(result_addr);
        return MPI_SUCCESS;
    }

    if (target_rank == win_ptr->comm_ptr->rank)
    {
        char *target_addr = static_cast<char *>(win_ptr->base) + win_ptr->disp_unit * target_disp;

        mpi_errno = MPIR_Localcopy((char *)win_ptr->base +
            win_ptr->disp_unit * target_disp,
            target_count, target_datatype,
            result_addr, result_count,
            result_datatype);
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }

        return MPIDI_Win_local_accumulate(
            origin_addr,
            origin_count,
            origin_datatype,
            target_count,
            target_addr,
            target_datatype,
            op
        );
    }
    return MPID_Win_queue_complex(
        win_ptr,
        MPIDI_RMA_OP_GET_ACCUMULATE,
        const_cast<void*>(origin_addr),
        origin_count,
        origin_datatype,
        result_addr,
        result_count,
        result_datatype,
        target_rank,
        target_disp,
        target_count,
        target_datatype,
        op,
        nullptr
    );
}


MPI_RESULT
MPID_Win_Raccumulate(
    _In_opt_ const void *origin_addr,
    _In_range_(>=, 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_range_(>=, 0) int target_rank,
    _In_range_(>=, 0) MPI_Aint target_disp,
    _In_range_(>=, 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
)
{
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPI_RESULT mpi_errno;
    MPID_Request* sreq;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo( origin_count, &dt_contig, &dt_true_lb )
        );

    if (data_sz == 0 || target_rank == MPI_PROC_NULL)
    {
        MPID_Request* preq = MPIDI_Request_create_completed_req();
        if (preq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }

        *request = preq;
        return MPI_SUCCESS;
    }

    if(win_ptr->dev.isShm)
    {
        MPI_Aint size_target;
        int target_disp_unit = 0;
        void *base_target = win_ptr->base;

        sreq = MPIDI_Request_create_completed_req();
        if (sreq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }

        MPID_Win_shared_query(win_ptr, target_rank, &size_target, &target_disp_unit, &base_target);

        char *target_addr = static_cast<char *>(base_target) + target_disp_unit * target_disp;
        mpi_errno = MPIDI_Win_local_accumulate(
            origin_addr,
            origin_count,
            origin_datatype,
            target_count,
            target_addr,
            target_datatype,
            op
            );

        sreq->status.MPI_ERROR = mpi_errno;
        *request = sreq;
        return mpi_errno;
    }

    if (target_rank == win_ptr->comm_ptr->rank)
    {
        sreq = MPIDI_Request_create_completed_req();
        if (sreq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        char *target_addr = static_cast<char *>(win_ptr->base) + win_ptr->disp_unit * target_disp;
        mpi_errno = MPIDI_Win_local_accumulate(
            origin_addr,
            origin_count,
            origin_datatype,
            target_count,
            target_addr,
            target_datatype,
            op
            );
        sreq->status.MPI_ERROR = mpi_errno;
        *request = sreq;
        return mpi_errno;
    }

    sreq = MPID_Request_create(MPID_REQUEST_RMA_PUT);
    if (sreq == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    sreq->init_pkt(MPIDI_CH3_PKT_ACCUMULATE);
    sreq->dev.target_rank = target_rank;
    sreq->dev.source_win_handle = win_ptr->handle;

    mpi_errno = MPID_Win_queue(
        win_ptr,
        MPIDI_RMA_OP_ACCUMULATE,
        const_cast<void*>(origin_addr),
        origin_count,
        origin_datatype,
        target_rank,
        target_disp,
        target_count,
        target_datatype,
        op,
        sreq
        );
    if (mpi_errno != MPI_SUCCESS)
    {
        sreq->Release();
        return mpi_errno;
    }
    *request = sreq;
    return mpi_errno;
}


MPI_RESULT
MPID_Win_Rget_accumulate(
    _In_opt_ const void* origin_addr,
    _In_range_(>= , 0) int origin_count,
    _In_ TypeHandle origin_datatype,
    _In_opt_ void* result_addr,
    _In_range_(>= , 0) int result_count,
    _In_ TypeHandle result_datatype,
    _In_range_(>= , 0) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_range_(>= , 0) int target_count,
    _In_ TypeHandle target_datatype,
    _In_ MPI_Op op,
    _In_ MPID_Win *win_ptr,
    _Outptr_ MPID_Request** request
)
{
    MPI_RESULT mpi_errno;
    bool dt_contig;
    MPI_Aint dt_true_lb;
    MPIDI_msg_sz_t data_sz = fixme_cast<MPIDI_msg_sz_t>(
        origin_datatype.GetSizeAndInfo(origin_count, &dt_contig, &dt_true_lb)
        );

    if (data_sz == 0 || target_rank == MPI_PROC_NULL)
    {
        MPID_Request* preq = MPIDI_Request_create_completed_req();
        if (preq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }

        *request = preq;
        return MPI_SUCCESS;
    }

    if (target_rank == win_ptr->comm_ptr->rank)
    {
        char *target_addr = static_cast<char *>(win_ptr->base) + win_ptr->disp_unit * target_disp;
        MPID_Request* preq = MPIDI_Request_create_completed_req();
        if (preq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        *request = preq;

        mpi_errno = MPIR_Localcopy((char *)win_ptr->base +
            win_ptr->disp_unit * target_disp,
            target_count, target_datatype,
            result_addr, result_count,
            result_datatype);
        if (mpi_errno != MPI_SUCCESS)
        {
            preq->status.MPI_ERROR = mpi_errno;
            return mpi_errno;
        }

        mpi_errno = MPIDI_Win_local_accumulate(
            origin_addr,
            origin_count,
            origin_datatype,
            target_count,
            target_addr,
            target_datatype,
            op
        );
        preq->status.MPI_ERROR = mpi_errno;
        return mpi_errno;
    }

    MPID_Request* rreq = MPID_Request_create(MPID_REQUEST_RMA_GET);
    if (rreq == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    rreq->dev.target_rank = target_rank;
    rreq->dev.source_win_handle = win_ptr->handle;


    mpi_errno = MPID_Win_queue_complex(
        win_ptr,
        MPIDI_RMA_OP_GET_ACCUMULATE,
        const_cast<void*>(origin_addr),
        origin_count,
        origin_datatype,
        result_addr,
        result_count,
        result_datatype,
        target_rank,
        target_disp,
        target_count,
        target_datatype,
        op,
        rreq
    );
    if (mpi_errno != MPI_SUCCESS)
    {
        rreq->Release();
        return mpi_errno;
    }
    *request = rreq;
    return mpi_errno;
}


MPI_RESULT
MPID_Win_Compare_and_swap(
    _In_ const void *origin_addr,
    _In_ const void *compare_addr,
    _In_ void *result_addr,
    _In_ TypeHandle datatype,
    _In_range_(>= , 0) int target_rank,
    _In_range_(>= , 0) MPI_Aint target_disp,
    _In_ MPID_Win *win_ptr
    )
{
    MPI_RESULT mpi_errno;
    if (target_rank == win_ptr->comm_ptr->rank)
    {
        const MPID_Win* win;
        MPI_Datatype hType = datatype.GetMpiHandle();

        MPI_Aint datatypeSize = static_cast<MPI_Aint>(datatype.GetSize());

        const BYTE* window_buff = 
            static_cast<BYTE*>(win_ptr->base) + win_ptr->disp_unit * target_disp;

        bool equal = true;
        for (MPI_Aint i = 0; i < datatypeSize; i++)
        {
            if (static_cast<const BYTE*>(compare_addr)[i] ^ window_buff[i])
            {
                equal = false;
                break;
            }
        }

        mpi_errno = MPIR_Localcopy(window_buff, 1, datatype, result_addr, 1, datatype);
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }

        if (equal)
        {
            return MPIR_Localcopy(origin_addr, 1, datatype, (char *)win_ptr->base +
                win_ptr->disp_unit * target_disp, 1, datatype);
        }
        else
        {
            return MPI_SUCCESS;
        }
    }

    //
    // instead of origin buffer, create a new buffer that contains origin value
    // and compare value
    //
    MPI_Aint datatypeSize = static_cast<MPI_Aint>(datatype.GetSize());

    void* temp_buff = MPIU_Malloc(datatypeSize * 2);
    if (temp_buff == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }
    mpi_errno = MPIR_Localcopy(origin_addr, 1, datatype, temp_buff, 1, datatype);
    if (mpi_errno != MPI_SUCCESS)
    {
        MPIU_Free(temp_buff);
        return mpi_errno;
    }
    mpi_errno = MPIR_Localcopy(compare_addr, 1, datatype, 
        static_cast<BYTE*>(temp_buff)+ datatypeSize, 1, datatype);
    if (mpi_errno != MPI_SUCCESS)
    {
        MPIU_Free(temp_buff);
        return mpi_errno;
    }

    return MPID_Win_queue_complex(
        win_ptr,
        MPIDI_RMA_OP_COMPARE_AND_SWAP,
        temp_buff,
        2,
        datatype,
        result_addr,
        1,
        datatype,
        target_rank,
        target_disp,
        2,
        datatype,
        MPI_OP_NULL,
        nullptr
    );
}


MPI_RESULT MPID_Win_test(const MPID_Win *win_ptr, int *flag)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    mpi_errno = MPID_Progress_pump();
    if (mpi_errno != MPI_SUCCESS)
        return mpi_errno;

    *flag = (win_ptr->dev.my_counter) ? 0 : 1;

    return mpi_errno;
}


static void copy_rma_datatype_info(MPIDI_RMA_dtype_info* dtype_info, MPID_Datatype* dtp)
{
    dtype_info->is_contig = dtp->is_contig;
    dtype_info->max_contig_blocks = dtp->max_contig_blocks;
    dtype_info->size = dtp->size;
    dtype_info->extent = dtp->extent;
    dtype_info->dataloop_size = dtp->dataloop_size;
    dtype_info->dataloop_depth = dtp->dataloop_depth;
    dtype_info->eltype = dtp->eltype;
    dtype_info->dataloop = dtp->dataloop;
    dtype_info->ub = dtp->ub;
    dtype_info->lb = dtp->lb;
    dtype_info->true_ub = dtp->true_ub;
    dtype_info->true_lb = dtp->true_lb;
    dtype_info->has_sticky_ub = dtp->has_sticky_ub;
    dtype_info->has_sticky_lb = dtp->has_sticky_lb;
}


static void TranslateReqRmaFlagsToPkt(
    _In_ UINT16 rmaFlags,
    _Inout_ UINT16* pktRmaFlags
    )
{
    if (rmaFlags & MPIDI_RMA_FLAG_LAST_OP)
    {
        *pktRmaFlags |= MPIDI_CH3_PKT_FLAG_RMA_LAST_OP;
    }
    if (rmaFlags & MPIDI_RMA_FLAG_LOCK_RELEASE)
    {
        *pktRmaFlags |= MPIDI_CH3_PKT_FLAG_RMA_UNLOCK;
    }
}


static MPI_RESULT
MPIDI_CH3I_Send_rma_msg(
    _In_ const MPIDI_RMA_ops *rma_op,
    _In_ MPID_Win *win_ptr,
    _In_ MPI_Win source_win_handle,
    _In_ MPI_Win target_win_handle,
    _In_ UINT16 rmaFlags,
    _In_ MPIDI_RMA_dtype_info *dtype_info,
    _In_ bool request_allocated,
    _Outptr_ void **dataloop,
    _Inout_ MPID_Request **request)
{
    int origin_dt_derived, target_dt_derived, origin_type_size;
    MPID_Datatype *target_dtp = NULL, *origin_dtp = NULL;
    StackGuardPointer<void> dloop;

    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPID_Request* sreq;
    if (!request_allocated)
    {
        sreq = MPID_Request_create(MPID_REQUEST_RMA_PUT);
        if (sreq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
        if (rma_op->type == MPIDI_RMA_OP_PUT)
        {
            sreq->init_pkt(MPIDI_CH3_PKT_PUT);
        }
        else
        {
            sreq->init_pkt(MPIDI_CH3_PKT_ACCUMULATE);
        }
    }
    else
    {
        sreq = *request;
    }

    MPIDI_CH3_Pkt_put_accum_t* pa_pkt = &sreq->get_pkt()->put_accum;
    pa_pkt->win.disp = rma_op->target_disp;
    pa_pkt->win.count = rma_op->target_count;
    pa_pkt->win.datatype = rma_op->target_datatype.GetMpiHandle();
    pa_pkt->dataloop_size = 0;
    pa_pkt->win.target = target_win_handle;
    pa_pkt->win.source = source_win_handle;
    pa_pkt->op = rma_op->op;

    TranslateReqRmaFlagsToPkt(rmaFlags, &pa_pkt->flags);

    if( rma_op->origin_datatype.IsPredefined() == false )
    {
        origin_dt_derived = 1;
        origin_dtp = rma_op->origin_datatype.Get();
    }
    else
    {
        origin_dt_derived = 0;
    }

    if (rma_op->target_datatype.IsPredefined() == false)
    {
        target_dt_derived = 1;
        target_dtp = rma_op->target_datatype.Get();
    }
    else
    {
        target_dt_derived = 0;
    }

    if (target_dt_derived)
    {
        /* derived datatype on target. fill derived datatype info */
        copy_rma_datatype_info(dtype_info, target_dtp);

        dloop = MPIU_Malloc(target_dtp->dataloop_size);
        if (dloop == nullptr)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        memcpy(dloop, target_dtp->dataloop, target_dtp->dataloop_size);

        pa_pkt->dataloop_size = target_dtp->dataloop_size;
    }

    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, rma_op->target_rank);

    origin_type_size = fixme_cast<int>(rma_op->origin_datatype.GetSize());

    if (!origin_dt_derived)
    {
        /* basic datatype on origin */
        if (!target_dt_derived)
        {
            /* basic datatype on target */
            sreq->add_send_iov(rma_op->origin_addr, rma_op->origin_count * origin_type_size);
        }
        else
        {
            /* derived datatype on target */
            sreq->add_send_iov(dtype_info, sizeof(*dtype_info));
            sreq->add_send_iov(dloop, target_dtp->dataloop_size);
            sreq->add_send_iov(rma_op->origin_addr, rma_op->origin_count * origin_type_size);
        }
    }
    else
    {
        /* derived datatype on origin */

        if (!target_dt_derived)
        {
            /* basic datatype on target */
        }
        else
        {
            /* derived datatype on target */
            sreq->add_send_iov(dtype_info, sizeof(*dtype_info));
            sreq->add_send_iov(dloop, target_dtp->dataloop_size);
        }

        //
        // A reference was taken when the rma_op gets created. It will get
        // released once the sreq gets released
        //
        sreq->dev.datatype = rma_op->origin_datatype;

        sreq->dev.segment_ptr = MPID_Segment_alloc();
        /* if (!sreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
        MPID_Segment_init(rma_op->origin_addr, rma_op->origin_count,
            rma_op->origin_datatype.GetMpiHandle(),
            sreq->dev.segment_ptr, 0);
        sreq->dev.segment_first = 0;
        sreq->dev.segment_size = rma_op->origin_count * origin_type_size;

        /* On the initial load of a send iov req, set the OnFinal action (null
        for point-to-point) */
        sreq->dev.OnFinal = 0;

        mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq);
        ON_ERROR_FAIL(mpi_errno);
    }

    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);

    if (target_dt_derived)
    {
        target_dtp->Release();
    }

fn_exit:
    *dataloop = dloop.detach();
    *request = sreq;
    return mpi_errno;
fn_fail:
    if (!request_allocated)
    {
        sreq->Release();
        sreq = NULL;
    }
    else
    {
        sreq->status.MPI_ERROR = mpi_errno;
        sreq->signal_completion();
    }
    goto fn_exit;
}


static MPI_RESULT
MPIDI_CH3I_Recv_rma_msg(
    _In_ MPIDI_RMA_ops *rma_op,
    _In_ MPID_Win *win_ptr,
    _In_ MPI_Win source_win_handle,
    _In_ MPI_Win target_win_handle,
    _In_ UINT16 rmaFlags,
    _In_ MPIDI_RMA_dtype_info *dtype_info,
    _In_ bool request_allocated,
    _Outptr_ void **dataloop,
    _Inout_ MPID_Request **request)
{
    MPI_RESULT mpi_errno;
    MPID_Datatype *dtp;
    StackGuardPointer<void> dloop;
    MPID_Request* rreq;

    /* create a request, store the origin buf, cnt, datatype in it,
       and pass a handle to it in the get packet. When the get
       response comes from the target, it will contain the request
       handle. */
    if (!request_allocated)
    {
        rreq = MPID_Request_create(MPID_REQUEST_RMA_GET);
        if (rreq == NULL)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto rreq_fail;
        }
    }
    else
    {
        rreq = *request;
    }

    rreq->dev.user_buf = rma_op->origin_addr;
    rreq->dev.user_count = rma_op->origin_count;
    rreq->dev.datatype = rma_op->origin_datatype;
    rreq->dev.target_win_handle = MPI_WIN_NULL;
    rreq->dev.source_win_handle = source_win_handle;

    //
    // Create a send request for this get_pkt
    //
    MPID_Request* sreq = MPID_Request_create(MPID_REQUEST_SEND);
    if (sreq == NULL)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto sreq_fail;
    }

    sreq->init_pkt(MPIDI_CH3_PKT_GET);
    MPIDI_CH3_Pkt_get_t* get_pkt = &sreq->get_pkt()->get;

    get_pkt->win.disp = rma_op->target_disp;
    get_pkt->win.count = rma_op->target_count;
    get_pkt->win.datatype = rma_op->target_datatype.GetMpiHandle();
    get_pkt->request_handle = rreq->handle;
    get_pkt->win.target = target_win_handle;
    get_pkt->win.source = source_win_handle;

    TranslateReqRmaFlagsToPkt(rmaFlags, &get_pkt->flags);

    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, rma_op->target_rank);

    //
    // We take an extra reference because GET_RESP receive completion processing
    // will release a reference.  This reference is for the request_handle stored
    // in the packet.
    //
    rreq->AddRef();

    if( rma_op->target_datatype.IsPredefined() == false )
    {
        //
        // In the case of derived datatype on target, we fill derived datatype
        // info and send it along with get_pkt.
        //
        dtp = rma_op->target_datatype.Get();
        copy_rma_datatype_info(dtype_info, dtp);

        dloop = MPIU_Malloc( dtp->dataloop_size );
        if( dloop == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        memcpy(dloop, dtp->dataloop, dtp->dataloop_size);
        get_pkt->dataloop_size = dtp->dataloop_size;

        sreq->add_send_iov(dtype_info, sizeof(*dtype_info));
        sreq->add_send_iov(dloop, dtp->dataloop_size);

        //
        // Release the target datatype. A reference was taken earlier when
        // the rma_op is constructed.
        //
        dtp->Release();
    }

    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);

    while (win_ptr->dev.error_found == false && sreq->test_complete() == false)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

    sreq->Release();

fn_exit:
    *dataloop = dloop.detach();
    *request = rreq;
    return mpi_errno;

fn_fail:
    sreq->Release();

    //
    // Release the reference taken for the GET_RESP.
    //
    if (!request_allocated)
    {
        rreq->Release();
    }
    else
    {
        rreq->status.MPI_ERROR = mpi_errno;
        rreq->signal_completion();
    }

sreq_fail:
    //
    // Release the request to free it.
    //
    if (!request_allocated)
    {
        rreq->Release();
        rreq = NULL;
    }

rreq_fail:
    goto fn_exit;
}


static MPI_RESULT
MPIDI_CH3I_Sendrecv_rma_msg(
    _In_ const MPIDI_RMA_ops *rma_op,
    _In_ MPID_Win *win_ptr,
    _In_ MPI_Win source_win_handle,
    _In_ MPI_Win target_win_handle,
    _In_ UINT16 rmaFlags,
    _In_ MPIDI_RMA_dtype_info *dtype_info,
    _In_ bool request_allocated,
    _Outptr_ void **dataloop,
    _Inout_ MPID_Request **request)
{
    MPI_RESULT mpi_errno;
    const MPID_Datatype *dtp;
    StackGuardPointer<void> dloop;
    MPID_Request* rreq;

    /* create a request, store the result buf, cnt, datatype in it,
    and pass a handle to it in the get packet. When the get
    response comes from the target, it will contain the request
    handle. */
    if (!request_allocated)
    {
        rreq = MPID_Request_create(MPID_REQUEST_RMA_GET);
        if (rreq == NULL)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto rreq_fail;
        }
    }
    else
    {
        rreq = *request;
    }

    rreq->dev.user_buf = rma_op->result_addr;
    rreq->dev.user_count = rma_op->result_count;
    rreq->dev.datatype = rma_op->result_datatype;
    rreq->dev.target_win_handle = MPI_WIN_NULL;
    rreq->dev.source_win_handle = source_win_handle;

    //
    // Create a send request for this get_pkt
    //
    MPID_Request* sreq = MPID_Request_create(MPID_REQUEST_SEND);
    if (sreq == NULL)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto sreq_fail;
    }

    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, rma_op->target_rank);

    if (rma_op->type == MPIDI_RMA_OP_GET_ACCUMULATE)
    {
        sreq->init_pkt(MPIDI_CH3_PKT_GET_ACCUMULATE);
        MPIDI_CH3_Pkt_get_accum_t* get_accum_pkt = &sreq->get_pkt()->get_accum;

        get_accum_pkt->win.disp = rma_op->target_disp;
        get_accum_pkt->win.count = rma_op->target_count;
        get_accum_pkt->win.datatype = rma_op->target_datatype.GetMpiHandle();
        get_accum_pkt->dataloop_size = 0;

        get_accum_pkt->request_handle = rreq->handle;
        get_accum_pkt->win.target = target_win_handle;
        get_accum_pkt->win.source = source_win_handle;
        get_accum_pkt->op = rma_op->op;

        TranslateReqRmaFlagsToPkt(rmaFlags, &get_accum_pkt->flags);

        //
        // We take an extra reference because GET_RESP receive completion processing
        // will release a reference.  This reference is for the request_handle stored
        // in the packet.
        //
        rreq->AddRef();

        int origin_dt_derived, target_dt_derived;
        MPID_Datatype *target_dtp = nullptr, *origin_dtp = nullptr;

        int origin_type_size = fixme_cast<int>(rma_op->origin_datatype.GetSize());

        if (rma_op->origin_datatype.IsPredefined() == false)
        {
            origin_dt_derived = 1;
            origin_dtp = rma_op->origin_datatype.Get();
        }
        else
        {
            origin_dt_derived = 0;
        }

        if (rma_op->target_datatype.IsPredefined() == false)
        {
            target_dt_derived = 1;
            target_dtp = rma_op->target_datatype.Get();
        }
        else
        {
            target_dt_derived = 0;
        }

        if (target_dt_derived)
        {
            //
            // In the case of derived datatype on target, we fill derived datatype
            // info and send it along with get_pkt.
            //
            copy_rma_datatype_info(dtype_info, target_dtp);

            dloop = MPIU_Malloc(target_dtp->dataloop_size);
            if (dloop == nullptr)
            {
                mpi_errno = MPIU_ERR_NOMEM();
                goto fn_fail;
            }

            memcpy(dloop, target_dtp->dataloop, target_dtp->dataloop_size);
            get_accum_pkt->dataloop_size = target_dtp->dataloop_size;

            //
            // Release the target datatype. A reference was taken earlier when
            // the rma_op is constructed.
            //
            target_dtp->Release();
        }


        if (!origin_dt_derived)
        {
            //
            // basic datatype on origin
            //
            if (!target_dt_derived)
            {
                //
                // basic datatype on target
                //
                sreq->add_send_iov(rma_op->origin_addr, rma_op->origin_count * origin_type_size);
            }
            else
            {
                //
                // derived datatype on target
                //
                sreq->add_send_iov(dtype_info, sizeof(*dtype_info));
                sreq->add_send_iov(dloop, target_dtp->dataloop_size);
                sreq->add_send_iov(rma_op->origin_addr, rma_op->origin_count * origin_type_size);
            }
        }
        else
        {
            //
            // derived datatype on origin
            //
            if (target_dt_derived)
            {
                //
                // derived datatype on target
                //
                sreq->add_send_iov(dtype_info, sizeof(*dtype_info));
                sreq->add_send_iov(dloop, target_dtp->dataloop_size);
            }

            //
            // A reference was taken when the rma_op gets created. It will get
            // released once the sreq gets released
            //
            sreq->dev.datatype = rma_op->origin_datatype;

            sreq->dev.segment_ptr = MPID_Segment_alloc();
            if (sreq->dev.segment_ptr == nullptr)
            {
                mpi_errno = MPIU_ERR_NOMEM();
                goto fn_fail;
            }

            MPID_Segment_init(rma_op->origin_addr, rma_op->origin_count,
                rma_op->origin_datatype.GetMpiHandle(),
                sreq->dev.segment_ptr, 0);
            sreq->dev.segment_first = 0;
            sreq->dev.segment_size = rma_op->origin_count * origin_type_size;

            //
            // On the initial load of a send iov req, set the OnFinal action (null
            // for point-to-point)
            //
            sreq->dev.OnFinal = 0;

            mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq);
            ON_ERROR_FAIL(mpi_errno);
        }
    }
    else if (rma_op->type == MPIDI_RMA_OP_COMPARE_AND_SWAP)
    {
        sreq->init_pkt(MPIDI_CH3_PKT_COMPARE_AND_SWAP);
        MPIDI_CH3_Pkt_compare_and_swap_t* compare_and_swap_pkt = &sreq->get_pkt()->compare_and_swap;

        compare_and_swap_pkt->win.disp = rma_op->target_disp;
        compare_and_swap_pkt->win.count = rma_op->target_count;
        compare_and_swap_pkt->win.datatype = rma_op->target_datatype.GetMpiHandle();

        compare_and_swap_pkt->request_handle = rreq->handle;
        compare_and_swap_pkt->win.target = target_win_handle;
        compare_and_swap_pkt->win.source = source_win_handle;

        TranslateReqRmaFlagsToPkt(rmaFlags, &compare_and_swap_pkt->flags);

        //
        // We take an extra reference because GET_RESP receive completion processing
        // will release a reference.  This reference is for the request_handle stored
        // in the packet.
        //
        rreq->AddRef();

        int origin_type_size = fixme_cast<int>(rma_op->origin_datatype.GetSize());
        sreq->add_send_iov(rma_op->origin_addr, rma_op->origin_count * origin_type_size);
    }

    mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
    ON_ERROR_FAIL(mpi_errno);

    while (win_ptr->dev.error_found == false && sreq->test_complete() == false)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

    sreq->Release();

fn_exit:
    *dataloop = dloop.detach();
    *request = rreq;
    return mpi_errno;

fn_fail:
    sreq->Release();

sreq_fail:
    //
    // Release the request to free it.
    //
    if (!request_allocated)
    {
        rreq->Release();
        rreq = nullptr;
    }
    else
    {
        rreq->status.MPI_ERROR = mpi_errno;
        rreq->signal_completion();
    }

rreq_fail:
    goto fn_exit;
}


static void FreeRmaOpsList(_In_ MPID_Win* win_ptr)
{
    MpiLockEnter(&win_ptr->winLock);

    int size = win_ptr->comm_ptr->remote_size;
    for (int i = 0; i < size; i++)
    {
        win_ptr->dev.requested_ops_count[i] = 0;
    }

    MPIDI_RMA_ops *curr_ptr = win_ptr->dev.rma_ops_list;
    MPIDI_RMA_ops *next_ptr;
    while (curr_ptr != nullptr)
    {
        next_ptr = curr_ptr->next;
        if (curr_ptr->type == MPIDI_RMA_OP_COMPARE_AND_SWAP)
        {
            MPIU_Free(curr_ptr->origin_addr);
        }
        MPIU_Free(curr_ptr);
        curr_ptr = next_ptr;
    }
    win_ptr->dev.rma_ops_list = nullptr;

    MpiLockLeave(&win_ptr->winLock);
}


static void FreeRmaOpsTargetList(
    _In_ MPID_Win* win_ptr,
    _In_ int target_rank
    )
{
    MpiLockEnter(&win_ptr->winLock);

    win_ptr->dev.requested_ops_count[target_rank] = 0;

    MPIDI_RMA_ops *curr_ptr = win_ptr->dev.rma_ops_list;
    MPIDI_RMA_ops *next_ptr;
    MPIDI_RMA_ops *prev_ptr = nullptr;
    while (curr_ptr != nullptr)
    {
        next_ptr = curr_ptr->next;
        if (curr_ptr->target_rank == target_rank)
        {
            if (curr_ptr->type == MPIDI_RMA_OP_COMPARE_AND_SWAP)
            {
                MPIU_Free(curr_ptr->origin_addr);
            }
            MPIU_Free(curr_ptr);
            curr_ptr = next_ptr;
            if (prev_ptr == nullptr)
            {
                win_ptr->dev.rma_ops_list = curr_ptr;
            }
            else
            {
                prev_ptr->next = next_ptr;
            }
        }
        else
        {
            prev_ptr = curr_ptr;
            curr_ptr = next_ptr;
        }
    }

    MpiLockLeave(&win_ptr->winLock);
}


static MPIDI_RMA_ops* GetNextOpToTarget(
    _In_ MPIDI_RMA_ops* pRmaOp,
    _In_ int dest
    )
{
    MPIDI_RMA_ops* result = pRmaOp;
    while (result != nullptr)
    {
        if (result->target_rank == dest)
        {
            break;
        }
        result = result->next;
    }
    return result;
}


static MPIDI_RMA_ops* GetNextOp(
    _In_ MPIDI_RMA_ops* pRmaOp
    )
{
    MPIDI_RMA_ops* result = pRmaOp;
    while (result != nullptr)
    {
        if (result->type != MPIDI_RMA_OP_LOCK)
        {
            break;
        }
        result = result->next;
    }
    return result;
}


static MPI_RESULT WaitForRmaRequestsComplete(
    _In_ const MPID_Win* win_ptr,
    _In_ MPID_Request** ppRequests,
    _In_ const bool* pRequestAttached,
    _In_ int numRequests
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    for (int i = 0; i<numRequests; i++)
    {
        if (ppRequests[i] == NULL)
        {
            continue;
        }
        //
        // wait for either all requests completion or the signal from the remote side 
        // that RMA operation resulted in an error
        //
        while (win_ptr->dev.error_found == false && ppRequests[i]->test_complete() == false)
        {
            mpi_errno = MPID_Progress_wait();
            ON_ERROR_FAIL(mpi_errno);
        }
        if (win_ptr->dev.error_found)
        {
            ppRequests[i]->signal_completion();
        }
        else
        {
            mpi_errno = ppRequests[i]->status.MPI_ERROR;
            ON_ERROR_FAIL(mpi_errno);
        }

        if (pRequestAttached[i] == false)
        {
            ppRequests[i]->Release();
            if (ppRequests[i]->kind == MPID_REQUEST_RMA_GET)
            {
                ppRequests[i]->Release();
            }
            ppRequests[i] = NULL;
        }
    }
fn_fail:
    return mpi_errno;
}


static MPI_RESULT IssueRmaOp(
    _In_ MPIDI_RMA_ops* pRmaOp,
    _In_ MPID_Win* win_ptr,
    _In_ UINT16 rmaFlags,
    _In_ MPIDI_RMA_dtype_info* pDtypeInfo,
    _In_ bool fRequestAttached,
    _Outptr_ void** ppDataloop,
    _Inout_ MPID_Request** ppRequest
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    
    int dest = pRmaOp->target_rank;
    MPI_Win source_win_handle = win_ptr->handle;
    MPI_Win target_win_handle = win_ptr->dev.all_win_handles[dest];

    switch (pRmaOp->type)
    {
    case (MPIDI_RMA_OP_PUT):  /* same as accumulate */
    case (MPIDI_RMA_OP_ACCUMULATE):
        win_ptr->dev.pt_rma_puts_accs[dest]++;
        mpi_errno = MPIDI_CH3I_Send_rma_msg(pRmaOp, win_ptr, source_win_handle,
            target_win_handle, rmaFlags, pDtypeInfo,
            fRequestAttached, ppDataloop, ppRequest);
        ON_ERROR_FAIL(mpi_errno);
        win_ptr->dev.remote_ops_done[dest] = 0;
        break;
    case (MPIDI_RMA_OP_GET):
        mpi_errno = MPIDI_CH3I_Recv_rma_msg(pRmaOp, win_ptr, source_win_handle,
            target_win_handle, rmaFlags, pDtypeInfo,
            fRequestAttached, ppDataloop, ppRequest);
        ON_ERROR_FAIL(mpi_errno);
        win_ptr->dev.remote_ops_done[dest] = 1;
        break;
    case (MPIDI_RMA_OP_GET_ACCUMULATE):
    case (MPIDI_RMA_OP_COMPARE_AND_SWAP):
        win_ptr->dev.pt_rma_puts_accs[dest]++;
        mpi_errno = MPIDI_CH3I_Sendrecv_rma_msg(pRmaOp, win_ptr, source_win_handle,
            target_win_handle, rmaFlags, pDtypeInfo,
            fRequestAttached, ppDataloop, ppRequest);
        ON_ERROR_FAIL(mpi_errno);
        win_ptr->dev.remote_ops_done[dest] = 1;
        break;
    default:
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**winrmaop");
        ON_ERROR_FAIL(mpi_errno);
    }

fn_fail:
    return mpi_errno;
}


static MPI_RESULT MPIDI_CH3I_Do_passive_target_rma(MPID_Win *win_ptr, UINT16 lastOpFlags, int dest)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int i, nops;
    MPIDI_RMA_ops *curr_ptr;
    //
    // array of requests
    //
    StackGuardArray<MPID_Request*> requests;

    //
    // array of flags if request is attached to the operation
    //
    StackGuardArray<bool> request_attached;

    //
    // to store dataloops for each datatype
    //
    StackGuardArray<void*> dataloops;

    StackGuardArray<MPIDI_RMA_dtype_info> dtype_infos;

    MPIDI_RMA_ops *first_rma_op;

    first_rma_op = GetNextOpToTarget(win_ptr->dev.rma_ops_list, dest);
    if (first_rma_op->type == MPIDI_RMA_OP_LOCK)
    {
        first_rma_op = GetNextOpToTarget(first_rma_op->next, dest);
    }

    win_ptr->dev.remote_ops_done[dest] = 0;

    //
    // Check if any of the rma ops is a get. If so, move it
    // to the end of the list and do it last, in which case an rma done
    // pkt is not needed. If there is no get, rma done pkt is needed
    //
    curr_ptr = first_rma_op;
    MPIDI_RMA_ops *get_op_ptr = nullptr;
    bool getExists = false;
    while (curr_ptr != nullptr)
    {
        if (curr_ptr->type == MPIDI_RMA_OP_GET ||
            curr_ptr->type == MPIDI_RMA_OP_GET_ACCUMULATE ||
            curr_ptr->type == MPIDI_RMA_OP_COMPARE_AND_SWAP)
        {
            getExists = true;
            get_op_ptr = curr_ptr;
        }
        curr_ptr = GetNextOpToTarget(curr_ptr->next, dest);
    }

    if (getExists && get_op_ptr->next != nullptr)
    {
        MpiLockEnter(&win_ptr->winLock);

        MPIDI_RMA_ops* prev = win_ptr->dev.rma_ops_list;
        MPIDI_RMA_ops* last = nullptr;
        if (prev == get_op_ptr)
        {
            if (prev->next != nullptr)
            {
                win_ptr->dev.rma_ops_list = win_ptr->dev.rma_ops_list->next;
                last = win_ptr->dev.rma_ops_list;
            }
        }
        else
        {
            while (prev->next != get_op_ptr)
            {
                prev = prev->next;
            }
            prev->next = prev->next->next;
            last = prev->next;
        }
        while (last->next != nullptr)
        {
            last = last->next;
        }
        last->next = get_op_ptr;
        get_op_ptr->next = nullptr;

        MpiLockLeave(&win_ptr->winLock);
    }

    first_rma_op = GetNextOpToTarget(win_ptr->dev.rma_ops_list, dest);
    if (first_rma_op->type == MPIDI_RMA_OP_LOCK)
    {
        first_rma_op = GetNextOpToTarget(first_rma_op->next, dest);
    }

    curr_ptr = first_rma_op;

    nops = win_ptr->dev.requested_ops_count[dest];

    requests = new MPID_Request*[nops];
    if( requests == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    request_attached = new bool[nops];
    if (request_attached == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    i = 0;
    while (curr_ptr != NULL)
    {
        requests[i] = curr_ptr->request;
        if (curr_ptr->request == nullptr)
        {
            request_attached[i++] = false;
        }
        else
        {
            request_attached[i++] = true;
        }
        curr_ptr = GetNextOpToTarget(curr_ptr->next, dest);
    }

    dtype_infos = new MPIDI_RMA_dtype_info[nops];
    if( dtype_infos == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    dataloops = new void*[nops];
    if( dataloops == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    for (i=0; i<nops; i++)
    {
        dataloops[i] = NULL;
    }

    i = 0;
    curr_ptr = first_rma_op;
    MPIU_Assert(curr_ptr != NULL);

    UINT16 rmaFlags = 0;
    while (curr_ptr != NULL && !win_ptr->dev.error_found)
    {
        if (i == nops - 1)
        {
            rmaFlags = lastOpFlags | MPIDI_RMA_FLAG_LAST_OP;
        }

        mpi_errno = IssueRmaOp(curr_ptr, win_ptr, rmaFlags,
            &dtype_infos[i], request_attached[i], &dataloops[i], &requests[i]);
        ON_ERROR_FAIL(mpi_errno);

        i++;
        curr_ptr = GetNextOpToTarget(curr_ptr->next, dest);
    }

    mpi_errno = WaitForRmaRequestsComplete(win_ptr, requests, request_attached, nops);
    ON_ERROR_FAIL(mpi_errno);

    for (i=0; i<nops; i++)
    {
        if (dataloops[i] != NULL)
        {
            MPIU_Free(dataloops[i]);
        }
    }

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


static MPI_RESULT IssueRmaOps(MPID_Win *win_ptr, UINT16 lastOpFlags)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int i, nops;
    MPIDI_RMA_ops *curr_ptr;

    //
    // array of requests
    //
    StackGuardArray<MPID_Request*> requests;

    //
    // array of flags if request is attached to the operation
    //
    StackGuardArray<bool> request_attached;

    //
    // to store dataloops for each datatype
    //
    StackGuardArray<void*> dataloops;

    //
    // number of ops issued to target
    //
    StackGuardArray<int> issuedOps;
    
    StackGuardArray<MPIDI_RMA_dtype_info> dtype_infos;

    //
    // find the first operation in the queue that is not a lock
    //
    MPIDI_RMA_ops *first_rma_op = win_ptr->dev.rma_ops_list;
    while (first_rma_op != nullptr && first_rma_op->type == MPIDI_RMA_OP_LOCK)
    {
        first_rma_op = first_rma_op->next;
    }

    //
    // Reset the done flags for all the targets to which
    // the operations have been issued (except self) to 0
    // (local rma operations should be completed).
    // Also count the total number of ops requested
    //
    int size = win_ptr->comm_ptr->remote_size;
    nops = 0;
    issuedOps = static_cast<int*>(MPIU_Malloc(size * sizeof(int)));
    for (int dest = 0; dest < size; ++dest)
    {
        issuedOps[dest] = 0;
        if (dest == win_ptr->comm_ptr->rank)
        {
            continue;
        }
        nops += win_ptr->dev.requested_ops_count[dest];
        if (win_ptr->dev.requested_ops_count[dest] > 0)
        {
            win_ptr->dev.remote_ops_done[dest] = 0;
        }
    }

    if (nops == 0)
    {
        //
        // nothing to do
        //
        return MPI_SUCCESS;
    }

    curr_ptr = first_rma_op;
    if (curr_ptr == nullptr)
    {
        return MPI_SUCCESS;
    }

    requests = new MPID_Request*[nops];
    if (requests == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    request_attached = new bool[nops];
    if (request_attached == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    i = 0;
    while (curr_ptr != nullptr)
    {
        requests[i] = curr_ptr->request;
        if (curr_ptr->request == nullptr)
        {
            request_attached[i++] = false;
        }
        else
        {
            request_attached[i++] = true;
        }
        curr_ptr = GetNextOp(curr_ptr->next);
    }

    dtype_infos = new MPIDI_RMA_dtype_info[nops];
    if (dtype_infos == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    dataloops = new void*[nops];
    if (dataloops == nullptr)
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    for (i = 0; i<nops; i++)
    {
        dataloops[i] = NULL;
    }

    i = 0;
    curr_ptr = first_rma_op;

    UINT16 rmaFlags = 0;
    while (curr_ptr != NULL && !win_ptr->dev.error_found)
    {
        int dest = curr_ptr->target_rank;

        if (issuedOps[dest] == win_ptr->dev.requested_ops_count[dest] - 1)
        {
            rmaFlags = lastOpFlags | MPIDI_RMA_FLAG_LAST_OP;
        }

        mpi_errno = IssueRmaOp(curr_ptr, win_ptr, rmaFlags,
            &dtype_infos[i], request_attached[i], &dataloops[i], &requests[i]);
        ON_ERROR_FAIL(mpi_errno);

        i++;
        issuedOps[dest]++;
        MPIU_Assert(issuedOps[dest] <= win_ptr->dev.requested_ops_count[dest]);

        curr_ptr = GetNextOp(curr_ptr->next);
    }

    mpi_errno = WaitForRmaRequestsComplete(win_ptr, requests, request_attached, nops);
    ON_ERROR_FAIL(mpi_errno);

    for (i = 0; i<nops; i++)
    {
        if (dataloops[i] != NULL)
        {
            MPIU_Free(dataloops[i]);
        }
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


static MPI_RESULT MPIDI_CH3I_Send_lock_put_or_acc(
    _In_ MPID_Win *win_ptr,
    _In_ int dest,
    _In_ MPIDI_RMA_ops* lock_op,
    _In_ MPIDI_RMA_ops* rma_op
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int origin_dt_derived;
    MPID_Datatype *origin_dtp=NULL;
    int origin_type_size;

    int lock_type = lock_op->lock_type;

    MPID_Request* sreq = rma_op->request;
    bool requestPassed = true;
    if (sreq == nullptr)
    {
        requestPassed = false;
        sreq = MPID_Request_create(MPID_REQUEST_RMA_PUT);
        if (sreq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
    }

    if (rma_op->type == MPIDI_RMA_OP_PUT)
    {
        sreq->init_pkt(MPIDI_CH3_PKT_LOCK_PUT_UNLOCK);
    }
    else if (rma_op->type == MPIDI_RMA_OP_ACCUMULATE)
    {
        sreq->init_pkt(MPIDI_CH3_PKT_LOCK_ACCUM_UNLOCK);
    }
    win_ptr->dev.pt_rma_puts_accs[dest]++;

    MPIDI_CH3_Pkt_lock_put_accum_unlock_t* lu_pkt = &sreq->get_pkt()->lock_put_accum_unlock;
    lu_pkt->win.target = win_ptr->dev.all_win_handles[dest];
    lu_pkt->win.source = win_ptr->handle;
    lu_pkt->lock_type = lock_type;
    lu_pkt->win.disp = rma_op->target_disp;
    lu_pkt->win.count = rma_op->target_count;
    lu_pkt->win.datatype = rma_op->target_datatype.GetMpiHandle();
    lu_pkt->op = rma_op->op;
    lu_pkt->flags |= MPIDI_CH3_PKT_FLAG_RMA_LAST_OP | MPIDI_CH3_PKT_FLAG_RMA_UNLOCK;

    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, dest);

    if( rma_op->origin_datatype.IsPredefined() == false )
    {
        origin_dt_derived = 1;
        origin_dtp = rma_op->origin_datatype.Get();
    }
    else
    {
        origin_dt_derived = 0;
    }

    origin_type_size = fixme_cast<int>( rma_op->origin_datatype.GetSize() );

    if (!origin_dt_derived)
    {
        /* basic datatype on origin */
        sreq->add_send_iov(rma_op->origin_addr, rma_op->origin_count * origin_type_size);

        mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        /* derived datatype on origin */
        sreq->dev.datatype = rma_op->origin_datatype;
        /* this will cause the datatype to be freed when the request
           is freed. */

        sreq->dev.segment_ptr = MPID_Segment_alloc( );
        /* if (!sreq->dev.segment_ptr) { MPIU_ERR_POP(); } */
        MPID_Segment_init(rma_op->origin_addr, rma_op->origin_count,
                          rma_op->origin_datatype.GetMpiHandle(),
                          sreq->dev.segment_ptr, 0);
        sreq->dev.segment_first = 0;
        sreq->dev.segment_size = rma_op->origin_count * origin_type_size;

        /* On the initial load of a send iov req, set the OnFinal action (null
           for point-to-point) */
        sreq->dev.OnFinal = 0;
        mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq);
        ON_ERROR_FAIL(mpi_errno)

        mpi_errno = MPIDI_CH3_SendRequest(vc, sreq);
        ON_ERROR_FAIL(mpi_errno)
    }

    mpi_errno = MPIR_Wait(sreq);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = sreq->status.MPI_ERROR;
    ON_ERROR_FAIL(mpi_errno);

    if (!requestPassed)
    {
        sreq->Release();
    }

fn_exit:
    return mpi_errno;

fn_fail:
    if (!requestPassed)
    {
        sreq->Release();
        sreq = NULL;
    }
    else
    {
        sreq->status.MPI_ERROR = mpi_errno;
        sreq->signal_completion();
    }
    goto fn_exit;
}


static MPI_RESULT MPIDI_CH3I_Send_lock_get(
    _In_ MPID_Win *win_ptr,
    _In_ int dest,
    _In_ MPIDI_RMA_ops* lock_op,
    _In_ MPIDI_RMA_ops* rma_op
    )
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int lock_type;
    MPID_Request *rreq=NULL;
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_lock_get_unlock_t *lock_get_unlock_pkt = &upkt.lock_get_unlock;

    lock_type = lock_op->lock_type;

    /* create a request, store the origin buf, cnt, datatype in it,
       and pass a handle to it in the get packet. When the get
       response comes from the target, it will contain the request
       handle. */
    rreq = rma_op->request;
    bool requestPassed = true;
    if (rreq == nullptr)
    {
        requestPassed = false;
        rreq = MPID_Request_create(MPID_REQUEST_RMA_GET);
        if (rreq == nullptr)
        {
            return MPIU_ERR_NOMEM();
        }
    }

    rreq->dev.user_buf = rma_op->origin_addr;
    rreq->dev.user_count = rma_op->origin_count;
    rreq->dev.datatype = rma_op->origin_datatype;
    rreq->dev.target_win_handle = MPI_WIN_NULL;
    rreq->dev.source_win_handle = win_ptr->handle;
    rreq->set_rma_last_op();
    rreq->set_rma_unlock();

    //
    // N.B. This code sends the rreq handle value to enable referencing this request
    //      back when the response arrives.  However, the code does not take a
    //      reference to the receive request as it blocks here until a response arrives.
    //
    // BUGBUG: erezh - We should still take a reference to the receive request, cause we
    // will stop blocking on error and the response might still come back, referencing
    // the deallocated request.  However there is no good mechanism to release this ref.
    //
    MPIDI_Pkt_init(lock_get_unlock_pkt, MPIDI_CH3_PKT_LOCK_GET_UNLOCK);
    lock_get_unlock_pkt->win.target = win_ptr->dev.all_win_handles[dest];
    lock_get_unlock_pkt->win.source = win_ptr->handle;
    lock_get_unlock_pkt->lock_type = lock_type;
    lock_get_unlock_pkt->win.disp = rma_op->target_disp;
    lock_get_unlock_pkt->win.count = rma_op->target_count;
    lock_get_unlock_pkt->win.datatype = rma_op->target_datatype.GetMpiHandle();
    lock_get_unlock_pkt->request_handle = rreq->handle;
    lock_get_unlock_pkt->flags |= MPIDI_CH3_PKT_FLAG_RMA_LAST_OP | MPIDI_CH3_PKT_FLAG_RMA_UNLOCK;

    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, dest);

    //
    // We take an extra reference because GET_RESP receive completion processing
    // will release a reference.  This reference is for the request_handle stored
    // in the packet.
    //
    rreq->AddRef();

    mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
    ON_ERROR_FAIL(mpi_errno);

    //
    // now wait for the data to arrive 
    //
    while (!win_ptr->dev.error_found && rreq->test_complete() == false)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }
    if (win_ptr->dev.error_found)
    {
        rreq->signal_completion();
        rreq->Release();
    }
    else 
    {
        MPIU_Assert(rreq->test_complete() == true);
        mpi_errno = rreq->status.MPI_ERROR;
        ON_ERROR_FAIL(mpi_errno);
    }

    //
    // if origin datatype was a derived datatype, it will get freed when the rreq gets freed.
    //
    if (!requestPassed)
    {
        rreq->Release();
    }

fn_exit:
    return mpi_errno;

fn_fail:
    //
    // Release the reference taken for the GET_RESP.
    //
    rreq->Release();
    //
    // Release the request to free it.
    //
    if (!requestPassed)
    {
        rreq->Release();
    }
    goto fn_exit;
}

MPI_RESULT MPID_Win_fence(int assert, MPID_Win *win_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int comm_size;
    StackGuardArray<int> rma_target_proc;
    int* nops_to_proc;
    int* curr_ops_cnt;
    int* recvcnts;
    int i, total_op_count;
    MPIDI_RMA_ops *curr_ptr, *next_ptr;
    StackGuardArray<MPID_Request*> requests; /* array of requests */
    MPI_Win source_win_handle, target_win_handle;
    StackGuardArray<MPIDI_RMA_dtype_info> dtype_infos;
    StackGuardArray<void*> dataloops;    /* to store dataloops for each datatype */

    /* In case this process was previously the target of passive target rma
     * operations, we need to take care of the following...
     * Since we allow MPI_Win_unlock to return without a done ack from
     * the target in the case of multiple rma ops and exclusive lock,
     * we need to check whether there is a lock on the window, and if
     * there is a lock, poke the progress engine until the operartions
     * have completed and the lock is released. */
    while (win_ptr->dev.lock_state != MPIDI_LOCKED_NOT_LOCKED)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

    if (assert & MPI_MODE_NOPRECEDE)
    {
        win_ptr->fence_cnt = (assert & MPI_MODE_NOSUCCEED) ? 0 : 1;
        goto fn_exit;
    }

    if (win_ptr->fence_cnt == 0)
    {
        //
        //   win_ptr->fence_cnt == 0 means either this is the very first
        //   call to fence or the preceding fence had the
        //   MPI_MODE_NOSUCCEED assert.
        //   If this fence has MPI_MODE_NOSUCCEED, do nothing and return.
        //   Otherwise just increment the fence count and return.
        //
        if (!(assert & MPI_MODE_NOSUCCEED))
        {
            win_ptr->fence_cnt = 1;
        }
        goto fn_exit;
    }

    /* This is the second or later fence. Do all the preceding RMA ops. */
    const MPID_Comm* comm_ptr = win_ptr->comm_ptr;

    /* First inform every process whether it is a target of RMA
       ops from this process */
    comm_size = comm_ptr->remote_size;

    //
    // Allocate space for the arrays of:
    //  1. target processes
    //  2. number of operations to each process
    //  3. current operation cound of each process
    //  4. receive counts for each process
    //
    // These all have 'comm_size' elements.
    //
    rma_target_proc = new int[comm_size * 4];
    if( rma_target_proc == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    nops_to_proc = rma_target_proc + comm_size;
    curr_ops_cnt = nops_to_proc + comm_size;
    recvcnts = curr_ops_cnt + comm_size;

    for (i=0; i<comm_size; i++)
    {
        rma_target_proc[i] = 0;

        /* keep track of no. of ops to each proc. Needed for knowing
        whether or not to decrement the completion counter. The
        completion counter is decremented only on the last
        operation. */
        nops_to_proc[i] = 0;
        curr_ops_cnt[i] = 0;
        /* set up the recvcnts array for reduce scatter */
        recvcnts[i] = 1;
    }

    /* set rma_target_proc[i] to 1 if rank i is a target of RMA
       ops from this process */
    total_op_count = 0;
    curr_ptr = win_ptr->dev.rma_ops_list;
    while (curr_ptr != nullptr)
    {
        total_op_count++;
        if (curr_ptr->request != nullptr)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestrmanotexpected");
            goto fn_fail;
        }
        rma_target_proc[curr_ptr->target_rank] = 1;
        nops_to_proc[curr_ptr->target_rank]++;
        curr_ptr = curr_ptr->next;
    }

    if (total_op_count != 0)
    {
        requests = new MPID_Request*[total_op_count];
        if( requests == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        dtype_infos = new MPIDI_RMA_dtype_info[total_op_count];
        if( dtype_infos == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        dataloops = new void*[total_op_count];
        if( dataloops == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        for (i=0; i<total_op_count; i++)
        {
            dataloops[i] = nullptr;
        }
    }

    /* do a reduce_scatter (with MPI_SUM) on rma_target_proc. As a result,
       each process knows how many other processes will be doing
       RMA ops on its window */

    /* first initialize the completion counter. */
    win_ptr->dev.my_counter = comm_size;

    mpi_errno = NMPI_Reduce_scatter(MPI_IN_PLACE, rma_target_proc, recvcnts,
                                    MPI_INT, MPI_SUM, comm_ptr->handle);
    ON_ERROR_FAIL(mpi_errno);

    /* Set the completion counter */
    /* FIXME: MT: this needs to be done atomically because other
       procs have the address and could decrement it. */
    win_ptr->dev.my_counter = win_ptr->dev.my_counter - comm_size + rma_target_proc[0];

    i = 0;
    curr_ptr = win_ptr->dev.rma_ops_list;
    UINT16 rmaFlags;
    while (curr_ptr != nullptr)
    {
        source_win_handle = win_ptr->handle;
        if (curr_ops_cnt[curr_ptr->target_rank] == nops_to_proc[curr_ptr->target_rank] - 1)
        {
            rmaFlags = MPIDI_RMA_FLAG_LAST_OP;
        }
        else
        {
            rmaFlags = 0;
        }

        target_win_handle = win_ptr->dev.all_win_handles[curr_ptr->target_rank];

        switch (curr_ptr->type)
        {
        case (MPIDI_RMA_OP_PUT):
        case (MPIDI_RMA_OP_ACCUMULATE):
            mpi_errno = MPIDI_CH3I_Send_rma_msg(curr_ptr, win_ptr, source_win_handle,
                                                target_win_handle, rmaFlags, &dtype_infos[i],
                                                false, &dataloops[i], &requests[i]);
            ON_ERROR_FAIL(mpi_errno);
            break;
        case (MPIDI_RMA_OP_GET):
            mpi_errno = MPIDI_CH3I_Recv_rma_msg(curr_ptr, win_ptr, source_win_handle,
                                                target_win_handle, rmaFlags, &dtype_infos[i],
                                                false, &dataloops[i], &requests[i]);
            ON_ERROR_FAIL(mpi_errno);
            break;
        case (MPIDI_RMA_OP_GET_ACCUMULATE):
        case (MPIDI_RMA_OP_COMPARE_AND_SWAP):
            mpi_errno = MPIDI_CH3I_Sendrecv_rma_msg(curr_ptr, win_ptr, source_win_handle,
                                                target_win_handle, rmaFlags, &dtype_infos[i],
                                                false, &dataloops[i], &requests[i]);
            ON_ERROR_FAIL(mpi_errno);
            break;
        default:
            ON_ERROR_FAIL(mpi_errno);
        }
        i++;
        curr_ops_cnt[curr_ptr->target_rank]++;
        curr_ptr = curr_ptr->next;
    }

    //
    // wait for all operations from other processes to finish 
    //
    while (win_ptr->dev.my_counter && !win_ptr->dev.error_found)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

    //
    // wait for all the requests to complete
    // or wait for an indication that RMA operation failed,
    // then release the requests
    //
    for (i = 0; i<total_op_count; i++)
    {
        if (requests[i] == nullptr)
        {
            continue;
        }

        while (requests[i]->test_complete() == false && !win_ptr->dev.error_found)
        {
            mpi_errno = MPID_Progress_wait();
            ON_ERROR_FAIL(mpi_errno);
        }

        mpi_errno = requests[i]->status.MPI_ERROR;
        ON_ERROR_FAIL(mpi_errno);

        requests[i]->Release();
        if (requests[i]->kind == MPID_REQUEST_RMA_GET)
        {
            requests[i]->Release();
        }
        requests[i] = nullptr;
    }

    for (i = 0; i<total_op_count; i++)
    {
        if (dataloops[i] != nullptr)
        {
            MPIU_Free(dataloops[i]); /* allocated in send_rma_msg or recv_rma_msg */
        }
    }

    FreeRmaOpsList(win_ptr);

    if (assert & MPI_MODE_NOSUCCEED)
    {
        win_ptr->fence_cnt = 0;
    }

    if (win_ptr->dev.error_found)
    {
        if (win_ptr->dev.rma_error == MPI_SUCCESS)
        {
            return MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestrmaremoteerror");
        }
        else
        {
            return win_ptr->dev.rma_error;
        }
    }

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


MPI_RESULT MPID_Win_post(const MPID_Group *group_ptr, int assert, MPID_Win *win_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPI_Group win_grp, post_grp;
    int i, post_grp_size, dst;
    StackGuardArray<int> ranks_in_post_grp;
    int* ranks_in_win_grp;

    /* In case this process was previously the target of passive target rma
     * operations, we need to take care of the following...
     * Since we allow MPI_Win_unlock to return without a done ack from
     * the target in the case of multiple rma ops and exclusive lock,
     * we need to check whether there is a lock on the window, and if
     * there is a lock, poke the progress engine until the operations
     * have completed and the lock is therefore released. */
    while (win_ptr->dev.lock_state != MPIDI_LOCKED_NOT_LOCKED)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

    post_grp_size = group_ptr->size;

    /* initialize the completion counter */
    win_ptr->dev.my_counter = post_grp_size;

    if ((assert & MPI_MODE_NOCHECK) == 0)
    {
        /* NOCHECK not specified. We need to notify the source
           processes that Post has been called. */

        /* We need to translate the ranks of the processes in
           post_group to ranks in win_ptr->comm_ptr, so that we
           can do communication */

        //
        // Allocate enough room for both the ranks in post and win groups.
        // Both are post_grp_size.
        //
        ranks_in_post_grp = new int[post_grp_size * 2];
        if( ranks_in_post_grp == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        ranks_in_win_grp = ranks_in_post_grp + post_grp_size;

        for (i=0; i<post_grp_size; i++)
        {
            ranks_in_post_grp[i] = i;
        }

        mpi_errno = NMPI_Comm_group(win_ptr->comm_ptr->handle, &win_grp);
        ON_ERROR_FAIL(mpi_errno);

        post_grp = group_ptr->handle;

        mpi_errno = NMPI_Group_translate_ranks(post_grp, post_grp_size,
                                               ranks_in_post_grp, win_grp,
                                               ranks_in_win_grp);
        ON_ERROR_FAIL(mpi_errno);

        /* Send a 0-byte message to the source processes */
        for (i=0; i<post_grp_size; i++)
        {
            dst = ranks_in_win_grp[i];

            if (dst != win_ptr->comm_ptr->rank)
            {
                mpi_errno = NMPI_Send(&i, 0, MPI_INT, dst, 100, win_ptr->comm_ptr->handle);
                ON_ERROR_FAIL(mpi_errno);
            }
        }

        mpi_errno = NMPI_Group_free(&win_grp);
        ON_ERROR_FAIL(mpi_errno);
    }

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


MPI_RESULT MPID_Win_start(MPID_Group *group_ptr, int assert, MPID_Win *win_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    /* In case this process was previously the target of passive target rma
     * operations, we need to take care of the following...
     * Since we allow MPI_Win_unlock to return without a done ack from
     * the target in the case of multiple rma ops and exclusive lock,
     * we need to check whether there is a lock on the window, and if
     * there is a lock, poke the progress engine until the operations
     * have completed and the lock is therefore released. */
    while (win_ptr->dev.lock_state != MPIDI_LOCKED_NOT_LOCKED)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }

    win_ptr->start_group_ptr = group_ptr;
    MPIR_Group_add_ref( group_ptr );
    win_ptr->start_assert = assert;

fn_exit:
    return mpi_errno;

fn_fail:
    goto fn_exit;
}


MPI_RESULT MPID_Win_complete(MPID_Win *win_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int comm_size, src, new_total_op_count;
    StackGuardArray<int> nops_to_proc;
    int* curr_ops_cnt;
    int i, j, dst, total_op_count;
    MPIDI_RMA_ops *curr_ptr, *next_ptr;
    StackGuardArray<MPID_Request*> requests; /* array of requests */
    MPI_Win source_win_handle, target_win_handle;
    StackGuardArray<MPIDI_RMA_dtype_info> dtype_infos;
    StackGuardArray<void*> dataloops;    /* to store dataloops for each datatype */
    MPI_Group win_grp, start_grp;
    int start_grp_size, rank;
    StackGuardArray<int> ranks_in_start_grp;
    int *ranks_in_win_grp;

    MPID_Comm* comm_ptr = win_ptr->comm_ptr;
    comm_size = comm_ptr->remote_size;

    //
    // Translate the ranks of the processes in
    // start_group to ranks in win_ptr->comm
    //
    start_grp_size = win_ptr->start_group_ptr->size;

    //
    // Allocate enough room for both the ranks in start and win groups.
    // Both are start_grp_size.
    //
    ranks_in_start_grp = new int[start_grp_size * 2];
    if( ranks_in_start_grp == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }
    ranks_in_win_grp = ranks_in_start_grp + start_grp_size;

    for (i=0; i<start_grp_size; i++)
    {
        ranks_in_start_grp[i] = i;
    }

    mpi_errno = NMPI_Comm_group(comm_ptr->handle, &win_grp);
    ON_ERROR_FAIL(mpi_errno);

    start_grp = win_ptr->start_group_ptr->handle;

    mpi_errno = NMPI_Group_translate_ranks(start_grp, start_grp_size,
                                           ranks_in_start_grp, win_grp,
                                           ranks_in_win_grp);
    ON_ERROR_FAIL(mpi_errno);

    rank = comm_ptr->rank;

    /* If MPI_MODE_NOCHECK was not specified, we need to check if
       Win_post was called on the target processes. Wait for a 0-byte sync
       message from each target process */
    if ((win_ptr->start_assert & MPI_MODE_NOCHECK) == 0)
    {
        for (i=0; i<start_grp_size; i++)
        {
            src = ranks_in_win_grp[i];
            if (src != rank)
            {
                mpi_errno = NMPI_Recv(NULL, 0, MPI_INT, src, 100,
                                      comm_ptr->handle, MPI_STATUS_IGNORE);
                ON_ERROR_FAIL(mpi_errno);
            }
        }
    }

    /* keep track of no. of ops to each proc. Needed for knowing
       whether or not to decrement the completion counter. The
       completion counter is decremented only on the last
       operation. */

    //
    // Allocate enough room for both the number of ops and current ops counts.
    // Both have comm_size elements.
    //
    nops_to_proc = new int[comm_size * 2];
    if( nops_to_proc == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }
    curr_ops_cnt = nops_to_proc + comm_size;
    for (i=0; i<comm_size; i++)
    {
        nops_to_proc[i] = 0;
        curr_ops_cnt[i] = 0;
    }

    total_op_count = 0;
    curr_ptr = win_ptr->dev.rma_ops_list;
    while (curr_ptr != NULL)
    {
        nops_to_proc[curr_ptr->target_rank]++;
        total_op_count++;
        if (curr_ptr->request != nullptr)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_REQUEST, "**requestrmanotexpected");
            goto fn_fail;
        }
        curr_ptr = curr_ptr->next;
    }

    requests = new MPID_Request*[total_op_count+start_grp_size];
    if( requests == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    /* We allocate a few extra requests because if there are no RMA
       ops to a target process, we need to send a 0-byte message just
       to decrement the completion counter. */

    if (total_op_count != 0)
    {
        dtype_infos = new MPIDI_RMA_dtype_info[total_op_count];
        if( dtype_infos == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        dataloops = new void*[total_op_count];
        if( dataloops == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
        for (i=0; i<total_op_count; i++) dataloops[i] = NULL;
    }

    i = 0;
    curr_ptr = win_ptr->dev.rma_ops_list;
    UINT16 rmaFlags;
    while (curr_ptr != NULL)
    {
        source_win_handle = win_ptr->handle;
        if (curr_ops_cnt[curr_ptr->target_rank] ==
            nops_to_proc[curr_ptr->target_rank] - 1)
        {
            rmaFlags = MPIDI_RMA_FLAG_LAST_OP | MPIDI_RMA_FLAG_LOCK_RELEASE;
        }
        else
        {
            rmaFlags = 0;
        }

        target_win_handle = win_ptr->dev.all_win_handles[curr_ptr->target_rank];

        switch (curr_ptr->type)
        {
        case (MPIDI_RMA_OP_PUT):
        case (MPIDI_RMA_OP_ACCUMULATE):
            mpi_errno = MPIDI_CH3I_Send_rma_msg(curr_ptr, win_ptr, source_win_handle,
                                                target_win_handle, rmaFlags, &dtype_infos[i],
                                                false, &dataloops[i], &requests[i]);
            ON_ERROR_FAIL(mpi_errno);
            break;
        case (MPIDI_RMA_OP_GET):
            mpi_errno = MPIDI_CH3I_Recv_rma_msg(curr_ptr, win_ptr, source_win_handle,
                                                target_win_handle, rmaFlags, &dtype_infos[i],
                                                false, &dataloops[i], &requests[i]);
            ON_ERROR_FAIL(mpi_errno);
            break;
        case (MPIDI_RMA_OP_GET_ACCUMULATE):
        case (MPIDI_RMA_OP_COMPARE_AND_SWAP):
            mpi_errno = MPIDI_CH3I_Sendrecv_rma_msg(curr_ptr, win_ptr, source_win_handle,
                                                target_win_handle, rmaFlags, &dtype_infos[i],
                                                false, &dataloops[i], &requests[i]);
            ON_ERROR_FAIL(mpi_errno);
            break;
        default:
            ON_ERROR_FAIL(mpi_errno);
        }
        i++;
        curr_ops_cnt[curr_ptr->target_rank]++;
        curr_ptr = curr_ptr->next;
    }

    /* If the start_group included some processes that did not end up
       becoming targets of  RMA operations from this process, we need
       to send a dummy message to those processes just to decrement
       the completion counter */

    j = i;
    new_total_op_count = total_op_count;
    for (i=0; i<start_grp_size; i++)
    {
        dst = ranks_in_win_grp[i];
        if (dst == rank)
        {
            /* FIXME: MT: this has to be done atomically */
            win_ptr->dev.my_counter -= 1;
        }
        else if (nops_to_proc[dst] == 0)
        {
            requests[j] = MPID_Request_create(MPID_REQUEST_SEND);
            if( requests[j] == nullptr )
            {
                mpi_errno = MPIU_ERR_NOMEM();
                goto fn_fail;
            }

            requests[j]->init_pkt(MPIDI_CH3_PKT_PUT);
            MPIDI_CH3_Pkt_put_accum_t* put_pkt = &requests[j]->get_pkt()->put_accum;
            put_pkt->win.disp = 0;
            put_pkt->win.count = 0;
            put_pkt->win.datatype = MPI_INT;
            put_pkt->win.target = win_ptr->dev.all_win_handles[dst];
            put_pkt->win.source = win_ptr->handle;

            MPIDI_VC_t* vc = MPIDI_Comm_get_vc(comm_ptr, dst);
            mpi_errno = MPIDI_CH3_SendRequest(vc, requests[j]);
            if(mpi_errno != MPI_SUCCESS)
            {
                requests[j]->Release();
                requests[j] = NULL;
            }
            ON_ERROR_FAIL(mpi_errno);

            j++;
            new_total_op_count++;
        }
    }

    for (i=0; i<new_total_op_count; i++)
    {
        if (requests[i] == NULL)
        {
            continue;
        }

        mpi_errno = MPIR_Wait(requests[i]);
        ON_ERROR_FAIL(mpi_errno);

        mpi_errno = requests[i]->status.MPI_ERROR;
        ON_ERROR_FAIL(mpi_errno);

        requests[i]->Release();
        requests[i] = NULL;
    }

    for (i=0; i<total_op_count; i++)
    {
        if (dataloops[i] != NULL)
        {
            MPIU_Free(dataloops[i]);
        }
    }

    FreeRmaOpsList(win_ptr);

    mpi_errno = NMPI_Group_free(&win_grp);
    ON_ERROR_FAIL(mpi_errno);

    //
    // free the group stored in window
    //
    MPIR_Group_release(win_ptr->start_group_ptr);
    win_ptr->start_group_ptr = NULL;

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


MPI_RESULT MPID_Win_wait(const MPID_Win *win_ptr)
{
    /* wait for all operations from other processes to finish */
    while (win_ptr->dev.my_counter)
    {
        MPI_RESULT mpi_errno = MPID_Progress_wait();
        if (mpi_errno != MPI_SUCCESS)
            return mpi_errno;
    }

    return MPI_SUCCESS;
}


MPI_RESULT MPID_Win_lock(int lock_type, int dest, int assert, MPID_Win *win_ptr)
{
    if (dest == MPI_PROC_NULL)
    {
        return MPI_SUCCESS;
    }

    if (dest == win_ptr->comm_ptr->rank)
    {
        //
        // The target is this process itself. We must block until the lock
        // is acquired.
        // poke the progress engine until lock is granted
        //
        while (!MPIDI_CH3I_Try_acquire_win_lock(win_ptr, lock_type))
        {
            MPI_RESULT mpi_errno = MPID_Progress_wait();
            if( mpi_errno != MPI_SUCCESS )
            {
                return mpi_errno;
            }
        }
        //
        // local lock acquired. local puts, gets, accumulates will be done
        // directly without queueing
        //
        win_ptr->dev.remote_lock_state[dest] = lock_type;
    }
    else
    {
        //
        // target is some other process. add the lock request to rma_ops_list
        //
        if (win_ptr->dev.lock_op_used == MPIDI_WIN_NOT_LOCKED)
        {
            win_ptr->dev.lock_op_used = MPIDI_WIN_LOCK_TARGET;
        }
        return MPID_Win_queue_lock(win_ptr, lock_type, dest, assert);
    }
    return MPI_SUCCESS;
}


MPI_RESULT MPID_Win_lock_all(int assert, MPID_Win *win_ptr)
{
    MPI_RESULT mpi_errno;
    win_ptr->dev.lock_op_used = MPIDI_WIN_LOCK_ALL;
    int size = win_ptr->comm_ptr->remote_size;
    for (int i = 0; i < size; ++i)
    {
        mpi_errno = MPID_Win_lock(MPI_LOCK_SHARED, i, assert, win_ptr);
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
    }
    return MPI_SUCCESS;
}


static MPI_RESULT LockTarget(MPID_Win* win_ptr, int dest, int lock_type)
{
    MPIDI_CH3_Pkt_t upkt;
    MPIDI_CH3_Pkt_lock_t *lock_pkt = &upkt.lock;
    MPI_RESULT mpi_errno;

    MPIU_Assert(win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED);

    MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, dest);

    MPIDI_Pkt_init(lock_pkt, MPIDI_CH3_PKT_LOCK);
    lock_pkt->win_target = win_ptr->dev.all_win_handles[dest];
    lock_pkt->win_source = win_ptr->handle;
    lock_pkt->lock_type = lock_type;

    mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
    ON_ERROR_FAIL(mpi_errno);

    //
    // After the target grants the lock, it sends a lock_granted
    // packet. This packet is received in ch3u_handle_recv_pkt.c.
    // The handler for the packet sets the remote_lock_state to "locked".
    //
    // Poke the progress engine until remote_lock_state is set to "locked"
    //
    while (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }
fn_fail:
    return mpi_errno;
}


static MPI_RESULT LockAllTargets(MPID_Win* win_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    //
    // LockAllTargets is only called from _all RMA routines,
    // all of them operate with MPI_LOCK_SHARED lock
    //
    int lock_type = MPI_LOCK_SHARED;
    bool fWaitToLock = false;

    int size = win_ptr->comm_ptr->remote_size;
    for (int dest = 0; dest < size; dest++)
    {
        if (win_ptr->dev.requested_ops_count[dest] == 0)
        {
            continue;
        }
        MPIU_Assert(win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED ||
            win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_SHARED);

        if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_SHARED)
        {
            //
            // this target has been locked already - local lock or locked called 
            // during previous flush
            //
            continue;
        }
        MPIDI_CH3_Pkt_t upkt;
        MPIDI_CH3_Pkt_lock_t *lock_pkt = &upkt.lock;

        MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, dest);

        MPIDI_Pkt_init(lock_pkt, MPIDI_CH3_PKT_LOCK);
        lock_pkt->win_target = win_ptr->dev.all_win_handles[dest];
        lock_pkt->win_source = win_ptr->handle;
        lock_pkt->lock_type = lock_type;

        mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
        ON_ERROR_FAIL(mpi_errno);

        fWaitToLock = true;
    }

    //
    // After the target grants the lock, it sends a lock_granted
    // packet. This packet is received in ch3u_handle_recv_pkt.c.
    // The handler for the packet sets the remote_lock_state to "locked".
    //
    // Poke the progress engine until remote_lock_state is set to "locked"
    // for all the targets
    //
    while (fWaitToLock)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);

        fWaitToLock = false;
        for (int dest = 0; dest < size; dest++)
        {
            if (win_ptr->dev.requested_ops_count[dest] != 0 && 
                win_ptr->dev.remote_lock_state[dest] != MPIDI_LOCKED_SHARED)
            {
                fWaitToLock = true;
                break;
            }
        }
    }

fn_fail:
    return mpi_errno;
}


static MPI_RESULT FlushPassiveTarget(int dest, MPID_Win *win_ptr, bool unlock)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    bool single_op_opt;
    int type_size;
    const MPIDI_RMA_ops *curr_op;

    single_op_opt = false;

    const MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, dest);

    MPIU_Assert(win_ptr->dev.rma_ops_list != nullptr);
    
    MPIDI_RMA_ops* lock_op = nullptr;
    MPIDI_RMA_ops* first_rma_op = nullptr;

    if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED)
    {
        lock_op = GetNextOpToTarget(win_ptr->dev.rma_ops_list, dest);
        MPIU_Assert(lock_op != nullptr);
        if (lock_op->type != MPIDI_RMA_OP_LOCK)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rmasync");
            ON_ERROR_FAIL(mpi_errno);
        }
        first_rma_op = GetNextOpToTarget(lock_op->next, dest);
        MPIU_Assert(first_rma_op != nullptr);
    }
    else
    {
        first_rma_op = GetNextOpToTarget(win_ptr->dev.rma_ops_list, dest);
        MPIU_Assert(first_rma_op != nullptr);
    }

    if (unlock 
        && win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED
        && win_ptr->dev.requested_ops_count[dest] == 1
        )
    {
        //
        // Single put, get, or accumulate between the lock and unlock. If it
        // is of small size and predefined datatype at the target, we
        // do an optimization where the lock and the RMA operation are
        // sent in a single packet. Otherwise, we send a separate lock
        // request first.
        //
        // TODO: mpicr: add one op optimization for Get_Accumulate and
        // Compare_and_swap
        //
        type_size = fixme_cast<int>(first_rma_op->origin_datatype.GetSize());

        if (first_rma_op->target_datatype.IsPredefined() &&
            (fixme_cast<MPIDI_msg_sz_t>(type_size * first_rma_op->origin_count)
                <= vc->eager_max_msg_sz) &&
            (first_rma_op->type == MPIDI_RMA_OP_PUT || first_rma_op->type == MPIDI_RMA_OP_ACCUMULATE))
        {
            single_op_opt = true;
            win_ptr->dev.remote_lock_state[dest] = lock_op->lock_type;
            if (first_rma_op->type == MPIDI_RMA_OP_GET)
            {
                mpi_errno = MPIDI_CH3I_Send_lock_get(win_ptr, dest, lock_op, first_rma_op);
                win_ptr->dev.remote_ops_done[dest] = 1;
            }
            else
            {
                mpi_errno = MPIDI_CH3I_Send_lock_put_or_acc(win_ptr, dest, lock_op, first_rma_op);
                win_ptr->dev.remote_ops_done[dest] = 0;
            }
            ON_ERROR_FAIL(mpi_errno);
        }
    }

    if (!single_op_opt)
    {
        //
        // Send a lock packet over to the target. wait for the lock_granted
        // reply. then do all the RMA ops.
        //
        if (lock_op != nullptr)
        {
            mpi_errno = LockTarget(win_ptr, dest, lock_op->lock_type);
            ON_ERROR_FAIL(mpi_errno);
        }

        //
        // Now do all the RMA operations
        //
        UINT16 lastOpFlags = 0;
        if (unlock)
        {
            lastOpFlags = MPIDI_RMA_FLAG_LOCK_RELEASE;
        }
        mpi_errno = MPIDI_CH3I_Do_passive_target_rma(win_ptr, lastOpFlags, dest);
        ON_ERROR_FAIL(mpi_errno);
    }

    //
    // If there are operations pending completion on the target,
    // wait until the "pt rma done" packet is received from the target.
    // This packet sets the win_ptr->dev.remote_ops_done flag to 1.
    // Poke the progress engine until remote_ops_done flag is set to 1
    //
    while (win_ptr->dev.remote_ops_done[dest] != 1 && !win_ptr->dev.error_found)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);
    }
    if (unlock)
    {
        win_ptr->dev.remote_lock_state[dest] = MPIDI_LOCKED_NOT_LOCKED;
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


static MPI_RESULT WaitForOpsDone(const MPID_Win* win_ptr, bool unlock)
{
    //
    // If there are operations pending completion on the target,
    // wait until the "pt rma done" packet is received from the target.
    // This packet sets the win_ptr->dev.remote_ops_done flag to 1.
    // Poke the progress engine until remote_ops_done flag is set to 1
    //
    MPI_RESULT mpi_errno;
    int size = win_ptr->comm_ptr->remote_size;
    bool fWait;
    do
    {
        fWait = false;
        for (int dest = 0; dest < size; dest++)
        {
            if (dest == win_ptr->comm_ptr->rank)
            {
                continue;
            }
            if (win_ptr->dev.remote_ops_done[dest] != 1)
            {
                fWait = true;
                break;
            }
        }

        if (fWait)
        {
            mpi_errno = MPID_Progress_wait();
            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
    } while (fWait && !win_ptr->dev.error_found);

    return MPI_SUCCESS;
}


static MPI_RESULT FlushPassiveTargetLocal(int dest, MPID_Win *win_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    int type_size;
    const MPIDI_RMA_ops *curr_op;

    MPIU_Assert(win_ptr->dev.rma_ops_list != nullptr);

    if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED)
    {
        MPIDI_RMA_ops* lock_op = GetNextOpToTarget(win_ptr->dev.rma_ops_list, dest);
        MPIU_Assert(lock_op != nullptr && lock_op->type == MPIDI_RMA_OP_LOCK);
        mpi_errno = LockTarget(win_ptr, dest, lock_op->lock_type);
        ON_ERROR_FAIL(mpi_errno);
    }

    //
    // Issue all the RMA operations
    //
    mpi_errno = MPIDI_CH3I_Do_passive_target_rma(win_ptr, 0, dest);
    ON_ERROR_FAIL(mpi_errno);

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


MPI_RESULT MPID_Win_unlock(int dest, MPID_Win *win_ptr)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPIDI_RMA_ops *rma_op;

    if (dest == MPI_PROC_NULL)
    {
        goto fn_exit;
    }

    MPID_Comm *comm_ptr = win_ptr->comm_ptr;
    if (dest == comm_ptr->rank)
    {
        /* local lock. release the lock on the window, grant the next one
         * in the queue, and return. */
        mpi_errno = MPIDI_CH3I_Release_lock(win_ptr);
        if( mpi_errno != MPI_SUCCESS )
        {
            goto fn_exit;
        }
        win_ptr->dev.remote_lock_state[dest] = MPIDI_LOCKED_NOT_LOCKED;
        mpi_errno = MPID_Progress_pump();
        goto fn_exit;
    }

    //
    // TODO: current implementation use MpiLock to guarantee
    // thread safety of RMA routines. The better approach would be 
    // to move processing of RMA operations to MPI progress engine,
    // which is single threaded
    //
    MpiLockEnter(&win_ptr->winLock);

    //
    // process the easy situations first
    //
    if (win_ptr->dev.requested_ops_count[dest] == 0)
    {
        if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_NOT_LOCKED)
        {
            //
            // win_lock was not called. return error 
            //
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rmasync");
            goto fn_exit;
        }
        else if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED)
        {
            //
            // only lock operation was called - clean the rma_ops entries
            //
            win_ptr->dev.remote_lock_state[dest] = MPIDI_LOCKED_NOT_LOCKED;
            goto fn_exit;
        }
        else
        {
            //
            // Send unlock packet to the target
            //
            MPIDI_CH3_Pkt_t upkt;
            MPIDI_CH3_Pkt_unlock_t *unlock_pkt = &upkt.unlock;

            MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, dest);

            MPIDI_Pkt_init(unlock_pkt, MPIDI_CH3_PKT_UNLOCK);
            unlock_pkt->win_target = win_ptr->dev.all_win_handles[dest];
            unlock_pkt->win_source = win_ptr->handle;

            mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
            ON_ERROR_FAIL(mpi_errno);

            //
            // This code guarantees the completion on the
            // operations on the target, operations may be completed locally, 
            // but not on the target by flush_local
            //
            while (win_ptr->dev.remote_lock_state[dest] != MPIDI_LOCKED_NOT_LOCKED &&
                    !win_ptr->dev.error_found)
            {
                mpi_errno = MPID_Progress_wait();
                ON_ERROR_FAIL(mpi_errno);
            }

            goto fn_exit;
        }
    }

    mpi_errno = FlushPassiveTarget(dest, win_ptr, true);
    ON_ERROR_FAIL(mpi_errno);

fn_exit:
    if (win_ptr->dev.lock_op_used == MPIDI_WIN_LOCK_TARGET)
    {
        win_ptr->dev.lock_op_used = MPIDI_WIN_NOT_LOCKED;
    }
    FreeRmaOpsTargetList(win_ptr, dest);
    MpiLockLeave(&win_ptr->winLock);
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


MPI_RESULT MPID_Win_unlock_all(MPID_Win *win_ptr)
{
    if (win_ptr->dev.lock_op_used != MPIDI_WIN_LOCK_ALL)
    {
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**winlockall");
    }
    MPI_RESULT mpi_errno = MPI_SUCCESS;

    mpi_errno = LockAllTargets(win_ptr);
    ON_ERROR_FAIL(mpi_errno);

    UINT16 lastOpFlags = MPIDI_RMA_FLAG_LOCK_RELEASE;
    mpi_errno = IssueRmaOps(win_ptr, lastOpFlags);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = WaitForOpsDone(win_ptr, true);
    ON_ERROR_FAIL(mpi_errno);

    //
    // if no operations have been issued to the target, 
    // this target might still be locked from the previous 
    // call to win_flush - it needs to be unlocked now
    //
    int size = win_ptr->comm_ptr->remote_size;
    bool fWaitToUnlock = false;
    for (int dest = 0; dest < size; dest++)
    {
        if (dest == win_ptr->comm_ptr->rank)
        {
            continue;
        }
        if (win_ptr->dev.remote_lock_state[dest] != MPIDI_LOCKED_NOT_LOCKED)
        {
            if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_LOCK_REQUESTED)
            {
                win_ptr->dev.remote_lock_state[dest] = MPIDI_LOCKED_NOT_LOCKED;
            }
            else
            {
                MPIU_Assert(win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_SHARED);
                //
                // Send unlock packet to the target
                //
                MPIDI_CH3_Pkt_t upkt;
                MPIDI_CH3_Pkt_unlock_t *unlock_pkt = &upkt.unlock;

                MPIDI_VC_t* vc = MPIDI_Comm_get_vc(win_ptr->comm_ptr, dest);

                MPIDI_Pkt_init(unlock_pkt, MPIDI_CH3_PKT_UNLOCK);
                unlock_pkt->win_target = win_ptr->dev.all_win_handles[dest];
                unlock_pkt->win_source = win_ptr->handle;

                mpi_errno = MPIDI_CH3_ffSend(vc, &upkt);
                ON_ERROR_FAIL(mpi_errno);

                fWaitToUnlock = true;
            }
        }
    }

    //
    // wait for all targets to be unlocked
    //
    
    while (fWaitToUnlock)
    {
        mpi_errno = MPID_Progress_wait();
        ON_ERROR_FAIL(mpi_errno);

        fWaitToUnlock = false;
        for (int dest = 0; dest < size; dest++)
        {
            if (dest == win_ptr->comm_ptr->rank)
            {
                continue;
            }
            if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_SHARED)
            {
                fWaitToUnlock = true;
                break;
            }
        }
    }

    //
    // unlock self (all other targets should be unlocked by now)
    //
    mpi_errno = MPIDI_CH3I_Release_lock(win_ptr);
    ON_ERROR_FAIL(mpi_errno);

    //
    // TODO: mpicr: check if it is really needed
    //
    //mpi_errno = MPID_Progress_pump();

    win_ptr->dev.remote_lock_state[win_ptr->comm_ptr->rank] = MPIDI_LOCKED_NOT_LOCKED;
    win_ptr->dev.lock_op_used = MPIDI_WIN_NOT_LOCKED;

    //
    // clean the queue
    //
    FreeRmaOpsList(win_ptr);

fn_fail:
    return mpi_errno;
}


//
// The difference between MPID_Win_unlock and MPID_Win_flush is that latter 
// does not release the lock on the target
//
MPI_RESULT MPID_Win_flush(int dest, MPID_Win *win_ptr, bool isLocal)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    MPIDI_RMA_ops *rma_op;

    //
    // if the flush is called on self - do nothing, 
    // local operations should be completed
    //
    if (dest == win_ptr->comm_ptr->rank)
    {
        return MPI_SUCCESS;
    }

    MpiLockEnter(&win_ptr->winLock);

    if (win_ptr->dev.requested_ops_count[dest] == 0)
    {
        if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_NOT_LOCKED)
        {
            //
            // win_lock was not called. return error 
            //
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rmasync");
        }
        else if (!isLocal)
        {
            //
            // no new rma operations - make sure that operations 
            // on the target are completed, they may be completed locally 
            // by previous call to MPI_Win_flush_local
            //
            while (win_ptr->dev.remote_ops_done[dest] != 1 && !win_ptr->dev.error_found)
            {
                mpi_errno = MPID_Progress_wait();
                if (mpi_errno != MPI_SUCCESS)
                {
                    break;
                }
            }
        }
        goto fn_exit;
    }

    rma_op = GetNextOpToTarget(win_ptr->dev.rma_ops_list, dest);


    if (win_ptr->dev.remote_lock_state[dest] == MPIDI_LOCKED_NOT_LOCKED && rma_op->type != MPIDI_RMA_OP_LOCK)
    {
        //
        // win_lock was not called. return error
        //
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rmasync");
    }

    if (isLocal)
    {
        mpi_errno = FlushPassiveTargetLocal(dest, win_ptr);
    }
    else
    {
        mpi_errno = FlushPassiveTarget(dest, win_ptr, false);
    }

fn_exit:
    FreeRmaOpsTargetList(win_ptr, dest);
    MpiLockLeave(&win_ptr->winLock);
    return mpi_errno;
}


MPI_RESULT MPID_Win_flush_all(MPID_Win *win_ptr, bool isLocal)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    if (win_ptr->dev.lock_op_used != MPIDI_WIN_LOCK_ALL)
    {
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**winlockall");
    }
    mpi_errno = LockAllTargets(win_ptr);
    ON_ERROR_FAIL(mpi_errno);

    UINT16 lastOpFlags = 0;
    mpi_errno = IssueRmaOps(win_ptr, lastOpFlags);
    ON_ERROR_FAIL(mpi_errno);

    if (!isLocal)
    {
        mpi_errno = WaitForOpsDone(win_ptr, true);
        ON_ERROR_FAIL(mpi_errno);
    }

    //
    // clean the queue
    //
    FreeRmaOpsList(win_ptr);

fn_fail:
    return mpi_errno;
}


_Success_( return == TRUE )
BOOL
MPID_Win_get_attr(
    _In_ MPID_Win* pWin,
    _In_ int hKey,
    _Outptr_result_maybenull_ void** pValue
    )
{
    int attr_idx = hKey & 0x0000000f;

    //
    // The C versions of the attributes return the address of a of the value
    // and the Fortran versions provide the actual value (as an MPI_Aint).
    //
    // Some of the attributes returned by reference are copies, so that the
    // original cannot be easily corrupted.
    //
    switch (attr_idx)
    {
    case 1: /* WIN_BASE */
    case 2: /* Fortran WIN_BASE */
        pWin->copyBase = (pWin->createFlavor == MPI_WIN_FLAVOR_DYNAMIC) ? MPI_BOTTOM : pWin->base;
        *pValue = pWin->copyBase;
        break;

    case 3: /* SIZE */
    case 4: /* Fortran SIZE */
        pWin->copySize = (pWin->createFlavor == MPI_WIN_FLAVOR_DYNAMIC) ? 0 : pWin->size;
        *pValue = &pWin->copySize;
        break;

    case 5: /* DISP_UNIT */
    case 6: /* Fortran DISP_UNIT */
        pWin->copyDispUnit = pWin->disp_unit;
        *pValue = &pWin->copyDispUnit;
        break;

    case 7: /* CREATE_FLAVOR */
    case 8: /* Fortran CREATE_FLAVOR */
        *pValue = &pWin->createFlavor;
        break;

    case 9: /* MODEL */
    case 10: /* Fortran MODEL */
        pWin->winModel = MPI_WIN_SEPARATE;
        *pValue = &pWin->winModel;
        break;

    default:
        return FALSE;
    }

    if( (attr_idx & 1) == 0 && (*pValue) != nullptr)
    {
        //
        // Indexes for attributes are 1 more in Fortran than the C index.
        // All Fortran attribute indexes are even.
        //
        // Convert the return value to be "by value" for Fortran callers.
        //
        MPI_Aint intValue = *reinterpret_cast<int*>(*pValue);
        *pValue = reinterpret_cast<void*>(intValue);
    }
    return TRUE;
}
