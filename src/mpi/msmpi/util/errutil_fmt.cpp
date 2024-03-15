// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"


static inline void
AppendAssertString(
    _Out_writes_z_(size) char* dst,
    _In_ size_t size,
    _In_z_ const char* astr,
    _In_ int prefix
    )
{
    if(prefix)
    {
        MPIU_Strnapp(dst, " | ", size);
    }
    MPIU_Strnapp(dst, astr, size);
}


#define CHECK_AND_APPEND_ASSERT_STRING(tag)\
    if(d & tag)\
    {\
        AppendAssertString(buffer, size, #tag, prefix);\
        prefix = 1;\
    }

_Ret_z_
static const char *
GetAssertString(
    _In_ int d,
    _Out_writes_(size) char* buffer,
    _In_ size_t size
    )
{
    int prefix = 0;
    char assert_val[12];

    if (d == 0)
        return "Assert=0";

    MPIU_Strncpy(buffer, "Assert=", size);
    CHECK_AND_APPEND_ASSERT_STRING(MPI_MODE_NOCHECK);
    CHECK_AND_APPEND_ASSERT_STRING(MPI_MODE_NOSTORE);
    CHECK_AND_APPEND_ASSERT_STRING(MPI_MODE_NOPUT);
    CHECK_AND_APPEND_ASSERT_STRING(MPI_MODE_NOPRECEDE);
    CHECK_AND_APPEND_ASSERT_STRING(MPI_MODE_NOSUCCEED);

    d &= ~(MPI_MODE_NOCHECK | MPI_MODE_NOSTORE | MPI_MODE_NOPUT | MPI_MODE_NOPRECEDE | MPI_MODE_NOSUCCEED);
    if (d)
    {
        MPIU_Snprintf(assert_val, _countof(assert_val), "0x%x", d);
        AppendAssertString(buffer, size, assert_val, prefix);
    }

    return buffer;
}

_Ret_z_
static const char *
GetDTypeString(
    _In_ MPI_Datatype d,
    _Out_writes_(size) char* buffer,
    size_t size
    )
{
    const char* str;
    int num_integers, num_addresses, num_datatypes, combiner = 0;

    //
    // Check that the provided datatype is valid.
    //
    // The MPID_Type_get_envelope call below may call TypePool::Get() and deref.
    // We may have gotten here because the datatype wasn't valid to begin with.
    //
    MPID_Datatype* pType = TypePool::Lookup( d );
    if( pType == nullptr )
    {
        if( d == MPI_DATATYPE_NULL )
        {
            return "MPI_DATATYPE_NULL";
        }

        return "INVALID DATATYPE";
    }

    MPID_Type_get_envelope(d, &num_integers, &num_addresses, &num_datatypes, &combiner);
    if (combiner == MPI_COMBINER_NAMED)
    {
        str = MPIDU_Datatype_builtin_to_string(d);
        if (str != nullptr)
            return str;

        MPIU_Snprintf(buffer, size, "dtype=0x%08x", d);
        return buffer;
    }

    /* default is not thread safe */
    str = MPIDU_Datatype_combiner_to_string(combiner);
    if (str != nullptr)
    {
        MPIU_Snprintf(buffer, size, "dtype=USER<%s>", str);
        return buffer;
    }

    MPIU_Snprintf(buffer, size, "dtype=USER<0x%08x>", d);
    return buffer;
}


_Ret_z_
static const char*
GetMPIOpString(
    _In_ MPI_Op o,
    _Out_writes_(size) char* buffer,
    size_t size
    )
{
    switch (o)
    {
    case MPI_OP_NULL:   return "MPI_OP_NULL";
    case MPI_MAX:       return "MPI_MAX";
    case MPI_MIN:       return "MPI_MIN";
    case MPI_SUM:       return "MPI_SUM";
    case MPI_PROD:      return "MPI_PROD";
    case MPI_LAND:      return "MPI_LAND";
    case MPI_BAND:      return "MPI_BAND";
    case MPI_LOR:       return "MPI_LOR";
    case MPI_BOR:       return "MPI_BOR";
    case MPI_LXOR:      return "MPI_LXOR";
    case MPI_BXOR:      return "MPI_BXOR";
    case MPI_MINLOC:    return "MPI_MINLOC";
    case MPI_MAXLOC:    return "MPI_MAXLOC";
    case MPI_REPLACE:   return "MPI_REPLACE";
    case MPI_NO_OP:     return "MPI_NO_OP";
    }

    MPIU_Snprintf(buffer, size, "op=0x%x", o);
    return buffer;
}


/* ------------------------------------------------------------------------ */
/* This routine takes an instance-specific string with format specifiers    */
/* This routine makes use of the above routines, along with some inlined    */
/* code, to process the format specifiers for the MPI objects               */
/* ------------------------------------------------------------------------ */
_Success_(return==MPI_SUCCESS)
int
MPIR_Err_vsnprintf_mpi(
    _Out_writes_z_(maxlen) char* str,
    _In_ size_t maxlen,
    _Printf_format_string_ const char* fmt,
    _In_ va_list list
    )
{
    const char* begin;
    const char* end;
    size_t len;
    GUID guid;
    MPI_Comm C;
    MPI_Info I;
    MPI_Datatype D;
    MPI_Win W;
    MPI_Group G;
    MPI_Op O;
    MPI_Request R;
    MPI_Errhandler E;
    const char *s;
    int t, i, d;
    const void *p;
    char buff[256];
    const char* strEnd;
    __int64 l;

    MPIU_Assert(maxlen > 0);

    begin = fmt;
    for(;;)
    {
        if(maxlen == 0)
        {
             *--str = '\0';
             return 0;
        }

        end = strchr(begin, '%');
        if(end == nullptr)
            break;

        /* pass the % char; point to the format specifier */
        end++;
        if(*end == '\0')
            break;

        /* add 1 (already in end) for the null char */
        len = (size_t)(end - begin);
        len = min(maxlen, len);
        MPIU_Strncpy(str, begin, len);

        /* len > 0 -> (end > begin) && (maxlen > 0) */
        len--;
        str += len;
        maxlen -= len;

        begin = end + 1;
        switch ((int)(*end))
        {
        case (int)'s':
            s = va_arg(list, char *);
            if (s)
            {
                strEnd = MPIU_Strncpy(str, s, maxlen);
                len = strEnd - str;
            }
            else
            {
                strEnd = MPIU_Strncpy(str, "<NULL>", maxlen );
                len = strEnd - str;
            }
            break;
        case (int)'d':
            d = va_arg(list, int);
            len = MPIU_Snprintf(str, maxlen, "%d", d);
            break;
        case (int)'x':
            d = va_arg(list, int);
            len = MPIU_Snprintf(str, maxlen, "0x%08x", d);
            break;
        case (int)'i':
            i = va_arg(list, int);
            switch (i)
            {
            case MPI_ANY_SOURCE:
                strEnd = MPIU_Strncpy(str, "MPI_ANY_SOURCE", maxlen);
                len = strEnd - str;
                break;
            case MPI_PROC_NULL:
                strEnd = MPIU_Strncpy(str, "MPI_PROC_NULL", maxlen);
                len = strEnd - str;
                break;
            case MPI_ROOT:
                strEnd = MPIU_Strncpy(str, "MPI_ROOT", maxlen);
                len = strEnd - str;
                break;
            case MPI_UNDEFINED:
                strEnd = MPIU_Strncpy(str, "MPI_UNDEFINED", maxlen);
                len = strEnd - str;
                break;
            default:
                len = MPIU_Snprintf(str, maxlen, "%d", i);
                break;
            }
            break;
        case (int)'t':
            t = va_arg(list, int);
            switch (t)
            {
            case MPI_ANY_TAG:
                strEnd = MPIU_Strncpy(str, "MPI_ANY_TAG", maxlen);
                len = strEnd - str;
                break;
            case MPI_UNDEFINED:
                strEnd = MPIU_Strncpy(str, "MPI_UNDEFINED", maxlen);
                len = strEnd - str;
                break;
            default:
                len = MPIU_Snprintf(str, maxlen, "%d", t);
                break;
            }
            break;
        case (int)'p':
            p = va_arg(list, void *);
            /* FIXME: A check for MPI_IN_PLACE should only be used
               where that is valid */
            if (p == MPI_IN_PLACE)
            {
                strEnd = MPIU_Strncpy(str, "MPI_IN_PLACE", maxlen);
                len = strEnd - str;
            }
            else
            {
                len = MPIU_Snprintf(str, maxlen, "0x%p", p);
            }
            break;
        case (int)'C':
            C = va_arg(list, MPI_Comm);
            switch (C)
            {
            case MPI_COMM_WORLD:
                strEnd = MPIU_Strncpy(str, "MPI_COMM_WORLD", maxlen);
                len = strEnd - str;
                break;
            case MPI_COMM_SELF:
                strEnd = MPIU_Strncpy(str, "MPI_COMM_SELF", maxlen);
                len = strEnd - str;
                break;
            case MPI_COMM_NULL:
                strEnd = MPIU_Strncpy(str, "MPI_COMM_NULL", maxlen);
                len = strEnd - str;
                break;
            default:
                len = MPIU_Snprintf(str, maxlen, "comm=0x%x", C);
                break;
            }
            break;
        case (int)'I':
            I = va_arg(list, MPI_Info);
            if (I == MPI_INFO_NULL)
            {
                strEnd = MPIU_Strncpy(str, "MPI_INFO_NULL", maxlen);
                len = strEnd - str;
            }
            else
            {
                len = MPIU_Snprintf(str, maxlen, "info=0x%x", I);
            }
            break;
        case (int)'D':
            D = va_arg(list, MPI_Datatype);
            len = MPIU_Snprintf(str, maxlen, "%s", GetDTypeString(D, buff, _countof(buff)));
            break;
#ifdef MPI_MODE_RDWR
        case (int)'F':
            {
                const MPI_File F = va_arg(list, MPI_File);
                OACR_USE_PTR(F);
                if (F == MPI_FILE_NULL)
                {
                    strEnd = MPIU_Strncpy(str, "MPI_FILE_NULL", maxlen);
                    len = strEnd - str;
                }
                else
                {
                    len = MPIU_Snprintf(str, maxlen, "file=0x%p", F);
                }
            }
            break;
#endif
        case (int)'W':
            W = va_arg(list, MPI_Win);
            if (W == MPI_WIN_NULL)
            {
                strEnd = MPIU_Strncpy(str, "MPI_WIN_NULL", maxlen);
                len = strEnd - str;
            }
            else
            {
                len = MPIU_Snprintf(str, maxlen, "win=0x%x", W);
            }
            break;
        case (int)'A':
            d = va_arg(list, int);
            len = MPIU_Snprintf(str, maxlen, "%s", GetAssertString(d, buff, _countof(buff)));
            break;
        case (int)'G':
            G = va_arg(list, MPI_Group);
            if (G == MPI_GROUP_NULL)
            {
                strEnd = MPIU_Strncpy(str, "MPI_GROUP_NULL", maxlen);
                len = strEnd - str;
            }
            else
            {
                len = MPIU_Snprintf(str, maxlen, "group=0x%x", G);
            }
            break;
        case (int)'O':
            O = va_arg(list, MPI_Op);
            len = MPIU_Snprintf(str, maxlen, "%s", GetMPIOpString(O, buff, _countof(buff)));
            break;
        case (int)'R':
            R = va_arg(list, MPI_Request);
            if (R == MPI_REQUEST_NULL)
            {
                strEnd = MPIU_Strncpy(str, "MPI_REQUEST_NULL", maxlen);
                len = strEnd - str;
            }
            else
            {
                len = MPIU_Snprintf(str, maxlen, "req=0x%x", R);
            }
            break;
        case (int)'E':
            E = va_arg(list, MPI_Errhandler);
            if (E == MPI_ERRHANDLER_NULL)
            {
                strEnd = MPIU_Strncpy(str, "MPI_ERRHANDLER_NULL", maxlen);
                len = strEnd - str;
            }
            else
            {
                len = MPIU_Snprintf(str, maxlen, "errh=0x%x", E);
            }
            break;
        case 'g':
            guid = va_arg(list, GUID);

            len = MPIU_Snprintf(
                str,
                maxlen,
                "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
                );
            break;
        case 'l':
            l = va_arg( list, __int64 );
            len = MPIU_Snprintf( str, maxlen, "%I64d", l );
            break;
        default:
            len = MPIU_Snprintf(str, maxlen, "(unknown format)");
            break;
        }

        maxlen -= len;
        str += len;
    }
    if (*begin != '\0')
    {
        MPIU_Strncpy(str, begin, maxlen);
    }

    return 0;
}
