// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

slist.h - Simple thread-safe singly linked list.  Lighter weight than
  OS provided SLIST, as it doesn't attempt to provide a useless depth.

--*/
#include<oacr.h>

#pragma once

template<class T>
class SListTraits
{
public:
    static T* GetNext( T* pT ){ return pT->m_pNext; }
    static void SetNext( T* pT, T* pNext ){ pT->m_pNext = pNext; }
};

template<class T, class TraitsT = SListTraits<T>>
class SList
{
    T* volatile m_pHead;
public:
    typedef typename TraitsT Traits;

private:
    SList( const SList<T>& list );
    SList& operator = ( const SList<T>& rhs );

public:
    SList() : m_pHead( NULL ){}
#if DBG
    ~SList(){ MPIU_Assert( m_pHead == NULL ); }
#endif
private:
    inline void Link( T* pFirst, T* pLast )
    {
        MPIU_Assert( pFirst != NULL );
        MPIU_Assert( pLast != NULL );

        T* pOldHead;
        T* pHead;
        for( ;; )
        {
            pHead = const_cast<T*>(m_pHead);
            Traits::SetNext( pLast, pHead );

            pOldHead = static_cast<T*>(
                InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile *>(&m_pHead),
                    pFirst,
                    pHead
                    )
                );

            if( pOldHead == pHead )
            {
                break;
            }
        }
    }

public:
    inline void Push( T* pT )
    {
        MPIU_Assert( pT != NULL );
        Link( pT, pT );
    }

    inline void PushList( T* pFirst, T* pLast )
    {
        MPIU_Assert( pFirst != NULL );
        MPIU_Assert( pLast != NULL );
#if DBG
        T* p = pFirst;
        while( p != pLast )
        {
            p = Traits::GetNext( p );
            MPIU_Assert( p != NULL );
        }
#endif

        Link( pFirst, pLast );
    }

    inline T* Pop()
    {
        T* pHead;
        T* pNext;
        for( ;; )
        {
            pHead = m_pHead;
            if( pHead == NULL )
            {
                return NULL;
            }

            __try
            {
                pNext = Traits::GetNext( pHead );
            }
            __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
               EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH )
            {
                continue;
            }

            pNext = static_cast<T*>(
                InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile *>(&m_pHead),
                    pNext,
                    pHead
                    )
                );
            if( pNext == pHead )
            {
                break;
            }
        }

        Traits::SetNext( pHead, NULL );
        return pHead;
    }

    T* Flush()
    {
        T* pHead = static_cast<T*>(
            InterlockedExchangePointer(
                reinterpret_cast<void* volatile *>(&m_pHead),
                NULL
                )
            );
        return pHead;
    }
};
