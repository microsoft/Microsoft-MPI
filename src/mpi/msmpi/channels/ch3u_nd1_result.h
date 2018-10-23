// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*++

ch3u_nd_result.h - Network Direct MPI CH3 Channel endpoint object

--*/

#pragma once

#ifndef CH3U_NDV1_RESULT_H
#define CH3U_NDV1_RESULT_H

namespace CH3_ND
{
namespace v1
{
    //
    // nd_result
    //
    // Description:
    //  Base class for various ND results.  Derived objects use the function
    //  pointers to handle completion processing.
    //
    struct nd_result_t : public ND_RESULT
    {
        typedef int
        (*CompletionRoutine)(
            __in struct nd_result_t* pResult,
            __out bool* pfMpiRequestDone
            );

        CompletionRoutine pfnSucceeded;
        CompletionRoutine pfnFailed;
    };

}   // namespace v1
}   // namespace CH3_ND

#endif // CH3U_NDV1_RESULT_H
