// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.


#include "precomp.h"
#include "winsock2.h"

BOOL
env_is_on_ex(
    _In_z_ const wchar_t* name,
    _In_opt_z_ const wchar_t* deprecatedName,
    _In_ BOOL defval
    )
{
    wchar_t env[5];

    DWORD err = MPIU_Getenv( name, env, _countof(env) );
    if( err == ERROR_ENVVAR_NOT_FOUND && deprecatedName != nullptr )
    {
        err = MPIU_Getenv( deprecatedName, env, _countof(env) );
    }

    if( err != NOERROR )
    {
        return defval;
    }

    if( CompareStringW( LOCALE_INVARIANT,
                        0,
                        env,
                        -1,
                        L"1",
                        -1 ) == CSTR_EQUAL )
    {
        return TRUE;
    }

    if( CompareStringW( LOCALE_INVARIANT,
                        NORM_IGNORECASE,
                        env,
                        -1,
                        L"on",
                        -1 ) == CSTR_EQUAL ||
        CompareStringW( LOCALE_INVARIANT,
                        NORM_IGNORECASE,
                        env,
                        -1,
                        L"yes",
                        -1 ) == CSTR_EQUAL ||
        CompareStringW( LOCALE_INVARIANT,
                        NORM_IGNORECASE,
                        env,
                        -1,
                        L"true",
                        -1 ) == CSTR_EQUAL )
    {
        return TRUE;
    }

    return FALSE;
}


int
env_to_int_ex(
    _In_z_ const wchar_t* name,
    _In_opt_z_ const wchar_t* deprecatedName,
    _In_ int defval,
    _In_ int minval
    )
{
    wchar_t val[12];
    DWORD err = MPIU_Getenv( name, val, _countof(val) );
    if( err == ERROR_ENVVAR_NOT_FOUND && deprecatedName != nullptr )
    {
        err = MPIU_Getenv( deprecatedName, val, _countof(val) );
    }

    if( err != NOERROR )
    {
        return defval;
    }

    defval = _wtoi(val);
    if(defval < minval)
    {
        return minval;
    }

    return defval;
}


_Success_(return == TRUE)
BOOL
env_to_range_ex(
    _In_z_ const wchar_t* name,
    _In_opt_z_ const wchar_t* deprecatedName,
    _In_ int minval,
    _In_ int maxval,
    _In_ bool allowSingleValue,
    _Out_ int* low,
    _Out_ int* high
    )
{
    //
    // We need a string big enough to be able to store
    // -INT_MAX : INT_MAX. 64 should be plenty
    //
    wchar_t range[64];
    wchar_t* next_token = nullptr;
    DWORD err = MPIU_Getenv( name, range, _countof(range) );
    if( err == ERROR_ENVVAR_NOT_FOUND && deprecatedName != nullptr )
    {
        err = MPIU_Getenv( deprecatedName, range, _countof(range) );
    }
    
    if( err != NOERROR )
    {
        return FALSE;
    }

    const wchar_t* pCur;
    
    //
    // tokenize min,max OR min:max OR min..max
    //
    pCur = wcstok_s( range, L",.:", &next_token );
    if( pCur == nullptr )
    {
        return FALSE;
    }

    int tmpLow = _wtoi( pCur );
    if( tmpLow < minval )
    {
        tmpLow = minval;
    }

    int tmpHigh;
    pCur = wcstok_s( nullptr, L",.:", &next_token );
    if( pCur != nullptr )
    {
        tmpHigh = _wtoi( pCur );
        if( tmpHigh > maxval )
        {
            tmpHigh = maxval;
        }
    }
    else if( allowSingleValue )
    {
        tmpHigh = tmpLow;
    }
    else
    {
        return FALSE;
    }

    if( tmpHigh < tmpLow )
    {
        return FALSE;
    }

    *low = tmpLow;
    *high = tmpHigh;
    return TRUE;
}
 

#define MAX_TCP_PORT 65535

int
FindNextOpenPort(
    int startPort
    )
{
    if( startPort > MAX_TCP_PORT )
    {
        return 0;
    }

    WSADATA wsaData;
    int ret = WSAStartup( MAKEWORD(2,0), &wsaData );
    if( ret != 0 )
    {
        return 0;
    }

    SOCKET      server;
    SOCKADDR_IN sockAddr;
    int         port = startPort;

    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = INADDR_ANY;

    server = socket( AF_INET, SOCK_STREAM, 0 );
    if( server == INVALID_SOCKET )
    {
        return 0;
    }

    for( ;; )
    {
        sockAddr.sin_port = htons( static_cast<unsigned short>( port ) );
        if( bind( server,
                  reinterpret_cast<const SOCKADDR*>( &sockAddr ),
                  sizeof( sockAddr ) ) == SOCKET_ERROR )
        {
            ++port;
            if( port > MAX_TCP_PORT )
            {
                return 0;
            }
        }
        else
        {
            //
            // Found an open port
            //
            break;
        }
    }

    closesocket( server );
    WSACleanup();
    return port;
}
