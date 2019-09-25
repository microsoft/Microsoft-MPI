// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef SMPD_H
#define SMPD_H

#include <winsock2.h>
#include <sys/types.h>
#include <io.h>
#include <rpc.h>
#include <userenv.h>
#define SECURITY_WIN32
#include <security.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <oacr.h>
#include <strsafe.h>
#include "mpi.h"
#include "pmi.h"
#include "mpidef.h"
#include "mpiiov.h"
#include "ex.h"
#include "mpidump.h"
#include "mpiutil.h"
#include "mpimem.h"
#include "mpistr.h"
#include "autoptr.h"
#include "smpd_database.h"
#include "smpd_queue.h"
#include "rpc.h"
#include "SmpdRpc.h"
#include "hwtree.h"
#include "kernel32util.h"
#include "mspms.h"
#include "ipcShm.h"
#include "authz.h"
#include "util.h"

//
// Process Management Protocol version.
//
#define SMPD_PMP_VERSION         4U


//
// Tokenizing macros
//
#define _S1(x) L#x
#define _S(x) _S1(x)

#define SMPD_SERVICE_NAME         TEXT("msmpi")
#define SMPD_LISTENER_PORT               8677

#define SMPD_EXIT_FROM_RPC               -3
#define SMPD_EXIT_FROM_TIMEOUT           -2

#define SMPD_MAX_NAME_LENGTH              256
#define SMPD_MAX_FILENAME                1024
#define SMPD_MAX_ERROR_LEN               4096

//
// Value selected to support up to 32768 ranks
//
#define SMPD_MAX_CMD_LENGTH          260*1024

/* Max non encoded stdin/out size; must be less than 1/2 of the cmd
 * buffer size. Buffer enocding alg uses two chars for one.
 */
#define SMPD_ENCODING_FACTOR                2
#define SMPD_MAX_STDIO_LENGTH            4096
C_ASSERT(SMPD_MAX_STDIO_LENGTH <= SMPD_MAX_CMD_LENGTH/SMPD_ENCODING_FACTOR);

#define SMPD_MAX_HOST_LENGTH               64
#define SMPD_MAX_EXE_LENGTH                 UNICODE_STRING_MAX_CHARS
#define SMPD_MAX_ENV_LENGTH              4096
#define SMPD_MAX_PORT_STR_LENGTH           (GUID_STRING_LENGTH + 1)

#define SMPD_FAIL_STR                       TEXT("FAIL")

#define SMPD_DBG_STATE_ERROUT            0x01
#define SMPD_DBG_STATE_STDOUT            0x02
#define SMPD_DBG_STATE_PREPEND_RANK      0x08
#define SMPD_DBG_STATE_ALL               (SMPD_DBG_STATE_ERROUT | SMPD_DBG_STATE_STDOUT | SMPD_DBG_STATE_PREPEND_RANK)

#define LOCAL_HOST_ADDR                     L"localhost"
#define SMPD_PMI_HOST                       LOCAL_HOST_ADDR


#define SMPD_PRIORITY_CLASS_MIN             0
#define SMPD_PRIORITY_CLASS_MAX             4
#define SMPD_PRIORITY_CLASS_DEFAULT         2

typedef enum _SMPD_IDENTITY
{
    SMPD_IS_ROOT = 0,
    SMPD_IS_TOP = 1,
} SMPD_IDENTITY;


typedef enum smpd_context_type_t
{
    SMPD_CONTEXT_APP_STDIN = 1, /* mgr: process stdin redirection */
    SMPD_CONTEXT_APP_STDOUT,    /* mgr: process stdout redirection */
    SMPD_CONTEXT_APP_STDERR,    /* mgr: process stderr redirection */
    SMPD_CONTEXT_LEFT_CHILD,    /* mgr/mpiexec: connection to left child */
    SMPD_CONTEXT_RIGHT_CHILD,   /* mgr: connection to right child */
    SMPD_CONTEXT_ROOT,          /* root: job spawning connection */
    SMPD_CONTEXT_MGR_PARENT,    /* mgr: connection to parent smpd */
    SMPD_CONTEXT_PMI_CLIENT,    /* app: pmi connection (client) */

} smpd_context_type_t;


