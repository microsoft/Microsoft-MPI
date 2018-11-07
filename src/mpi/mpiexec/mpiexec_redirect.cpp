// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "smpd.h"

//----------------------------------------------------------------------------
//
// STDIN redirection
// mpiexec reads stdin and sends this data to smpd 1 targeting rank 0
// when stdin is closed the signal is sent to smpd 1 targeting rank 0
//
//----------------------------------------------------------------------------

struct stdin_overlapped_t
{
    EXOVERLAPPED ov;
    char buffer[SMPD_MAX_STDIO_LENGTH];

    stdin_overlapped_t(ExCompletionRoutine pfn)
    {
        ExInitOverlapped(&ov, pfn, pfn);
    }
};


static inline stdin_overlapped_t* stdin_ov_from_exov(EXOVERLAPPED* pexov)
{
    return CONTAINING_RECORD(pexov, stdin_overlapped_t, ov);
}


static int mpiexec_state_reading_stdin(EXOVERLAPPED* pexov);

static DWORD WINAPI mpiexec_stdin_read_thread(LPVOID /*pv*/)
{
    stdin_overlapped_t* pov;

    for(;;)
    {
        pov = new stdin_overlapped_t(mpiexec_state_reading_stdin);
        if(pov == NULL)
        {
            smpd_err_printf(L"unable to create a context for stdin.\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        if((hStdin == INVALID_HANDLE_VALUE) || (hStdin == NULL))
        {
            //
            // Don't print an error in case there is no stdin handle
            //
            smpd_dbg_printf(L"Unable to get the stdin handle.\n");
            goto fn_fail;
        }

        //
        // ReadFile returns with less than nNumberOfBytesToRead on EOL when
        // using input device. it returns on EOF when files are piped in.
        // Try to read as much as posible.
        //
        DWORD num_read;
        if(!ReadFile(hStdin, pov->buffer, sizeof(pov->buffer), &num_read, NULL))
        {
            DWORD gle = GetLastError();
            smpd_dbg_printf(L"ReadFile failed (%u), closing stdin reader thread.\n", gle);
            goto fn_fail;
        }

        //
        // zero bytes indicates no input available
        //
        if(num_read == 0)
            goto fn_fail;

        ExPostOverlappedResult(smpd_process.set, &pov->ov, MPI_SUCCESS, num_read);
    }

fn_fail:
    //
    // report that stdin is closed or no more input available by posting an error result
    //
    ExPostOverlappedResult(smpd_process.set, &pov->ov, STATUS_NO_MEMORY, 0);
    return ERROR_HANDLE_EOF;
}


static DWORD
mpiexec_send_stdin_command(
    _In_reads_z_(cchBuffer) const char* buffer,
    _In_ UINT32 cchBuffer
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_STDIN,
        SMPD_IS_ROOT,
        SMPD_IS_TOP );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create stdin command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pCmd->StdinCmd.size = cchBuffer;
    pCmd->StdinCmd.buffer = reinterpret_cast<BYTE*>(
        const_cast<char*>(buffer) );

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        nullptr );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for stdin command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD rc = smpd_post_command(
        smpd_process.left_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"unable to post stdin command from root.\n");
    }
    return rc;
}


static DWORD
mpiexec_send_stdin_close_command(void)
{
    if( smpd_process.left_context == nullptr )
    {
        //
        // Stdin is getting disconnected due to mpiexec tree being torn down
        // In this case we do not post a stdin_close command
        //
        smpd_dbg_printf(L"child context already closed. Not sending stdin_close request\n");
        return NOERROR;
    }

    SmpdCmd* pCmd = smpd_create_command(
        SMPD_STDIN_CLOSE,
        SMPD_IS_ROOT,
        SMPD_IS_TOP );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create stdin_close command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        nullptr );
    if( pRes == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create result for stdin_close command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD rc = smpd_post_command(
        smpd_process.left_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != RPC_S_OK )
    {
        delete pRes;
        delete pCmd;
        smpd_err_printf(L"unable to post stdin_close command from root.\n");
    }

    return rc;
}


static int
mpiexec_state_reading_stdin(
    EXOVERLAPPED* pexov
    )
{
    UINT32 num_encoded;
    DWORD num_read = ExGetBytesTransferred(pexov);
    HRESULT status = ExGetStatus(pexov);
    stdin_overlapped_t* pov = stdin_ov_from_exov(pexov);
    char buffer[SMPD_MAX_STDIO_LENGTH * SMPD_ENCODING_FACTOR + 1];

    if(FAILED(status))
    {
        goto fn_stdin_close;
    }

    ASSERT(num_read > 0);
    ASSERT(num_read <= SMPD_MAX_STDIO_LENGTH);

    smpd_encode_buffer(buffer, _countof(buffer), pov->buffer, num_read, &num_encoded);
    ASSERT(num_encoded == num_read);

    //
    // The number of characters we're sending is the null terminated
    // encoded string which has the size of num_encoded*2 + 1
    //
    DWORD rc = mpiexec_send_stdin_command(buffer, num_encoded*2 + 1);
    if( rc != NOERROR )
    {
        goto fn_stdin_close;
    }

fn_exit:
    delete pov;

    return NOERROR;

fn_stdin_close:
    smpd_dbg_printf(L"stdin to mpiexec closed. sending stdin_close command.\n");
    mpiexec_send_stdin_close_command();
    goto fn_exit;
}


