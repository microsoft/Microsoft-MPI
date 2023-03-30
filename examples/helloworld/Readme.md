## Compile and run a sample MPI code on Windows
1. Download MS-MPI SDK and Redist installers and install them. The download link to a stable realease is available from [this](https://github.com/microsoft/Microsoft-MPI/releases) page.
2. After installation, you can verify that the MS-MPI environment variables have been set correctly (you will want to use these environment variables in Visual Studio)
![inline](./screenshots/set_msmpi.png)
3. Open Visual Studio and create a Console App project. Let's name the project `MPIHelloWorld`
   * Instead of creating a project, you may open the provided `MPIHelloWorld.vcxproj` project file in Visual Studio and go to step 7. 
4. Use [this](MPIHelloWorld.cpp) code in the newly created project
5. Setup the include directories so that the compiler can find the MS-MPI header files. Note that we will be building 
for 64 bits so we will point the include directory to `$(MSMPI_INC);$(MSMPI_INC)\x64`. If you will be building for 32 bits 
please use `$(MSMPI_INC);$(MSMPI_INC)\x86`
![inline](./screenshots/inc_dir.png)
6. Setup the linker options. Add `msmpi.lib` to the Additional Dependencies and also add `$(MSMPI_LIB64)` to the Additional 
Library Directories. Note that we will be building for 64 bits so we will point the Additional Library Directories to $(MSMPI_LIB64). 
If you will be building for 32 bits please use `$(MSMPI_LIB32)`
![inline](./screenshots/lib_dir.png)
7. Build the MPIHelloWorld project
![inline](./screenshots/vs_build.png)
8. Test run the program on the command line
![inline](./screenshots/mpiexec.png)

Alternatively, you can use Developer Command Prompt for your version of Visual Studio to compile and link the `MPIHelloWorld.cpp` 
code (replacing steps 3-7 above). To build a 64-bit application, choose x64 Native Tools Command Prompt from the Visual Studio folder 
in the Start menu.
![inline](./screenshots/x64_prompt.png)

To compile your program into an `.obj` file, go to the folder where `MPIHelloWorld.cpp` exists and run (you may ignore the warning message):<br>
`cl /I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" /c MPIHelloWorld.cpp`
![inline](./screenshots/compile.png)

To create an executable file from the .obj file created in the previous step, run:<br>
`link /machine:x64 /out:MPIHelloWorld.exe "msmpi.lib" /libpath:"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" MPIHelloWorld.obj`
![inline](./screenshots/link.png)

You may use the `nmake` command from Developer Command Prompt to compile and build the exmaple using the provided [`Makefile`](Makefile).
