// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "smpd.h"
#include "PmiDbgImpl.h"


//
// FW Define callback handlers.
//
static FN_PmiDbgControl   HandlePmiDbgControl;
static FN_PmiDbgUnload  HandlePmiDbgUnload;


//
// Local structure def to hold the chain of loaded
//  extension dlls and the function table for each
//
typedef struct _PMIDBG_EXTENSION
{
    struct _PMIDBG_EXTENSION*   Next;
    HMODULE                     Module;
    PMIDBG_FUNCTIONS            Functions;

} PMIDBG_EXTENSION;



//
// Local state bag for the PmiDbg extensions system.
//
static struct _PmiDbg
{
    PMIDBG_EXTENSION*           Loaded;             //Linked list of loaded extensions
    const PMIDBG_NOTIFICATION*  CurrentNotify;      //Pointer to the current notification
    BOOL                        UnloadCurrent;      //Bool set when extensions wants immediate unload.
    ULONG                       ThreadId;           //ThreadId of the thread executing the current notification
    PMIDBG_HOST_TYPE            HostType;           //The type of host this process is.

} PmiDbg = {0};



//
// Summary:
//  Function to send a notification to all loaded PmiDbg extensions.
//
// Parameters:
//  notify      - the specific notification to fire.
//  ...         - additional arguments that are specific to the notification.
//
void NotifyPmiDbgExtensions( PMIDBG_NOTIFICATION notify, ... )
{
    PMIDBG_EXTENSION* pCurrent;
    PMIDBG_EXTENSION* pPrev;
    va_list argptr;

    va_start(argptr, notify);


    //
    // As an extra security check, we make sure that callbacks only
    // occur from the same thread that is firing the notifications.
    // This prevents extensions from calling us from background
    // threads when we don't expect it.
    //
    PmiDbg.ThreadId = GetCurrentThreadId();

    PmiDbg.CurrentNotify = &notify;
    PmiDbg.UnloadCurrent = FALSE;

    pCurrent = PmiDbg.Loaded;
    pPrev    = NULL;

    while( pCurrent != NULL )
    {
        if( pCurrent->Functions.Notify != NULL )
        {
            pCurrent->Functions.Notify( notify.Type, (void*)argptr );

            //
            // If the extension calls the Unload handler and we are not in
            // a finalize event, then unload the extension.  We ignore the
            // finalize events, because the extension will be unloaded in
            // just a second anyways.
            //
            if( notify.Type != PMIDBG_NOTIFY_FINALIZE && PmiDbg.UnloadCurrent )
            {
                PMIDBG_EXTENSION* pNext;

                pCurrent->Functions.Notify( PMIDBG_NOTIFY_FINALIZE, NULL );

                if( pPrev != NULL )
                {
                    pPrev->Next = pCurrent->Next;
                }
                else
                {
                    PmiDbg.Loaded = pCurrent->Next;
                }
                pNext = pCurrent->Next;
                FreeLibrary(pCurrent->Module);
                free(pCurrent);
                pCurrent = pNext;
                PmiDbg.UnloadCurrent = FALSE;
                continue;
            };
        }

        pPrev    = pCurrent;
        pCurrent = pCurrent->Next;
    }
    PmiDbg.CurrentNotify = NULL;
}


//
// Summary:
//  Utility function to initialize the PMIDBG_SYSTEM_INFO structure.
//
inline bool InitializePmiDbgSystemInfo(
        __in PMIDBG_HOST_TYPE hostType,
        __out PMIDBG_SYSTEM_INFO* pInfo
        )
{
    pInfo->Version      = PMIDBG_VERSION;
    pInfo->Host         = hostType;
    pInfo->AppName      = smpd_process.appname;

    char* host = nullptr;
    if (MPIU_WideCharToMultiByte(smpd_process.host, &host) != NOERROR)
    {
        return false;
    }

    pInfo->LocalName    = host;
    pInfo->Control      = HandlePmiDbgControl;
    pInfo->Unload       = HandlePmiDbgUnload;
    return true;
}

