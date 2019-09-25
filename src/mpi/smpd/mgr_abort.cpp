// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"
#include "smpd.h"


//
// TODO: Need to investigate the return path of this function
// It seems like mpiexec_handle_abort does not return a result
//
static DWORD
smpd_send_abort_command(
    const wchar_t* error_str
    )
{
    SmpdCmd* pCmd = smpd_create_command(
        SMPD_ABORT,
        smpd_process.tree_id,
        SMPD_IS_ROOT );
    if( pCmd == nullptr )
    {
        smpd_err_printf(L"unable to create an abort command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    size_t len = MPIU_Strlen(error_str) + 1;
    pCmd->AbortCmd.error_msg = static_cast<wchar_t*>(
        midl_user_allocate( len * sizeof(wchar_t) ));
    if( pCmd->AbortCmd.error_msg == nullptr )
    {
        delete pCmd;
        smpd_err_printf(L"unable to create an abort command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    MPIU_Strcpy( pCmd->AbortCmd.error_msg, len, error_str );

    SmpdResWrapper* pRes = smpd_create_result_command(
        smpd_process.tree_id,
        nullptr );
    if( pRes == nullptr )
    {
        midl_user_free(pCmd->AbortCmd.error_msg);
        delete pCmd;
        smpd_err_printf(L"unable to create result for 'abort' command.\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    smpd_dbg_printf(L"sending abort command to parent context.\n");
    DWORD rc = smpd_post_command(
        smpd_process.parent_context,
        pCmd,
        &pRes->Res,
        nullptr );
    if( rc != NOERROR )
    {
        midl_user_free(pCmd->AbortCmd.error_msg);
        delete pCmd;
        delete pRes;
        smpd_err_printf(L"Unable to add the error string to the abort command.\n");
    }

    return rc;
}


void smpd_post_abort_command(const wchar_t *fmt, ...)
{
    wchar_t error_str[2048] = L"";
    va_list list;

    va_start(list, fmt);
    MPIU_Vsnprintf(error_str, _countof(error_str), fmt, list);
    va_end(list);

    if(smpd_process.parent_context == NULL)
    {
        fwprintf(stderr, L"\nAborting: %s\n", error_str);
        fflush(stderr);
        smpd_signal_exit_progress(MPI_ERR_INTERN);
    }

    int rc = smpd_send_abort_command(error_str);
    if(rc != MPI_SUCCESS)
    {
        smpd_err_printf(L"unable to send abort command to the parent context.\n");
        smpd_signal_exit_progress(rc);
    }
}
