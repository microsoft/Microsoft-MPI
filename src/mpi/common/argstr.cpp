// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *   Copyright (C) 1997 University of Chicago.
 *   See COPYRIGHT notice in top-level directory.
 */
#include "precomp.h"


#define MPIU_STR_TRUNCATED MPIU_STR_NOMEM

_Success_(return != nullptr)
_Ret_z_
static const char *
first_token(
    _In_opt_z_ const char *str
    )
{
    if (str == nullptr)
        return nullptr;
    /* isspace is defined only if isascii is true */
    while (/*isascii(*str) && isspace(*str)*/ *str == MPIU_STR_SEPAR_CHAR)
        str++;
    if (*str == '\0')
        return nullptr;
    return str;
}


_Ret_maybenull_z_
static const char*
next_token(
    _In_opt_z_ const char *str
    )
{
    if (str == nullptr)
        return nullptr;
    str = first_token(str);
    if (str == nullptr)
        return nullptr;
    if (*str == MPIU_STR_QUOTE_CHAR)
    {
        /* move over string */
        str++; /* move over the first quote */
        if (*str == '\0')
            return nullptr;
        while (*str != MPIU_STR_QUOTE_CHAR)
        {
            /* move until the last quote, ignoring escaped characters */
            if (*str == MPIU_STR_ESCAPE_CHAR)
            {
                str++;
            }
            str++;
            if (*str == '\0')
                return nullptr;
        }
        str++; /* move over the last quote */
    }
    else
    {
        if (*str == MPIU_STR_DELIM_CHAR)
        {
            /* move over the DELIM token */
            str++;
        }
        else
        {
            /* move over literal */
            while (/*(isascii(*str) &&
                    !isspace(*str)) &&*/
                    *str != MPIU_STR_SEPAR_CHAR &&
                   *str != MPIU_STR_DELIM_CHAR &&
                   *str != '\0')
                str++;
        }
    }
    return first_token(str);
}

static int
compare_token(
    _In_opt_z_ const char *token,
    _In_opt_z_ const char *str
    )
{
    if (token == nullptr || str == nullptr)
        return -1;

    if (*token == MPIU_STR_QUOTE_CHAR)
    {
        /* compare quoted strings */
        token++; /* move over the first quote */
        /* compare characters until reaching the end of the string or the end quote character */
        for(;;)
        {
            if (*token == MPIU_STR_ESCAPE_CHAR)
            {
                token++;
                if (*token != *str)
                    break;
            }
            else
            {
                if (*token != *str || *token == MPIU_STR_QUOTE_CHAR)
                    break;
            }
            if (*str == '\0')
                break;
            token++;
            str++;
        }
        if (*str == '\0' && *token == MPIU_STR_QUOTE_CHAR)
            return 0;
        if (*token == MPIU_STR_QUOTE_CHAR)
            return 1;
        if (*str < *token)
            return -1;
        return 1;
    }

    /* compare DELIM token */
    if (*token == MPIU_STR_DELIM_CHAR)
    {
        if (*str == MPIU_STR_DELIM_CHAR)
        {
            str++;
            if (*str == '\0')
                return 0;
            return 1;
        }
        if (*token < *str)
            return -1;
        return 1;
    }

    /* compare literals */
    while (*token == *str &&
           *str != '\0' &&
           *token != MPIU_STR_DELIM_CHAR &&
           (/*isascii(*token) && !isspace(*token)*/ *token != MPIU_STR_SEPAR_CHAR) )
    {
        token++;
        str++;
    }
    if ( (*str == '\0') &&
         (*token == MPIU_STR_DELIM_CHAR ||
          (/*isascii(*token) && isspace(*token)*/ *token == MPIU_STR_SEPAR_CHAR) || *token == '\0') )
        return 0;
    if (*token == MPIU_STR_DELIM_CHAR ||
        (/*isascii(*token) && isspace(*token)*/ *token == MPIU_STR_SEPAR_CHAR) || *token < *str)
        return -1;
    return 1;
}

