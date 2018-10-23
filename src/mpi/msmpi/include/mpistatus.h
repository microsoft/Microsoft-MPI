// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef STATUS_H_INCLUDED
#define STATUS_H_INCLUDED


//
// Internal struct that overlays MPI_Status.
//
#pragma pack( push, 4 )
struct MPIR_Status
{
    unsigned __int64 count : 63;
    unsigned __int64 cancelled : 1;
    int MPI_SOURCE;
    int MPI_TAG;
    int MPI_ERROR;
};
#pragma pack( pop )
C_ASSERT(
    sizeof( MPIR_Status ) == sizeof( MPI_Status ) &&
    __alignof( MPIR_Status) == __alignof( MPI_Status )
    );


static inline void
MPIR_Status_set_count(
    _Inout_ MPI_Status *status,
    _In_range_(>=, 0) MPI_Count count
    )
{
    reinterpret_cast<MPIR_Status*>( status )->count = count;
}


static inline MPI_Count
MPIR_Status_get_count(
    _In_ const MPI_Status *status
    )
{
    return reinterpret_cast<const MPIR_Status*>( status )->count;
}


static inline void
MPIR_Status_set_cancelled(
    _Inout_ MPI_Status *status,
    _In_range_(0, 1) int cancelled
    )
{
    MPIU_Assert( cancelled == TRUE || cancelled == FALSE );
    reinterpret_cast<MPIR_Status*>( status )->cancelled = cancelled;
}


static inline int
MPIR_Status_get_cancelled(
    _In_ const MPI_Status *status
    )
{
    return reinterpret_cast<const MPIR_Status*>( status )->cancelled;
}


static inline void
MPI_Status_clear_with_source(
    _Inout_ MPI_Status *status,
    _In_ int source
    )
{
    if( status != MPI_STATUS_IGNORE )
    {
        //
        // Do not set MPI_ERROR (only set if MPI_ERR_IN_STATUS is returned).
        // According to the MPI 1.1 standard page 22 lines 9-12, the MPI_ERROR
        // field may not be modified except by the functions in section 3.7.5
        // which return MPI_ERR_IN_STATUS (MPI_Wait{all,some} and MPI_Test{all,some}).
        //
        MPIR_Status_set_count( status, 0 );
        MPIR_Status_set_cancelled( status, FALSE );
        status->MPI_SOURCE = source;
        status->MPI_TAG = MPI_ANY_TAG;
    }
}


static inline void
MPIR_Status_set_empty(
    _Inout_ MPI_Status *status
    )
{
    MPI_Status_clear_with_source( status, MPI_ANY_SOURCE );
}


static inline void
MPIR_Status_set_error(
    _Inout_ MPI_Status *status,
    _In_ int err
    )
{
    if( status != MPI_STATUS_IGNORE )
    {
        status->MPI_ERROR = err;
    }
}


//
// See MPI 1.1, section 3.11, Null Processes.
//
static inline void
MPIR_Status_set_procnull(
    _Inout_ MPI_Status *status
    )
{
    MPI_Status_clear_with_source( status, MPI_PROC_NULL );
}


#endif // STATUS_H_INCLUDED

