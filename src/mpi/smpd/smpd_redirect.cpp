// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"

static DWORD
gle_create_pipe(
    ExSetHandle_t set,
    BOOL fClientReads,
    PHANDLE phServerPipe,
    PHANDLE phClientPipe
    )
{
    static LONG PipeSerialNumber;

    DWORD ServerOpenMode;
    DWORD ClientOpenMode;

    HANDLE hClient;
    HANDLE hServer;
    wchar_t PipeName[64];

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;


    MPIU_Snprintf(
        PipeName,
        _countof(PipeName),
        L"\\\\.\\pipe\\msmpi_std_pipe.%x.%x",
        GetCurrentProcessId(),
        InterlockedIncrement(&PipeSerialNumber)
        );

    ServerOpenMode  = FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
    ServerOpenMode |= fClientReads ?  PIPE_ACCESS_OUTBOUND : PIPE_ACCESS_INBOUND;

    hServer = CreateNamedPipeW(
                PipeName,       // lpName
                ServerOpenMode, // dwOpenMode
                PIPE_TYPE_BYTE, // dwPipeMode
                1,              // nMaxInstances
                0,              // nOutBufferSize
                0,              // nInBufferSize
                120 * 1000,     // nDefaultTimeOut
                NULL            // lpSecurityAttributes
                );

    if(hServer == INVALID_HANDLE_VALUE)
    {
        DWORD gle = GetLastError();
        if( gle == ERROR_ACCESS_DENIED )
        {
            //
            // Try using local pipe instead
            //
            smpd_dbg_printf(L"failed to create named pipe for smpd and mpi process redirection, trying again with local named pipe\n");
            MPIU_Snprintf(
                PipeName,
                _countof(PipeName),
                L"\\\\.\\pipe\\local\\msmpi_std_pipe.%x.%x",
                GetCurrentProcessId(),
                InterlockedIncrement(&PipeSerialNumber)
                );
            hServer = CreateNamedPipeW(
                PipeName,       // lpName
                ServerOpenMode, // dwOpenMode
                PIPE_TYPE_BYTE, // dwPipeMode
                1,              // nMaxInstances
                0,              // nOutBufferSize
                0,              // nInBufferSize
                120 * 1000,     // nDefaultTimeOut
                NULL            // lpSecurityAttributes
                );
            if( hServer == INVALID_HANDLE_VALUE )
            {
                return GetLastError();
            }
        }
        else
        {
            return gle;
        }
    }

    ClientOpenMode = fClientReads ? GENERIC_READ : GENERIC_WRITE;

    hClient = CreateFileW(
                PipeName,       // lpFileName
                ClientOpenMode, // dwDesiredAccess
                0,              // dwShareMode
                &sa,            // lpSecurityAttributes
                OPEN_EXISTING,  // dwCreationDisposition
                0,              // dwFlagsAndAttributes
                NULL            // hTemplateFile
                );
    OACR_WARNING_ENABLE(USE_WIDE_API, "restoring");

    if(hClient == INVALID_HANDLE_VALUE)
    {
        DWORD gle = GetLastError();
        CloseHandle(hServer);
        return gle;
    }

    if( set != nullptr )
    {
        ExAttachHandle(set, hServer);
    }

    *phServerPipe = hServer;
    *phClientPipe = hClient;
    return NO_ERROR;
}


static
inline
void
smpd_redir_init_overlapped(
    smpd_overlapped_t* pov,
    smpd_state_completion_routine pfn,
    smpd_context_t* context
    )
{
    smpd_init_overlapped(pov, pfn, context);
    pov->uov.ov.Offset = 0;
    pov->uov.ov.OffsetHigh = 0;
}


//----------------------------------------------------------------------------
//
// STDIN redirection
// smpd writes the stdin data from mpiexec to rank 0 stdin
//
//----------------------------------------------------------------------------

static void smpd_state_smpd_writing_data_to_stdin(_In_ EXOVERLAPPED* pexov);


static DWORD
gle_write_stdin(
    _In_ smpd_context_t* context,
    _In_ const char* buffer,
    _In_ UINT32 length,
    _In_ smpd_overlapped_t* pov
    )
{
    BOOL fSucc;
    DWORD nBytesWritten;

    smpd_redir_init_overlapped(pov, smpd_state_smpd_writing_data_to_stdin, context);
    fSucc = WriteFile(
                context->pipe,
                buffer,
                length,
                &nBytesWritten,
                &pov->uov.ov
                );
    if(!fSucc)
    {
        DWORD gle = GetLastError();
        if(gle != ERROR_IO_PENDING)
        {
            return gle;
        }
    }

    return NO_ERROR;
}


