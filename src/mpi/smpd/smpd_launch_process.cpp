// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"
#include "kernel32util.h"
#include "PmiDbgImpl.h"
#include "mpitrace.h"


struct SmpdLaunchHelper
{
    const SmpdLaunchCmd* pld;
    char* pExe;
    char* pArgs;

    SmpdLaunchHelper(const SmpdLaunchCmd* p): pld(p), pExe(nullptr), pArgs(nullptr){}
    ~SmpdLaunchHelper()
    {
        delete [] pExe;
        delete [] pArgs;
    }
};


static void
SetEnvironmentVariables(
    _In_ const wchar_t* const *envBlock,
    _In_ UINT16 envCount
    )
{
    for( UINT16 i = 0; i < envCount; i = i + 2 )
    {
        smpd_dbg_printf(L"setting environment variable: <%s> = <%s>\n",
                        envBlock[i], envBlock[i+1]);
        SetEnvironmentVariableW( envBlock[i], envBlock[i+1] );
    }
}


static void
RemoveEnvironmentVariables(
    _In_ const wchar_t* const *envBlock,
    _In_ UINT16 envCount
    )
{
    for( UINT16 i = 0; i < envCount; i = i + 2 )
    {
        SetEnvironmentVariableW( envBlock[i], nullptr );
    }
}


static DWORD
smpd_priority_class_to_win_class(
    _In_ DWORD priority_class
    )
{
    switch (priority_class)
    {
    case 0: return IDLE_PRIORITY_CLASS;
    case 1: return BELOW_NORMAL_PRIORITY_CLASS;
    case 2: return NORMAL_PRIORITY_CLASS;
    case 3: return ABOVE_NORMAL_PRIORITY_CLASS;
    case 4: return HIGH_PRIORITY_CLASS;
    default: return NORMAL_PRIORITY_CLASS;
    }
}


static const wchar_t s_prefixArr[] = L"\\\\?\\";

_Success_( return == true )
bool
ConvertLongPathToShortPath(
    _In_ PCWSTR longPath,
    _Out_writes_z_(length) wchar_t* shortPath,
    _In_ DWORD length
    )
{
    wchar_t tempBuffer[MAX_PATH + _countof(s_prefixArr) - 1];
    DWORD ret = GetShortPathNameW( longPath, tempBuffer, length );
    if( ret == 0 )
    {
        DWORD gle = GetLastError();
        smpd_dbg_printf(L"Failed to convert the provided path '%s' to short path error %u\n",
                        longPath,
                        gle);
        return false;
    }

    //
    // If the previous call to GetShortPathName is successful, the
    // resulting string should fit in a buffer size MAX_PATH after
    // the \\?\ prefix is stripped
    //
    HRESULT hr = StringCchCopyW( shortPath, length, tempBuffer + _countof(s_prefixArr) - 1);
    if( FAILED( hr ) )
    {
        smpd_dbg_printf(L"Failed to convert the provided path '%s' to short path error 0x%08x\n",
                        longPath,
                        hr);
        return false;
    }

    return true;
}


static bool
smpd_search_path(
    _In_opt_z_ PCWSTR path,
    _Out_writes_z_(cchSize) wchar_t* appExe,
    _In_ DWORD cchSize,
    _In_ const SmpdLaunchCmd* pld
    )
{
    //
    // Search the provided path using extension .exe Unlike
    // SearchPathA which leaves the out-buffer untouched if the
    // executable cannot be located, SearchPathW will zero out the
    // output buffer.
    //
    DWORD ret = SearchPathW(
        path,
        pld->appExe,
        L".exe",
        cchSize,
        appExe,
        nullptr);

    if((ret == 0) || (ret > cchSize))
    {
        return false;
    }

    return true;
}


static bool
smpd_resolve_exe_name(
    _Out_writes_z_(cchSize) wchar_t* appExe,
    _In_ DWORD cchSize,
    _In_ PCWSTR wdir,
    _In_ const SmpdLaunchCmd* pld
    )
{
    //
    // Search the expanded process workdir
    //
    smpd_dbg_printf(L"searching for '%s' in workdir '%s'\n", pld->appExe, wdir);
    if(smpd_search_path(pld->wdir, appExe, cchSize, pld))
    {
        return true;
    }

    //
    // Search the process path
    //
    smpd_dbg_printf(L"searching for '%s' in path '%s'\n", pld->appExe, pld->path);
    if(smpd_search_path(pld->path, appExe, cchSize, pld))
    {
        return true;
    }

    //
    // Search the system path
    //
    smpd_dbg_printf(L"searching for '%s' in system path\n", pld->appExe);
    if(smpd_search_path(nullptr, appExe, cchSize, pld))
    {
        return true;
    }

    //
    // File not found
    //
    smpd_dbg_printf(L"failed to find '%s', launching executable as provided\n", pld->appExe);
    return false;
}


