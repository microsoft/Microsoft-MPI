// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "mpiimpl.h"
#include "mpidimpl.h"
#include <stdio.h>

#ifndef _COLLTUNER_H_
#define _COLLTUNER_H_

#include "colltunersettings.h"


class CollectiveTuner
{
protected:
    //
    //i: the communicator used (default, on-box, between-box)
    //j: the algorithm imposed (small, medium, etc.)
    //k: the data size tested (2^k)
    //
    LONGLONG*** m_pMeasurements;

    unsigned int  m_nSamples;
    unsigned int  m_nAlgorithms;
    unsigned int* m_pSwitchA;
    unsigned int* m_pSwitchB;
    BYTE* m_pSendBuffer;
    BYTE* m_pRecvBuffer;
    unsigned int m_SizeLimit;
    unsigned int m_TimeLimit;
    unsigned int m_LocalRank;
    unsigned int m_GlobalRank;
    bool m_UseInner;

public:
    CollectiveTuner(
        unsigned int size,
        unsigned int time,
        unsigned int nAlgorithms
        );

    virtual const wchar_t* Name() const = 0;

    virtual void  PrintValues(
        FILE* pOutputFile,
        _Printf_format_string_ const wchar_t* pFormat
        ) const = 0;

    //
    // Measure this function for the different communicators.
    // int timeLimit: the number of seconds to allow for measurement.
    // int sizeLimit: the maximum size message to measure (including all
    //     powers of 2 up and including the maximum size.
    //
    virtual int MeasureFunction();

    int Initialize();

    virtual bool Enabled() const = 0;

    virtual ~CollectiveTuner();
protected:
    unsigned int NumberOfAlgorithms() const;

    int TimeCall(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm,
        LONGLONG*      pElapsedTime,
        LONGLONG*      pHigh,
        LONGLONG*      pLow
        );

    int DoMeasurements(
        LONGLONG     timeLimit,
        LONGLONG*   pTimeUsed,
        MPID_Comm*  pComm,
        unsigned int collSet,
        unsigned int algorithm,
        MPI_Comm     syncHandle
        );

    //
    // Returns MPI error number if failure.
    //
    // timeLimit: number of cycles remaining for this operation
    //
    int MeasureAndSet(
        MPID_Comm*  pComm,
        LONGLONG     timeLimit,
        unsigned int collSet,
        MPI_Comm     syncHandle
        );

    void AnalyzeMeasurements(
        unsigned int measurementIndex
        );

    virtual bool SmpEnabled() const;

    virtual void SynthesizeAggregate() {};

    virtual void SetAlgorithm(
        unsigned int           algorithm,
        CollectiveSwitchover* pSwitchover
        ) = 0;

    virtual int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        ) = 0;

    virtual int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        ) = 0;
private:
    void SetSMPInner(bool useInner);
    virtual void SetGlobalSMPThreshold(unsigned int /*sizeThreshold*/) const {};

};


class BcastTuner : public CollectiveTuner
{
public:
    BcastTuner(
        unsigned int size,
        unsigned int time
        );

    __override const wchar_t* Name() const;

    __override void  PrintValues(FILE* pOutputFile, _Printf_format_string_ const wchar_t* pFormat) const;

    __override bool Enabled() const;

protected:
    __override void SetAlgorithm(
        unsigned int Algorithm,
        CollectiveSwitchover* pSwitchover
        );

    __override int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        );

    __override int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        );

    __override bool SmpEnabled() const;
    __override void SynthesizeAggregate();
private:
    __override void SetGlobalSMPThreshold(unsigned int sizeThreshold) const;
};


class ReduceTuner : public CollectiveTuner
{
public:
    ReduceTuner(
        unsigned int size,
        unsigned int time
        );

    __override const wchar_t* Name() const;

    __override void PrintValues(FILE* pOutputFile, _Printf_format_string_ const wchar_t* pFormat) const;

    __override bool Enabled() const;

protected:
    __override void SetAlgorithm(
        unsigned int algorithm,
        CollectiveSwitchover* pSwitchover
        );

    __override int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        );

    __override int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        );

    __override bool SmpEnabled() const;
    __override void SynthesizeAggregate();
private:
    __override void SetGlobalSMPThreshold(unsigned int sizeThreshold) const;
};


class AllreduceTuner : public CollectiveTuner
{
public:
    AllreduceTuner(
        unsigned int sizeLimit,
        unsigned int timeLimit
        );

    __override const wchar_t* Name() const;

    __override void PrintValues(FILE* pOutputFile, _Printf_format_string_ const wchar_t* pFormat) const;

    __override bool Enabled() const;

protected:
    __override void SetAlgorithm(
        unsigned int algorithm,
        CollectiveSwitchover* pSwitchover
        );

    __override int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        );

    __override int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        );

    __override bool SmpEnabled() const;
    __override void SynthesizeAggregate();
private:
    __override void SetGlobalSMPThreshold(unsigned int sizeThreshold) const;
};


class ReduceScatterTuner : public CollectiveTuner
{
public:
    ReduceScatterTuner(
        unsigned int size,
        unsigned int time
        );

    __override const wchar_t* Name() const;

    __override void PrintValues(FILE* pOutputFile, _Printf_format_string_ const wchar_t* pFormat) const;

    __override bool Enabled() const;

protected:
    __override void SetAlgorithm(
        unsigned int Algorithm,
        CollectiveSwitchover* pSwitchover
        );

    __override int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        );

    __override int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        );
};


class AlltoallTuner : public CollectiveTuner
{
public:
    AlltoallTuner(
        unsigned int sizeLimit,
        unsigned int timeLimit
        );

    __override const wchar_t* Name() const;

    __override void PrintValues(FILE* pOutputFile, _Printf_format_string_ const wchar_t* pFormat) const;

    __override bool Enabled() const;

protected:
    __override void SetAlgorithm(
        unsigned int algorithm,
        CollectiveSwitchover* pSwitchover
        );

    __override int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        );

    __override int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        );
};


class GatherTuner : public CollectiveTuner
{
public:
    GatherTuner(
        unsigned int size,
        unsigned int time
        );

    __override const wchar_t* Name() const;

    __override void PrintValues(FILE* pOutputFile, _Printf_format_string_ const wchar_t* pFormat) const;

    __override bool Enabled() const;

protected:
    __override void SetAlgorithm(
        unsigned int algorithm,
        CollectiveSwitchover* pSwitchover
        );

    __override int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        );

    __override int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        );
};


class AllgatherTuner : public CollectiveTuner
{
public:
    AllgatherTuner(
        unsigned int size,
        unsigned int time
        );

    __override const wchar_t* Name() const;

    __override void PrintValues(FILE* pOutputFile, _Printf_format_string_ const wchar_t* pFormat) const;

    __override bool Enabled() const;

protected:
    __override void SetAlgorithm(
        unsigned int algorithm,
        CollectiveSwitchover* pSwitchover
        );

    __override int SetSwitchPoints(
        MPI_Comm               comm,
        CollectiveSwitchover* pSwitchover,
        unsigned int           measurementIndex
        );

    __override int Measure(
        unsigned int   nElements,
        unsigned int    root,
        LARGE_INTEGER* pStart,
        LARGE_INTEGER* pStop,
        MPID_Comm*     pComm
        );
};

int MeasureAlgorithmicSwitchPoints();

#endif // _COLLTUNER_H_
