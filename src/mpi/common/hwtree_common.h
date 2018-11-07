// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

//
// Represents the node types in the tree.  Each value
//  corrisponds to depth in the HW tree.
//
typedef enum _HWNODE_TYPE
{
    HWNODE_TYPE_WORLD   = -1,
    HWNODE_TYPE_MACHINE = 0,
    HWNODE_TYPE_GROUP   = 1,
    HWNODE_TYPE_NUMA    = 2,
    HWNODE_TYPE_PCORE   = 3,
    HWNODE_TYPE_LCORE   = 4,

    HWNODE_TYPE_MAX,

} HWNODE_TYPE;


enum SMPD_AFFINITY_PLACEMENT
{
    SMPD_AFFINITY_DISABLED = 0,
    SMPD_AFFINITY_SPREAD = 1,
    SMPD_AFFINITY_SEQUENTIAL = 2,
    SMPD_AFFINITY_BALANCED = 3,
    SMPD_AFFINITY_DEFAULT = SMPD_AFFINITY_SPREAD,
};



typedef struct _AffinityOptions
{
    BOOL                            isSet;
    BOOL                            isExplicit;
    BOOL                            isAuto;
    enum SMPD_AFFINITY_PLACEMENT    placement;
    HWNODE_TYPE                     target;
    HWNODE_TYPE                     stride;
    INT                             affinityTableStyle;
    INT                             hwTableStyle;
} AffinityOptions;