DWORD mpiexec_redirect_stdin(void)
{
    DWORD tid;
    HANDLE hThread;

    //
    // Unfortunately, we can't use overlapped operations with stdin;
    // create a thread to read stdin and post the read buffers to the
    // main thread.
    //
    hThread = CreateThread(nullptr, 0, mpiexec_stdin_read_thread, nullptr, 0, &tid);
    if(hThread == nullptr)
    {
        DWORD gle = GetLastError();
        smpd_err_printf(L"Unable to create a thread to read stdin, error %u\n", gle);
        return gle;
    }

    CloseHandle(hThread);

    smpd_process.stdin_redirecting = TRUE;

    return NOERROR;
}


//----------------------------------------------------------------------------
//
// STDOUT/STDERR redirection
// mpiexec writes rank n stdout/stderr sent by smpd to mpiexec stdout
//
// mpiexec output optimizes for a single stdout/err output stream; e.g., the
// console or a single file.  Extra '\n' might be written when using separate
// stdout/stderr streams as a result of cross sream rank switching.
//
//----------------------------------------------------------------------------

static void printstd(const void* buffer, size_t num_bytes, HANDLE hFile)
{
    //
    // N.B. The output is written raw to the target; the CR-LF
    // translation happen already by the mpi process
    //
    DWORD nBytesWritten;
    WriteFile(hFile, buffer, (DWORD)num_bytes, &nBytesWritten, NULL);
}


static const char* search_eol(const char* s)
{
    while(*s != '\0' && *s != '\n')
    {
        s++;
    }

    return s;
}


static void write_prefixed_output(const char* data, int rank, BOOL no_prefix, HANDLE std)
{
    const char* start;
    const char* end;
    size_t prefix_length;
    char prefix[20];

    prefix_length = MPIU_Snprintf(prefix, _countof(prefix), "[%d]", rank);

    start = data;
    for(;;)
    {
        end = search_eol(start);
        if(*end != '\0')
        {
            end++;
        }

        if(end == start)
            break;

        if(no_prefix)
        {
            no_prefix = FALSE;
        }
        else
        {
            printstd(prefix, prefix_length, std);
        }
        printstd(start, end - start, std);

        start = end;
    }
}


static DWORD
mpiexec_handle_stdouterr_command(
    _In_ smpd_overlapped_t* pov,
    _In_ HANDLE std
    )
{
    //
    // Both StdoutCmd and StderrCmd have the same memory
    // layout
    //
    SmpdStdoutCmd* pStdoutCmd = &pov->pCmd->StdoutCmd;

    //
    // Save the previous rank to print out. The value -2 signifies that no new line
    // is required before printing.
    //
    // Note that this code is optimized for a single stdout/err output stream, thus
    // using a single value for both stdout and stderr. (see note above)
    //
    static int s_prev_rank = -2;
    static GUID s_prev_kvs = GUID_NULL;

    //
    // Get the rank and data to print
    //
    UINT16 rank = pStdoutCmd->rank;
    GUID kvs = pStdoutCmd->kvs;

    UINT32 length = pStdoutCmd->size;

    BOOL same_rank_no_eol = (s_prev_rank == static_cast<int>(rank)) && (IsEqualGUID(s_prev_kvs, kvs));

    if(!same_rank_no_eol && (s_prev_rank != -2))
    {
        //
        // The output is written raw; put CR and LF characters
        //
        printstd("\r\n", 2, std);
    }

    //smpd_dbg_printf(L"Decoding stdout/stderr buffer %S\n", pStdoutCmd->buffer);

    //
    // Decode the data buffer in-place and remember if it ends with eol.
    //
    UINT32 num_decoded = 0;
    smpd_decode_buffer(
        reinterpret_cast<char*>(pStdoutCmd->buffer),
        reinterpret_cast<char*>(pStdoutCmd->buffer),
        length,
        &num_decoded);
    if(pStdoutCmd->buffer[num_decoded - 1] == '\n')
    {
        s_prev_rank = -2;
    }
    else
    {
        s_prev_rank = rank;
        s_prev_kvs = kvs;
    }

    //
    // No need for prefix
    //
    if(!smpd_process.prefix_output)
    {
        printstd(reinterpret_cast<char*>(pStdoutCmd->buffer), num_decoded, std);
        return NOERROR;
    }

    write_prefixed_output(reinterpret_cast<char*>(pStdoutCmd->buffer),
                          rank, same_rank_no_eol, std);
    return NOERROR;
}


DWORD
mpiexec_handle_stdout_command(
    smpd_overlapped_t* pov
    )
{
    smpd_dbg_printf(L"Handling SMPD_STDOUT\n");
    return mpiexec_handle_stdouterr_command(
        pov,
        GetStdHandle(STD_OUTPUT_HANDLE) );
}


DWORD
mpiexec_handle_stderr_command(
    smpd_overlapped_t* pov
    )
{
    smpd_dbg_printf(L"Handling SMPD_STDERR\n");
    return mpiexec_handle_stdouterr_command(
        pov,
        GetStdHandle(STD_ERROR_HANDLE) );
}
