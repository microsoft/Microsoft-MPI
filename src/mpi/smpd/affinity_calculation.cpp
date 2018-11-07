// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "affinity_calculation.h"


typedef HRESULT (*PFN_HWSUMMARY_INIT) (UINT32*, HWSUMMARY*);
typedef HRESULT (*PFN_HWTREE_INIT) (UINT32*, HWTREE*, HWSUMMARY*);
typedef HRESULT (*PFN_HWVIEW_INIT) (UINT32*, HWVIEW*, const HWTREE*, const HWSUMMARY*);


static const wchar_t* sOccupiedCoreInfoRegion = L"msmpi_aa_info_%x";
static const wchar_t* sAffinityAllocationLock = L"Global\\msmpi_aa_lock";
static const DWORD    sLockTimeout = 5000;
static const UINT32   sPublishedPattern = 0xa1b23c4d;

//
// Used for initializing object pointers with incomplete types.
// If initialization fails, returns nullptr.
//
// This function needs to use midl_user_free/midl_user_allocate
// because the result buffer will be used in the RPC response (to
// avoid allocating extra and unnecessary temporary buffer. Due to the
// way our current SMPD IDL is declared, the RPC runtime will call
// midl_user_free to deallocate memory allocated by this function.
// For any other user of this function, the caller needs to call
// midl_user_free to free memory allocated by this function (such as
// hwview)
//
template<typename FInit, typename THw, typename... Arguments>
HRESULT InitializeHW(FInit functInit, UINT32* pcbHw, THw** ppHw, Arguments... params)
{
    HRESULT     hr;
    UINT32      sizeHw = 0;
    THw*        pHw = nullptr;

    *ppHw = nullptr;
    do
    {
        hr = functInit(&sizeHw, pHw, params...);
        if( FAILED( hr ) )
        {
            midl_user_free(pHw);

            if( hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) )
            {
                return hr;
            }
            pHw = static_cast<THw*>( midl_user_allocate( sizeHw ) );
            if( nullptr == pHw )
            {
                return hr;
            }
        }
    }while( hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) );

    *ppHw = pHw;
    *pcbHw = sizeHw;
    return S_OK;
};


//
// Get the affinity for the current process on a HW layer according to
// the placement algorithm that is specified for each level.
//
static UINT32
GetLevelAffinity(
    _In_ const HWVIEW *pView,
    _In_ const SMPD_AFFINITY_PLACEMENT *pPlacement,
    _In_ UINT32 currentLevel,
    _In_ UINT32 endLevel,
    _In_ UINT32 levelParent,
    _In_ UINT32 levelStart,
    _In_ UINT32 levelEnd,
    _In_ UINT32 currentProc,
    _In_ UINT32 totalProcs
    )
{
    while( currentLevel <= endLevel )
    {
        UINT32 levelLength = levelEnd - levelStart + 1;

        switch( pPlacement[currentLevel] )
        {
        case SMPD_AFFINITY_DISABLED:
            {
                //
                // Simply flatten this level
                //
                levelStart = pView->Nodes[levelStart].FirstChild;
                levelEnd = pView->Nodes[levelEnd].LastChild;
            }
            break;
        case SMPD_AFFINITY_SEQUENTIAL:
            {
                //
                // Sequential algorithm, so currentProc-th node is selected.
                // calculate the corresponding index in the new node
                // calculate how many processes will the selected node have
                //
                levelParent = levelStart + currentProc % levelLength;
                currentProc = currentProc / levelLength;
                totalProcs = totalProcs / levelLength;
                levelStart = pView->Nodes[levelParent].FirstChild;
                levelEnd = pView->Nodes[levelParent].LastChild;
            }
            break;
        case SMPD_AFFINITY_BALANCED:
            {
                //
                // Indices are distributed over level evenly, with consecutive indices
                // groupped together. E.g: {1,2,3} {4,5,6}
                //
                UINT32 procPerLevel = totalProcs / levelLength;
                UINT32 leftOvers = totalProcs - procPerLevel * levelLength;
                if( leftOvers !=0 )
                {
                    // not all resources are used
                    ++procPerLevel;
                }

                levelParent = levelStart + currentProc / procPerLevel;
                currentProc -= (procPerLevel * (currentProc / procPerLevel));
                levelStart = pView->Nodes[levelParent].FirstChild;
                levelEnd = pView->Nodes[levelParent].LastChild;
            }
            break;
        case SMPD_AFFINITY_SPREAD:
            {
                UINT32 childCount = 2;
                levelParent = levelStart;

                do
                {
                    if( ( levelLength & 1 ) == 1)
                    {
                        //
                        // If cannot bisect, traverse sequentially and do not proceed further
                        //
                        childCount = levelLength;
                        levelLength = 1;
                    }
                    else
                    {
                        childCount =  2;
                        levelLength /= 2;
                    }
                    levelParent += levelLength * (currentProc % childCount);
                    currentProc /= childCount;
                }while( levelLength > 1 );

                levelStart = pView->Nodes[levelParent].FirstChild;
                levelEnd = pView->Nodes[levelParent].LastChild;
            }
            break;
        default:
            break;
        }

        ++currentLevel;
    }

    return levelParent;
}


