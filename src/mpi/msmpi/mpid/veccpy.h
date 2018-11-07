// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef VECCPY_H
#define VECCPY_H

#ifdef HAVE_ANY_INT64_T_ALIGNEMENT
#define MPIR_ALIGN8_TEST(p1,p2) 1
#else
#define MPIR_ALIGN8_TEST(p1,p2) ((((MPI_Aint)p1 | (MPI_Aint)p2) & 0x7) == 0)
#endif

#ifdef HAVE_ANY_INT32_T_ALIGNEMENT
#define MPIR_ALIGN4_TEST(p1,p2)  1
#else
#define MPIR_ALIGN4_TEST(p1,p2) ((((MPI_Aint)p1 | (MPI_Aint)p2) & 0x3) == 0)
#endif

template<typename T>
inline void MPIDI_Copy_from_vec(
    _In_ char** src,
    _Inout_ _Outptr_result_bytebuffer_( sizeof(T) * nelms * count ) char** dest,
    _In_ DLOOP_Offset stride,
    _In_range_( >=, 0 ) DLOOP_Count nelms,
    _In_range_( >, 0 ) DLOOP_Count count
    )
{
    if (nelms == 0)
    {
        src = src + (count * stride);
    }
    else if (stride % sizeof(T))
    {
        MPIDI_Copy_from_vec_unaligned<T>(src,dest,stride,nelms,count);
    }
    else
    {
        MPIDI_Copy_from_vec_aligned<T>(src,dest,stride/sizeof(T),nelms,count);
    }
}

template<typename T>
inline void MPIDI_Copy_to_vec(
    _In_ char** src,
    _Inout_ _Outptr_result_bytebuffer_( sizeof(T) * nelms * count ) char** dest,
    _In_ DLOOP_Offset stride,
    _In_range_( >=, 0 ) DLOOP_Count nelms,
    _In_range_( >, 0 ) DLOOP_Count count
    )
{
    if (nelms == 0)
    {
        dest = dest + (count * stride);
    }
    else if (stride % sizeof(T))
    {
        MPIDI_Copy_to_vec_unaligned<T>(src,dest,stride,nelms,count);
    }
    else
    {
        MPIDI_Copy_to_vec_aligned<T>(src,dest,stride/sizeof(T),nelms,count);
    }
}

template<typename T>
inline void MPIDI_Copy_from_vec_aligned(
    _In_ char** src,
    _Inout_ _Outptr_result_bytebuffer_( sizeof(T) * nelms * count ) char** dest,
    _In_ DLOOP_Offset stride,
    _In_range_( >, 0 ) DLOOP_Count nelms,
    _In_range_( >, 0 ) DLOOP_Count count
    )
{
    const T *l_src = reinterpret_cast<const T *>(*src);
    T *l_dest = reinterpret_cast<T *>(*dest);
    const T * tmp_src = l_src;
    register int i, j, k;
    unsigned long total_count = count * nelms;
    if (nelms == 1)
    {
        for (i = total_count; i; i--)
        {
            *l_dest++ = *l_src;
            l_src += stride;
        }
    }
    else if (nelms == 2)
    {
        for (i = total_count; i; i -= 2)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            l_src += stride;
        }
    }
    else if (nelms == 3)
    {
        for (i = total_count; i; i -= 3)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            l_src += stride;
        }
    }
    else if (nelms == 4)
    {
        for (i = total_count; i; i -= 4)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            l_src += stride;
        }
    }
    else if (nelms == 5)
    {
        for (i = total_count; i; i -= 5)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            l_src += stride;
        }
    }
    else if (nelms == 6)
    {
        for (i = total_count; i; i -= 6)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            *l_dest++ = l_src[5];
            l_src += stride;
        }
    }
    else if (nelms == 7)
    {
        for (i = total_count; i; i -= 7)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            *l_dest++ = l_src[5];
            *l_dest++ = l_src[6];
            l_src += stride;
        }
    }
    else if (nelms == 8)
    {
        for (i = total_count; i; i -= 8)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            *l_dest++ = l_src[5];
            *l_dest++ = l_src[6];
            *l_dest++ = l_src[7];
            l_src += stride;
        }
    }
    else
    {
        i = total_count;
        while (i)
        {
            tmp_src = l_src;
            j = nelms;
            while (j >= 8)
            {
                *l_dest++ = tmp_src[0];
                *l_dest++ = tmp_src[1];
                *l_dest++ = tmp_src[2];
                *l_dest++ = tmp_src[3];
                *l_dest++ = tmp_src[4];
                *l_dest++ = tmp_src[5];
                *l_dest++ = tmp_src[6];
                *l_dest++ = tmp_src[7];
                j -= 8;
                tmp_src += 8;
            }
            for (k = 0; k < j; k++)
            {
                *l_dest++ = *tmp_src++;
            }
            l_src += stride;
            i -= nelms;
        }
    }
    *src = reinterpret_cast<char *>(const_cast<T *>(l_src));
    *dest = reinterpret_cast<char *>(l_dest);
}

