// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"
#include "colltuner.h"

unsigned int CollectiveTuner::NumberOfAlgorithms() const
{
    return m_nAlgorithms;
}


CollectiveTuner::CollectiveTuner(unsigned int size, unsigned int time, unsigned int nAlgorithms)
{
    m_SizeLimit = size;
    m_TimeLimit = time;
    m_nAlgorithms = nAlgorithms;
    m_pMeasurements = NULL;
    m_pSendBuffer = NULL;
    m_pRecvBuffer = NULL;
    m_pSwitchA = NULL;
    m_pSwitchB = NULL;
    m_UseInner = TRUE;
    // determine how many sample sizes there are; every power of two and the max size
    m_nSamples = 0;
    while(size > 0)
    {
        m_nSamples++;
        size >>= 1;
    }
    if(static_cast<unsigned int>(1 << m_nSamples) < m_SizeLimit)
    {
        m_nSamples++;
    }
}


int CollectiveTuner::Initialize()
{
    int mpi_errno = MPI_SUCCESS;
    m_pSendBuffer = static_cast<BYTE*>(MPIU_Malloc(m_SizeLimit*sizeof(BYTE)));
    if( m_pSendBuffer == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    m_pRecvBuffer = static_cast<BYTE*>(MPIU_Malloc(m_SizeLimit*sizeof(BYTE)));
    if( m_pRecvBuffer == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    m_pSwitchA = static_cast<unsigned int*>(
        MPIU_Malloc(sizeof(unsigned int)*TUNER_SWITCHPOINT_SET_SIZE)
        );
    if( m_pSwitchA == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    if(m_nAlgorithms == COLLECTIVE_HAS_THREE_ALGORITHMS)
    {
        m_pSwitchB = static_cast<unsigned int*>(
            MPIU_Malloc(sizeof(unsigned int)*TUNER_SWITCHPOINT_SET_SIZE)
            );
        if( m_pSwitchB == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }
    }

    unsigned int setSize = TUNER_SWITCHPOINT_SET_SIZE;

    if(SmpEnabled())
    {
        setSize++;
    }

    m_pMeasurements = static_cast<LONGLONG***>(
        MPIU_Malloc(setSize*sizeof(LONGLONG**))
        );
    if( m_pMeasurements == nullptr )
    {
        return MPIU_ERR_NOMEM();
    }

    for(unsigned int set = 0; set < setSize; set++)
    {
        m_pMeasurements[set] = static_cast<LONGLONG**>(
            MPIU_Malloc(m_nAlgorithms*sizeof(LONGLONG*))
            );
        if( m_pMeasurements[set] == nullptr )
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }

        for(unsigned int i = 0; i < m_nAlgorithms; ++i)
        {
            m_pMeasurements[set][i] = static_cast<LONGLONG*>(
                MPIU_Malloc(m_nSamples*sizeof(LONGLONG))
                );
            if( m_pMeasurements[set][i] == nullptr )
            {
                mpi_errno = MPIU_ERR_NOMEM();
                goto fn_fail;
            }
        }
    }

fn_fail:
    return mpi_errno;
}


CollectiveTuner::~CollectiveTuner()
{
    if(m_pSendBuffer != NULL)
    {
        MPIU_Free(m_pSendBuffer);
        m_pSendBuffer = NULL;
    }
    if(m_pRecvBuffer != NULL)
    {
        MPIU_Free(m_pRecvBuffer);
        m_pRecvBuffer = NULL;
    }
    if(m_pSwitchA != NULL)
    {
        MPIU_Free(m_pSwitchA);
        m_pSwitchA = NULL;
    }
    if(m_pSwitchB != NULL)
    {
        MPIU_Free(m_pSwitchB);
        m_pSwitchB = NULL;
    }
    if(m_pMeasurements != NULL)
    {
        unsigned int setSize = TUNER_SWITCHPOINT_SET_SIZE;
        if(SmpEnabled())
        {
            setSize++;
        }
        for(unsigned int i = 0; i < setSize; ++i)
        {
            if(m_pMeasurements[i] != NULL)
            {
                for(unsigned int j = 0; j < m_nAlgorithms; ++j)
                {
                    if(m_pMeasurements[i][j] != NULL)
                    {
                        MPIU_Free(m_pMeasurements[i][j]);
                        m_pMeasurements[i][j] = NULL;
                    }
                }
                m_pMeasurements[i] = NULL;
            }
        }
        m_pMeasurements = NULL;
    }
}


int CollectiveTuner::TimeCall(
    unsigned int    nElements,
    unsigned int    root,
    LARGE_INTEGER* pStart,
    LARGE_INTEGER* pStop,
    MPID_Comm*     pComm,
    LONGLONG*      pElapsedTime,
    LONGLONG*      pHighTime,
    LONGLONG*      pLowTime
    )
{
    int mpi_errno;
    LONGLONG time;
    mpi_errno = Measure(
        nElements,
        root,
        pStart,
        pStop,
        pComm
    );
    if(mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }
    time = pStop->QuadPart - pStart->QuadPart;
    if(*pHighTime < time)
    {
        *pHighTime = time;
    }
    if(*pLowTime > time)
    {
        *pLowTime = time;
    }
    *pElapsedTime += time;
    return mpi_errno;
}


int CollectiveTuner::DoMeasurements(
    LONGLONG     timeLimit,
    LONGLONG*   pTimeUsed,
    MPID_Comm*  pComm,
    unsigned int collSet,
    unsigned int algorithm,
    MPI_Comm     syncHandle
    )
{
    int mpi_errno;
    unsigned int root = 0;
    unsigned int size = pComm->remote_size;
    LARGE_INTEGER start, stop;
    LONGLONG high, low;
    LONGLONG elapsedForAllSizes = 0;

    start.QuadPart = 0;
    stop.QuadPart = 0;
    unsigned int requiredIterations = TUNER_MINIMUM_ITERATION_SIZE;

    for(unsigned int nElements = 1, i = 0;
        i < m_nSamples;
        nElements = nElements * 2, ++i)
    {
        if(nElements > m_SizeLimit)
        {
            nElements = m_SizeLimit;
        }
        LONGLONG elapsedForSize = 0;
        LONGLONG timeLimitForIter = (timeLimit - elapsedForAllSizes) / (m_nSamples - i);
        if(timeLimitForIter < 0)
        {
            timeLimitForIter = 0;
        }
        LARGE_INTEGER startOfMeasurementsForIter;
        QueryPerformanceCounter(&startOfMeasurementsForIter);
        high = 0;
        low = INT_MAX;
        for(unsigned int iter = 0; iter < requiredIterations; ++iter)
        {
            mpi_errno = NMPI_Barrier(pComm->handle);
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }

            mpi_errno = TimeCall(
                nElements,
                root,
                &start,
                &stop,
                pComm,
                &elapsedForSize,
                &high,
                &low
                );
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
            //pick a new root
            root = (root + 1 + size/7) % size;
        }
        unsigned int remainingIterations = 0;
        //figure out about how many more iterations to run
        LONGLONG elapsedForIter = stop.QuadPart - startOfMeasurementsForIter.QuadPart;
        //
        // num iterations remaining = time remaining / time for a single iteration
        //
        if(timeLimitForIter < elapsedForIter)
        {
            remainingIterations = 0;
        }
        else
        {
            remainingIterations = static_cast<unsigned int>(
                (timeLimitForIter - elapsedForIter) /
                (1 + (elapsedForIter / requiredIterations))
                );
        }

        if(remainingIterations + requiredIterations > Mpi.TunerSettings.IterationLimit)
        {
            remainingIterations = Mpi.TunerSettings.IterationLimit - requiredIterations;
        }

        mpi_errno = NMPI_Allreduce(MPI_IN_PLACE, &remainingIterations, 1, MPI_UNSIGNED, MPI_MIN, syncHandle);
        if(mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }


        unsigned int totalIterations = requiredIterations + remainingIterations;

        if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
        {
            wprintf(
                L"%u (%u): Performing %u iterations for %s at size %u and slot %u (%I64d,%I64d).\n",
                m_GlobalRank,
                m_LocalRank,
                totalIterations,
                Name(),
                nElements,
                i,
                high,
                low
                );
            fflush(stdout);
        }
        while(remainingIterations > 0)
        {
            remainingIterations--;
            mpi_errno = NMPI_Barrier(pComm->handle);
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }

            mpi_errno = TimeCall(
                nElements,
                root,
                &start,
                &stop,
                pComm,
                &elapsedForSize,
                &high,
                &low
                );
            if(mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
            root = (root + 1 + size/7) % size;
        }
        //
        //return the average time for an iteration
        //
        m_pMeasurements[collSet][algorithm][i] = (elapsedForSize - high - low)/(totalIterations - 2);
        //
        //add the total elapsed time for this function
        //
        elapsedForAllSizes += stop.QuadPart - startOfMeasurementsForIter.QuadPart;
    }
    //now the total time counter is incremented
    *pTimeUsed += elapsedForAllSizes;
    return MPI_SUCCESS;
}


void CollectiveTuner::SetSMPInner(bool useInner)
{
    m_UseInner = useInner;
}

int CollectiveTuner::MeasureAndSet(
    MPID_Comm*  pComm,
    LONGLONG     time,
    unsigned int collSet,
    MPI_Comm     syncHandle
    )
{
    int mpi_errno = MPI_SUCCESS;

    if(time < 0)
    {
        time = 0;
    }

    LONGLONG algorithmTimeSlice;
    LONGLONG elapsedTime = 0;

    for(unsigned int i = 0; i < m_nAlgorithms; ++i)
    {
        if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
        {
            wprintf(L"%u (%u): Checking algorithm %u for %s.\n",
                m_GlobalRank, m_LocalRank, i, Name());
            wprintf(L"%u (%u): Using Switchpoints at %p and Measurements at %p.\n",
                m_GlobalRank, m_LocalRank, pComm->SwitchPoints(), m_pMeasurements[collSet][i]);
            fflush(stdout);
        }
        //figure out how much time is left for the next algorithm to run
        algorithmTimeSlice = (time - elapsedTime) / (1 + (m_nAlgorithms-i));
        if(algorithmTimeSlice < 0)
        {
            algorithmTimeSlice = 0;
        }
        SetAlgorithm(i, pComm->SwitchPoints());
        mpi_errno = DoMeasurements(
            algorithmTimeSlice,
            &elapsedTime,
            pComm,
            collSet,
            i,
            syncHandle
            );
        ON_ERROR_FAIL(mpi_errno);
    }

    if(pComm->rank == 0)
    {
        if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
        {
            wprintf(
                L"%u (%u): Analyzing measurements for %s:\n",
                m_GlobalRank,
                m_LocalRank,
                Name()
                );
            fflush(stdout);
        }
        AnalyzeMeasurements(collSet);

        if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
        {
            wprintf(
                L"%u (%u): Setting switchover points for %s and switchover point set %u:\n",
                m_GlobalRank,
                m_LocalRank,
                Name(),
                collSet
                );
            fflush(stdout);
        }
        mpi_errno = SetSwitchPoints(pComm->handle, pComm->SwitchPoints(), collSet);
        ON_ERROR_FAIL(mpi_errno);
    }
    else
    {
        mpi_errno = SetSwitchPoints(pComm->handle, pComm->SwitchPoints(), collSet);
        ON_ERROR_FAIL(mpi_errno);
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


static inline unsigned int min2(LONGLONG a, LONGLONG b)
{
    if(a <= b)
    {
        return 0;
    }
    return 1;
}


static inline double Percent2(LONGLONG a, LONGLONG b)
{
    if(a <= b)
    {
        return 1-(double)a/(double)b;
    }
    else
    {
        return 1-(double)b/(double)a;
    }
}


static inline LONGLONG Difference2(LONGLONG a, LONGLONG b)
{
    if(a <= b)
    {
        return b - a;
    }
    else
    {
        return a - b;
    }
}


static unsigned int DoAnalysisForTwoAlgorithms(
    const LONGLONG* pA,
    const LONGLONG* pB,
    unsigned int nMeasurements
    )
{
    unsigned int min;
    unsigned int prevMin = 0;
    unsigned int switchover = 1<<nMeasurements;
    for(unsigned int i = 0; i < nMeasurements; ++i)
    {
        min = min2(pA[i], pB[i]);
        if(min != prevMin)
        {
            if(Percent2(pA[i], pB[i]) > TUNER_MINIMUM_CHANGE_THRESHOLD)
            {
                unsigned int last = min;
                unsigned int nextChange = i;
                for(unsigned int j = i + 1; j < nMeasurements; ++j)
                {
                    min = min2(pA[j], pB[j]);
                    if(min != last)
                    {
                        if(Percent2(pA[j], pB[j]) > TUNER_MINIMUM_CHANGE_BACK_THRESHOLD)
                        {
                            nextChange = j;
                            break;
                        }
                    }
                }
                if(nextChange == i)
                {
                    for(int k = nMeasurements - 1; k >= 0; k--)
                    {
                        min = min2(pA[k], pB[k]);
                        if(min != last)
                        {
                            //switchover point is at k+1
                            switchover = 1<<(k+1);
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
    if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_LOW)
    {
        for(unsigned int v = 0; v < nMeasurements; ++v)
        {
            wprintf(
                L"size=%9.u, min = %u (%9.I64d,%9.I64d) %9.I64d %1.6f\n",
                1UL<<v,
                min2(pA[v],pB[v]),
                pA[v],
                pB[v],
                Difference2(pA[v], pB[v]),
                Percent2(pA[v], pB[v])
                );
        }
        fflush(stdout);
    }
    return switchover;
}


static void DoAnalysisForThreeAlgorithms(
    const LONGLONG* pA,
    const LONGLONG* pB,
    const LONGLONG* pC,
    unsigned int  nMeasurements,
    unsigned int* pFirstPoint,
    unsigned int* pSecondPoint
    )
{
    *pFirstPoint = DoAnalysisForTwoAlgorithms(pA, pB, nMeasurements);
    *pSecondPoint = DoAnalysisForTwoAlgorithms(pB, pC, nMeasurements);
    if(*pSecondPoint < *pFirstPoint)
    {
        /* middle algorithm is pointless */
        *pFirstPoint = *pSecondPoint = DoAnalysisForTwoAlgorithms(pA, pC, nMeasurements);
    }
}


int CollectiveTuner::MeasureFunction()
{
    int mpi_errno = MPI_SUCCESS;
    LONGLONG nCycles;
    LARGE_INTEGER frequency;

    if(!QueryPerformanceFrequency(&frequency))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**frequency");
        goto fn_fail;
    }

    if(SmpEnabled())
    {
        nCycles = (m_TimeLimit * frequency.QuadPart) / TUNER_SWITCHPOINT_SET_SIZE;
    }
    else
    {
        nCycles = m_TimeLimit * frequency.QuadPart;
    }

    MPID_Comm* pComm = CommPool::Lookup(MPI_COMM_WORLD);
    m_GlobalRank = pComm->rank;
    if(pComm->intra.local_subcomm != nullptr)
    {
        m_LocalRank = pComm->intra.local_subcomm->rank;
    }
    else
    {
        m_LocalRank = UINT_MAX;
    }

    if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
    {
        wprintf(
            L"%u (%u): Frequency is %I64d.\n",
            m_GlobalRank,
            m_LocalRank,
            frequency.QuadPart
            );
        wprintf(
            L"%u (%u): Tuning default values for %s.\n",
            m_GlobalRank,
            m_LocalRank,
            Name()
            );
        fflush(stdout);
    }

    mpi_errno = MeasureAndSet(
        pComm,
        nCycles,
        0,
        MPI_COMM_WORLD
        );
    ON_ERROR_FAIL(mpi_errno);

    if(SmpEnabled())
    {
        if(pComm->intra.local_subcomm != nullptr)
        {
            if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
            {
                wprintf(
                    L"%u (%u): Tuning intranode values for %s with Communicator at %p.\n",
                    m_GlobalRank,
                    m_LocalRank,
                    Name(),
                    pComm->intra.local_subcomm
                    );
                fflush(stdout);
            }
            mpi_errno = MeasureAndSet(
                pComm->intra.local_subcomm,
                nCycles,
                1,
                MPI_COMM_WORLD
                );
            ON_ERROR_FAIL(mpi_errno);
        }
        if( pComm->intra.leaders_subcomm != nullptr &&
            pComm->intra.leaders_subcomm->remote_size > 1)
        {
            if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
            {
                wprintf(
                    L"%u (%u): Tuning internode values for %s with Communicator at %p.\n",
                    m_GlobalRank,
                    m_LocalRank,
                    Name(),
                    pComm->intra.leaders_subcomm
                    );
                fflush(stdout);
            }
            mpi_errno = MeasureAndSet(
                pComm->intra.leaders_subcomm,
                nCycles,
                2,
                pComm->intra.leaders_subcomm->handle
                );
            ON_ERROR_FAIL(mpi_errno);
        }
        //
        //Tell the tuner not to use the inner implementation, rather evaluate the
        //macro smp aware algorithm.
        //
        SetSMPInner(false);

        //
        //Now do measurements for the whole SMP algorithm
        //this takes into account the tuning already done.
        //
        LONGLONG timeUsed = 0;
        DoMeasurements(
            nCycles,
            &timeUsed,
            pComm,
            3, // the comparison between off vs on
            1, // only need to collect for on
            MPI_COMM_WORLD
            );
        //Synthesize the measurements for off
        SynthesizeAggregate();

        if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_LOW)
        {
            wprintf(L"Measurement data for comparing SMP to non SMP aware collective %s on rank %u:\n",
                Name(),
                m_GlobalRank
                );
        }

        //Find the point to switch between the smp agnostic and the smp aware version
        int sizeThreshold = DoAnalysisForTwoAlgorithms(
            m_pMeasurements[3][0],
            m_pMeasurements[3][1],
            m_nSamples
            );
        //Gain consensus on the switchover point
        NMPI_Allreduce(
            MPI_IN_PLACE,
            &sizeThreshold,
            1,
            MPI_INT,
            MPI_MIN,
            MPI_COMM_WORLD
            );
        //Set the switchover point for on vs off.
        SetGlobalSMPThreshold(sizeThreshold);
    }
    if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_HIGH)
    {
        wprintf(L"%u (%u): Done tuning for %s.\n", m_GlobalRank, m_LocalRank, Name());
        fflush(stdout);
    }
    mpi_errno = NMPI_Barrier(pComm->handle);
fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}


void CollectiveTuner::AnalyzeMeasurements(unsigned int measurementIndex)
{
    if(Mpi.TunerSettings.Verbose >= TUNER_VERBOSE_LOW)
    {
        wprintf(L"Measurement data for collective %s:\n",
            Name()
            );
    }

    if(m_nAlgorithms == COLLECTIVE_HAS_TWO_ALGORITHMS)
    {
        m_pSwitchA[measurementIndex] = DoAnalysisForTwoAlgorithms(
            m_pMeasurements[measurementIndex][0],
            m_pMeasurements[measurementIndex][1],
            m_nSamples
            );
    }    //
    else // must be 3 as it is the only other option
    {    //
        DoAnalysisForThreeAlgorithms(
            m_pMeasurements[measurementIndex][0],
            m_pMeasurements[measurementIndex][1],
            m_pMeasurements[measurementIndex][2],
            m_nSamples,
            &m_pSwitchA[measurementIndex],
            &m_pSwitchB[measurementIndex]
            );
    }
}


bool CollectiveTuner::SmpEnabled() const
{
    return false;
}


static CollectiveTuner* GetCollectiveTuner(
    MPIR_Collective Collective,
    unsigned int sizeLimit,
    unsigned int timeLimit
    )
{
    switch(Collective)
    {
        case Broadcast:
            return new  BcastTuner(sizeLimit, timeLimit);
        case Reduce:
            return new  ReduceTuner(sizeLimit, timeLimit);
        case AllReduce:
            return new  AllreduceTuner(sizeLimit, timeLimit);
        case ReduceScatter:
            return new  ReduceScatterTuner(sizeLimit, timeLimit);
        case Alltoall:
            return new  AlltoallTuner(sizeLimit, timeLimit);
        case Gather:
            return new  GatherTuner(sizeLimit, timeLimit);
        case Allgather:
            return new  AllgatherTuner(sizeLimit, timeLimit);
        default:
            return NULL;
    }
}


int MeasureAlgorithmicSwitchPoints()
{
    int ferr;
    int mpi_errno = MPI_SUCCESS;
    unsigned int timeLimit = Mpi.TunerSettings.TimeLimit;
    unsigned int sizeLimit = Mpi.TunerSettings.SizeLimit;
    CollectiveTuner* pCollTune = NULL;
    FILE* output = NULL;
    unsigned int rank = Mpi.CommWorld->rank;

    if(rank == 0)
    {
        if(Mpi.TunerSettings.OutputFile[0] != L'\0')
        {
            ferr = _wfopen_s(&output, Mpi.TunerSettings.OutputFile, L"w");
            if(ferr != 0)
            {
                wprintf(L"Tuner is unable to open output file %s.\n"
                        L"Redirecting output to console.\n",
                        Mpi.TunerSettings.OutputFile);
                output = stdout;
            }
        }
        else
        {
            output = stdout;
        }
    }

    for(unsigned int i = 0; i < CollectiveEnd; ++i)
    {
        pCollTune = GetCollectiveTuner(
            static_cast<MPIR_Collective>(i),
            sizeLimit,
            timeLimit/CollectiveEnd
            );
        if( pCollTune == nullptr )
        {
            return MPIU_ERR_NOMEM();
        }

        if(pCollTune->Enabled())
        {
            mpi_errno = pCollTune->Initialize();
            ON_ERROR_FAIL(mpi_errno);

            mpi_errno = pCollTune->MeasureFunction();
            ON_ERROR_FAIL(mpi_errno);

            if(rank == 0)
            {
                switch(Mpi.TunerSettings.PrintSettings)
                {
                case OptionFile:
                    pCollTune->PrintValues(output, L"-env %s %d \\\n");
                    break;
                case Cluscfg:
                    pCollTune->PrintValues(output, L"cluscfg setenvs %s=%d\n");
                    break;
                case Mpiexec:
                    pCollTune->PrintValues(output, L"-env %s %d ");
                    break;
                default:
                    break;
                }
            }
        }
        delete pCollTune;
        pCollTune = NULL;
    }

    if(output != NULL && output != stdout)
    {
        fflush(output);
        fclose(output);
    }


fn_exit:
    return mpi_errno;

fn_fail:
    delete pCollTune;
    goto fn_exit;
}
