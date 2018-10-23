// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiexec.h"
#include <direct.h>
#include <ntverp.h>

#define ENV_MPIEXEC_AFFINITY_TABLE L"MPIEXEC_AFFINITY_TABLE"
#define ENV_MPIEXEC_HWTREE_TABLE L"MPIEXEC_HWTREE_TABLE"

void mp_print_options(void)
{
    wprintf(
        L"Microsoft MPI Startup Program [Version %d.%d.%d.%d]%s\n"
        L"\n"
        L"Launches an application on multiple hosts.\n"
        L"\n"
        L"Usage:\n"
        L"\n"
        L"    mpiexec [options] executable [args] [ : [options] exe [args] : ... ]\n"
        L"    mpiexec -configfile <file name>\n"
        L"\n"
        L"Common options:\n"
        L"\n"
        L"-n <num_processes>\n"
        L"-env <env_var_name> <env_var_value>\n"
        L"-wdir <working_directory>\n"
        L"-hosts n host1 [m1] host2 [m2] ... hostn [mn]\n"
        L"-cores <num_cores_per_host>\n"
        L"-lines\n"
        L"-debug [0-3]\n"
        L"-logfile <log file>\n"
        L"\n"
        L"Examples:\n"
        L"\n"
        L"    mpiexec -n 4 pi.exe\n"
        L"    mpiexec -hosts 1 server1 master : -n 8 worker\n"
        L"\n"
        L"For a complete list of options, run mpiexec -help2\n"
        L"For a list of environment variables, run mpiexec -help3\n"
        L"\n"
        L"You can reach the Microsoft MPI team via email at askmpi@microsoft.com\n",
        MSMPI_VER_MAJOR(MSMPI_VER_EX),
        MSMPI_VER_MINOR(MSMPI_VER_EX),
        _BLDNUMMAJOR,
        _BLDNUMMINOR,
        MSMPI_BUILD_LABEL
    );
}


static void mp_print_extra_options(void)
{
    wprintf(
        L"Launches an application on multiple hosts.\n"
        L"\n"
        L"Usage:\n"
        L"\n"
        L"    mpiexec [options] executable [args] [ : [options] exe [args] : ... ]\n"
        L"    mpiexec -configfile <file name>\n"
        L"\n"
        L"All options:\n"
        L"\n"
        L"-configfile <file_name>\n"
        L" Read mpiexec command line from <file_name>.\n"
        L" The lines of filename are command line sections of the form:\n"
        L"    [options] executable [arguments]\n"
        L" The command may span multiple lines by terminating a line with '\\'.\n"
        L" Comment lines begin with '#', and empty lines are ignored.\n"
        L"\n"
        L"-optionfile <file_name>\n"
        L" Read mpiexec command options from <file_name>.\n"
        L" The lines of <file_name> are command options of the form:\n"
        L"    [option 1] [option 2] ... [option n]\n"
        L" The options may span multiple lines by terminating a line with '\\'.\n"
        L" Comment lines begin with '#' and empty lines are ignored.\n"
        L" Additional options may be used as part of the command line.\n"
        L"\n"
        L"-n <num_processes>\n"
        L"-np <num_processes>\n"
        L" Launch the specified number of processes.\n"
        L"\n"
        L"-n *\n"
        L"-np *\n"
        L" Launch one process on each available core. The absence of the -n option is\n"
        L" equivalent to -n *.\n"
        L"\n"
        L"-machinefile <file_name>\n"
        L" Read the list of hosts from <file_name> on which to run the application.\n"
        L" The format is one host per line, optionally followed by the number of cores.\n"
        L" Comments are from '#' to end of line, and empty lines are ignored.\n"
        L" The -n * option uses the sum of cores in the file.\n"
        L"\n"
        L"-host <host_name>\n"
        L" Launch the application on <host_name>.\n"
        L" The -n * option uses 1 core.\n"
        L"\n"
        L"-hosts n host1 [m1][,mask[:group]] host2 [m2] ... hostn [mn]\n"
        L" Launch the application on n hosts with m(i) processes on host(i).\n"
        L" The number of processes on each host is optional, and defaults to 1.\n"
        L" The total number of processes is the sum of m1 + ... + mn.\n"
        L" For each process count, an optional list of affinity masks can be specified\n"
        L" that will assign the process to run on specific cores.  The group can be \n"
        L" specified for systems that support > 64 logical cores.\n"
        L"\n"
        L"-c <num_processes>\n"
        L"-cores <num_processes>\n"
        L" Set hosts to <num_processes> cores each. This option overrides the cores count\n"
        L" specified for each host by the -hosts or the -machinefile options in all sections.\n"
        L"\n"
        L"-a\n"
        L"-affinity\n"
        L" Set the affinity mask to a single core for each of the launched processes,\n"
        L" spreading them as far as possible throughout the available cores. This is the\n"
        L" equivalent of using the \"-affinity_layout spr:L\" option.\n"
        L"\n"
        L"-al <algo>[:<target>]\n"
        L"-al <algo>[:<stride>:<target>]\n"
        L"-affinity_layout <algo>[:<target>] \n"
        L"-affinity_layout <algo>[:<stride>:<target>]\n"
        L" Set the algorithm used to distribute launched processes to the compute cores,\n"
        L" and optionally specify the stride and the affinity target. The process\n"
        L" affinity is set to the specified target. Setting the stride in addition to\n"
        L" the target provides finer granularity (for example, \"spr:P:L\" will spread\n"
        L" processes across physical cores, and will bind each process to a single\n"
        L" logical core). If no stride is specified, it is assumed to be the same as the\n"
        L" target (for example, \"seq:p\" is the same as \"seq:p:p\"). Specifying affinity\n"
        L" with this option overrides any setting for the MPIEXEC_AFFINITY environment\n"
        L" variable.\n"
        L"\n"
        L" The following table lists the values for the <algo> parameter:\n"
        L"  Value      Description\n"
        L"  --------   -----------------------------------------------------------------\n"
        L"  0          No affinity (overrides any setting for MPIEXEC_AFFINITY).\n"
        L"  1 or spr   Spread: Distribute the processes as far as possible. (Default)\n"
        L"  2 or seq   Sequential: Distribute the processes sequentially.\n"
        L"  3 or bal   Balanced: Distribute the processes over the available NUMA nodes.\n"
        L"\n"
        L" The following table lists the values for the <target> and <stride> parameters:\n"
        L"  Value      Description\n"
        L"  --------   -----------------------------------------------------------------\n"
        L"  l or L     Assign each process to a logical core. (Default)\n"
        L"  p or P     Assign each process to a physical core.\n"
        L"  n or N     Assign each process to a NUMA node.\n"
        L"\n"
        L"-aa\n"
        L"-affinity_auto\n"
        L" affinity_auto complements affinity_layout's behavior. It targets the\n"
        L" case where multiple jobs are running on the same node. When it is set, mpiexec\n"
        L"    - Reads in the affinity settings which are published by other jobs running\n"
        L"    on the same node, and determines the cores that are in use.\n"
        L"    - Runs the specified MPIEXEC_AFFINITY algorithm avoiding the cores that are\n"
        L"    in use, and calculates the affinity setting for this job.\n"
        L"    - Publishes the calculated affinity settings so that upcoming jobs can avoid\n"
        L"    the cores that are in use.\n"
        L" This way, multiple jobs on the same node can use the cores in a mutually exclusive\n"
        L" manner.\n"
        L"\n"
        L"-dir <working_directory>\n"
        L"-wdir <working_directory>\n"
        L" Set the working directory for the launched application. The directory may be\n"
        L" a local or remote path and may include environment variables to be expanded at\n"
        L" the target host. The maximum accepted length of the path is 260 characters.\n"
        L"\n"
        L"-env <env_var_name> <env_var_value>\n"
        L" Set an environment variable for the launched application.\n"
        L"\n"
        L"-genvlist <env_var_name1>[,env2,env3,...]\n"
        L" Pass the values of the specified environment variables to the launched\n"
        L" application. The list is a comma separated list of environment variables.\n"
        L"\n"
        L"-exitcodes\n"
        L" Print the processes exit codes at the end of the run.\n"
        L"\n"
        L"-priority {0-4}\n"
        L" Set the process startup priority class.\n"
        L" The priority values are: 0=idle, 1=below, 2=normal, 3=above, 4=high.\n"
        L" the default is -priority normal.\n"
        L"\n"
        L"-p <port>\n"
        L"-port <port>\n"
        L" Specify the port that smpd is listening on.\n"
        L"\n"
        L"-path <path1>[;<path2>...]\n"
        L" Search for the application on the specified path on the target host.\n"
        L" To specify multiple paths, separate paths with a semicolon ';'.\n"
        L" Does not replace or append the PATH environment variable.\n"
        L"\n"
        L"-timeout <seconds>\n"
        L" Set the timeout for the job.\n"
        L"\n"
        L"-job <string>\n"
        L" Associate the application with a job created by the Windows HPC Server.\n"
        L"\n"
        L"-l\n"
        L"-lines\n"
        L" Prefix the output with the process rank.\n"
        L"\n"
        L"-d [level]\n"
        L"-debug [level]\n"
        L" Print debug output to stderr. Level is: 0=none, 1=error, 2=debug 3=both.\n"
        L" When level is not specified '2=debug' is used.\n"
        L"\n"
        L"-logFile <log file>\n"
        L" Redirect logs to the given file.\n"
        L"\n"
        L"-genv, -gpath, -gdir, -gwdir, -ghost, -gmachinefile\n"
        L" These options are the global version of the corresponding option affecting all\n"
        L" sections of the command line.\n"
        L"\n"
        L"-pwd <string>\n"
        L"Authenticate the user with the provided password. This option is only valid when\n"
        L"MS-MPI Launch Service is being used.\n"
        L"\n"
        L"-saveCreds\n"
        L"Notify the launch service to save credentials. This option is only valid when -pwd\n"
        L"is provided.\n"
        L"After a successful invocation of saveCreds, it is not necessary to provide\n"
        L"the password with -pwd unless the password is changed.\n"
        L"\n"
        L"-unicode\n"
        L"Switch mpiexec output to unicode stream. This only affects the output of mpiexec.\n"
        L"Unicode path and executables are supported with or without this option.\n"
        L"\n"
        L"-?\n"
        L"-help\n"
        L" Display a list of common options for mpiexec command line.\n"
        L"\n"
        L"-??\n"
        L"-help2\n"
        L" Display this help message.\n"
        L"\n"
        L"-???\n"
        L"-help3\n"
        L" List environment variables.\n"
        L"\n"
        L"\n"
        L"Examples:\n"
        L"\n"
        L" Run four pi.exe processes on the local host with four cores:\n"
        L"    mpiexec pi.exe\n"
        L"    mpiexec -n * pi\n"
        L"\n"
        L" Run one master process and three worker processes on the local host with four\n"
        L" cores:\n"
        L"    mpiexec -n 1 master : worker\n"
        L"\n"
        L" Run one master process and 31 worker processes on the hosts listed in the\n"
        L" hosts.txt file (which lists four hosts with 8 cores each):\n"
        L"    mpiexec -gmachinefile hosts.txt -n 1 master : worker\n"
        );
}


