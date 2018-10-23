// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "colltuner.h"

#define SWITCHADEFAULT L"MPICH_DEFAULT_REDSCAT_COMMUTATIVE_LONG_MSG"

ReduceScatterTuner::ReduceScatterTuner(
    unsigned int size,
    unsigned int time
    ) : CollectiveTuner(size, time, COLLECTIVE_HAS_TWO_ALGORITHMS)
{
}


const wchar_t* ReduceScatterTuner::Name() const
{
    return L"ReduceScatter";
}


void ReduceScatterTuner::PrintValues(FILE* pOutFile, const wchar_t* pFormat) const
{
    fwprintf(
        pOutFile,
        pFormat,
        SWITCHADEFAULT,
        Mpi.SwitchoverSettings[COLL_SWITCHOVER_FLAT].MPIR_redscat_commutative_long_msg
        );
    fflush(pOutFile);
}


void ReduceScatterTuner::SetAlgorithm(unsigned int algorithm, CollectiveSwitchover* s)
{
    switch(algorithm)
    {
    case 0: /* short */
        s->MPIR_redscat_commutative_long_msg = UINT_MAX;
        break;
    case 1: /* long */
        s->MPIR_redscat_commutative_long_msg = 0;
        break;
    default:
        break;
    }
}


int ReduceScatterTuner::SetSwitchPoints(
    MPI_Comm comm,
    CollectiveSwitchover* pSwitchover,
    unsigned int measurementIndex
    )
{
    int mpi_errno;
    mpi_errno = NMPI_Bcast(&m_pSwitchA[measurementIndex], 1, MPI_INT, 0, comm);
    ON_ERROR_FAIL(mpi_errno);

    pSwitchover->MPIR_redscat_commutative_long_msg = m_pSwitchA[measurementIndex];

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


int ReduceScatterTuner::Measure(
    unsigned int    nElements,
    unsigned int    /*root*/,
    LARGE_INTEGER* pStart,
    LARGE_INTEGER* pStop,
    MPID_Comm*     pComm
    )
{
    BOOL startMeasurementSucceeded = FALSE;
    BOOL stopMeasurementSucceeded = FALSE;

    int* pResCount = static_cast<int*>(
        MPIU_Malloc(sizeof(unsigned int)*pComm->remote_size)
        );
    if( pResCount == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    int len = nElements / pComm->remote_size;
    for(int i = 0; i < pComm->remote_size; ++i)
    {
        pResCount[i] = len;
    }

    startMeasurementSucceeded = QueryPerformanceCounter(pStart);

    MPIU_Assert( pComm->comm_kind != MPID_INTERCOMM );

    MPI_RESULT mpi_errno = MPIR_Reduce_scatter_intra(
        m_pSendBuffer,
        m_pRecvBuffer,
        pResCount,
        g_hBuiltinTypes.MPI_Byte,
        OpPool::Get( MPI_BOR ),
        pComm
        );
    stopMeasurementSucceeded = QueryPerformanceCounter(pStop);
    MPIU_Free(pResCount);

    if(startMeasurementSucceeded == FALSE || stopMeasurementSucceeded == FALSE)
    {
        if(mpi_errno == MPI_SUCCESS)
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**measurementfailed");
        }
    }
    return mpi_errno;
}

bool ReduceScatterTuner::Enabled() const
{
    return Mpi.TunerSettings.TuneRedscat;
}
