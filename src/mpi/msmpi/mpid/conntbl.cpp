// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "mpi_conntbl.h"
#include "ch3u_sshm.h"
#include "math.h"


//
// Each row in the table will be:
//  hostname (char array, up to MAX_HOSTNAME bytes, NULL terminated)
//  connectivity information, character per rank
//  terminating NULL character.
//
struct CONNECTIVITY_ROW
{
    char Hostname[MAXHOSTNAMELEN];

    int nShm;
    int nNd;
    int nSock;

    //
    // Changing the following constants will affect the output of the
    // connectivity table.  Change with care to make sure it's still
    // readable.
    //
    static const char SHM_INDICATOR = '+';
    static const char SOCK_INDICATOR = 'S';
    static const char ND_INDICATOR = '@';
    static const char NOTHING_INDICATOR = '.';
    //
    // The indicators are both an array of characters, as well as a string.
    // There's an extra NULL terminating character to make printing the table
    // simpler (removes double for loop)
    //
    char Indicators[1];
};


ConnectivityTable::ConnectivityTable()
    : m_pBuf( NULL )
{
    m_RowSize = sizeof(CONNECTIVITY_ROW) + MPIDI_Process.my_pg->size;
    SIZE_T pad = MPIDI_Process.my_pg->size % sizeof(ULONG_PTR);
    if( pad != 0 )
    {
        m_RowSize += sizeof(ULONG_PTR) - pad;
    }
}


ConnectivityTable::~ConnectivityTable()
{
    if( m_pBuf != NULL )
    {
        delete[] m_pBuf;
    }
}


void ConnectivityTable::FormatRow( CONNECTIVITY_ROW* pRow )
{
    //
    // Intentionally ignore return value. Truncation is acceptable.
    // If the function fails due to other reasons, we will have
    // potentially have garbage data in the hostname.
    //
    OACR_WARNING_SUPPRESS(HRESULT_NOT_CHECKED, "Don't care about failure here");
    StringCchCopyA( pRow->Hostname, MAXHOSTNAMELEN, MPIDI_CH3U_Hostname_sshm() );

    for( int i = 0; i < MPIDI_Process.my_pg->size; ++i )
    {
        const MPIDI_VC_t* pVc = &MPIDI_Process.my_pg->vct[i];

        if( pVc->state == MPIDI_VC_STATE_INACTIVE )
        {
            pRow->Indicators[i] = CONNECTIVITY_ROW::NOTHING_INDICATOR;
            continue;
        }

        switch( pVc->ch.channel )
        {
        case MPIDI_CH3I_CH_TYPE_SHM:
            pRow->Indicators[i] = CONNECTIVITY_ROW::SHM_INDICATOR;
            pRow->nShm++;
            break;
        case MPIDI_CH3I_CH_TYPE_ND:
        case MPIDI_CH3I_CH_TYPE_NDv1:
            pRow->Indicators[i] = CONNECTIVITY_ROW::ND_INDICATOR;
            pRow->nNd++;
            break;
        case MPIDI_CH3I_CH_TYPE_SOCK:
            pRow->Indicators[i] = CONNECTIVITY_ROW::SOCK_INDICATOR;
            pRow->nSock++;
            break;
        default:
            MPIU_Assert(
                pVc->ch.channel == MPIDI_CH3I_CH_TYPE_SHM ||
                pVc->ch.channel == MPIDI_CH3I_CH_TYPE_SOCK ||
                pVc->ch.channel == MPIDI_CH3I_CH_TYPE_ND ||
                pVc->ch.channel == MPIDI_CH3I_CH_TYPE_NDv1
                );
        }
    }
}


MPI_RESULT ConnectivityTable::AllocateAndGather( int nRows )
{
    SIZE_T allocSize = m_RowSize * nRows;
    //
    // The MPI_Gather operation takes counts as signed integers.  Trap potential overflow.
    //
    if( allocSize > INT_MAX )
    {
        return MPIU_ERR_CREATE(MPI_ERR_COUNT, "**countneg %d", -1 );
    }

    if( m_pBuf != NULL )
    {
        delete[] m_pBuf;
    }

    m_pBuf = new BYTE[allocSize];
    if( m_pBuf == NULL )
    {
        return MPIU_ERR_NOMEM();
    }

    ZeroMemory( m_pBuf, allocSize );

    //
    // Format our entry.
    //
    FormatRow( &At(0) );

    const void* pSendBuf;
    //
    // We can't have overlapping send and receive buffers.  On rank 0,
    // data is already in place so we tell MPI_Gather to ignore the send
    // buffer.
    //
    if( MPIDI_Process.my_pg_rank == 0 )
    {
        pSendBuf = MPI_IN_PLACE;
    }
    else
    {
        pSendBuf = m_pBuf;
    }

    MPI_RESULT mpiErrno = MPIR_Gather_intra(
        pSendBuf,
        static_cast<int>(m_RowSize),
        g_hBuiltinTypes.MPI_Byte,
        m_pBuf,
        static_cast<int>(m_RowSize),
        g_hBuiltinTypes.MPI_Byte,
        0,
        Mpi.CommWorld
        );

    if( mpiErrno != MPI_SUCCESS )
    {
        return MPIU_ERR_FAIL(mpiErrno);
    }

    return MPI_SUCCESS;
}