static void mp_print_environment_variables(void)
{
    wprintf(
        L"MPIEXEC environment variables:\n"
        L"These environment variables are equivalent to the command line options and take\n"
        L"effect only if the equivalent command line option is not specified. These\n"
        L"environment variables should be set before mpiexec is launched.\n"
        L"\n"
        L"Environment variables with numerical values will use the closer of the\n"
        L"specified maximum or minimum allowed value for that environment variable if the\n"
        L"specified value is out of range.\n"
        L"\n"
        L"MPIEXEC_AFFINITY=[<algo>[:<target>]] or [<algo>[:<stride>:<target>]]\n"
        L" Set the algorithm used to distribute launched processes to the compute cores,\n"
        L" and optionally specify the stride and the affinity target. The process\n"
        L" affinity is set to the specified target. Setting the stride in addition to\n"
        L" the target provides finer granularity (for example, \"spr:p:l\" will spread\n"
        L" processes across physical cores, and will bind each process to a single\n"
        L" logical core). If no stride is specified, it is assumed to be the same as the\n"
        L" target (for example, \"seq:p\" is the same as \"seq:p:p\"). Specifying affinity\n"
        L" with the -affinity or -affinity_layout options overrides any setting on this\n"
        L" environment variable.\n"
        L"  \n"
        L" The following table lists the values for the <algo> parameter:\n"
        L"  Value      Description\n"
        L"  --------   -----------------------------------------------------------------\n"
        L"  0          No affinity (overrides any setting for MPIEXEC_AFFINITY).\n"
        L"  1 or spr   Spread: Distribute the processes as far as possible.\n"
        L"  2 or seq   Sequential: Distribute the processes sequentially.\n"
        L"  3 or bal   Balanced: Distribute the processes over the available NUMA nodes.\n"
        L"\n"
        L" The following table lists the values for the <target> and <stride> parameters:\n"
        L"  Value      Description\n"
        L"  --------   -----------------------------------------------------------------\n"
        L"  l or L     Assign each process to a logical core. (Default)\n"
        L"  p or P     Assign each process to a physical core.\n"
        L"  n or N     Assign each process to a NUMA node.\n"
        L"\n"
        L"MPIEXEC_AFFINITY_AUTO=[0|1]\n"
        L" MPIEXEC_AFFINITY_AUTO complements MPIEXEC_AFFINITY's behavior. It targets the\n"
        L" case where multiple jobs are running on the same node. When it is set, mpiexec\n"
        L"    - Reads in the affinity settings which are published by other jobs running\n"
        L"    on the same node, and determines the cores that are in use.\n"
        L"    - Runs the specified MPIEXEC_AFFINITY algorithm avoiding the cores that are\n"
        L"    in use, and calculates the affinity setting for this job.\n"
        L"    - Publishes the calculated affinity settings so that upcoming jobs can avoid\n"
        L"    the cores that are in use.\n"
        L" This way, multiple jobs on the same node can use the cores in a mutually exclusive\n"
        L" manner.\n"
        L"\n"
        L"MPIEXEC_TIMEOUT=seconds\n"
        L" Set the timeout for the job.\n"
        L"\n"
        L"MPIEXEC_CONNECT_RETRIES\n"
        L" Set the number of retries for connection failures for mpiexec and smpd.\n"
        L" The default value is 12.\n"
        L"\n"
        L"MPIEXEC_CONNECT_RETRY_INTERVAL\n"
        L" Set the number of seconds to wait before retrying a failed connection.\n"
        L" The default value is 5.\n"
        L"\n"
        L"MPIEXEC_DISABLE_KERB=[0|1|2]\n"
        L" When set to 1, MS-MPI process management (mpiexec and smpd) will not use Kerberos.\n"
        L" When set to 2, MS-MPI process management will use NTLM.\n"
        L"\n"
        L"MPICH environment variables:\n"
        L"The MPICH environment variables are set using the -env, -genv or -genvlist\n"
        L"command line options. These variables are visible to the launched application\n"
        L"and are affecting its execution.\n"
        L"\n"
        L"MPICH_NETMASK=address/subnet\n"
        L" When set, limits the Sockets and Network Direct interconnects to use only\n"
        L" connections that match the network mask. For example, the following value will\n"
        L" use only networks that match 10.0.0.x.: \n"
        L"    -env MPICH_NETMASK 10.0.0.5/255.255.255.0\n"
        L"         or\n"
        L"    -env MPICH_NETMASK 10.0.0.5/24\n"
        L"\n"
        L"MPICH_SOCKET_BUFFER_SIZE=size (bytes)\n"
        L" Set the Sockets send and receive buffer sizes in bytes (SO_SNDBUF and\n"
        L" SO_RCVBUF). The default is 32768.\n"
        L"\n"
        L"MPICH_SOCKET_RBUFFER_SIZE=size (bytes)\n"
        L" Set the Sockets receive buffer size in bytes (SO_RCVBUF).\n"
        L" Overrides any values specified by MPICH_SOCKET_BUFFER_SIZE.\n"
        L" The default is 32768.\n"
        L"\n"
        L"MPICH_SOCKET_SBUFFER_SIZE=size (bytes)\n"
        L" Set the Sockets send buffer size in bytes (SO_SNDBUF).\n"
        L" Overrides any value specified by MPICH_SOCKET_BUFFER_SIZE.\n"
        L" The default is 32768.\n"
        L"\n"
        L"MPICH_PORT_RANGE=min,max\n"
        L" **Deprecated** see MSMPI_PORT_RANGE.\n"
        L"\n"
        L"MPICH_DISABLE_ND\n"
        L" **Deprecated** see MSMPI_DISABLE_ND.\n"
        L"\n"
        L"MPICH_DISABLE_SHM\n"
        L" **Deprecated** See MSMPI_DISABLE_SHM.\n"
        L"\n"
        L"MPICH_DISABLE_SOCK=[0|1]\n"
        L" **Deprecated** See MSMPI_DISABLE_SOCK.\n"
        L"\n"
        L"MPICH_PROGRESS_SPIN_LIMIT\n"
        L" **Deprecated** See MSMPI_PROGRESS_SPIN_LIMIT.\n"
        L"\n"
        L"MPICH_SHM_EAGER_LIMIT\n"
        L" **Deprecated** See MSMPI_SHM_EAGER_LIMIT.\n"
        L"\n"
        L"MPICH_SOCK_EAGER_LIMIT\n"
        L" **Deprecated** See MSMPI_SOCK_EAGER_LIMIT.\n"
        L"\n"
        L"MPICH_ND_EAGER_LIMIT\n"
        L" **Deprecated** See MSMPI_ND_EAGER_LIMIT.\n"
        L"\n"
        L"MPICH_ND_ENABLE_FALLBACK\n"
        L" **Deprecated** See MSMPI_ND_ENABLE_FALLBACK.\n"
        L"\n"
        L"MPICH_ND_ZCOPY_THRESHOLD\n"
        L" **Deprecated** See MSMPI_ND_ZCOPY_THRESHOLD.\n"
        L"\n"
        L"MPICH_ND_MR_CACHE_SIZE\n"
        L" **Deprecated** See MSMPI_ND_MR_CACHE_SIZE.\n"
        L"\n"
        L"MPICH_CONNECT_RETRIES\n"
        L" **Deprecated** See MSMPI_CONNECT_RETRIES.\n"
        L"\n"
        L"MPICH_INIT_BREAK\n"
        L" **Deprecated** See MSMPI_INIT_BREAK.\n"
        L"\n"
        L"MPICH_CONNECTIVITY_TABLE\n"
        L" **Deprecated** See MSMPI_CONNECTIVITY_TABLE.\n"
        L"\n"
        L"MSMPI environment variables:\n"
        L"The MSMPI environment variables are set using the -env, -genv or -genvlist\n"
        L"command line options. These variables are visible to the launched application\n"
        L"and affect its execution.\n"
        L"\n"
        L"MSMPI_PORT_RANGE=min,max\n"
        L" Set the Sockets listener port range.\n"
        L"\n"
        L"MSMPI_ND_PORT_RANGE=min,max\n"
        L" Set the Network Direct listener port range.\n"
        L"\n"
        L"MSMPI_DISABLE_ND=[0|1]\n"
        L" When set to 1, disables the use of the Network Direct interconnect.\n"
        L"\n"
        L"MSMPI_DISABLE_SHM=[0|1]\n"
        L" When set to 1, disables the use of the Shared Memory interconnect\n"
        L"\n"
        L"MSMPI_DISABLE_SOCK=[0|1]\n"
        L" When set to 1, disables the use of the Sockets interconnect.\n"
        L"\n"
        L"MSMPI_PROGRESS_SPIN_LIMIT=number\n"
        L" Set the progress engine fixed spin count limit (1 - 2G).\n"
        L" The default of 0 uses an adaptive spin count limit.\n"
        L" For oversubscribed cores use a low value fixed spin limit (e.g., 16)\n"
        L"\n"
        L"MSMPI_SHM_EAGER_LIMIT=size (bytes)\n"
        L" Set the message size above which to use the rendezvous protocol for shared\n"
        L" memory communication. The default is 128000 (1500 - 2G).\n"
        L"\n"
        L"MSMPI_SOCK_EAGER_LIMIT=size (bytes)\n"
        L" Set the message size above which to use the rendezvous protocol for sockets\n"
        L" communication. The default is 128000 (1500 - 2G).\n"
        L"\n"
        L"MSMPI_ND_EAGER_LIMIT=size (bytes)\n"
        L" Set the message size above which to use the rendezvous protocol for\n"
        L" Network Direct communication. The default is 128000 (1500 - 2G).\n"
        L"\n"
        L"MSMPI_ND_ENABLE_FALLBACK=[0|1]\n"
        L" When set to 1, enables the use of the sockets interconnect if the Network\n"
        L" Direct interconnect is enabled but connection over Network Direct fails.\n"
        L"\n"
        L"MSMPI_ND_ZCOPY_THRESHOLD=size (bytes)\n"
        L" Set the message size above which to perform zcopy transfers.\n"
        L" The default value of -1 disables zcopy transfers.\n"
        L" The value 0 uses the threshold indicated by the Network Direct provider.\n"
        L"\n"
        L"MSMPI_ND_MR_CACHE_SIZE=size (MB)\n"
        L" Set the size in megabytes of the Network Direct memory registration cache.\n"
        L" The default is half of physical memory divided by the number of cores.\n"
        L"\n"
        L"MSMPI_ND_SENDQ_DEPTH=number\n"
        L" Set the maximum number of sends that can be outstanding on a Network\n"
        L" Direct QueuePair, from 1 to 128, rounded up to the nearest power of two\n"
        L" (default 16).  Applies only to Network Direct v2.\n"
        L"\n"
        L"MSMPI_ND_RECVQ_DEPTH=number\n"
        L" Set the maximum number of receives that can be outstanding on a Network\n"
        L" Direct QueuePair, from 2 to 128, rounded up to the nearest power of two\n"
        L" (default 128).  Applies only to Network Direct v2.\n"
        L"\n"
        L"MSMPI_CONNECT_RETRIES=n\n"
        L" Set the number of times to retry Network Direct or Socket connection.\n"
        L" The default is 5.\n"
        L"\n"
        L"MSMPI_INIT_BREAK=[preinit|all|*|<range>]\n"
        L" When set, the application debug breaks at MPI initialization.\n"
        L" preinit - break before MPI is initialized on all ranks.\n"
        L" all     - break after MPI is initialized on all ranks.\n"
        L" *       - break after MPI is initialized on all ranks.\n"
        L" <range> - break after MPI is initialized on ranks specified in <range>.\n"
        L" The rank range is in the form a,c-e; where a c and e are decimal integers.\n"
        L"\n"
        L"MSMPI_CONNECTIVITY_TABLE=[0|1]\n"
        L" When set to 1, displays information about the communication channels used.\n"
        L"\n"
        L"MSMPI_SOCK_COMPRESSION_THRESHOLD=n\n"
        L" When set, the MPI library attempts to compress all messages communicated using\n"
        L" the sockets channel that are larger, in bytes, than the specified threshold\n"
        L" (threshold values that are below the minimum will be rounded up to the minimum\n"
        L" threshold of 512).\n"
        L"\n"
        L"MSMPI_HA_COLLECTIVE=[all|<collective>]\n"
        L" Specifies which hierarchy-aware collective algorithms to use. These algorithms\n"
        L" rely on the hierarchy of rank interconnects to achive better performance.\n"
        L" The default is to enable all available HA algorithms.\n"
        L" off          - disable all hierarchy aware algorithms\n"
        L" all          - enable hierarchy awareness for Bcast, Barrier, Reduce,\n"
        L"                and Allreduce operations.\n"
        L" <collective> - enable hierarchy awareness for one or more of Bcast,\n"
        L"                Barrier, Reduce, and Allreduce.\n"
        L" The collectives are specified in the form a[,b]*; where a, b are one of Bcast,\n"
        L" Barrier, Reduce, or Allreduce.  Any combination of operations may be\n"
        L" specified.\n"
        L"\n"
        L"MSMPI_TUNE_COLLECTIVE=[all|<collective>]\n"
        L" When set, the MPI library runs a series of trials to determine what data size\n"
        L" to use for various algorithms that make up a collective operation.\n"
        L" all          - tune all collective operations that have multiple algorithms.\n"
        L" <collective> - tune specified collective operations to optimize performance.\n"
        L" The collectives are specified in the form of a[,b]* where a, b are one of\n"
        L" Bcast, Reduce, Allreduce, Gather, Allgather, Reducescatter, and Alltoall. Any\n"
        L" combination of operations may be specified.\n"
        L"\n"
        L"MSMPI_TUNE_PRINT_SETTINGS=[optionfile|cluscfg|mpiexec]\n"
        L" When set in concert with MSMPI_TUNE_COLLECTIVE, the MPI library produces\n"
        L" the values that are determined to be optimal for selecting the available\n"
        L" collective algorithms.\n"
        L" optionfile - print the values determined for optimal performance in\n"
        L"              <var> <value> format, one on each line. The resulting file can be\n"
        L"              used with the -optionfile argument.\n"
        L" cluscfg    - print the values determined for optimal performance in a script\n"
        L"              format that will set the environment via cluscfg.\n"
        L" mpiexec    - print the values determined for optimal performance in a block of\n"
        L"              -env flags that can be passed to mpiexec.\n"
        L"\n"
        L"MSMPI_TUNE_SETTINGS_FILE=<file name>\n"
        L" When used in concert with MSMPI_TUNE_COLLECTIVE writes the output of tuning to\n"
        L" the specified file. The default is to write the output on the console. The\n"
        L" output is always written by rank 0.\n"
        L"\n"
        L"MSMPI_TUNE_TIME_LIMIT=n\n"
        L" When set in concert with MSMPI_TUNE_COLLECTIVE, changes the default limit\n"
        L" used, in seconds, for running the trials to optimize the collective\n"
        L" operations. This time limit is a suggestion to the MPI library and does not\n"
        L" represent a hard limit. Every collective that is tuned is run a minimum of\n"
        L" five times for each data size. The default time limit is 60 seconds.\n"
        L"\n"
        L"MSMPI_TUNE_ITERATION_LIMIT=n\n"
        L" When set in concert with MSMPI_TUNE_COLLECTIVE, changes the default maximum\n"
        L" number of trials for each data size and algorithm. The default iteration limit\n"
        L" is 10000. The minimum value is five (5).\n"
        L"\n"
        L"MSMPI_TUNE_SIZE_LIMIT=n\n"
        L" When set in concert with MSMPI_TUNE_COLLECTIVE, changes the default maximum\n"
        L" data size in bytes, to attempt for time trials of collective algorithms. Every\n"
        L" data size that is a power of two that is less than the size limit is tested.\n"
        L" The default size limit is 16777216. The minimum value is one (1).\n"
        L"\n"
        L"MSMPI_TUNE_VERBOSE=[0|1|2]\n"
        L" When set in concert with MSMPI_TUNE_COLLECTIVE, the MPI library produces\n"
        L" verbose output while running the trials to optimize the collective operations.\n"
        L" Verbose output is off by default.  All output is written to the console by\n"
        L" rank 0.\n"
        L" 0 - verbose output is turned off.\n"
        L" 1 - print data tables.\n"
        L" 2 - debug output.\n"
        L"\n"
        L"MSMPI_PRECONNECT=[all|*|<range>]\n"
        L" When set, the MPI library attempts to establish connections between processes.\n"
        L" all     - all processes will be fully connected after MPI is initialized.\n"
        L" *       - all processes will be fully connected after MPI is initialized.\n"
        L" <range> - each process in <range> will be connected to all other processes\n"
        L"           after MPI is initialized.\n"
        L" The rank range is in the form a,c-e; where a c and e are decimal integers.\n"
        L"\n"
        L"MSMPI_DUMP_MODE=[0|1|2|3|4]\n"
        L" When set, the MPI library generates dump files of processes when MPI errors\n"
        L" are encountered.\n"
        L" 0 - No dump files are generated when MPI errors are encountered.\n"
        L" 1 - Processes that encounter MPI errors generate a minidump.\n"
        L" 2 - All processes in the job generate a minidump when any process terminates\n"
        L"     due to an MPI error.\n"
        L" 3 - Processes that encounter an MPI error generate a full memory dump.\n"
        L" 4 - All processes in the job generate a full memory dump when any process\n"
        L"     terminates due to an MPI error.\n"
        L" A process that encounters multiple errors will overwrite the dump file, and\n"
        L" only the state at the time of the last error is recorded.\n"
        L"\n"
        L"MSMPI_DUMP_PATH=<path>\n"
        L" When set in conjunction with MSMPI_DUMP_MODE, sets the minidump path. Assuming\n"
        L" rank is the process' rank in MPI_COMM_WORLD, the dump files are stored at the\n"
        L" provided path (or the default path of %%USERPROFILE%% if the environment\n"
        L" variable is unset) with the following names: mpi_dump_<rank>.dmp. When used in\n"
        L" Windows HPC Server environment, jobid.taskid.taskinstanceid is added before\n"
        L" the rank.\n"
        L"\n"
        L"MSMPI_JOB_CONTEXT=<string>\n"
        L" When MSMPI is used in a managed setting outside of the Microsoft HPC product,\n"
        L" the node manager entity may need to authorize the launch of the MPI job on the\n"
        L" requested set of resources.  The MSMPI_JOB_CONTEXT is a string that is passed\n"
        L" to the node manager entity as part of start up that can be used to identify\n"
        L" this instance of MpiExec.exe.\n"
        L"\n"
        L"MSMPI_PRINT_ENVIRONMENT_BLOCK=[0|1]\n"
        L" When set to 1 each MPI process will write the contents of its environment\n"
        L" block to the standard output stream or the file specified using the\n"
        L" MSMPI_PRINT_ENVIRONMENT_BLOCK_FILE environment variable.\n"
        L"\n"
        L"MSMPI_PRINT_ENVIRONMENT_BLOCK_FILE=prefix\n"
        L" When the MSMPI_PRINT_ENVIRONMENT_BLOCK option is used, this optional\n"
        L" environment variable causes the environment block contents to be written to a\n"
        L" file instead of the console. The file name is the specified prefix with the\n"
        L" PID appended as: prefix_<pid>.txt, where <pid> is replaced with the process\n"
        L" identifier assigned by the operating system. The file is written to the\n"
        L" current working directory of the process unless the prefix contains a path or\n"
        L" an environment variable that expands to a path.\n"
        L"\n"
        L"PMI environment variables:\n"
        L"The pmi environment variables are set by smpd to the launched application.\n"
        L"\n"
        L"PMI_APPNUM=appnum\n"
        L" The application number on mpiexec command line, zero based.\n"
        L" (the applications on mpiexec are ':' separated)\n"
        L"\n"
        L"PMI_RANK=rank\n"
        L" The rank of that process in the MPI world.\n"
        L"\n"
        L"PMI_SIZE=size\n"
        L" The size of the MPI world\n"
        L"\n"
        L"PMI_SMPD_KEY=n\n"
        L" The smpd local id of the launched process.\n"
        L"\n"
        L"PMI_SPAWN=[0|1]\n"
        L" The value 1 indicates the application was spawned by another MPI application\n"
        L"PMI_PARENT_PORT_NAME=<string>\n"
        L" The name of the port opened by the parent processes for spawned processes to connect to\n"
        );
}