static void
smpd_add_block_pmi_environment(
    _In_ const SmpdLaunchCmd* pld
    )
{
    wchar_t str[MPI_MAX_PORT_NAME + 1];

    MPIU_Snprintf(str, _countof(str), L"%hu", pld->nproc);
    smpd_dbg_printf(L"env: PMI_SIZE=%s\n", str);
    SetEnvironmentVariableW(L"PMI_SIZE", str);

    GuidToStr( pld->kvs, str, _countof(str) );
    smpd_dbg_printf(L"env: PMI_KVS=%s\n", str);
    SetEnvironmentVariableW(L"PMI_KVS", str);

    GuidToStr( pld->domain, str, _countof(str) );
    smpd_dbg_printf(L"env: PMI_DOMAIN=%s\n", str);
    SetEnvironmentVariableW(L"PMI_DOMAIN", str);

    smpd_dbg_printf(L"env: PMI_HOST=%s\n", SMPD_PMI_HOST);
    SetEnvironmentVariableW(L"PMI_HOST", SMPD_PMI_HOST);

    GuidToStr( smpd_process.localServerPort, str, _countof(str) );
    smpd_dbg_printf(L"env: PMI_PORT=%s\n", str);
    SetEnvironmentVariableW(L"PMI_PORT", str);

    MPIU_Snprintf(str, _countof(str), L"%hd", smpd_process.tree_id);
    smpd_dbg_printf(L"env: PMI_SMPD_ID=%s\n", str);
    SetEnvironmentVariableW(L"PMI_SMPD_ID", str);

    MPIU_Snprintf(str, _countof(str), L"%hu", pld->appNum);
    smpd_dbg_printf(L"env: PMI_APPNUM=%s\n", str);
    SetEnvironmentVariableW(L"PMI_APPNUM", str);

    MPIU_Snprintf(str, _countof(str), L"%hu", (pld->fSpawned ? 1 : 0));
    smpd_dbg_printf(L"env: PMI_SPAWN=%s\n", str);
    SetEnvironmentVariableW(L"PMI_SPAWN", str);

    if (pld->fSpawned)
    {
        MPIU_Snprintf(str, _countof(str), L"%S", pld->parentPortName);
        smpd_dbg_printf(L"env: PMI_PARENT_PORT_NAME=%s\n", str);
        SetEnvironmentVariableA("PMI_PARENT_PORT_NAME", pld->parentPortName);
    }

    smpd_dbg_printf(L"env: PMI_NODE_IDS=%s\n", smpd_process.node_id_region_name);
    SetEnvironmentVariableW(L"PMI_NODE_IDS", smpd_process.node_id_region_name);

    if ((smpd_process.rank_affinities != NULL) && (!pld->fSpawned))
    {
        smpd_dbg_printf(L"env: PMI_RANK_AFFINITIES=%s\n", smpd_process.rank_affinity_region_name);
        SetEnvironmentVariableW(L"PMI_RANK_AFFINITIES", smpd_process.rank_affinity_region_name);
    }
}


static void
smpd_add_pmi_environment(
    _In_ const smpd_process_t* process
    )
{
    wchar_t str[GUID_STRING_LENGTH + 1];
    MPIU_Snprintf(str, _countof(str), L"%hu", process->rank);
    smpd_dbg_printf(L"env: PMI_RANK=%s\n", str);
    SetEnvironmentVariableW(L"PMI_RANK", str);


    MPIU_Snprintf(str, _countof(str), L"%hu", process->id);
    smpd_dbg_printf(L"env: PMI_SMPD_KEY=%s\n", str);
    SetEnvironmentVariableW(L"PMI_SMPD_KEY", str);

}


static void
smpd_remove_pmi_environment()
{
    SetEnvironmentVariableW(L"PMI_RANK", NULL);
    SetEnvironmentVariableW(L"PMI_SMPD_KEY", NULL);
}


static void
smpd_remove_block_pmi_environment()
{

    SetEnvironmentVariableW(L"PMI_SIZE", NULL);
    SetEnvironmentVariableW(L"PMI_KVS", NULL);
    SetEnvironmentVariableW(L"PMI_DOMAIN", NULL);
    SetEnvironmentVariableW(L"PMI_HOST", NULL);
    SetEnvironmentVariableW(L"PMI_PORT", NULL);
    SetEnvironmentVariableW(L"PMI_SMPD_ID", NULL);
    SetEnvironmentVariableW(L"PMI_APPNUM", NULL);
    SetEnvironmentVariableW(L"PMI_NODE_IDS", NULL);
    SetEnvironmentVariableW(L"PMI_RANK_AFFINITIES", NULL);

}


static void
smpd_ensure_rank_affinity_section_created(
    UINT16 rankcount
    )
{
    if (smpd_process.rank_affinities != nullptr )
    {
        return;
    }

    DWORD buf_size = rankcount * sizeof(HWAFFINITY);

    //
    //format name of the memory section for affinities
    //
    HRESULT hr = StringCchPrintfW( smpd_process.rank_affinity_region_name,
                                   _countof(smpd_process.rank_affinity_region_name),
                                   L"affinity_region_%u",
                                   GetCurrentProcessId() );
    if( FAILED( hr ) )
    {
        smpd_err_printf(L"could not format name of memory section for affinities.");
        return;
    }

    //
    // Create shm region
    //
    smpd_process.rank_affinity_region =
        CreateFileMappingW( INVALID_HANDLE_VALUE,  //use the page file
                            NULL,                  //default security
                            PAGE_READWRITE,        //read/write access
                            0,                     //object size, high-order dword
                            buf_size,              //object size, low-order dword
                            smpd_process.rank_affinity_region_name); // region name

    if( smpd_process.rank_affinity_region == nullptr )
    {
        smpd_err_printf(L"could not create shared memory region for rank affinity mapping (%u).",
                        GetLastError());
        return;
    }

    //Get a pointer to the region
    smpd_process.rank_affinities = static_cast<HWAFFINITY*>( MapViewOfFile(
        smpd_process.rank_affinity_region,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        buf_size ) );

    if( smpd_process.rank_affinities == nullptr )
    {
        smpd_err_printf(L"could not map view of shared memory region for rank affinity mapping.");
        return;
    }

    //
    // Reset all affinity data, we will add entries as new processes
    // are launched for local ranks. Remote ranks will remain 0.
    //
    ZeroMemory(smpd_process.rank_affinities, buf_size);
}


