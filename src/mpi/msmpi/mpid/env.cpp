// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "precomp.h"

#include "pmi.h"
#include "ch3_compression.h"
#include "colltuner.h"
#include "mpi_conntbl.h"

//
// Global Instances.
//
MpiProcess      Mpi;


#ifdef HAVE_DEBUGGER_SUPPORT
extern "C"
{
__declspec(dllexport) volatile int MPIR_debug_gate = 0;
}

static void MPIR_WaitForDebugger( void )
{
    if (env_is_on(L"MPIEXEC_DEBUG", FALSE))
    {
        while (!MPIR_debug_gate) ;
    }
}
#endif


//
// Summary:
//  Cause the environment block to be printed either to the console
//  or to a file if provided.
//
static MPI_RESULT PrintEnvironmentBlock(void)
{
    MPI_RESULT mpi_errno = MPI_SUCCESS;
    FILE* pOut = stdout;
    wchar_t* filename = NULL;

    if(!env_is_on_ex(L"MSMPI_PRINT_ENVIRONMENT_BLOCK", NULL, FALSE))
    {
        return MPI_SUCCESS;
    }

    wchar_t pEnvFile[MAX_PATH];
    DWORD err = MPIU_Getenv( L"MSMPI_PRINT_ENVIRONMENT_BLOCK_FILE",
                             pEnvFile,
                             _countof(pEnvFile) );
    if( err == NOERROR )
    {
        DWORD pid = GetCurrentProcessId();

        //
        // 15 characters =
        //     10 characters for the PID (GetCurrentProcessId returns a DWORD)
        //      5 characters for the _.txt
        //
        // Note: ExpandEnvironmentStringsW requires one additional character for the null terminator.
        //
        static const int fileExtLen = 15;

        DWORD envFileLen = fileExtLen + ExpandEnvironmentStringsW(pEnvFile, NULL, 0);
        filename = static_cast<wchar_t*>(MPIU_Malloc(sizeof(wchar_t)*envFileLen));
        if(nullptr == filename)
        {
            return MPIU_ERR_NOMEM();
        }

        DWORD expandResultLen = ExpandEnvironmentStringsW(pEnvFile, filename, envFileLen);
        if(expandResultLen != (envFileLen - fileExtLen))
        {
            MPIU_Free(filename);
            return MPIU_ERR_CREATE(MPI_ERR_BAD_FILE, "**envexpand");
        }

        //
        // fileExtLen + 1 because the null terminator character slot is allocated and we want
        // StringCchPrintf to populate it appropriately.
        //
        HRESULT hr = StringCchPrintfW(
            filename + (expandResultLen - 1),
            fileExtLen + 1,
            L"_%u.txt",
            pid
            );

        if( FAILED( hr ) )
        {
            return MPIU_ERR_NOMEM();
        }

        pOut = _wfopen(filename, L"w");
        MPIU_Free(filename);

        if(nullptr == pOut)
        {
            return MPIU_ERR_CREATE(MPI_ERR_BAD_FILE, "**badfile %s", _strerror(NULL));
        }
    }

    LPWCH pEnvStrings = GetEnvironmentStringsW();
    if(NULL == pEnvStrings)
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**getenvfailed");
        goto fn_exit;
    }

    size_t i = 0;
    const wchar_t* env;
    while(L'\0' != pEnvStrings[i])
    {
        env = pEnvStrings + i;

        fwprintf(pOut, L"%s\n", env);

        size_t len;
        HRESULT hr = StringCchLengthW( env, STRSAFE_MAX_CCH, &len );
        if( FAILED(hr) )
        {
            mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER,"**getenvfailed");
            goto fn_exit;
        }

        i += len + 1;
    }

    fflush(pOut);

    if(FALSE == FreeEnvironmentStringsW(pEnvStrings))
    {
        mpi_errno = MPIU_ERR_CREATE(MPI_ERR_OTHER, "**freeenvfailed");
    }

fn_exit:
    if(pOut != stdout)
    {
        fclose(pOut);
    }
    return mpi_errno;
}


MPI_RESULT CollectiveSwitchoverSettings::Initialize()
{
    SmpBarrierEnabled = true;
    SmpBcastEnabled = true;
    SmpReduceEnabled = true;
    SmpAllreduceEnabled = true;

    wchar_t env[12];
    DWORD err = MPIU_Getenv(L"MSMPI_REQUEST_GROUP_SIZE", env, _countof(env));
    if (err == NOERROR)
    {
        int val = _wtoi(env);
        if (val > 0 && val <= 1024)
        {
            RequestGroupSize = val;
        }
    }


    wchar_t pSmpEnvVar[256];
    err = MPIU_Getenv(L"MSMPI_HA_COLLECTIVE",
                             pSmpEnvVar,
                             _countof(pSmpEnvVar) );
    if( err == NOERROR )
    {
        // when env var is set, only enable those specified by the user
        SmpBarrierEnabled = false;
        SmpBcastEnabled = false;
        SmpReduceEnabled = false;
        SmpAllreduceEnabled = false;

        // interpret other options only if it wasn't set to "off"
        if( CompareStringW( LOCALE_INVARIANT,
                            NORM_IGNORECASE,
                            pSmpEnvVar,
                            -1,
                            L"off",
                            -1 ) != CSTR_EQUAL )
        {

            const wchar_t* pDelims = L",";
            wchar_t* pNextToken = NULL;
            const wchar_t* pOption = wcstok_s( pSmpEnvVar, pDelims, &pNextToken );
            while(pOption != NULL)
            {
                if( CompareStringW( LOCALE_INVARIANT,
                                    NORM_IGNORECASE,
                                    pOption,
                                    -1,
                                    L"bcast",
                                    -1 ) == CSTR_EQUAL )
                {
                    SmpBcastEnabled = true;
                }
                else if( CompareStringW( LOCALE_INVARIANT,
                                         NORM_IGNORECASE,
                                         pOption,
                                         -1,
                                         L"barrier",
                                         -1 ) == CSTR_EQUAL )
                {
                    SmpBarrierEnabled = true;
                }
                else if( CompareStringW( LOCALE_INVARIANT,
                                         NORM_IGNORECASE,
                                         pOption,
                                         -1,
                                         L"reduce",
                                         -1 ) == CSTR_EQUAL )
                {
                    SmpReduceEnabled = true;
                }
                else if( CompareStringW( LOCALE_INVARIANT,
                                         NORM_IGNORECASE,
                                         pOption,
                                         -1,
                                         L"allreduce",
                                         -1 ) == CSTR_EQUAL )
                {
                    SmpAllreduceEnabled = true;
                }
                else if( CompareStringW( LOCALE_INVARIANT,
                                         NORM_IGNORECASE,
                                         pOption,
                                         -1,
                                         L"all",
                                         -1 ) == CSTR_EQUAL )
                {
                    SmpBcastEnabled = true;
                    SmpBarrierEnabled = true;
                    SmpReduceEnabled = true;
                    SmpAllreduceEnabled = true;
                }
                else
                {
                    SmpBcastEnabled = true;
                    SmpBarrierEnabled = true;
                    SmpReduceEnabled = true;
                    SmpAllreduceEnabled = true;
                    SmpCollectiveEnabled = true;
                    return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**badenv %s", "MSMPI_HA_COLLECTIVE");
                }
                pOption = wcstok_s( NULL, pDelims, &pNextToken);
            }
        }
    }
    SmpCollectiveEnabled   = SmpBcastEnabled   ||
                             SmpBarrierEnabled ||
                             SmpReduceEnabled  ||
                             SmpAllreduceEnabled;

    InitializeSwitchPointsTable();
    return MPI_SUCCESS;
}