typedef enum _SmpdAuthOptions
{
    SMPD_AUTH_DEFAULT = 0,
    SMPD_AUTH_DISABLE_KERB,
    SMPD_AUTH_NTLM,
    SMPD_AUTH_INVALID
} SmpdAuthOptions;


typedef struct _SmpdMgrData
{
    BOOL            isMpiProcess;
    SmpdAuthOptions authOptions;
    INT16           id;
    INT16           parent;
    INT16           level;
    UINT16          parentTcpPort;
    GUID            parentLrpcPort;
    UINT16          worldSize;
}SmpdMgrData;


struct smpd_overlapped_t;

//
// Defining the prototype of a handler for SMPD command
//
typedef DWORD (SmpdCmdHandler)( _In_ smpd_overlapped_t* pov );

SmpdCmdHandler smpd_handle_invalid_command;
SmpdCmdHandler smpd_handle_barrier_command;
SmpdCmdHandler smpd_handle_closed_command;
SmpdCmdHandler smpd_handle_collect_command;
SmpdCmdHandler smpd_handle_connect_command;
SmpdCmdHandler smpd_handle_dbget_command;
SmpdCmdHandler smpd_handle_dbput_command;
SmpdCmdHandler smpd_handle_bcget_command;
SmpdCmdHandler smpd_handle_bcput_command;
SmpdCmdHandler smpd_handle_kill_command;
SmpdCmdHandler smpd_handle_launch_command;
SmpdCmdHandler smpd_handle_start_dbs_command;
SmpdCmdHandler smpd_handle_add_dbs_command;
SmpdCmdHandler smpd_handle_stdin_command;
SmpdCmdHandler smpd_handle_stdin_close_command;
SmpdCmdHandler smpd_handle_suspend_command;
SmpdCmdHandler smpd_handle_ping_command;

//
// Forward Declaration.
//

struct smpd_context_t;
struct smpd_command_handler_t;
struct smpd_process_t;
struct smpd_launch_node_t;
struct smpd_launch_block_t;
struct smpd_rank_data_t;
struct smpd_barrier_in_t;
struct smpd_barrier_node_t;


//
// Defining the prototype of a handler for SMPD Result
//
typedef void (SmpdResHandler)( _In_ smpd_overlapped_t* pov );


//
// We need a wrapper type because MIDL does not support
// function pointer
//
typedef struct _SmpdResWrapper
{
    SmpdRes         Res;
    SmpdResHandler* OnResultFn;
} SmpdResWrapper;


struct smpd_pwait_t
{
    HANDLE hProcess, hThread;

};


struct smpd_rank_data_t
{
    smpd_rank_data_t* next;
    smpd_launch_block_t* parent;
    UINT16 rank;
    UINT16 nodeProcOrder;
    HWAFFINITY affinity;
    ~smpd_rank_data_t()
    {
        delete next;
    }
};


struct smpd_node_id_node_t
{
    smpd_node_id_node_t* next;
    GUID                 kvs;
    UINT16               nproc;
    UINT16*              node_id_array;

    ~smpd_node_id_node_t()
    {
        delete[] node_id_array;
    }
};


struct smpd_launch_block_t
{
    smpd_launch_block_t* next;
    smpd_launch_block_t* prev;
    GUID                 kvs;
    GUID                 domain;
    const wchar_t*       pExe;
    wchar_t*             pArgs;
    wchar_t**            ppEnv;
    const wchar_t*       pDir;
    const wchar_t*       pPath;
    int                  host_id;
    wchar_t              hostname[SMPD_MAX_HOST_LENGTH];
    int                  appnum;
    int                  priority_class;
    UINT16               blockIndex;
    UINT16               blockRankCount;
    UINT16               envCount;
    HWAFFINITY           affinity;
    char*                parentPortName;
    bool                 launched;
    smpd_rank_data_t*    pRankData;

