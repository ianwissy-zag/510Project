I am choosing AXI as my interface, as it meets my requirements in terms of data bandwidth while being less complex to implement than PCIE, which would be the next step up in terms of throughput. 

The host platform is my laptop, which establishes the upper bound in terms of memory transfer speed (since data needs to transferred from DRAM before being transferred to my accelerator). The maximum throughput of my DRAM memory is 38.4 GB/s. This serves as the upper bound of meaningful data transfer bandwidth to my accelerator, beyond which I would be replacing transfer bandwidth with DRAM bandwidth as the limiting factor. 

I computed the arithmetic intensitiy of my kernel to be 267 FLOP/B. Therefore if I were to use SPI to transfer data to my accelerator, the maximum transfer rate I could expect would be 100Mbps, which would limit my throughput to 100Mbps / 8 b/B * 267 FLOP/B = 3.125 GFLOP/s, a significant downgrade from the CPU execution throughput.

AXI, on the other hand, can handle throughput of upwards of 64GB/s with a 1024 bit wide bus at 500MHz, which exceeds the throughput of my DRAM. This wide of a bus may not be reasonable/feasible, but running a 256 bit wide bus at 800MHz would still result in a throughput of 25.6 GB/s, which, while below the throughput of the DRAM, is still sufficient to allow a a throughput limit of 25.6 GB/s * 267 FLOP/B = 6853 GFLOP/s, a speedup of allow a speedup of 43x. The Amdahl runtime comparison to the theoretical maximum based on DRAM memory throughput (65x) is: 

65x speedup - .18 + .82/65 = .192 of original runtime

43x speedup - .18 + .82/43 = .199 of original runtime

Therefore I will use AXI as my transfer protocol, as it is simpler to implement than PCIe, and still allows significant speedup before becoming memory bound. Given that my initial speedup target for the kernel is only 10x, AXI is easily fast enough attain this goal.
