// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "DynProc.h"
#include "DynProcAlloc.h"


CONN_INFO_TYPE* 
ConnInfoAlloc(
   size_t count
   )
{
    return static_cast<CONN_INFO_TYPE*>(
        midl_user_allocate( count * sizeof( CONN_INFO_TYPE ) ) );
}


void
ConnInfoFree(
    _In_opt_ CONN_INFO_TYPE* ppConn
    )
{
    midl_user_free( ppConn );
}
