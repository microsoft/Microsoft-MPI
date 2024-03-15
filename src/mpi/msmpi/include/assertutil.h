// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

//
//
// Summary:
//  This file contains the assert utility macros.
//    Note: the "Invariant" versions that are included in retail.
//
//   InvariantAssert/Assert(exp_) :
//      This uses interupt 2c, which enables new assert break and skip features.
//          If this assert fires, it will raise a fatal error and crash the host process.
//          If AeDebug key is set, this will cause the debugger to launch
//          When debugger attached, the 'ahi' command can be used to ignore the assertion and continue.
//        NOTE: Unlike the CRT assert, the strings for these asserts are stored in the symbols,
//              so asserts do not add strings to the image.
//
//   InvariantAssertP/AssertP(exp_) :
//      This is the same as the InvariantAssert/Assert macro, except that it will only passively fire
//          when a debugger is actually attached.
//
//

#define InvariantAssert(exp_) \
    ((!(exp_)) ? \
        (__annotation(L"Debug", L"AssertFail", L#exp_), \
         __int2c(), FALSE) : \
        TRUE)

#define InvariantAssertP( exp_ ) \
    ((!(exp_) && IsDebuggerPresent()) ? \
        (__annotation(L"Debug", L"PassiveAssertFail", L#exp_), \
         __int2c(), FALSE) : \
        TRUE)

#if DBG
#  define Assert(exp_)      __analysis_assume(exp_);InvariantAssert(exp_)
#  define AssertP( exp_ )   __analysis_assume(exp_);InvariantAssertP(exp_)
#else
#  define Assert( exp_ )  __analysis_assume(exp_)
#  define AssertP( exp_ ) __analysis_assume(exp_)
#endif
