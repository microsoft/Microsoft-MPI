// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


_Ret_z_
static const char*
find_digits(
    _In_z_ const char* p
    )
{
    while(!isdigit(static_cast<unsigned char>(*p)))
    {
        if(*p == '\0')
            return p;

        p++;
    }

    return p;
}


_Ret_z_
static const char*
skip_digits(
    _In_z_ const char* p
    )
{
    while(isdigit(static_cast<unsigned char>(*p)))
    {
        p++;
    }

    return p;
}

//
// Given the range of ranks a,b,d-f,x-z in which separators are any character
// except '-', this function checks whether the given rank belongs to this range
//
// Input:
// rank       - the rank to check for inclusion
// range      - the string containing the range of ranks
// world_size - the size of MPI_COMM_WORLD
//
// Output
// isWithinRange         - true if the rank belongs to this range
// total_unique_ranks    - the number of unique ranks in the list
//
// Return: MPI_SUCCESS if the call succeeded, error otherwise.
//
//
_Success_(return==MPI_SUCCESS)
int
MPIU_Parse_rank_range(
    _In_                  unsigned int  rank,
    _In_z_                const char*   range,
    _In_                  unsigned int  world_size,
    _Out_                 bool*         isWithinRange,
    _Out_writes_(world_size) unsigned int* total_unique_ranks
    )
{
    MPIU_Assert( range != nullptr );

    if( CompareStringA( LOCALE_INVARIANT,
                        0,
                        range,
                        -1,
                        "all",
                        -1 ) == CSTR_EQUAL ||
        CompareStringA( LOCALE_INVARIANT,
                        0,
                        range,
                        -1,
                        "*",
                        -1 ) == CSTR_EQUAL )
    {
        *isWithinRange = true;
        *total_unique_ranks = world_size;
        return MPI_SUCCESS;
    }

    //
    // The first character has to be a digit
    //
    if( !isdigit( static_cast<unsigned char>( range[0] ) ) )
    {
        return MPIU_ERR_CREATE( MPI_ERR_OTHER, "**invalidrange %s", range );
    }

    bool *ranks = new bool[world_size]();

    if (ranks == nullptr)
    {
        return MPIU_ERR_NOMEM();
    }

    for (unsigned int i=0; i < world_size; i++ )
    {
        ranks[i] = false;
    }

    bool found = false;
    unsigned int total = 0;
    for( const char* curPos = find_digits( range );
         *curPos != '\0';
         curPos = find_digits( curPos ) )
    {
        unsigned int low = atoi( curPos );
        unsigned int high = low;

        curPos = skip_digits( curPos );

        if( *curPos == '-' )
        {
            //
            // Anything not a digit and not '-' is a valid separator
            //
            curPos ++;
            if ( isdigit( static_cast<unsigned char>( *curPos ) ) )
            {
                high = atoi( curPos );
            }
        }

        for ( unsigned int i = low; i <= high; i++ )
        {
            if ( i >= world_size )
            {
                delete [] ranks;
                return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rank %d %d", i, world_size );
            }
            if ( ranks[i] == true )
            {
                delete [] ranks;
                return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**rangedup %s %d", range, i);
            }
            ranks[i] = true;
            total++;
        }

        if ( rank >= low && rank <= high )
        {
            found = true;
        }
        curPos = skip_digits( curPos );
    }

    *total_unique_ranks = total;
    *isWithinRange = found;

    delete [] ranks;
    return MPI_SUCCESS;
}