static bool
smpd_state_smpd_writing_data_to_stdin_prolog(
    _In_    smpd_overlapped_t* pov,
    _Inout_ smpd_process_t*    proc
    )
{
    DWORD gle;
    smpd_stdin_write_node_t* node;
    smpd_context_t* context = pov->pContext;

    node = proc->stdin_write_queue.dequeue();
    ASSERT(node != NULL);

    smpd_dbg_printf(L"wrote %u bytes to stdin of rank 0\n", node->length);
    free(node);

    node = proc->stdin_write_queue.head();
    if(node == NULL)
    {
        //
        // no more stdin to write, we're done free the overlpapped
        //
        smpd_free_overlapped(pov);
        return true;
    }

    if(node->length == 0)
    {
        //
        // zero length buffer signals to close stdin
        //
        smpd_close_stdin(proc);
        smpd_free_overlapped(pov);
        return true;
    }

    gle = gle_write_stdin(context, node->buffer, node->length, pov);
    if(gle)
    {
        smpd_err_printf(L"unable to write the next %u bytes to stdin of rank 0. error %u\n",
                        node->length, gle);
        return false;
    }

    return true;
}


static void
smpd_state_smpd_writing_data_to_stdin(
    _In_ EXOVERLAPPED* pexov
    )
{
    smpd_overlapped_t* pov = smpd_ov_from_exov(pexov);
    smpd_context_t* context = pov->pContext;
    smpd_process_t* proc = smpd_find_process_by_id(context->process_id);

    if(proc == nullptr || proc->in == nullptr)
    {
        //
        // Handle rundown; the stdin connection is already close, waiting for this
        // async operation to complete.
        //
        smpd_free_context(context);
        smpd_free_overlapped(pov);
        return;
    }

    if(smpd_state_smpd_writing_data_to_stdin_prolog(pov, proc))
    {
        return;
    }

    smpd_dbg_printf(L"writing stding to rank 0 failed, closing context.\n");
    smpd_close_stdin(proc);
    smpd_free_overlapped(pov);
}