static
void
FillErrorMsg(
    _In_ const SmpdLaunchCmd* pld,
    _In_ PCWSTR appExe,
    _In_ DWORD err,
    _Out_ SmpdLaunchRes* pLaunchRes
    )
{
    pLaunchRes->error_msg = static_cast<wchar_t*>(
        midl_user_allocate( SMPD_MAX_ERROR_LEN * sizeof(wchar_t)));
    if( pLaunchRes->error_msg == nullptr )
    {
        smpd_err_printf(L"Failed to allocate memory to send back error\n");
        pLaunchRes->count = 0;
    }
    else
    {
        smpd_translate_win_error(
            err,
            pLaunchRes->error_msg,
            SMPD_MAX_ERROR_LEN,
            const_cast<wchar_t*>(L"failed to launch %.0s'%s' on %s\nError (%u) "),
            appExe,  // ignored by the above format; is to help windows error 193 format
            pld->appArgs,
            smpd_process.host,
            err
            );

        pLaunchRes->count = static_cast<UINT32>(
            MPIU_Strlen( pLaunchRes->error_msg, SMPD_MAX_ERROR_LEN ) + 1 );
    }
}


static int
count_commas(
    _In_z_ const wchar_t* p
    )
{
    int n = 0;
    while (*p != L'\0')
    {
        if (*p == L',')
        {
            n++;
        }
        p++;
    }
    return n;
}


static
DWORD
gle_retrieve_HPCPack_affinity(
    _Inout_     int*         pHPCGroupNum,
    _Outptr_result_maybenull_ HWAFFINITY** ppHPCAffinity
    )
{
    int cHPCGroupNum = -1;
    HWAFFINITY* pHPCAffinity = nullptr;

    DWORD gle = NOERROR;

    //
    // CCP_AFFINITY env variable is a list of HEX representation of affinity masks 
    // for every processor group, separated by comma.
    // There is max of 4 processor groups.
    // Max size of processor group is 64 = 16 characters in HEX representation.
    // Max length would be 16*4+3 = 67, 128 characters should be more than enough.
    //
    wchar_t p[128];
    gle = MPIU_Getenv(L"CCP_AFFINITY", p, _countof(p));
    if (gle != NOERROR)
    {
        if (gle == ERROR_ENVVAR_NOT_FOUND)
        {
            *pHPCGroupNum = 0;
            gle = NOERROR;
        }
        goto fn_exit;
    }

    //
    // Read the context of CCP_AFFINITY env variable
    // into HWAFFINITY structures
    //
    cHPCGroupNum = count_commas(p) + 1;
    pHPCAffinity = static_cast<HWAFFINITY*>(MPIU_Malloc(sizeof(HWAFFINITY) * cHPCGroupNum));
    if (pHPCAffinity == nullptr)
    {
        gle = ERROR_NOT_ENOUGH_MEMORY;
        goto fn_exit;
    }

    UINT16 n = 0;
    const wchar_t* curr = p;
    while (n < cHPCGroupNum)
    {
        wchar_t *end;

        pHPCAffinity[n].GroupId = n;
        pHPCAffinity[n].Mask = _wcstoui64(curr, &end, 16);

        if (cHPCGroupNum == 1 && pHPCAffinity[n].Mask == 0)
        {
            cHPCGroupNum = 0;
            MPIU_Free(pHPCAffinity);
            pHPCAffinity = nullptr;
            goto fn_exit;
        }
        if (*end == L',')
        {
            curr = end + 1;
        }
        n++;
    }

fn_exit:
    *pHPCGroupNum = cHPCGroupNum;
    *ppHPCAffinity = pHPCAffinity;
    return gle;
}


static UINT16 num_set_bits(UINT64 mask)
{
    UINT16 res = 0;
    while (mask)
    {
        res += mask & 1;
        mask = mask >> 1;
    }
    return res;
}


static HWAFFINITY*
smpd_next_HPCPack_affinity(
    _In_ smpd_context_t* pContext,
    _In_ HWAFFINITY* pHPCPackAffinity,
    _In_ int cHPCPackGroups,
    _In_ UINT16 index
    )
{
    UINT16 nProcGroup = 0;
    UINT64 mask = 0;
    UINT16 groupId = 0;

    int curSum = 0;

    while (true)
    {
        UINT16 numSetBits = num_set_bits(pHPCPackAffinity[nProcGroup].Mask);
        if (curSum + numSetBits <= index)
        {
            curSum += numSetBits;
            nProcGroup = (nProcGroup + 1) % cHPCPackGroups;
            continue;
        }
        else
        {
            groupId = nProcGroup;
            for (UINT16 i = 0; i < 64; i++)
            {
                if ((pHPCPackAffinity[nProcGroup].Mask & (1i64 << i)) != 0)
                {
                    if (curSum == index)
                    {
                        mask = (1i64 << i);
                        break;
                    }
                    else
                    {
                        curSum++;
                    }
                }
            }
            break;
        }
    }

    pContext->affinityList[index].Mask = mask;
    pContext->affinityList[index].GroupId = groupId;

    return &pContext->affinityList[index];
}


