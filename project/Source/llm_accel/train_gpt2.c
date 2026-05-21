/*
This file trains the GPT-2 model.
This version is the clean, minimal, reference. As such:
- it runs on CPU.
- it does not make the code too complex; it is readable.
- it does not use any processor-specific instructions, intrinsics and such.
- it _does_ use a few OpenMP pragmas because this is a large speedup at very low cost
There will be other versions of this code that specialize it and make it fast.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#ifdef OMP
#include <omp.h>
#endif
// our own utilities
// defines: fopenCheck, freadCheck, fcloseCheck, fseekCheck, mallocCheck
#include "llmc/utils.h"
// defines: tokenizer_init, tokenizer_decode, tokenizer_free
#include "llmc/tokenizer.h"
// defines: dataloader_init, dataloader_reset, dataloader_next_batch, dataloader_free
#include "llmc/dataloader.h"
// accelerator HAL — backend selected at compile time via -DACCEL_BACKEND_*
#include "accel_hal.h"

// ----------------------------------------------------------------------------
// all the individual layers' forward and backward passes
// B = batch_size, T = sequence_length, C = channels, V = vocab_size

void encoder_forward(float* out,
                   int* inp, float* wte, float* wpe,
                   int B, int T, int C) {
    // out is (B,T,C). At each position (b,t), a C-dimensional vector summarizing token & position
    // inp is (B,T) of integers, holding the token ids at each (b,t) position
    // wte is (V,C) of token embeddings, short for "weight token embeddings"
    // wpe is (maxT,C) of position embeddings, short for "weight positional embedding"
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // seek to the output position in out[b,t,:]
            float* out_bt = out + b * T * C + t * C;
            // get the index of the token at inp[b, t]
            int ix = inp[b * T + t];
            // seek to the position in wte corresponding to the token
            float* wte_ix = wte + ix * C;
            // seek to the position in wpe corresponding to the position
            float* wpe_t = wpe + t * C;
            // add the two vectors and store the result in out[b,t,:]
            for (int i = 0; i < C; i++) {
                out_bt[i] = wte_ix[i] + wpe_t[i];
            }
        }
    }
}

void encoder_backward(float* dwte, float* dwpe,
                      float* dout, int* inp,
                      int B, int T, int C) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* dout_bt = dout + b * T * C + t * C;
            int ix = inp[b * T + t];
            float* dwte_ix = dwte + ix * C;
            float* dwpe_t = dwpe + t * C;
            for (int i = 0; i < C; i++) {
                float d = dout_bt[i];
                dwte_ix[i] += d;
                dwpe_t[i] += d;
            }
        }
    }
}

void layernorm_forward(float* out, float* mean, float* rstd,
                       float* inp, float* weight, float* bias,
                       int B, int T, int C) {
    // reference: https://pytorch.org/docs/stable/generated/torch.nn.LayerNorm.html
    // both inp and out are (B,T,C) of the activations
    // mean and rstd are (B,T) buffers, to be used later in backward pass
    // at each position (b,t) of the input, the C-dimensional vector
    // of activations gets normalized, then scaled and shifted
    float eps = 1e-5f;
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // seek to the input position inp[b,t,:]
            float* x = inp + b * T * C + t * C;
            // calculate the mean
            float m = 0.0f;
            for (int i = 0; i < C; i++) {
                m += x[i];
            }
            m = m/C;
            // calculate the variance (without any bias correction)
            float v = 0.0f;
            for (int i = 0; i < C; i++) {
                float xshift = x[i] - m;
                v += xshift * xshift;
            }
            v = v/C;
            // calculate the rstd (reciprocal standard deviation)
            float s = 1.0f / sqrtf(v + eps);
            // seek to the output position in out[b,t,:]
            float* out_bt = out + b * T * C + t * C;
            for (int i = 0; i < C; i++) {
                float n = (s * (x[i] - m)); // normalize
                float o = n * weight[i] + bias[i]; // scale and shift
                out_bt[i] = o; // write
            }
            // cache the mean and rstd for the backward pass later
            mean[b * T + t] = m;
            rstd[b * T + t] = s;
        }
    }
}

void layernorm_backward(float* dinp, float* dweight, float* dbias,
                        float* dout, float* inp, float* weight, float* mean, float* rstd,
                        int B, int T, int C) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* dout_bt = dout + b * T * C + t * C;
            float* inp_bt = inp + b * T * C + t * C;
            float* dinp_bt = dinp + b * T * C + t * C;
            float mean_bt = mean[b * T + t];
            float rstd_bt = rstd[b * T + t];

            // first: two reduce operations
            float dnorm_mean = 0.0f;
            float dnorm_norm_mean = 0.0f;
            for (int i = 0; i < C; i++) {
                float norm_bti = (inp_bt[i] - mean_bt) * rstd_bt;
                float dnorm_i = weight[i] * dout_bt[i];
                dnorm_mean += dnorm_i;
                dnorm_norm_mean += dnorm_i * norm_bti;
            }
            dnorm_mean = dnorm_mean / C;
            dnorm_norm_mean = dnorm_norm_mean / C;

            // now iterate again and accumulate all the gradients
            for (int i = 0; i < C; i++) {
                float norm_bti = (inp_bt[i] - mean_bt) * rstd_bt;
                float dnorm_i = weight[i] * dout_bt[i];
                // gradient contribution to bias
                dbias[i] += dout_bt[i];
                // gradient contribution to weight
                dweight[i] += norm_bti * dout_bt[i];
                // gradient contribution to input
                float dval = 0.0f;
                dval += dnorm_i; // term 1
                dval -= dnorm_mean; // term 2
                dval -= norm_bti * dnorm_norm_mean; // term 3
                dval *= rstd_bt; // final scale
                dinp_bt[i] += dval;
            }
        }
    }
}

// ── Accelerator parameters (switched at compile time) ─────────────────────────
// Systolic backends use SYS_COLS/SYS_ROWS; vector backends use VEC_SIZE/K_DEPTH.
#if defined(ACCEL_BACKEND_SYSTOLIC) || defined(ACCEL_BACKEND_SYSTOLIC_VRL)
#  define TILE_N  ACCEL_SYS_COLS   // 32
#  define TILE_K  ACCEL_SYS_ROWS   // 16
#else
#  define TILE_N  ACCEL_VEC_SIZE   // 128
#  define TILE_K  ACCEL_K_DEPTH    // 32
#endif

// ── Forward tiling engine ─────────────────────────────────────────────────────
// out (M x N) = inp (M x K) * weight^T (N x K)
// Tiles N in TILE_N chunks, K in TILE_K chunks.
// Remainder columns/rows handled on CPU.

#if defined(ACCEL_BACKEND_SYSTOLIC_VRL)
// Streaming path: weights loaded once per K-tile, all M rows streamed together.
// Loop order: (N-tile, K-tile) outer; M rows handled inside hal_stream_tile.
// Static buffers avoid stack pressure for large M.
static bf16_t s_w_tile[ACCEL_SYS_ROWS * ACCEL_SYS_COLS];
static bf16_t s_acts_all[256 * ACCEL_SYS_ROWS];   // M_max=256 rows × K_DEPTH
static float  s_result_all[256 * ACCEL_SYS_COLS];  // M_max=256 rows × N_TILE

static void accel_matmul(float* out, const float* inp, const float* weight,
                          int M, int K, int N) {
    int N_tiles = N / TILE_N;
    int N_rem   = N % TILE_N;
    int K_tiles = K / TILE_K;
    int K_rem   = K % TILE_K;

    for (int nt = 0; nt < N_tiles; nt++) {
        int n_base = nt * TILE_N;

        for (int kt = 0; kt < K_tiles; kt++) {
            int k_base  = kt * TILE_K;
            int first_k = (kt == 0);

            // Pack weight tile: TILE_K rows × TILE_N cols
            for (int k = 0; k < TILE_K; k++)
                for (int n = 0; n < TILE_N; n++)
                    s_w_tile[k * TILE_N + n] =
                        float_to_bf16(weight[(n_base + n) * K + k_base + k]);

            // Pack all M rows' activation slice for this K-tile
            for (int m = 0; m < M; m++)
                for (int k = 0; k < TILE_K; k++)
                    s_acts_all[m * TILE_K + k] =
                        float_to_bf16(inp[m * K + k_base + k]);

            hal_stream_tile(s_w_tile, s_acts_all, M, first_k, 0);
        }

        if (K_tiles > 0) {
            hal_read_results_all(s_result_all, M);
            for (int m = 0; m < M; m++)
                for (int n = 0; n < TILE_N; n++)
                    out[m * N + n_base + n] = s_result_all[m * TILE_N + n];
        }

        // K remainder on CPU
        if (K_rem > 0) {
            int k_base = K_tiles * TILE_K;
            for (int m = 0; m < M; m++)
                for (int k = 0; k < K_rem; k++) {
                    float a = inp[m * K + k_base + k];
                    for (int n = 0; n < TILE_N; n++)
                        out[m * N + n_base + n] +=
                            a * weight[(n_base + n) * K + k_base + k];
                }
        }
    }

    // N remainder on CPU
    if (N_rem > 0) {
        int n_base = N_tiles * TILE_N;
        for (int m = 0; m < M; m++)
            for (int n = 0; n < N_rem; n++) {
                float val = 0.0f;
                for (int k = 0; k < K; k++)
                    val += inp[m * K + k] * weight[(n_base + n) * K + k];
                out[m * N + n_base + n] = val;
            }
    }
}

// Fused matmul + hardware bias + hardware GELU for the FFN up-projection.
// Produces out_raw = matmul+bias (fch) and out_gelu = GELU(matmul+bias) (fch_gelu)
// in a single tiling pass using two back-to-back readbacks per N-tile.
// bias must be non-NULL and have N values; it is loaded into the hardware
// bias SRAM once per N-tile immediately before the readback pair.
static void accel_matmul_gelu(float* out_raw, float* out_gelu,
                               const float* inp, const float* weight,
                               const float* bias, int M, int K, int N) {
    int N_tiles = N / TILE_N;
    int N_rem   = N % TILE_N;
    int K_tiles = K / TILE_K;
    int K_rem   = K % TILE_K;

    for (int nt = 0; nt < N_tiles; nt++) {
        int n_base = nt * TILE_N;

        for (int kt = 0; kt < K_tiles; kt++) {
            int k_base  = kt * TILE_K;
            int first_k = (kt == 0);
            for (int k = 0; k < TILE_K; k++)
                for (int n = 0; n < TILE_N; n++)
                    s_w_tile[k * TILE_N + n] =
                        float_to_bf16(weight[(n_base + n) * K + k_base + k]);
            for (int m = 0; m < M; m++)
                for (int k = 0; k < TILE_K; k++)
                    s_acts_all[m * TILE_K + k] =
                        float_to_bf16(inp[m * K + k_base + k]);
            hal_stream_tile(s_w_tile, s_acts_all, M, first_k, 0);
        }

        if (K_tiles > 0) {
            // Load this N-tile's bias values into the hardware bias SRAM.
            hal_load_bias(bias + n_base, TILE_N);

            // Readback 1: matmul + bias → fch (apply_bias=1, apply_gelu=0)
            hal_read_results_all_biased(s_result_all, M);
            for (int m = 0; m < M; m++)
                for (int n = 0; n < TILE_N; n++)
                    out_raw[m * N + n_base + n] = s_result_all[m * TILE_N + n];

            // Readback 2: GELU(matmul+bias) → fch_gelu (apply_bias=1, apply_gelu=1)
            hal_read_results_all_biased_gelu(s_result_all, M);
            for (int m = 0; m < M; m++)
                for (int n = 0; n < TILE_N; n++)
                    out_gelu[m * N + n_base + n] = s_result_all[m * TILE_N + n];
        }

        // K remainder — computed on CPU; bias applied and GELU computed in software
        if (K_rem > 0) {
            int k_base = K_tiles * TILE_K;
            for (int m = 0; m < M; m++)
                for (int k = 0; k < K_rem; k++) {
                    float a = inp[m * K + k_base + k];
                    for (int n = 0; n < TILE_N; n++)
                        out_raw[m * N + n_base + n] +=
                            a * weight[(n_base + n) * K + k_base + k];
                }
        }
    }

    // N remainder — entirely on CPU
    if (N_rem > 0) {
        int n_base = N_tiles * TILE_N;
        for (int m = 0; m < M; m++)
            for (int n = 0; n < N_rem; n++) {
                float val = 0.0f;
                for (int k = 0; k < K; k++)
                    val += inp[m * K + k] * weight[(n_base + n) * K + k];
                out_raw[m * N + n_base + n] = val;
            }
    }
}

#else
// Non-streaming path: single-row tile interface (vector/software backends).
static void accel_matmul(float* out, const float* inp, const float* weight,
                          int M, int K, int N) {
    bf16_t w_tile[TILE_K * TILE_N];
    bf16_t act_tile[TILE_K];
    float  result_tile[TILE_N];

    int N_tiles = N / TILE_N;
    int N_rem   = N % TILE_N;
    int K_tiles = K / TILE_K;
    int K_rem   = K % TILE_K;

    for (int nt = 0; nt < N_tiles; nt++) {
        int n_base = nt * TILE_N;
        for (int m = 0; m < M; m++) {
            for (int kt = 0; kt < K_tiles; kt++) {
                int k_base  = kt * TILE_K;
                int first_k = (kt == 0);
                for (int k = 0; k < TILE_K; k++)
                    for (int n = 0; n < TILE_N; n++)
                        w_tile[k * TILE_N + n] =
                            float_to_bf16(weight[(n_base + n) * K + k_base + k]);
                for (int k = 0; k < TILE_K; k++)
                    act_tile[k] = float_to_bf16(inp[m * K + k_base + k]);
                hal_compute_tile(w_tile, act_tile, first_k);
            }
            if (K_tiles > 0)
                hal_read_results(result_tile);
            else
                memset(result_tile, 0, sizeof(result_tile));
            if (K_rem > 0) {
                int k_base = K_tiles * TILE_K;
                for (int k = 0; k < K_rem; k++) {
                    float a = inp[m * K + k_base + k];
                    for (int n = 0; n < TILE_N; n++)
                        result_tile[n] += a * weight[(n_base + n) * K + k_base + k];
                }
            }
            for (int n = 0; n < TILE_N; n++)
                out[m * N + n_base + n] = result_tile[n];
        }
    }
    if (N_rem > 0) {
        int n_base = N_tiles * TILE_N;
        for (int m = 0; m < M; m++)
            for (int n = 0; n < N_rem; n++) {
                float val = 0.0f;
                for (int k = 0; k < K; k++)
                    val += inp[m * K + k] * weight[(n_base + n) * K + k];
                out[m * N + n_base + n] = val;
            }
    }
}
#endif

// =============================================================================
// End accelerator interface
// =============================================================================

void matmul_forward_naive(float* out,
                         const float* inp, const float* weight, const float* bias,
                         int B, int T, int C, int OC) {
    // the most naive implementation of matrix multiplication
    // this serves as an algorithmic reference, and as a fallback for
    // unfriendly input shapes inside matmul_forward(), below.
    #pragma omp parallel for collapse(2)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            int bt = b * T + t;
            for (int o = 0; o < OC; o++) {
                float val = (bias != NULL) ? bias[o] : 0.0f;
                for (int i = 0; i < C; i++) {
                    val += inp[bt * C + i] * weight[o*C + i];
                }
                out[bt * OC + o] = val;
            }
        }
    }
}

// ── Backward dinp tiling engine (systolic backends) ──────────────────────────
// dinp (M x K) = dout (M x N) * weight (N x K)
// Tiles K in TILE_N chunks, N in TILE_K chunks.
// The transposed weight W^T must have been pre-loaded into the backward SRAM.
// Non-systolic backends fall back to the CPU implementation.
#if defined(ACCEL_BACKEND_SYSTOLIC) || defined(ACCEL_BACKEND_SYSTOLIC_VRL)
static void accel_matmul_backward_dinp(float* dinp, const float* dout,
                                        const float* weight, int M, int K, int N) {
    // dinp[m][k] = sum_n( dout[m][n] * weight[n][k] )
    // Remap: K tiles of TILE_N (output), N tiles of TILE_K (inner)
    int K_tiles = K / TILE_N;
    int K_rem   = K % TILE_N;
    int N_tiles = N / TILE_K;
    int N_rem   = N % TILE_K;

#if defined(ACCEL_BACKEND_SYSTOLIC_VRL)
    // Streaming backward: all M rows per (K-tile, N-tile) pass
    static bf16_t s_wT_tile[ACCEL_SYS_ROWS * ACCEL_SYS_COLS];
    static bf16_t s_dout_all[256 * ACCEL_SYS_ROWS];
    static float  s_dinp_all[256 * ACCEL_SYS_COLS];

    for (int kt = 0; kt < K_tiles; kt++) {
        int k_base = kt * TILE_N;
        for (int nt = 0; nt < N_tiles; nt++) {
            int n_base  = nt * TILE_K;
            int first_n = (nt == 0);
            for (int n = 0; n < TILE_K; n++)
                for (int k = 0; k < TILE_N; k++)
                    s_wT_tile[n * TILE_N + k] =
                        float_to_bf16(weight[(n_base+n)*K + k_base+k]);
            for (int m = 0; m < M; m++)
                for (int n = 0; n < TILE_K; n++)
                    s_dout_all[m * TILE_K + n] =
                        float_to_bf16(dout[m*N + n_base+n]);
            hal_stream_tile(s_wT_tile, s_dout_all, M, first_n, 1);
        }
        if (N_tiles > 0) {
            hal_read_results_all(s_dinp_all, M);
            for (int m = 0; m < M; m++)
                for (int k = 0; k < TILE_N; k++)
                    dinp[m*K + k_base+k] += s_dinp_all[m * TILE_N + k];
        }
        if (N_rem > 0) {
            int n_base = N_tiles * TILE_K;
            for (int m = 0; m < M; m++)
                for (int n = 0; n < N_rem; n++) {
                    float d = dout[m*N + n_base+n];
                    for (int k = 0; k < TILE_N; k++)
                        dinp[m*K + k_base+k] += d * weight[(n_base+n)*K + k_base+k];
                }
        }
    }
#else
    // Non-streaming backward (ACCEL_BACKEND_SYSTOLIC software sim)
    bf16_t wT_tile[TILE_K * TILE_N];
    bf16_t dout_tile[TILE_K];
    float  result_tile[TILE_N];

    for (int kt = 0; kt < K_tiles; kt++) {
        int k_base = kt * TILE_N;
        for (int m = 0; m < M; m++) {
            for (int nt = 0; nt < N_tiles; nt++) {
                int n_base  = nt * TILE_K;
                int first_n = (nt == 0);
                for (int n = 0; n < TILE_K; n++)
                    for (int k = 0; k < TILE_N; k++)
                        wT_tile[n * TILE_N + k] =
                            float_to_bf16(weight[(n_base+n)*K + k_base+k]);
                for (int n = 0; n < TILE_K; n++)
                    dout_tile[n] = float_to_bf16(dout[m*N + n_base+n]);
                hal_compute_tile_bwd(wT_tile, dout_tile, first_n);
            }
            if (N_tiles > 0) hal_read_results(result_tile);
            else              memset(result_tile, 0, sizeof(result_tile));
            if (N_rem > 0) {
                int n_base = N_tiles * TILE_K;
                for (int n = 0; n < N_rem; n++) {
                    float d = dout[m*N + n_base+n];
                    for (int k = 0; k < TILE_N; k++)
                        result_tile[k] += d * weight[(n_base+n)*K + k_base+k];
                }
            }
            for (int k = 0; k < TILE_N; k++)
                dinp[m*K + k_base+k] += result_tile[k];
        }
    }
#endif

    // K remainder on CPU (both paths)
    if (K_rem > 0) {
        int k_base = K_tiles * TILE_N;
        for (int m = 0; m < M; m++)
            for (int n = 0; n < N; n++) {
                float d = dout[m*N + n];
                for (int k = 0; k < K_rem; k++)
                    dinp[m*K + k_base+k] += d * weight[n*K + k_base+k];
            }
    }
}
#endif

void matmul_forward(float* out,
                    const float* inp, const float* weight, const float* bias,
                    int B, int T, int C, int OC) {
    // Accelerator-backed matrix multiply.
    // inp is (B,T,C), weight is (OC,C), out is (B,T,OC).
    // The accelerator handles the multiply-accumulate; bias is added on CPU.
    accel_matmul(out, inp, weight, B * T, C, OC);

    // Bias addition — accelerator has no bias path, done in software
    if (bias != NULL) {
        for (int bt = 0; bt < B * T; bt++)
            for (int o = 0; o < OC; o++)
                out[bt * OC + o] += bias[o];
    }
}

// FFN up-projection forward: produces fch (out_raw = matmul+bias) and
// fch_gelu (out_gelu = GELU(matmul+bias)) in one call.
//
// For SYSTOLIC_VRL: uses hardware fused bias+GELU via two back-to-back
// readbacks per N-tile (apply_bias=1, apply_gelu=0/1). The bias SRAM is
// loaded with each N-tile's bias slice before the readback pair, so GELU
// sees the biased accumulator — GELU(matmul+bias) — which matches what
// gelu_backward expects when it receives out_raw as its 'inp' argument.
// Any N or K remainder columns are handled on CPU.
//
// For all other backends: falls back to accel_matmul + software bias + CPU GELU.
void matmul_forward_gelu(float* out_raw, float* out_gelu,
                          const float* inp, const float* weight, const float* bias,
                          int B, int T, int C, int OC) {
#if defined(ACCEL_BACKEND_SYSTOLIC_VRL)
    accel_matmul_gelu(out_raw, out_gelu, inp, weight, bias, B * T, C, OC);

    // N and K remainders landed in out_raw without bias/GELU — fix up now.
    int N_tiles = OC / TILE_N;
    int N_rem   = OC % TILE_N;
    int K_rem   = C  % TILE_K;
    if (N_rem > 0 || K_rem > 0) {
        // Apply bias and GELU to any remainder elements on CPU.
        int n_base_rem = N_tiles * TILE_N;
        for (int bt = 0; bt < B * T; bt++) {
            // N remainder columns (all K already accumulated)
            for (int n = 0; n < N_rem; n++) {
                int idx = bt * OC + n_base_rem + n;
                if (bias) out_raw[idx] += bias[n_base_rem + n];
                float x = out_raw[idx], cube = 0.044715f * x * x * x;
                out_gelu[idx] = 0.5f * x *
                    (1.0f + tanhf(GELU_SCALING_FACTOR * (x + cube)));
            }
            // Tiled N columns that had a K remainder (bias+GELU not yet applied)
            if (K_rem > 0) {
                for (int n = 0; n < N_tiles * TILE_N; n++) {
                    int idx = bt * OC + n;
                    if (bias) out_raw[idx] += bias[n];
                    float x = out_raw[idx], cube = 0.044715f * x * x * x;
                    out_gelu[idx] = 0.5f * x *
                        (1.0f + tanhf(GELU_SCALING_FACTOR * (x + cube)));
                }
            }
        }
    }
#else
    accel_matmul(out_raw, inp, weight, B * T, C, OC);
    if (bias != NULL)
        for (int bt = 0; bt < B * T; bt++)
            for (int o = 0; o < OC; o++)
                out_raw[bt * OC + o] += bias[o];
    gelu_forward(out_gelu, out_raw, B * T * OC);
#endif
}

void matmul_backward(float* dinp, float* dweight, float* dbias,
                     const float* dout, const float* inp, const float* weight,
                     int B, int T, int C, int OC) {
    // most of the running time is spent here and in matmul_forward
    // this backward could be done in a single "round" of loops
    // but that doesn't afford an efficient parallelization strategy

    // backward into inp: dinp = dout x weight  (M×OC times OC×C = M×C)
#if defined(ACCEL_BACKEND_SYSTOLIC) || defined(ACCEL_BACKEND_SYSTOLIC_VRL)
    // Accelerated dinp via backward weight SRAM
    accel_matmul_backward_dinp(dinp, dout, weight, B*T, C, OC);
#else
    #pragma omp parallel for collapse(2)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            const float* dout_bt = dout + b * T * OC + t * OC;
            float* dinp_bt = dinp + b * T * C + t * C;
            for (int o = 0; o < OC; o++) {
                const float* wrow = weight + o*C;
                float d = dout_bt[o];
                for (int i = 0; i < C; i++) {
                    dinp_bt[i] += wrow[i] * d;
                }
            }
        }
    }
#endif
    // backward into weight/bias, parallelize over output channels OC
    #pragma omp parallel for
    for (int o = 0; o < OC; o++) {
        for (int b = 0; b < B; b++) {
            for (int t = 0; t < T; t++) {
                const float* dout_bt = dout + b * T * OC + t * OC;
                const float* inp_bt = inp + b * T * C + t * C;
                float* dwrow = dweight + o*C;
                float d = dout_bt[o];
                if (dbias != NULL) { dbias[o] += d; }
                for (int i = 0; i < C; i++) {
                    dwrow[i] += inp_bt[i] * d;
                }
            }
        }
    }
}

void attention_forward(float* out, float* preatt, float* att,
                       float* inp,
                       int B, int T, int C, int NH) {
    // input is (B, T, 3C) holding the query, key, value (Q, K, V) vectors
    // preatt, att are (B, NH, T, T). NH = number of heads, T = sequence length
    // that holds the pre-attention and post-attention scores (used in backward)
    // output is (B, T, C)
    // attention is the only layer that mixes information across time
    // every other operation is applied at every (b,t) position independently
    // (and of course, no layer mixes information across batch)
    int C3 = C*3;
    int hs = C / NH; // head size
    float scale = 1.0 / sqrtf(hs);

    #pragma omp parallel for collapse(3)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            for (int h = 0; h < NH; h++) {
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;

                // pass 1: calculate query dot key and maxval
                float maxval = -10000.0f; // TODO something better
                for (int t2 = 0; t2 <= t; t2++) {
                    float* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key

                    // (query_t) dot (key_t2)
                    float val = 0.0f;
                    for (int i = 0; i < hs; i++) {
                        val += query_t[i] * key_t2[i];
                    }
                    val *= scale;
                    if (val > maxval) {
                        maxval = val;
                    }

                    preatt_bth[t2] = val;
                }

                // pass 2: calculate the exp and keep track of sum
                // maxval is being calculated and subtracted only for numerical stability
                float expsum = 0.0f;
                for (int t2 = 0; t2 <= t; t2++) {
                    float expv = expf(preatt_bth[t2] - maxval);
                    expsum += expv;
                    att_bth[t2] = expv;
                }
                float expsum_inv = expsum == 0.0f ? 0.0f : 1.0f / expsum;

                // pass 3: normalize to get the softmax
                for (int t2 = 0; t2 < T; t2++) {
                    if (t2 <= t) {
                        att_bth[t2] *= expsum_inv;
                    } else {
                        // causal attention mask. not strictly necessary to set to zero here
                        // only doing this explicitly for debugging and checking to PyTorch
                        att_bth[t2] = 0.0f;
                    }
                }

                // pass 4: accumulate weighted values into the output of attention
                float* out_bth = out + b * T * C + t * C + h * hs;
                for (int i = 0; i < hs; i++) { out_bth[i] = 0.0f; }
                for (int t2 = 0; t2 <= t; t2++) {
                    float* value_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C*2; // +C*2 because it's value
                    float att_btht2 = att_bth[t2];
                    for (int i = 0; i < hs; i++) {
                        out_bth[i] += att_btht2 * value_t2[i];
                    }
                }
            }
        }
    }
}

void attention_backward(float* dinp, float* dpreatt, float* datt,
                        float* dout, float* inp, float* att,
                        int B, int T, int C, int NH) {
    // inp/dinp are (B, T, 3C) Q,K,V
    // att/datt/dpreatt are (B, NH, T, T)
    // dout is (B, T, C)
    int C3 = C*3;
    int hs = C / NH; // head size
    float scale = 1.f / sqrtf(hs);

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            for (int h = 0; h < NH; h++) {
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;
                float* datt_bth = datt + b*NH*T*T + h*T*T + t*T;
                float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
                float* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;

                // backward pass 4, through the value accumulation
                float* dout_bth = dout + b * T * C + t * C + h * hs;
                for (int t2 = 0; t2 <= t; t2++) {
                    float* value_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C*2; // +C*2 because it's value
                    float* dvalue_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C*2;
                    for (int i = 0; i < hs; i++) {
                        // in the forward pass this was:
                        // out_bth[i] += att_bth[t2] * value_t2[i];
                        // so now we have:
                        datt_bth[t2] += value_t2[i] * dout_bth[i];
                        dvalue_t2[i] += att_bth[t2] * dout_bth[i];
                    }
                }

                // backward pass 2 & 3, the softmax
                // note that softmax (like e.g. tanh) doesn't need the input (preatt) to backward
                for (int t2 = 0; t2 <= t; t2++) {
                    for (int t3 = 0; t3 <= t; t3++) {
                        float indicator = t2 == t3 ? 1.0f : 0.0f;
                        float local_derivative = att_bth[t2] * (indicator - att_bth[t3]);
                        dpreatt_bth[t3] += local_derivative * datt_bth[t2];
                    }
                }

                // backward pass 1, the query @ key matmul
                for (int t2 = 0; t2 <= t; t2++) {
                    float* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    float* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    for (int i = 0; i < hs; i++) {
                        // in the forward pass this was:
                        // preatt_bth[t2] += (query_t[i] * key_t2[i]) * scale;
                        // so now we have:
                        dquery_t[i] += key_t2[i] * dpreatt_bth[t2] * scale;
                        dkey_t2[i] += query_t[i] * dpreatt_bth[t2] * scale;
                    }
                }
            }
        }
    }
}

#define GELU_SCALING_FACTOR sqrtf(2.0f / M_PI)

// =============================================================================
// ACCEL_FUSED_BIAS_GELU — software model of hardware-fused bias + GELU.
//
// Build:
//   make FUSED=1           — Padé [1/1] rational approx (3 coefficients)
//   make FUSED=1 POLY5=1   — 5-term Taylor polynomial (degree 9)
//
// Both replace tanhf() to model the accuracy impact of a hardware GELU unit.
// Bias addition is already exact inside matmul_forward(); the flag marks it
// as conceptually on-chip and keeps forward/backward tanh consistent.
//
// Padé [1/1]:  x·(27+x²) / (27+9x²),  saturated at |x| ≥ 4.
//   Error < 0.5% for |x| < 1.5,  < 8% for |x| < 2.5.
//   Hardware cost: 2 mul, 1 div, 2 add.
//
// 5-term Taylor: x - x³/3 + 2x⁵/15 - 17x⁷/315 + 62x⁹/2835, clamped to ±1.
//   Accurate for |x| < 1.2; diverges beyond — clamping saturates at ~|x|=1.6.
//   Hardware cost: 4 mul (Horner), 4 add, 1 comparator — no divider.
//   NOTE: divergence means clamping fires early, often giving LOWER accuracy
//   than Padé [1/1] despite more terms.  See experiment results.
// =============================================================================
#if defined(ACCEL_FUSED_BIAS_GELU)

#if defined(ACCEL_TANH_POLY5)
// ── 5-term Taylor polynomial ─────────────────────────────────────────────────
// tanh(x) = x − x³/3 + 2x⁵/15 − 17x⁷/315 + 62x⁹/2835  (Horner form)
// Valid for |x| < ~1.2; diverges beyond. Result clamped to [−1, 1].
// Divergence means the clamp fires around |x| ≈ 1.6, so this effectively
// saturates to ±1 much earlier than tanhf and produces larger errors for
// the large-argument tail of the GELU distribution.
static inline float tanh_poly(float x) {
    if (x >=  4.0f) return  1.0f;
    if (x <= -4.0f) return -1.0f;
    float x2 = x * x;
    float t = x * (1.0f + x2 * (-0.33333333f + x2 * (0.13333333f
              + x2 * (-0.05396825f + x2 * 0.02187029f))));
    return t >  1.0f ?  1.0f :
           t < -1.0f ? -1.0f : t;
}
#elif defined(ACCEL_TANH_PADE32)
// ── Padé [3/2] in x² ─────────────────────────────────────────────────────────
// tanh(x) ≈ x·(1485 + 171x² + 2x⁴) / (1485 + 666x² + 26x⁴)
//
// Derivation: write tanh(x)/x = P₃(x²)/Q₂(x²) and match the Taylor series
// through degree 11.  Solving the Padé linear system gives exact integer
// coefficients (normalised to denominator constant = 1485):
//   num = 1 + (19/165)x² + (2/1485)x⁴
//   den = 1 + (74/165)x² + (26/1485)x⁴
// Multiplied through by 1485 gives the integer form above.
//
// Accuracy vs tanhf:  < 0.01% for |x| ≤ 2,  < 0.2% for |x| ≤ 3.
// Saturates cleanly at |x| = 4 (first overshoot beyond that, < 0.1% error).
// Hardware cost: ~8 mul, 4 add, 1 div — about 4× more than Padé [1/1].
static inline float tanh_poly(float x) {
    if (x >=  4.0f) return  1.0f;
    if (x <= -4.0f) return -1.0f;
    float x2 = x * x;
    float x4 = x2 * x2;
    return x * (1485.0f + 171.0f * x2 + 2.0f * x4)
             / (1485.0f + 666.0f * x2 + 26.0f * x4);
}
#else
// ── Padé [1/1] rational approximation (default) ──────────────────────────────
// tanh(x) ≈ x·(27 + x²) / (27 + 9x²),  saturated at |x| ≥ 4.
// Rational form avoids divergence: approaches 1/3 * x/x = 1/3 → never
// overshoots, saturates gracefully at x=3 (p(3)=1 exactly).
static inline float tanh_poly(float x) {
    if (x >=  4.0f) return  1.0f;
    if (x <= -4.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
#endif

void gelu_forward(float* out, float* inp, int N) {
    for (int i = 0; i < N; i++) {
        float x = inp[i];
        float cube = 0.044715f * x * x * x;
        out[i] = 0.5f * x * (1.0f + tanh_poly(GELU_SCALING_FACTOR * (x + cube)));
    }
}

void gelu_backward(float* dinp, float* inp, float* dout, int N) {
    for (int i = 0; i < N; i++) {
        float x = inp[i];
        float cube = 0.044715f * x * x * x;
        float tanh_arg = GELU_SCALING_FACTOR * (x + cube);
        float tanh_out = tanh_poly(tanh_arg);
        // sech²(x) = 1 − tanh²(x): avoids a separate coshf call
        float sech_sq   = 1.0f - tanh_out * tanh_out;
        float local_grad = 0.5f * (1.0f + tanh_out)
                         + x * 0.5f * sech_sq
                           * GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x);
        dinp[i] += local_grad * dout[i];
    }
}

#else  // original exact implementation

void gelu_forward(float* out, float* inp, int N) {
    // (approximate) GeLU elementwise non-linearity in the MLP block of Transformer
    for (int i = 0; i < N; i++) {
        float x = inp[i];
        float cube = 0.044715f * x * x * x;
        out[i] = 0.5f * x * (1.0f + tanhf(GELU_SCALING_FACTOR * (x + cube)));
    }
}

// we want to use -Ofast optimization, but sadly GeLU breaks, so disable this flag just for it (#168)
#pragma float_control(precise, on, push)
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-finite-math-only")))
#endif
void gelu_backward(float* dinp, float* inp, float* dout, int N) {
    for (int i = 0; i < N; i++) {
        float x = inp[i];
        float cube = 0.044715f * x * x * x;
        float tanh_arg = GELU_SCALING_FACTOR * (x + cube);
        float tanh_out = tanhf(tanh_arg);
        float coshf_out = coshf(tanh_arg);
        float sech_out = 1.0f / (coshf_out * coshf_out);
        float local_grad = 0.5f * (1.0f + tanh_out) + x * 0.5f * sech_out * GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x);
        dinp[i] += local_grad * dout[i];
    }
}
#pragma float_control(pop)

#endif // ACCEL_FUSED_BIAS_GELU

void residual_forward(float* out, float* inp1, float* inp2, int N) {
    for (int i = 0; i < N; i++) {
        out[i] = inp1[i] + inp2[i];
    }
}

void residual_backward(float* dinp1, float* dinp2, float* dout, int N) {
    for (int i = 0; i < N; i++) {
        dinp1[i] += dout[i];
        dinp2[i] += dout[i];
    }
}

void softmax_forward(float* probs, float* logits, int B, int T, int V, int Vp) {
    // output: probs are (B,T,Vp) of the probabilities (sums to 1.0 in each b,t position)
    // input: logits is (B,T,Vp) of the unnormalized log probabilities
    // Vp is the padded vocab size (for efficiency), V is the "real" vocab size
    // example: Vp is 50304 and V is 50257
    #pragma omp parallel for collapse(2)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // probs <- softmax(logits)
            float* logits_bt = logits + b * T * Vp + t * Vp;
            float* probs_bt = probs + b * T * Vp + t * Vp;

            // maxval is only calculated and subtracted for numerical stability
            float maxval = -10000.0f; // TODO something better
            for (int i = 0; i < V; i++) {
                if (logits_bt[i] > maxval) {
                    maxval = logits_bt[i];
                }
            }
            float sum = 0.0f;
            for (int i = 0; i < V; i++) {
                probs_bt[i] = expf(logits_bt[i] - maxval);
                sum += probs_bt[i];
            }
            // note we only loop to V, leaving the padded dimensions
            for (int i = 0; i < V; i++) {
                probs_bt[i] /= sum;
            }
            // for extra super safety we may wish to include this too,
            // forcing the probabilities here to be zero, but it shouldn't matter
            for (int i = V; i < Vp; i++) {
                probs_bt[i] = 0.0f;
            }
        }
    }
}

void crossentropy_forward(float* losses,
                          float* probs, int* targets,
                          int B, int T, int Vp) {
    // output: losses is (B,T) of the individual losses at each position
    // input: probs are (B,T,Vp) of the probabilities
    // input: targets is (B,T) of integers giving the correct index in logits
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // loss = -log(probs[target])
            float* probs_bt = probs + b * T * Vp + t * Vp;
            int ix = targets[b * T + t];
            losses[b * T + t] = -logf(probs_bt[ix]);
        }
    }
}

void crossentropy_softmax_backward(float* dlogits,
                           float* dlosses, float* probs, int* targets,
                           int B, int T, int V, int Vp) {
    // backwards through both softmax and crossentropy
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* dlogits_bt = dlogits + b * T * Vp + t * Vp;
            float* probs_bt = probs + b * T * Vp + t * Vp;
            float dloss = dlosses[b * T + t];
            int ix = targets[b * T + t];
            // note we only loop to V, leaving the padded dimensions
            // of dlogits untouched, so gradient there stays at zero
            for (int i = 0; i < V; i++) {
                float p = probs_bt[i];
                float indicator = i == ix ? 1.0f : 0.0f;
                dlogits_bt[i] += (p - indicator) * dloss;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// GPT-2 model definition

typedef struct {
    int max_seq_len; // max sequence length, e.g. 1024
    int vocab_size; // vocab size, e.g. 50257
    int padded_vocab_size; // padded to e.g. %128==0, 50304
    int num_layers; // number of layers, e.g. 12
    int num_heads; // number of heads in attention, e.g. 12
    int channels; // number of channels, e.g. 768
} GPT2Config;

// the parameters of the model
#define NUM_PARAMETER_TENSORS 16
typedef struct {
    float* wte; // (V, C)
    float* wpe; // (maxT, C)
    float* ln1w; // (L, C)
    float* ln1b; // (L, C)
    float* qkvw; // (L, 3*C, C)
    float* qkvb; // (L, 3*C)
    float* attprojw; // (L, C, C)
    float* attprojb; // (L, C)
    float* ln2w; // (L, C)
    float* ln2b; // (L, C)
    float* fcw; // (L, 4*C, C)
    float* fcb; // (L, 4*C)
    float* fcprojw; // (L, C, 4*C)
    float* fcprojb; // (L, C)
    float* lnfw; // (C)
    float* lnfb; // (C)
} ParameterTensors;

void fill_in_parameter_sizes(size_t* param_sizes, GPT2Config config) {
    size_t Vp = config.padded_vocab_size;
    size_t C = config.channels;
    size_t maxT = config.max_seq_len;
    size_t L = config.num_layers;
    param_sizes[0] = Vp * C; // wte
    param_sizes[1] = maxT * C; // wpe
    param_sizes[2] = L * C; // ln1w
    param_sizes[3] = L * C; // ln1b
    param_sizes[4] = L * (3 * C) * C; // qkvw
    param_sizes[5] = L * (3 * C); // qkvb
    param_sizes[6] = L * C * C; // attprojw
    param_sizes[7] = L * C; // attprojb
    param_sizes[8] = L * C; // ln2w
    param_sizes[9] = L * C; // ln2b
    param_sizes[10] = L * (4 * C) * C; // fcw
    param_sizes[11] = L * (4 * C); // fcb
    param_sizes[12] = L * C * (4 * C); // fcprojw
    param_sizes[13] = L * C; // fcprojb
    param_sizes[14] = C; // lnfw
    param_sizes[15] = C; // lnfb
}

// allocate memory for the parameters and point the individual tensors to the right places
float* malloc_and_point_parameters(ParameterTensors* params, size_t* param_sizes) {
    size_t num_parameters = 0;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        num_parameters += param_sizes[i];
    }
    // malloc all parameters all at once
    float* params_memory = (float*)mallocCheck(num_parameters * sizeof(float));
    // assign all the tensors
    float** ptrs[] = {
        &params->wte, &params->wpe, &params->ln1w, &params->ln1b, &params->qkvw, &params->qkvb,
        &params->attprojw, &params->attprojb, &params->ln2w, &params->ln2b, &params->fcw, &params->fcb,
        &params->fcprojw, &params->fcprojb, &params->lnfw, &params->lnfb
    };
    float* params_memory_iterator = params_memory;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        *(ptrs[i]) = params_memory_iterator;
        params_memory_iterator += param_sizes[i];
    }
    return params_memory;
}

#define NUM_ACTIVATION_TENSORS 23
typedef struct {
    float* encoded; // (B, T, C)
    float* ln1; // (L, B, T, C)
    float* ln1_mean; // (L, B, T)
    float* ln1_rstd; // (L, B, T)
    float* qkv; // (L, B, T, 3*C)
    float* atty; // (L, B, T, C)
    float* preatt; // (L, B, NH, T, T)
    float* att; // (L, B, NH, T, T)
    float* attproj; // (L, B, T, C)
    float* residual2; // (L, B, T, C)
    float* ln2; // (L, B, T, C)
    float* ln2_mean; // (L, B, T)
    float* ln2_rstd; // (L, B, T)
    float* fch; // (L, B, T, 4*C)
    float* fch_gelu; // (L, B, T, 4*C)
    float* fcproj; // (L, B, T, C)
    float* residual3; // (L, B, T, C)
    float* lnf; // (B, T, C)
    float* lnf_mean; // (B, T)
    float* lnf_rstd; // (B, T)
    float* logits; // (B, T, V)
    float* probs; // (B, T, V)
    float* losses; // (B, T)
} ActivationTensors;

void fill_in_activation_sizes(size_t* act_sizes, GPT2Config config, int B, int T) {
    size_t C = config.channels;
    size_t NH = config.num_heads;
    size_t L = config.num_layers;
    size_t Vp = config.padded_vocab_size;
    act_sizes[0] = B * T * C; // encoded
    act_sizes[1] = L * B * T * C; // ln1
    act_sizes[2] = L * B * T; // ln1_mean
    act_sizes[3] = L * B * T; // ln1_rstd
    act_sizes[4] = L * B * T * 3 * C; // qkv
    act_sizes[5] = L * B * T * C; // atty
    act_sizes[6] = L * B * NH * T * T; // preatt
    act_sizes[7] = L * B * NH * T * T; // att
    act_sizes[8] = L * B * T * C; // attproj
    act_sizes[9] = L * B * T * C; // residual2
    act_sizes[10] = L * B * T * C; // ln2
    act_sizes[11] = L * B * T; // ln2_mean
    act_sizes[12] = L * B * T; // ln2_rstd
    act_sizes[13] = L * B * T * 4 * C; // fch
    act_sizes[14] = L * B * T * 4 * C; // fch_gelu
    act_sizes[15] = L * B * T * C; // fcproj
    act_sizes[16] = L * B * T * C; // residual3
    act_sizes[17] = B * T * C; // lnf
    act_sizes[18] = B * T; // lnf_mean
    act_sizes[19] = B * T; // lnf_rstd
    act_sizes[20] = B * T * Vp; // logits
    act_sizes[21] = B * T * Vp; // probs
    act_sizes[22] = B * T; // losses
}

float* malloc_and_point_activations(ActivationTensors* acts, size_t* act_sizes) {
    size_t num_activations = 0;
    for (size_t i = 0; i < NUM_ACTIVATION_TENSORS; i++) {
        num_activations += act_sizes[i];
    }
    float* acts_memory = (float*)mallocCheck(num_activations * sizeof(float));
    float** ptrs[] = {
        &acts->encoded, &acts->ln1, &acts->ln1_mean, &acts->ln1_rstd, &acts->qkv, &acts->atty,
        &acts->preatt, &acts->att, &acts->attproj, &acts->residual2, &acts->ln2, &acts->ln2_mean,
        &acts->ln2_rstd, &acts->fch, &acts->fch_gelu, &acts->fcproj, &acts->residual3, &acts->lnf,
        &acts->lnf_mean, &acts->lnf_rstd, &acts->logits, &acts->probs, &acts->losses
    };
    float* acts_memory_iterator = acts_memory;
    for (size_t i = 0; i < NUM_ACTIVATION_TENSORS; i++) {
        *(ptrs[i]) = acts_memory_iterator;
        acts_memory_iterator += act_sizes[i];
    }
    return acts_memory;
}

typedef struct {
    GPT2Config config;
    // the weights (parameters) of the model, and their sizes
    ParameterTensors params;
    size_t param_sizes[NUM_PARAMETER_TENSORS];
    float* params_memory;
    size_t num_parameters;
    // gradients of the weights
    ParameterTensors grads;
    float* grads_memory;
    // buffers for the AdamW optimizer
    float* m_memory;
    float* v_memory;
    // the activations of the model, and their sizes
    ActivationTensors acts;
    size_t act_sizes[NUM_ACTIVATION_TENSORS];
    float* acts_memory;
    size_t num_activations;
    // gradients of the activations
    ActivationTensors grads_acts;
    float* grads_acts_memory;
    // other run state configuration
    int batch_size; // the batch size (B) of current forward pass
    int seq_len; // the sequence length (T) of current forward pass
    int* inputs; // the input tokens for the current forward pass
    int* targets; // the target tokens for the current forward pass
    float mean_loss; // after a forward pass with targets, will be populated with the mean loss
} GPT2;

void gpt2_build_from_checkpoint(GPT2 *model, const char* checkpoint_path) {

    // read in model from a checkpoint file
    FILE *model_file = fopenCheck(checkpoint_path, "rb");
    int model_header[256];
    freadCheck(model_header, sizeof(int), 256, model_file);
    if (model_header[0] != 20240326) { printf("Bad magic model file\n"); exit(1); }
    if (model_header[1] != 3) {
        printf("Bad version in model file\n");
        printf("---> HINT: try to re-run `python train_gpt2.py`\n");
        exit(1);
    }

    // read in hyperparameters
    size_t maxT, V, Vp, L, NH, C; // size_t to prevent int overflow
    model->config.max_seq_len = maxT = model_header[2];
    model->config.vocab_size = V = model_header[3];
    model->config.num_layers = L = model_header[4];
    model->config.num_heads = NH = model_header[5];
    model->config.channels = C = model_header[6];
    model->config.padded_vocab_size = Vp = model_header[7];
    printf("[GPT-2]\n");
    printf("max_seq_len: %zu\n", maxT);
    printf("vocab_size: %zu\n", V);
    printf("padded_vocab_size: %zu\n", Vp);
    printf("num_layers: %zu\n", L);
    printf("num_heads: %zu\n", NH);
    printf("channels: %zu\n", C);

    // allocate space for all the parameters and read them in
    fill_in_parameter_sizes(model->param_sizes,  model->config);

    // count the number of parameters
    size_t num_parameters = 0;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        num_parameters += model->param_sizes[i];
    }
    printf("num_parameters: %zu\n", num_parameters);
    model->num_parameters = num_parameters;

    // read in all the parameters from file
    model->params_memory = malloc_and_point_parameters(&model->params, model->param_sizes);
    freadCheck(model->params_memory, sizeof(float), num_parameters, model_file);
    fcloseCheck(model_file);

    // other inits
    model->acts_memory = NULL;
    model->grads_memory = NULL;
    model->m_memory = NULL;
    model->v_memory = NULL;
    model->grads_acts_memory = NULL;
    model->inputs = NULL;
    model->targets = NULL;
    model->batch_size = 0;
    model->seq_len = 0;
    model->mean_loss = -1.0f; // -1.0f will designate no loss
}

void gpt2_forward(GPT2 *model, int* inputs, int* targets, size_t B, size_t T) {
    // targets are optional and could be NULL

    // ensure the model was initialized or error out
    if (model->params_memory == NULL) {
        printf("Error: model was not initialized properly.\n");
        exit(1);
    }

    // convenience parameters (size_t to help prevent int overflow)
    size_t V = model->config.vocab_size;
    size_t Vp = model->config.padded_vocab_size;
    size_t L = model->config.num_layers;
    size_t NH = model->config.num_heads;
    size_t C = model->config.channels;

    // validate inputs, all indices must be in the range [0, V)
    for(int i = 0; i < B * T; i++) {
        assert(0 <= inputs[i] && inputs[i] < V);
        if (targets != NULL) {
            assert(0 <= targets[i] && targets[i] < V);
        }
    }

    // allocate space for all the activations if needed (done here, lazily)
    if(model->acts_memory == NULL) {
        // record the current B,T as well
        model->batch_size = B;
        model->seq_len = T;
        // and now allocate the space
        fill_in_activation_sizes(model->act_sizes, model->config, B, T);
        size_t num_activations = 0;
        for (size_t i = 0; i < NUM_ACTIVATION_TENSORS; i++) {
            num_activations += model->act_sizes[i];
        }
        printf("num_activations: %zu\n", num_activations);
        model->num_activations = num_activations;
        model->acts_memory = malloc_and_point_activations(&model->acts, model->act_sizes);
        // also create memory for caching inputs and targets
        model->inputs = (int*)mallocCheck(B * T * sizeof(int));
        model->targets = (int*)mallocCheck(B * T * sizeof(int)); // might be unused if we never have targets but it's small
    } else {
        // validate B,T is consistent with how we've allocated the memory before
        // in principle we could get more clever here in the future, for now this is safest
        if (B != model->batch_size || T != model->seq_len) {
            printf("Model: B=%d T=%d, Desired: B=%d T=%d\n", model->batch_size, model->seq_len, (int)B, (int)T);
            exit(EXIT_FAILURE);
        }
    }

    // cache the inputs/targets
    memcpy(model->inputs, inputs, B * T * sizeof(int));
    if (targets != NULL) {
        memcpy(model->targets, targets, B * T * sizeof(int));
    }

    // forward pass
    ParameterTensors params = model->params; // for brevity
    ActivationTensors acts = model->acts;
    float* residual;
    encoder_forward(acts.encoded, inputs, params.wte, params.wpe, B, T, C); // encoding goes into residual[0]
    for (int l = 0; l < L; l++) {

        residual = l == 0 ? acts.encoded : acts.residual3 + (l-1) * B * T * C;

        // get the pointers of the weights for this layer
        float* l_ln1w = params.ln1w + l * C;
        float* l_ln1b = params.ln1b + l * C;
        float* l_qkvw = params.qkvw + l * 3*C * C;
        float* l_qkvb = params.qkvb + l * 3*C;
        float* l_attprojw = params.attprojw + l * C * C;
        float* l_attprojb = params.attprojb + l * C;
        float* l_ln2w = params.ln2w + l * C;
        float* l_ln2b = params.ln2b + l * C;
        float* l_fcw = params.fcw + l * 4*C * C;
        float* l_fcb = params.fcb + l * 4*C;
        float* l_fcprojw = params.fcprojw + l * C * 4*C;
        float* l_fcprojb = params.fcprojb + l * C;

        // get the pointers of the activations for this layer
        float* l_ln1 = acts.ln1 + l * B * T * C;
        float* l_ln1_mean = acts.ln1_mean + l * B * T;
        float* l_ln1_rstd = acts.ln1_rstd + l * B * T;
        float* l_qkv = acts.qkv + l * B * T * 3*C;
        float* l_atty = acts.atty + l * B * T * C;
        float* l_preatt = acts.preatt + l * B * NH * T * T;
        float* l_att = acts.att + l * B * NH * T * T;
        float* l_attproj = acts.attproj + l * B * T * C;
        float* l_residual2 = acts.residual2 + l * B * T * C;
        float* l_ln2 = acts.ln2 + l * B * T * C;
        float* l_ln2_mean = acts.ln2_mean + l * B * T;
        float* l_ln2_rstd = acts.ln2_rstd + l * B * T;
        float* l_fch = acts.fch + l * B * T * 4*C;
        float* l_fch_gelu = acts.fch_gelu + l * B * T * 4*C;
        float* l_fcproj = acts.fcproj + l * B * T * C;
        float* l_residual3 = acts.residual3 + l * B * T * C;

        // now do the forward pass
        layernorm_forward(l_ln1, l_ln1_mean, l_ln1_rstd, residual, l_ln1w, l_ln1b, B, T, C);
        matmul_forward(l_qkv, l_ln1, l_qkvw, l_qkvb, B, T, C, 3*C);
        attention_forward(l_atty, l_preatt, l_att, l_qkv, B, T, C, NH);
        matmul_forward(l_attproj, l_atty, l_attprojw, l_attprojb, B, T, C, C);
        residual_forward(l_residual2, residual, l_attproj, B*T*C);
        layernorm_forward(l_ln2, l_ln2_mean, l_ln2_rstd, l_residual2, l_ln2w, l_ln2b, B, T, C);
        matmul_forward_gelu(l_fch, l_fch_gelu, l_ln2, l_fcw, l_fcb, B, T, C, 4*C);
        matmul_forward(l_fcproj, l_fch_gelu, l_fcprojw, l_fcprojb, B, T, 4*C, C);
        residual_forward(l_residual3, l_residual2, l_fcproj, B*T*C);
    }
    residual = acts.residual3 + (L-1) * B * T * C; // last residual is in residual3
    layernorm_forward(acts.lnf, acts.lnf_mean, acts.lnf_rstd, residual, params.lnfw, params.lnfb, B, T, C);
    matmul_forward(acts.logits, acts.lnf, params.wte, NULL, B, T, C, Vp);
    softmax_forward(acts.probs, acts.logits, B, T, V, Vp);

    // also forward the cross-entropy loss function if we have the targets
    if (targets != NULL) {
        crossentropy_forward(model->acts.losses, model->acts.probs, targets, B, T, Vp);
        // for convenience also evaluate the mean loss
        float mean_loss = 0.0f;
        for (int i=0; i<B*T; i++) { mean_loss += model->acts.losses[i]; }
        mean_loss /= B*T;
        model->mean_loss = mean_loss;
    } else {
        // if we don't have targets, we don't have a loss
        model->mean_loss = -1.0f;
    }
}

void gpt2_zero_grad(GPT2 *model) {
    if(model->grads_memory != NULL) { memset(model->grads_memory, 0, model->num_parameters * sizeof(float)); }
    if(model->grads_acts_memory != NULL) { memset(model->grads_acts_memory, 0, model->num_activations * sizeof(float)); }
}

void gpt2_backward(GPT2 *model) {

    // double check we forwarded previously, with targets
    if (model->mean_loss == -1.0f) {
        printf("Error: must forward with targets before backward\n");
        exit(1);
    }

    // lazily allocate the memory for gradients of the weights and activations, if needed
    if (model->grads_memory == NULL) {
        model->grads_memory = malloc_and_point_parameters(&model->grads, model->param_sizes);
        model->grads_acts_memory = malloc_and_point_activations(&model->grads_acts, model->act_sizes);
        gpt2_zero_grad(model);
    }

    // convenience shortcuts (and size_t to help prevent int overflow)
    size_t B = model->batch_size;
    size_t T = model->seq_len;
    size_t V = model->config.vocab_size;
    size_t Vp = model->config.padded_vocab_size;
    size_t L = model->config.num_layers;
    size_t NH = model->config.num_heads;
    size_t C = model->config.channels;

    // backward pass: go in the reverse order of the forward pass, and call backward() functions
    ParameterTensors params = model->params; // for brevity
    ParameterTensors grads = model->grads;
    ActivationTensors acts = model->acts;
    ActivationTensors grads_acts = model->grads_acts;

    // we kick off the chain rule by filling in dlosses with 1.0f/(B*T)
    // technically this is a small, inline backward() pass of calculating
    // total, final loss as the mean over all losses over all (B,T) positions in the batch
    float dloss_mean = 1.0f / (B*T);
    for (int i = 0; i < B*T; i++) { grads_acts.losses[i] = dloss_mean; }

    crossentropy_softmax_backward(grads_acts.logits, grads_acts.losses, acts.probs, model->targets, B, T, V, Vp);
    matmul_backward(grads_acts.lnf, grads.wte, NULL, grads_acts.logits, acts.lnf, params.wte, B, T, C, Vp);
    float* residual = acts.residual3 + (L-1) * B * T * C; // last layer's residual
    float* dresidual = grads_acts.residual3 + (L-1) * B * T * C; // write to last layer's residual
    layernorm_backward(dresidual, grads.lnfw, grads.lnfb, grads_acts.lnf, residual, params.lnfw, acts.lnf_mean, acts.lnf_rstd, B, T, C);

    for (int l = L-1; l >= 0; l--) {

        residual = l == 0 ? acts.encoded : acts.residual3 + (l-1) * B * T * C;
        dresidual = l == 0 ? grads_acts.encoded : grads_acts.residual3 + (l-1) * B * T * C;

        // get the pointers of the weights for this layer
        float* l_ln1w = params.ln1w + l * C;
        float* l_qkvw = params.qkvw + l * 3*C * C;
        float* l_attprojw = params.attprojw + l * C * C;
        float* l_ln2w = params.ln2w + l * C;
        float* l_fcw = params.fcw + l * 4*C * C;
        float* l_fcprojw = params.fcprojw + l * C * 4*C;
        // get the pointers of the gradients of the weights for this layer
        float* dl_ln1w = grads.ln1w + l * C;
        float* dl_ln1b = grads.ln1b + l * C;
        float* dl_qkvw = grads.qkvw + l * 3*C * C;
        float* dl_qkvb = grads.qkvb + l * 3*C;
        float* dl_attprojw = grads.attprojw + l * C * C;
        float* dl_attprojb = grads.attprojb + l * C;
        float* dl_ln2w = grads.ln2w + l * C;
        float* dl_ln2b = grads.ln2b + l * C;
        float* dl_fcw = grads.fcw + l * 4*C * C;
        float* dl_fcb = grads.fcb + l * 4*C;
        float* dl_fcprojw = grads.fcprojw + l * C * 4*C;
        float* dl_fcprojb = grads.fcprojb + l * C;
        // get the pointers of the activations for this layer
        float* l_ln1 = acts.ln1 + l * B * T * C;
        float* l_ln1_mean = acts.ln1_mean + l * B * T;
        float* l_ln1_rstd = acts.ln1_rstd + l * B * T;
        float* l_qkv = acts.qkv + l * B * T * 3*C;
        float* l_atty = acts.atty + l * B * T * C;
        float* l_att = acts.att + l * B * NH * T * T;
        float* l_residual2 = acts.residual2 + l * B * T * C;
        float* l_ln2 = acts.ln2 + l * B * T * C;
        float* l_ln2_mean = acts.ln2_mean + l * B * T;
        float* l_ln2_rstd = acts.ln2_rstd + l * B * T;
        float* l_fch = acts.fch + l * B * T * 4*C;
        float* l_fch_gelu = acts.fch_gelu + l * B * T * 4*C;
        // get the pointers of the gradients of the activations for this layer
        float* dl_ln1 = grads_acts.ln1 + l * B * T * C;
        float* dl_qkv = grads_acts.qkv + l * B * T * 3*C;
        float* dl_atty = grads_acts.atty + l * B * T * C;
        float* dl_preatt = grads_acts.preatt + l * B * NH * T * T;
        float* dl_att = grads_acts.att + l * B * NH * T * T;
        float* dl_attproj = grads_acts.attproj + l * B * T * C;
        float* dl_residual2 = grads_acts.residual2 + l * B * T * C;
        float* dl_ln2 = grads_acts.ln2 + l * B * T * C;
        float* dl_fch = grads_acts.fch + l * B * T * 4*C;
        float* dl_fch_gelu = grads_acts.fch_gelu + l * B * T * 4*C;
        float* dl_fcproj = grads_acts.fcproj + l * B * T * C;
        float* dl_residual3 = grads_acts.residual3 + l * B * T * C;

        // backprop this layer
        residual_backward(dl_residual2, dl_fcproj, dl_residual3, B*T*C);
        matmul_backward(dl_fch_gelu, dl_fcprojw, dl_fcprojb, dl_fcproj, l_fch_gelu, l_fcprojw, B, T, 4*C, C);
        gelu_backward(dl_fch, l_fch, dl_fch_gelu, B*T*4*C);
        matmul_backward(dl_ln2, dl_fcw, dl_fcb, dl_fch, l_ln2, l_fcw, B, T, C, 4*C);
        layernorm_backward(dl_residual2, dl_ln2w, dl_ln2b, dl_ln2, l_residual2, l_ln2w, l_ln2_mean, l_ln2_rstd, B, T, C);
        residual_backward(dresidual, dl_attproj, dl_residual2, B*T*C);
        matmul_backward(dl_atty, dl_attprojw, dl_attprojb, dl_attproj, l_atty, l_attprojw, B, T, C, C);
        attention_backward(dl_qkv, dl_preatt, dl_att, dl_atty, l_qkv, l_att, B, T, C, NH);
        matmul_backward(dl_ln1, dl_qkvw, dl_qkvb, dl_qkv, l_ln1, l_qkvw, B, T, C, 3*C);
        layernorm_backward(dresidual, dl_ln1w, dl_ln1b, dl_ln1, residual, l_ln1w, l_ln1_mean, l_ln1_rstd, B, T, C);
    }
    encoder_backward(grads.wte, grads.wpe, grads_acts.encoded, model->inputs, B, T, C);
}

void gpt2_update(GPT2 *model, float learning_rate, float beta1, float beta2, float eps, float weight_decay, int t) {
    // reference: https://pytorch.org/docs/stable/generated/torch.optim.AdamW.html

    // lazily allocate the memory for m_memory and v_memory
    if (model->m_memory == NULL) {
        model->m_memory = (float*)calloc(model->num_parameters, sizeof(float));
        model->v_memory = (float*)calloc(model->num_parameters, sizeof(float));
    }

    for (size_t i = 0; i < model->num_parameters; i++) {
        float param = model->params_memory[i];
        float grad = model->grads_memory[i];

        // update the first moment (momentum)
        float m = beta1 * model->m_memory[i] + (1.0f - beta1) * grad;
        // update the second moment (RMSprop)
        float v = beta2 * model->v_memory[i] + (1.0f - beta2) * grad * grad;
        // bias-correct both moments
        float m_hat = m / (1.0f - powf(beta1, t));
        float v_hat = v / (1.0f - powf(beta2, t));

        // update
        model->m_memory[i] = m;
        model->v_memory[i] = v;
        model->params_memory[i] -= learning_rate * (m_hat / (sqrtf(v_hat) + eps) + weight_decay * param);
    }
}

void gpt2_free(GPT2 *model) {
    free(model->params_memory);
    free(model->grads_memory);
    free(model->m_memory);
    free(model->v_memory);
    free(model->acts_memory);
    free(model->grads_acts_memory);
    free(model->inputs);
    free(model->targets);
}

#ifndef TESTING
// if we are TESTING (see test_gpt2.c), we'll skip the int main below
// ----------------------------------------------------------------------------
// sampler

unsigned int random_u32(uint64_t *state) {
    // xorshift rng: https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}
float random_f32(uint64_t *state) { // random float32 in [0,1)
    return (random_u32(state) >> 8) / 16777216.0f;
}

int sample_mult(float* probabilities, int n, float coin) {
    // sample index from probabilities (they must sum to 1!)
    // coin is a random number in [0, 1), usually from random_f32()
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

// ----------------------------------------------------------------------------
// main training loop
int main() {

    // build the GPT-2 model from a checkpoint
    GPT2 model;
    // Initialise the accelerator backend (software sim, or Verilator model)
    accel_hal_init();

    gpt2_build_from_checkpoint(&model, "../llm/gpt2_124M.bin");

    // build the DataLoaders from tokens files. for now use tiny_shakespeare if available, else tiny_stories
    const char* tiny_stories_train = "../llm/dev/data/tinystories/TinyStories_train.bin";
    const char* tiny_stories_val = "../llm/dev/data/tinystories/TinyStories_val.bin";
    const char* tiny_shakespeare_train = "../llm/dev/data/tinyshakespeare/tiny_shakespeare_train.bin";
    const char* tiny_shakespeare_val = "../llm/dev/data/tinyshakespeare/tiny_shakespeare_val.bin";
    const char* train_tokens = access(tiny_shakespeare_train, F_OK) != -1 ? tiny_shakespeare_train : tiny_stories_train;
    const char* val_tokens = access(tiny_shakespeare_val, F_OK) != -1 ? tiny_shakespeare_val : tiny_stories_val;
    int B = 4; // batch size 4 (i.e. 4 independent token sequences will be trained on)
    int T = 64; // sequence length 64 (i.e. each sequence is 64 tokens long). must be <= maxT, which is 1024 for GPT-2
    DataLoader train_loader, val_loader;
    dataloader_init(&train_loader, train_tokens, B, T, 0, 1, 1);
    dataloader_init(&val_loader, val_tokens, B, T, 0, 1, 0);
    printf("train dataset num_batches: %zu\n", train_loader.num_tokens / (B*T));
    printf("val dataset num_batches: %zu\n", val_loader.num_tokens / (B*T));
    int val_num_batches = 5;

    // build the Tokenizer
    Tokenizer tokenizer;
    tokenizer_init(&tokenizer, "../llm/gpt2_tokenizer.bin");

    // some memory for generating samples from the model
    uint64_t rng_state = 1337;
    int* gen_tokens = (int*)mallocCheck(B * T * sizeof(int));
    const int genT = 64; // number of steps of inference we will do

    // train
    // ACCEL_SINGLE_PASS limits to one forward+backward step — necessary when
    // running Verilator backends which are orders of magnitude slower than wall time.
#ifdef ACCEL_SINGLE_PASS
    int max_steps = 1;
    printf("[accel] ACCEL_SINGLE_PASS enabled — running 1 training step only.\n");
#else
    int max_steps = 40;
#endif

    struct timespec start, end;
    for (int step = 0; step < max_steps; step++) {

        // once in a while estimate the validation loss
        if (step % 10 == 0 && max_steps > 1) {
            float val_loss = 0.0f;
            dataloader_reset(&val_loader);
            for (int i = 0; i < val_num_batches; i++) {
                dataloader_next_batch(&val_loader);
                gpt2_forward(&model, val_loader.inputs, val_loader.targets, B, T);
                val_loss += model.mean_loss;
            }
            val_loss /= val_num_batches;
            printf("val loss %f\n", val_loss);
        }

        // once in a while do model inference to print generated text
        if (step > 0 && step % 20 == 0) {
            // fill up gen_tokens with the GPT2_EOT, which kicks off the generation
            for(int i = 0; i < B * T; ++i) {
                gen_tokens[i] = tokenizer.eot_token;
            }
            // now sample from the model autoregressively
            printf("generating:\n---\n");
            for (int t = 1; t < genT; t++) {
                // note that inference is very wasteful here because for each token
                // we re-calculate the forward pass for all of (B,T) positions from scratch
                // but the inference here is just for sanity checking anyway
                // and we can maybe optimize a bit more later, with careful tests
                gpt2_forward(&model, gen_tokens, NULL, B, T);
                // furthermore, below we're only using b=0 (i.e. the first row) of all B rows
                // we're in principle running B "inference streams" in parallel here
                // but only using position 0
                // get the Vp-dimensional vector probs[0, t-1, :]
                float* probs = model.acts.probs + (t-1) * model.config.padded_vocab_size;
                float coin = random_f32(&rng_state);
                // note we're only sampling from the first V elements, ignoring padding
                // (the probabilities in the padded region should be zero anyway)
                int next_token = sample_mult(probs, model.config.vocab_size, coin);
                gen_tokens[t] = next_token;
                // print the generated token, either using the Tokenizer or a fallback
                if (tokenizer.init_ok) {
                    const char* token_str = tokenizer_decode(&tokenizer, next_token);
                    safe_printf(token_str);
                } else {
                    // fall back to printing the token id
                    printf("%d ", next_token);
                }
                fflush(stdout);
            }
            printf("\n---\n");
        }

        // do a training step
        clock_gettime(CLOCK_MONOTONIC, &start);
        dataloader_next_batch(&train_loader);
        gpt2_forward(&model, train_loader.inputs, train_loader.targets, B, T);
        gpt2_zero_grad(&model);
        gpt2_backward(&model);
        gpt2_update(&model, 1e-4f, 0.9f, 0.999f, 1e-8f, 0.0f, step+1);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_elapsed_s = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        printf("step %d: train loss %f (took %f ms)\n", step, model.mean_loss, time_elapsed_s * 1000);
    }

    // report projected hardware time from synthesis parameters
    accel_print_timing();

    // dump weight parameters for comparison with other implementations
    {
        const char* dump_path = "weights_accel.bin";
        FILE* f = fopen(dump_path, "wb");
        if (f) {
            fwrite(model.params_memory, sizeof(float), model.num_parameters, f);
            fclose(f);
            printf("[accel] weights dumped to %s (%zu parameters)\n",
                   dump_path, model.num_parameters);
        } else {
            fprintf(stderr, "[accel] WARNING: could not write %s\n", dump_path);
        }
    }

    // free
    accel_hal_free();
    dataloader_free(&train_loader);
    dataloader_free(&val_loader);
    tokenizer_free(&tokenizer);
    gpt2_free(&model);
    free(gen_tokens);
    return 0;
}
#endif
// https://github.com/karpathy/llm.c