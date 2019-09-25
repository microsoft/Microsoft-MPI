// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <strsafe.h>
#include <oacr.h>


/*@ MPIU_Strncpy - Copy a string with buffer size. Force null termination

    Input Parameters:
.   dst - String to copy into
+   src - String to copy
-   n - 'dst' buffer size in chars (including null char)

    Return value:
    pointer to the end terminating null char

    Notes:
    This routine is the routine that you wish 'strncpy' was.  In copying
    'src' to 'dst', it stops when either the end of 'src' (the
    null character) is seen or the maximum length 'n is reached.
    Unlike 'strncpy', it does not add enough nulls to 'dst' after
    copying 'src' in order to move precisely 'n' characters.
    This routine is safer than strncpy; it always null terminates the dst
    string. (except when the dst size is zero)

    MPIU_Strncpy is implemented inline to help the compiler optimize
    per use instance.

  Module:
  Utility
  @*/
_Ret_z_
_Success_(return!=nullptr)
static inline char*
MPIU_Strncpy(
    _Out_writes_z_(n) char* dst,
    _In_z_ const char* src,
    _In_ size_t n
    )
{
    char* end;
    OACR_WARNING_SUPPRESS(USE_WIDE_API, "MSMPI uses only ANSI character set.");
    HRESULT hr = StringCchCopyExA( dst, n, src, &end, nullptr, 0 );
    if( hr == STRSAFE_E_INVALID_PARAMETER )
    {
        return nullptr;
    }

    return end;
}


_Ret_z_
_Success_(return!=nullptr)
static inline wchar_t*
MPIU_Strncpy(
    _Out_writes_z_(n) wchar_t* dst,
    _In_z_ const wchar_t* src,
    _In_ size_t n
    )
{
    wchar_t* end;
    HRESULT hr = StringCchCopyExW( dst, n, src, &end, nullptr, 0 );
    if( hr == STRSAFE_E_INVALID_PARAMETER )
    {
        return nullptr;
    }

    return end;
}


//
// Summary:
// This is a convenient wrapper for StringCchCopyA
//
// Return: 0 if success, other errors if failure
//
_Success_(return == 0)
int
MPIU_Strcpy(
    _Out_writes_z_(cchDest) char*    dest,
    _In_                 size_t      cchDest,
    _In_z_               const char* src
    );


//
// Summary:
// This is a convenient wrapper for StringCchCopyW
//
// Return: 0 if success, other errors if failure
//
_Success_(return == 0)
int
MPIU_Strcpy(
    _Out_writes_z_(cchDest) wchar_t*    dest,
    _In_                 size_t         cchDest,
    _In_z_               const wchar_t* src
    );


/*@ MPIU_Szncpy - Copy a string into a fixed sized buffer; force null termination

    MPIU_Szncpy is a helper macro provided for copying into fixed sized char arrays.
    The macro computes the size (char count) of the dst array. Usage example,

    char buffer[333];
    ...
    -* copy max 333 chars into buffer; buffer will be null terminated. *-
    MPIU_Szncpy(buffer, str);

 @*/
#define MPIU_Szncpy(dst, src) MPIU_Strncpy(dst, src, _countof(dst))


/*@ MPIU_Strnapp - Append to a string with buffer size. Force null termination

    Input Parameters:
.   dst - String to copy into
+   src - String to append
-   n - 'dst' buffer size in chars (including null char)

    Output Parameter:
    pointer to the end terminating null char

    Notes:
    This routine is similar to 'strncat' except that the 'n' argument
    is the maximum total length of 'dst', rather than the maximum
    number of characters to move from 'src'.  Thus, this routine is
    easier to use when the declared size of 'src' is known.

    MPIU_Strnapp is implemented inline to help the compiler optimize
    per use instance.

  Module:
  Utility
  @*/
void MPIU_Strnapp(
        _Out_writes_z_(n) char *dst,
        _In_z_ const char *src,
        _In_ size_t n);


void MPIU_Strnapp(
    _Out_writes_z_(n) wchar_t *dst,
    _In_z_ const wchar_t *src,
    _In_ size_t n);


/*@ MPIU_Sznapp - Append a string into a fixed sized buffer; force null termination

    MPIU_Sznapp is a helper macro provided for appending into fixed sized char arrays.
    The macro computes the size (char count) of the dst array. Usage example,

    char buffer[333] = "Initial string";
    ...
    -* copy max 333 chars into buffer; buffer will be null terminated. *-
    MPIU_Sznapp(buffer, str);

 @*/
#define MPIU_Sznapp(dst, src) MPIU_Strnapp(dst, src, _countof(dst))


size_t MPIU_Strlen(
    _In_ PCSTR  src,
    _In_ size_t cchMax = STRSAFE_MAX_CCH );


size_t MPIU_Strlen(
    _In_ PCWSTR  src,
    _In_ size_t  cchMax = STRSAFE_MAX_CCH );


/* ---------------------------------------------------------------------- */
/* FIXME - The string routines do not belong in the memory header file  */
/* FIXME - The string error code such be MPICH2-usable error codes */
#define MPIU_STR_SUCCESS    0
#define MPIU_STR_FAIL      -1
#define MPIU_STR_NOMEM      1

