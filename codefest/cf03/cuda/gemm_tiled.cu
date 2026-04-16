#include <iostream>
#include <cuda_runtime.h>

#define N 1024
#define TILE_SIZE 8

// ---------------------------------------------------------
// Tiled Matrix Multiplication Kernel (Shared Memory)
// Uses TILE_SIZE x TILE_SIZE thread blocks
// ---------------------------------------------------------
__global__ void matMulTiled(const float *A, const float *B, float *C) {
    // Allocate shared memory for the tiles
    __shared__ float sA[TILE_SIZE][TILE_SIZE];
    __shared__ float sB[TILE_SIZE][TILE_SIZE];

    int bx = blockIdx.x;  int by = blockIdx.y;
    int tx = threadIdx.x; int ty = threadIdx.y;

    // Identify the row and column of the output element mapped to this thread
    int row = by * TILE_SIZE + ty;
    int col = bx * TILE_SIZE + tx;

    float value = 0.0f;

    // Loop over the tiles of the input matrices
    for (int ph = 0; ph < N / TILE_SIZE; ++ph) {
        
        // Load data into shared memory collaboratively
        sA[ty][tx] = A[row * N + ph * TILE_SIZE + tx];
        sB[ty][tx] = B[(ph * TILE_SIZE + ty) * N + col];

        // Wait for all threads to finish loading the tile before computing
        __syncthreads();

        // Perform the dot product for this tile
        for (int k = 0; k < TILE_SIZE; ++k) {
            value += sA[ty][k] * sB[k][tx];
        }

        // Wait for all threads to finish computing before loading the next tile
        __syncthreads();
    }

    // Write the final computed value to global memory
    if (row < N && col < N) {
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
    dim3 threadsPerBlock(TILE_SIZE, TILE_SIZE);
    dim3 blocksPerGrid(N / TILE_SIZE, N / TILE_SIZE);

    // Launch Tiled Kernel
    matMulTiled<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C);
    cudaDeviceSynchronize();

    // Copy result back to host
    cudaMemcpy(h_C, d_C, bytes, cudaMemcpyDeviceToHost);

    // Simple verification (1.0 * 2.0 * 1024 = 2048.0)
    std::cout << "[Tiled] Top-left element value: " << h_C[0] << " (Expected: 2048)" << std::endl;

    // Free memory
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C);
    free(h_A); free(h_B); free(h_C);

    return 0;
}
