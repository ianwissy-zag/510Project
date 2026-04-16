The dominant kernels for my algorithm are the matmul_foward and matmul_backwards functions. They collectively take up 82% of the runtime of the program. They are considered together, as they are essentially identical in terms of functionality. For arithmetic intensity calculations, the forward function is used, as it takes up more runtime than the matmul_backwards function. The function is: 

```
void matmul_forward(float* out,
                    const float* inp, const float* weight, const float* bias,
                    int B, int T, int C, int OC) {
    // most of the running time is spent here and in matmul_backward
    // therefore, the implementation below is very mildly optimized
    // this function is otherwise identical to that of matmul_forward_naive()
    // OC is short for "output channels"
    // inp is (B,T,C), weight is (OC, C), bias is (OC)
    // out will be (B,T,OC)

    // make sure the tiled loop will be correct or fallback to naive version
    const int LOOP_UNROLL = 8;
    if (B*T % LOOP_UNROLL != 0) {
        matmul_forward_naive(out, inp, weight, bias, B, T, C, OC);
        return;
    }

    // collapse the B and T loops into one and turn it into a strided loop.
    // then we can tile the inner loop, and reuse the loaded weight LOOP_UNROLL many times
    #pragma omp parallel for
    for (int obt = 0; obt < B * T; obt += LOOP_UNROLL) {
        for (int o = 0; o < OC; o++) {
            // we'll keep LOOP_UNROLL many results in registers
            float result[LOOP_UNROLL];
            // initialize the bias, if it exists
            for (int ibt = 0; ibt < LOOP_UNROLL; ibt++) {
                result[ibt] = (bias != NULL) ? bias[o] : 0.0f;
            }
            // inner loops. Because we do LOOP_UNROLL steps of inner bt, we can cache
            // the value of weight[i + o * C] and reuse it.
            // we compile with -Ofast, so the compiler will turn the inner loop into FMAs
            for (int i = 0; i < C; i++) {
                float w = weight[i + o * C];
                for (int ibt = 0; ibt < LOOP_UNROLL; ibt++) {
                    int bt = obt + ibt;
                    result[ibt] += inp[bt * C + i] * w;
                }
            }
            // write back results to main memory
            for (int ibt = 0; ibt < LOOP_UNROLL; ibt++) {
                int bt = obt + ibt;
                out[bt * OC + o] = result[ibt];
            }
        }
    }
}
```

For square matrix multiplication, arithmetic intensity goes as N/6, where N is the width/length of the matrix. This function multiplies rectangular matrices, so the computation is more complicated. 

This function multiples matrices of dimension M x K by K x N, where M is the product of the batch size and the sequence length, K is the number of input channels, and N is the number of output channels. The computation goes as:

AI = 2 * M * K * N / (4 * (M * K + K * N + M * N))

For my benchmark execution, the batch size is 4, the sequence length is 1024, the number of input channels is 768, and the number of output channel is 3072. Therefore the computed algebraic intensity is:

AI = 2 * 4096 * 768 * 3072 / (4 * (4096 * 768 + 768 * 3072 + 4096 * 3072))

This resolves to:

AI = 267 FLOPs/Byte.


Note: If it is assumed that no caching is used and that every access goes to DRAM, the above calculation is incorrect. In this case, every matrix element must be accessed for every computation, which means that two loads must be done for every multiply and accumulate. Therefore for a large matrix, the approximate arithmetic intensity goes as 2/(2 * 4), for 32 bit (4 byte) FP operands, or .25 FLOPs/Byte. Actual execution exceeds the throughput that would be associated with this computed arithmetic intensity, so it is assumed that efficient caching is used, and that the AI of 267 FLOP/B is the more accurate choice to use when determining throughput limits. 

Roofline Analysis: There is no assigned document to place the analysis behind the roofline model graph, so I am including it in this one.

My processor is a 4-core 1.6GHz Intel i5-8250. The computed max throughput for this processor is 204.8 GFLOPs/s. My memory is 2400MT/s DDR4, with a computed maximum transfer rate of 38.4 GB/s. 

The ridgepoint is then 204.8 GFLOP/s divide by 38.4 GB/s, which equals 5.33 FLOP/B. This places the matrix multiplication algorithm firmly in the compute bound portion of the graph.

The time to execute a single step in the training algorithm was found to be ~4400 ms, which translates to approximately 155 GFLOP/s. 

I consider the feasibility of a 10x speedup in my custom hardware. This value sits well below the theoretical limit of 10217 GFLOP/s dictated by the transfer rate of my memory unit, so this will be my initial target for accelerated performance. 