//
// Retrieves the information about the relationships of 
// logical processors and related hardware.
//
static
DWORD
gle_retrieve_numa_info()
{
    DWORD gle = NOERROR;

    smpd_process.cNumaNodeInfoLen = 0;
    BOOL fSucc = Kernel32::Methods.GetLogicalProcessorInformationEx(
        RelationNumaNode,
        nullptr,
        &smpd_process.cNumaNodeInfoLen);
    if (!fSucc)
    {
        gle = GetLastError();
        if (gle != ERROR_INSUFFICIENT_BUFFER)
        {
            goto fn_exit;
        }
        else
        {
            gle = NOERROR;
        }
    }

    smpd_process.pNumaNodeInfo =
        static_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(MPIU_Malloc(smpd_process.cNumaNodeInfoLen));

    if (smpd_process.pNumaNodeInfo == nullptr)
    {
        gle = ERROR_NOT_ENOUGH_MEMORY;
        goto fn_exit;
    }

    fSucc = Kernel32::Methods.GetLogicalProcessorInformationEx(
        RelationNumaNode,
        smpd_process.pNumaNodeInfo,
        &smpd_process.cNumaNodeInfoLen);
    if (!fSucc)
    {
        MPIU_Free(smpd_process.pNumaNodeInfo);
        gle = GetLastError();
        goto fn_exit;
    }

fn_exit:
    return gle;
}


