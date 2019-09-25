// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"


HMODULE g_hModule = NULL;


void WINAPI
GetCtxLaunchInfo(
    _In_     const void*  launchCtx,
    _Outptr_ const char** ppJobName,
    _Outptr_ const char** ppPwd,
    _Out_    BOOL*        pSaveCreds
    )
{
    const smpd_context_t* pCtx = reinterpret_cast<const smpd_context_t*>(launchCtx);

    if (ppJobName != nullptr)
    {
        *ppJobName = pCtx->jobObjName;
    }

    if (ppPwd != nullptr)
    {
        *ppPwd = pCtx->pPwd;
    }

    if (pSaveCreds != nullptr)
    {
        *pSaveCreds = pCtx->saveCreds;
    }
}


smpd_global_t smpd_process =
{
    -1,               /* tree_level             */
    -1,               /* tree_id                */
    -1,               /* parent_id              */
    -1,               /* left_child_id          */
    -1,               /* right_child_id         */
    NULL,             /* left_context           */
    NULL,             /* right_context          */
    NULL,             /* parent_context         */
    NULL,             /* appname                */
    L"",              /* jobObjName             */
    L"",              /* pwd                    */
    SMPD_AUTH_DEFAULT,/* authOptions            */
    FALSE,            /* saveCreds              */
    false,            /* parent_closed          */
    false,            /* closing                */
    EX_INVALID_SET,   /* set                    */
    L"",              /* host                   */
    L"",              /* szExe                  */
    0,                /* dbg_state              */
    false,            /* have_dbs               */
    GUID_NULL,        /* domain                 */
    SMPD_LISTENER_PORT, /* root server port     */
    0,                /* smpd manager port      */
    GUID_NULL,        /* pmi server port        */
    NULL,             /* host_list              */
    NULL,             /* launch_list            */
    false,            /* output_exit_codes      */
    false,            /* local_root             */
    0,                /* nproc                  */
    NULL,             /* num_children_list      */
    0,                /* nproc_exited           */
    false,            /* verbose                */
    {                 /* affinityOptions        */
        false,                  /* isSet */
        false,                  /* isExplicit */
        false,                  /* isAuto */
        SMPD_AFFINITY_DISABLED, /* placement */
        HWNODE_TYPE_LCORE,      /* target */
        HWNODE_TYPE_LCORE,      /* stride */
        0,                      /* affinityTableStyle */
        0                       /* hwTableStyle */
    },
    NULL,             /* barrier_list           */
    NULL,             /* barrier_forward_list   */
    false,            /* stdin_redirecting      */
    0,                /* mpiexec_parent_id      */
    1,                /* mpiexec_tree_id        */
    0,                /* connectRetryCount      */
    0,                /* connectRetryInterval   */
    NULL,             /* pg_list                */
    0,                /* mpiexec_exit_code      */
    -1,               /* timeout                */
    false,            /* prefix_output          */
    false,            /* unicodeOutput */
    NULL,             /* job_context  */
    NULL,             /* hJobObj - smpd instance's job object */
    NULL,             /* hHeartbeatTimer */
    L"",              /* node id shm region name */
    NULL,             /* handle to node id region */
    L"",              /* rank affinities shm region name */
    NULL,             /* handle to rank affinities shm region*/
    NULL,             /* pointer to rank affinities buffer */
    NULL,             /* pointer to launch manager function */
    {                 /* initialization of service launch interface */
        nullptr,      /* CreateLaunchCtx */
        nullptr,      /* StartLaunchCtx */
        nullptr,      /* EndLaunchCtx */
        nullptr,      /* CleaunupLaunchCtx */
        GetCtxLaunchInfo, /* GetLaunchInfo */
    },
    NULL,             /* pointer to aborting function */
    NULL,             /* pointer to array of business cards */
    NULL,             /* pointer to array of command handlers */
    {0},              /* Smpd Service's critical section */
    NULL,             /* pointer to mpiexec's global block options */
    NULL,             /* pointer to processor configuration information */
    0,                /* length of the processor information data structure */
};



