// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

//
// Summary:
//  Ensure that the OS version is greater than or equal to the specified version.
//
// Parameters:
//  major           - Windows major version
//  minor           - Windows minor version
//
_Success_(return!=FALSE)
BOOL
CheckOSVersion(
    _In_ DWORD major,
    _In_ DWORD minor
    );


//
// Summary:
// Check if the smpd instance is running on azure and if so,
// return the logical name of the node
//
// Input:
// szBuffer: the size of the name buffer
//
// Output:
// buffer  : store the logical name. If null, name is not returned
//
// Return:
// true  if the node is on azure
// false if the node is not on azure, or if the size of the buffer is
//       too small
//
//
bool get_azure_node_logical_name(
    _Out_opt_z_cap_(szBuffer) wchar_t* buffer,
    _In_                      DWORD    szBuffer
    );


//
// We use a looping count means because the 99% case, the
//  bits will all be sequential in the low 32bits of the
//  value.  Most uses will be to count 2 to 8 bits, so the
//  loop will be less overall overhead for the 99% case.
//
template<typename T>
inline UINT8 CountBits( T value )
{
    UINT8 c = 0;
    while( value != 0 )
    {
        c += static_cast<UINT8>(value & 1);
        value >>= 1;
    }
    return c;
}


inline ULONG PowerOf2Floor( _In_range_(>, 0) ULONG value )
{
    MPIU_Assert( value != 0 );

    ULONG msb;
    _BitScanReverse( &msb, value );

    return 1 << msb;
}


inline bool IsPowerOf2( _In_range_(>, 0) ULONG value )
{
    MPIU_Assert( value != 0 );

    return ((value & (value - 1)) == 0);
}


#define POWER_OF_2_CEILING_LIMIT    0x80000000
inline ULONG PowerOf2Ceiling( _In_range_(1, POWER_OF_2_CEILING_LIMIT) ULONG value )
{
    MPIU_Assert( value != 0 && value <= POWER_OF_2_CEILING_LIMIT );

    if( IsPowerOf2( value ) )
    {
        return value;
    }

    ULONG msb;
    _BitScanReverse( &msb, value );

    return 2 << msb;
}


//
// Returns the size of the binomial subtree with root 'rank' in a tree of 'size'
// total nodes.
//
inline
unsigned
TreeSize(
    _In_range_(0, size - 1) unsigned rank,
    _In_range_(>, 0) unsigned size
    )
{
    MPIU_Assert( size > 0 );
    MPIU_Assert( rank < size );

    ULONG k;
    if( _BitScanForward( &k, rank ) == 0 )
    {
        MPIU_Assert( rank == 0 );
        return size;
    }

    k = 1 << k;
    return k < size - rank ? k : size - rank;
}


//
// Returns the number of children in the biniomial subtree with root 'rank' in
// a tree of 'size' total nodes.
//
inline
unsigned
ChildCount(
    _In_range_(0, size - 1) unsigned rank,
    _In_range_(>, 0) unsigned size
    )
{
    MPIU_Assert( size > 0 );
    MPIU_Assert( rank < size );

    unsigned treeSize = TreeSize( rank, size );
    MPIU_Assert(treeSize > 0);

    ULONG msb;
    _BitScanReverse( &msb, treeSize );

    return IsPowerOf2( treeSize ) ? msb : msb + 1;
}


//
// Returns max value in an array
//
inline
int
MaxElement(
    _In_reads_(size) const int  cnts[],
    _In_range_(>, 0) unsigned   size
    )
{
    MPIU_Assert(size > 0);

    int maxElement = cnts[0];
    for (unsigned i = 1; i < size; ++i)
    {
        if (cnts[i] > maxElement)
        {
            maxElement = cnts[i];
        }
    }
    return maxElement;
}