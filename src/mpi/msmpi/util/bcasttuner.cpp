// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "colltuner.h"

#define SWITCHADEFAULT L"MPICH_DEFAULT_BCAST_SHORT_MSG"
#define SWITCHBDEFAULT L"MPICH_DEFAULT_BCAST_LONG_MSG"
#define SWITCHAINTRA L"MPICH_INTRANODE_BCAST_SHORT_MSG"
#define SWITCHBINTRA L"MPICH_INTRANODE_BCAST_LONG_MSG"
#define SWITCHAINTER L"MPICH_INTERNODE_BCAST_SHORT_MSG"
#define SWITCHBINTER L"MPICH_INTERNODE_BCAST_LONG_MSG"
#define THRESHOLD L"MPICH_DEFAULT_BCAST_SMP_THRESHOLD"

BcastTuner::BcastTuner(
    unsigned int size,
    unsigned int time
    )
    : CollectiveTuner(size, time, COLLECTIVE_HAS_THREE_ALGORITHMS)
{
}


const wchar_t* BcastTuner::Name() const
{
    return L"Broadcast";
}

void BcastTuner::PrintValues(FILE* pOutFile, const wchar_t* pFormat) const
{
    fwprintf(
        pOutFile,
        pFormat,
        SWITCHADEFAULT,
        Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_bcast_short_msg
        );
    fwprintf(
        pOutFile,
        pFormat,
        SWITCHBDEFAULT,
        Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_bcast_long_msg
        );
    if(SmpEnabled())
    {
        fwprintf(
            pOutFile,
            pFormat,
            SWITCHAINTRA,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_INTRA_NODE].MPIR_bcast_short_msg
            );
        fwprintf(
            pOutFile,
            pFormat,
            SWITCHBINTRA,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_INTRA_NODE].MPIR_bcast_long_msg
            );
        fwprintf(
            pOutFile,
            pFormat,
            SWITCHAINTER,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_INTER_NODE].MPIR_bcast_short_msg
            );
        fwprintf(
            pOutFile,
            pFormat,
            SWITCHBINTER,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_INTER_NODE].MPIR_bcast_long_msg
            );
        fwprintf(
            pOutFile,
            pFormat,
            THRESHOLD,
            Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_bcast_smp_threshold
            );
    }
    fflush(pOutFile);
}


void BcastTuner::SetAlgorithm(unsigned int algorithm, CollectiveSwitchover* s)
{
    switch(algorithm)
    {
    case 0: /* short */
        s->MPIR_bcast_short_msg = UINT_MAX;
        break;
    case 1: /* medium */
        s->MPIR_bcast_short_msg = 0;
        s->MPIR_bcast_long_msg = UINT_MAX;
        break;
    case 2:/* long */
        s->MPIR_bcast_short_msg = 0;
        s->MPIR_bcast_long_msg = 0;
        break;
    default:
        break;
    }
}


int BcastTuner::SetSwitchPoints(
    MPI_Comm comm,
    CollectiveSwitchover* pSwitchover,
    unsigned int measurementIndex
    )
{
    int mpi_errno;
    mpi_errno = NMPI_Bcast(&m_pSwitchA[measurementIndex], 1, MPI_UNSIGNED, 0, comm);
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = NMPI_Bcast(&m_pSwitchB[measurementIndex], 1, MPI_UNSIGNED, 0, comm);
    ON_ERROR_FAIL(mpi_errno);

    pSwitchover->MPIR_bcast_short_msg = m_pSwitchA[measurementIndex];
    pSwitchover->MPIR_bcast_long_msg = m_pSwitchB[measurementIndex];

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


int BcastTuner::Measure(
    unsigned int   nElements,
    unsigned int   root,
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
        mpi_errno = MPIR_Bcast_intra_flat(
            m_pSendBuffer,
            nElements,
            root,
            pComm
            );
        stopMeasurementSucceeded = QueryPerformanceCounter(pStop);
    }
    else
    {
        startMeasurementSucceeded = QueryPerformanceCounter(pStart);
        mpi_errno = MPIR_Bcast_intra(
            m_pSendBuffer,
            nElements,
            g_hBuiltinTypes.MPI_Byte,
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


bool BcastTuner::SmpEnabled() const
{
    return Mpi.SwitchoverSettings.SmpBcastEnabled;
}


bool BcastTuner::Enabled() const
{
    return Mpi.TunerSettings.TuneBcast;
}


void BcastTuner::SetGlobalSMPThreshold(unsigned int sizeThreshold) const
{
    Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_bcast_smp_threshold = sizeThreshold;
}


void BcastTuner::SynthesizeAggregate()
{
    unsigned int j = 1;
    unsigned int i;
    for(i = 0; i < m_nSamples; ++i)
    {
        if(j < Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_bcast_short_msg)
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
        if(j < Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_bcast_long_msg)
        {
            m_pMeasurements[3][0][i] = m_pMeasurements[0][1][i];
        }
        else
        {
            //found the second switchover point
            break;
        }
        j <<= 1;
    }
    for(; i < m_nSamples; ++i)
    {
        m_pMeasurements[3][0][i] = m_pMeasurements[0][2][i];
    }
}