static inline wchar_t**
find_cmdline_block_seperator(
    _In_ wchar_t** argv
    )
{
    while( *argv != NULL &&
           CompareStringW( LOCALE_INVARIANT,
                           0,
                           *argv,
                           -1,
                           L":",
                           -1 ) != CSTR_EQUAL )
    {
        argv++;
    }

    return argv;
}


bool CheckParameter(
    _In_ PCWSTR param,
    _In_reads_z_(numValues) wchar_t* values[],
    _In_ ULONG numValues
    )
{
    for( ULONG i = 0; i < numValues; ++i )
    {
        if( CompareStringW(
                LOCALE_INVARIANT,
                NORM_IGNORECASE,
                param,
                -1,
                values[i],
                -1 ) == CSTR_EQUAL
          )
        {
            return true;
        }
    }

    return false;
}


static mp_host_t*
new_host_node(
    _In_ const wchar_t* host_name,
    _In_ int nproc
    )
{
    return new mp_host_t(host_name,nproc);
}


_Success_( return == NULL )
_Ret_maybenull_
const wchar_t*
mp_parse_hosts_argv(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_host_pool_t* pool
    )
{
    if(**argvp == NULL || !isposnumber(**argvp))
        return L"Error: expecting a positive number of hosts following the -hosts option.";

    mp_host_t** phosts = &pool->hosts;
    int nhosts = _wtoi(**argvp);

    //
    // In case of overflowing, _wtoi returns INT_MAX and we'll catch it as well.
    //
    if( nhosts > MSMPI_MAX_RANKS )
    {
        return L"Error: number of hosts exceeds the maximum supported ranks.";
    }

    int nproc = nhosts;
    BOOL bHostExplicit = FALSE;

    ++*argvp;

    for(int i = 0; i < nhosts; i++)
    {
        if(**argvp == NULL)
            return L"Error: expecting a host name following the -hosts option.";

        mp_host_t* host = new_host_node(**argvp, 1);
        if(host == NULL)
            return L"Error: not enough memory to allocate a host node.";

        ++*argvp;
        if(**argvp != NULL)
        {
            //
            // If there is a , after the digit, it will provide the
            // explicit affinity masks for processes.
            //
            const wchar_t* pSep = skip_digits(**argvp);
            if( L',' == *pSep )
            {
                if( i > 0 && FALSE == bHostExplicit )
                {
                    return L"Error: All hosts must specify explicit affinity if any do";
                }

                smpd_process.affinityOptions.isSet       = TRUE;
                bHostExplicit = smpd_process.affinityOptions.isExplicit = TRUE;

                wchar_t digits[64];
                HRESULT hr = StringCchCopyNW(
                    digits,
                    _countof(digits),
                    **argvp,
                    pSep - (**argvp));
                if( FAILED(hr) )
                {
                    return L"Error: not enough memory to copy number of processes following a host";
                }

                host->nproc = _wtoi(digits);
                if(host->nproc < 1)
                {
                    return L"Error: expecting a positive number of cores following the host name.";
                }

                if( host->nproc + static_cast<unsigned int>(nproc) - 1 >
                    MSMPI_MAX_RANKS )
                {
                    return L"Error: number of processes requested by the host entry exceeds the maximum supported ranks.";
                }

                host->explicit_affinity = static_cast<HWAFFINITY*>(
                    malloc( sizeof(*host->explicit_affinity) * host->nproc ) );
                if( NULL == host->explicit_affinity )
                {
                    return L"Error: Unable to allocate memory for explicit affinity list.";
                }
                host->explicit_count = host->nproc;

                for( int j = 0; j < host->nproc; j++ )
                {
                    if( L',' != *pSep )
                    {
                        return L"Error: Affinity mask count mismatch in hosts list specifier.";
                    }

                    //
                    // Insert the explicit_affinity elements in
                    // reverse so we can remove them from the tail
                    // later.
                    //
                    pSep++;

                    //skip any 0x prefixes.
                    if (L'0' == pSep[0] && L'x' == pSep[1]) {
                        pSep += 2;
                    }

                    if( 1 != swscanf_s(
                            pSep,
                            L"%I64x",
                            &host->explicit_affinity[ host->explicit_count - j - 1 ].Mask )
                        )
                    {
                        return L"Error: Invalid affinity mask specifier in hosts list.";
                    }

                    pSep = skip_hex(pSep);

                    if (L':' == *pSep)
                    {
                        pSep++;
                        if( 1 != swscanf_s(
                                pSep,
                                L"%hu",
                                &host->explicit_affinity[ host->explicit_count - j - 1 ].GroupId )
                            )
                        {
                            return L"Error: Invalid affinity group specifier in hosts list.";
                        }
                        pSep = skip_digits(pSep);
                    }
                    else
                    {
                        host->explicit_affinity[ host->explicit_count - j - 1 ].GroupId = 0;
                    }
                }

                nproc += host->nproc - 1;
                ++*argvp;
            }
            else
            {
                if( i > 0 && FALSE != bHostExplicit )
                {
                    return L"Error: All hosts must specify explicit affinity if any do";
                }

                if(isnumber(**argvp))
                {
                    host->nproc = _wtoi(**argvp);
                    if(host->nproc < 1)
                    {
                        return L"Error: expecting a positive number of cores following the host name.";
                    }
                    if( host->nproc + static_cast<unsigned int>(nproc) - 1 >
                        MSMPI_MAX_RANKS )
                    {
                        return L"Error: number of processes requested by the host entry exceeds the maximum supported ranks.";
                    }

                    nproc += host->nproc - 1;
                    ++*argvp;
                }
            }
        }

        *phosts = host;
        phosts = reinterpret_cast<mp_host_t**>( &host->Next );
    }

    ASSERT( nproc <= MSMPI_MAX_RANKS );
    pool->nproc = nproc;
    return NULL;
}