/* FIXME: TRUE/FALSE definitions should either not be used or be
   used consistently.  These also do not belong in the mpimem header file. */
#define MPIU_TRUE  1
#define MPIU_FALSE 0

/* FIXME: Global types like this need to be discussed and agreed to */
typedef int MPIU_BOOL;

/* FIXME: These should be scoped to only the routines that need them */
#ifdef USE_HUMAN_READABLE_TOKENS

#define MPIU_STR_QUOTE_CHAR     '\"'
#define MPIU_STR_QUOTE_STR      "\""
#define MPIU_STR_DELIM_CHAR     '='
#define MPIU_STR_DELIM_STR      "="
#define MPIU_STR_ESCAPE_CHAR    '\\'
#define MPIU_STR_SEPAR_CHAR     ' '
#define MPIU_STR_SEPAR_STR      " "

#else

#define MPIU_STR_QUOTE_CHAR     '\"'
#define MPIU_STR_QUOTE_STR      "\""
#define MPIU_STR_DELIM_CHAR     '#'
#define MPIU_STR_DELIM_STR      "#"
#define MPIU_STR_ESCAPE_CHAR    '\\'
#define MPIU_STR_SEPAR_CHAR     '$'
#define MPIU_STR_SEPAR_STR      "$"

#endif

_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_get_string_arg(
    _In_opt_z_           const char* str,
    _In_opt_z_           const char* key,
    _Out_writes_z_(val_len) char*       val,
    _In_                 size_t      val_len
    );


_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_get_int_arg(
    _In_z_ const char *str,
    _In_z_ const char *flag,
    _Out_  int *val_ptr
    );


_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_add_string_arg(
    _Inout_ _Outptr_result_buffer_(*maxlen_ptr) PSTR*str_ptr,
    _Inout_ int *maxlen_ptr,
    _In_z_ const char *flag,
    _In_z_ const char *val
    );


_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_add_int_arg(
    _Inout_ _Outptr_result_buffer_(*maxlen_ptr) PSTR*str_ptr,
    _Inout_ int *maxlen_ptr,
    _In_z_ const char *flag,
    _In_ int val
    );


_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_add_string(
    _Inout_ _Outptr_result_buffer_(*maxlen_ptr) PSTR*str_ptr,
    _Inout_ int *maxlen_ptr,
    _In_z_ const char *val
    );


_Success_(return == 0)
int
MPIU_Str_get_string(
    _Inout_ _Outptr_result_maybenull_z_ PCSTR* str_ptr,
    _Out_writes_z_(val_len)char *val,
    _In_ size_t val_len
    );


//
// Provide a fallback snprintf for systems that do not have one
//
_Success_(return >= 0 && return <= cchDest)
int
MPIU_Snprintf(
    _Null_terminated_ _Out_writes_to_(cchDest, return) char* dest,
    _In_ size_t cchDest,
    _Printf_format_string_ const char* format,
    ...
    );


//
// Overloaded function for wide characters
//
_Success_(return >= 0 && return <= cchDest)
int
MPIU_Snprintf(
    _Null_terminated_ _Out_writes_to_(cchDest, return) wchar_t* dest,
    _In_ size_t cchDest,
    _Printf_format_string_ const wchar_t* format,
    ...
    );


//
// Provide vsnprintf functionality by using strsafe's StringCchVPrintfEx
//
_Success_(return >= 0 && return <= cchDest)
int
MPIU_Vsnprintf(
    _Null_terminated_ _Out_writes_to_(cchDest,return)char*     dest,
    _In_                 size_t      cchDest,
    _Printf_format_string_ const char* format,
    _In_                 va_list     args
    );


//
// Overloaded function for wide characters
// Provide vsnprintf functionality by using strsafe's StringCchVPrintfEx
//
_Success_(return >= 0 && return <= cchDest)
int
MPIU_Vsnprintf(
    _Null_terminated_ _Out_writes_to_(cchDest, return) wchar_t*       dest,
    _In_                 size_t         cchDest,
    _Printf_format_string_ const wchar_t* format,
    _In_                 va_list        args
    );


//
// Provide _strdup functionality
//
_Ret_valid_ _Null_terminated_
_Success_(return != nullptr)
char*
MPIU_Strdup(
    _In_z_ const char* str
    );


_Ret_valid_ _Null_terminated_
_Success_(return != nullptr)
wchar_t*
MPIU_Strdup(
    _In_z_ const wchar_t* str
    );


//
// Callee will need to call delete[] to free the memory allocated
// for wname_ptr if the function succeeds.
//
_Success_(return == NOERROR)
DWORD
MPIU_MultiByteToWideChar(
    _In_z_ const char* name,
    _Outptr_result_z_ wchar_t** wname_ptr
    );


//
// Callee will need to call delete[] to free the memory allocated
// for outputStr if the function succeeds.
//
_Success_(return == NOERROR)
DWORD
MPIU_WideCharToMultiByte(
    _In_z_ const wchar_t* str,
    _Outptr_result_z_ char** outputStr
    );