//
// For a given node in HwView, get active affinity mask. If location is not an LCore,
// active mask is equal to the combination of all children at LCore level.
//
static void
GetActiveAffinityMask(
    _In_ const HWVIEW *pView,
    _In_ UINT32 location,
    _Out_ HWAFFINITY *pAffinity
    )
{
    if( pView->Nodes[location].Type < HWNODE_TYPE_LCORE )
    {
        UINT32 childId = pView->Nodes[location].FirstChild;

        pAffinity->Mask = 0;
        pAffinity->GroupId = pView->Tree->Nodes[ pView->Nodes[childId].NodeId ].Affinity.GroupId;
        do
        {
            ASSERT( pAffinity->GroupId == pView->Tree->Nodes[ pView->Nodes[childId].NodeId ].Affinity.GroupId );
            HWAFFINITY childAffinity;
            GetActiveAffinityMask(pView, childId, &childAffinity);
            pAffinity->Mask |= childAffinity.Mask;
            childId = pView->Nodes[childId].NextSibling;
        }while( childId != HWNODEID_NONE );
    }
    else
    {
        UINT32 treeNodeId  = pView->Nodes[location].NodeId;
        *pAffinity = pView->Tree->Nodes[treeNodeId].Affinity;
    }
}


//
// Checks children of a level in hwview. If all nodes in a level have the same number
// of children, returns true. Otherwise returns false.
//
static BOOL
IsLevelUniform(
    _In_ const HWVIEW* pView,
    _In_range_(<, HWNODE_TYPE_MAX) UINT32 level
    )
{
    UINT32 levelStart = pView->Strides[level];
    HWVIEWNODE nodeIt = pView->Nodes[levelStart];
    UINT32 childCount = nodeIt.LastChild - nodeIt.FirstChild + 1;

    while(nodeIt.NextSibling != HWNODEID_NONE)
    {
        nodeIt = pView->Nodes[nodeIt.NextSibling];
        if(nodeIt.LastChild - nodeIt.FirstChild + 1 != childCount)
        {
            return FALSE;
        }
    }

    return TRUE;
}


//
// According to the affinity layout that is specified, initializes the placement table
// at each level of the hardware tree
//
static void
InitializePlacementTable(
    _In_ SMPD_AFFINITY_PLACEMENT placement,
    _In_ HWNODE_TYPE             stride,
    _In_ const HWVIEW*           pView,
    _Out_writes_(HWNODE_TYPE_MAX) SMPD_AFFINITY_PLACEMENT *pTable
    )
{
    UINT32 startLevel = stride;

    if( placement == SMPD_AFFINITY_SPREAD )
    {
        while (--startLevel >= HWNODE_TYPE_NUMA)
        {
            if (!IsLevelUniform(pView, startLevel))
            {
                break;
            }
        }
        ++startLevel;
    }

    for( UINT32 currentLevel = 0; currentLevel < HWNODE_TYPE_MAX; ++currentLevel )
    {
        if( currentLevel < startLevel )
        {
            pTable[currentLevel] = SMPD_AFFINITY_DISABLED;
            continue;
        }

        switch( placement )
        {
        case SMPD_AFFINITY_SEQUENTIAL:
            pTable[currentLevel] = SMPD_AFFINITY_SEQUENTIAL;
            break;
        case SMPD_AFFINITY_BALANCED:
            pTable[currentLevel] = SMPD_AFFINITY_SEQUENTIAL;
            pTable[HWNODE_TYPE_NUMA] = SMPD_AFFINITY_BALANCED;
            break;
        case SMPD_AFFINITY_SPREAD:
            pTable[currentLevel] = SMPD_AFFINITY_SPREAD;
            break;
        default:
            ASSERT( false );
            break;
        }
    }
}


