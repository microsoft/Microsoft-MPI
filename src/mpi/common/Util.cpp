// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "util.h"
#include "kernel32util.h"

//
// Summary:
//  Ensure that the OS version is greater than or equal to the specified version.
//
// Parameters:
//  major           - Windows major version
//  minor           - Windows minor version
//
_Success_(return!=FALSE)
BOOL
CheckOSVersion(
    _In_ DWORD major,
    _In_ DWORD minor
    )
{
    OSVERSIONINFOEX version         = {};
    ULONG           mask            = VER_MAJORVERSION | VER_MINORVERSION;
    ULONGLONG       conditions      = 0;

    version.dwOSVersionInfoSize  = sizeof(version);
    version.dwMajorVersion  = major;
    version.dwMinorVersion  = minor;

    conditions = ::VerSetConditionMask( conditions, VER_MAJORVERSION, VER_GREATER_EQUAL );
    conditions = ::VerSetConditionMask( conditions, VER_MINORVERSION, VER_GREATER_EQUAL );

    return ::VerifyVersionInfo( &version, mask, conditions  );
}


static const wchar_t * const AZURE_REGISTRY_VALUE  = L"NodeLogicalName";
static const wchar_t * const AZURE_REGISTRY_KEY = L"SOFTWARE\\MICROSOFT\\HPC";

//
// Summary:
// Check if the smpd instance is running on azure and if so,
// return the logical name of the node
//
// Input:
// szBuffer: the size of the name buffer
//
// Output:
// buffer  : store the logical name. If null, name is not returned
//
// Return:
// true  if the node is on azure
// false if the node is not on azure, or if the size of the buffer is
//       too small
//
//
bool get_azure_node_logical_name(
    _Out_opt_z_cap_(szBuffer) wchar_t*  buffer,
    _In_                      DWORD  szBuffer )
{
    HKEY  key;
    DWORD size   = szBuffer - 1;
    DWORD status = RegOpenKeyExW( HKEY_LOCAL_MACHINE,
                                  AZURE_REGISTRY_KEY,
                                  NULL,
                                  KEY_READ,
                                  &key );

    if( status != ERROR_SUCCESS )
    {
        return false;
    }

    status = RegQueryValueExW( key,
                               AZURE_REGISTRY_VALUE,
                               NULL,
                               NULL,
                               reinterpret_cast<BYTE*>(buffer),
                               &size );

    RegCloseKey( key );

    if( status != ERROR_SUCCESS )
    {
        return false;
    }

    if( buffer != NULL )
    {
        buffer[size] = L'\0';
    }

    return true;
}