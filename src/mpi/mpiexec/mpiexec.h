// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef MPIEXEC_H
#define MPIEXEC_H

#include "smpd.h"
#include "PmiDbg.h"


SmpdCmdHandler mpiexec_handle_invalid_command;
SmpdCmdHandler mpiexec_handle_abort_command;
SmpdCmdHandler mpiexec_handle_abort_job_command;
SmpdCmdHandler mpiexec_handle_exit_command;
SmpdCmdHandler mpiexec_handle_finalize_command;
SmpdCmdHandler mpiexec_handle_init_command;
SmpdCmdHandler mpiexec_handle_stderr_command;
SmpdCmdHandler mpiexec_handle_stdout_command;
SmpdCmdHandler mpiexec_handle_spawn_command;


SmpdResHandler mpiexec_handle_collect_result;
SmpdResHandler mpiexec_handle_kill_result;
SmpdResHandler mpiexec_handle_launch_result;
SmpdResHandler mpiexec_handle_start_dbs_result;
SmpdResHandler mpiexec_handle_suspend_result;


DWORD
mpiexec_send_connect_command(
    _In_ smpd_context_t*    context,
    _In_ const smpd_host_t* host
    );


DWORD
mpiexec_send_collect_commands();


DWORD
mpiexec_send_start_dbs_command(
    smpd_context_t* pContext
    );


DWORD
mpiexec_handle_node_connection(
    _In_  smpd_context_t* context,
    _Out_ smpd_host_t*    host
    );


DWORD
mpiexec_redirect_stdin();


smpd_process_group_t*
find_pg(
    const GUID& kvs
    );


DWORD
mpiexec_abort_job(
    smpd_process_group_t* pg,
    UINT16                rank,
    abort_reason_t        abort_reason,
    int                   exit_code,
    const wchar_t*        err_msg
    );


void mpiexec_tear_down_tree();


struct mp_host_t
    : public HWMACHINEINFO
{
    const wchar_t*  name;
    int             nproc;
    HWAFFINITY*     explicit_affinity;
    UINT32          explicit_count;
    UINT32          explicit_index;

public:
    mp_host_t(const wchar_t* pName = NULL, int nProc = 0)
        : name(pName)
        , nproc(nProc)
        , explicit_affinity( NULL )
        , explicit_count(0)
        , explicit_index(0)
    {
        Next = NULL;
        HostId = 0;
        Summary = NULL;
    }
};

extern mp_host_t g_mp_host;

struct mp_host_pool_t
{
    mp_host_t*      hosts;
    int             nproc;

public:
    mp_host_pool_t()
        : hosts(NULL)
        , nproc(0)
    {
    }

};


struct smpd_launch_node_t
{
    smpd_launch_node_t* next;
    smpd_launch_node_t* prev;
    int iproc;
    int host_id;
    wchar_t hostname[SMPD_MAX_HOST_LENGTH];
    HWAFFINITY* affinity;
};


struct smpd_env_node_t
{
    smpd_env_node_t* next;
    wchar_t*         name;
    wchar_t*         value;

    ~smpd_env_node_t()
    {
        MPIU_Free(name);
        MPIU_Free(value);
    }
};


struct mp_block_options_t
{
    mp_block_options_t*     next;
    smpd_env_node_t*        env;
    wchar_t**               envBlock;
    UINT16                  envCount;
    const wchar_t*          path;
    const wchar_t*          wdir;
    const wchar_t*          exe;
    mp_host_pool_t          pool;
    int                     priority;
    int                     nproc;
    int                     appnum;
    wchar_t                 cmdline[SMPD_MAX_EXE_LENGTH];

public:
    mp_block_options_t()
        : next(nullptr)
        , env(nullptr)
        , envBlock(nullptr)
        , envCount(0)
        , path(nullptr)
        , wdir(nullptr)
        , exe(nullptr)
        , priority(SMPD_PRIORITY_CLASS_DEFAULT)
        , nproc(0)
        , appnum(0)
    {
        cmdline[0] = '\0';
    }

