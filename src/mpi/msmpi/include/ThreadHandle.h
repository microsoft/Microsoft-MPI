// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "tls.h"


class ThreadHandle
{
    HANDLE m_hThread;
    volatile LONG m_nRef;

public:
    ThreadHandle()
        : m_hThread( NULL )
        , m_nRef( 1 )
    {
    }

    ~ThreadHandle()
    {
        if( m_hThread != NULL)
        {
            MPIU_Assert( m_nRef == 0 );
            ::CloseHandle( m_hThread );
        }
    }

    inline void AddRef()
    {
        MPIU_Assert( m_hThread != NULL );
        InterlockedIncrement( &m_nRef );
    }

    inline LONG Release()
    {
        MPIU_Assert( m_nRef > 0 );
        MPIU_Assert( m_hThread != NULL );
        LONG nRef = InterlockedDecrement( &m_nRef );
        if( nRef == 0 )
        {
            delete this;
        }
        return nRef;
    }

    inline int Initialize()
    {
        MPIU_Assert( m_hThread == NULL );

        BOOL ret = ::DuplicateHandle(
            GetCurrentProcess(),
            GetCurrentThread(),
            GetCurrentProcess(),
            &m_hThread,
            THREAD_SET_CONTEXT,
            FALSE,
            0
            );

        if( ret == FALSE )
        {
            return MPIU_ERR_FATAL_GET(MPI_SUCCESS, MPI_ERR_OTHER, "**duphandle %d", ::GetLastError());
        }

        return MPI_SUCCESS;
    }

    inline void QueueApc( PAPCFUNC pfnApc, ULONG_PTR param ) const
    {
        MPIU_Assert( m_hThread != NULL );
        MPIU_Assert( m_nRef > 0 );

        ::QueueUserAPC( pfnApc, m_hThread, param );
    }
};