static int
count_white_spaces(
    _In_z_ const wchar_t* p
    )
{
    int n = 0;

    while(*p != L'\0')
    {
        if( iswspace(*p++) )
        {
            n++;
            while( iswspace(*p) )
            {
                ++p;
            }
        }
    }

    return n;
}


const wchar_t*
mp_parse_hosts_string(
    _Inout_z_ wchar_t* p,
    mp_host_pool_t* pool
    )
{
    //
    // Find out the size of argv and allocate it
    //
    int argc = count_white_spaces(p) + 1;
    wchar_t** argv = (wchar_t**)malloc((argc + 1) * sizeof(wchar_t*));
    if(argv == NULL)
        return L"Error: not enough memory to allocate the hosts string";

    int n = 0;
    argv[n++] = p;

    while(*p != L'\0')
    {
        if( iswspace(*p++) )
        {
            //
            // set the first white space to null char and seek for the next
            // argv on this string
            //
            *(p - 1) = L'\0';
            while( iswspace(*p) )
            {
                ++p;
            }
            argv[n++] = p;
        }
    }

    ASSERT(n == argc);
    argv[n] = NULL;

    //
    // Here we have the string parsed into argv format
    //
    wchar_t** argv_ = argv;
    const wchar_t* error = mp_parse_hosts_argv(&argv_, pool);
    free(argv);
    return error;
}


static wchar_t* machinefile_invalid = (wchar_t*)(LONG_PTR)-1;
static const wchar_t* const CCP_NODES_OVERFLOW = L"Overflow";


static const wchar_t*
mp_parse_ccp_nodes_env(
    mp_host_pool_t* pool
    )
{
    //
    // Maximum size of an environment variable
    //
    wchar_t p[MAX_ENV_LENGTH];
    DWORD err = MPIU_Getenv( L"CCP_NODES", p, _countof(p) );
    if( err != NOERROR )
    {
        MPIU_Assert( err == ERROR_ENVVAR_NOT_FOUND );
        pool->nproc = 0;
        pool->hosts = NULL;
        return NULL;
    }

    if( CompareStringW( LOCALE_INVARIANT,
                        NORM_IGNORECASE,
                        p,
                        -1,
                        CCP_NODES_OVERFLOW,
                        -1 ) == CSTR_EQUAL )
    {
        return machinefile_invalid;
    }

    return mp_parse_hosts_string(p, pool);
}


_Success_( return != NULL )
smpd_env_node_t*
add_new_env_node(
    _Inout_ smpd_env_node_t** list,
    _In_ PCWSTR name,
    _In_ PCWSTR value
    )
{
    smpd_env_node_t* env_node = new smpd_env_node_t;
    if(env_node == NULL)
        return NULL;

    env_node->next = NULL;
    env_node->name = MPIU_Strdup( name );
    env_node->value = MPIU_Strdup( value );

    //
    // Add the node to the end of the list. This enables env var overriding in
    // "logical" order; last env rules.
    //
    while(*list != NULL)
    {
        list = &(*list)->next;
    }

    *list = env_node;
    return env_node;
}


_Success_( return == true )
static bool
smpd_parse_envlist(
    _Inout_ smpd_env_node_t** list,
    _Inout_ wchar_t* envlist
    )
{
    const smpd_env_node_t* env_node;

    const wchar_t* token;
    wchar_t* next_token = NULL;
    token = wcstok_s(envlist, L",", &next_token);
    while (token)
    {
        wchar_t value[SMPD_MAX_ENV_LENGTH];
        DWORD err = MPIU_Getenv( token,
                                 value,
                                 _countof(value) );
        if( err == NOERROR )
        {
            env_node = add_new_env_node(list, token, value);
            if(env_node == NULL)
                return false;

        }
        token = wcstok_s(NULL, L",", &next_token);
    }

    return true;
}


_Success_( return != NULL )
static mp_block_options_t*
add_new_block_options(
    _Inout_ mp_block_options_t** list
    )
{
    mp_block_options_t* bo = new mp_block_options_t;
    if(bo == NULL)
        return NULL;

    //
    // Add the node to the end of the list.
    //
    while(*list != NULL)
    {
        list = &(*list)->next;
    }

    *list = bo;
    return bo;
}


static void read_mpiexec_timeout()
{
    //
    // check to see if a timeout is specified by the environment variable only if
    // a timeout has not been specified on the command line
    //
    if(smpd_process.timeout > 0)
        return;

    wchar_t p[12];
    DWORD err = MPIU_Getenv( L"MPIEXEC_TIMEOUT",
                             p,
                             _countof(p) );
    if( err != NO_ERROR || !isposnumber(p) )
    {
        return;
    }

    smpd_process.timeout = _wtoi(p);
}


static bool read_job_context()
{
    //
    // If we are launched by something other than the HPC job scheduler they
    // may want to set a context string to identify their MPI job to the remote
    // node manager entity.  Read that here.
    //
    // If the CCP_TASKCONTEXT variable is set, this will be overwritten.
    // Similarly, if -job is specified on the command line, this will be overwritten.
    //
    wchar_t jobCtx[SMPD_MAX_NAME_LENGTH];
    jobCtx[0] = L'\0';
    GetEnvironmentVariableW( L"MSMPI_JOB_CONTEXT",
                             jobCtx,
                             _countof(jobCtx) );

    if( jobCtx[0] != L'\0' )
    {
        delete[] smpd_process.job_context;
        DWORD err =  MPIU_WideCharToMultiByte( jobCtx, &smpd_process.job_context );
        if( err != NOERROR )
        {
            wprintf( L"Failed to read MSMPI_JOB_CONTEXT environment variable erorr %u\n", err);
            return false;
        }
    }
    return true;
}


static bool read_ccp_taskcontext()
{
    //
    // If we're launched by the CCP job scheduler,
    // we will build the job_context string as follows
    // CCP_TASKCONTEXT.CCP_TASKID.CCP_TASKINSTANCEID
    //
    wchar_t env_str[SMPD_MAX_NAME_LENGTH];
    wchar_t jobCtx[SMPD_MAX_NAME_LENGTH];

    DWORD err = MPIU_Getenv( L"CCP_TASKCONTEXT",
                             env_str,
                             _countof(env_str) );
    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        return true;
    }
    else if( err == ERROR_INSUFFICIENT_BUFFER )
    {
        wprintf(L"Error: Unexpected value for CCP_TASKCONTEXT.\n");
        return false;
    }
    MPIU_Strcpy( jobCtx,
                 _countof(jobCtx),
                 env_str );

    env_str[0] = L'.';
    err = MPIU_Getenv( L"CCP_TASKID",
                       &env_str[1],
                       _countof(env_str) - 1 );
    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        //
        // If we arrive here, CCP_TASKCONTEXT is set, but CCP_TASKID is not.
        // This typically means the environment is broken because
        // in the cluster environment, these variables will be set.
        // In SDK mode, CCP_TASKCONTEXT should not be set to start with
        //
        wprintf(L"Error: CCP_TASKCONTEXT is set but CCP_TASKID is not set.\n");
        return false;
    }
    else if( err == ERROR_INSUFFICIENT_BUFFER )
    {
        wprintf(L"Error: Unexpected value for CCP_TASKID.\n");
        return false;
    }

    HRESULT hr = StringCchCatW( jobCtx,
                                _countof(jobCtx),
                                env_str );
    if( FAILED(hr) )
    {
        wprintf(L"Error: Failed to process CCP_TASKID error 0x%08x\n", hr);
        return false;
    }

    err = MPIU_Getenv( L"CCP_TASKINSTANCEID",
                       &env_str[1],
                       _countof(env_str) - 1 );

    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        //
        // If we arrive here, CCP_TASKCONTEXT is set, but CCP_TASKINSTANCED is not.
        // This typically means the environment is broken because
        // in the cluster environment, these variables will be set.
        // In SDK mode, CCP_TASKCONTEXT should not be set to start with
        //
        wprintf(L"Error: CCP_TASKCONTEXT is set but CCP_TASKINSTANCEID is not set.\n");
        return false;
    }
    else if( err == ERROR_INSUFFICIENT_BUFFER )
    {
        wprintf(L"Error: Unexpected value for CCP_TASKINSTANCEID.\n");
        return false;
    }

    hr = StringCchCatW( jobCtx,
                        _countof(jobCtx),
                        env_str );
    if( FAILED(hr) )
    {
        wprintf(L"Error: Failed to process CCP_TASKINSTANCEID error 0x%08x\n", hr);
        return false;
    }

    delete[] smpd_process.job_context;

    err = MPIU_WideCharToMultiByte( jobCtx, &smpd_process.job_context );
    if( err != NOERROR )
    {
        wprintf(L"Failed processing CCP_TASKCONTEXT error %u\n", err);
        return false;
    }

    return true;
}


_Success_( return == true )
static bool
read_ccp_envlist(
    _Inout_ mp_global_options_t* go
    )
{
    //
    // Propagate CCP environment variables to the spawned tree
    //
    wchar_t env_str[MAX_ENV_LENGTH];
    DWORD err = MPIU_Getenv( L"CCP_ENVLIST",
                             env_str,
                             _countof(env_str) );
    if( err == ERROR_ENVVAR_NOT_FOUND )
    {
        return true;
    }

    const smpd_env_node_t* env_node;
    env_node = add_new_env_node(&go->env, L"CCP_ENVLIST", env_str);
    if(env_node == NULL)
    {
        wprintf(L"Error: unable to allocate memory to hold the CCP_ENVLIST environment variable.\n");
        return false;
    }

    if(!smpd_parse_envlist(&go->env, env_str))
    {
        wprintf(L"Error: unable to allocate memory for the environment variables listed in in CCP_ENVLIST.\n");
        return false;
    }

    return true;
}


