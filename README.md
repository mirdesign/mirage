===========================================
    CONSOLE APPLICATION : MirAge PRoject
===========================================


    MirAge

    www.mirdesign.nl/mirage


===========================================

A research project for high performance pattern recognition and analysis on large data sets.

Version: 0.10
Created by: Maarten Pater (January 2015-2025)
Contact: mirage@mirdesign.nl | www.mirdesign.nl
License: Licensed under Creative Commons Attribution–NonCommercial 4.0 International (CC BY-NC 4.0) — free to use, share, and adapt, but not for commercial use. 
Funded: European Commission, https://ror.org/00k4n6c32, 602525, 643476

First public release: December 2025
    Manuscript: https://www.biorxiv.org/content/10.64898/2025.12.02.691787v1
    Manual: www.mirdesign.nl/mirage
===========================================


This code is Beta. Although torouglyly tested and highly optimized for performance,
the code layout is not optimal. It is intended, since refactoring did interfere with 
performance and therefore did not got prioritized. 

Be aware:
- This code is provided "as is" without any guarantees or liability on any side.
- I'm sorry for the code layout. Performance and research were the priority.
- Debug information is really rudimentary.
- Some code placements are intentional to guarantee optimal performance.
- GPU and CPU code is mixed in the same files to guarantee optimal data transfer.
- Some comments are kept, to show an alternative approach (mostly for performance reasons). This is not really documented well.
- CPU and CUDA supported. OpenCL support is dropped for now, since CUDA had focus due to availability of hardware during research.


Capabilities:
- Analysis on CPU and GPU
- Auto balancing of optimal settings to guarantee best performance on given hardware with given data
- Direct hits are detected and therefore not analyzed on GPU.
- Reliable time estimation for long running analysis
- Building hash tables and save to disk for re-use (using custom binary format)
- Does use hash lookups for fast presized pattern matching and calculates the offsize head and tail on the fly.
- Can run on multiple hosts in parallel, using shared network storage for communication. This way, supercomputer clusters can be used (64+ GPU nodes (2 CUDA cards per node) tested). Workload is automatically distributed.
- Is used for DNA analysis, but can be used to analyze any type of data that can be represented as a sequence of bytes.
- Use a configuration file to set parameters and file locations. Override parameters using commandline arguments.

This is part of a Manuscript.
Please refer to the Manuscript for further information.
url: https://www.biorxiv.org/content/10.64898/2025.12.02.691787v1
manual: www.mirdesign.nl/mirage