_Success_(return == MPIU_STR_SUCCESS)
static int
token_copy(
    _In_opt_z_ const char *token,
    _Out_writes_z_(maxlen) char *str,
    _In_ size_t maxlen
    )
{
    /* check parameters */
    if (token == nullptr || str == nullptr)
        return MPIU_STR_FAIL;

    /* check special buffer lengths */
    if (maxlen < 1)
        return MPIU_STR_FAIL;
    if (maxlen == 1)
    {
        *str = '\0';
        return (token[0] == '\0') ? MPIU_STR_SUCCESS : MPIU_STR_TRUNCATED;
    }

    /* cosy up to the token */
    token = first_token(token);
    if (token == nullptr)
    {
        *str = '\0';
        return MPIU_STR_SUCCESS;
    }

    if (*token == MPIU_STR_DELIM_CHAR)
    {
        /* copy the special deliminator token */
        str[0] = MPIU_STR_DELIM_CHAR;
        str[1] = '\0';
        return MPIU_STR_SUCCESS;
    }

    if (*token == MPIU_STR_QUOTE_CHAR)
    {
        /* quoted copy */
        token++; /* move over the first quote */

        do
        {
            if (*token == MPIU_STR_QUOTE_CHAR)
            {
                *str = '\0';
                return MPIU_STR_SUCCESS;
            }

            if (*token == MPIU_STR_ESCAPE_CHAR)
            {
                token++;
            }

            if( *token == '\0' )
            {
                //
                // Bad input - we're expecting a closing quote
                //
                return MPIU_STR_FAIL;
            }

            *str = *token;
            str++;
            token++;

        } while( --maxlen > 0 );

        /* we've run out of destination characters so back up and null terminate the string */
        str--;
        *str = '\0';
        return MPIU_STR_TRUNCATED;
    }

    /* literal copy */
    while (*token != MPIU_STR_DELIM_CHAR &&
           (/*isascii(*token) && !isspace(*token)*/ *token != MPIU_STR_SEPAR_CHAR) && *token != '\0' && maxlen)
    {
        *str = *token;
        str++;
        token++;
        maxlen--;
    }
    if (maxlen)
    {
        *str = '\0';
        return MPIU_STR_SUCCESS;
    }
    str--;
    *str = '\0';
    return MPIU_STR_TRUNCATED;
}


/*@ MPIU_Str_get_string_arg - Extract an option from a string with a maximum length

    Input Parameters:
+   str - Source string
.   key - key
-   val_len - Maximum total length of 'val'

    Output Parameter:
.   val - output string

    Return value:
    MPIU_STR_SUCCESS, MPIU_STR_NOMEM, MPIU_STR_FAIL

    Notes:
    This routine searches for a "key = value" entry in a string

  Module:
  Utility
  @*/
_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_get_string_arg(
    _In_opt_z_           const char* str,
    _In_opt_z_           const char* flag,
    _Out_writes_z_(val_len) char*    val,
    _In_                 size_t      val_len
    )
{
    if (val_len < 1)
        return MPIU_STR_FAIL;

    /* line up with the first token */
    str = first_token(str);
    if (str == nullptr)
        return MPIU_STR_FAIL;

    /* This loop will match the first instance of "flag = value" in the string. */
    do
    {
        if (compare_token(str, flag) == 0)
        {
            str = next_token(str);
            if (compare_token(str, MPIU_STR_DELIM_STR) == 0)
            {
                str = next_token(str);
                if (str == nullptr)
                    return MPIU_STR_FAIL;
                return token_copy(str, val, val_len);
            }
        }
        else
        {
            str = next_token(str);
        }
    } while (str);
    return MPIU_STR_FAIL;
}


/*@ MPIU_Str_get_int_arg - Extract an option from a string

    Input Parameters:
+   str - Source string
-   key - key

    Output Parameter:
.   val_ptr - pointer to the output integer

    Return value:
    MPIU_STR_SUCCESS, MPIU_STR_NOMEM, MPIU_STR_FAIL

    Notes:
    This routine searches for a "key = value" entry in a string and decodes the value
    back to an int.

  Module:
  Utility
  @*/
_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_get_int_arg(
    _In_z_ const char *str,
    _In_z_ const char *flag,
    _Out_  int *val_ptr
    )
{
    int result;
    char int_str[12];

    result = MPIU_Str_get_string_arg(str, flag, int_str, _countof(int_str));
    if (result == MPIU_STR_SUCCESS)
    {
        *val_ptr = atoi(int_str);
        return MPIU_STR_SUCCESS;
    }
    return result;
}

/* quoted_printf does not nullptr terminate the string if maxlen is reached */
_Success_(return < maxlen)
static
int
quoted_printf(
    _Out_writes_z_(maxlen) char *str,
    _In_ int maxlen,
    _In_z_ const char *val)
{
    int count = 0;
    if (maxlen < 1)
        return 0;
    *str = MPIU_STR_QUOTE_CHAR;
    str++;
    maxlen--;
    count++;
    while (maxlen)
    {
        if (*val == '\0')
            break;
        if (*val == MPIU_STR_QUOTE_CHAR || *val == MPIU_STR_ESCAPE_CHAR)
        {
            *str = MPIU_STR_ESCAPE_CHAR;
            str++;
            maxlen--;
            count++;
            if (maxlen == 0)
                return count;
        }
        *str = *val;
        str++;
        maxlen--;
        count++;
        val++;
    }
    if (maxlen)
    {
        *str = MPIU_STR_QUOTE_CHAR;
        str++;
        maxlen--;
        count++;
        if (maxlen == 0)
            return count;
        *str = '\0';
    }
    return count;
}

