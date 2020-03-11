# CacheSimulators
Attempts at writing direct-mapped and k-way set associate data caches simulators in C, as part of the Computer Organization elective class at SFSU.  

## Overview of files:
1) system1.c, a direct-mapped data cache of either 2KB and 4KB size, as specified by the user
2) system2.c, k-way set associative data cache, 2KB and 4KB

Both system1.c and system2.c use data memory address traces as input, which were available on the SFSU unixlab server, accessed through the macOS Terminal. The two files were copied to the server and run with an older gcc compiler, which introduced serious challenges to the project - code had to be rewritten. 
Instructions to run the programs with gcc on the SFSU Unixlab server is included in the header of each file, and they both have verbose mode, enabled by running them with the -v argv parameter. It has not been tested, but they should run on any system with gcc installed, if the trace files are provided as input.

## Trace files
gcc-1K.trace, gcc.ac, sjeng.xac, test.trace
