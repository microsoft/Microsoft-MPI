// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

Module Name:
    autoptr.h

Abstract:
    Useful templates for Auto pointer and auto Release

Author:
    Erez Haba (erezh) 11-Mar-96

--*/

#pragma once

#ifndef _MSMQ_AUTOPTR_H_
#define _MSMQ_AUTOPTR_H_


//---------------------------------------------------------
//
//  template class P
//
//---------------------------------------------------------
template<typename T>
class CppDeallocator
{
public:
    static void Free( T* p ){ delete p; }
};


template<> void CppDeallocator<void>::Free( void* p )
{
    MPIU_Free( p );
}


template<typename T, typename Deallocator = CppDeallocator<T>>
class StackGuardPointer
{
private:
    T* m_p;

public:
    StackGuardPointer(T* p = nullptr) : m_p(p)    {}
   ~StackGuardPointer()     { Deallocator::Free( m_p ); }

    operator T*() const     { return m_p; }
    T* operator->() const   { return m_p; }
    T* get() const          { return m_p; }
    T* detach()             { T* p = m_p; m_p = nullptr; return p; }
    void free()             { Deallocator::Free( detach() ); }


    T** operator&()
    {
        ASSERT(("Auto pointer in use, can't take it's address", m_p == 0));
        return &m_p;
    }


    StackGuardPointer& operator=(T* p)
    {
        ASSERT(("Auto pointer in use, can't assign it", m_p == 0));
        m_p = p;
        return *this;
    }


    VOID*& ref_unsafe()
    {
        // unsafe ref to auto pointer, for special uses like
        // InterlockedCompareExchangePointer

        return *reinterpret_cast<VOID**>(&m_p);
    }


private:
    StackGuardPointer(const StackGuardPointer&);
        StackGuardPointer<T, Deallocator>& operator=(const StackGuardPointer<T, Deallocator>&);
};


//---------------------------------------------------------
//
//  template class StackGuardArray
//
//---------------------------------------------------------
template<class T>
class StackGuardArray
{
private:
    T* m_p;

public:
    StackGuardArray(T* p = 0) : m_p(p)   {}
   ~StackGuardArray()                    { delete[] m_p; }

    operator T*() const     { return m_p; }
    T* operator->() const   { return m_p; }
    T* get() const          { return m_p; }
    T* detach()             { T* p = m_p; m_p = 0; return p; }
    void free()             { delete[] detach(); }
    void swap(StackGuardArray& rhs)      { T* t = m_p; m_p = rhs.m_p, rhs.m_p = t;}

    T** operator&()
    {
        ASSERT(("Auto pointer in use, can't take it's address", m_p == 0));
        return &m_p;
    }


    StackGuardArray& operator=(T* p)
    {
        ASSERT(("Auto pointer in use, can't assign", m_p == 0));
        m_p = p;
        return *this;
    }


    VOID*& ref_unsafe()
    {
        // unsafe ref to auto pointer, for special uses like
        // InterlockedCompareExchangePointer

        return *reinterpret_cast<VOID**>(&m_p);
    }

private:
    StackGuardArray(const StackGuardArray&);
        StackGuardArray<T>& operator=(const StackGuardArray<T>&);

};

//---------------------------------------------------------
//
//  template SafeAssign helper function.
//
//---------------------------------------------------------
template <class T> T&  SafeAssign(T& dest , T& src)
{
        if(dest.get() != src.get() )
        {
                dest.free();
                if(src.get() != NULL)
                {
                        dest =  src.detach();
                }
        }
        return dest;
}


template<class T> void SafeDestruct(T* p)
{
    if (p != NULL)
    {
        p->~T();
    }
}

//---------------------------------------------------------
//
//  template class D
//
//---------------------------------------------------------
template<class T>
class D
{
private:
    T* m_p;

public:
    D(T* p = 0) : m_p(p)    {}
   ~D()                     { SafeDestruct<T>(m_p); }

    operator T*() const     { return m_p; }
    T* operator->() const   { return m_p; }
    T* get() const          { return m_p; }
    T* detach()             { T* p = m_p; m_p = 0; return p; }
    void free()             { SafeDestruct<T>(detach()); }


    T** operator&()
    {
        ASSERT(("Auto pointer in use, can't take it's address", m_p == 0));
        return &m_p;
    }


    D& operator=(T* p)
    {
        ASSERT(("Auto pointer in use, can't assign", m_p == 0));
        m_p = p;
        return *this;
    }

private:
    D(const D&);
};


//---------------------------------------------------------
//
//  template SafeAddRef/SafeRelease helper functions.
//
//---------------------------------------------------------
template<class T> T* SafeAddRef(T* p)
{
    if (p != nullptr)
    {
        p->AddRef();
    }

    return p;
}


template<class T> void SafeRelease(T* p)
{
    if (p != nullptr)
    {
        p->Release();
    }
}


//---------------------------------------------------------
//
//  template class StackGuardRef
//
//---------------------------------------------------------
template<class T>
class StackGuardRef
{
private:
    T* m_p;

public:
    explicit StackGuardRef(T* p = nullptr) : m_p(p)    {}
   ~StackGuardRef()                     { SafeRelease<T>(m_p); }

    //
    // Removed casting operator, this operator leads to bugs that are
    // hard to detect. To get the object pointer use get() explicitly.
    //
    //operator T*() const     { return m_p; }

    //
    // Removed pointer reference operator, this operator prevents StackGuardRef usage in
    // STL containers. Use the ref() member instead. e.g., &p.ref()
    //
    //T** operator&()       { return &m_p; }

    T* operator->() const   { return m_p; }
    T* get() const          { return m_p; }
    T* detach()             { T* p = m_p; m_p = nullptr; return p; }


    void attach( T* p )
    {
        SafeRelease<T>(m_p);
        m_p = p;
    }


    bool operator==(const T* p) const
    {
        return m_p == p;
    }


    bool operator!=(const T* p) const
    {
        return m_p != p;
    }


    T*& ref()
    {
        ASSERT(("Auto release in use, can't take object reference", m_p == 0));
        return m_p;
    }


    void free()
    {
        SafeRelease<T>(detach());
    }


private:
    //
    // This is really a StackGuard, so we don't do fancy ref counting for the user.
    //
    StackGuardRef( const StackGuardRef& r );

    StackGuardRef& operator=(const StackGuardRef& r);
};


#endif // _MSMQ_AUTOPTR_H_