/*@ MPIU_Str_add_string - Add a string to a string

    Input Parameters:
+   str_ptr - pointer to the destination string
.   maxlen_ptr - pointer to the maximum length of '*str_ptr'
-   val - string to add

    Output Parameter:
+   str_ptr - The string pointer is updated to the next available location in the string
-   maxlen_ptr - maxlen is decremented by the amount str_ptr is incremented

    Return value:
    MPIU_STR_SUCCESS, MPIU_STR_NOMEM, MPIU_STR_FAIL

    Notes:
    This routine adds a string to a string in such a way that MPIU_Str_get_string can
    retreive the same string back.  It takes into account spaces and quote characters.
    The string pointer is updated to the start of the next string in the string and maxlen
    is updated accordingly.

  Module:
  Utility
  @*/
_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_add_string(
    _Inout_ _Outptr_result_buffer_(*maxlen_ptr) PSTR* str_ptr,
    _Inout_ int *maxlen_ptr,
    _In_z_ const char *val
    )
{
    int num_chars;
    char *str;
    int maxlen;

    str = *str_ptr;
    maxlen = *maxlen_ptr;

    if (strchr(val, MPIU_STR_SEPAR_CHAR) ||
        strchr(val, MPIU_STR_QUOTE_CHAR) ||
        strchr(val, MPIU_STR_DELIM_CHAR))
    {
        num_chars = quoted_printf(str, maxlen, val);
        if (num_chars == maxlen)
        {
            /* truncation, cleanup string */
            *str = '\0';
            return -1;
        }
        if (num_chars < maxlen - 1)
        {
            str[num_chars] = MPIU_STR_SEPAR_CHAR;
            str[num_chars+1] = '\0';
            num_chars++;
        }
        else
        {
            str[num_chars] = '\0';
        }
    }
    else
    {
        if (*val == '\0')
        {
            num_chars = MPIU_Snprintf(str, maxlen, MPIU_STR_QUOTE_STR MPIU_STR_QUOTE_STR/*"\"\""*/);
        }
        else
        {
            num_chars = MPIU_Snprintf(str, maxlen, "%s%c", val, MPIU_STR_SEPAR_CHAR);
        }
        if ((num_chars < 0) || (num_chars == maxlen))
        {
            *str = '\0';
            return -1;
        }
    }
    *str_ptr += num_chars;
    *maxlen_ptr -= num_chars;
    return 0;
}

/*@ MPIU_Str_get_string - Get the next string from a string

    Input Parameters:
+   str_ptr - pointer to the destination string
-   val_len - to the maximum length of '*str_ptr'

    Output Parameter:
+   str_ptr - location of the next string
-   val - location to store the string

    Return Value:
    The return value is 0 for success, -1 for insufficient buffer space, and -2 for failure.

    Notes:
    This routine gets a string that was previously added by MPIU_Str_add_string.
    It takes into account spaces and quote characters. The string pointer is updated to the
    start of the next string in the string.

  Module:
  Utility
  @*/

_Success_(return == 0)
int
MPIU_Str_get_string(
    _Inout_ _Outptr_result_maybenull_z_ PCSTR* str_ptr,
    _Out_writes_z_(val_len)char *val,
    _In_ size_t val_len
    )
{
    int result;
    PCSTR str;

    if (str_ptr == nullptr)
    {
        return -2;
    }

    if (val_len < 1)
    {
        return -2;
    }

    str = *str_ptr;
    *val = '\0';

    /* line up with the first token */
    str = first_token(str);
    if (str == nullptr)
    {
        return 0;
    }

    /* copy the token */
    result = token_copy(str, val, val_len);
    if (result == MPIU_STR_SUCCESS)
    {
        str = next_token(str);
        *str_ptr = str;
        return 0;
    }
    else if (result == MPIU_STR_TRUNCATED)
    {
        return -1;
    }

    /* failure */
    return -2;
}

/*@ MPIU_Str_add_string_arg - Add an option to a string with a maximum length

    Input Parameters:
+   str_ptr - Pointer to the destination string
.   maxlen_ptr - Pointer to the maximum total length of '*str_ptr'
.   key - key
-   val - input string

    Output Parameter:
+   str_ptr - The string pointer is updated to the next available location in the string
-   maxlen_ptr - maxlen is reduced by the number of characters written

    Return value:
    MPIU_STR_SUCCESS, MPIU_STR_NOMEM, MPIU_STR_FAIL

    Notes:
    This routine adds a string option to a string in the form "key = value".

  Module:
  Utility
  @*/