//
// Traverse launch nodes and assign affinity to them
//
static UINT32
SetAffinityToLaunchNodes(
    _In_ const HWVIEW                   *pView,
    _In_ const SMPD_AFFINITY_PLACEMENT  *pPlacement,
    const int                           procCount,
    HWNODE_TYPE                         affinityTarget,
    HWNODE_TYPE                         affinityStride,
    _Inout_ HWAFFINITY                  *pAffinity
    )
{
    UINT32 updateCount = 0;
    UINT32 endLevel = affinityStride;

    if( affinityTarget > affinityStride )
    {
        //
        // cases like N:L
        //
        endLevel = affinityTarget;
    }

    for( int procOrder = 0; procOrder < procCount; ++procOrder )
    {
        UINT32 affinityNode = GetLevelAffinity(
            pView,
            pPlacement,
            HWNODE_TYPE_MACHINE,
            endLevel,
            0,
            pView->Nodes[0].FirstChild,
            pView->Nodes[0].LastChild,
            procOrder,
            procCount
            );

        UINT32 endIdx = affinityStride;
        for( UINT32 currentIdx = affinityTarget;
            currentIdx < endIdx;
            ++currentIdx )
        {
            //
            // For cases like L:N, where the stride unit is smaller than the target unit,
            // move up the HW view and find the parent at desired target level
            //
            affinityNode = pView->Nodes[affinityNode].Parent;
        }

        GetActiveAffinityMask( pView, affinityNode, pAffinity + procOrder );
        ++updateCount;
    }

    return updateCount;
}


static HRESULT
SetAffinityDynamic(
    _In_ const AffinityOptions* pOptions,
    _In_ const HWVIEW*          pView,
    _Inout_ smpd_context_t*     pContext,
    _Inout_ wchar_t*            errorMsg,
    _In_    size_t              msgSize
    )
{
    SMPD_AFFINITY_PLACEMENT placement[HWNODE_TYPE_MAX];
    InitializePlacementTable(
        pOptions->placement,
        pOptions->stride,
        pView,
        placement
        );
    UINT32 setNodeCount = SetAffinityToLaunchNodes(
        pView,
        placement,
        pContext->processCount,
        pOptions->target,
        pOptions->stride,
        pContext->affinityList
        );

    if(setNodeCount == static_cast<UINT32>(pContext->processCount))
    {
        return S_OK;
    }
    else
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return, no good recovery for this error handling path");
        StringCchPrintfW(errorMsg, msgSize, L"Failed to set affinity for all processes.\n");
        return E_FAIL;
    }

}


static HRESULT
SetAffinityExplicit(
    _In_ const HWSUMMARY                    *pSummary,
    _In_ const smpd_context_t               *pContext,
    _Inout_ wchar_t*                        errorMsg,
    _In_ size_t                             msgSize
    )
{
    //
    //check validity of given affinity values
    //

    HWAFFINITY *pAffinity = pContext->affinityList;
    for(int i = 0; i < pContext->processCount; ++i)
    {
        if(pSummary-> Count < pAffinity[i].GroupId)
        {
            OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return, no good recovery for this error handling path");
            StringCchPrintfW(errorMsg, msgSize, L"An explicit affinity mask specified an invalid processor group.");
            return E_INVALIDARG;
        }

        UINT64 activeMask = pSummary->Infos[pAffinity[i].GroupId].ActiveMask;

        if( pAffinity[i].Mask != (activeMask & pAffinity[i].Mask) )
        {
            OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return, no good recovery for this error handling path");
            StringCchPrintfW(errorMsg, msgSize, L"An explicit affinity was used but the cores specified were either not allowed or do not exist.");
            return E_INVALIDARG;
        }
    }
    return S_OK;
}