MPI_RESULT CollectiveTunerSettings::Initialize()
{
    wchar_t pTunerEnvVar[256];
    DWORD err = MPIU_Getenv( L"MSMPI_TUNE_COLLECTIVE",
                             pTunerEnvVar,
                             _countof(pTunerEnvVar) );
    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        return MPI_SUCCESS;
    }
    else if( err == ERROR_INSUFFICIENT_BUFFER )
    {
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**badenv %s", "MSMPI_TUNE_COLLECTIVE");
    }

    TuneBcast = false;
    TuneReduce = false;
    TuneAllreduce = false;
    TuneGather = false;
    TuneAllgather = false;
    TuneAlltoall = false;
    TuneRedscat = false;
    TuneBcast = false;

    const wchar_t* pDelims = L",";
    wchar_t* pNextToken = NULL;
    const wchar_t* pOption = wcstok_s( pTunerEnvVar, pDelims, &pNextToken );
    while(pOption != NULL)
    {
        if( CompareStringW( LOCALE_INVARIANT,
                            NORM_IGNORECASE,
                            pOption,
                            -1,
                            L"bcast",
                            -1 ) == CSTR_EQUAL )
        {
            TuneBcast = true;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pOption,
                                 -1,
                                 L"reduce",
                                 -1 ) == CSTR_EQUAL )
        {
            TuneReduce = true;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pOption,
                                 -1,
                                 L"allreduce",
                                 -1 ) == CSTR_EQUAL )
        {
            TuneAllreduce = true;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pOption,
                                 -1,
                                 L"gather",
                                 -1 ) == CSTR_EQUAL )
        {
            TuneGather = true;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pOption,
                                 -1,
                                 L"allgather",
                                 -1 ) == CSTR_EQUAL )
        {
            TuneAllgather = true;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pOption,
                                 -1,
                                 L"alltoall",
                                 -1 ) == CSTR_EQUAL )
        {
            TuneAlltoall = true;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pOption,
                                 -1,
                                 L"reducescatter",
                                 -1 ) == CSTR_EQUAL )
        {
            TuneRedscat = true;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pOption,
                                 -1,
                                 L"all",
                                 -1 ) == CSTR_EQUAL )
        {
            TuneBcast = true;
            TuneReduce = true;
            TuneAllreduce = true;
            TuneGather = true;
            TuneAllgather = true;
            TuneAlltoall = true;
            TuneRedscat = true;
        }
        else
        {
            return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**badenv %s", "MSMPI_TUNE_COLLECTIVE");
        }
        pOption = wcstok_s( NULL, pDelims, &pNextToken);
    }

    wchar_t pTunerSettingsVar[16];
    err = MPIU_Getenv( L"MSMPI_TUNE_PRINT_SETTINGS",
                       pTunerSettingsVar,
                       _countof(pTunerSettingsVar) );
    if( err == NOERROR )
    {
        if( CompareStringW( LOCALE_INVARIANT,
                            NORM_IGNORECASE,
                            pTunerSettingsVar,
                            -1,
                            L"optionfile",
                            -1 ) == CSTR_EQUAL )
        {
            PrintSettings = OptionFile;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pTunerSettingsVar,
                                 -1,
                                 L"mpiexec",
                                 -1 ) == CSTR_EQUAL )
        {
            PrintSettings = Mpiexec;
        }
        else if( CompareStringW( LOCALE_INVARIANT,
                                 NORM_IGNORECASE,
                                 pTunerSettingsVar,
                                 -1,
                                 L"cluscfg",
                                 -1 ) == CSTR_EQUAL )
        {
            PrintSettings = Cluscfg;
        }
        else
        {
            return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**badenv %s", "MSMPI_TUNE_PRINT_SETTINGS");
        }
    }
    else if( err == ERROR_INSUFFICIENT_BUFFER )
    {
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**badenv %s", "MSMPI_TUNE_PRINT_SETTINGS");
    }

    err = MPIU_Getenv( L"MSMPI_TUNE_SETTINGS_FILE",
                       OutputFile,
                       _countof(OutputFile) );
    if( err != NOERROR )
    {
        OutputFile[0] = L'\0';
    }

    Verbose = env_to_int(
            L"MSMPI_TUNE_VERBOSE",
            TUNER_VERBOSE_DEFAULT,
            TUNER_VERBOSE_MIN
            );
    if(Verbose > TUNER_VERBOSE_MAX)
    {
        Verbose = TUNER_VERBOSE_MAX;
    }

    SizeLimit = env_to_int(
            L"MSMPI_TUNE_SIZE_LIMIT",
            TUNER_SIZE_LIMIT_DEFAULT,
            TUNER_SIZE_LIMIT_MIN
            );
    if(SizeLimit > TUNER_SIZE_LIMIT_MAX)
    {
        SizeLimit = TUNER_SIZE_LIMIT_MAX;
    }

    TimeLimit = env_to_int(
            L"MSMPI_TUNE_TIME_LIMIT",
            TUNER_TIME_LIMIT_DEFAULT,
            TUNER_TIME_LIMIT_MIN
            );
    if(TimeLimit > TUNER_TIME_LIMIT_MAX)
    {
        TimeLimit = TUNER_TIME_LIMIT_MAX;
    }

    IterationLimit = env_to_int(
            L"MSMPI_TUNE_ITERATION_LIMIT",
            TUNER_ITERATION_LIMIT_DEFAULT,
            TUNER_ITERATION_LIMIT_MIN
            );
    if(IterationLimit > TUNER_ITERATION_LIMIT_MAX)
    {
        IterationLimit = TUNER_ITERATION_LIMIT_MAX;
    }

    return MPI_SUCCESS;
}

