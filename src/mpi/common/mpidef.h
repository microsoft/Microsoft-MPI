// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#if !defined(MPITYPEDEFS_H_INCLUDED)
#define MPITYPEDEFS_H_INCLUDED

/* Define if addresses are larger than Fortran integers */
#ifdef _WIN64
#define HAVE_AINT_LARGER_THAN_FINT
#endif

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define if debugger support is included */
/* #undef HAVE_DEBUGGER_SUPPORT */

/* Define if F90 type routines available */
/* #undef HAVE_F90_TYPE_ROUTINES */

/* Define if Fortran is supported */
/* Defined as a compiler directive, see mpich2sources.inc */
/* #undef HAVE_FORTRAN_BINDING */

/* Controls byte alignment of structures (for aligning allocated structures)
   */
#define HAVE_MAX_STRUCT_ALIGNMENT 8

/* Define if a name publishing service is available */
#define HAVE_NAMEPUB_SERVICE 1

/* Define if the Fortran types are not available in C */
/* #undef HAVE_NO_FORTRAN_MPI_TYPES_IN_C */

/* Define as the name of the debugger support library */
/* #undef MPICH_INFODLL_LOC */

/* Level of thread support selected at compile time */
#ifdef MPICH_MULTITHREADED
#define MPICH_THREAD_LEVEL MPI_THREAD_MULTIPLE
#else
#define MPICH_THREAD_LEVEL MPI_THREAD_SERIALIZED
#endif

/* C type to use for MPI_INTEGER16 */
/* #undef MPIR_INTEGER16_CTYPE */

/* C type to use for MPI_REAL2 */
/* #undef MPIR_REAL2_CTYPE */

/* C type to use for MPI_REAL16 */
/* #undef MPIR_REAL16_CTYPE */

/* C99 types available for MPI_C_XXX */
/* #undef MPIR_C99_TYPES */

/* Define if alloca should be used if available */
/* #undef USE_ALLOCA */

/* Define to use ='s and spaces in the string utilities. */
#define USE_HUMAN_READABLE_TOKENS 1

/* if C does not support restrict */
#define restrict

/* The following are defined as a compiler directive, see mpich2sources.inc */
/* Added here to make it easier to search for the define */
/*
#define USE_MPI_FOR_NMPI
#define HAVE_FORTRAN_BINDING
#define MPIDI_CH3_HAS_NO_DYNAMIC_PROCESS
*/


#ifndef EXTERN_C
#if defined(__cplusplus)
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif
#endif


#ifdef _PREFIX_
EXTERN_C void __pfx_assert(bool, const char*);
#undef __analysis_assert
#define __analysis_assert(expr) __pfx_assert(expr, "")
#else
#ifndef __analysis_assert
#define __analysis_assert(expr)
#endif
#endif


#ifdef _PREFIX_
EXTERN_C void __pfx_assume(bool, const char*);
#undef __analysis_assume
#define __analysis_assume(expr) __pfx_assume(expr, "")
#elif _PREFAST_
#undef __analysis_assume
#define __analysis_assume(expr) __assume(expr)
#else
#ifndef __analysis_assume
#define __analysis_assume(expr)
#endif
#endif

#ifdef __midl
typedef int MPI_RESULT;
#else
typedef _Check_return_ _Return_type_success_(return == MPI_SUCCESS) int MPI_RESULT;
#endif


/* Set to a type that can express the size of a send/receive buffer */
typedef unsigned int MPIU_Bsize_t;
#define MPIU_BSIZE_MAX UINT_MAX

typedef MPIU_Bsize_t MPIDI_msg_sz_t;
#define MPIDI_MSG_SZ_MAX MPIU_BSIZE_MAX


/* Use the MPIU_PtrToXXX macros to convert pointers to and from integer types */

/* The Microsoft compiler will not allow casting of different sized types
 * without
 * printing a compiler warning.  Using these macros allows compiler specific
 * type casting and avoids the warning output.  These macros should only be used
 * in code that can handle loss of bits.
 */

/* PtrToInt converts a pointer to a int type, truncating bits if necessary */
#define MPIU_PtrToInt(p) ((INT)(INT_PTR) (p) )

/* PtrToAint converts a pointer to an MPI_Aint type, truncating bits if necessary */
#define MPIU_PtrToAint(a) ((MPI_Aint)(INT_PTR) (a) )

/* IntToPtr converts a int to a pointer type, extending bits if necessary */
#define MPIU_IntToPtr(i) ((VOID *)(INT_PTR)((int)i))


//
// Constant used to define the maximum number of characters in the hostname
// string placed into the PMI KVS when publishing our node id.
//
#if (!defined MAXHOSTNAMELEN) && (!defined MAX_HOSTNAME_LEN)
#define MAX_HOSTNAME_LEN 256
#elif !defined MAX_HOSTNAME_LEN
#define MAX_HOSTNAME_LEN MAXHOSTNAMELEN
#endif

#define MSMPI_VER_MAJOR( _v ) (_v >> 24)
#define MSMPI_VER_MINOR( _v ) ((_v >> 16) & 0xFF)
#define MSMPI_VER_BUILD( _v ) (_v & 0xFFFF)
#if MSMPI_IS_RTM
#define MSMPI_BUILD_LABEL   L""
#else
#define MSMPI_BUILD_LABEL   L" [PRE-RELEASE]"
#endif

//
// This is the maximum length of an environment variable according
// to platform specification.
//
#define MAX_ENV_LENGTH            32767

#define MSMPI_MAX_RANKS           32768

#define MSMPI_MAX_TRANSFER_SIZE   INT_MAX

//
// Maximum number of connection retires in case of errors.  Applies to both sockets and ND.
//
#define MSMPI_DEFAULT_CONNECT_RETRIES   5

//
// This string identifies the service_name in spn construction
//
#define MSMPI_SPN_SERVICE_NAME L"msmpi"

//
// Cast definition to help flag places we use potentially truncate but shouldn't.
//
#define fixme_cast static_cast

#endif /* !defined(MPITYPEDEFS_H_INCLUDED) */
