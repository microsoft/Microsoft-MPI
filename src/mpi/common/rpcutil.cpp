// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "rpc.h"
#include "rpcutil.h"
#include "ntdsapi.h"

#define SECURITY_WIN32
#include "security.h"

extern "C"
void __RPC_FAR *
__RPC_USER MIDL_user_allocate(
    size_t    cBytes
    )
{
    return malloc( cBytes );
}


extern "C"
void __RPC_USER
MIDL_user_free(
    void* pBuffer
    )
{
    free( pBuffer );
}


//
// Summary:
// Start the RPC server
//
// In:
// pProtSeq           : The protocol sequence
// pEndpoint          : The endpoint (port). If null, we use dynamic port
// rpcInterface       : The RPC interface handle
// pSecurityCallbackFn: The security callback function
// maxConcurrentCalls : Number of concurrent client calls that the server will accept
// localOnly          : If true, use only LRPC for the server
//
// Out:
// pPort              : The TCP port that the RPC server is accepting calls on
// pLrpcEndpoint      : The LRPC port that the RPC server is accepting calls on
//
// Return:
// NOERROR on success, other errors otherwise
//
_Success_(return == NOERROR)
RPC_STATUS
StartRpcServer(
    _In_z_     PCWSTR              pProtSeq,
    _In_opt_z_ PCWSTR              pEndpoint,
    _In_       RPC_IF_HANDLE       rpcInterface,
    _In_opt_   RPC_IF_CALLBACK_FN* pSecurityCallbackFn,
    _Out_opt_  UINT16*             pPort,
    _Out_opt_  GUID*               pLrpcEndpoint,
    _In_       UINT                maxConcurrentCalls,
    _In_       bool                localOnly
    )
{
    RPC_STATUS          status = RPC_S_OK;

    GUID lrpcEp = {0};
    if( pLrpcEndpoint != nullptr )
    {
        //
        // Enable listening on LRPC.
        // We generate a GUID to use as endpoint for LRPC.
        //
        status = UuidCreate( &lrpcEp );
        if( status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY )
        {
            return status;
        }

        wchar_t guidStr[37];
        GuidToStr( lrpcEp, guidStr, _countof(guidStr) );

        wchar_t protSeq[] = L"ncalrpc";
        status = RpcServerUseProtseqEpW(
            protSeq,
            RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
            guidStr,
            nullptr
            );
        if( status != RPC_S_OK )
        {
            return status;
        }
    }

    if( localOnly == false )
    {
        //
        // Dynamic and static endpoints are setup through different RPC APIs
        //
        if( pEndpoint == nullptr )
        {
            //
            // Specify that we will be using TCP socket with dynamic endpoint
            //
            status = RpcServerUseProtseqW(
                const_cast<wchar_t*>( pProtSeq ),
                RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                nullptr
                );
        }
        else
        {
            //
            // Specify that we will be using TCP socket with static endpoint
            //
            status = RpcServerUseProtseqEpW(
                const_cast<wchar_t*>( pProtSeq ),
                RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                const_cast<wchar_t*>( pEndpoint ),
                nullptr
                );
        }

        if( status != RPC_S_OK )
        {
            return status;
        }

        wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD   len = _countof(computerName);
        if( GetComputerNameW( computerName, &len ) == 0 )
        {
            return GetLastError();
        }

        wchar_t spn[MAX_PATH+1];
        len = _countof(spn);

        status = DsMakeSpnW(
            MSMPI_SPN_SERVICE_NAME,
            computerName,
            nullptr,
            0,
            nullptr,
            &len,
            spn );
        if( status != ERROR_SUCCESS )
        {
            return status;
        }

#if !defined(MSMPI_NO_SEC)
        status = RpcServerRegisterAuthInfoW( spn,
                                             RPC_C_AUTHN_GSS_NEGOTIATE,
                                             nullptr,
                                             nullptr );
        if( status != RPC_S_OK )
        {
            return status;
        }
#else
        pSecurityCallbackFn = nullptr;
#endif
    }
    else
    {
        ASSERT( pLrpcEndpoint != nullptr && pPort == nullptr );
        pSecurityCallbackFn = nullptr;
    }

    //
    // Register the interface and start the server
    //
    status = RpcServerRegisterIfEx( rpcInterface,
                                    nullptr,
                                    nullptr,
                                    RPC_IF_AUTOLISTEN,
                                    maxConcurrentCalls,
                                    pSecurityCallbackFn );
    if( status != RPC_S_OK )
    {
        return status;
    }

    if( pPort == nullptr )
    {
        if( pLrpcEndpoint != nullptr )
        {
            *pLrpcEndpoint = lrpcEp;
        }

        return NOERROR;
    }

    //
    // Extract the dynamic port that the server is using
    //
    RPC_BINDING_VECTOR* pBindingVector;

    //
    // Get the server binding handle vector. This vector has information
    // about the server bindings (which includes the port)
    //
    status = RpcServerInqBindings( &pBindingVector );
    if( status != RPC_S_OK )
    {
        return status;
    }

    wchar_t* bindingStr;
    wchar_t* endpointStr;
    wchar_t* protSeqStr;
    bool found = false;
    for( unsigned i = 0; i < pBindingVector->Count; ++i )
    {
        status = RpcBindingToStringBindingW(
            pBindingVector->BindingH[i],
            reinterpret_cast<RPC_WSTR*>( &bindingStr ) );

        if( status != RPC_S_OK )
        {
            return status;
        }

        //
        // Get the port
        //
        status = RpcStringBindingParseW(
            bindingStr,
            nullptr,
            &protSeqStr,
            nullptr,
            &endpointStr,
            nullptr
            );
        RpcStringFreeW( &bindingStr );
        if( status != RPC_S_OK )
        {
            return status;
        }

        if( CompareStringW( LOCALE_INVARIANT,
                            0,
                            protSeqStr,
                            -1,
                            L"ncacn_ip_tcp",
                            -1 ) == CSTR_EQUAL )
        {
            *pPort = static_cast<UINT16>(_wtoi( endpointStr ));
            found = true;

            if( env_is_on(L"MPIEXEC_USE_NP", FALSE) )
            {
                wchar_t npEndpoint[64];
                MPIU_Snprintf(
                    npEndpoint,
                    _countof( npEndpoint ),
                    L"\\pipe\\msmpi\\smpd\\%s",
                    endpointStr );

                status = RpcServerUseProtseqEpW(
                    reinterpret_cast<RPC_WSTR>( L"ncacn_np" ),
                    0,
                    reinterpret_cast<RPC_WSTR>( npEndpoint ),
                    NULL );
            }
            RpcStringFreeW( &endpointStr );
            RpcStringFreeW( &protSeqStr );
            break;
        }

        RpcStringFreeW( &endpointStr );
        RpcStringFreeW( &protSeqStr );
    }

    RpcBindingVectorFree( &pBindingVector );

    ASSERT( found == true );
    if( pLrpcEndpoint != nullptr )
    {
        *pLrpcEndpoint = lrpcEp;
    }

    if( status != RPC_S_OK )
    {
        return status;
    }
    return NOERROR;
}