_Pre_satisfies_( *ppPids == nullptr )
static DWORD GetAllProcessIds(
    _Inout_ DWORD* pProcessCount,
    _Inout_ DWORD** ppPids
    )
{
    DWORD pidAllocCount = 0;
    //
    // Start with a random number of expected processes, since EnumProcesses returns
    // only the number of processes it can enumerate into the buffer, and does not return
    // how many processes there are.
    //
    DWORD cbPidRetrieved = 64 * sizeof( DWORD );
    DWORD* pPids = nullptr;

    do
    {
        free( pPids );
        //
        // double the number of expected processes, since the buffer was not sufficient to
        // get all processes.
        //
        pidAllocCount = cbPidRetrieved << 1;

        pPids = static_cast<DWORD*>( malloc(pidAllocCount) );
        if( pPids == nullptr )
        {
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        BOOL succeeded = EnumProcesses( pPids, pidAllocCount, &cbPidRetrieved );
        if( !succeeded )
        {
            free( pPids );
            return GetLastError();
        }
    }while( pidAllocCount <= cbPidRetrieved );

    *pProcessCount = cbPidRetrieved / sizeof( DWORD );
    *ppPids = pPids;
    return NO_ERROR;
}


static DWORD GetOccupiedCores(_Inout_ HWSUMMARY* pSummary)
{
    DWORD*              pPids = nullptr;
    DWORD               processCount = 0;
    DWORD               status = NO_ERROR;

    status = GetAllProcessIds(&processCount, &pPids);
    if( status != NO_ERROR )
    {
        goto exitCleanUp;
    }

    for( DWORD i = 0; i < processCount; ++i )
    {
        wchar_t pidRegionName[MAX_PATH];
        HRESULT hr = StringCchPrintfW(
            pidRegionName,
            _countof(pidRegionName),
            sOccupiedCoreInfoRegion,
            pPids[i]
            );
        if( FAILED( hr ) )
        {
            smpd_err_printf(L"could not format name of memory section for affinities.");
            status = ERROR_INSUFFICIENT_BUFFER;
            goto exitCleanUp;
        }

        GlobalShmRegion shmRegion;
        DWORD res = shmRegion.Open(pidRegionName);
        if(res == ERROR_FILE_NOT_FOUND)
        {
            continue;
        }

        const AutoAffinityInfo* pNext = static_cast<const AutoAffinityInfo*>(shmRegion.ReadPtr());
        if(pNext == nullptr)
        {
            status = GetLastError();
            goto exitCleanUp;
        }

        if(pNext->published != sPublishedPattern || pNext->version != MSMPI_AA_VERSION)
        {
            //
            // Abnormally terminated or version mismatch
            //
            continue;
        }

        if(pNext->groupCount != pSummary->Count)
        {
            status = ERROR_INVALID_PARAMETER;
            goto exitCleanUp;
        }

        for(unsigned int j = 0; j < pSummary->Count; ++j)
        {
            pSummary->Infos[j].ActiveMask &= ~pNext->occupiedCoresMask[j];
        }
    }

exitCleanUp:
    free(pPids);
    return status;
}


HRESULT
ConstructHwInfo(
    _In_    BOOL         checkOccupied,
    _Inout_ UINT32*      pcbView,
    _Inout_ HWVIEW**     ppView,
    _Inout_ UINT32*      pcbTree,
    _Inout_ HWTREE**     ppTree,
    _Inout_ UINT32*      pcbSummary,
    _Inout_ HWSUMMARY**  ppSummary,
    _Inout_ wchar_t*     errorMsg,
    _In_    size_t       msgSize
    )
{
    HWSUMMARY*                 summary     = nullptr;
    HWTREE*                    hwTree      = nullptr;
    HWVIEW*                    hwView      = nullptr;
    HRESULT                    hr;

    hr = InitializeHW(HwSummaryInitialize, pcbSummary, &summary);
    MPIU_Assert(NULL != summary);
    if( FAILED( hr ) )
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return, no good recovery for this error handling path");
        StringCchPrintfW(errorMsg, msgSize, L"Failed to construct hwsummary for collect command. 0x%x\n", hr);
        return hr;
    }

    if(checkOccupied)
    {
        GetOccupiedCores(summary);
        UINT64 availableCores = 0;
        for(UINT32 i = 0; i < summary->Count; ++i)
        {
            availableCores |= summary->Infos[i].ActiveMask;
        }

        if(availableCores == 0)
        {
            OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return, no good recovery for this error handling path");
            StringCchPrintfW(errorMsg, msgSize, L"All cores are being used. No unused cores detected to set auto-affinity.");
            midl_user_free(summary);
            return E_FAIL;
        }
    }

    hr = InitializeHW(HwTreeInitialize, pcbTree, &hwTree, summary);
    if( FAILED(hr) )
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return, no good recovery for this error handling path");
        StringCchPrintfW(errorMsg, msgSize, L"Failed to construct hwtree for collect command. 0x%x\n", hr);
        midl_user_free(summary);
        return hr;
    }

    hr = InitializeHW(HwViewInitialize, pcbView, &hwView, hwTree, summary);
    if( FAILED(hr) )
    {
        OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignore return, no good recovery for this error handling path");
        StringCchPrintfW(errorMsg, msgSize, L"Failed to construct hwview for collect command. 0x%x\n", hr);
        midl_user_free(summary);
        midl_user_free(hwTree);
        return hr;
    }

    *ppSummary  = summary;
    *ppView     = hwView;
    *ppTree     = hwTree;
    return S_OK;
}