    smpd_launch_block_t():
        next(nullptr),
        prev(nullptr),
        pExe(nullptr),
        pArgs(nullptr),
        ppEnv(nullptr),
        pDir(nullptr),
        pPath(nullptr),
        parentPortName(nullptr),
        launched(false),
        pRankData(nullptr)
    {}

    ~smpd_launch_block_t()
    {
        delete pRankData;
        delete next;
    }
};


struct smpd_host_t : public HWMACHINEINFO
{
    struct smpd_host_t* left;
    struct smpd_host_t* right;
    int parent;
    int nproc;
    BOOL connected;
    BOOL collected;
    wchar_t name[SMPD_MAX_HOST_LENGTH];
    char*   nameA;
    UINT16 nodeProcCount;
    HWTREE* hwTree;
    smpd_launch_block_t* pLaunchBlockList;
    void* pBlockOptions;

    ~smpd_host_t()
    {
        delete[] nameA;
        free(Summary);
        free(hwTree);
        delete pLaunchBlockList;
        delete static_cast<smpd_host_t*>(Next);
    }
};


typedef void (*pfn_on_cmd_error_t)(smpd_overlapped_t* pov, int error);

struct smpd_context_t
{
    smpd_context_type_t type;
    ExSetHandle_t set;
    pfn_on_cmd_error_t on_cmd_error;
    wchar_t name[SMPD_MAX_HOST_LENGTH];
    wchar_t port_str[SMPD_MAX_PORT_STR_LENGTH];
    char* job_context;
    char stdioBuffer[SMPD_MAX_STDIO_LENGTH];
    UINT16 process_id;
    int process_rank;
    GUID kvs;
    HANDLE pipe;
    union
    {
        RPC_BINDING_HANDLE         hClientBinding;
        PVOID                      hServerContext;
    };
    UINT16 processCount;
    HWAFFINITY* affinityList;
    GlobalShmRegion* pAutoAffinityRegion;
    char* jobObjName;
    char* pPwd;
    BOOL  saveCreds;
};


struct smpd_stdin_write_node_t
{
    struct smpd_stdin_write_node_t* next;
    UINT32 length;
    char buffer[];

};


struct smpd_process_t
{
    smpd_process_t* next;
    smpd_context_t* in;
    smpd_queue<smpd_stdin_write_node_t> stdin_write_queue;
    smpd_pwait_t wait;
    UINT16 id;
    volatile ULONG isRundown;
    volatile ULONG context_refcount;
    int pid;
    UINT16 rank;
    HWAFFINITY launch_affinity;
    int exitcode;
    MINIDUMP_TYPE dump_type;
    wchar_t dump_path[MAX_PATH];
    int dump_job_id;
    int dump_task_id;
    int dump_taskinstance_id;
    GUID kvs;
    HANDLE hStdout;
    HANDLE hStderr;
    HANDLE hStdin;
    EXOVERLAPPED rundown_overlapped;
};


struct smpd_barrier_in_t
{
    smpd_overlapped_t* pov;
    INT16 dest;
    UINT16 ctx_key;

};


struct smpd_barrier_node_t
{
    smpd_barrier_node_t* next;
    GUID kvs;
    UINT16 numExpected;
    UINT16 numReached;
    smpd_barrier_in_t *in_array;
};


struct smpd_barrier_forward_t
{
    smpd_barrier_forward_t* next;
    GUID kvs;
    UINT16 left;
    UINT16 right;

    smpd_barrier_forward_t(GUID in_kvs) : 
        left(0), 
        right(0),
        next(nullptr)
    {
        kvs = in_kvs;
    }
};


