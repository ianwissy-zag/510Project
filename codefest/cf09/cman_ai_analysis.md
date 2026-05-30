Tasks
1. The dominant kernel in my accelerator is matrix multiplication. There are several different matrix multiplication operations that I accelerate, but the most dominant in software runtime is the forward pass, which has dimensions [256 x 768] x [768 x 3072]. Multiplication is performed in BF16 and accumulation results are stored in FP32. 

2. For [N x K] x [K x M] multiplication, the number of computations is given by 2 x N x K x M - N x M, which can be approximated as 2 x N x K x M. In this case, this comes out to 2 x 256 x 768 x 3072 = 1,207,862,944 FLOPs. 

3. Lower Bound: In the lower bound case, full data reuse requires each multiplication to source two BF16s from DRAM and return one BF16. Accumulation requires sourcing one BF16, one FP32, and returning one FP32. This comes to 6 bytes transferred for each multiply and 10 bytes transferred for each addition. Since the number of adds and multiplies is approximately the same, each computation requires on average 8 bytes of data transferred, which corresponds to an AI of .125 FLOPs/Byte. 

Upper Bound: With perfect data reuse, the DRAM traffic is just the sum of the sizes of each matrix. The inputs are of size:  
256 * 768 * 2 bytes = 393,216 bytes  
768 * 3072 * 2 bytes = 4,718,592 bytes  
And the output matrix is of size:  
256 * 3072 * 4 bytes = 3,145,728 bytes.  
The total is then: 8,257,536 bytes, for an AI of 146.3 FLOP/Byte.

5. My design is current memory bound with an AI of 25.5 FLOP/Byte. The limiting factor is the interface bandwidth, though in reality the implementation is at the ridge point of the true compute bound when the unavoidable fill-drain losses associated with the batch size in my kernel are taken into account. My design is currently sitting under the ridge line for several reasons, all of which are being fixed in the next implementation of my design. First, is that I am not double buffering my accumulator SRAM, so I cannot perform computation while returning data to the host, which stalls the pipeline. Second, I am losing compute cycles to weight loading, which is being fixed by loading weights during the drain time of the previous tile's compute. Third, my design is not currently using the double buffered structure of the weight SRAM, which was resulting in weight loads and weight transfer from DRAM occurring sequentially and not concurrently. Fixing all these issues should bring my speedup over CPU from 19x to 25x. 
