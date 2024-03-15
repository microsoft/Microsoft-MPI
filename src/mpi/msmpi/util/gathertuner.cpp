// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "colltuner.h"

#define SWITCHADEFAULT L"MPICH_DEFAULT_GATHER_VSMALL_MSG"

GatherTuner::GatherTuner(
    unsigned int size,
    unsigned int time
    ) : CollectiveTuner(size, time, COLLECTIVE_HAS_TWO_ALGORITHMS)
{
}


const wchar_t* GatherTuner::Name() const
{
    return L"Gather";
}


void GatherTuner::PrintValues(FILE* pOutFile, const wchar_t* pFormat) const
{
    fwprintf(
        pOutFile,
        pFormat,
        SWITCHADEFAULT,
        Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_gather_vsmall_msg
        );
    fflush(pOutFile);
}


void GatherTuner::SetAlgorithm(unsigned int algorithm, CollectiveSwitchover* s)
{
    switch(algorithm)
    {
    case 0: /* short */
        s->MPIR_gather_vsmall_msg = UINT_MAX;
        break;
    case 1: /* long */
        s->MPIR_gather_vsmall_msg = 0;
        break;
    default:
        break;
    }
}


int GatherTuner::SetSwitchPoints(
    MPI_Comm comm,
    CollectiveSwitchover* pSwitchover,
    unsigned int measurementIndex
    )
{
    int mpi_errno;
    mpi_errno = NMPI_Bcast(&m_pSwitchA[measurementIndex], 1, MPI_INT, 0, comm);
    ON_ERROR_FAIL(mpi_errno);

    pSwitchover->MPIR_gather_vsmall_msg = m_pSwitchA[measurementIndex];

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


int GatherTuner::Measure(
    unsigned int    nElements,
    unsigned int    root,
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

    mpi_errno = MPIR_Gather_intra(
        m_pSendBuffer,
        len,
        g_hBuiltinTypes.MPI_Byte,
        m_pRecvBuffer,
        len,
        g_hBuiltinTypes.MPI_Byte,
        root,
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

bool GatherTuner::Enabled() const
{
    return Mpi.TunerSettings.TuneGather;
}