enum abort_reason_t
{
    arTerminated,
    arCtrlC,
    arExitNoInit,
    arExitNoFinalize,
    arFatalError,
    arAppAbort,
};


struct smpd_exit_process_t
{
    BOOL launched;
    BOOL init_called;
    BOOL finalize_called;
    BOOL suspend_pending;
    BOOL suspended;
    BOOL exited;
    BOOL failed_launch;
    UINT16 node_id;
    int exitcode;
    UINT16 ctx_key;
    abort_reason_t abortreason;
    const wchar_t* errmsg;
    wchar_t host[SMPD_MAX_HOST_LENGTH];

};


struct smpd_process_group_t
{
    smpd_process_group_t* next;
    BOOL aborted;
    BOOL any_noinit_process_exited;
    BOOL any_init_received;
    UINT16 num_procs;
    int num_exited;
    int num_pending_suspends;
    int num_pending_kills;
    int num_results;
    GUID kvs;
    smpd_exit_process_t *processes;

};


struct smpd_process_biz_cards_t
{
    GUID kvs;
    UINT16 nproc;
    char** ppBizCards;
    smpd_process_biz_cards_t* pNext;
};


struct smpd_numchildren_t
{
    smpd_numchildren_t* next;
    GUID kvs;
    UINT16 numChildren;
};


/* If you add elements to the process structure you must add the appropriate
   initializer in smpd_init.cpp where the global variable, smpd_process, lives */
struct smpd_global_t
{
    INT16                    tree_level;
    INT16                    tree_id;
    INT16                    parent_id;
    INT16                    left_child_id;
    INT16                    right_child_id;
    smpd_context_t*          left_context;
    smpd_context_t*          right_context;
    smpd_context_t*          parent_context;
    const char*              appname;
    wchar_t                  jobObjName[MAX_PATH];
    wchar_t*                 pwd;
    SmpdAuthOptions          authOptions;
    BOOL                     saveCreds;
    bool                     parent_closed;
    bool                     closing;
    ExSetHandle_t            set;
    wchar_t                  host[SMPD_MAX_HOST_LENGTH];
    wchar_t                  szExe[SMPD_MAX_EXE_LENGTH];
    int                      dbg_state;
    int                      have_dbs;
    GUID                     domain;
    UINT16                   rootServerPort;
    UINT16                   mgrServerPort;
    GUID                     localServerPort;
    smpd_host_t             *host_list;
    smpd_launch_node_t      *launch_list;
    int                      output_exit_codes;
    bool                     local_root;
    UINT16                   nproc;
    smpd_numchildren_t      *num_children_list;
    int                      nproc_exited;
    bool                     verbose;
    AffinityOptions          affinityOptions;
    smpd_barrier_node_t     *barrier_list;
    smpd_barrier_forward_t  *barrier_forward_list;
    bool                     stdin_redirecting;
    int                      mpiexec_parent_id;
    int                      mpiexec_tree_id;
    UINT32                   connectRetryCount;
    UINT32                   connectRetryInterval;
    smpd_process_group_t    *pg_list;
    int                      mpiexec_exit_code;
    int                      timeout;
    bool                     prefix_output;
    bool                     unicodeOutput;
    char*                    job_context;
    HANDLE                   hJobObj;
    HANDLE                   hHeartbeatTimer;
    //
    // name of shared memory section that holds the node ids for all
    // ranks (GLOBAL)
    //
    wchar_t                  node_id_region_name[SMPD_MAX_FILENAME];

    union
    {
        //
        // smpd instance will use this to store the mapped region
        // for node_id
        //
        HANDLE                   node_id_region;

        //
        // mpiexec will use this to store its computed node_id_arrays
        // for each process group separately
        //
        smpd_node_id_node_t*     node_id_list;
    };

    //
    // name of shared memory section that holds the launch affinities
    // for all ranks (NODE LOCAL - remote ranks will be marked with 0
    // as they aren't needed for node local optimizations)
    //
    wchar_t                  rank_affinity_region_name[SMPD_MAX_FILENAME];