//
// Summary:
// Stop this RPC server
//
// In:
// rpcInterface    : The RPC interface handle
//
RPC_STATUS
StopRpcServer(
    _In_       RPC_IF_HANDLE       rpcInterface
    )
{
    return RpcServerUnregisterIf( rpcInterface, nullptr, FALSE );
}


_Success_(return == NOERROR)
RPC_STATUS
CreateRpcBinding(
    _In_     PCWSTR                   pProtSeq,
    _In_opt_z_ PCWSTR                 pHostName,
    _In_     PCWSTR                   pEndpoint,
    _In_     UINT                     AuthnLevel,
    _In_     UINT                     AuthnSvc,
    _In_opt_ RPC_AUTH_IDENTITY_HANDLE pAuthIdentity,
    _Out_    handle_t*                phBinding
    )
{
    PWSTR  bindingStr;
    RPC_STATUS status = RpcStringBindingComposeW(
        nullptr,
        const_cast<wchar_t*>(pProtSeq),
        const_cast<wchar_t*>(pHostName),
        const_cast<wchar_t*>(pEndpoint),
        nullptr,
        &bindingStr
        );
    if( status != RPC_S_OK )
    {
        return status;
    }

    handle_t hBinding;
    status = RpcBindingFromStringBindingW(
        bindingStr,
        &hBinding
        );
    RpcStringFreeW( &bindingStr );
    if( status != RPC_S_OK )
    {
        return status;
    }

#if !defined(MSMPI_NO_SEC)
    wchar_t* pSpn = nullptr;
    wchar_t spn[MAX_PATH+1];

    SEC_WINNT_AUTH_IDENTITY_EXW secAuth;
    if( AuthnSvc == RPC_C_AUTHN_GSS_NEGOTIATE &&
        pAuthIdentity == nullptr )
    {
        DWORD len = _countof(spn);
        status = DsMakeSpnW(
            MSMPI_SPN_SERVICE_NAME,
            pHostName,
            nullptr,
            0,
            nullptr,
            &len,
            spn );
        if( status != ERROR_SUCCESS )
        {
            if( status == ERROR_INVALID_PARAMETER )
            {
                //
                // This should only happen because the host is an IP
                // address and not a proper name.  Kerberos requires
                // names which means we will have to disable Kerberos
                // to authenticate.
                //
                InitializeAuthIdentity(
                    disableKerbStr,
                    _countof(DISABLE_KERB_STR) - 1,
                    &secAuth );
                pAuthIdentity = &secAuth;
            }
            else
            {
                RpcBindingFree( &hBinding );
                return status;
            }
        }
        else
        {
            pSpn = spn;
        }
    }

    status = RpcBindingSetAuthInfoW(
        hBinding,
        pSpn,
        AuthnLevel,
        AuthnSvc,
        pAuthIdentity,
        0 );
    if( status != RPC_S_OK )
    {
        RpcBindingFree( &hBinding );
        return status;
    }
#else
    UNREFERENCED_PARAMETER(AuthnLevel);
    UNREFERENCED_PARAMETER(AuthnSvc);
    UNREFERENCED_PARAMETER(pAuthIdentity);
#endif

    *phBinding = hBinding;
    return NOERROR;
}


void
InitializeAuthIdentity(
    _In_ PWSTR packageStr,
    _In_ DWORD packageLen,
    _Out_ SEC_WINNT_AUTH_IDENTITY_EXW* pSecAuth
    )
{
    ZeroMemory( pSecAuth, sizeof(*pSecAuth) );
    pSecAuth->Version = SEC_WINNT_AUTH_IDENTITY_VERSION;
    pSecAuth->Length = sizeof(*pSecAuth);
    pSecAuth->PackageList = packageStr;
    pSecAuth->PackageListLength = packageLen;
    pSecAuth->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
}
