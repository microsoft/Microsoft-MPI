// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef OACR_INC_H
#define OACR_INC_H

#define HRESULT_NOT_CHECKED 25031
#define COMPARING_HRESULT_TO_INT 6221
#define RETVAL_IGNORED_FUNC_COULD_FAIL 6031
#define NONCONST_BUFFER_PARAM 25033
#define EXCEPT_BLOCK_EMPTY 6322
#define PRINTF_FORMAT_STRING_PARAM_NEEDS_REVIEW 25141
#define UNSAFE_STRING_FUNCTION 25025
#define USE_WIDE_API 25068
#define DIFFERENT_PARAM_TYPE_SIZE 25054

#define OACR_REVIEWED_CALL( reviewer, functionCall ) functionCall
#define OACR_WARNING_SUPPRESS( cWarning, comment ) __pragma ( warning( suppress: cWarning ) )
#define OACR_WARNING_ENABLE( cWarning, comment ) __pragma ( warning( default: cWarning ) )
#define OACR_USE_PTR(p) __noop
#define OACR_WARNING_DISABLE( cWarning, comment ) __pragma(warning(disable:cWarning))
#endif