    HANDLE                   rank_affinity_region;

    //
    // local pointer for the shared memory section
    //
    HWAFFINITY*              rank_affinities;

    //
    // pointer to the MSPMS interface
    //
    const PmiManagerInterface* manager_interface;

    //
    // Service launch info interface
    //
    PmiServiceLaunchInterface svcIfLaunch;

    //
    // pointer to RPC aborting function
    //
    void (*abortExFunc)();

    //
    // array for caching business cards
    //
    smpd_process_biz_cards_t*  pBizCardsList;

    //
    // array of command handler
    //
    SmpdCmdHandler*          cmdHandler[SMPD_CMD_MAX];

    //
    // Critical section used for the service during launching smpd manager
    // and for the manager to deal with business card caching
    //
    CRITICAL_SECTION         svcCriticalSection;

    //
    // Store the constructed global block options
    //
    void* pGlobalBlockOpt;

    //
    // Store information about the processor configuration of the node smpd is running on
    //
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* pNumaNodeInfo;
    DWORD cNumaNodeInfoLen;
};


//
// This structure stores a pending RPC Async request.
// The RPC Async request in this structure has a dependency on "count" other
// RPC requests. Those RPC requests should maintain a reference to this structure
// and decrement the count whenver they finish. When the count goes to 0
// "pAsync" can be completed and the memory allocated to this struct should be freed.
//
class SmpdPendingRPC_t
{
private:
    ULONG            m_refcount;
    RPC_ASYNC_STATE* m_pAsync;
    SmpdRes*         m_pRes;
    volatile DWORD   m_err;

public:
    SmpdPendingRPC_t( RPC_ASYNC_STATE& p, SmpdRes* pRes )
        : m_pAsync(&p), m_pRes(pRes), m_refcount(1), m_err(NOERROR)
    {}


    ULONG AddRef()
    {
        return InterlockedIncrement( &m_refcount );
    }


    void UpdateStatus( DWORD err )
    {
        InterlockedCompareExchange( &m_err, err, NOERROR );
    }


    void Release()
    {
        if( InterlockedDecrement( &m_refcount ) == 0 )
        {
            delete this;
        }
    }

private:
    ~SmpdPendingRPC_t()
    {
        SmpdResHdr* pHeader = reinterpret_cast<SmpdResHdr*>( m_pRes );
        pHeader->err = m_err;
        //
        // Ignore the return status of this call because there's no actionable
        // error path.
        //
        OACR_WARNING_DISABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
            "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no actionable recovery.");
        RpcAsyncCompleteCall( m_pAsync, nullptr );
        OACR_WARNING_ENABLE(RETVAL_IGNORED_FUNC_COULD_FAIL,
            "Intentionally ignore the return value of RpcAsyncCompleteCall since there is no actionable recovery.");
    }
};


#define JOB_CONTEXT_UNASSIGNED_STR  L"job"

extern smpd_global_t smpd_process;


#define smpd_szncpy(dst, src) MPIU_Szncpy(dst, src)
#define smpd_strncpy(dst, n, src) MPIU_Strncpy(dst, src, n)

#define smpd_szncat(dst, src) MPIU_Sznapp(dst, src)


static inline const wchar_t* skip_ws(const wchar_t* str)
{
    while( iswspace(*str) )
    {
        ++str;
    }

    return str;
}


static inline const wchar_t* skip_graph(const wchar_t* str)
{
    while( iswgraph(*str) )
    {
        ++str;
    }

    return str;
}


static inline const wchar_t* skip_digits(const wchar_t* p)
{
    while( iswdigit(*p) )
    {
        p++;
    }

    return p;
}


static inline BOOL ishex(wchar_t c)
{
    if( iswdigit( c )  ||
        ( c >= L'a' && c <= L'f' ) ||
        ( c >= L'A' && c <= L'F' ) )
    {
        return TRUE;
    }

    return FALSE;
}


