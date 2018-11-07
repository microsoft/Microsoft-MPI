// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"


/*
 * This file contains "safe" versions of the various string and printf
 * operations.
 */

_Success_(return >= 0 && return <= cchDest)
int
MPIU_Snprintf(
    _Null_terminated_ _Out_writes_to_(cchDest, return) char* dest,
    _In_ size_t cchDest,
    _Printf_format_string_ const char* format,
    ...
    )
{
    size_t len;
    va_list args;
    va_start( args, format );

    OACR_WARNING_SUPPRESS(USE_WIDE_API, "SMPD uses only ANSI character set.");
    HRESULT hr = StringCchVPrintfExA(
        dest,
        cchDest,
        nullptr,
        &len,
        0,
        format,
        args
        );

    va_end( args );

    if( FAILED( hr ) )
    {
        return 0;
    }

    return static_cast<int>(cchDest - len);
}


//
// Overloaded function for wide characters
//
_Success_(return >= 0 && return <= cchDest)
int
MPIU_Snprintf(
    _Null_terminated_ _Out_writes_to_(cchDest, return) wchar_t* dest,
    _In_ size_t cchDest,
    _Printf_format_string_ const wchar_t* format,
    ...
    )
{
    size_t len;
    va_list args;
    va_start( args, format );

    HRESULT hr = StringCchVPrintfExW(
        dest,
        cchDest,
        nullptr,
        &len,
        0,
        format,
        args
        );

    va_end( args );

    if( FAILED( hr ) )
    {
        return 0;
    }

    return static_cast<int>(cchDest - len);
}


_Success_(return >= 0 && return <= cchDest)
int
MPIU_Vsnprintf(
    _Null_terminated_ _Out_writes_to_(cchDest, return) char*       dest,
    _In_                 size_t      cchDest,
    _Printf_format_string_ const char* format,
    _In_                 va_list     args
    )
{
    size_t len;
    OACR_WARNING_SUPPRESS(USE_WIDE_API, "MSMPI uses only ANSI character set.");
    HRESULT hr = StringCchVPrintfExA(
        dest,
        cchDest,
        nullptr,
        &len,
        0,
        format,
        args
        );

    if( FAILED( hr ) )
    {
        return 0;
    }

    return static_cast<int>(cchDest - len);
}


//
// Overloaded function for wide characters
//
_Success_(return >= 0 && return <= cchDest)
int
MPIU_Vsnprintf(
    _Null_terminated_ _Out_writes_to_(cchDest, return) wchar_t*       dest,
    _In_                 size_t         cchDest,
    _Printf_format_string_ const wchar_t* format,
    _In_                 va_list        args
    )
{
    size_t len;
    HRESULT hr = StringCchVPrintfExW(
        dest,
        cchDest,
        nullptr,
        &len,
        0,
        format,
        args
        );

    if( FAILED( hr ) )
    {
        return 0;
    }

    return static_cast<int>(cchDest - len);
}


_Success_(return == 0)
int
MPIU_Strcpy(
    _Out_writes_z_(cchDest) char*    dest,
    _In_                 size_t      cchDest,
    _In_z_               const char* src )
{
    OACR_WARNING_SUPPRESS(USE_WIDE_API, "MSMPI uses only ANSI character set.");
    HRESULT hr = StringCchCopyA( dest, cchDest, src );

    if( FAILED( hr ) )
    {
        return static_cast<int>( hr );
    }

    return 0;
}


_Success_(return == 0)
int
MPIU_Strcpy(
    _Out_writes_z_(cchDest) wchar_t*    dest,
    _In_                 size_t      cchDest,
    _In_z_               const wchar_t* src )
{
    HRESULT hr = StringCchCopyW( dest, cchDest, src );

    if( FAILED( hr ) )
    {
        return static_cast<int>( hr );
    }

    return 0;
}


void
MPIU_Strnapp(
    _Out_writes_z_(n)     char*       dst,
    _In_z_                const char* src,
    _In_                  size_t      n )
{
    OACR_WARNING_SUPPRESS(USE_WIDE_API, "MSMPI uses only ANSI character set.");
    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignoring return value.");
    StringCchCatA( dst, n, src );
}


void
MPIU_Strnapp(
    _Out_writes_z_(n)     wchar_t*       dst,
    _In_z_                const wchar_t* src,
    _In_                  size_t      n)
{
    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Ignoring return value.");
    StringCchCatW(dst, n, src);
}