static wchar_t* smpd_find_last_backslash(_Inout_z_ wchar_t* path)
{
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if(lastSlash == NULL)
    {
        path[0] = L'\0';
        return NULL;
    }
    lastSlash++;
    return lastSlash;
}


static void
smpd_get_smpd_exec_path(
    _Out_writes_z_(size) wchar_t* path,
    _In_ size_t size
    )
{
    HRESULT hr;
    HKEY regKey;
    DWORD ntError;
    wchar_t* end;
    size_t endSize;

    assert(size > 0);

    path[0] = L'\0';

    //
    // To enable private build scenarios, we check the working location
    // of the MSMPI.dll first.  After that, we check the registry for the
    // installation location of smpd.exe, if that fails, we look for it
    // in the same location as the executing process.
    //

    //
    // For the assemblies such as smpd.exe and mpiexec.exe that still link
    //  this lib internally, the g_hModule value will be null, so it will
    //  grab the path of the current executing process, just as expected.
    //
    DWORD len = GetModuleFileNameW(g_hModule, path, (DWORD)size);
    if(len == size)
    {
        if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            path[0] = L'\0';
            return;
        }
    }

    if (len > 0 )
    {
        end = smpd_find_last_backslash(path);
        if (NULL != end)
        {
            endSize = size - (end - path);
            MPIU_Strcpy(end, endSize, L"smpd.exe");

            if (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(path))
            {
                return;
            }
        }
    }

    //we didnt't find it in the location of MSMPI.dll, now we try the registry
    //reset the path to empty
    path[0] = L'\0';

    ntError = RegOpenKeyExW(
                    HKEY_LOCAL_MACHINE,
                    L"Software\\Microsoft\\MPI",
                    0,
                    KEY_READ,
                    &regKey);
    if (ERROR_SUCCESS == ntError)
    {
        len = static_cast<DWORD>(size);

        ntError = RegGetValueW(
                        regKey,
                        NULL,
                        L"InstallRoot",
                        RRF_RT_REG_SZ,
                        NULL,
                        reinterpret_cast<void*>(path),
                        &len);

        RegCloseKey(regKey);

        if (ERROR_SUCCESS == ntError)
        {
            hr = StringCchCatW(path,
                               size,
                               L"\\bin\\smpd.exe");
            if (SUCCEEDED(hr))
            {
                if (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(path))
                {
                    //we found SMPD based on the registry key.
                    return;
                }
            }
        }
    }

    //
    // Finally we fall back to the old code and allow the previous behavior
    //  to execute.
    //

    len = GetModuleFileNameW(NULL, path, (DWORD)size);
    if(len == size)
    {
        if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            path[0] = L'\0';
            return;
        }
    }
    if(len == 0)
    {
        return;
    }

    end = smpd_find_last_backslash(path);
    if (NULL == end)
    {
        return;
    }

    endSize = size - (end - path);
    MPIU_Strcpy(end, endSize, L"smpd.exe");
}


void
smpd_init_process(
    _In_ PCSTR appname,
    _In_ bool localonly
    )
{
    /* tree data */
    smpd_process.tree_level = -1;
    smpd_process.tree_id = -1;
    smpd_process.parent_id = -1;
    smpd_process.left_child_id = -1;
    smpd_process.right_child_id = -1;
    smpd_process.left_context = NULL;
    smpd_process.right_context = NULL;
    smpd_process.parent_context = NULL;
    smpd_process.set = EX_INVALID_SET;
    smpd_process.appname = appname;

    smpd_get_smpd_exec_path(smpd_process.szExe, _countof(smpd_process.szExe));

    DWORD size = _countof(smpd_process.host);
    GetComputerNameW( smpd_process.host, &size );

    if( localonly == true )
    {
        smpd_process.local_root = TRUE;
    }
    else
    {
        smpd_clear_local_root();
    }
    smpd_process.closing = false;

    srand(smpd_getpid());
}


void smpd_clear_local_root(void)
{
    smpd_process.local_root = false;
}