    ~mp_block_options_t()
    {
        delete next;
        while( nullptr != pool.hosts )
        {
            mp_host_t* p = static_cast<mp_host_t*>( pool.hosts->Next );

            //
            // skip the global "local machine" sentinel value
            //
            if( &g_mp_host != pool.hosts )
            {
                delete pool.hosts;
            }
            pool.hosts = p;
        }

        while( env != nullptr )
        {
            smpd_env_node_t* p = env->next;
            delete env;
            env = p;
        }
        delete [] envBlock;
    }
};


struct mp_global_options_t
{
    smpd_env_node_t*    env;
    const wchar_t*      path;
    wchar_t*            wdir;
    wchar_t*            pwd;
    BOOL                saveCreds;
    mp_host_pool_t      pool;
    int                 cores_per_host;
    AffinityOptions     affinityOptions;
    mp_block_options_t* bo_list;

public:
    mp_global_options_t()
        : env(nullptr)
        , path(nullptr)
        , wdir(nullptr)
        , pwd(nullptr)
        , saveCreds(FALSE)
        , pool()
        , cores_per_host(0)
        , bo_list(nullptr)
        {
            affinityOptions.placement = SMPD_AFFINITY_DISABLED;
            affinityOptions.target = HWNODE_TYPE_MACHINE;
            affinityOptions.isAuto = FALSE;
            affinityOptions.affinityTableStyle = 0;
            affinityOptions.hwTableStyle = 0;
        }

    ~mp_global_options_t()
    {
        while( nullptr != pool.hosts )
        {
            mp_host_t* p = static_cast<mp_host_t*>( pool.hosts->Next );

            //
            // skip the global "local machine" sentinel value
            //
            if( &g_mp_host != pool.hosts )
            {
                delete pool.hosts;
            }
            pool.hosts = p;
        }

        while( env != nullptr )
        {
            smpd_env_node_t* p = env->next;
            delete env;
            env = p;
        }

        if( wdir != nullptr )
        {
            MPIU_Free( wdir );
        }

        if( bo_list != nullptr )
        {
            delete bo_list;
        }
    }
};


smpd_host_t* 
smpd_get_host_id(
    _In_ PCWSTR host
    );


//
// These externs are required to be in mpiexec to enable debugging
// support
// MPIR_Proctable_size: World's size at startup
// MPIR_Proctable: Information about MPΙ processes (hostname, rank, pid)
// MPIR_debug_state
//    Values are 0 (before MPI_Init), 1 (after MPI_init), and 2 (Aborting).
// MPIR_being_debugged
//    Set to 1 if the process is started or attached under the debugger
//
extern "C"
{
extern __declspec(dllexport) MPIR_PROCDESC* MPIR_Proctable;
extern __declspec(dllexport) int MPIR_Proctable_size;
extern __declspec(dllexport) volatile int MPIR_debug_state;
extern __declspec(dllexport) volatile int MPIR_being_debugged;
extern __declspec(dllexport) char MPIR_dll_name[];
}

typedef _Success_(return == true) bool (*pfn_on_option_t)(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t* bo);


typedef struct mp_option_handler_t
{
    const wchar_t* option;
    pfn_on_option_t on_option;

} mp_option_handler_t;


_Success_( return == true )
bool
mp_parse_command_args(
    _In_ wchar_t* argv[]
    );


void
mp_print_options(void);


void
mpiexec_print_hwtree(
    _In_ const smpd_host_t*     pHosts
    );


void
mpiexec_print_affinity_table(
    __in const smpd_launch_node_t*          pList,
    __in const smpd_host_t*                 pHosts
    );


bool
MpiexecGetUserInfoInteractive();


const wchar_t*
mp_parse_hosts_string(
    _Inout_z_ wchar_t* p,
    mp_host_pool_t* pool
    );


_Success_(return == NULL)
_Ret_maybenull_
const wchar_t*
mp_parse_hosts_argv(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_host_pool_t* pool
    );


_Success_(return != NULL)
smpd_env_node_t*
add_new_env_node(
    _Inout_ smpd_env_node_t** list,
    _In_ PCWSTR name,
    _In_ PCWSTR value
    );

#endif // MPIEXEC_H