void ConnectivityTable::Print()
{
    int size = MPIDI_Process.my_pg->size;

    printf( "\n\nMPI Connectivity Table\n" );

    //
    // Rank table.
    //
    printf( "\nRank:Node Listing\n"
        "------------------------------------------------------------\n" );

    //
    // Calculate the number of digits in our largest rank so that we can properly
    // align the rank information.  We don't want to do log(0), so we keep the value
    // at least 1.  Note that log10 gives us the power of 10, and for 1-9 it returns 0,
    // so we add 1 to compensate.
    //
    int digits = static_cast<int>(
        log10( static_cast<double>( max( 1, size - 1 ) ) ) + 1 );

    for( int i = 0; i < size; i++ )
    {
        printf( "%*d: (%s)\n", digits, i, At(i).Hostname );
    }

    //
    // Connectivity table.
    //
    printf(
        "\nConnectivity\n"
        "------------------------------------------------------------\n"
        "SourceRank:[Indicators for all TargetRanks]\n"
        "  Where %c=Shared Memory, %c=Network Direct, %c=Socket, %c=Not Connected\n\n",
        CONNECTIVITY_ROW::SHM_INDICATOR,
        CONNECTIVITY_ROW::ND_INDICATOR,
        CONNECTIVITY_ROW::SOCK_INDICATOR,
        CONNECTIVITY_ROW::NOTHING_INDICATOR
        );

    //
    // Table Header, from highest digit to lowest digit.
    //
    for( int i = digits - 1; i >= 0; i-- )
    {
        //
        // Pad for row legend.  Note that we pad an extra character for the ':'.
        //
        for( int j = 0; j <= digits; j++ )
        {
            printf( " " );
        }

        //
        // Print out a row of the column legend.
        //
        int divisor = static_cast<int>( pow( 10.0, static_cast<double>( i ) ) );
        for( int j = 0; j < size; j++ )
        {
            //
            // We print only multiples of 8.
            //
            if( (j % 8 == 0) && ((j >= divisor) || (divisor == 1)) )
            {
                printf( "%d", (j / divisor) % 10 );
            }
            else
            {
                printf( " " );
            }
        }
        printf( "\n" );
    }

    //
    // Check for asymmetry.  This can happen if some nodes enter MPI_Finalize before
    // others, as the gather for the connectivity table can establish new connections.
    //
    for( int src = 0; src < size; src++ )
    {
        for( int dest = src + 1; dest < size; dest++ )
        {
            if( At(src).Indicators[dest] != At(dest).Indicators[src] )
            {
                CONNECTIVITY_ROW* pAsymmetricRow;
                int column;
                //
                // Only overwrite if one member of the pair is unconnected.
                //
                if( At(src).Indicators[dest] == CONNECTIVITY_ROW::NOTHING_INDICATOR )
                {
                    pAsymmetricRow = &At(dest);
                    column = src;
                }
                else if( At(dest).Indicators[src] == CONNECTIVITY_ROW::NOTHING_INDICATOR )
                {
                    pAsymmetricRow = &At(src);
                    column = dest;
                }
                else
                {
                    break;
                }

                switch( pAsymmetricRow->Indicators[column] )
                {
                case CONNECTIVITY_ROW::SHM_INDICATOR:
                    pAsymmetricRow->nShm--;
                    break;
                case CONNECTIVITY_ROW::SOCK_INDICATOR:
                    pAsymmetricRow->nSock--;
                    break;
                case CONNECTIVITY_ROW::ND_INDICATOR:
                    pAsymmetricRow->nNd--;
                    break;
                }

                pAsymmetricRow->Indicators[column] = CONNECTIVITY_ROW::NOTHING_INDICATOR;
            }
        }
    }

    int shm = 0;
    int nd = 0;
    int sock = 0;

    //
    // Table Rows.
    //
    for( int i = 0; i < size; i++ )
    {
        const CONNECTIVITY_ROW& row = At(i);
        printf( "%*d:%s\n", digits, i, row.Indicators );
        shm += row.nShm;
        nd += row.nNd;
        sock += row.nSock;
    }

    //
    // We double counted connections by counting them at each endpoint.
    //
    shm /= 2;
    nd /= 2;
    sock /= 2;

    //
    // Summary.
    //
    printf( "\nSummary\n"
        "------------------------------------------------------------\n" );
    printf( "Total Ranks:          %d\n", size );
    printf( "Total Connections:    %d\n", shm + nd + sock );
    printf( "  Shared Memory:      %d\n", shm );
    printf( "  Network Direct:     %d\n", nd );
    printf( "  Socket:             %d\n", sock );

    fflush( stdout );
}
