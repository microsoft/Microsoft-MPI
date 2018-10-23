// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "smpd.h"
#include "PmiDbgImpl.h"
#include <wmistr.h>
#include <evntrace.h>


//
// FW define the callback functions.
//
static FN_PmiDbgControl SmpdPmiDbgControlBeforeCreateProcess;
static FN_PmiDbgControl SmpdPmiDbgControlAfterCreateProcess;


//
// Define the notification events for SMPD
//
const PMIDBG_NOTIFICATION SmpdNotifyInitialize =
{
    PMIDBG_NOTIFY_INITIALIZE,
    NULL
};

const PMIDBG_NOTIFICATION SmpdNotifyFinalize =
{
    PMIDBG_NOTIFY_FINALIZE,
    NULL
};

const PMIDBG_NOTIFICATION SmpdNotifyBeforeCreateProcess =
{
    PMIDBG_NOTIFY_BEFORE_CREATE_PROCESS,
    SmpdPmiDbgControlBeforeCreateProcess
};

const PMIDBG_NOTIFICATION SmpdNotifyAfterCreateProcess =
{
    PMIDBG_NOTIFY_AFTER_CREATE_PROCESS,
    SmpdPmiDbgControlAfterCreateProcess
};


static HRESULT __stdcall
SmpdPmiDbgControlBeforeCreateProcess(
    __in PMIDBG_OPCODE_TYPE               type,
    __in void*                           pData,
    __inout_bcount(cbBuffer) void*       pBuffer,
    __in SIZE_T                          cbBuffer
    )
{
    va_list     args            = reinterpret_cast<va_list>(pData);
    char*       pExe            = va_arg(args,char*);
    char*       pArgs           = va_arg(args,char*);
    int         rank            = va_arg(args,int);

    switch( type )
    {
    case PMIDBG_OPCODE_GET_PROCESS_COMMAND:
        {
            if( cbBuffer < sizeof(pExe) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *((char**)pBuffer) = pExe;
        }
        break;
    case PMIDBG_OPCODE_GET_PROCESS_ARGUMENTS:
        {
            if( cbBuffer < sizeof(pArgs) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *((char**)pBuffer) = pArgs;
        }
        break;
    case PMIDBG_OPCODE_GET_PROCESS_RANK:
        {
            if( cbBuffer < sizeof(rank) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *((int*)pBuffer) = rank;
        }
        break;
    default:
        return E_INVALIDARG;
    }
    return S_OK;
}


static HRESULT __stdcall
SmpdPmiDbgControlAfterCreateProcess(
    __in PMIDBG_OPCODE_TYPE               type,
    __in void*                           pData,
    __inout_bcount(cbBuffer) void*       pBuffer,
    __in SIZE_T                          cbBuffer
    )
{
    va_list                 args            = reinterpret_cast<va_list>(pData);
    char*                   pExe            = va_arg(args,char*);
    char*                   pArgs           = va_arg(args,char*);
    int                     rank            = va_arg(args,int);
    PROCESS_INFORMATION*    pProcInfo       = va_arg(args,PROCESS_INFORMATION*);
    BOOL*                   pOverrideResume = va_arg(args,BOOL*);

    switch( type )
    {
    case PMIDBG_OPCODE_GET_PROCESS_COMMAND:
        {
            if( cbBuffer < sizeof(pExe) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *((char**)pBuffer) = pExe;
        }
        break;
    case PMIDBG_OPCODE_GET_PROCESS_ARGUMENTS:
        {
            if( cbBuffer < sizeof(pArgs) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *((char**)pBuffer) = pArgs;
        }
        break;
    case PMIDBG_OPCODE_GET_PROCESS_RANK:
        {
            if( cbBuffer < sizeof(rank) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *((int*)pBuffer) = rank;
        }
        break;
    case PMIDBG_OPCODE_GET_PROCESS_INFORMATION:
        {
            if( cbBuffer < sizeof(pProcInfo) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *((PROCESS_INFORMATION**)pBuffer) = pProcInfo;
        }
        break;
    case PMIDBG_OPCODE_OVERRIDE_PROCESS_RESUME:
        {
            *pOverrideResume = TRUE;
        }
        break;
    default:
        return E_INVALIDARG;
    }
    return S_OK;
}
