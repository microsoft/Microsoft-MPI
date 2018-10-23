// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_mr.h - Network Direct MPI CH3 Channel Memory Registration

--*/

#pragma once

#ifndef CH3U_NDV1_MR_H
#define CH3U_NDV1_MR_H


namespace CH3_ND
{
namespace v1
{

class CAdapter;

class CMr
{
    friend class ListHelper<CMr>;

private:
    CMr();
    ~CMr();
    int Init( _In_ CAdapter* pAdapter, _In_reads_(Length) const char* pBuf, SIZE_T Length );

public:
    static
    MPI_RESULT
    Create(
        _In_ CAdapter* pAdapter,
        _In_reads_(Length) const char* pBuf,
        _In_ SIZE_T Length,
        _Outptr_ CMr** ppMr
        );

public:
    void Shutdown();

    LONG AddRef()
    {
        return ::InterlockedIncrement(&m_nRef);
    }

    void Release();

    bool Matches( const CAdapter* pAdapter, const char* pBuf, SIZE_T Length );
    bool Overlaps( const char* pBuf, SIZE_T Length ) const;
    bool Idle() const { return m_nRef == 1; }
    bool Stale() const { return m_pAdapter.get() == NULL; }

    ND_MR_HANDLE GetHandle(){ return m_hMr; }
    SIZE_T GetLength() const { return m_Length; }

private:
    volatile LONG m_nRef;

    LIST_ENTRY m_link;

    StackGuardRef<CAdapter> m_pAdapter;

    const char* m_pBuf;
    SIZE_T m_Length;
    ND_MR_HANDLE m_hMr;
};

}   // namespace v1
}   // namespace CH3_ND

#endif // CH3U_NDV1_MR_H