static inline const wchar_t* skip_hex(const wchar_t* p)
{
    //
    // skip 0x prefix
    //
    if (p[0] == L'0' && p[1] == L'x')
    {
        p+=2;
    }

    while(ishex(*p))
    {
        p++;
    }

    return p;
}


static inline BOOL isdigits(const wchar_t* str)
{
    do
    {
        if( !iswdigit(*str) )
            return FALSE;

    } while(*++str != L'\0');

    return TRUE;
}


static inline BOOL iszeros(const wchar_t* str)
{
    do
    {
        if(*str != L'0')
            return FALSE;

    } while(*++str != L'\0');

    return TRUE;
}


static inline BOOL isnumber(const wchar_t* str)
{
    if(*str == L'-')
    {
        ++str;
    }

    return isdigits(str);
}


static inline BOOL isposnumber(const wchar_t* str)
{
    return (isdigits(str) && !iszeros(str));
}


static inline int is_flag_char(const wchar_t* pc)
{
    return ((pc != NULL) && ((*pc == L'-') || (*pc == L'/')));
}


//
// An expression that yields the type of a field in a struct.
//
#define GET_FIELD_TYPE(type, field) (((type*)0)->field)

//
// Given typedef struct _FOO { BYTE Bar[123]; } FOO;
// COUNTOF_FIELD(FOO, Bar) == 123
//
#define COUNTOF_FIELD(type, field) _countof(GET_FIELD_TYPE(type, field))

/* function prototypes */
_Success_(return == true)
bool
smpd_parse_command_args(
    _In_     wchar_t* argv[],
    _Outptr_ HANDLE* phManagerWritePipe
    );

void smpd_print_options(void);
int smpd_progress(ExSetHandle_t set);
void smpd_signal_exit_progress(int rc);


void smpd_init_process(
    _In_ PCSTR appname,
    _In_ bool localonly = false
    );


void smpd_clear_local_root(void);


smpd_context_t*
smpd_create_context(
    _In_     smpd_context_type_t type,
    _In_     ExSetHandle_t       set,
    _In_opt_z_ PCWSTR            pJobObjName = nullptr,
    _In_opt_z_ PCWSTR            pPwd = nullptr,
    _In_     BOOL                saveCreds = FALSE
    );


smpd_context_type_t
smpd_destination_context_type(
    _In_ int dest
    );


void smpd_free_context(
    _In_ _Post_ptr_invalid_ smpd_context_t *context
    );


SmpdCmd*
smpd_create_command(
    _In_ SMPD_CMD_TYPE cmdType,
    _In_ INT16         src,
    _In_ INT16         dest
    );


void
smpd_init_result_command(
    _Out_ SmpdRes* pRes,
    _In_  INT16    dest
    );


SmpdResWrapper*
smpd_create_result_command(
    _In_     INT16           dest,
    _In_opt_ SmpdResHandler* resHandler
    );


void
smpd_free_command(
    _In_ _Post_ptr_invalid_ SmpdCmd* pCmd
    );


const wchar_t*
CmdTypeToString(
    _In_ SMPD_CMD_TYPE cmdType
    );


DWORD
smpd_post_command(
    _In_ smpd_context_t*       pContext,
    _In_ SmpdCmd*              pCmd,
    _In_ SmpdRes*              pRes,
    _In_opt_ SmpdPendingRPC_t* pPendingRPC,
    _In_opt_ char*             pPmiTmpBuffer = nullptr,
    _In_opt_ DWORD*            pPmiTmpErr = nullptr
    );


DWORD
SmpdPostCommandSync(
    _In_ smpd_context_t* pContext,
    _In_ SmpdCmd* pCmd,
    _In_ SmpdRes* pRes
    );


void
smpd_handle_result(
    _In_ EXOVERLAPPED* pexov
    );


void
smpd_handle_command(
    _In_ EXOVERLAPPED* pexov
    );


