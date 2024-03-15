// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef _COLLUTIL_H_
#define _COLLUTIL_H_

struct CollectiveSwitchover
{
    unsigned int MPIR_bcast_short_msg;
    unsigned int MPIR_bcast_long_msg;
    unsigned int MPIR_bcast_min_procs;
    unsigned int MPIR_alltoall_short_msg;
    unsigned int MPIR_alltoall_medium_msg;
    unsigned int MPIR_alltoallv_short_msg;
    unsigned int MPIR_redscat_commutative_long_msg;
    unsigned int MPIR_redscat_noncommutative_short_msg; /* inter */
    unsigned int MPIR_allgather_short_msg;
    unsigned int MPIR_allgather_long_msg;
    unsigned int MPIR_reduce_short_msg;
    unsigned int MPIR_allreduce_short_msg;
    unsigned int MPIR_gather_vsmall_msg;
    unsigned int MPIR_gather_short_msg; /* inter */
    unsigned int MPIR_scatter_vsmall_msg;
    unsigned int MPIR_scatter_short_msg; /* inter */
    unsigned int MPIR_bcast_smp_threshold;
    unsigned int MPIR_bcast_smp_ceiling;
    unsigned int MPIR_reduce_smp_threshold;
    unsigned int MPIR_reduce_smp_ceiling;
    unsigned int MPIR_allreduce_smp_threshold;
    unsigned int MPIR_allreduce_smp_ceiling;
    unsigned int MPIR_alltoall_pair_algo_msg_threshold;
    unsigned int MPIR_alltoall_pair_algo_procs_threshold;
};

enum CollectiveSwitchoverKind
{
    COLL_SWITCHOVER_FLAT,
    COLL_SWITCHOVER_INTER_NODE,
    COLL_SWITCHOVER_INTRA_NODE,
    COLL_SWITCHOVER_COUNT
};

class CollectiveSwitchoverSettings
{

public:
    bool SmpBcastEnabled;
    bool SmpBarrierEnabled;
    bool SmpReduceEnabled;
    bool SmpAllreduceEnabled;
    bool SmpCollectiveEnabled;

    unsigned int RequestGroupSize;

private:
    CollectiveSwitchover Table[COLL_SWITCHOVER_COUNT];

public:
    CollectiveSwitchoverSettings()
    : SmpBcastEnabled(false)
    , SmpBarrierEnabled(false)
    , SmpReduceEnabled(false)
    , SmpAllreduceEnabled(false)
    , SmpCollectiveEnabled(false)
    , RequestGroupSize(1)
    {
        RtlZeroMemory(Table, sizeof(Table));
    }

    ~CollectiveSwitchoverSettings()
    {
    }

    MPI_RESULT Initialize();

    const CollectiveSwitchover& operator[](int i) const
    {
        return Table[i];
    }

    CollectiveSwitchover& operator[](int i)
    {
        return Table[i];
    }

private:
    void InitializeSwitchPointsTable();

private:
    CollectiveSwitchoverSettings(const CollectiveSwitchoverSettings& other);
    CollectiveSwitchoverSettings& operator = (const CollectiveSwitchoverSettings& other);
};


#endif // _COLLUTIL_H_