static
DWORD
gle_create_mpi_process(
    _Inout_ smpd_process_t* process,
    _In_ const SmpdLaunchHelper* pldHelper,
    _In_opt_ const HWAFFINITY* pAffinity,
    _In_ PCWSTR appExe,
    _In_ HANDLE hStdin,
    _In_ HANDLE hStdout,
    _In_ HANDLE hStderr,
    _In_ bool   shortPath,
    _When_(return != NOERROR, _Out_) SmpdLaunchRes* pLaunchRes
)
{
    DWORD gle = NOERROR;
    STARTUPINFOEXW si;
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.hStdInput = hStdin;
    si.StartupInfo.hStdOutput = hStdout;
    si.StartupInfo.hStdError = hStderr;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

    const SmpdLaunchCmd* pld = pldHelper->pld;
    process->kvs = pld->kvs;

    //
    // Set the per-process PMI environment variables
    //
    smpd_add_pmi_environment(process);

    enum MSMPI_DUMP_MODE dumpMode = GetDumpMode();
    if (dumpMode == MsmpiDumpAllMini || dumpMode == MsmpiDumpAllFull)
    {
        if (MPIU_Getenv(L"MSMPI_DUMP_PATH",
            process->dump_path,
            _countof(process->dump_path)) != NOERROR)
        {
            process->dump_path[0] = L'\0';
        }

        if (dumpMode == MsmpiDumpAllFull)
        {
            process->dump_type = MiniDumpWithFullMemory;
        }
        else
        {
            process->dump_type = MiniDumpNormal;
        }

        //
        // Capture job ID, task ID, and task instance ID while the
        // environment variables are in scope.
        //
        process->dump_job_id = env_to_int(L"CCP_JOBID", 0, 0);
        process->dump_task_id = env_to_int(L"CCP_TASKID", 0, 0);
        process->dump_taskinstance_id = env_to_int(L"CCP_TASKINSTANCEID", 0, 0);
    }

    DWORD flags =
        CREATE_NO_WINDOW |
        CREATE_SUSPENDED |
        CREATE_UNICODE_ENVIRONMENT |
        smpd_priority_class_to_win_class(pld->priorityClass);

    NotifyPmiDbgExtensions(SmpdNotifyBeforeCreateProcess,
        pldHelper->pExe,
        pldHelper->pArgs,
        process->rank);

    Trace_SMPD_Context_info(process->rank, smpd_process.job_context);

    //
    // CreateProcessW requires commandline to be writable. We do not
    // want the original pld->appArgs to be overwritten because it's
    // part of the launching block and the data will be used to launch
    // all processes in this launching block
    //
    size_t argLen = MPIU_Strlen(pld->appArgs);
    wchar_t* dupCommandLine = new wchar_t[argLen + 1];
    if (dupCommandLine == nullptr)
    {
        FillErrorMsg(pld, appExe, ERROR_NOT_ENOUGH_MEMORY, pLaunchRes);
        goto fn_exit;
    }

    if (shortPath != true)
    {
        MPIU_Strcpy(dupCommandLine, argLen + 1, pld->appArgs);
    }
    else
    {
        MPIU_Strcpy(dupCommandLine, argLen + 1, pld->appArgs + _countof(s_prefixArr) - 1);
    }

    if (pAffinity != nullptr && pAffinity->Mask != 0)
    {
        if (g_IsWin7OrGreater)
        {
            //
            // iterate over NUMA nodes to find the first one that belongs to the specified group
            //
            USHORT numaNodeNumber = 0xFFFF;
            SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* pInfoIter = smpd_process.pNumaNodeInfo;
            DWORD len = smpd_process.cNumaNodeInfoLen;
            while (len > 0)
            {
                NUMA_NODE_RELATIONSHIP numaNode = pInfoIter->NumaNode;
                GROUP_AFFINITY groupMask = numaNode.GroupMask;
                if (groupMask.Group == pAffinity->GroupId)
                {
                    //
                    // it is not documented, but the size of the PROC_THREAD_ATTRIBUTE_PREFERRED_NODE
                    // attribute that is passed to UpdateProcThreadAttribute must be 2 bytes
                    //
                    numaNodeNumber = static_cast<USHORT>(numaNode.NodeNumber);
                    break;
                }
                else
                {
                    len -= pInfoIter->Size;
                    pInfoIter = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                        reinterpret_cast<BYTE*>(pInfoIter) + pInfoIter->Size);
                    
                }
            }

            SIZE_T AttributeListSize = 0;

            BOOL fSucc = Kernel32::Methods.InitializeProcThreadAttributeList(NULL, 1, 0, &AttributeListSize);
            if (!fSucc)
            {
                gle = GetLastError();
                if (gle != ERROR_INSUFFICIENT_BUFFER)
                {
                    FillErrorMsg(pld, appExe, gle, pLaunchRes);
                    goto fn_exit;
                }
                else
                {
                    gle = NOERROR;
                }
            }

            si.lpAttributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(MPIU_Malloc(AttributeListSize));
            if (si.lpAttributeList == nullptr)
            {
                gle = ERROR_NOT_ENOUGH_MEMORY;
                FillErrorMsg(pld, appExe, gle, pLaunchRes);
                goto fn_exit;
            }

            fSucc = Kernel32::Methods.InitializeProcThreadAttributeList(si.lpAttributeList,
                1,
                0,
                &AttributeListSize);
            if (!fSucc)
            {
                gle = GetLastError();
                FillErrorMsg(pld, appExe, gle, pLaunchRes);
                goto fn_exit;
            }

            fSucc = Kernel32::Methods.UpdateProcThreadAttribute(si.lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_PREFERRED_NODE,
                &numaNodeNumber,
                sizeof(numaNodeNumber),
                NULL,
                NULL);
            if (!fSucc)
            {
                gle = GetLastError();
                FillErrorMsg(pld, appExe, gle, pLaunchRes);
                goto fn_exit;
            }

            flags = flags | EXTENDED_STARTUPINFO_PRESENT;
        }
    }

    PROCESS_INFORMATION pi;
    BOOL fSucc = OACR_REVIEWED_CALL(
        mpicr,
        CreateProcessW(
            appExe,                   // lpApplicationName
            dupCommandLine,           // lpCommandLine (windows requires cmdline to be writeable)
            nullptr,                  // lpProcessAttributes
            nullptr,                  // lpThreadAttributes
            TRUE,                     // bInheritHandles
            flags,                    // dwCreationFlags
            nullptr,                  // lpEnvironment
            nullptr,                  // lpCurrentDirectory
            (LPSTARTUPINFOW)&si,      // lpStartupInfo
            &pi                       // lpProcessInformation
        ));
    if (!fSucc)
    {
        delete[] dupCommandLine;
        gle = GetLastError();
        FillErrorMsg(pld, appExe, gle, pLaunchRes);
        goto fn_exit;
    }
    delete[] dupCommandLine;

    if (pAffinity != nullptr && pAffinity->Mask != 0)
    {
        if (g_IsWin7OrGreater)
        {
            //
            // MSDN states that a max of 4 groups is supported.
            //
            USHORT groups[4];
            USHORT nGroups = _countof(groups);

            //
            // Check the group affinity of newly created process is the one that
            // we wanted, and set the affinity mask
            //

            fSucc = Kernel32::Methods.GetProcessGroupAffinity(pi.hProcess, &nGroups, groups);
            if (!fSucc)
            {
                gle = GetLastError();
                FillErrorMsg(pld, appExe, gle, pLaunchRes);
                goto fn_exit;
            }
            if (nGroups == 1 && groups[0] == pAffinity->GroupId)
            {
                SetProcessAffinityMask(pi.hProcess,
                    static_cast<KAFFINITY>(pAffinity->Mask));
            }
        }
        else
        {
            ASSERT(pAffinity->GroupId == 0);
            SetProcessAffinityMask(pi.hProcess, static_cast<KAFFINITY>(pAffinity->Mask));
            SetThreadAffinityMask(pi.hThread, static_cast<KAFFINITY>(pAffinity->Mask));
        }
    }

    //
    // Assign this process to this manager's job object.
    //
    if (smpd_process.hJobObj != nullptr)
    {
        fSucc = AssignProcessToJobObject(
            smpd_process.hJobObj,
            pi.hProcess);
        if (!fSucc)
        {
            gle = GetLastError();
            smpd_dbg_printf(L"failed to assign process to job object, error %u\n", gle);
            CloseHandle(pi.hThread);

            TerminateProcess(pi.hProcess, gle);
            CloseHandle(pi.hProcess);

            FillErrorMsg(pld, appExe, gle, pLaunchRes);
            goto fn_exit;
        }
    }

    BOOL bSuspendResume = FALSE;
    NotifyPmiDbgExtensions(SmpdNotifyAfterCreateProcess,
        pldHelper->pExe,
        pldHelper->pArgs,
        process->rank,
        &pi,
        &bSuspendResume);

    if (bSuspendResume == FALSE)
    {
        ResumeThread(pi.hThread);
    }

    process->pid = pi.dwProcessId;
    if (pAffinity != nullptr)
    {
        process->launch_affinity = *pAffinity;
    }

    process->wait.hProcess = pi.hProcess;
    process->wait.hThread = pi.hThread;

    if (smpd_process.rank_affinities != nullptr && pAffinity != nullptr)
    {
        smpd_process.rank_affinities[process->rank] = *pAffinity;
    }

fn_exit:
    smpd_remove_pmi_environment();
    return gle;
}


enum std_inout { siInp, siOut, siErr };

