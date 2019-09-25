// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "precomp.h"
#include "smpd.h"

static char hexchar(int x)
{
    switch (x)
    {
    case 0: return '0';
    case 1: return '1';
    case 2: return '2';
    case 3: return '3';
    case 4: return '4';
    case 5: return '5';
    case 6: return '6';
    case 7: return '7';
    case 8: return '8';
    case 9: return '9';
    case 10: return 'A';
    case 11: return 'B';
    case 12: return 'C';
    case 13: return 'D';
    case 14: return 'E';
    case 15: return 'F';
    }
    return '0';
}

static char charhex(char c)
{
    switch (c)
    {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'A': return 10;
    case 'B': return 11;
    case 'C': return 12;
    case 'D': return 13;
    case 'E': return 14;
    case 'F': return 15;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
    }
    return 0;
}


static void char_to_hex(char ch, _Out_cap_(2) char *hex)
{
    *hex = hexchar((ch>>4) & 0xF);
    hex++;
    *hex = hexchar(ch & 0xF);
}


static char hex_to_char(const char *hex)
{
    unsigned char ch;
    ch = charhex(*hex) << 4;
    hex++;
    ch = ch | charhex(*hex);
    return (char)ch;
}



void
smpd_encode_buffer(
    _Out_writes_z_(dest_length) char *dest,
    _In_ size_t dest_length,
    _In_reads_(src_length) PCSTR src,
    _In_ UINT32 src_length,
    _Out_ UINT32 *num_encoded
    )
{
    UINT32 n = 0;
    while ((src_length > 0) && (dest_length > 2))
    {
        char_to_hex(*src, dest);
        src++;
        dest += 2;
        src_length--;
        dest_length -= 2;
        n++;
    }
    *dest = '\0';
    *num_encoded = n;
}


void
smpd_decode_buffer(
    _In_ PCSTR str,
    _Out_writes_z_(dest_length) char *dest,
    _In_ UINT32 dest_length,
    _Out_ UINT32* num_decoded
    )
{
    UINT32 n = 0;
    while(dest_length > 1)
    {
        if(*str == '\0' || *(str+1) == '\0')
        {
            break;
        }

        *dest = hex_to_char(str);
        str += 2;
        dest++;
        dest_length--;
        n++;
    }
    *dest = '\0';
    *num_decoded = n;
}
