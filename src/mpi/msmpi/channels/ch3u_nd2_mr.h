// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_mr.h - Network Direct MPI CH3 Channel Memory Registration

--*/

#pragma once

#ifndef CH3U_ND_MR_H
#define CH3U_ND_MR_H


namespace CH3_ND
{

class Adapter;

class Mr
{
    friend class ListHelper<Mr>;

    volatile LONG m_nRef;

    LIST_ENTRY m_link;

    StackGuardRef<Adapter> m_pAdapter;

    const char* m_pBuf;
    SIZE_T m_Length;
    IND2MemoryRegion* m_pMr;

private:
    Mr();
    Mr( const Mr& rhs );
    ~Mr();
    int Init( _In_ Adapter& adapter, _In_reads_bytes_(cbBuf) const char* pBuf, _In_ SIZE_T cbBuf );
    Mr& operator = ( const Mr& rhs );

public:
    static
    MPI_RESULT
    Create(
        _In_ Adapter& adapter,
        _In_reads_(cbBuf)  const char* pBuf,
        _In_ SIZE_T cbBuf,
        _Outptr_ Mr** ppMr
        );

public:
    void Shutdown();

    LONG AddRef()
    {
        return ::InterlockedIncrement(&m_nRef);
    }

    void Release();

    bool Matches( const Adapter& adapter, const char* pBuf, SIZE_T cbBuf );
    bool Overlaps( const char* pBuf, SIZE_T cbBuf ) const;
    bool Idle() const { return m_nRef == 1; }
    bool Stale() const { return m_pAdapter.get() == NULL; }

    IND2MemoryRegion* IMr(){ return m_pMr; }
    SIZE_T GetLength() const { return m_Length; }
};

}   // namespace CH3_ND

#endif // CH3U_ND_MR_H