static DWORD
gle_create_std_redirection(
    _In_ ExSetHandle_t set,
    _In_ smpd_process_t* process,
    _In_ HANDLE hPipe[3])
{
    DWORD gle;
    gle = gle_create_stdin_pipe(set, process->id, process->rank, process->kvs, &hPipe[siInp], &process->in);
    if(gle != NO_ERROR)
    {
        smpd_err_printf(L"failed to create stdin forwarding pipe, error %u\n", gle);
        goto fn_fail1;
    }

    gle = gle_create_stdout_pipe(set, process->id, process->rank, process->kvs, &hPipe[siOut]);
    if(gle != NO_ERROR)
    {
        smpd_err_printf(L"failed to create stdout forwarding pipe, error %u\n", gle);
        goto fn_fail2;
    }

    process->context_refcount++;

    gle = gle_create_stderr_pipe(set, process->id, process->rank, process->kvs, &hPipe[siErr]);
    if(gle != NO_ERROR)
    {
        smpd_err_printf(L"failed to create stderr forwarding pipe, error %u\n", gle);
        goto fn_fail3;
    }

    process->context_refcount++;

    return NO_ERROR;

fn_fail3:
    CloseHandle(hPipe[siOut]);
fn_fail2:
    CloseHandle(hPipe[siInp]);
    smpd_free_context(process->in);
    process->in = NULL;
fn_fail1:
    return gle;
}


static void smpd_close_std_pipes(HANDLE hPipe[3])
{
    int j;

    for(j = 0; j < 3; j++)
    {
        if(hPipe[j] != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hPipe[j]);
        }
    }
}


static HWAFFINITY*
smpd_next_proc_affinity(
    _In_ const smpd_context_t* pContext
    )
{
    static unsigned int i = 0;
    if( pContext->affinityList == nullptr )
    {
        return nullptr;
    }

    return &pContext->affinityList[i++];
}


static
void
KillFailedLaunchBlock(
    _In_ const smpd_process_t* prevProc
    )
{
    //
    // Kill all the processes we have launched for this block
    // by walking the list of processes and kill everything until
    // we hit the last successfully launched process of the
    // previous launching block
    //
    smpd_process_t* proc = smpd_get_first_process();
    while( proc != nullptr )
    {
        if( proc == prevProc )
        {
            break;
        }

        //
        // smpd_kill_process will remove the process from the list
        // and destroy the process structure. We obtain the next process
        // in advance
        //
        smpd_process_t* nextProc = proc->next;
        smpd_kill_process( proc, -1 );

        proc = nextProc;
    }
}