_Success_( return == true )
static bool read_ccp_mpi_netmask(
    _Inout_ mp_global_options_t* go
    )
{
    //
    // Ignore CCP_MPI_NETMASK if we're on azure
    //
    if( get_azure_node_logical_name( NULL, 0 ) == true )
    {
        return true;
    }

    //
    // Propagate the network mask from mpiexec environment to the spawned tree
    // Max length of IP/netmask plus null terminating char is 32
    //
    wchar_t env_str[32];
    DWORD err = MPIU_Getenv( L"CCP_MPI_NETMASK",
                             env_str,
                             _countof(env_str) );
    if( err != NOERROR )
    {
        return true;
    }

    const smpd_env_node_t* env_node;
    env_node = add_new_env_node(&go->env, L"MPICH_NETMASK", env_str);
    if(env_node == NULL)
    {
        wprintf(L"Error: unable to allocate memory to hold the MPICH_NETMASK environment variable.\n");
        return false;
    }

    return true;
}


static void read_job_name()
{
    //
    // If we are launched by something other than the HPC job scheduler they
    // may want to set a context string to identify their MPI job to the remote
    // node manager entity.  Read that here.
    //
    // If the CCP_TASKCONTEXT variable is set, this will be overwritten.
    // Similarly, if -job is specified on the command line, this will be overwritten.
    //
    GetEnvironmentVariableW( L"MPICH_WIN32_JOBOBJ",
                             smpd_process.jobObjName,
                             _countof(smpd_process.jobObjName));
}


_Success_( return == true )
static bool
copy_mp_hostname_to_env(
    _Inout_ mp_global_options_t* go
    )
{
    //
    // Send mpiexec's hostname to smpd
    //
    const smpd_env_node_t* env_node;
    env_node = add_new_env_node( &go->env, L"MPIEXEC_HOSTNAME", smpd_process.host );
    if( env_node == NULL )
    {
        wprintf(L"Error: unable to allocate memory to pass MPIEXEC_HOSTNAME environment variable to smpd.\n");
        return false;
    }
    return true;
}


_Success_( return == true )
static bool
ParseAffinityArgument(
    _In_z_ const wchar_t* arg,
    _Out_ HWNODE_TYPE *pNodeType
    )
{
    if ( arg[0] == L'\0' || arg[1] != L'\0' )
    {
        return false;
    }

    switch( arg[0] )
    {
    case L'L':
    case L'l':
        *pNodeType = HWNODE_TYPE_LCORE;
        break;
    case L'P':
    case L'p':
        *pNodeType = HWNODE_TYPE_PCORE;
        break;
    case L'N':
    case L'n':
        *pNodeType = HWNODE_TYPE_NUMA;
        break;
    default:
        return false;
    }
    return true;
}


_Success_( return == true )
static bool
parse_affinity_layout_value(
    _In_  PCWSTR input,
    _Out_ mp_global_options_t* go,
    _In_  PCWSTR srcName
    )
{
    HRESULT hr;
    wchar_t tmpBuff[128] = {0};
    wchar_t* tmp;
    const wchar_t* algo;
    const wchar_t* target;
    const wchar_t* stride;

    hr = StringCchCopyW(tmpBuff,
                        _countof(tmpBuff) - 1,
                        input);
    if ( FAILED(hr) ) {
        smpd_err_printf(L"Invalid value specified in affinity layout\n");
        return false;
    }

    tmp    = tmpBuff;
    algo   = tmpBuff;
    target = nullptr;
    stride = nullptr;

    //
    // Option format is algo[[:stride]:target]
    // e.g.: 2:P:L (stride by physical cores, target logical cores)
    //       seq:L (both stride and target are logical cores)
    //
    while ( tmp[0] != L'\0' )
    {
        if ( tmp[0] == L':' && tmp[1] != L'\0' )
        {
            tmp[0] = L'\0';

            if ( stride != nullptr )
            {
                target = ++tmp;
                break;
            }
            stride = ++tmp;
        }
        ++tmp;
    }

    wchar_t* disabledOptions[] = {L"0"};
    wchar_t* spreadOptions[] = {L"1", L"spr", L"spread"};
    wchar_t* sequentialOptions[] = {L"2", L"seq", L"sequential"};
    wchar_t* balancedOptions[] = {L"3", L"bal", L"balanced"};

    if ( CheckParameter( algo, disabledOptions, 1 ) )
    {
        go->affinityOptions.placement = SMPD_AFFINITY_DISABLED;
    }
    else if ( CheckParameter( algo, spreadOptions, 3 ) )
    {
        go->affinityOptions.placement = SMPD_AFFINITY_SPREAD;
    }
    else if ( CheckParameter(algo, sequentialOptions, 3) )
    {
        go->affinityOptions.placement = SMPD_AFFINITY_SEQUENTIAL;
    }
    else if ( CheckParameter(algo, balancedOptions, 3) )
    {
        go->affinityOptions.placement = SMPD_AFFINITY_BALANCED;
    }
    else
    {
        smpd_err_printf( L"Unable to parse algorithm specified in %s.\n", srcName );
        return false;
    }

    if ( stride == nullptr )
    {
        go->affinityOptions.stride = HWNODE_TYPE_LCORE;
        go->affinityOptions.target = HWNODE_TYPE_LCORE;
        return true;
    }

    if ( !ParseAffinityArgument( stride, &(go->affinityOptions.stride)) )
    {
        smpd_err_printf( L"Invalid affinity argument specified in %s.\n", srcName );
        return false;
    }

    if ( target == nullptr )
    {
        go->affinityOptions.target = go->affinityOptions.stride;
        return true;
    }

    if ( !ParseAffinityArgument( target, &(go->affinityOptions.target) ) )
    {
        smpd_err_printf( L"Invalid affinity argument specified in %s.\n", srcName );
        return false;
    }

    return true;
}


//
// Affinity is initialized to SMPD_AFFINITY_DISABLED
// by the global block option's constructor.
//
static void
read_mpiexec_affinity(
    _Pre_valid_ _Out_ mp_global_options_t* go
    )
{
    wchar_t p[128];
    DWORD err = MPIU_Getenv( L"MPIEXEC_AFFINITY",
                             p,
                             _countof(p) );
    if( err != NOERROR )
    {
        return;
    }
    parse_affinity_layout_value(p, go, L"MPIEXEC_AFFINITY environment variable");
}


static void
read_mpiexec_affinity_auto(
    _Pre_valid_ _Out_ mp_global_options_t* go
    )
{
    go->affinityOptions.isAuto = env_is_on(
        L"MPIEXEC_AFFINITY_AUTO",
        FALSE
        );
}


_Success_( return == true )
static bool
add_app_to_launch_list(
    _Inout_ mp_block_options_t*  bo,
    _In_    int                  rank,
    _Inout_ mp_host_pool_t*      iter,
    _In_    mp_host_t*           firstHost
    )
{
    static UINT16 blockCount = 0;

    int                  n              = iter->nproc;
    mp_host_t*           host           = iter->hosts;
    bool                 needHostLookup = true;
    smpd_host_t*         pHost          = nullptr;
    smpd_launch_block_t* pLaunchBlock   = nullptr;
    const wchar_t        errMsg[]       = L"unable to allocate launching block\n";
    for(int i = 0; i < bo->nproc; i++)
    {
        if(n == host->nproc)
        {
            //
            // Reaching max process per host as indicated by the host
            // list, moving on to the next host and reset the count.
            //
            host = static_cast<mp_host_t*>( host->Next );
            n = 0;
            needHostLookup = true;
        }

        if(host == NULL)
        {
            //
            // Run out of hosts in the block's pool, loop around.
            //
            host = firstHost;
            n = 0;
            needHostLookup = true;
        }

        n++;

        ASSERT(host != NULL);
        ASSERT(host->nproc > 0);

        //
        //
        // Upon the first invocation on a hostname, smpd_get_host_id
        // will create a new host structure for it and chain it
        // to the smpd_process.host_list
        //
        if( needHostLookup )
        {
            pHost = smpd_get_host_id(host->name);
            if( pHost == nullptr )
            {
                return false;
            }
            needHostLookup = false;

            if( pHost->pBlockOptions != bo )
            {
                //
                // Each block option needs a different launch block
                //
                pLaunchBlock = new smpd_launch_block_t;
                if( pLaunchBlock == nullptr )
                {
                    smpd_err_printf( errMsg );
                    return false;
                }

                pLaunchBlock->host_id = pHost->HostId;
                MPIU_Strcpy(
                    pLaunchBlock->hostname,
                    _countof(pLaunchBlock->hostname),
                    host->name );

                pLaunchBlock->appnum = bo->appnum;
                pLaunchBlock->priority_class = bo->priority;

                pLaunchBlock->envCount = bo->envCount;
                pLaunchBlock->ppEnv = bo->envBlock;

                //
                // wdir is never null because if user does
                // not specify wdir in the block option, we will
                // user global option's wdir. If gwdir is also not
                // specified, we use the current directory
                //
                ASSERT( bo->wdir != nullptr );
                pLaunchBlock->pDir = bo->wdir;

                if(bo->path != nullptr)
                {
                    pLaunchBlock->pPath = bo->path;
                }
                else
                {
                    pLaunchBlock->pPath = L"";
                }

                pLaunchBlock->pExe = bo->exe;
                pLaunchBlock->pArgs = bo->cmdline;

                //
                // Insert this launch block to the host's list of
                // launch blocks
                //
                pLaunchBlock->next = pHost->pLaunchBlockList;
                pHost->pLaunchBlockList = pLaunchBlock;

                //
                // Mark the current block options so we know when we loop
                // around
                //
                pHost->pBlockOptions = bo;
            }
            else
            {
                //
                // The case where we loop around
                //
                pLaunchBlock = pHost->pLaunchBlockList;
            }
        }

        ASSERT( pLaunchBlock != nullptr );

        smpd_rank_data_t* pRank = new smpd_rank_data_t;
        if( pRank == nullptr )
        {
            smpd_err_printf(L"unable to allocate a rank data structure.\n");
            return false;
        }

        pRank->parent = pLaunchBlock;
        pRank->rank = static_cast<UINT16>( rank++ );
        pRank->nodeProcOrder = pHost->nodeProcCount++ ;

        //
        // Create a launch node in the case the user requested that
        // mpiexec print the affinity table
        //
        smpd_launch_node_t *launch_node = nullptr;
        if( smpd_process.affinityOptions.isSet &&
            smpd_process.affinityOptions.affinityTableStyle != 0 )
        {
            launch_node = new smpd_launch_node_t;
            if(launch_node == NULL)
            {
                smpd_err_printf(L"unable to allocate a launch node structure.\n");
                delete pRank;
                return false;
            }

            MPIU_Strcpy(
                launch_node->hostname,
                _countof(launch_node->hostname),
                host->name );

            launch_node->iproc = pRank->rank;
            launch_node->host_id = pHost->HostId;
            launch_node->affinity = &pRank->affinity;

            launch_node->next = smpd_process.launch_list;
            if(smpd_process.launch_list)
            {
                smpd_process.launch_list->prev = launch_node;
            }
            smpd_process.launch_list = launch_node;
            launch_node->prev = nullptr;
        }


        //
        // If we are in explicit affinity mode, IE. someone gave mask
        //  values in -hosts or -machinefile we will grab the next
        //  available mask from the list on that host.
        //
        if( smpd_process.affinityOptions.isSet && smpd_process.affinityOptions.isExplicit)
        {
            ASSERT(host->explicit_count > 0);

            if (0 == host->explicit_index)
            {
                host->explicit_index = host->explicit_count;
            }
            pRank->affinity = host->explicit_affinity[ host->explicit_index - 1 ];

            host->explicit_index--;
        }

        //
        // Insert this rank's data to the launch's block rank data
        // list
        //
        pRank->next = pLaunchBlock->pRankData;
        pLaunchBlock->pRankData = pRank;
    }

    iter->nproc = n;
    iter->hosts = host;
    blockCount++;

    return true;
}