//
// Summary:
//  Handler for the stdincommand from MPIEXEC
//
// Parameters:
//  pov     - pointer to the overlapped structure that
//            contains pointers to the command and result
//
DWORD
smpd_handle_stdin_command(
    _In_ smpd_overlapped_t* pov
    )
{
    smpd_process_t* proc = smpd_find_process_by_rank( 0 );
    if( proc == nullptr || proc->in == nullptr )
    {
        //
        // The process could be a quick running program and exits already
        //
        return NOERROR;
    }
    SmpdStdinCmd* pStdinCmd = &pov->pCmd->StdinCmd;

    UINT32 length = static_cast<UINT32>( pStdinCmd->size / SMPD_ENCODING_FACTOR + 1 );

    smpd_stdin_write_node_t* node = static_cast<smpd_stdin_write_node_t*>(
        malloc( sizeof(smpd_stdin_write_node_t) + length * sizeof(char) ) );
    if( node == nullptr )
    {
        //
        // I think we can delay smpd_close_stdin(proc)
        // Returning ERROR_NOT_ENOUGH_MEMORY so that if the caller
        // wants to try again it is possible at some other time when
        // we have memory available
        //
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    smpd_decode_buffer(
        reinterpret_cast<char*>(pStdinCmd->buffer),
        node->buffer, length, &length );

    node->length = length;

    if( proc->stdin_write_queue.head() != nullptr )
    {
        //
        // If the stdin queue is not empty we queue the stdin data up
        // and reuse the overlapped
        //
        proc->stdin_write_queue.enqueue(node);
        return NOERROR;
    }

    //
    // Creating a new overlapped to handle stdin because the existing
    // overlapped will be freed upon the return of this function. We
    // will also complete the async to tell the caller that we have
    // queued the stdin request for processing
    //
    smpd_overlapped_t* povNew = smpd_create_overlapped();
    if( povNew == nullptr )
    {
        free(node);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    proc->stdin_write_queue.enqueue(node);
    DWORD err = gle_write_stdin(proc->in, node->buffer, node->length, povNew);
    if( err != NOERROR )
    {
        //
        // Note that this is guaranteed to dequeue the "node" that we
        // just queued earlier because we are only entering this function
        // in a single_threaded manner (smpd_handle_x_command) due to
        // the smpd_handle_command call dispatch being queued in the main
        // thread's IOCP
        //
        proc->stdin_write_queue.dequeue();

        ASSERT( proc->stdin_write_queue.head() == nullptr );
        smpd_err_printf( L"unable to write %u bytes to stdin of rank 0. error %u\n",
                         node->length,
                         err );
        free(node);
        smpd_free_overlapped( povNew );
    }

    return err;
}


//
// STDIN Close redirection
// smpd signals rank 0 that stdin was closed for mpiexec
//
DWORD
smpd_handle_stdin_close_command(
    _In_ smpd_overlapped_t* pov
    )
{
    UNREFERENCED_PARAMETER( pov );
    smpd_process_t* proc = smpd_find_process_by_rank(0);
    if( proc == nullptr || proc->in == nullptr )
    {
        //
        // The process could be a quick running program and exits already
        //
        return NOERROR;
    }

    if( proc->stdin_write_queue.head() == nullptr )
    {
        smpd_close_stdin(proc);
        return NOERROR;
    }

    //
    // Queue the close request; zero length buffer signify close request
    //
    smpd_stdin_write_node_t* node = static_cast<smpd_stdin_write_node_t*>(
        malloc( sizeof(smpd_stdin_write_node_t) ) );
    if( node == nullptr )
    {
        smpd_close_stdin(proc);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    node->length = 0;
    proc->stdin_write_queue.enqueue(node);

    return NOERROR;
}


_Success_(return == NOERROR)
DWORD
gle_create_stdin_pipe(
    _In_ ExSetHandle_t set,
    _In_ UINT16 process_id,
    _In_ int rank,
    _In_ GUID kvs,
    _Out_ HANDLE* phClientPipe,
    _Outptr_ smpd_context_t** pstdin_context
    )
{
    DWORD gle;
    smpd_context_t* context;

    context = smpd_create_context(SMPD_CONTEXT_APP_STDIN, set);
    if(context == NULL)
    {
        smpd_err_printf(L"failed to allocate stdin forwarding context\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    gle = gle_create_pipe(set, TRUE, &context->pipe, phClientPipe);
    if(gle)
    {
        smpd_err_printf(L"failed to create stdin forwarding pipe, error %u\n", gle);
        smpd_free_context(context);
        return gle;
    }

    context->process_id = process_id;
    context->process_rank = rank;
    context->kvs = kvs;
    *pstdin_context = context;
    return NO_ERROR;
}


//----------------------------------------------------------------------------
//
// STDOUT/ERR redirection
// smpd reads rank n stdout/err and sends it to mpiexec
//
//----------------------------------------------------------------------------

static void smpd_state_reading_stdouterr(EXOVERLAPPED* pexov);

static DWORD gle_read_stdouterr(smpd_overlapped_t* pov, smpd_context_t* context)
{
    BOOL fSucc;
    DWORD nBytesRead;

    smpd_redir_init_overlapped(pov, smpd_state_reading_stdouterr, context);

    fSucc = ReadFile(
                context->pipe,
                context->stdioBuffer,
                SMPD_MAX_STDIO_LENGTH,
                &nBytesRead,
                &pov->uov.ov
                );

    if(!fSucc)
    {
        DWORD gle = GetLastError();
        if(gle != ERROR_IO_PENDING)
            return gle;
    }

    return NO_ERROR;
}


bool
smpd_send_stdouterr_command(
    _In_z_ const wchar_t* cmd_name,
    _In_ UINT16 rank,
    _In_ GUID kvs,
    _In_ const char* data,
    _In_ UINT32 size
    )
{
    if( smpd_process.parent_closed )
    {
        smpd_dbg_printf(L"parent terminated, not sending stdout/stderr command\n");
        return true;
    }

    SMPD_CMD_TYPE cmdType;
    if( CompareStringW( LOCALE_INVARIANT,
                        0,
                        cmd_name,
                        -1,
                        L"stdout",
                        -1 ) == CSTR_EQUAL )
    {
        cmdType = SMPD_STDOUT;
    }
    else
    {
        cmdType = SMPD_STDERR;
    }

    SmpdCmd* pCmd = smpd_create_command(
        cmdType,
        smpd_process.tree_id,
        SMPD_IS_ROOT ); // output always goes to mpiexec
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create an output command.\n");
        return false;
    }

    //
    // Note that both stdout and stderr use the same
    // StdoutCmd structure
    //
    pCmd->StdoutCmd.rank = rank;
    pCmd->StdoutCmd.kvs = kvs;
    pCmd->StdoutCmd.buffer = reinterpret_cast<BYTE*>(
        const_cast<char*>(data) );
    pCmd->StdoutCmd.size = size;
    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        nullptr );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for output command.\n");
        return false;
    }

    DWORD rc = smpd_post_command(
        smpd_process.parent_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK  )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"unable to post %s command from the root.\n", cmd_name);
        return false;
    }

    return true;
}


