// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef SMPD_DATABASE_H
#define SMPD_DATABASE_H

#define SMPD_MAX_DBS_KEY_LEN      32
#define SMPD_MAX_DBS_VALUE_LEN    512

bool smpd_dbs_init();

void smpd_dbs_finalize();


_Success_( return == true )
bool
smpd_dbs_create(
    _In_    UINT16 size,
    _Inout_ GUID*  id
    );


bool smpd_dbs_destroy(const char *name);

_Success_(return == true)
bool
smpd_dbs_get(
    _In_ const GUID& id,
    _In_ PCSTR key,
    _Out_writes_z_(val_size) char* value,
    _In_ size_t val_size
    );


_Success_(return == true)
bool
smpd_dbs_bcget(
    _In_ const GUID& id,
    _In_ UINT16 rank,
    _Out_writes_z_(val_size) char* value,
    _In_ size_t val_size
    );


bool
smpd_dbs_put(
    _In_ const GUID& id,
    _In_ PCSTR key,
    _In_ PCSTR value
    );


bool
smpd_dbs_bcput(
    _In_ const GUID& id,
    _In_ UINT16 rank,
    _In_ PCSTR value
    );


#endif
