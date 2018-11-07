// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#define TUNER_SWITCHPOINT_SET_SIZE          3
#define TUNER_MINIMUM_ITERATION_SIZE        5
#define TUNER_MINIMUM_CHANGE_THRESHOLD      .1
#define TUNER_MINIMUM_CHANGE_BACK_THRESHOLD .3

#define TUNER_VERBOSE_DEFAULT         false
#define TUNER_PRINT_SETTINGS_DEFAULT  false

#define TUNER_SIZE_LIMIT_DEFAULT      16777216
#define TUNER_SIZE_LIMIT_MIN          1
#define TUNER_SIZE_LIMIT_MAX          INT_MAX

#define TUNER_TIME_LIMIT_DEFAULT      60
#define TUNER_TIME_LIMIT_MIN          0
#define TUNER_TIME_LIMIT_MAX          INT_MAX

#define TUNER_ITERATION_LIMIT_DEFAULT 100000
#define TUNER_ITERATION_LIMIT_MIN     TUNER_MINIMUM_ITERATION_SIZE
#define TUNER_ITERATION_LIMIT_MAX     INT_MAX

#define TUNER_VERBOSE_MIN 0
#define TUNER_VERBOSE_LOW 1
#define TUNER_VERBOSE_HIGH 2
#define TUNER_VERBOSE_MAX TUNER_VERBOSE_HIGH

#define COLLECTIVE_HAS_THREE_ALGORITHMS 3
#define COLLECTIVE_HAS_TWO_ALGORITHMS 2

enum TunerPrintFormat
{
    Off = 0,
    Cluscfg,
    Mpiexec,
    OptionFile,
    TunerPrintFormatEnd
};


enum MPIR_Collective
{
    Broadcast     = 0,
    Reduce,
    AllReduce,
    ReduceScatter,
    Alltoall,
    Gather,
    Allgather,
    CollectiveEnd
};



class CollectiveTunerSettings
{

public:
    bool TuneBcast;
    bool TuneReduce;
    bool TuneAllreduce;
    bool TuneGather;
    bool TuneAllgather;
    bool TuneAlltoall;
    bool TuneRedscat;

    unsigned int PrintSettings;
    unsigned int Verbose;
    unsigned int SizeLimit;
    unsigned int TimeLimit;
    unsigned int IterationLimit;
    wchar_t      OutputFile[MAX_PATH];

public:
    CollectiveTunerSettings()
    : TuneBcast(false)
    , TuneReduce(false)
    , TuneAllreduce(false)
    , TuneGather(false)
    , TuneAllgather(false)
    , TuneAlltoall(false)
    , TuneRedscat(false)
    , PrintSettings(TUNER_PRINT_SETTINGS_DEFAULT)
    , Verbose(TUNER_VERBOSE_DEFAULT)
    , SizeLimit(TUNER_SIZE_LIMIT_DEFAULT)
    , TimeLimit(TUNER_TIME_LIMIT_DEFAULT)
    , IterationLimit(TUNER_ITERATION_LIMIT_DEFAULT)
    {
        RtlZeroMemory(OutputFile, sizeof(OutputFile));
    }

    ~CollectiveTunerSettings()
    {
    }

    MPI_RESULT Initialize();

private:
    CollectiveTunerSettings(const CollectiveTunerSettings& other);
    CollectiveTunerSettings& operator = (const CollectiveTunerSettings& other);
};