//
// Summary:
//  Global function to load all extensions into the
//  current process.
//
// Parameters:
//  hostType        - the role of the current process.
//
// Remarks:
//  This function will enumerate all sub keys under
//    HKLM\Software\Microsoft\Mpi\PmiExtensions and
//    read the default value.  This must be the path
//    to the dll.  Any failures during initializing
//    an extension will cause it to be skipped.
//
void LoadPmiDbgExtensions( PMIDBG_HOST_TYPE hostType )
{
    DWORD                   ntError;
    HKEY                    hKey;
    HKEY                    hSubKey;
    DWORD                   nSubKey;
    DWORD                   iSubKey;
    PMIDBG_EXTENSION*       pItem;
    PMIDBG_EXTENSION*       pHead;

    PMIDBG_SYSTEM_INFO      systemInfo;
    PMIDBG_FUNCTIONS        callbacks;

    HMODULE                 hModule;
    FN_PmiDbgInitExtension* pfnInit;

    DWORD                   cchPath;
    char                    path[MAX_PATH];

    DWORD                   cchName;
    DWORD                   cchMaxName;
    char*                   pName;

    //collect up the system info
    if (!InitializePmiDbgSystemInfo(hostType, &systemInfo))
    {
        smpd_err_printf(L"failed to initialize DbgSystemInfo");
        return;
    }

    //
    // Open the base registry key
    //
    ntError = ::RegOpenKeyExA(
                    HKEY_LOCAL_MACHINE,
                    PMIDBG_REG_PATH_A,
                    0,
                    KEY_READ,
                    &hKey
                    );
    if( ntError != NOERROR )
    {
        return;
    }

    //
    // Get the count of sub keys
    //
    OACR_WARNING_SUPPRESS(USE_WIDE_API, "SMPD uses only ANSI character set.");
    ntError = RegQueryInfoKeyA(
        hKey,           //hKey
        NULL,           //lpClass
        NULL,           //lpcClass
        NULL,           //lpReserved
        &nSubKey,       //lpcSubKeys
        &cchMaxName,    //lpcMaxSubKeyLen
        NULL,           //lpcMaxClassLen
        NULL,           //lpcValues
        NULL,           //lpcMaxValueNameLen
        NULL,           //lpcMaxValueLen
        NULL,           //lpcbSecurityDescriptor
        NULL            //lpftLastWriteTime
        );
    if( ntError != NOERROR || nSubKey == 0)
    {
        RegCloseKey(hKey);
        return;
    }

    pName = static_cast<char*>( malloc( cchMaxName + sizeof('\0') ) );
    if( pName == NULL )
    {
        RegCloseKey(hKey);
        return;
    }

    //
    // Enumerate all of the sub keys and get the default
    //  value from the key.
    //
    pHead = NULL;
    for( iSubKey = 0; iSubKey < nSubKey; iSubKey++ )
    {
        //
        // Get the current subkey name.
        //
        cchName = cchMaxName + 1;
        OACR_WARNING_SUPPRESS(USE_WIDE_API, "SMPD uses only ANSI character set.");
        ntError = RegEnumKeyExA(
                            hKey,           //hKey
                            iSubKey,        //dwIndex
                            pName,           //lpName
                            &cchName,       //lpcName
                            NULL,           //lpReserved
                            NULL,           //lpClass
                            NULL,           //lpcClass
                            NULL            //lpftLastWriteTime
                            );
        if( ntError != NOERROR )
        {
            continue;
        }

        //
        // Open the subkey handle so we can pass it to the
        //   extensions init function.
        //
        OACR_WARNING_SUPPRESS(USE_WIDE_API, "SMPD uses only ANSI character set.");
        ntError = RegOpenKeyExA(
                            hKey,
                            pName,
                            0,
                            KEY_READ,
                            &hSubKey
                            );
        if( ntError != NOERROR )
        {
            continue;
        }

        //
        // There is no requirement that the person who wrote the string
        //  value to the registry included the null terminator, so we ensure
        //  that there is enough space for a final null if required.
        //
        cchPath = sizeof(path) - sizeof('\0');
        OACR_WARNING_SUPPRESS(USE_WIDE_API, "SMPD uses only ANSI character set.");
        ntError = RegGetValueA(
                            hSubKey,
                            NULL,
                            NULL,
                            RRF_RT_REG_SZ,
                            NULL,
                            path,
                            &cchPath
                            );
        if( ntError != NOERROR )
        {
            RegCloseKey( hSubKey );
            continue;
        }

        //
        // if the value was empty, just continue;
        //
        if( cchPath == 0 )
        {
            RegCloseKey( hSubKey );
            continue;
        }


        //
        // ensure we have a null terminated string.
        //
        path[cchPath] = 0;

        //
        // Should be __WARNING_BANNED_API_USE_CORESYSTEM, OACR doesn't like it
        //
        OACR_WARNING_SUPPRESS(28752, "SMPD uses only ANSI character set.");
        OACR_REVIEWED_CALL(
            mpicr,
            hModule = LoadLibraryExA( path, nullptr, 0 ) );
        if( hModule == NULL )
        {
            RegCloseKey( hSubKey );
            continue;
        }

        //
        // Lookup the init function
        //
        pfnInit = (FN_PmiDbgInitExtension*)GetProcAddress(
            hModule,
            PMIDBG_INIT_EXTENSION_FN_NAME );

        if( pfnInit == NULL )
        {
            FreeLibrary(hModule);
            RegCloseKey( hSubKey );
            continue;
        }

        pItem = static_cast<PMIDBG_EXTENSION*>( malloc( sizeof(*pItem) ) );
        if( pItem == NULL )
        {
            FreeLibrary(hModule);
            RegCloseKey( hSubKey );
            continue;
        }

        //
        // Call the init function.
        //
        if( pfnInit( hSubKey, &systemInfo, &callbacks ) == FALSE )
        {
            free(pItem);
            FreeLibrary(hModule);
            RegCloseKey( hSubKey );
            continue;
        }

        pItem->Functions        = callbacks;
        pItem->Module           = hModule;
        pItem->Next             = pHead;
        pHead                   = pItem;

        RegCloseKey(hSubKey);
        smpd_dbg_printf( L"successfully loaded and initialized the extension %S\n", path );
    }

    PmiDbg.ThreadId         = GetCurrentThreadId();
    PmiDbg.HostType         = hostType;
    PmiDbg.CurrentNotify    = NULL;
    PmiDbg.Loaded           = pHead;
    RegCloseKey(hKey);
    free(pName);
}