HANDLE LockAutoAffinity()
{
    HANDLE              mutex = nullptr;
    SECURITY_ATTRIBUTES security;

    ZeroMemory(&security, sizeof(security));
    security.nLength = sizeof(security);
    ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;;GA;;;WD)",
            SDDL_REVISION_1,
            &security.lpSecurityDescriptor,
            NULL);

    mutex = CreateMutexW(&security, FALSE, sAffinityAllocationLock);

    LocalFree(security.lpSecurityDescriptor);

    if(mutex == nullptr)
    {
        return nullptr;
    }

    if(WaitForSingleObject(mutex, sLockTimeout) != WAIT_OBJECT_0)
    {
        CloseHandle(mutex);
        return nullptr;
    }

    return mutex;
}


HRESULT WriteAutoAffinity(_In_ smpd_context_t* pContext, _In_ UINT32 groupCount)
{
    wchar_t pidRegionName[MAX_PATH];
    HRESULT hr = StringCchPrintfW(
        pidRegionName,
        _countof(pidRegionName),
        sOccupiedCoreInfoRegion,
        GetCurrentProcessId()
        );
    if( FAILED( hr ) )
    {
        smpd_err_printf(L"could not format name of memory section for affinities.");
        return hr;
    }

    UINT32 regionSize = AutoAffinityInfo::GetSize(groupCount);
    pContext->pAutoAffinityRegion = new GlobalShmRegion();
    pContext->pAutoAffinityRegion->Create(pidRegionName, regionSize);
    AutoAffinityInfo* pWrite = static_cast<AutoAffinityInfo*>(
        pContext->pAutoAffinityRegion->WritePtr()
        );

    if(pWrite == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    pWrite->published = 0;
    pWrite->version = MSMPI_AA_VERSION;
    pWrite->groupCount = groupCount;
    for(int i = 0; i < pContext->processCount; ++i)
    {
        pWrite->occupiedCoresMask[pContext->affinityList[i].GroupId] |= pContext->affinityList[i].Mask;
    }

    pWrite->published = sPublishedPattern;
    return S_OK;
}


void UnlockAutoAffinity(_In_ HANDLE affinityLock)
{
    ReleaseMutex(affinityLock);
    CloseHandle(affinityLock);
}


HRESULT
SetAffinity(
    _In_ const AffinityOptions* pOptions,
    _In_ const HWSUMMARY*       pSummary,
    _In_ const HWVIEW*          pView,
    _Inout_ smpd_context_t*     pContext,
    _Inout_ wchar_t*            errorMsg,
    _In_    size_t              msgSize
    )
{
    if(pOptions->isExplicit)
    {
        return SetAffinityExplicit(pSummary, pContext, errorMsg, msgSize);
    }

    return SetAffinityDynamic(pOptions, pView,  pContext, errorMsg, msgSize);
}
