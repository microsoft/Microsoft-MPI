// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

struct MpiCallState
{
private:
    ThreadHandle*   thread;

public:
    int             nest_count;
    int             op_errno;

    MpiCallState()
        : thread(nullptr)
        , nest_count(0)
        , op_errno(0)
    {
    }

    ~MpiCallState()
    {
        if (nullptr != thread)
        {
            thread->Release();
        }
    }

    int OpenThreadHandle(_Outptr_result_maybenull_ ThreadHandle** ppHandle)
    {
        int result = MPI_SUCCESS;
        if (nullptr == thread)
        {
            thread = new ThreadHandle;
            if (nullptr == thread)
            {
                result = MPIU_ERR_NOMEM();
                goto exit_fn;
            }

            result = thread->Initialize();
            if (MPI_SUCCESS != result)
            {
                thread->Release();
                thread = nullptr;
                goto exit_fn;
            }
        }

        thread->AddRef();
exit_fn:
        *ppHandle = thread;
        return result;
    }

private:
    MpiCallState(const MpiCallState& other);
    MpiCallState& operator = (const MpiCallState& other);
};




