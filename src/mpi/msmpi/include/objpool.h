// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

objpool.h - Templates for object pool that manages allocation and handles.

--*/
#include "MpiLock.h"
#ifndef _OBJPOOL_
#define _OBJPOOL_


template<typename H> class ObjectTraits
{
public:
    typedef typename H HandleType;
};


template<typename PoolType, typename T, typename TraitsT> class ObjectPool
{
private:
    typedef typename TraitsT::HandleType HandleType;

private:
    T *m_freeList;
    T **m_indirect;
    T m_direct[TraitsT::DIRECT];
    T m_builtin[TraitsT::BUILTIN];
    int m_indirectSize;   /* Number of allocated indirect blocks */
    bool m_initialized;

    MPI_RWLOCK m_lock;

#if DBG
    // Number of objects out of the pool.
    int m_numAlloc;
    // Number of objects out of the pool owned by the user.
    int m_numUserAlloc;
#endif

    static PoolType s_instance;

protected:
    ObjectPool() :
#if DBG
       m_numAlloc(0),
       m_numUserAlloc(0),
#endif
       m_freeList(NULL),
       m_indirect(NULL),
       m_indirectSize(0),
       m_initialized(false),
       m_lock(MPI_RWLOCK_INIT)
    {
    }


    ~ObjectPool()
    {
        MPIU_Assert( m_indirect == nullptr );
    }


public:
    static void Cleanup()
    {
        if( s_instance.m_indirect == nullptr )
        {
            return;
        }

        for( int i = 0; i < s_instance.m_indirectSize; i++ )
        {
            delete[] s_instance.m_indirect[i];
        }

        delete[] s_instance.m_indirect;
        s_instance.m_indirect = nullptr;
    }


private:
    static void InitEntry(
        _Out_ T* entry,
        _In_range_(HANDLE_TYPE_INVALID, HANDLE_TYPE_INDIRECT) int type,
        _In_range_(0, HANDLE_BLOCK_INDEX_SIZE) int iIndirect,
        _In_range_(>=, 0) int index,
        _Pre_maybenull_ T* next
        )
    {
        entry->handle =
            (type << HANDLE_TYPE_SHIFT) |
            (TraitsT::KIND << HANDLE_MPI_KIND_SHIFT) |
            (iIndirect << HANDLE_INDIRECT_SHIFT) |
            index;
        entry->ref_count = 0;
        entry->next = next;
    }


    static void InitSlab(
        _Out_writes_(nElements) T slab[],
        _In_range_(1, INT_MAX) int nElements,
        _In_range_(HANDLE_TYPE_INVALID, HANDLE_TYPE_INDIRECT) int type,
        _In_range_(0, HANDLE_BLOCK_INDEX_SIZE) int iIndirect = 0 )
    {
        int i;
        for( i = 0; i < nElements - 1; i++ )
        {
            InitEntry( &slab[i], type, iIndirect, i, &slab[i+1] );
        }

        InitEntry( &slab[i], type, iIndirect, i, NULL );
    }


    T* GrowIndirect()
    {
        if( m_indirect == NULL )
        {
            m_indirect = new T*[HANDLE_BLOCK_INDEX_SIZE];
            if( m_indirect == NULL )
            {
                return NULL;
            }
        }
        if( m_indirectSize == HANDLE_BLOCK_INDEX_SIZE )
        {
            return NULL;
        }
        T* slab = new T[HANDLE_BLOCK_SIZE];
        if( slab == NULL )
        {
            return NULL;
        }
        InitSlab(
            slab,
            HANDLE_BLOCK_SIZE,
            HANDLE_TYPE_INDIRECT,
            m_indirectSize
            );
        m_indirect[m_indirectSize] = slab;
        m_indirectSize++;

        return slab;
    }


    T* Grow()
    {
        if( m_initialized == false )
        {
            InitSlab( m_direct, _countof(m_direct), HANDLE_TYPE_DIRECT );
            m_initialized = true;
            return m_direct;
        }

        MPIU_Assert( m_freeList == NULL );
        return GrowIndirect();
    }


public:
    _Check_return_ static __forceinline T* Alloc()
    {

        MpiRwLockAcquireExclusive(&s_instance.m_lock);

        T* t = s_instance.m_freeList;

        if( t == NULL )
        {
            t = s_instance.Grow();
            if( t == NULL )
            {
                goto fn_exit;
            }
        }

#if DBG
        s_instance.m_numAlloc++;
#endif
        s_instance.m_freeList = t->next;
        t->ref_count = 1;

fn_exit:
        MpiRwLockReleaseExclusive(&s_instance.m_lock);
        return t;
    }


    static __forceinline void Free( _In_ T* t )
    {
        MpiRwLockAcquireExclusive(&s_instance.m_lock);

        t->next = s_instance.m_freeList;
        s_instance.m_freeList = t;
#if DBG
        s_instance.m_numAlloc--;
#endif

        MpiRwLockReleaseExclusive(&s_instance.m_lock);
    }

    _Check_return_ static __forceinline T* Lookup( HandleType h )
    {
        if( HANDLE_GET_MPI_KIND(h) != TraitsT::KIND )
        {
            return NULL;
        }

        MpiRwLockAcquireShared(&s_instance.m_lock);

        T* t = nullptr;
        switch( HANDLE_GET_TYPE(h) )
        {
        case HANDLE_TYPE_BUILTIN:
            if( (HANDLE_DIRECT_INDEX(h) & TraitsT::BUILTINMASK) < TraitsT::BUILTIN )
            {
                t = &s_instance.m_builtin[HANDLE_DIRECT_INDEX(h) & TraitsT::BUILTINMASK];
            }
            break;

        case HANDLE_TYPE_DIRECT:
            if( HANDLE_DIRECT_INDEX(h) < TraitsT::DIRECT )
            {
                t = &s_instance.m_direct[HANDLE_DIRECT_INDEX(h)];
            }

            break;

        case HANDLE_TYPE_INDIRECT:
            if( HANDLE_INDIRECT_BLOCK(h) < s_instance.m_indirectSize &&
                HANDLE_INDIRECT_INDEX(h) < HANDLE_BLOCK_SIZE )
            {
                t = &s_instance.m_indirect[HANDLE_INDIRECT_BLOCK(h)][HANDLE_INDIRECT_INDEX(h)];
            }
            break;

        default:
            break;
        }

        MpiRwLockReleaseShared(&s_instance.m_lock);

        if(t != nullptr && t->ref_count < 1 )
        {
            t = nullptr;
        }
        return t;
    }


protected:
    //
    // Get is an unsafe lookup by handle.  It does not do any kind of validation
    // of the input handle at runtime (though it does assert on bounds checks).
    //
    _Ret_notnull_ static __forceinline T* GetImpl( HandleType h )
    {
        MPIU_Assert( HANDLE_GET_MPI_KIND(h) == TraitsT::KIND );
        MPIU_Assert( HANDLE_GET_TYPE(h) != HANDLE_TYPE_INVALID );

        MpiRwLockAcquireShared(&s_instance.m_lock);

        T* t = nullptr;

        switch( HANDLE_GET_TYPE(h) )
        {
        case HANDLE_TYPE_BUILTIN:
            MPIU_Assert( (HANDLE_DIRECT_INDEX(h) & TraitsT::BUILTINMASK) < TraitsT::BUILTIN );
            t = &s_instance.m_builtin[HANDLE_DIRECT_INDEX(h) & TraitsT::BUILTINMASK];
            break;

        case HANDLE_TYPE_DIRECT:
            MPIU_Assert( HANDLE_DIRECT_INDEX(h) < TraitsT::DIRECT );
            t = &s_instance.m_direct[HANDLE_DIRECT_INDEX(h)];
            break;

        case HANDLE_TYPE_INDIRECT:
            MPIU_Assert( HANDLE_INDIRECT_BLOCK(h) < s_instance.m_indirectSize );
            MPIU_Assert( HANDLE_INDIRECT_INDEX(h) < HANDLE_BLOCK_SIZE );
            t = &s_instance.m_indirect[HANDLE_INDIRECT_BLOCK(h)][HANDLE_INDIRECT_INDEX(h)];
            break;

        default:
            __assume(0);
            break;
        }

        MpiRwLockReleaseShared(&s_instance.m_lock);
        return t;
    }


public:
    _Ret_notnull_ static __forceinline T* Get( HandleType h )
    {
        return PoolType::GetImpl( h );
    }
};


#endif /* _OBJPOOL_ */