template<typename T>
inline void MPIDI_Copy_from_vec_unaligned(
    _In_ char** src,
    _Inout_ _Outptr_result_bytebuffer_( sizeof(T) * nelms * count ) char** dest,
    _In_ DLOOP_Offset stride,
    _In_range_( >, 0 ) DLOOP_Count nelms,
    _In_range_( >, 0 ) DLOOP_Count count
    )
{
    const T *l_src = reinterpret_cast<const T *>(*src);
    T *l_dest = reinterpret_cast<T *>(*dest);
    const T * tmp_src = l_src;
    register int i, j, k;
    unsigned long total_count = count * nelms;

    if (nelms == 1)
    {
        for (i = total_count; i; i--)
        {
            *l_dest++ = *l_src;
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else if (nelms == 2)
    {
        for (i = total_count; i; i -= 2)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else if (nelms == 3)
    {
        for (i = total_count; i; i -= 3)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else if (nelms == 4)
    {
        for (i = total_count; i; i -= 4)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else if (nelms == 5)
    {
        for (i = total_count; i; i -= 5)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else if (nelms == 6)
    {
        for (i = total_count; i; i -= 6)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            *l_dest++ = l_src[5];
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else if (nelms == 7)
    {
        for (i = total_count; i; i -= 7)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            *l_dest++ = l_src[5];
            *l_dest++ = l_src[6];
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else if (nelms == 8)
    {
        for (i = total_count; i; i -= 8)
        {
            *l_dest++ = l_src[0];
            *l_dest++ = l_src[1];
            *l_dest++ = l_src[2];
            *l_dest++ = l_src[3];
            *l_dest++ = l_src[4];
            *l_dest++ = l_src[5];
            *l_dest++ = l_src[6];
            *l_dest++ = l_src[7];
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
        }
    }
    else
    {
        i = total_count;
        while (i)
        {
            tmp_src = l_src;
            j = nelms;
            while (j >= 8)
            {
                *l_dest++ = tmp_src[0];
                *l_dest++ = tmp_src[1];
                *l_dest++ = tmp_src[2];
                *l_dest++ = tmp_src[3];
                *l_dest++ = tmp_src[4];
                *l_dest++ = tmp_src[5];
                *l_dest++ = tmp_src[6];
                *l_dest++ = tmp_src[7];
                j -= 8;
                tmp_src += 8;
            }
            for (k = 0; k < j; k++)
            {
                *l_dest++ = *tmp_src++;
            }
            l_src = reinterpret_cast<const T *> (reinterpret_cast<const char *>(l_src) + stride);
            i -= nelms;
        }
    }
    *src = reinterpret_cast<char *>(const_cast<T *>(l_src));
    *dest = reinterpret_cast<char *>(l_dest);
}

template<typename T>
inline void MPIDI_Copy_to_vec_aligned(
    _In_ char** src,
    _Inout_ _Outptr_result_bytebuffer_( sizeof(T) * nelms * count ) char** dest,
    _In_ DLOOP_Offset stride,
    _In_range_( >, 0 ) DLOOP_Count nelms,
    _In_range_( >, 0 ) DLOOP_Count count
    )
{
    const T *l_src = reinterpret_cast<const T *>(*src);
    T *l_dest = reinterpret_cast<T *>(*dest);
    T * tmp_dest = l_dest;
    register int i, j, k;
    unsigned long total_count = count * nelms;

    if (nelms == 1)
    {
        for (i = total_count; i; i--)
        {
            *l_dest = *l_src++;
            l_dest += stride;
        }
    }
    else if (nelms == 2)
    {
        for (i = total_count; i; i -= 2)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest += stride;
        }
    }
    else if (nelms == 3)
    {
        for (i = total_count; i; i -= 3)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest += stride;
        }
    }
    else if (nelms == 4)
    {
        for (i = total_count; i; i -= 4)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest += stride;
        }
    }
    else if (nelms == 5)
    {
        for (i = total_count; i; i -= 5)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest += stride;
        }
    }
    else if (nelms == 6)
    {
        for (i = total_count; i; i -= 6)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest[5] = *l_src++;
            l_dest += stride;
        }
    }
    else if (nelms == 7)
    {
        for (i = total_count; i; i -= 7)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest[5] = *l_src++;
            l_dest[6] = *l_src++;
            l_dest += stride;
        }
    }
    else if (nelms == 8)
    {
        for (i = total_count; i; i -= 8)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest[5] = *l_src++;
            l_dest[6] = *l_src++;
            l_dest[7] = *l_src++;
            l_dest += stride;
        }
    }
    else
    {
        i = total_count;
        while (i)
        {
            tmp_dest = l_dest;
            j = nelms;
            while (j >= 8)
            {
                tmp_dest[0] = *l_src++;
                tmp_dest[1] = *l_src++;
                tmp_dest[2] = *l_src++;
                tmp_dest[3] = *l_src++;
                tmp_dest[4] = *l_src++;
                tmp_dest[5] = *l_src++;
                tmp_dest[6] = *l_src++;
                tmp_dest[7] = *l_src++;
                j -= 8;
                tmp_dest += 8;
            }
            for (k = 0; k < j; k++)
            {
                *tmp_dest++ = *l_src++;
            }
            l_dest += stride;
            i -= nelms;
        }
    }
    *src = reinterpret_cast<char *>(const_cast<T *>(l_src));
    *dest = reinterpret_cast<char *>(l_dest);
}

template<typename T>
inline void MPIDI_Copy_to_vec_unaligned(
    _In_ char** src,
    _Inout_ _Outptr_result_bytebuffer_( sizeof(T) * nelms * count ) char** dest,
    _In_ DLOOP_Offset stride,
    _In_range_( >, 0 ) DLOOP_Count nelms,
    _In_range_( >, 0 ) DLOOP_Count count
    )
{
    const T *l_src = reinterpret_cast<const T *>(*src);
    T *l_dest = reinterpret_cast<T *>(*dest);
    T * tmp_dest = l_dest;
    register int i, j, k;
    unsigned long total_count = count * nelms;

    if (nelms == 1)
    {
        for (i = total_count; i; i--)
        {
            *l_dest = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else if (nelms == 2)
    {
        for (i = total_count; i; i -= 2)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else if (nelms == 3)
    {
        for (i = total_count; i; i -= 3)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else if (nelms == 4)
    {
        for (i = total_count; i; i -= 4)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else if (nelms == 5)
    {
        for (i = total_count; i; i -= 5)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else if (nelms == 6)
    {
        for (i = total_count; i; i -= 6)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest[5] = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else if (nelms == 7)
    {
        for (i = total_count; i; i -= 7)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest[5] = *l_src++;
            l_dest[6] = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else if (nelms == 8)
    {
        for (i = total_count; i; i -= 8)
        {
            l_dest[0] = *l_src++;
            l_dest[1] = *l_src++;
            l_dest[2] = *l_src++;
            l_dest[3] = *l_src++;
            l_dest[4] = *l_src++;
            l_dest[5] = *l_src++;
            l_dest[6] = *l_src++;
            l_dest[7] = *l_src++;
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
        }
    }
    else
    {
        i = total_count;
        while (i)
        {
            tmp_dest = l_dest;
            j = nelms;
            while (j >= 8)
            {
                tmp_dest[0] = *l_src++;
                tmp_dest[1] = *l_src++;
                tmp_dest[2] = *l_src++;
                tmp_dest[3] = *l_src++;
                tmp_dest[4] = *l_src++;
                tmp_dest[5] = *l_src++;
                tmp_dest[6] = *l_src++;
                tmp_dest[7] = *l_src++;
                j -= 8;
                tmp_dest += 8;
            }
            for (k = 0; k < j; k++)
            {
                *tmp_dest++ = *l_src++;
            }
            l_dest = reinterpret_cast<T *> (reinterpret_cast<char *>(l_dest) + stride);
            i -= nelms;
        }
    }
    *src = reinterpret_cast<char *>(const_cast<T *>(l_src));
    *dest = reinterpret_cast<char *>(l_dest);
}

#endif /* VECCPY_H */
