// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "shlobj.h"
#include "Shlwapi.h"
#include "mpistr.h"

//
// A global shared memory for IPC. Allows ipc among different users on the same computer, 
// with no specific previlige like SeCrateGlobalPrevilige. Processes communicate over
// a temporary, i.e no disk flush, delete-on-close file mapped view.
//

class GlobalShmRegion
{
private:
    BOOL        isReadOnly;
    HANDLE      file;
    HANDLE      fileMap;
    void*       pView;
    WCHAR       fullRegionName[MAX_PATH];

    //
    // Non-copyable
    //
    GlobalShmRegion(const GlobalShmRegion&);
    GlobalShmRegion& operator = (const GlobalShmRegion&);

    HRESULT CreateFullRegionName(_In_z_ PCWSTR regionName)
    {
        //
        // The file system directory that contains application data for all users
        //
        HRESULT result = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, fullRegionName);
        if(result != S_OK)
        {
            return result;
        }

        //
        // SHGetFolderPathW returns full path without a trailing backslash.
        //
        size_t fullPathLen = MPIU_Strlen(fullRegionName) + 1 + MPIU_Strlen(regionName);
        if (fullPathLen >= _countof(fullRegionName))
        {
            return E_FAIL;
        }

        OACR_WARNING_SUPPRESS(UNSAFE_STRING_FUNCTION, "Buffer length is already checked.");
        BOOL appended = PathAppendW(fullRegionName, regionName);
        return appended ? S_OK : E_FAIL;
    }


public:
    GlobalShmRegion():
        isReadOnly(FALSE),
        file(INVALID_HANDLE_VALUE),
        fileMap(nullptr),
        pView(nullptr)
    {
    }


    ~GlobalShmRegion()
    {
        if(pView != nullptr)
        {
            UnmapViewOfFile(pView);
        }
        if(fileMap != nullptr)
        {
            CloseHandle(fileMap);
        }
        if(file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file);
        }
    }


    DWORD Open(_In_z_ PCWSTR regionName)
    {
        if(pView != nullptr)
        {
            return ERROR_ALREADY_EXISTS;
        }

        isReadOnly = TRUE;

        HRESULT result = CreateFullRegionName(regionName);
        if(result != S_OK)
        {
            return ERROR_INVALID_NAME;
        }

        file = CreateFileW(
            fullRegionName,
            GENERIC_READ,
            FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_TEMPORARY,
            nullptr
            );

        if(file == INVALID_HANDLE_VALUE)
        {
            return GetLastError();
        }

        fileMap = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);

        if(fileMap ==nullptr)
        {
            CloseHandle(file);
            file = nullptr;
            return GetLastError();
        }

        pView = MapViewOfFile(
            fileMap,
            FILE_MAP_READ,
            0,
            0,
            0
            );
        if( pView == nullptr )
        {
            CloseHandle(file);
            CloseHandle(fileMap);
            file = nullptr;
            fileMap = nullptr;
            return GetLastError();
        }

        return NO_ERROR;
    }


    DWORD Create(_In_z_ PCWSTR regionName, UINT32 regionSize)
    {
        if(pView != nullptr)
        {
            return ERROR_ALREADY_EXISTS;
        }

        isReadOnly = FALSE;

        HRESULT result = CreateFullRegionName(regionName);
        if(result != S_OK)
        {
            return ERROR_INVALID_NAME;
        }

        SECURITY_ATTRIBUTES security;
        ZeroMemory(&security, sizeof(security));
        security.nLength = sizeof(security);
        ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;;GA;;;WD)",
            SDDL_REVISION_1,
            &security.lpSecurityDescriptor,
            NULL);

        file = CreateFileW(
            fullRegionName,
            GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ,
            &security,
            CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
            NULL
            );

        LocalFree(security.lpSecurityDescriptor);

        if(file == INVALID_HANDLE_VALUE)
        {
            return GetLastError();
        }

        fileMap = CreateFileMappingW(
            file,
            nullptr,
            PAGE_READWRITE,
            0,
            regionSize,
            nullptr
            );

        if( fileMap == nullptr )
        {
            CloseHandle(file);
            file = nullptr;
            return GetLastError();
        }

        pView = MapViewOfFile(
            fileMap,
            FILE_MAP_WRITE,
            0,
            0,
            0
            );

        if(pView != nullptr)
        {
            ZeroMemory(pView, regionSize);
            return NO_ERROR;
        }

        CloseHandle(file);
        CloseHandle(fileMap);
        file = nullptr;
        fileMap = nullptr;
        return GetLastError();
    }


    const void* ReadPtr() const
    {
        return pView;
    }


    void* WritePtr()
    {
        if(isReadOnly)
        {
            return nullptr;
        }

        return pView;
    }

};