size_t
MPIU_Strlen(
    _In_ PCSTR  src,
    _In_ size_t cchMax
    )
{
    size_t len;
    OACR_WARNING_SUPPRESS(USE_WIDE_API, "MSMPI uses only ANSI character set.");
    HRESULT hr = StringCchLengthA( src, cchMax, &len );
    if( FAILED( hr ) )
    {
        return SIZE_MAX;
    }

    return len;
}


size_t
MPIU_Strlen(
    _In_ PCWSTR  src,
    _In_ size_t  cchMax
    )
{
    size_t len;
    HRESULT hr = StringCchLengthW( src, cchMax, &len );
    if( FAILED( hr ) )
    {
        return SIZE_MAX;
    }

    return len;
}


_Ret_valid_ _Null_terminated_
_Success_(return != nullptr)
wchar_t*
MPIU_Strdup(
    _In_z_ const wchar_t* str
    )
{
    size_t maxlen = MPIU_Strlen( str );
    if( maxlen == SIZE_MAX )
    {
        return nullptr;
    }

    //
    // Need one extra for the null terminating character
    //
    maxlen++;
    wchar_t* s = static_cast<wchar_t*>( MPIU_Malloc( sizeof(wchar_t) * maxlen ) );
    if( s != nullptr )
    {
        CopyMemory( s, str, maxlen * sizeof(wchar_t) );
    }
    return s;
}


_Ret_valid_ _Null_terminated_
_Success_(return != nullptr)
char*
MPIU_Strdup(
    _In_z_ const char* str
    )
{
    size_t maxlen = MPIU_Strlen( str );
    if( maxlen == SIZE_MAX )
    {
        return nullptr;
    }

    //
    // Need one extra for the null terminating character
    //
    maxlen++;
    char* s = static_cast<char*>( MPIU_Malloc( sizeof(char) * maxlen ) );
    if( s != nullptr )
    {
        CopyMemory( s, str, maxlen );
    }
    return s;
}


_Success_(return==NO_ERROR)
DWORD
MPIU_Getenv(
    _In_z_                 PCSTR name,
    _Out_writes_z_(cchBuffer) PSTR  buffer,
    _In_                   DWORD cchBuffer
    )
{
    OACR_WARNING_SUPPRESS( USE_WIDE_API, "MS MPI uses ANSI char set" );
    DWORD cchRet = GetEnvironmentVariableA( name, buffer, cchBuffer );
    if( cchRet == 0 )
    {
        //
        // There can be errors other than ERROR_ENVVAR_NOT_FOUND.
        // We treat them all as if the env var does not exist.
        //
        return ERROR_ENVVAR_NOT_FOUND;
    }
    else if( cchRet >= cchBuffer )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    return NO_ERROR;
}


_Success_(return==NO_ERROR)
DWORD
MPIU_Getenv(
    _In_z_                 PCWSTR name,
    _Out_writes_z_(cchBuffer) PWSTR  buffer,
    _In_                   DWORD  cchBuffer
    )
{
    DWORD cchRet = GetEnvironmentVariableW( name, buffer, cchBuffer );
    if( cchRet == 0 )
    {
        //
        // There can be errors other than ERROR_ENVVAR_NOT_FOUND.
        // We treat them all as if the env var does not exist.
        //
        return ERROR_ENVVAR_NOT_FOUND;
    }
    else if( cchRet >= cchBuffer )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    return NOERROR;
}


//
// Callee will need to call delete[] to free the memory allocated
// for wname_ptr if the function succeeds.
//
_Success_(return == NOERROR)
DWORD
MPIU_MultiByteToWideChar(
    _In_z_ const char* name,
    _Outptr_result_z_ wchar_t** wname_ptr
    )
{
    int len = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        name,
        -1,
        NULL,
        0
        );
    if( len == 0 )
    {
        return GetLastError();
    }

    wchar_t* wname = new wchar_t[len];
    if( wname == NULL )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

   len = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        name,
        -1,
        wname,
        len
        );
    if( len == 0 )
    {
        delete[] wname;
        return GetLastError();
    }

    *wname_ptr = wname;
    return NOERROR;
}


//
// Callee will need to call delete[] to free the memory allocated
// for outputStr if the function succeeds.
//
_Success_(return == NOERROR)
DWORD
MPIU_WideCharToMultiByte(
    _In_z_ const wchar_t* str,
    _Outptr_result_z_ char** outputStr
    )
{
    int len = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        str,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
        );
    if( len == 0 )
    {
        return GetLastError();
    }

    char* tmpStr = new char[len];
    if( tmpStr == nullptr )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

   len = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        str,
        -1,
        tmpStr,
        len,
        nullptr,
        nullptr
        );
    if( len == 0 )
    {
        delete[] tmpStr;
        return GetLastError();
    }

    *outputStr = tmpStr;
    return NOERROR;
}