void
smpd_forward_command(
    _In_ EXOVERLAPPED* pexov
    );


void
smpd_handle_close_command(
    _In_ EXOVERLAPPED* pexov
    );


int
smpd_dbg_printf(
    _In_ _Printf_format_string_ PCWSTR str,
    ...
    );

int
smpd_err_printf(
    _In_ _Printf_format_string_ PCWSTR str,
    ...
    );


int smpd_getpid(void);
const char* get_sock_error_string(int error);


void
smpd_handle_process_close(
    EXOVERLAPPED* pexov
    );


DWORD
smpd_launch_processes(
    _In_ smpd_context_t* pContext,
    _In_ const SmpdLaunchCmd* pld,
    _Out_ SmpdLaunchRes* pLaunchRes,
    _In_ ExSetHandle_t set
    );


void
smpd_encode_buffer(
    _Out_writes_z_(dest_length) char *dest,
    _In_ size_t dest_length,
    _In_reads_(src_length) PCSTR src,
    _In_ UINT32 src_length,
    _Out_ UINT32* num_encoded
    );


void
smpd_decode_buffer(
    _In_ PCSTR str,
    _Out_writes_z_(dest_length) char *dest,
    _In_ UINT32 dest_length,
    _Out_ UINT32* num_decoded
    );


smpd_process_t*
smpd_create_process_struct(
    _In_ UINT16 rank,
    _In_ GUID kvs
    );


void
smpd_free_process_struct(
    _In_ _Post_ptr_invalid_ smpd_process_t *process
    );


void
smpd_add_process(
    _Inout_ smpd_process_t* process
    );


void
smpd_remove_process(
    smpd_process_t* process
    );


int
smpd_rundown_process(
    smpd_process_t* proc
    );


smpd_process_t*
smpd_get_first_process();


smpd_process_t*
smpd_find_process_by_id(
    UINT16 id
    );


smpd_process_t*
smpd_find_process_by_rank(
    UINT16 rank
    );


const wchar_t*
smpd_get_context_str(
    const smpd_context_t* context
    );


HRESULT
smpd_start_win_mgr(
    smpd_context_t* context,
    int dbg_state
    );


void
smpd_post_abort_command(
    _Printf_format_string_ const wchar_t *fmt,
    ...);


void smpd_kill_all_processes();


void
smpd_translate_win_error(
    _In_ int error,
    _Out_writes_z_(maxlen) wchar_t* msg,
    _In_ int maxlen,
    _Printf_format_string_ PCWSTR prepend,
    ...
    );


_Ret_notnull_
wchar_t*
smpd_pack_cmdline(
    _In_ PCWSTR const argv[],
    _Out_writes_(size) wchar_t* buff,
    _In_ int size
    );


void
smpd_unpack_cmdline(
    _In_ PCWSTR cmdline,
    _Inout_ wchar_t** argv,
    _Out_writes_opt_(*nchars) wchar_t* args,
    _Out_ int* nargs,
    _Out_ int* nchars
    );


_Success_( return == NULL )
_Ret_maybenull_
PCWSTR
smpd_get_hosts_from_file(
    _In_ PCWSTR filename,
    _Outptr_result_z_ wchar_t** phosts
    );


smpd_rank_data_t*
next_rank_data(
    _In_opt_ smpd_rank_data_t* pNode,
    _In_ const smpd_host_t* pHost
    );


_Success_( return == 0 )
_Ret_maybenull_
PCWSTR
smpd_get_argv_from_file(
    _In_ PCWSTR filename,
    _Outptr_ wchar_t ***argvp
    );


void smpd_kill_process(smpd_process_t *process, int exit_code);
void smpd_fix_up_host_tree(smpd_host_t* host);


HRESULT
SmpdProcessBizCard(
    _In_    bool     isResult,
    _In_    SmpdCmd* pCmd,
    _Inout_ SmpdRes* pRes
    );


