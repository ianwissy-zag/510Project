# Remaining Design Tasks — M4 → Next Implementation

The following tasks address the pipeline inefficiencies identified in the M4 benchmark
(19.4× kernel speedup, 45.4% PE utilization). Completing all four is expected to raise
kernel speedup to approximately 25× and push effective PE utilization to ~88% at M=256.

---

## Task 1 — Double-buffer the accumulator SRAM

**Current state:** A single accumulator SRAM bank is shared between the systolic array
write path and the AXI readback unit. Readback must therefore wait until the current
N-tile compute completes before draining results to the host, and the next N-tile cannot
start until readback finishes.

**Required change:** Instantiate two accumulator SRAM banks (ping and pong). Add an
`accum_buf_sel` control input to `sys_top` and `controller`. When `accum_buf_sel=0`
the systolic array writes to bank 0 and readback reads from bank 1; when
`accum_buf_sel=1` the roles reverse. The HAL alternates the signal each N-tile.

**Expected gain:** Eliminates the readback stall between consecutive N-tiles entirely,
removing approximately 2×M cycles of dead time per N-tile.

---

## Task 2 — Overlap compute and writeback using the double-buffered accumulator

**Prerequisite:** Task 1.

**Current state:** Even after double-buffering is added to the RTL, the HAL must be
updated to exploit it. Currently `hal_read_results_all` (and its biased/GELU variants)
are called synchronously: they block until all readback beats are received before the
next K-tile streaming call begins.

**Required change:** Split each readback into a non-blocking `hal_readback_start()` and
a deferred `hal_readback_sync()`. Harvest AXI readback beats inside every `tick()` call
so that any subsequent `hal_stream_tile` call automatically drains the readback pipeline
concurrently. Add an `accum_buf_sel` toggle guard (`g_accum_toggled`) to prevent
double-toggling within one N-tile, and a `g_accum_gelu_pending` flag to protect the
biased + GELU two-readback sequence from an interleaved K-tile resetting the guard.
Update `accel_matmul`, `accel_matmul_gelu`, `accel_matmul_backward_dinp`, and
`accel_matmul_dweight` in `train_gpt2.c` to use the async API.

**Expected gain:** Readback of N-tile n fully overlaps with K-tile streaming of N-tile
n+1, eliminating ~2×M cycles of idle time per N-tile on the AXI master port.

---

## Task 3 — Load next weights during the drain phase (drain-overlap LOAD_WT)

**Current state:** After the last activation beat of a K-tile is sent, the systolic array
drains for ROWS cycles before asserting `done`. During this window the weight SRAM and
the systolic array weight-shift chain are both idle. The following K-tile's LOAD_WT
(another ROWS cycles) then runs sequentially after `done`, doubling the per-tile overhead.

**Required change:** Add `ovlp_cnt`, `ovlp_active`, and `wt_preloaded_r` registers to
`controller.sv`. When `preload_rdy` is asserted and `stream_cnt == M_count+1` (start of
drain), begin shifting the next tile's weights from the opposite SRAM bank into the PE
array concurrently with the drain. When `ovlp_cnt` reaches ROWS−1 set `wt_preloaded_r`;
on the next `start` pulse transition directly from IDLE to STREAM, skipping LOAD_WT.
Add `preload_rdy` as a new top-level input. Update the HAL to assert `preload_rdy`
whenever a next-tile weight pointer is available, and to send the next tile's weights
to the SRAM during the LOAD_WT window of the current tile (non-preloaded path) or
immediately before streaming (preloaded path).

**Expected gain:** Saves ROWS=32 cycles per K-tile in steady state, reducing per-tile
overhead from M+67 cycles to M+35 cycles.

---

## Task 4 — Read next-next weights from DRAM concurrently with drain-overlap LOAD_WT

**Prerequisite:** Task 3.

**Current state:** After Task 3, drain-overlap reads K(n+1) from the weight SRAM into
the PE array during K(n)'s drain phase. However, the AXI bus is idle during this window:
no new DRAM→SRAM transfer is initiated, so K(n+2) must be written to the SRAM in the
following tile's LOAD_WT window, which no longer exists on the preloaded path.

**Required change:** Add a `next_next_w_tile` parameter to `hal_stream_tile`. After the
M activation beats are sent and while drain-overlap LOAD_WT is reading K(n+1) from SRAM
bank `~buf_sel` into the PEs, use the AXI slave port to simultaneously write K(n+2) into
SRAM bank `buf_sel` (the bank just vacated by K(n)). The two SRAM banks have independent
read and write ports, so there is no conflict. The 32 write beats fit within the 33-cycle
drain window. Update `train_gpt2.c` to pre-pack `next_next_w_tile` one iteration ahead
in the K-tile loops of all four matmul functions.

**Expected gain:** Keeps the AXI slave port fully utilized every cycle (activation beats
or weight-preload beats), removing the last idle interval in the memory subsystem and
sustaining ~99% AXI bus utilization in steady state.
