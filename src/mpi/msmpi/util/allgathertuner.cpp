// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "colltuner.h"

#define SWITCHADEFAULT L"MPICH_DEFAULT_ALLGATHER_SHORT_MSG"
#define SWITCHBDEFAULT L"MPICH_DEFAULT_ALLGATHER_LONG_MSG"

AllgatherTuner::AllgatherTuner(
    unsigned int size,
    unsigned int time
    ) : CollectiveTuner(size, time, COLLECTIVE_HAS_THREE_ALGORITHMS)
{
}


const wchar_t* AllgatherTuner::Name() const
{
    return L"Allgather";
}


void AllgatherTuner::PrintValues(FILE* pOutFile, const wchar_t* pFormat) const
{
    fwprintf(
        pOutFile,
        pFormat,
        SWITCHADEFAULT,
        Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_allgather_short_msg
        );

    fwprintf(
        pOutFile,
        pFormat,
        SWITCHBDEFAULT,
        Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_allgather_long_msg
        );
    fflush(pOutFile);
}


void AllgatherTuner::SetAlgorithm(unsigned int algorithm, CollectiveSwitchover* s)
{
    switch(algorithm)
    {
    case 0: /* short */
        s->MPIR_allgather_short_msg = UINT_MAX;
        break;
    case 1: /* medium */
        s->MPIR_allgather_short_msg = 0;
        s->MPIR_allgather_long_msg = UINT_MAX;
        break;
    case 2:/* long */
        s->MPIR_allgather_short_msg = 0;
        s->MPIR_allgather_long_msg = 0;
        break;
    default:
        break;
    }
}


int AllgatherTuner::SetSwitchPoints(
    MPI_Comm comm,
    CollectiveSwitchover* pSwitchover,
    unsigned int measurementIndex
    )
{
    int mpi_errno;
    mpi_errno = NMPI_Bcast(&m_pSwitchA[measurementIndex], 1, MPI_INT, 0, comm);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = NMPI_Bcast(&m_pSwitchB[measurementIndex], 1, MPI_INT, 0, comm);
    ON_ERROR_FAIL(mpi_errno);

    pSwitchover->MPIR_allgather_short_msg = m_pSwitchA[measurementIndex];
    pSwitchover->MPIR_allgather_long_msg = m_pSwitchB[measurementIndex];

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


int AllgatherTuner::Measure(
    unsigned int    nElements,
    unsigned int    /*root*/,
    LARGE_INTEGER* pStart,
    LARGE_INTEGER* pStop,
    MPID_Comm*     pComm
    )
{
    BOOL startMeasurementSucceeded = FALSE;
    BOOL stopMeasurementSucceeded = FALSE;
    int mpi_errno = MPI_SUCCESS;
    unsigned int len = nElements / pComm->remote_size;
    startMeasurementSucceeded = QueryPerformanceCounter(pStart);

    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    mpi_errno = MPIR_Allgather_intra(
        m_pSendBuffer,
        len,
        g_hBuiltinTypes.MPI_Byte,
        m_pRecvBuffer,
        len,
        g_hBuiltinTypes.MPI_Byte,
        pComm
        );
    stopMeasurementSucceeded = QueryPerformanceCounter(pStop);
    if(startMeasurementSucceeded == FALSE || stopMeasurementSucceeded == FALSE)
    {
        if(mpi_errno == MPI_SUCCESS)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**measurementfailed");
        }
    }
    return mpi_errno;
}

bool AllgatherTuner::Enabled() const
{
    return Mpi.TunerSettings.TuneAllgather;
}
