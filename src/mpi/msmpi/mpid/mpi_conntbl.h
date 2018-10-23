// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

//
// Connectivity Table: collects and prints information about how processes
// connected to one another (SHM, SOCK, ND.)
//

#pragma once


class ConnectivityTable
{
    BYTE* m_pBuf;
    SIZE_T m_RowSize;

public:
    ConnectivityTable();
    ~ConnectivityTable();

    MPI_RESULT AllocateAndGather( int nRows );
    void Print();

private:
    static void FormatRow( struct CONNECTIVITY_ROW* pRow );
    struct CONNECTIVITY_ROW& At( SIZE_T index )
    {
        return *reinterpret_cast<CONNECTIVITY_ROW*>( m_pBuf + (m_RowSize * index) );
    }
};