_Success_(return == RPC_S_OK)
DWORD
smpd_connect_mgr_smpd(
    _In_     smpd_context_type_t context_type,
    _In_opt_z_ PCWSTR hostName,
    _In_     INT16 id,
    _In_     PCWSTR portStr,
    _Outptr_ smpd_context_t** ppContext
    );


DWORD
smpd_connect_root_server(
    _In_ smpd_context_type_t context_type,
    _In_ int                 HostId,
    _In_ PCWSTR              name,
    _In_ bool                isChildSmpd,
    _In_ UINT32              retryCount,
    _In_ UINT32              retryInterval
    );


DWORD
smpd_create_root_server(
    _In_ UINT16          port,
    _In_ UINT16          numThreads
    );


void
smpd_stop_root_server(
    );


DWORD
smpd_create_mgr_server(
    _Inout_opt_ UINT16* pPort,
    _Out_opt_   GUID*   pLrpcPort
    );


void
smpd_stop_mgr_server(
    );


void smpd_close_stdin(
    _Inout_ smpd_process_t* proc
    );


_Success_(return == NOERROR)
DWORD
gle_create_stdin_pipe(
    _In_  ExSetHandle_t set,
    _In_  UINT16        process_id,
    _In_  int           rank,
    _In_  GUID          kvs,
    _Out_ HANDLE*       phClientPipe,
    _Outptr_ smpd_context_t** pstdin_context
    );


_Success_(return == NOERROR)
DWORD
gle_create_stdout_pipe(
    _In_  ExSetHandle_t set,
    _In_  UINT16        process_id,
    _In_  int           rank,
    _In_  GUID          kvs,
    _Out_ HANDLE*       phClientPipe
    );


_Success_(return == NOERROR)
DWORD
gle_create_stderr_pipe(
    _In_  ExSetHandle_t set,
    _In_  UINT16        process_id,
    _In_  int           rank,
    _In_  GUID          kvs,
    _Out_ HANDLE*       phClientPipe
    );


DWORD
smpd_send_close_command(
    smpd_context_t* pContext,
    INT16 dest
    );


bool
smpd_send_stdouterr_command(
    _In_z_ const char* cmd_name,
    _In_ UINT16 rank,
    _In_ GUID kvs,
    _In_ const char* data,
    _In_ UINT32 size
    );


HRESULT
smpd_get_user_from_token(
    _In_ HANDLE hToken,
    _Outptr_ PTOKEN_USER* ppTokenUser
    );


HRESULT
smpd_equal_user_sid(
    _In_ PSID pUserSid1,
    _In_ PSID pUserSid2
    );


RPC_STATUS CALLBACK SmpdSecurityCallbackFn(
    _In_ RPC_IF_HANDLE,
    _In_ void* Context
    );


void
SmpdHandleCmdError(
    _In_ smpd_overlapped_t* pov,
    _In_ int error
    );


inline bool
EnableUnicodeOutput()
{
    fflush( stdout );
    fflush( stderr );
    int res = _setmode( _fileno(stdout), _O_U16TEXT );
    if (res == -1)
    {
        wprintf(L"Error: cannot enable unicode mode for stdout.\n");
        return false;
    }
    res = _setmode( _fileno(stderr), _O_U16TEXT );
    if (res == -1)
    {
        wprintf(L"Error: cannot enable unicode mode for stderr.\n");
        return false;
    }
    smpd_process.unicodeOutput = true;
    return true;
}


inline bool
DirectoryExists(wchar_t *szPath)
{
    DWORD dwAttrib = GetFileAttributesW(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


inline bool
RemoveFileSpec(wchar_t *fileName)
{
    int len = wcslen(fileName);
    for (int i = len - 1; i >= 0; i--)
    {
        if (fileName[i] == L'\\' || fileName[i] == L'/')
        {
            fileName[i] = L'\0';
            return true;
        }
    }
    return false;
}

#include "smpd_overlapped.h"
#endif // SMPD_H
