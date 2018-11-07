// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

Module Name:
    cs.h

Abstract:
    Critical Section Auto classes

Author:
    Erez Haba (erezh) 06-jan-97

--*/

#pragma once

#ifndef _MSMQ_CS_H_
#define _MSMQ_CS_H_


//---------------------------------------------------------
//
//  class CriticalSection
//
//---------------------------------------------------------
class CriticalSection
{

    friend class CS;

public:
    CriticalSection()
    {
        InitializeCriticalSection(&m_cs);
    }

    ~CriticalSection()
    {
        DeleteCriticalSection(&m_cs);
    }

private:
    void Lock()
    {
        EnterCriticalSection(&m_cs);
    }


    void Unlock()
    {
        LeaveCriticalSection(&m_cs);
    }


private:
    CRITICAL_SECTION m_cs;
};


//---------------------------------------------------------
//
//  class CS
//
//---------------------------------------------------------
class CS
{
public:
    CS(CriticalSection& lock) : m_lock(&lock)
    {
        m_lock->Lock();
    }


    ~CS()
    {
        m_lock->Unlock();
    }

private:
    CriticalSection* m_lock;
};



#endif // _MSMQ_CS_H_