DWORD
smpd_launch_processes(
    _In_ smpd_context_t* pContext,
    _In_ const SmpdLaunchCmd* pld,
    _Out_ SmpdLaunchRes* pLaunchRes,
    _In_ ExSetHandle_t set
    )
{
    DWORD gle = NOERROR;
    SmpdLaunchHelper pldHelper(pld);

    int cHPCGroupNum = -1;
    HWAFFINITY* pHPCAffinity;

    //
    // We need to call smpd_ensure_rank_affinity_section_created()
    // first because it formats
    // smpd_process.rank_affinity_region_name, which is used by
    // smpd_add_block_pmi_environment() to set PMI_RANK_AFFINITIES
    //
    smpd_ensure_rank_affinity_section_created( static_cast<UINT16>(smpd_process.nproc) );

    //
    // Set the application env vars
    //
    SetEnvironmentVariables( pld->env, pld->envCount );
    smpd_add_block_pmi_environment( pld );


    //
    // Expand the working directory string
    // If ExpandEnvironmentStrings fails, use workdir as is
    //
    wchar_t wdir[MAX_PATH];
    wdir[0] = L'\0';

    DWORD err = ExpandEnvironmentStringsW( pld->wdir, wdir, _countof(wdir) );
    if( err == 0 || err > _countof(wdir) )
    {
        MPIU_Strcpy( wdir, _countof(wdir), pld->wdir );
    }

    //
    // Obtain the current working directory of SMPD so we can restore it
    // later
    //
    wchar_t tmpdir[MAX_PATH];
    DWORD dirLen = GetCurrentDirectoryW( _countof(tmpdir), tmpdir );
    BOOL fSucc = (dirLen > 0 && dirLen <= _countof(tmpdir));

    if(fSucc)
    {
        //
        // Set the application working directory.
        // This directory might be used to save the trace file
        //
        fSucc = SetCurrentDirectoryW( wdir );
    }

    if(!fSucc)
    {
        gle = GetLastError();
        smpd_err_printf(L"failed to set workdir to %s, error %u\n", pld->wdir, gle);

        pLaunchRes->error_msg = static_cast<wchar_t*>(
            midl_user_allocate( SMPD_MAX_ERROR_LEN * sizeof(wchar_t) ));
        if( pLaunchRes->error_msg == nullptr )
        {
            smpd_err_printf(L"Failed to allocate memory to send back error\n");
            pLaunchRes->count = 0;
            goto fn_exit;
        }

        smpd_translate_win_error(
            gle,
            pLaunchRes->error_msg,
            SMPD_MAX_ERROR_LEN,
            const_cast<wchar_t*>(L"failed to set work directory to '%s' on %s\nError (%u) "),
            wdir,
            smpd_process.host,
            gle
            );
        pLaunchRes->count = static_cast<UINT32>(
            MPIU_Strlen( pLaunchRes->error_msg, SMPD_MAX_ERROR_LEN ) + 1 );

        goto fn_exit;
    }

    //
    // Resolve the exe name using the expanded workdir
    //
    wchar_t appExe[UNICODE_STRING_MAX_CHARS];
    if( smpd_resolve_exe_name(appExe, _countof(appExe), wdir, pld) == false )
    {
        //
        // We could not resolve the exe name using either the provided
        // path (through -path or -gpath), the working directory
        // (-wdir, -gwdir), or the system path. Note that for
        // executable without a specified extension, we search for
        // .exe. It is possible that the user is expecting a .cmd or
        // .bat file. In that case we are passing the specified
        // executable as-is to CreateProcessW
        //
        MPIU_Strcpy( appExe, _countof(appExe), pld->appExe );
    }

    const wchar_t* pApp = appExe;
    wchar_t shortPath[MAX_PATH];
    bool useShortPath = false;
    if( env_is_on(L"MSMPI_USE_SHORTPATH", FALSE) == TRUE )
    {
        if( !( pApp[0] == s_prefixArr[0] &&
               pApp[1] == s_prefixArr[1] &&
               pApp[2] == s_prefixArr[2] &&
               pApp[3] == s_prefixArr[3] ) )
        {
            //
            // If the user did not provide us with a long path prefix, there is
            // no need to do any conversion.
            //
            smpd_dbg_printf(L"Only long path with \\\\?\\ prefix will be converted\n");
        }
        else
        {
            if (ConvertLongPathToShortPath(appExe, shortPath, _countof(shortPath)) == false)
            {
                smpd_err_printf(L"Failed to convert '%s' to short path form\n", pld->appExe);
                gle = ERROR_INVALID_PARAMETER;
                FillErrorMsg(pld, appExe, gle, pLaunchRes);
                goto fn_exit;
            }
            useShortPath = true;
            pApp = shortPath;
        }
    }

    pLaunchRes->ctxkeyArray = static_cast<UINT16*>(
        midl_user_allocate(sizeof(UINT16) * pld->rankCount));
    if (pLaunchRes->ctxkeyArray == nullptr)
    {
        pLaunchRes->rankCount = 0;
        smpd_err_printf(L"insufficient memory to launch process.\n");
        gle = ERROR_NOT_ENOUGH_MEMORY;
        FillErrorMsg(pld, appExe, gle, pLaunchRes);
        goto fn_exit;
    }
    pLaunchRes->rankCount = pld->rankCount;

    //
    // Convert the executable name and arguments back to ANSI
    // because PmiDbg extensions are currently ANSI only
    //
    gle = MPIU_WideCharToMultiByte(appExe, &pldHelper.pExe);
    if (gle != NOERROR)
    {
        FillErrorMsg(pld, appExe, gle, pLaunchRes);
        goto fn_exit;
    }

    if (pld->appArgs != nullptr)
    {
        gle = MPIU_WideCharToMultiByte(pld->appArgs, &pldHelper.pArgs);
        if (gle != NOERROR)
        {
            FillErrorMsg(pld, appExe, gle, pLaunchRes);
            goto fn_exit;
        }
    }

    smpd_numchildren_t* findNumChildrenNode = smpd_process.num_children_list;
    while (findNumChildrenNode != nullptr)
    {
        if (IsEqualGUID(findNumChildrenNode->kvs, pld->kvs))
        {
            break;
        }
        findNumChildrenNode = findNumChildrenNode->next;
    }

    if (findNumChildrenNode == nullptr)
    {
        smpd_numchildren_t* numChildrenNode = new smpd_numchildren_t;
        if (numChildrenNode == nullptr)
        {
            smpd_err_printf(L"insufficient memory to launch process.\n");
            gle = ERROR_NOT_ENOUGH_MEMORY;
            FillErrorMsg(pld, appExe, gle, pLaunchRes);
            goto fn_exit;
        }
        numChildrenNode->next = smpd_process.num_children_list;
        numChildrenNode->numChildren = pld->totalRankCount;
        numChildrenNode->kvs = pld->kvs;
        smpd_process.num_children_list = numChildrenNode;
    }

    //
    // Retrieve the information about the relationships of 
    // logical processors and related hardware.
    //
    if (g_IsWin7OrGreater)
    {
        if (smpd_process.pNumaNodeInfo == nullptr)
        {
            gle = gle_retrieve_numa_info();
            if (gle != NOERROR)
            {
                FillErrorMsg(pld, appExe, gle, pLaunchRes);
                goto fn_exit;
            }
        }
    }

    //
    // Obtain affinity masks from HPC Pack environment
    //
    gle = gle_retrieve_HPCPack_affinity(&cHPCGroupNum, &pHPCAffinity);
    if (gle != NOERROR)
    {
        FillErrorMsg(pld, appExe, gle, pLaunchRes);
        goto fn_exit;
    }

    //
    // See if HPC Affinities are in conflict with mpiexec affinities
    //
    bool fIgnoreMpiexecAffinities = false;
    if (pContext->affinityList == nullptr && pHPCAffinity != nullptr)
    {
        fIgnoreMpiexecAffinities = true;
    }

    if (pHPCAffinity != nullptr && !fIgnoreMpiexecAffinities)
    {
        for (UINT16 i = 0; i < pld->rankCount; ++i)
        {
            HWAFFINITY* pAff = &pContext->affinityList[i];
            if (pAff != nullptr && pHPCAffinity != nullptr)
            {
                if ((pHPCAffinity[pAff->GroupId].Mask & pAff->Mask) == 0)
                {
                    fIgnoreMpiexecAffinities = true;
                    break;
                }
            }
        }
    }

    //
    // Obtain the last successfully launched process
    //
    smpd_process_t* prevProc = smpd_get_first_process();
    for( UINT16 i = 0; i < pld->rankCount; ++i )
    {
        smpd_process_t* process = smpd_create_process_struct( pld->rankArray[i], pld->kvs );
        if( process == nullptr )
        {
            smpd_err_printf(L"insufficient memory to launch process.\n");
            gle = ERROR_NOT_ENOUGH_MEMORY;

            FillErrorMsg( pld, appExe, gle, pLaunchRes );
            KillFailedLaunchBlock( prevProc );
            goto fn_exit;
        }

        pLaunchRes->ctxkeyArray[i] = process->id;

        //
        // process stdin/out/err redirection pipes
        //
        HANDLE hPipe[3];

        gle = gle_create_std_redirection(set, process, hPipe);
        if(gle != NO_ERROR)
        {
            smpd_err_printf(L"failed to start std redirection for rank %hu, error %u\n",
                            process->rank, gle);
            FillErrorMsg( pld, appExe, gle, pLaunchRes );
            smpd_free_process_struct( process );
            KillFailedLaunchBlock( prevProc );
            goto fn_exit;
        }

        smpd_dbg_printf(L"%s>CreateProcess(%s %s)\n", wdir, appExe, pld->appArgs);

        HWAFFINITY* pAff = nullptr;
        if (fIgnoreMpiexecAffinities)
        {
            if (pContext->affinityList == nullptr)
            {
                size_t affinityLen = sizeof(HWAFFINITY) * pContext->processCount;
                pContext->affinityList = static_cast<HWAFFINITY*>(malloc(affinityLen));
                if (pContext->affinityList == nullptr)
                {
                    smpd_err_printf(L"insufficient memory to launch process.\n");
                    gle = ERROR_NOT_ENOUGH_MEMORY;

                    FillErrorMsg(pld, appExe, gle, pLaunchRes);
                    KillFailedLaunchBlock(prevProc);
                    goto fn_exit;
                }
            }

            pAff = smpd_next_HPCPack_affinity(pContext, pHPCAffinity, cHPCGroupNum, i);
        }
        else
        {
            pAff = smpd_next_proc_affinity(pContext);
            if (pAff != nullptr && pAff->Mask != 0 && pHPCAffinity != nullptr)
            {
                pAff->Mask = pHPCAffinity[pAff->GroupId].Mask & pAff->Mask;
            }
        }

        gle = gle_create_mpi_process(
            process,
            &pldHelper,
            pAff,
            pApp,
            hPipe[siInp],
            hPipe[siOut],
            hPipe[siErr],
            useShortPath,
            pLaunchRes
            );
        if( gle != NO_ERROR )
        {
            smpd_err_printf(L"Launching rank %hu %s>%s %s failed, error %u\n",
                            pld->rankArray[i], pld->wdir, pld->appExe, pld->appArgs, gle);
            smpd_close_std_pipes( hPipe );
            smpd_free_process_struct( process );
            KillFailedLaunchBlock( prevProc );

            break;
        }

        //
        // Save the new process to the process list
        //
        smpd_add_process( process );
        smpd_close_std_pipes( hPipe );
    }
    if (pHPCAffinity != nullptr)
    {
        MPIU_Free(pHPCAffinity);
    }

fn_exit:
    smpd_remove_block_pmi_environment();
    RemoveEnvironmentVariables( pld->env, pld->envCount );

    SetCurrentDirectoryW(tmpdir);

    return gle;
}


