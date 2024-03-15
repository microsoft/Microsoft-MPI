// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

void MPIU_dbg_preinit();

_Success_(return==MPI_SUCCESS)
int
MPIU_dbg_init(
    _In_ unsigned int rank,
    _In_ unsigned int world_size
    );

void
MPIU_dbg_printf(
    _Printf_format_string_ const char * str,
    ...
    );