//
// Summary:
//  Function to unload all loaded PmiDbg Extensions
//
void UnloadPmiDbgExtensions()
{
    PMIDBG_EXTENSION* p;

    while( PmiDbg.Loaded )
    {
        p = PmiDbg.Loaded->Next;
        if( PmiDbg.Loaded->Module )
        {
            FreeLibrary(PmiDbg.Loaded->Module);
        }
        free(PmiDbg.Loaded);
        PmiDbg.Loaded = p;
    }
}



//
//
//
static HRESULT __stdcall
HandlePmiDbgUnload()
{
    //
    // There must be a notification currently running and that the current thread
    //  is the notificaiton thread.
    //
    if( PmiDbg.CurrentNotify == NULL || GetCurrentThreadId() != PmiDbg.ThreadId )
    {
        return E_FAIL;
    }

    //
    // Set the trigger to unload the current extension
    //  when the current notify call to that extension returns.
    //
    PmiDbg.UnloadCurrent = TRUE;
    return S_OK;
}


//
// Summary:
//  Entry Callback for dispatching PmiDbg query calls to the correct
//  notification handlers.
//
static HRESULT __stdcall
HandlePmiDbgControl(
    PMIDBG_OPCODE_TYPE     type,
    void*                  pData,
    void*                  pBuffer,
    SIZE_T                 cbBuffer )
{
    //
    // There must be a notification currently running and that the current thread
    //  is the notificaiton thread.
    //
    if( PmiDbg.CurrentNotify == NULL ||
        GetCurrentThreadId() != PmiDbg.ThreadId )
    {
        return E_FAIL;
    }

    //
    // All notifications must support this, so we implement handling
    //  this query here.
    //
    if( type == PMIDBG_OPCODE_GET_SYSTEM_INFO )
    {
        //
        // pBuffer must point to a valid PMIDBG_SYSTEM_INFO structure.
        //
        if( cbBuffer < sizeof(PMIDBG_SYSTEM_INFO) )
        {
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }

        //
        // initialize the output buffer
        //
        if (!InitializePmiDbgSystemInfo(PmiDbg.HostType,
            static_cast<PMIDBG_SYSTEM_INFO*>(pBuffer)))
        {
            return E_FAIL;
        }
        return S_OK;
    }

    if( type == PMIDBG_OPCODE_GET_JOB_CONTEXT )
    {
        if( cbBuffer < sizeof(char*) )
        {
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }

        *((char**)pBuffer) = smpd_process.job_context;
        return S_OK;
    }

    //
    // if the notification doesn't support extended query calls,
    //  we return an error.
    //
    if( PmiDbg.CurrentNotify->Control == NULL )
    {
        return E_FAIL;
    }

    //
    // pass the query call to the notification handler
    //
    return PmiDbg.CurrentNotify->Control( type, pData, pBuffer, cbBuffer );
}
