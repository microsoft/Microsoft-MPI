// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "mpimem.h"


__forceinline void* __cdecl operator new( size_t size )
{
    return MPIU_Malloc( size );
}


__forceinline void* __cdecl operator new( size_t /*size*/, _In_ void* pMem )
{
    return pMem;
}


__forceinline void* __cdecl operator new[]( size_t size )
{
    return MPIU_Malloc( size );
}


__forceinline void* __cdecl operator new[]( size_t /*size*/, _In_ void* pMem )
{
    return pMem;
}


__forceinline void __cdecl operator delete( _In_opt_ _Post_ptr_invalid_ void* pObj )
{
    MPIU_Free( pObj );
}


__forceinline void __cdecl operator delete( _In_opt_ void* /*pObj*/, _In_ void* /*pMem*/ )
{
}


__forceinline void __cdecl operator delete[]( _In_opt_ _Post_ptr_invalid_ void* pObj )
{
    MPIU_Free( pObj );
}


__forceinline void __cdecl operator delete[]( _In_opt_ void* /*pObj*/, _In_ void* /*pMem*/ )
{
}