_Success_( return == true )
static bool
create_launch_list(
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t*  bo_list
    )
{
    mp_host_pool_t giter = go->pool;
    giter.nproc = 0;

    UINT16 cur_rank = 0;
    bool fResult = true;

    for(mp_block_options_t* bo = bo_list; bo != NULL; bo = bo->next)
    {
        mp_host_pool_t liter = bo->pool;
        liter.nproc = 0;

        //
        // Use the local pool as hosts iterator
        //
        mp_host_pool_t* iter = &liter;
        mp_host_t* host = bo->pool.hosts;
        if( host == NULL )
        {
            //
            // No local pool, use the global pool as hosts iterator.
            //
            iter = &giter;
            host = go->pool.hosts;
        }

        if( !add_app_to_launch_list(bo, cur_rank, iter, host) )
        {
            fResult = false;
            goto fn_exit;
        }

        if( static_cast<UINT32>(cur_rank) + bo->nproc > MSMPI_MAX_RANKS )
        {
            wprintf(L"Error: exceeding the maximum supported ranks.\n");
            fResult = false;
            goto fn_exit;
        }

        cur_rank += static_cast<UINT16>(bo->nproc);
    }

    smpd_process.nproc = cur_rank;

fn_exit:
    return fResult;
}


//----------------------------------------------------------------------------
//
// Option parsing functions (begin)
//
//----------------------------------------------------------------------------
_Success_( return == true )
static bool
parse_np(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Inout_ mp_block_options_t* bo
    )
{
    const wchar_t* opt = (*argvp)[0];
    if(bo->nproc != 0)
    {
        wprintf(L"Error: unexpected %s option; only one of -n,"
                L" -np or -hosts can specify the number of processes.\n", opt);
        return false;
    }

    const wchar_t* s = (*argvp)[1];
    if( s == nullptr ||
        !( isposnumber(s) ||
           (s[0] == L'*' && s[1] == L'\0') )
        )
    {
        wprintf(L"Error: expecting a positive number of processes"
                L" following the %s option.\n", opt);
        return false;
    }

    bo->nproc = _wtoi(s);
    if( bo->nproc > MSMPI_MAX_RANKS )
    {
        wprintf(L"Error: exceeding the maximum supported ranks.\n");
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_cores(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    if(go->cores_per_host != 0)
    {
        wprintf(L"Error: unexpected %s option; "
                L"this option can be specifed only once.\n", opt);
        return false;
    }

    const wchar_t* s = (*argvp)[1];
    if( s == nullptr || !(isposnumber(s)) )
    {
        wprintf(L"Error: expecting a positive number of cores "
                L"following the %s option.\n", opt);
        return false;
    }

    go->cores_per_host = _wtoi(s);
    if( go->cores_per_host > MSMPI_MAX_RANKS )
    {
        wprintf(L"Error: number of cores per node exceeding the maximum supported ranks.\n");
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_machinefile_opt(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_host_pool_t* pool)
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == nullptr)
    {
        wprintf(L"Error: expecting a filename following the %s option.\n", opt);
        return false;
    }

    smpd_clear_local_root();

    wchar_t* hosts;
    const wchar_t* error = smpd_get_hosts_from_file(s, &hosts);
    if(error != NULL)
    {
        wprintf(L"Error: while reading machinefile '%s': %s.\n", s, error);
        return false;
    }

    error = mp_parse_hosts_string(hosts, pool);
    if(error != NULL)
    {
        wprintf(L"Error: while parsing machinefile '%s': %s\n", s, error);
        return false;
    }

    machinefile_invalid = NULL;
    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_machinefile(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Inout_ mp_block_options_t* bo
    )
{
    const wchar_t* opt = (*argvp)[0];
    if(bo->pool.hosts != NULL)
    {
        wprintf(L"Error: unexpected %s option; only one of -host, "
                L"-hosts or -machinefile can be specified.\n", opt);
        return false;
    }

    return parse_machinefile_opt(argvp, &bo->pool);
}


_Success_( return == true )
static bool
parse_gmachinefile(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    if(go->pool.hosts != NULL)
    {
        wprintf(L"Error: unexpected %s option; only one of"
                L" -ghost or -gmachinefile can be specified.\n", opt);
        return false;
    }

    return parse_machinefile_opt(argvp, &go->pool);
}


_Success_( return == true )
static bool parse_wdir_opt(
    _Inout_ wchar_t* *argvp[],
    _Outptr_result_z_ PCWSTR* pwdir
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL)
    {
        wprintf(L"Error: expecting a directory name following the %s option.\n", opt);
        return false;
    }

    *pwdir = s;

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_wdir(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Inout_ mp_block_options_t* bo
    )
{
    return parse_wdir_opt(argvp, &bo->wdir);
}


_Success_( return == true )
static bool
parse_gwdir(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* s;
    bool ret = parse_wdir_opt(argvp, &s);
    if( ret == true )
    {
        //
        // The DTOR of mp_global_options_t will free go->wdir
        //
        go->wdir = MPIU_Strdup( s );
    }
    return ret;
}


_Success_( return == true )
static bool
parse_job(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL)
    {
        wprintf(L"Error: expecting job context string following the %s option.\n", opt);
        return false;
    }

    delete[] smpd_process.job_context;

    DWORD err = MPIU_WideCharToMultiByte( s, &smpd_process.job_context );
    if( err != NOERROR )
    {
        wprintf(L"Error: cannot convert job context %s to multibyte, error %u\n", s, err);
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_env_opt(
    _Inout_ wchar_t* *argvp[],
    _Inout_ smpd_env_node_t** envlist
    )
{
    const wchar_t* opt = (*argvp)[0];
    if((*argvp)[1] == NULL || (*argvp)[2] == NULL)
    {
        wprintf(L"Error: expecting environment variable name "
                L"and value following the %s option.\n", opt);
        return false;
    }

    const wchar_t* name = (*argvp)[1];
    const wchar_t* value = (*argvp)[2];
    const smpd_env_node_t* env_node;
    env_node = add_new_env_node(envlist, name, value);
    if(env_node == NULL)
    {
        wprintf(L"Error: not enough memory to allocate "
                L"the environment variable '%s'.\n", name);
        return false;
    }

    (*argvp) += 3;
    return true;
}


_Success_( return == true )
static bool
parse_env(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Inout_ mp_block_options_t* bo
    )
{
    return parse_env_opt(argvp, &bo->env);
}


_Success_( return == true )
static bool
parse_genv(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    return parse_env_opt(argvp, &go->env);
}


_Success_( return == true )
static bool
parse_genvlist(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    wchar_t* s = (*argvp)[1];
    if(s == NULL)
    {
        wprintf(L"Error: expecting environment variables "
                L"list following the %s option.\n", opt);
        return false;
    }

    if(!smpd_parse_envlist(&go->env, s))
    {
        wprintf(L"Error: unable to allocate memory to hold environment list.\n");
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool parse_configfile_error(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    wprintf(L"Error: the %s option must be the first and only option specified.\n", opt);
    return false;
}


_Success_( return == true )
static bool
parse_host_opt(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_host_pool_t* pool
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL)
    {
        wprintf(L"Error: expecting host name following the %s option.\n", opt);
        return false;
    }

    smpd_clear_local_root();

    pool->nproc = 1;
    pool->hosts = new_host_node(s, 1);
    if(pool->hosts == NULL)
    {
        wprintf(L"Error: not enought memory to allocate the host node.\n");
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_host(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Inout_ mp_block_options_t* bo
    )
{
    const wchar_t* opt = (*argvp)[0];
    if(bo->pool.hosts != NULL)
    {
        wprintf(L"Error: unexpected %s option; only one of"
                L" -host, -hosts or -machinefile can be specified.\n", opt);
        return false;
    }

    return parse_host_opt(argvp, &bo->pool);
}


_Success_( return == true )
static bool
parse_ghost(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    if(go->pool.hosts != NULL)
    {
        wprintf(L"Error: unexpected %s option; only one of "
                L"-ghost or -gmachinefile can be specified.\n", opt);
        return false;
    }

    return parse_host_opt(argvp, &go->pool);
}


_Success_( return == true )
static bool
parse_hosts(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Inout_ mp_block_options_t* bo
    )
{
    const wchar_t* opt = (*argvp)[0];
    if(bo->nproc != 0)
    {
        wprintf(L"Error: unexpected %s option; only one of -n, "
                L"-np or -hosts can specify the number of processes.\n", opt);
        return false;
    }

    if(bo->pool.hosts != NULL)
    {
        wprintf(L"Error: unexpected %s option; only one of -host,"
                L" -hosts or -machinefile can be specified.\n", opt);
        return false;
    }

    smpd_clear_local_root();

    (*argvp) += 1;
    const wchar_t* error = mp_parse_hosts_argv(argvp, &bo->pool);
    if(error != NULL)
    {
        wprintf(L"%s\n", error);
        return false;
    }

    //
    // Set nproc to -1 to indicate that the number of processes was specified.
    // The true number is set in the cmdline post process function.
    //
    bo->nproc = -1;
    return true;
}


//
// Suppressing OACR 28718 Unannotated buffer
//
#ifdef _PREFAST_
#pragma warning(push)
#pragma warning(disable:28718)
#endif

static bool
parse_help(
    wchar_t**[] /*argvp*/,
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    mp_print_options();
    exit(0);
}


static bool
parse_help2(
    wchar_t**[] /*argvp*/,
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    mp_print_extra_options();
    exit(0);
}


static bool
parse_help3(
    wchar_t**[] /*argvp*/,
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/)
{
    mp_print_environment_variables();
    exit(0);
}

#ifdef _PREFAST_
#pragma warning(pop)
#endif


_Success_( return == true )
static bool
parse_exitcodes(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    smpd_process.output_exit_codes = TRUE;

    (*argvp) += 1;
    return true;
}


_Success_( return == true )
static bool
parse_priority(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Inout_ mp_block_options_t* bo
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL || !isdigits(s))
    {
        wprintf(L"Error: expecting a priority value in the range"
                L" [" _S(SMPD_PRIORITY_CLASS_MIN) L":" _S(SMPD_PRIORITY_CLASS_MAX) L"] "
                L"following the %s option.", opt);
        return false;
    }

    bo->priority = _wtoi(s);

    if(bo->priority < SMPD_PRIORITY_CLASS_MIN || bo->priority > SMPD_PRIORITY_CLASS_MAX)
    {
        wprintf(L"Error: expecting a priority value in the range"
                L" [" _S(SMPD_PRIORITY_CLASS_MIN) L":" _S(SMPD_PRIORITY_CLASS_MAX) L"] "
                L"following the %s option.", opt);
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_debug(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    smpd_process.verbose = TRUE;
    smpd_process.dbg_state = SMPD_DBG_STATE_ERROUT | SMPD_DBG_STATE_STDOUT;

    const wchar_t* s = (*argvp)[1];
    if(s != NULL && isdigits(s))
    {
        smpd_process.dbg_state = _wtoi(s) & SMPD_DBG_STATE_ALL;
        (*argvp) += 1;
    }

    smpd_process.dbg_state |= SMPD_DBG_STATE_PREPEND_RANK;

    (*argvp) += 1;
    return true;
}


_Success_(return == true)
static bool
parse_logFile(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* option = (*argvp)[0];
    const wchar_t* logFile = (*argvp)[1];
    if (logFile == NULL)
    {
        wprintf(L"Error: expecting a fileName following the %s option.\n", option);
        return false;
    }

    //
    // Make sure logFile folder does exist
    //
    wchar_t logFileFolder[MAX_PATH] = { 0 };
    wcscpy_s(logFileFolder, MAX_PATH, logFile);
    if (RemoveFileSpec(logFileFolder))
    {
        if (!DirectoryExists(logFileFolder))
        {
            wprintf(L"Error: folder %s does not exist.\n", logFileFolder);
            return false;
        }
    }

    if (_wfreopen(logFile, L"w", stdout) == NULL ||
        _wfreopen(logFile, L"a+", stderr) == NULL)
    {
        wprintf(L"Error: Failed to redirect logs to %s.\n", logFile);
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_port(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL || !isposnumber(s))
    {
        wprintf(L"Error: expecting a positive port number following the %s option.\n", opt);
        return false;
    }

    smpd_process.rootServerPort = static_cast<UINT16>(_wtoi(s));
    if( smpd_process.rootServerPort == 0 )
    {
        wprintf(L"Error: expecting a valid TCP port number following the %s option.\n", opt);
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_path_opt(
    _Inout_ wchar_t* *argvp[],
    _Outptr_result_z_ PCWSTR* ppath
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL)
    {
        wprintf(L"Error: expecting a path string following the %s option.\n", opt);
        return false;
    }

    *ppath = s;

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_path(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    _Out_ mp_block_options_t* bo
    )
{
    return parse_path_opt(argvp, &bo->path);
}


_Success_( return == true )
static bool
parse_gpath(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    return parse_path_opt(argvp, &go->path);
}


_Success_( return == true )
static bool parse_timeout(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL || !isposnumber(s))
    {
        wprintf(L"Error: expecting a positive timeout value following the %s option.\n", opt);
        return false;
    }

    smpd_process.timeout = _wtoi(s);

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool parse_lines(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
    )
{
    smpd_process.prefix_output = TRUE;

    (*argvp) += 1;
    return true;
}


static void parse_affinity_styles()
{
    if( env_is_on( ENV_MPIEXEC_AFFINITY_TABLE, FALSE ) )
    {
        smpd_process.affinityOptions.affinityTableStyle = 1;
    }

    wchar_t pVal[_countof(L"yes")];
    DWORD err = MPIU_Getenv( ENV_MPIEXEC_AFFINITY_TABLE,
                             pVal,
                             _countof(pVal) );
    if( err == NOERROR )
    {
        smpd_process.affinityOptions.affinityTableStyle = _wtoi(pVal);
    }

    if( env_is_on( ENV_MPIEXEC_HWTREE_TABLE, FALSE ) )
    {
        smpd_process.affinityOptions.hwTableStyle = 1;
    }

    err = MPIU_Getenv( L"MPIEXEC_HWTREE_TABLE",
                       pVal,
                       _countof(pVal) );
    if( err == NOERROR )
    {
        smpd_process.affinityOptions.hwTableStyle = _wtoi(pVal);
    }
}


_Success_( return == true )
static bool parse_affinity_layout(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL)
    {
        wprintf(L"Error: expecting a layout format string for the %s option.\n", opt);
        return false;
    }

    if( FALSE != smpd_process.affinityOptions.isSet  &&
        FALSE != smpd_process.affinityOptions.isExplicit )
    {
        wprintf(L"Error: Explicit affinity options can not be"
                L" combined with other affinity options.");
        return false;
    }

    if (!parse_affinity_layout_value(s, go, L"-affinity_layout argument")) {
        return false;
    }

    (*argvp) += 2;
    return true;
}


_Success_( return == true )
static bool
parse_affinity(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    if( FALSE != smpd_process.affinityOptions.isSet
        && FALSE != smpd_process.affinityOptions.isExplicit )
    {
        wprintf(L"Error: Explicit affinity options cannot "
                L"be combined with other affinity options.");
        return false;
    }

    //
    // enable affinity if not already enabled by affinity_layout
    //

    if (SMPD_AFFINITY_DISABLED == go->affinityOptions.placement) {
        go->affinityOptions.placement = SMPD_AFFINITY_DEFAULT;
        go->affinityOptions.target = HWNODE_TYPE_LCORE;
        go->affinityOptions.stride = HWNODE_TYPE_LCORE;
    }

    (*argvp) += 1;
    return true;
}


_Success_( return == true )
static bool parse_affinity_auto(
    _Inout_ wchar_t* *argvp[],
    _Out_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
    )
{
    go->affinityOptions.isAuto = true;

    (*argvp) += 1;
    return true;
}


_Success_( return == true )
static bool parse_pwd(
    _Inout_ wchar_t* *argvp[],
    _Out_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
)
{
    const wchar_t* opt = (*argvp)[0];
    wchar_t* s = (*argvp)[1];
    if (s == nullptr)
    {
        wprintf(L"Error: expecting a password string for the %s option.\n", opt);
        return false;
    }

    go->pwd = s;

    (*argvp) += 2;
    return true;
}


_Success_(return == true)
static bool parse_saveCreds(
    _Inout_ wchar_t* *argvp[],
    _Out_ mp_global_options_t* go,
    mp_block_options_t* /*bo*/
)
{
    go->saveCreds = TRUE;

    (*argvp) += 1;
    return true;
}


_Success_(return == true)
static bool parse_unicode(
    _Inout_ wchar_t* *argvp[],
    mp_global_options_t* /*go*/,
    mp_block_options_t* /*bo*/
)
{
    if (!EnableUnicodeOutput())
    {
        return false;
    }

    (*argvp) += 1;
    return true;
}


_Success_( return == true )
static bool parse_executable(
    _In_ wchar_t* argv[],
    _Inout_ mp_block_options_t* bo
    )
{
    const wchar_t* s = argv[0];
    if(s == NULL)
    {
        wprintf(L"Error: no executable specified.\n");
        return false;
    }

    bo->exe = s;

    //
    // Applications expect argv[0] to be the module name
    //
    const wchar_t* end;
    end = smpd_pack_cmdline(argv, bo->cmdline, _countof(bo->cmdline));

    //
    // If the command line is too long, smpd_pack_cmdline() returns a pointer to the last
    // character of the buffer.
    //
    if(end < &bo->cmdline[_countof(bo->cmdline)-1])
    {
        return true;
    }

    wprintf(L"Error: executable command line exceeds the %Iu "
            L"characters limit.\n", _countof(bo->cmdline)-1);
    return false;
}


_Success_( return == true )
static bool parse_configfile(
    _Inout_ wchar_t** argvp[]
    )
{
    const wchar_t* opt = (*argvp)[0];
    const wchar_t* s = (*argvp)[1];
    if(s == NULL)
    {
        wprintf(L"Error: expecting filename following the %s option.\n", opt);
        return false;
    }

    if((*argvp)[2] != NULL)
    {
        wprintf(L"Error: too many arguments following the %s option;"
                L" only filename is expected.\n", opt);
        return false;
    }

    const wchar_t* error = smpd_get_argv_from_file(s, argvp);
    if(error != NULL)
    {
        wprintf(L"Error: while parsing configfile '%s': %s\n", s, error);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
//
// Options parser
//
//----------------------------------------------------------------------------

#if DBG

static void Assert_Options_table_sorted(const mp_option_handler_t* p, size_t n)
{
    if(n <= 1)
        return;

    for(size_t i = 1; i < n; i++)
    {
        if( CompareStringW( LOCALE_INVARIANT,
                            0,
                            p[i].option,
                            -1,
                            p[i-1].option,
                            -1 ) != CSTR_GREATER_THAN )
        {
            ASSERT(("command handlers table not sorted", 0));
        }
    }
}

#else

#define Assert_Options_table_sorted(p, n) ((void)0)


#endif


static int __cdecl mp_compare_option(const void* pv1, const void* pv2)
{
    const wchar_t* option = static_cast<const wchar_t*>(pv1);
    const mp_option_handler_t* entry = static_cast<const mp_option_handler_t*>(pv2);

    //
    // Subtracting 2 from the return value to maintain backcompat to
    // CRT comparison behavior.
    //
    return CompareStringW( LOCALE_INVARIANT,
                           NORM_IGNORECASE,
                           option,
                           -1,
                           entry->option,
                           -1 ) - 2;
}


_Success_( return == true )
static
bool
mp_parse_options(
    _Inout_ wchar_t* *argvp[],
    _In_    const mp_option_handler_t* oh,
    _In_    size_t ohsize,
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t* bo
    )
{
    Assert_Options_table_sorted(oh, ohsize);

    while(is_flag_char(**argvp))
    {
        const void* p;
        p = bsearch(
                &(**argvp)[1],
                oh,
                ohsize,
                sizeof(oh[0]),
                mp_compare_option
                );

        //
        // Option flag was not found
        // Return true to enable further parsing.
        //
        if(p == NULL)
            return true;

        //
        // Call the option handler
        //
        bool fSucc = static_cast<const mp_option_handler_t*>(p)->on_option(argvp, go, bo);
        if(!fSucc)
            return false;
    }

    return true;
}


_Success_( return == true )
static bool
parse_optionfile(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t* bo
    );


//----------------------------------------------------------------------------
//
// Option handlers table (must be sorted)
//
//----------------------------------------------------------------------------

static const mp_option_handler_t g_mpiexec_option_handlers[] =
{
    { L"?",                  parse_help },
    { L"??",                 parse_help2 },
    { L"???",                parse_help3 },
    { L"a",                  parse_affinity },
    { L"aa",                 parse_affinity_auto },
    { L"affinity",           parse_affinity },
    { L"affinity_auto",      parse_affinity_auto},
    { L"affinity_layout",    parse_affinity_layout },
    { L"al",                 parse_affinity_layout },
    { L"c",                  parse_cores },
    { L"configfile",         parse_configfile_error },
    { L"cores",              parse_cores },
    { L"d",                  parse_debug },
    { L"debug",              parse_debug },
    { L"dir",                parse_wdir },
    { L"env",                parse_env },
    { L"exitcodes",          parse_exitcodes },
    { L"gdir",               parse_gwdir },
    { L"genv",               parse_genv },
    { L"genvlist",           parse_genvlist },
    { L"ghost",              parse_ghost },
    { L"gmachinefile",       parse_gmachinefile },
    { L"gpath",              parse_gpath },
    { L"gwdir",              parse_gwdir },
    { L"help",               parse_help },
    { L"help2",              parse_help2 },
    { L"help3",              parse_help3 },
    { L"host",               parse_host },
    { L"hosts",              parse_hosts },
    { L"job",                parse_job },
    { L"l",                  parse_lines },
    { L"lines",              parse_lines },
    { L"logFile",            parse_logFile },
    { L"machinefile",        parse_machinefile },
    { L"n",                  parse_np },
    { L"np",                 parse_np },
    { L"optionfile",         parse_optionfile },
    { L"p",                  parse_port },
    { L"path",               parse_path },
    { L"port",               parse_port },
    { L"priority",           parse_priority },
    { L"pwd",                parse_pwd},
    { L"saveCreds",          parse_saveCreds},
    { L"timeout",            parse_timeout },
    { L"unicode",            parse_unicode },
    { L"wdir",               parse_wdir },
};


_Success_( return == true )
static bool
parse_optionfile(
    _Inout_ wchar_t* *argvp[],
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t* bo
    )
{
    if((*argvp)[1] == NULL)
    {
        wprintf(L"Error: expecting filename following the %s option.\n", (*argvp)[0]);
        return false;
    }

    const wchar_t* pFilename = (*argvp)[1];

    if(MPIU_Strlen( pFilename ) > SMPD_MAX_FILENAME)
    {
        wprintf(L"Error: expecting filename to be "
                L"less than %d characters.\n", SMPD_MAX_FILENAME);
        return false;
    }

    wchar_t** argv;

    const wchar_t* err = smpd_get_argv_from_file(pFilename, &argv);
    if(err != NULL)
    {
        wprintf(L"%s", err);
        return false;
    }

    (*argvp) += 2;

    bool fSucc = mp_parse_options(
        &argv,
        g_mpiexec_option_handlers,
        _countof(g_mpiexec_option_handlers),
        go,
        bo
        );

    if(fSucc == false)
    {
        return false;
    }

    //
    // Check after the return from mp_parse_options because if an unknown
    // option is found, argv will point to the unknown option.
    //
    if(is_flag_char(argv[0]))
    {
        wprintf(L"Unknown option: %s\n", argv[0]);
        return false;
    }

    return true;
}


_Success_( return == true )
static bool
parse_cmdline(
    _In_ wchar_t* argv[],
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t** bo_list
    )
{
    int appnum = 0;
    wchar_t** next_argv = argv;
    while (*next_argv != NULL)
    {
        /* set argv and find the next block */
        argv = next_argv;
        next_argv = find_cmdline_block_seperator(next_argv);
        if(*next_argv != NULL)
        {
            *next_argv = NULL;
            next_argv++;
        }

        mp_block_options_t* bo = add_new_block_options(bo_list);
        if(bo == NULL)
        {
            wprintf(L"Error: not enough memory to allocate allocate block options.\n");
            return false;
        }

        //
        // Set the appnum for each cmdline block.
        //
        bo->appnum = appnum++;

        /* parse the current block */
        bool fSucc = mp_parse_options(
                            &argv,
                            g_mpiexec_option_handlers,
                            _countof(g_mpiexec_option_handlers),
                            go,
                            bo
                            );

        if(!fSucc)
            return false;


        if(is_flag_char(argv[0]))
        {
            wprintf(L"Unknown option: %s\n", argv[0]);
            return false;
        }

        if(!parse_executable(argv, bo))
            return false;
    }

    parse_affinity_styles();
    return true;
}


_Success_( return == true )
static bool set_global_cwd(
    _Inout_ mp_global_options_t* go
    )
{
    if(go->wdir != NULL)
        return true;

    //
    // Both the ANSI and Unicode versions of SetCurrentDirectory and
    // GetCurrentDirectory limits the buffer to MAX_PATH
    //
    wchar_t env_str[MAX_PATH];
    DWORD err = MPIU_Getenv( L"CCP_MPI_WORKDIR",
                             env_str,
                             _countof(env_str) );
    if( err == NOERROR )
    {
        go->wdir = MPIU_Strdup( env_str );
        return true;
    }

    //
    // CCP_MPI_WORKDIR is not set or invalid. We use the current working directory
    //
    DWORD cchRet = GetCurrentDirectoryW( _countof(env_str),
                                         env_str );
    if( cchRet == 0 || cchRet >= _countof(env_str) )
    {
        return false;
    }

    go->wdir = MPIU_Strdup( env_str );
    return true;
}


static DWORD get_processors_count()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}


mp_host_t g_mp_host(smpd_process.host, 1);


_Success_( return == true )
static bool
set_global_host(
    _Inout_ mp_global_options_t* go
    )
{
    if(go->pool.hosts != NULL)
        return true;

    const wchar_t* error = mp_parse_ccp_nodes_env(&go->pool);
    if(error != NULL)
    {
        if( machinefile_invalid != NULL )
        {
            wprintf(
                L"Error: The HPC Server Scheduler could not fit all the node names for\n"
                L"this job in the CCP_NODES environment variable, preventing this job from\n"
                L"running on all assigned nodes.  Use mpiexec's \"-machinefile\" option to\n"
                L"specify all the nodes to run this job.\n"
                );
        }
        else
        {
            wprintf(L"Error: while parsing CCP_NODES environment variable. %s\n", error);
        }
        return false;
    }

    if(go->pool.hosts != NULL)
    {
        smpd_clear_local_root();
        return true;
    }

    go->pool.hosts = &g_mp_host;
    go->pool.nproc = get_processors_count();
    go->pool.hosts->nproc = go->pool.nproc;

    return true;
}


//
// Get the total number of procs.
//
static int
get_total_proc_used(
    _In_ const mp_block_options_t* bo
    )
{
    int nproc = 0;
    for( ; bo != NULL; bo = bo->next)
    {
        nproc += bo->nproc;
    }

    return nproc;
}


//
// Get the number of procs to be used out of the global pool.
//
static int
get_global_proc_used(
    _In_ const mp_block_options_t* bo
    )
{
    int nproc = 0;
    for( ; bo != NULL; bo = bo->next)
    {
        if(bo->pool.hosts != NULL)
            continue;

        nproc += bo->nproc;
    }

    return nproc;
}


static void
set_cores_per_host(
    _In_range_(>=, 0) int n,
    _Inout_ mp_host_pool_t* p
    )
{
    if(n == 0)
        return;

    for(mp_host_t* h = p->hosts; h != NULL; h = static_cast<mp_host_t*>( h->Next ) )
    {
        p->nproc -= h->nproc - n;
        h->nproc = n;
    }
}


static bool
ProcessBlockEnv(
    _In_ mp_global_options_t* go,
    _Inout_ mp_block_options_t* bo
    )
{
    //
    // Find out how many environment variables we need to pass
    //
    UINT16 envCount = 0;
    smpd_env_node_t* pEnvNode;
    for( pEnvNode = go->env; pEnvNode != nullptr; pEnvNode = pEnvNode->next )
    {
        envCount++;
    }
    for( pEnvNode = bo->env; pEnvNode != nullptr; pEnvNode = pEnvNode->next )
    {
        envCount++;
    }

    wchar_t** envBlock = nullptr;
    if( envCount != 0 )
    {
        //
        // Need two strings per environment variable
        // One for the name of the env var, one for the value
        //
        envBlock = new wchar_t*[envCount * 2];
        if( envBlock == nullptr )
        {
            smpd_err_printf(L"insufficient memory for environment variables\n");
            return false;
        }
    }

    size_t envIndex = 0;
    bool global = true;
    pEnvNode = go->env;
    if( pEnvNode == nullptr )
    {
        global = false;
        pEnvNode = bo->env;
    }

    while( pEnvNode != nullptr )
    {
        envBlock[envIndex++] = pEnvNode->name;
        envBlock[envIndex++] = pEnvNode->value;
        pEnvNode = pEnvNode->next;

        if( global && pEnvNode == nullptr )
        {
            //
            // After we finish the global env var, move on to the block env var
            //
            pEnvNode = bo->env;
            global = false;
        }
    }
    ASSERT( envIndex == static_cast<size_t>(envCount * 2) );

    bo->envBlock = envBlock;
    bo->envCount = static_cast<UINT16>(envIndex);
    return true;
}


_Success_( return == true )
static bool post_process_cmdline(
    _Inout_ mp_global_options_t* go,
    _Inout_ mp_block_options_t* bo_list
    )
{
    //
    // If we still do not have a job_context, use the default job_context
    //
    if( smpd_process.job_context == nullptr )
    {
        MPIU_WideCharToMultiByte( JOB_CONTEXT_UNASSIGNED_STR, &smpd_process.job_context );
    }

    if(!set_global_cwd(go))
        return false;

    if(!set_global_host(go))
        return false;

    set_cores_per_host(go->cores_per_host, &go->pool);

    mp_block_options_t* zbo = NULL;
    for(mp_block_options_t* bo = bo_list; bo != NULL; bo = bo->next)
    {
        if( !ProcessBlockEnv(go,bo) )
        {
            return false;
        }

        set_cores_per_host(go->cores_per_host, &bo->pool);

        if(bo->wdir == NULL)
        {
            bo->wdir = go->wdir;
        }

        if(bo->path == NULL)
        {
            bo->path = go->path;
        }

        //
        // If -hosts was specified, take the number of processes from the pool
        // after the pool was adjusted (posibly for -core option)
        //
        if(bo->nproc == -1)
        {
            bo->nproc = 0;
        }

        //
        // Okay if -n -np was specified.
        //
        if(bo->nproc != 0)
            continue;

        //
        // if -hosts or -machinefile specified (and no -n) use the entire pool.
        //
        if(bo->pool.nproc != 0)
        {
            bo->nproc = bo->pool.nproc;
            continue;
        }

        //
        // No procs specified, use what's left out of the global hosts pool.
        //
        if(zbo == NULL)
        {
            zbo = bo;
            continue;
        }

        //
        // Error no procs specified in more than one block.
        //
        wprintf(L"Error: expecting one of -n, -np, -hosts or -machinefile in section %d.\n", bo->appnum + 1);
        return false;
    }

    int tnproc = get_total_proc_used(bo_list);
    if( tnproc > MSMPI_MAX_RANKS )
    {
        wprintf(L"Error: exceeding the maximum supported ranks.\n");
        return false;
    }

    int gnproc = get_global_proc_used(bo_list);

    if(zbo != NULL)
    {
        if(gnproc >= go->pool.nproc)
        {
            wprintf(
                L"Error: not enough cores left for '%s' in section %d."
                L" the command line already subscribes %d processes on %d cores.\n",
                zbo->exe,
                zbo->appnum + 1,
                tnproc,
                go->pool.nproc + tnproc - gnproc
                );

            return false;
        }

        zbo->nproc = go->pool.nproc - gnproc;
    }

    if(gnproc > go->pool.nproc)
    {
        smpd_dbg_printf(L"Warning: some cores are over-subscribed, launching %d processes on %d cores.\n", tnproc, go->pool.nproc + tnproc - gnproc);
    }

    //
    // if the -affinity or MPIEXEC_AFFINITY flag is set, and not in explicit affinity mode
    //
    if( go->affinityOptions.placement != SMPD_AFFINITY_DISABLED
        && FALSE == smpd_process.affinityOptions.isExplicit )
    {
        smpd_process.affinityOptions.isSet = TRUE;
        smpd_process.affinityOptions.placement = go->affinityOptions.placement;
        smpd_process.affinityOptions.target = go->affinityOptions.target;
        smpd_process.affinityOptions.stride = go->affinityOptions.stride;
        smpd_process.affinityOptions.isAuto = go->affinityOptions.isAuto;
    }

    if (go->pwd != nullptr)
    {
        size_t length = MPIU_Strlen(go->pwd) + 1;
        smpd_process.pwd = new wchar_t[length];
        if (smpd_process.pwd == nullptr)
        {
            wprintf(L"Insufficient memory for processing command line.\n");
            return false;
        }
        MPIU_Strcpy( smpd_process.pwd, length, go->pwd );

        smpd_process.saveCreds = go->saveCreds;
    }

    return true;
}


_Success_( return == true )
bool mp_parse_command_args(
    _In_ wchar_t* argv[]
    )
{
    mp_global_options_t* go = new mp_global_options_t;

    read_mpiexec_affinity(go);
    read_mpiexec_affinity_auto(go);
    if( !read_job_context() )
    {
        return false;
    }

    read_job_name();

    if(!read_ccp_taskcontext())
    {
        return false;
    }

    if(!read_ccp_mpi_netmask(go))
    {
        return false;
    }

    if(!read_ccp_envlist(go))
    {
        return false;
    }

    if(!copy_mp_hostname_to_env(go))
    {
        return false;
    }

    //
    // Skip argv[0], the executable name.
    //
    argv++;

    //
    // Check for the -configfile option.  It must be the first and only option
    //
    if(is_flag_char(argv[0]) &&
       CompareStringW( LOCALE_INVARIANT,
                       0,
                       &argv[0][1],
                       -1,
                       L"configfile",
                       -1 ) == CSTR_EQUAL )
    {
        if(!parse_configfile(&argv))
        {
            return false;
        }
    }

    //
    // parse all blocks on the command line
    //
    mp_block_options_t* bo_list = NULL;

    if( !parse_cmdline(argv, go, &bo_list) ||
        !post_process_cmdline(go, bo_list) ||
        !create_launch_list(go, bo_list) )
    {
        delete bo_list;
        return false;
    }
    //
    // Save the bo_list in the global block options
    // so that the DTOR of go will clean it up.
    // We need the bo_list to stay alive until the end
    // of mpiexec because the launching blocks are referencing
    // the wdir/path/env from the bo_list
    //
    go->bo_list = bo_list;
    smpd_process.pGlobalBlockOpt = go;

    smpd_fix_up_host_tree(smpd_process.host_list);
    read_mpiexec_timeout();
    return true;
}
