// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "colltuner.h"

#define SWITCHADEFAULT L"MPICH_DEFAULT_REDUCE_SHORT_MSG"
#define SWITCHAINTRA L"MPICH_INTRANODE_REDUCE_SHORT_MSG"
#define SWITCHAINTER L"MPICH_INTERNODE_REDUCE_SHORT_MSG"
#define THRESHOLD L"MPICH_DEFAULT_REDUCE_SMP_THRESHOLD"

ReduceTuner::ReduceTuner(
    unsigned int size,
    unsigned int time
    ) : CollectiveTuner(size, time, COLLECTIVE_HAS_TWO_ALGORITHMS)
{
}


const wchar_t* ReduceTuner::Name() const
{
    return L"Reduce";
}


void ReduceTuner::PrintValues(FILE* pOutFile, const wchar_t* pFormat) const
{
    fwprintf(
        pOutFile,
        pFormat,
        SWITCHADEFAULT,
        Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_reduce_short_msg
        );
    if(SmpEnabled())
    {
        fwprintf(
            pOutFile,
            pFormat,
            SWITCHAINTRA,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_INTRA_NODE].MPIR_reduce_short_msg
            );
        fwprintf(
            pOutFile,
            pFormat,
            SWITCHAINTER,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_INTER_NODE].MPIR_reduce_short_msg
            );
        fwprintf(
            pOutFile,
            pFormat,
            THRESHOLD,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_reduce_smp_threshold
            );
    }
    fflush(pOutFile);
}


void ReduceTuner::SetAlgorithm(unsigned int algorithm, CollectiveSwitchover* s)
{
    switch(algorithm)
    {
    case 0: /* short */
        s->MPIR_reduce_short_msg = UINT_MAX;
        break;
    case 1: /* long */
        s->MPIR_reduce_short_msg = 0;
        break;
    default:
        break;
    }
}


int ReduceTuner::SetSwitchPoints(
    MPI_Comm comm,
    CollectiveSwitchover* pSwitchover,
    unsigned int measurementIndex
    )
{
    int mpi_errno;
    mpi_errno = NMPI_Bcast(&m_pSwitchA[measurementIndex], 1, MPI_UNSIGNED, 0, comm);
    ON_ERROR_FAIL(mpi_errno);

    pSwitchover->MPIR_reduce_short_msg = m_pSwitchA[measurementIndex];

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


int ReduceTuner::Measure(
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
    if(m_UseInner)
    {
        startMeasurementSucceeded = QueryPerformanceCounter(pStart);
        mpi_errno = MPIR_Reduce_intra_flat(
            m_pSendBuffer,
            m_pRecvBuffer,
            nElements,
            g_hBuiltinTypes.MPI_Byte,
            OpPool::Get( MPI_BOR ),
            root,
            pComm
            );
        stopMeasurementSucceeded = QueryPerformanceCounter(pStop);
    }
    else
    {
        startMeasurementSucceeded = QueryPerformanceCounter(pStart);
        mpi_errno = MPIR_Reduce_intra(
            m_pSendBuffer,
            m_pRecvBuffer,
            nElements,
            g_hBuiltinTypes.MPI_Byte,
            OpPool::Get( MPI_BOR ),
            root,
            pComm
            );
        stopMeasurementSucceeded = QueryPerformanceCounter(pStop);
    }
    if(startMeasurementSucceeded == FALSE || stopMeasurementSucceeded == FALSE)
    {
        if(mpi_errno == MPI_SUCCESS)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**measurementfailed");
        }
    }
    return mpi_errno;
}


bool ReduceTuner::SmpEnabled() const
{
    return Mpi.SwitchoverSettings.SmpReduceEnabled;
}


bool ReduceTuner::Enabled() const
{
    return Mpi.TunerSettings.TuneReduce;
}


void ReduceTuner::SetGlobalSMPThreshold(unsigned int sizeThreshold) const
{
    Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_reduce_smp_threshold = sizeThreshold;
}


void ReduceTuner::SynthesizeAggregate()
{
    unsigned int j = 1;
    unsigned int i;
    for(i = 0; i < m_nSamples; ++i)
    {
        if(j < Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_reduce_short_msg)
        {
            m_pMeasurements[3][0][i] = m_pMeasurements[0][0][i];
        }
        else
        {
            // found the first switchover point
            break;
        }
        j <<= 1;
    }
    for(; i < m_nSamples; ++i)
    {
        m_pMeasurements[3][0][i] = m_pMeasurements[0][1][i];
    }
}
