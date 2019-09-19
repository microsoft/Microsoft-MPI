The `mpiexec` code from MSMPI can be used to launch applications on multiple nodes. There are two ways to launch applications 
on multiple nodes:
1. Using MS-MPI Launch Service:
   * Start MS-MPI Launch Service on all the compute nodes
   * Specify your compute nodes in the `mpiexec` command line (either using `hosts` or `hostfile`)
2. Using `spmd`:
   * Run `spmd -d` on all compute nodes
      * The `spmd.exe` program is availble after installation of MSMPI (in the folder pointed by the `MSMPI_BIN` variable)
   * Specify your compute nodes in the `mpiexec` command line (either using `hosts` or `hostfile`)