void CollectiveSwitchoverSettings::InitializeSwitchPointsTable()
{
    //default
    Table[COLL_SWITCHOVER_FLAT].MPIR_bcast_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_BCAST_SHORT_MSG",
            MPIR_BCAST_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_bcast_long_msg
        = env_to_int(
            L"MPICH_DEFAULT_BCAST_LONG_MSG",
            MPIR_BCAST_LONG_MSG_DEFAULT,
            MPIR_BCAST_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_bcast_min_procs
        = env_to_int(
            L"MPICH_DEFAULT_BCAST_MIN_PROCS",
            MPIR_BCAST_MIN_PROCS_DEFAULT,
            1
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_alltoall_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_SHORT_MSG",
            MPIR_ALLTOALL_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_alltoall_medium_msg
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_MEDIUM_MSG",
            MPIR_ALLTOALL_MEDIUM_MSG_DEFAULT,
            MPIR_ALLTOALL_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_alltoallv_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALLV_SHORT_MSG",
            MPIR_ALLTOALLV_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_redscat_commutative_long_msg
        = env_to_int(
            L"MPICH_DEFAULT_REDSCAT_COMMUTATIVE_LONG_MSG",
            MPIR_REDSCAT_COMMUTATIVE_LONG_MSG_DEFAULT,
            0
        );
    Table[COLL_SWITCHOVER_FLAT].MPIR_redscat_noncommutative_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_REDSCAT_NONCOMMUTATIVE_SHORT_MSG",
            MPIR_REDSCAT_NONCOMMUTATIVE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_allgather_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_ALLGATHER_SHORT_MSG",
            MPIR_ALLGATHER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_allgather_long_msg
        = env_to_int(
            L"MPICH_DEFAULT_ALLGATHER_LONG_MSG",
            MPIR_ALLGATHER_LONG_MSG_DEFAULT,
            MPIR_ALLGATHER_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_reduce_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_REDUCE_SHORT_MSG",
            MPIR_REDUCE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_allreduce_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_ALLREDUCE_SHORT_MSG",
            MPIR_ALLREDUCE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_gather_vsmall_msg
        = env_to_int(
            L"MPICH_DEFAULT_GATHER_VSMALL_MSG",
            MPIR_GATHER_VSMALL_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_gather_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_GATHER_SHORT_MSG",
            MPIR_GATHER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_scatter_vsmall_msg
        = env_to_int(
            L"MPICH_DEFAULT_SCATTER_VSMALL_MSG",
            MPIR_SCATTER_VSMALL_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_scatter_short_msg
        = env_to_int(
            L"MPICH_DEFAULT_SCATTER_SHORT_MSG",
            MPIR_SCATTER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_bcast_smp_threshold
        = env_to_int(
            L"MPICH_DEFAULT_BCAST_SMP_THRESHOLD",
            MPIR_BCAST_SMP_THRESHOLD_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_bcast_smp_ceiling
        = env_to_int(
            L"MPICH_DEFAULT_BCAST_SMP_CEILING",
            MPIR_BCAST_SMP_CEILING_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_reduce_smp_threshold
        = env_to_int(
            L"MPICH_DEFAULT_REDUCE_SMP_THRESHOLD",
            MPIR_REDUCE_SMP_THRESHOLD_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_reduce_smp_ceiling
        = env_to_int(
            L"MPICH_DEFAULT_REDUCE_SMP_CEILING",
            MPIR_REDUCE_SMP_CEILING_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_allreduce_smp_threshold
        = env_to_int(
            L"MPICH_DEFAULT_ALLREDUCE_SMP_THRESHOLD",
            MPIR_ALLREDUCE_SMP_THRESHOLD_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_allreduce_smp_ceiling
        = env_to_int(
            L"MPICH_DEFAULT_ALLREDUCE_SMP_CEILING",
            MPIR_ALLREDUCE_SMP_CEILING_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_alltoall_pair_algo_msg_threshold
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_PAIR_ALGO_MSG_THRESHOLD",
            MPIR_ALLTOALL_PAIR_ALGO_MSG_THRESHOLD_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_FLAT].MPIR_alltoall_pair_algo_procs_threshold
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_PAIR_ALGO_PROCS_THRESHOLD",
            MPIR_ALLTOALL_PAIR_ALGO_PROCS_THRESHOLD_DEFAULT,
            0
            );

    //intranode
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_bcast_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_BCAST_SHORT_MSG",
            MPIR_BCAST_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_bcast_long_msg
        = env_to_int(
            L"MPICH_INTRANODE_BCAST_LONG_MSG",
            MPIR_BCAST_LONG_MSG_DEFAULT,
            MPIR_BCAST_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_bcast_min_procs
        = env_to_int(
            L"MPICH_INTRANODE_BCAST_MIN_PROCS",
            MPIR_BCAST_MIN_PROCS_DEFAULT,
            1
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_alltoall_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_ALLTOALL_SHORT_MSG",
            MPIR_ALLTOALL_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_alltoall_medium_msg
        = env_to_int(
            L"MPICH_INTRANODE_ALLTOALL_MEDIUM_MSG",
            MPIR_ALLTOALL_MEDIUM_MSG_DEFAULT,
            MPIR_ALLTOALL_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_alltoallv_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_ALLTOALLV_SHORT_MSG",
            MPIR_ALLTOALLV_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_redscat_commutative_long_msg
        = env_to_int(
            L"MPICH_INTRANODE_REDSCAT_COMMUTATIVE_LONG_MSG",
            MPIR_REDSCAT_COMMUTATIVE_LONG_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_redscat_noncommutative_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_REDSCAT_NONCOMMUTATIVE_SHORT_MSG",
            MPIR_REDSCAT_NONCOMMUTATIVE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_allgather_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_ALLGATHER_SHORT_MSG",
            MPIR_ALLGATHER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_allgather_long_msg
        = env_to_int(
            L"MPICH_INTRANODE_ALLGATHER_LONG_MSG",
            MPIR_ALLGATHER_LONG_MSG_DEFAULT,
            MPIR_ALLGATHER_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_reduce_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_REDUCE_SHORT_MSG",
            MPIR_REDUCE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_allreduce_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_ALLREDUCE_SHORT_MSG",
            MPIR_ALLREDUCE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_gather_vsmall_msg
        = env_to_int(
            L"MPICH_INTRANODE_GATHER_VSMALL_MSG",
            MPIR_GATHER_VSMALL_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_gather_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_GATHER_SHORT_MSG",
            MPIR_GATHER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_scatter_vsmall_msg
        = env_to_int(
            L"MPICH_INTRANODE_SCATTER_VSMALL_MSG",
            MPIR_SCATTER_VSMALL_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_scatter_short_msg
        = env_to_int(
            L"MPICH_INTRANODE_SCATTER_SHORT_MSG",
            MPIR_SCATTER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_alltoall_pair_algo_msg_threshold
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_PAIR_ALGO_MSG_THRESHOLD",
            MPIR_ALLTOALL_PAIR_ALGO_MSG_THRESHOLD_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTRA_NODE].MPIR_alltoall_pair_algo_procs_threshold
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_PAIR_ALGO_PROCS_THRESHOLD",
            MPIR_ALLTOALL_PAIR_ALGO_PROCS_THRESHOLD_DEFAULT,
            0
            );

    //internode
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_bcast_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_BCAST_SHORT_MSG",
            MPIR_BCAST_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_bcast_long_msg
        = env_to_int(
            L"MPICH_INTERNODE_BCAST_LONG_MSG",
            MPIR_BCAST_LONG_MSG_DEFAULT,
            MPIR_BCAST_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_bcast_min_procs
        = env_to_int(
            L"MPICH_INTERNODE_BCAST_MIN_PROCS",
            MPIR_BCAST_MIN_PROCS_DEFAULT,
            1
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_alltoall_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_ALLTOALL_SHORT_MSG",
            MPIR_ALLTOALL_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_alltoall_medium_msg
        = env_to_int(
            L"MPICH_INTERNODE_ALLTOALL_MEDIUM_MSG",
            MPIR_ALLTOALL_MEDIUM_MSG_DEFAULT,
            MPIR_ALLTOALL_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_alltoallv_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_ALLTOALLV_SHORT_MSG",
            MPIR_ALLTOALLV_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_redscat_commutative_long_msg
        = env_to_int(
            L"MPICH_INTERNODE_REDSCAT_COMMUTATIVE_LONG_MSG",
            MPIR_REDSCAT_COMMUTATIVE_LONG_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_redscat_noncommutative_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_REDSCAT_NONCOMMUTATIVE_SHORT_MSG",
            MPIR_REDSCAT_NONCOMMUTATIVE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_allgather_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_ALLGATHER_SHORT_MSG",
            MPIR_ALLGATHER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_allgather_long_msg
        = env_to_int(
            L"MPICH_INTERNODE_ALLGATHER_LONG_MSG",
            MPIR_ALLGATHER_LONG_MSG_DEFAULT,
            MPIR_ALLGATHER_SHORT_MSG_DEFAULT
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_reduce_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_REDUCE_SHORT_MSG",
            MPIR_REDUCE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_allreduce_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_ALLREDUCE_SHORT_MSG",
            MPIR_ALLREDUCE_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_gather_vsmall_msg
        = env_to_int(
            L"MPICH_INTERNODE_GATHER_VSMALL_MSG",
            MPIR_GATHER_VSMALL_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_gather_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_GATHER_SHORT_MSG",
            MPIR_GATHER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_scatter_vsmall_msg
        = env_to_int(
            L"MPICH_INTERNODE_SCATTER_VSMALL_MSG",
            MPIR_SCATTER_VSMALL_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_scatter_short_msg
        = env_to_int(
            L"MPICH_INTERNODE_SCATTER_SHORT_MSG",
            MPIR_SCATTER_SHORT_MSG_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_alltoall_pair_algo_msg_threshold
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_PAIR_ALGO_MSG_THRESHOLD",
            MPIR_ALLTOALL_PAIR_ALGO_MSG_THRESHOLD_DEFAULT,
            0
            );
    Table[COLL_SWITCHOVER_INTER_NODE].MPIR_alltoall_pair_algo_procs_threshold
        = env_to_int(
            L"MPICH_DEFAULT_ALLTOALL_PAIR_ALGO_PROCS_THRESHOLD",
            MPIR_ALLTOALL_PAIR_ALGO_PROCS_THRESHOLD_DEFAULT,
            0
            );
}


MpiProcess::MpiProcess()
    : InitState(MSMPI_STATE_UNINITIALIZED)
    , ThreadLevel(0)
    , MasterThread(0)
    , CommWorld(nullptr)
    , CommSelf(nullptr)
    , CommParent(nullptr)
    , Attributes()
    , Heap(HeapCreate( 0, 0, 0 ))
    , DeferredRequestCounter(0)
    , ProgressCounter(0)
    , CurrentProgressThread(0)
    , ForceAsyncWorkflow(false)
    , CommLockInitialized(false)
{
    MPIU_Assertp(nullptr != Heap);

    //
    // We just Zero this for now, in case invalid usage occurs it will
    //  error reliably.  This member cannot be initialized until the
    //  threading model for the process is decided.  This occurs later
    //  during the initialize phase.
    //
    RtlZeroMemory(&CommLock, sizeof(CommLock));

    Attributes.appnum          = -1;
    Attributes.host            = 0;
    Attributes.io              = 0;
    Attributes.lastusedcode    = MPI_ERR_LASTCODE;
    Attributes.tag_ub          = 0x7fffffff;
    Attributes.universe        = MPIR_UNIVERSE_SIZE_NOT_SET;
    Attributes.wtime_is_global = 0;
}

MpiProcess::~MpiProcess()
{
    MPIU_Assert(nullptr == Heap);
}

static void init_predefined_comm(MPID_Comm* comm, int handle, const MPI_CONTEXT_ID& context_id)
{
    comm->handle = handle;
    comm->SetRef( 1 );
    comm->context_id   = context_id;
    comm->recvcontext_id = context_id;
    comm->attributes   = nullptr;
    comm->group        = nullptr;
    comm->comm_kind    = MPID_INTRACOMM_SHM_AWARE;
    InitName<MPID_Comm>( comm );
    comm->errhandler   = nullptr;
}

static MPI_RESULT Preconnect()
{
    unsigned int world_size  = Mpi.CommWorld->remote_size;
    unsigned int rank        = Mpi.CommWorld->rank;
    bool         needPreconnect;
    unsigned int nRanksPreconnect;

    char env[MAX_ENV_LENGTH];
    DWORD err = MPIU_Getenv( "MSMPI_PRECONNECT",
                             env,
                             _countof(env) );
    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        return MPI_SUCCESS;
    }
    MPI_RESULT mpi_errno =  MPIU_Parse_rank_range( rank,
                                                   env,
                                                   world_size,
                                                   &needPreconnect,
                                                   &nRanksPreconnect );

    if (mpi_errno != MPI_SUCCESS ||
        nRanksPreconnect == 0)
    {
        return mpi_errno;
    }

    //
    // We use All-to-all in the case of preconnecting all ranks
    //
    if (nRanksPreconnect >= MPID_REQUEST_PREALLOC)
    {
        //
        // To reduce perf impact, we allocate a single chunk of memory.
        //
        char* tempbuf  = static_cast<char*>(
            MPIU_Malloc( world_size * sizeof( char ) * 2 +
                         world_size * sizeof( int ) * 2 ) );

        if( tempbuf == NULL )
        {
            return MPIU_ERR_NOMEM();
        }

        int* sendrecvcnts = reinterpret_cast<int*>( tempbuf );
        int* sendrecvdis  = sendrecvcnts + world_size;

        char* sendbuf  = reinterpret_cast<char*>( sendrecvdis + world_size );
        char* recvbuf  = sendbuf + world_size;

        for( unsigned int i = 0; i < world_size; ++i )
        {
            sendrecvcnts[i] = 1;
            sendrecvdis[i]  = i;
        }

        mpi_errno = NMPI_Alltoallv( sendbuf,
                                    sendrecvcnts,
                                    sendrecvdis,
                                    MPI_CHAR,
                                    recvbuf,
                                    sendrecvcnts,
                                    sendrecvdis,
                                    MPI_CHAR,
                                    MPI_COMM_WORLD);
        MPIU_Free( tempbuf );
        return mpi_errno;
    }


    //
    // The rank does not need to preconnect to itself
    //
    if (needPreconnect == true)
    {
        nRanksPreconnect = nRanksPreconnect - 1;
    }

    //
    // Ranks that need to preconnect will send out ssend messages
    // to establish connections. Other ranks need to post
    // recvs to avoid deadlocks. We don't really care about the data
    // nor the source
    //
    MPID_Request* request[MPID_REQUEST_PREALLOC];

    for (unsigned int i = 0; i < nRanksPreconnect; i++)
    {
        //
        // We use MPID_Recv here so that we can use negative tag space.
        //
        mpi_errno = MPID_Recv( NULL,
                               0,
                               g_hBuiltinTypes.MPI_Byte,
                               MPI_ANY_SOURCE,
                               MPIR_TAG_PRECONNECT,
                               Mpi.CommWorld,
                               &request[i] );
        if (mpi_errno != MPI_SUCCESS)
        {
            return mpi_errno;
        }
    }

    //
    // Sending out messages using synchronous sends to force bidirectional
    // connection establishment.  We use MPID_Ssend here so that we can use
    // negative tag space.
    //
    if (needPreconnect == true)
    {
        for (unsigned int i = 0; i < world_size; i++)
        {
            if (i == rank) continue;

            MPID_Request* sreq;
            mpi_errno = MPID_Ssend( NULL,
                                    0,
                                    g_hBuiltinTypes.MPI_Byte,
                                    i,
                                    MPIR_TAG_PRECONNECT,
                                    Mpi.CommWorld,
                                    &sreq );

            if (mpi_errno == MPI_SUCCESS)
            {
                mpi_errno = MPIR_Wait( sreq );
                sreq->Release();
            }

            if (mpi_errno != MPI_SUCCESS)
            {
                return mpi_errno;
            }
        }
    }

    for (unsigned int i = 0; i < nRanksPreconnect; i++)
    {
        int mpi_errno2 = MPIR_Wait( request[i] );
        request[i]->Release();

        if( mpi_errno2 != MPI_SUCCESS )
        {
            //
            // Store the error but continue through the loop so we release all requests.
            //
            mpi_errno = mpi_errno2;
        }
    }
    if (mpi_errno != MPI_SUCCESS)
    {
        return mpi_errno;
    }

    return MPI_SUCCESS;
}

LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    CreateDumpFileIfConfigured(pExceptionInfo);
    return EXCEPTION_CONTINUE_SEARCH;
}

MPI_RESULT MpiProcess::Initialize(_In_ int required, _Out_opt_ int* provided)
{
    if (MSMPI_STATE_UNINITIALIZED != InterlockedCompareExchange(&InitState, MSMPI_STATE_INITIALIZING, MSMPI_STATE_UNINITIALIZED))
    {
        return MPIU_ERR_CREATE(MPI_ERR_OTHER, "**inittwice" );
    }

    MasterThread      = GetCurrentThreadId();

    if (g_IsWin8OrGreater)
    {
        ThreadLevel = min(MPI_THREAD_MULTIPLE, required);
    }
    else
    {
        ThreadLevel = min(MPI_THREAD_SERIALIZED, required);
    }

    if (IsMultiThreaded() == false)
    {
        MpiLockInitializeSingleThreadMode();
    }

    //
    // This value must be initialized after we decide our threading model.
    //
    MpiLockInitialize(&CommLock);
    CommLockInitialized = true;

    MPIU_dbg_preinit();


    MPID_Wtime_init();
    MPID_Datatype::GlobalInit();
    MPID_Op::GlobalInit();
    MPIR_Nest_init();

    //
    // We need to initialize the switchover settings here
    // before MpiProcess::InitializeCore() because the flags read in from those env
    // vars are needed when we initialize world comm, which happens
    // inside MpiProcess::InitializeCore().  However if it returns an error (bad format)
    // we can't immediately return from here because the callers of
    // Initialize will eventually branch into MPID_Abort(),
    // which relies on initializations performed by MpiProcess::InitializeCore(). So we
    // defer the error check.
    //
    int read_smp_vars_errno = SwitchoverSettings.Initialize();

    MPI_RESULT mpi_errno = InitializeCore();
    ON_ERROR_FAIL(mpi_errno);

    //
    // As explained above we check any errors returned from
    // ReadSmpEnvironemtnSettings after MPID_Init() is done.
    //
    mpi_errno = read_smp_vars_errno;
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = ADIO_Init();
    ON_ERROR_FAIL(mpi_errno);

    mpi_errno = MPIU_dbg_init( CommWorld->rank,
                               CommWorld->remote_size );
    ON_ERROR_FAIL(mpi_errno);

#ifdef HAVE_DEBUGGER_SUPPORT
    MPIR_WaitForDebugger();
#endif

    //
    // Set exception handler to create dump files if configured
    //
    if (MPIR_debug_gate == 0)
    {
        SetUnhandledExceptionFilter(TopLevelExceptionHandler);
    }

    mpi_errno = InitializeCompression();
    ON_ERROR_FAIL(mpi_errno);

    //
    // Print out the environment block if requested.
    //
    mpi_errno = PrintEnvironmentBlock();
    ON_ERROR_FAIL(mpi_errno);

    //
    //  We have initialized successfully, load up the collective
    //  switch point environment variables
    //
    mpi_errno = TunerSettings.Initialize();
    ON_ERROR_FAIL(mpi_errno);

    //
    // We move to the active state here so that Preconnect and Collective Tuning
    //  can call the the API level functions.
    //
    InterlockedExchange(&InitState, MSMPI_STATE_INITIALIZED);

    //
    //  Establish connections in advance if necessary
    //
    mpi_errno = Preconnect();
    ON_ERROR_FAIL(mpi_errno);

    //
    //  Tune the collective operations
    //
    mpi_errno = MeasureAlgorithmicSwitchPoints();
    ON_ERROR_FAIL(mpi_errno);

    if (nullptr != provided)
    {
        *provided = ThreadLevel;
    }
fn_fail:
    return mpi_errno;
}




/* The following routines provide a callback facility for modules that need
   some code called on exit.  This method allows us to avoid forcing
   MPI_Finalize to know the routine names a priori.  Any module that wants to
   have a callback calls MPIR_Add_finalize( routine, extra, priority ).

 */
typedef struct Finalize_func_t
{
    int (*f)( void * );      /* The function to call */
    void *extra_data;        /* Data for the function */
    unsigned int  priority;  /* priority is used to control the order
                                in which the callbacks are invoked */
} Finalize_func_t;
static Finalize_func_t g_fstack[16];
static unsigned int g_fstack_sp = 0;
static unsigned int g_fstack_max_priority = 0;

void MPIR_Add_finalize( int (*f)( void * ), void *extra_data, unsigned int priority )
{
    MPIU_Assert(g_fstack_sp < _countof(g_fstack));

    g_fstack[g_fstack_sp].f = f;
    g_fstack[g_fstack_sp].priority = priority;
    g_fstack[g_fstack_sp].extra_data = extra_data;
    g_fstack_sp++;

    if (priority > g_fstack_max_priority)
    {
        g_fstack_max_priority = priority;
    }
}

/* Invoke the registered callbacks */
void MPIR_Call_finalize_callbacks( _In_ unsigned int min_prio)
{
    unsigned int i, j;

    if (min_prio > g_fstack_max_priority)
        return;

    for (j = g_fstack_max_priority ; ; j--)
    {
        for (i = g_fstack_sp; i > 0; i--)
        {
            Finalize_func_t* p = &g_fstack[i - 1];
            if ((p->f != NULL) && (p->priority == j))
            {
                p->f(p->extra_data);
                p->f = NULL;
            }
        }

        if (j <= min_prio)
            break;
    }
}


double MPID_Seconds_per_tick=0.0;  /* High performance counter frequency */

void MPID_Wtime_init(void)
{
    LARGE_INTEGER n;
    QueryPerformanceFrequency(&n);
    MPID_Seconds_per_tick = 1.0 / (double)n.QuadPart;
}

double MPID_Wtick(void)
{
    return MPID_Seconds_per_tick;
}

void MPID_Wtime_todouble( const MPID_Time_t *t, double *val )
{
    *val = (double)t->QuadPart * MPID_Seconds_per_tick;
}


static wchar_t processorName[MPI_MAX_PROCESSOR_NAME] = L"";
static bool  setProcessorName = false;


/*
 * MPID_Get_processor_name()
 */
MPI_RESULT MPID_Get_processor_name(_Out_cap_(namelen) char * name, int namelen, int * resultlen)
{
    int len;
    /* FIXME: Make thread safe */
    if (!setProcessorName)
    {
        DWORD size = MPI_MAX_PROCESSOR_NAME;
        /* Use the fully qualified name instead of the short name because the
           SSPI security functions require the full name */
        GetComputerNameExW(ComputerNameDnsFullyQualified, processorName, &size);
        setProcessorName = true;
    }

    len = WideCharToMultiByte(
        CP_UTF8,
        0,
        processorName,
        -1,
        name,
        namelen,
        NULL,
        NULL
        );

    if (len == 0)
    {
        return MPI_ERR_UNKNOWN;
    }

    //
    // The value returned by WideCharToMultiByte includes the null-terminating character
    // while the length output by MPI_Get_processor_name should not.
    //
    *resultlen = len - 1;
    return MPI_SUCCESS;
}

MPIDI_Process_t MPIDI_Process = { NULL };
MPIDI_CH3U_SRBuf_element_t * MPIDI_CH3U_SRBuf_pool = NULL;


/*
 * Initialize the process group structure by using PMI calls.
 * This routine initializes PMI and uses PMI calls to setup the
 * process group structures.
 *
 */
static
MPI_RESULT
PGInitialize(
    _In_ int pg_size,
    _Outptr_ MPIDI_PG_t** pg_p
    )
{
    /*
     * Create a new structure to track the process group for our MPI_COMM_WORLD
     */
    return MPIDI_PG_Create(pg_size, PMI_KVS_Get_id(), pg_p);
}


static
MPI_RESULT
PMIInitialize(
    _Out_ int*   has_parent,
    _Out_ char** parentPortName,
    _Out_ int*   pg_rank_p,
    _Out_ int*   pg_size_p,
    _Out_ int*   appnum_p
    )
{
    /*
     * Initialize the process manangement interface (PMI),
     * and get rank and size information about our process group
     */
    MPI_RESULT mpi_errno = PMI_Init(has_parent, parentPortName);
    ON_ERROR_FAIL(mpi_errno);

    *pg_rank_p = PMI_Get_rank();
    *pg_size_p = PMI_Get_size();
    *appnum_p = PMI_Get_appnum();

fn_fail:
    return mpi_errno;
}


/*@
  MPID_Init - Initialize the device

  Return value:
  Returns 'MPI_SUCCESS' on success and an MPI error code on failure.  Failure
  can happen when, for example, the device is unable  to start or contact the
  number of processes specified by the 'mpiexec' command.

  Notes:
  This call also initializes all MPID data needed by the device.  This
  includes the 'MPID_Request's and any other data structures used by
  the device.

  This routine is used to implement both 'MPI_Init' and 'MPI_Init_thread'.

  @*/
MPI_RESULT MpiProcess::InitializeCore()
{
    int has_parent;
    MPIDI_PG_t * pg=NULL;
    int pg_rank=-1;
    int pg_size;
    int p;
    char* parentPortName = nullptr;

    if (env_is_on(L"MSMPI_FORCE_ASYNC_WORKFLOW", FALSE))
    {
        ForceAsyncWorkflow = true;
    }

    /* "Allocate" from the reserved space for builtin communicators and
       (partially) initialize predefined communicators.  comm_parent is
       intially NULL and will be allocated by the device if the process group
       was started using one of the MPI_Comm_spawn functions. */
    CommWorld = CommPool::Get( MPI_COMM_WORLD );
    init_predefined_comm(CommWorld, MPI_COMM_WORLD, MPI_WORLD_CONTEXT_ID);
    MPIR_COMML_REMEMBER( CommWorld );

    CommSelf = CommPool::Get( MPI_COMM_SELF );
    init_predefined_comm(CommSelf, MPI_COMM_SELF, MPI_SELF_CONTEXT_ID);
    MPIR_COMML_REMEMBER( CommSelf );

    /*
     * The channel might depend on PMI. Initilize it before channel initialization.
     */
    MPI_RESULT mpi_errno = PMIInitialize(&has_parent, &parentPortName, &pg_rank, &pg_size, &Attributes.appnum);
    ON_ERROR_FAIL(mpi_errno);

    /*
     * Perform channel-independent PMI initialization
     */
    mpi_errno = PGInitialize(pg_size, &pg);
    ON_ERROR_FAIL(mpi_errno);

    MPIDI_Process.my_pg = pg;
    MPIDI_Process.my_pg_rank = pg_rank;
    /* FIXME: Why do we add a ref to pg here? */
    MPIDI_PG_add_ref(pg);

    /* We intentionally call this before the channel init so that the channel
       can use the node_id info. */
    /* Ideally this wouldn't be needed.  Once we have PMIv2 support for node
       information we should probably eliminate this function. */
    mpi_errno = MPIDI_Populate_vc_node_ids(pg);
    ON_ERROR_FAIL(mpi_errno);

    /*
     * Let the channel perform any necessary initialization
     * The channel init should assume that PMI_Init has been called and that
     * the basic information about the job has been extracted from PMI (e.g.,
     * the size and rank of this process, and the process group id)
     */
    mpi_errno = MPIDI_CH3_Init(has_parent, pg_rank, pg);
    ON_ERROR_FAIL(mpi_errno);

    /*
     * Initialize the MPI_COMM_WORLD object
     */
    CommWorld->rank        = pg_rank;
    CommWorld->remote_size = pg_size;

    MPID_VCRT* vcrt = MPID_VCRT_Create(CommWorld->remote_size);
    if( vcrt == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    CommWorld->vcr = MPID_VCRT_Get_ptr(vcrt);

    if( SwitchoverSettings.SmpCollectiveEnabled == true )
    {
        //
        // Note that marking the communicator as a parent does not
        // force the use of HA collectives (or the setup of subcommunicators).
        // These will be created as appropriate by MPIR_Comm_commit.
        //
        CommWorld->comm_kind = MPID_INTRACOMM_PARENT;
    }
    else
    {
        //
        // FLAT communicators have HA collectives explicitly turned off.
        //
        CommWorld->comm_kind = MPID_INTRACOMM_SHM_AWARE;
    }

    /* Initialize the connection table on COMM_WORLD from the process group's
       connection table */
    for (p = 0; p < pg_size; p++)
    {
        CommWorld->vcr[p] = MPID_VCR_Dup( &pg->vct[p] );
    }

    mpi_errno = MPIR_Comm_commit(CommWorld);
    ON_ERROR_FAIL(mpi_errno);

    /*
     * Initialize the MPI_COMM_SELF object
     */
    CommSelf->rank        = 0;
    CommSelf->remote_size  = 1;

    vcrt = MPID_VCRT_Create(CommSelf->remote_size);
    if( vcrt == nullptr )
    {
        mpi_errno = MPIU_ERR_NOMEM();
        goto fn_fail;
    }

    CommSelf->vcr = MPID_VCRT_Get_ptr(vcrt);

    CommSelf->vcr[0] = MPID_VCR_Dup( &pg->vct[pg_rank] );

    /*
     * If this process group was spawned by a MPI application, then
     * form the MPI_COMM_PARENT inter-communicator.
     */

    if (has_parent)
    {
        mpi_errno = MPID_Comm_connect(parentPortName, NULL, 0,
                                      CommWorld, &CommParent);

        if (mpi_errno != MPI_SUCCESS)
        {
            mpi_errno = MPIU_ERR_GET(mpi_errno, "**ch3|conn_parent %s", parentPortName);
            goto fn_fail;
        }

        MPIU_Assert(CommParent != NULL);

        if (SetName<MPID_Comm>(CommParent, (char*)"MPI_COMM_PARENT") == false)
        {
            mpi_errno = MPIU_ERR_NOMEM();
            goto fn_fail;
        }
    }

fn_exit:
    delete parentPortName;
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

static void MpiPrintConnectivity()
{
    ConnectivityTable table;
    int size;
    if( MPIDI_Process.my_pg_rank == 0 )
    {
        size = MPIDI_Process.my_pg->size;
    }
    else
    {
        size = 1;
    }
    int mpiErrno = table.AllocateAndGather( size );
    if( mpiErrno != MPI_SUCCESS )
    {
        printf( "\n\nMPI Connectivity Table failed\n" );
        return;
    }

    if( MPIDI_Process.my_pg_rank != 0 )
    {
        return;
    }

    //
    // Post processing time.
    //
    table.Print();
}


/*@
  MpiProcess::Finalize - Perform the device-specific termination of an MPI job

  Return Value:
  'MPI_SUCCESS' or a valid MPI error code.  Normally, this routine will
  return 'MPI_SUCCESS'.  Only in extrordinary circumstances can this
  routine fail; for example, if some process stops responding during the
  finalize step.  In this case, 'MPID_Finalize' should return an MPI
  error code indicating the reason that it failed.

  Notes:

  Module:
  MPID_CORE

  Questions:
  Need to check the MPI-2 requirements on 'MPI_Finalize' with respect to
  things like which process must remain after 'MPID_Finalize' is called.
  @*/
MPI_RESULT MpiProcess::Finalize(void)
{
    int mpi_errno = MPI_SUCCESS;

    /*
     * Wait for all posted receives to complete.  For now we are not doing
     * this since it will cause invalid programs to hang.
     * The side effect of not waiting is that posted any source receives
     * may erroneous blow up.
     *
     * For now, we are placing a warning at the end of MPID_Finalize() to
     * inform the user if any outstanding posted receives exist.
     */
     /* FIXME: The correct action here is to begin a shutdown protocol
      * that lets other processes know that this process is now
      * in finalize.
      *
      * Note that only requests that have been freed with MPI_Request_free
      * are valid at this point; other pending receives can be ignored
      * since a valid program should wait or test for them before entering
      * finalize.
      *
      * The easist fix is to allow an MPI_Barrier over comm_world (and
      * any connected processes in the MPI-2 case).  Once the barrier
      * completes, all processes are in finalize and any remaining
      * unmatched receives will never be matched (by a correct program;
      * a program with a send in a separate thread that continues after
      * some thread calls MPI_Finalize is erroneous).
      *
      * Avoiding the barrier is hard.  Consider this sequence of steps:
      * Send in-finalize message to all connected processes.  Include
      * information on whether there are pending receives.
      *   (Note that a posted receive with any source is a problem)
      *   (If there are many connections, then this may take longer than
      *   the barrier)
      * Allow connection requests from anyone who has not previously
      * connected only if there is an possible outstanding receive;
      * reject others with a failure (causing the source process to
      * fail).
      * Respond to an in-finalize message with the number of posted receives
      * remaining.  If both processes have no remaining receives, they
      * can both close the connection.
      *
      * Processes with no pending receives and no connections can exit,
      * calling PMI_Finalize to let the process manager know that they
      * are in a controlled exit.
      *
      * Processes that still have open connections must then try to contact
      * the remaining processes.
      *
      */

    if( env_is_on_ex( L"MSMPI_CONNECTIVITY_TABLE", L"MPICH_CONNECTIVITY_TABLE", FALSE ) != FALSE )
    {
        MpiPrintConnectivity();
    }

    mpi_errno = MPID_VCRT_Release(CommSelf->GetVcrt(), 0);
    ON_ERROR_FAIL(mpi_errno);

    CleanupName<MPID_Comm>( CommSelf );

    mpi_errno = MPID_VCRT_Release(CommWorld->GetVcrt(), 0);
    ON_ERROR_FAIL(mpi_errno);

    CleanupName<MPID_Comm>( CommWorld );

    /* Re-enabling the close step because many tests are failing
     * without it, particularly under gforker */

    /* FIXME: The close actions should use the same code as the other
       connection close code */
    MPIDI_PG_Close_VCs();

    /*
     * Wait for all VCs to finish the close protocol
     */
    mpi_errno = MPIDI_CH3U_VC_WaitForClose();
    ON_ERROR_FAIL(mpi_errno);

    MPID_Recvq_flush();

#if DBG
    if( MPID_Request_mem.num_user_msg_alloc > 0 )
    {
        printf(
            "WARNING: Application leaked %d message handles\n",
            MPID_Request_mem.num_user_msg_alloc
            );
        fflush( NULL );
    }

    if( MPID_Request_mem.num_user_alloc > 0 )
    {
        printf(
            "WARNING: Application leaked %d requests\n",
            MPID_Request_mem.num_user_alloc
            );
        fflush( NULL );
    }

    MPIU_Assert( (MPID_Request_mem.num_alloc -
        ( MPID_Request_mem.num_user_alloc + MPID_Request_mem.num_user_msg_alloc )) == 0 );
#endif

    /* Note that the CH3I_Progress_finalize call has been removed; the
       CH3_Finalize routine should call it */
    mpi_errno = MPIDI_CH3_Finalize();
    ON_ERROR_FAIL(mpi_errno);

    /* Tell the process group code that we're done with the process groups.
       This will notify PMI (with PMI_Finalize) if necessary.  It
       also frees all PG structures, including the PG for COMM_WORLD, whose
       pointer is also saved in MPIDI_Process.my_pg */
    MPIDI_PG_Finalize();

    mpi_errno = PMI_Finalize();
    ON_ERROR_FAIL(mpi_errno);

#ifdef MPIDI_CH3_HAS_SPAWN
    MPIDI_CH3_FreeParentPort();
#endif

    /* Release any SRbuf pool storage */
    if (MPIDI_CH3U_SRBuf_pool)
    {
        MPIDI_CH3U_SRBuf_element_t *p, *pNext;
        p = MPIDI_CH3U_SRBuf_pool;
        while (p)
        {
            pNext = p->next;
            MPIU_Free(p);
            p = pNext;
        }
    }

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}


DECLSPEC_NORETURN static void MPIDI_CH3I_PMI_Abort(BOOL intern, int exit_code, const char *error_msg)
{
    /* FIXME: What is the scope for PMI_Abort?  Shouldn't it be one or more
       process groups?  Shouldn't abort of a communicator abort either the
       process groups of the communicator or only the current process?
       Should PMI_Abort have a parameter for which of these two cases to
       perform? */
    PMI_Abort(intern, exit_code, error_msg);
}


#pragma warning(push)
#pragma warning(disable: 4702) // unreachable code
#pragma warning(disable: 4646) // nonvoid noreturn call
_Analysis_noreturn_
DECLSPEC_NORETURN
int
MPID_Abort(
    _Inout_opt_ MPID_Comm* /*comm*/,
    _In_ BOOL intern,
    _In_ int exit_code,
    _In_z_ const char* error_msg
    )
{
    MPIDI_CH3I_PMI_Abort(intern, exit_code, error_msg);
}
#pragma warning(pop)


void *MPID_Alloc_mem( size_t size, MPID_Info* /*info_ptr*/ )
{
    void *ap;
    ap = MPIU_Malloc(size);
    return ap;
}


MPI_RESULT MPID_Free_mem( void *ptr )
{
    MPIU_Free(ptr);

    return MPI_SUCCESS;
}


/*
 * MPID_Get_universe_size - Get the universe size from the process manager
 *
 * Notes: The ch3 device requires that the PMI routines are used to
 * communicate with the process manager.  If a channel wishes to
 * bypass the standard PMI implementations, it is the responsibility of the
 * channel to provide an implementation of the PMI routines.
 */
int MPID_Get_universe_size()
{
    int universe_size = PMI_Get_universe_size();
    if( universe_size < 0 )
    {
        return MPIR_UNIVERSE_SIZE_NOT_AVAILABLE;
    }

    return universe_size;
}

void MpiProcess::PostFinalize()
{
    TypePool::Cleanup();
    CommPool::Cleanup();
    OpPool::Cleanup();

    if (CommLockInitialized)
    {
        MpiLockDelete(&CommLock);
        CommLockInitialized = false;
    }

    if (Heap != nullptr)
    {
        HeapDestroy( Heap );
        Heap = nullptr;
    }
}


void* MPIU_Malloc( _In_ SIZE_T size )
{
    return ::HeapAlloc( Mpi.Heap, 0, size );
}


void* MPIU_Calloc( _In_ SIZE_T elements, _In_ SIZE_T size )
{
    return ::HeapAlloc( Mpi.Heap, HEAP_ZERO_MEMORY, size * elements );
}


void MPIU_Free( _In_opt_ _Post_ptr_invalid_ void* pMem )
{
    if( pMem != nullptr )
    {
        ::HeapFree( Mpi.Heap, 0, pMem );
    }
}


void* MPIU_Realloc( _In_ void* pMem, _In_ SIZE_T size )
{
    return ::HeapReAlloc( Mpi.Heap, 0, pMem, size );
}