_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_add_string_arg(
    _Inout_ _Outptr_result_buffer_(*maxlen_ptr) PSTR* str_ptr,
    _Inout_ int *maxlen_ptr,
    _In_z_ const char *flag,
    _In_z_ const char *val
    )
{
    int num_chars;
    char *orig_str_ptr;
    int orig_maxlen;

    if (maxlen_ptr == nullptr)
        return MPIU_STR_FAIL;

    orig_maxlen = *maxlen_ptr;
    orig_str_ptr = *str_ptr;

    if (*maxlen_ptr < 1)
        return MPIU_STR_FAIL;

    /* add the flag */
/*     printf("strstr flag\n"); */
    if (strstr(flag, MPIU_STR_SEPAR_STR) || strstr(flag, MPIU_STR_DELIM_STR) || flag[0] == MPIU_STR_QUOTE_CHAR)
    {
        num_chars = quoted_printf(*str_ptr, *maxlen_ptr, flag);
    }
    else
    {
        num_chars = MPIU_Snprintf(*str_ptr, *maxlen_ptr, "%s", flag);
    }

    if(num_chars < 0)
    {
        num_chars = *maxlen_ptr;
    }

    *maxlen_ptr = *maxlen_ptr - num_chars;
    if (*maxlen_ptr < 1)
    {
        **str_ptr = '\0';
        /*(*str_ptr)[num_chars-1] = '\0';*/
        return MPIU_STR_NOMEM;
    }
    *str_ptr = *str_ptr + num_chars;

    /* add the deliminator character */
    **str_ptr = MPIU_STR_DELIM_CHAR;
    *str_ptr = *str_ptr + 1;
    *maxlen_ptr = *maxlen_ptr - 1;

    /* add the value string */
/*     printf("strstr val\n"); */
    if (strstr(val, MPIU_STR_SEPAR_STR) || strstr(val, MPIU_STR_DELIM_STR) || val[0] == MPIU_STR_QUOTE_CHAR)
    {
        num_chars = quoted_printf(*str_ptr, *maxlen_ptr, val);
    }
    else
    {
        if (*val == '\0')
        {
            num_chars = MPIU_Snprintf(*str_ptr, *maxlen_ptr, MPIU_STR_QUOTE_STR MPIU_STR_QUOTE_STR/*"\"\""*/);
        }
        else
        {
            num_chars = MPIU_Snprintf(*str_ptr, *maxlen_ptr, "%s", val);
        }
    }

    if(num_chars < 0)
    {
        num_chars = *maxlen_ptr;
    }

    *str_ptr = *str_ptr + num_chars;
    *maxlen_ptr = *maxlen_ptr - num_chars;
    if (*maxlen_ptr < 2)
    {
        *orig_str_ptr = '\0';
        *str_ptr = orig_str_ptr;
        *maxlen_ptr = orig_maxlen;
        return MPIU_STR_NOMEM;
    }

    /* add the trailing space */
    **str_ptr = MPIU_STR_SEPAR_CHAR;
    *str_ptr = *str_ptr + 1;
    **str_ptr = '\0';
    *maxlen_ptr = *maxlen_ptr - 1;

    return MPIU_STR_SUCCESS;
}

/*@ MPIU_Str_add_int_arg - Add an option to a string with a maximum length

    Input Parameters:
+   str_ptr - Pointer to the destination string
.   maxlen_ptr - Pointer to the maximum total length of '*str_ptr'
.   key - key
-   val - input integer

    Output Parameter:
+   str_ptr - The string pointer is updated to the next available location in the string
-   maxlen_ptr - maxlen is reduced by the number of characters written

    Return value:
    MPIU_STR_SUCCESS, MPIU_STR_NOMEM, MPIU_STR_FAIL

    Notes:
    This routine adds an integer option to a string in the form "key = value".

  Module:
  Utility
  @*/
_Success_(return == MPIU_STR_SUCCESS)
int
MPIU_Str_add_int_arg(
    _Inout_ _Outptr_result_buffer_(*maxlen_ptr) PSTR* str_ptr,
    _Inout_ int *maxlen_ptr,
    _In_z_ const char *flag,
    _In_ int val
    )
{
    char val_str[12];
    MPIU_Snprintf(val_str, 12, "%d", val);
    return MPIU_Str_add_string_arg(str_ptr, maxlen_ptr, flag, val_str);
}