static int
smpd_signal_process_close(
    const smpd_context_t* pContext
    )
{
    ASSERT(pContext->type != SMPD_CONTEXT_APP_STDIN);

    smpd_process_t* proc = smpd_find_process_by_id(pContext->process_id);
    if(proc == nullptr)
    {
        return MPI_SUCCESS;
    }

    ULONG refCnt = InterlockedDecrement(&proc->context_refcount);
    smpd_dbg_printf(L"process_id=%hu process refcount == %u, %s closed.\n",
                    proc->id,
                    refCnt,
                    smpd_get_context_str(pContext));

    if(  refCnt > 0 )
    {
        return MPI_SUCCESS;
    }

    return smpd_rundown_process(proc);
}


void
smpd_handle_process_close(
    EXOVERLAPPED* pexov
    )
{
    smpd_overlapped_t* pov = smpd_ov_from_exov(pexov);
    smpd_context_t* pContext = pov->pContext;

    int rc = smpd_signal_process_close(pContext);

    smpd_free_context(pContext);
    smpd_free_overlapped(pov);
    if(rc != MPI_SUCCESS)
    {
        //
        // Failed to send the exit command to mpiexec.
        // Missing exit command will hang the tree, thus exit this manager
        //
        smpd_signal_exit_progress(rc);
        return;
    }
}


void smpd_kill_process(smpd_process_t *process, int exit_code)
{
    HANDLE tempDumpFile = INVALID_HANDLE_VALUE;
    if( process->dump_type != -1 && exit_code != -arFatalError )
    {
        //
        // If FlushDump is successful, it will return a handle to a
        // temporary dump file.
        //
        tempDumpFile = CreateTempDumpFile(
            process->wait.hProcess,
            process->pid,
            process->dump_type,
            process->dump_path,
            NULL
            );
    }

    TerminateProcess( process->wait.hProcess, exit_code );
    WaitForSingleObject( process->wait.hProcess, INFINITE );

    if( tempDumpFile != INVALID_HANDLE_VALUE )
    {
        CreateFinalDumpFile(
            tempDumpFile,
            process->rank,
            process->dump_path,
            process->dump_job_id,
            process->dump_task_id,
            process->dump_taskinstance_id
            );
        CloseHandle( tempDumpFile );
    }

    smpd_rundown_process( process );
}


void smpd_kill_all_processes(void)
{
    smpd_process_t* proc = smpd_get_first_process();
    while( proc != nullptr )
    {
        //
        // smpd_kill_process will remove the process from the list
        // and destroy the process structure. We obtain the next process
        // in advance
        //
        smpd_process_t* nextProc = proc->next;
        smpd_kill_process(proc, -1);
        proc = nextProc;
    }
}