static bool
smpd_state_reading_stdouterr_prolog(
    smpd_overlapped_t* pov
    )
{
    UINT32 num_encoded;
    DWORD gle;
    DWORD num_read = ExGetBytesTransferred(&pov->uov);
    HRESULT status = ExGetStatus(&pov->uov);
    smpd_context_t* context = pov->pContext;
    char buffer[SMPD_MAX_STDIO_LENGTH * SMPD_ENCODING_FACTOR + 1];

    if(FAILED(status))
    {
        smpd_dbg_printf(L"reading failed, assuming %s is closed. error 0x%08x\n",
                        smpd_get_context_str(context), status);
        return false;
    }

    if(num_read == 0)
    {
        smpd_dbg_printf(L"reading zero bytes, assuming %s is closed.\n",
                        smpd_get_context_str(context));
        return false;
    }

    ASSERT(num_read <= SMPD_MAX_STDIO_LENGTH);
    smpd_dbg_printf(L"read %u bytes from %s\n",
                    num_read,
                    smpd_get_context_str(context));

    smpd_encode_buffer(
        buffer,
        _countof(buffer),
        context->stdioBuffer,
        num_read,
        &num_encoded);
    ASSERT(num_encoded == num_read);

    /* create an output command */
    if(smpd_send_stdouterr_command(
           smpd_get_context_str(context),
           static_cast<UINT16>( context->process_rank ),
           context->kvs,
           buffer,
           num_encoded * 2 + 1 ) == false )
    {
        smpd_err_printf(L"unable to send the encoded %s command.\n",
                        smpd_get_context_str(context));
        return false;
    }

    /* post a read for the next byte of data */
    gle = gle_read_stdouterr(pov, context);
    if(gle)
    {
        smpd_err_printf(L"unable to post a read on %s context, error %u.\n",
                        smpd_get_context_str(context), gle);
        return false;
    }

    return true;
}


static void smpd_state_reading_stdouterr(EXOVERLAPPED* pexov)
{
    smpd_overlapped_t* pov = smpd_ov_from_exov(pexov);
    smpd_context_t* context = pov->pContext;

    if(smpd_state_reading_stdouterr_prolog(pov))
    {
        return;
    }

    CloseHandle(context->pipe);
    smpd_handle_process_close(&pov->uov);
}


_Success_( return == NOERROR )
static DWORD
smpd_create_stdouterr_pipe(
    _In_ smpd_context_type_t context_type,
    _In_ ExSetHandle_t set,
    _In_ UINT16 process_id,
    _In_ int rank,
    _In_ GUID kvs,
    _Out_ HANDLE* phClientPipe
    )
{
    DWORD gle;
    smpd_context_t* context;
    smpd_overlapped_t* pov;

    pov = smpd_create_overlapped();
    if(pov == NULL)
    {
        smpd_err_printf(L"failed to allocate std forwarding overlapped\n");
        gle = ERROR_NOT_ENOUGH_MEMORY;
        goto fn_fail1;
    }

    context = smpd_create_context(context_type, set);
    if(context == NULL)
    {
        smpd_err_printf(L"failed to allocate std forwarding context\n");
        gle = ERROR_NOT_ENOUGH_MEMORY;
        goto fn_fail2;
    }

    gle = gle_create_pipe(set, FALSE, &context->pipe, phClientPipe);
    if(gle)
    {
        smpd_err_printf(L"failed to create forwarding pipe, error %u\n", gle);
        goto fn_fail3;
    }

    gle = gle_read_stdouterr(pov, context);
    if(gle)
    {
        smpd_err_printf(L"unable to read %s pipe, error %u\n",
                        smpd_get_context_str(context), gle);
        goto fn_fail4;
    }

    context->process_id = process_id;
    context->process_rank = rank;
    context->kvs = kvs;
    return NO_ERROR;

fn_fail4:
    CloseHandle(*phClientPipe);
    CloseHandle(context->pipe);
fn_fail3:
    smpd_free_context(context);
fn_fail2:
    smpd_free_overlapped(pov);
fn_fail1:
    return gle;
}


_Success_( return == NOERROR )
DWORD
gle_create_stdout_pipe(
    _In_  ExSetHandle_t set,
    _In_  UINT16        process_id,
    _In_  int           rank,
    _In_  GUID          kvs,
    _Out_ HANDLE*       phClientPipe
    )
{
    return smpd_create_stdouterr_pipe(
        SMPD_CONTEXT_APP_STDOUT,
        set,
        process_id,
        rank,
        kvs,
        phClientPipe );
}


_Success_( return == NOERROR )
DWORD
gle_create_stderr_pipe(
    _In_  ExSetHandle_t set,
    _In_  UINT16        process_id,
    _In_  int           rank,
    _In_  GUID          kvs,
    _Out_ HANDLE*       phClientPipe
    )
{
    return smpd_create_stdouterr_pipe(
        SMPD_CONTEXT_APP_STDERR,
        set,
        process_id,
        rank,
        kvs,
        phClientPipe );
}
