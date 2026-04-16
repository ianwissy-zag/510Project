The base platform that my algorithm was tested on is my laptop, which uses an Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz with four cores. My main memory is a 2400 MT/s DDR4. I am executing a compiled C program with a batch size of 4. The measured run time for a single training step on my program across 41 different steps is 5230.8833 ms per step. 

Measuring the memory usage of the program with perf resulted in 12,836,981,295 LLC-load-misses and 985,307,204 LLC-store-misses for a total of 13,822,288,499 loads from DRAM. Since a cache line is 64 bytes, this represents a total memory transfer of 884.6 GB. The wall time execution of the program is 680.31 seconds, for an average transfer rate of 1.30 GB/s. 

Measuring the throughput of my program, my program execution consisted of 3,526,893,582,613 instructions, which executed over the course of 6,590,996,224,566 cycles and 380.571506053 seconds, for computed IPC and IPS values of 9.26 GIPS and .54 IPC. 
