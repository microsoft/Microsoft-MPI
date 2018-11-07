// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "mpiexec.h"
#include "PmiDbgImpl.h"

extern "C"
{
__declspec(dllexport) MPIR_PROCDESC* MPIR_Proctable;
__declspec(dllexport) int MPIR_Proctable_size;
__declspec(dllexport) volatile int MPIR_debug_state    = 0;
__declspec(dllexport) volatile int MPIR_being_debugged = 0;
}

//
// FW define the callback functions.
//
static FN_PmiDbgControl MpiexecPmiDbgControlBeforeCreateProcesses;


//
// Define the notification events in Mpiexec
//
const PMIDBG_NOTIFICATION MpiexecNotifyInitialize =
{
    PMIDBG_NOTIFY_INITIALIZE,
    NULL
};

const PMIDBG_NOTIFICATION MpiexecNotifyFinalize =
{
    PMIDBG_NOTIFY_FINALIZE,
    NULL
};

const PMIDBG_NOTIFICATION MpiexecNotifyBeforeCreateProcesses =
{
    PMIDBG_NOTIFY_BEFORE_CREATE_PROCESSES,
    MpiexecPmiDbgControlBeforeCreateProcesses
};


const PMIDBG_NOTIFICATION MpiexecNotifyAfterCreateProcesses =
{
    PMIDBG_NOTIFY_AFTER_CREATE_PROCESSES,
    NULL
};


static HRESULT __stdcall
MpiexecPmiDbgControlBeforeCreateProcesses(
    __in PMIDBG_OPCODE_TYPE             type,
    __in void*                          pData,
    __inout_bcount(cbBuffer) void*      pBuffer,
    __in SIZE_T                         cbBuffer
    )
{
    va_list             args   = reinterpret_cast<va_list>(pData);
    smpd_global_t*      pSmpdProcess  = va_arg(args,smpd_global_t*);
    smpd_host_t*        pHosts = va_arg(args,smpd_host_t*);
    switch(type)
    {
    case PMIDBG_OPCODE_GET_WORLD_SIZE:
        {
            if( cbBuffer < sizeof(pSmpdProcess->nproc) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *(reinterpret_cast<unsigned int*>(pBuffer)) = static_cast<unsigned int>( pSmpdProcess->nproc );
        }
        break;
    case PMIDBG_OPCODE_ENUM_WORLD_NODES:
        {
            PMIDBG_ENUM_WORLD_NODES* pEnum;
            smpd_host_t*             pEntry;

            if( cbBuffer < sizeof(*pEnum) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }

            pEnum = static_cast<PMIDBG_ENUM_WORLD_NODES*>( pBuffer );

            //
            // If they pass in a list that is already at the end, we error
            //
            if( pEnum->Context == PMIDBG_ENUM_END )
            {
                return E_INVALIDARG;
            }

            if( pEnum->Context == PMIDBG_ENUM_BEGIN )
            {
                //
                // If they are requesting the begin
                //
                pEntry = pHosts;
            }
            else
            {
                //
                // Else use the context value passed in
                //  NOTE: PMIDBG_ENUM_BEGIN == 0, so null is not possible
                //
                pEntry = static_cast<smpd_host_t*>(
                         reinterpret_cast<smpd_host_t*>( pEnum->Context )->Next);
            }

            if( NULL == pEntry )
            {
                pEnum->Context  = PMIDBG_ENUM_END;
            }
            else
            {
                pEnum->Context  = reinterpret_cast<LONG_PTR>( pEntry );
                MPIU_WideCharToMultiByte( pEntry->name, &pEntry->nameA );

                pEnum->Hostname = pEntry->nameA;
            }
        }
        break;
    case PMIDBG_OPCODE_GET_PROCSIZE_ADDR:
        {
            if( cbBuffer < sizeof(&MPIR_Proctable_size) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *(reinterpret_cast<int**>(pBuffer)) = &MPIR_Proctable_size;
        }
        break;
    case PMIDBG_OPCODE_GET_PROCTABLE_ADDR:
        {
            if( cbBuffer < sizeof(MPIR_Proctable) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            *(reinterpret_cast<MPIR_PROCDESC***>(pBuffer)) = &MPIR_Proctable;
        }
        break;
    case PMIDBG_OPCODE_GET_DEBUG_MODE:
        {
            if( cbBuffer < sizeof(MPIDBG_DBG_MODE) )
            {
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            }
            if( MPIR_being_debugged == 1 )
            {
                *(reinterpret_cast<MPIDBG_DBG_MODE*>(pBuffer)) = MPIDBG_DBG_LAUNCH;
            }
            else
            {
                *(reinterpret_cast<MPIDBG_DBG_MODE*>(pBuffer)) = MPIDBG_DBG_ATTACH;
            }
        }
        break;
    default:
        return E_INVALIDARG;
    }
    return S_OK;
}
