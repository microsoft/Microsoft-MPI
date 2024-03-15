// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

template<typename T>
class SimpleAllocator
{
public:
    T* Allocate()
    {
        return new T;
    }
    void Free(T* p)
    {
        delete p;
    }
};


template<typename T, typename AllocatorT = SimpleAllocator<T>>
class Tls
{
public:
    typedef typename T                  ElementType;
    typedef typename Tls<T,AllocatorT>  ThisType;
    typedef typename AllocatorT         ThisAllocator;

public:
    DWORD                   SlotIndex;
    mutable ThisAllocator   Allocator;

public:
    Tls()
        : SlotIndex(::TlsAlloc())
        , Allocator()
    {
    }
    ~Tls()
    {
        if (SlotIndex != TLS_OUT_OF_INDEXES)
        {
            ::TlsFree(SlotIndex);
        }
    }

    void DetachCurrentThread() const
    {
        if (SlotIndex != TLS_OUT_OF_INDEXES)
        {
            Allocator.Free(
                static_cast<T*>(::TlsGetValue(SlotIndex))
                );
            ::TlsSetValue(SlotIndex, nullptr);
        }
    }

    ElementType& Get() const
    {
        ElementType* p = nullptr;
        if (SlotIndex != TLS_OUT_OF_INDEXES)
        {
            p = static_cast<ElementType*>(::TlsGetValue(SlotIndex));
            if (p == nullptr)
            {
                p = Allocator.Allocate();
                if (p != nullptr)
                {
                    ::TlsSetValue(SlotIndex, p);
                }
            }
        }

        //
        // we consider it fatal if pointer is null, so we perform an
        // invariant assert so we can fail FRE builds as well as debug.
        //
        MPIU_Assertp(nullptr != p);

        return *p;
    }


    ElementType* operator ->()
    {
        return &Get();
    }

private:
    Tls(const ThisType& other);
    ThisType& operator = (const ThisType& other);
};



