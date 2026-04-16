#include <iostream>
#include <cuda_runtime.h>

#define N 1024
#define BLOCK_SIZE 8 // Kept at 8 to match the thread block dimensions of the tiled version

// ---------------------------------------------------------
// Naive Matrix Multiplication Kernel
// One thread computes one element of the output matrix C
// ---------------------------------------------------------
__global__ void matMulNaive(const float *A, const float *B, float *C) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    // Bounds check
    if (row < N && col < N) {
        float value = 0.0f;
        for (int k = 0; k < N; ++k) {
            value += A[row * N + k] * B[k * N + col];
        }
        C[row * N + col] = value;
    }
}

// ---------------------------------------------------------
// Host Code
// ---------------------------------------------------------
int main() {
    size_t bytes = N * N * sizeof(float);

    // Host pointers
    float *h_A = (float*)malloc(bytes);
    float *h_B = (float*)malloc(bytes);
    float *h_C = (float*)malloc(bytes);

    // Initialize matrices with simple values
    for (int i = 0; i < N * N; ++i) {
        h_A[i] = 1.0f;
        h_B[i] = 2.0f;
    }

    // Device pointers
    float *d_A, *d_B, *d_C;
    cudaMalloc(&d_A, bytes);
    cudaMalloc(&d_B, bytes);
    cudaMalloc(&d_C, bytes);

    // Copy data from host to device
    cudaMemcpy(d_A, h_A, bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, bytes, cudaMemcpyHostToDevice);

    // Define Grid and Block dimensions
    dim3 threadsPerBlock(BLOCK_SIZE, BLOCK_SIZE);
    dim3 blocksPerGrid(N / BLOCK_SIZE, N / BLOCK_SIZE);

    // Launch Naive Kernel
    matMulNaive<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C);
    cudaDeviceSynchronize();

    // Copy result back to host
    cudaMemcpy(h_C, d_C, bytes, cudaMemcpyDeviceToHost);

    // Simple verification (1.0 * 2.0 * 1024 = 2048.0)
    std::cout << "[Naive] Top-left element value: " << h_C[0] << " (Expected: 2048)" << std::endl;

    // Free memory
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C);
    free(h_A); free(h_B); free(h_C);

    return 0;
}
