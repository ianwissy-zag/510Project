# =============================================================================
# genus.tcl — Cadence Genus synthesis script for sys_top (32×32 BF16 systolic
#             with fused bias addition and GELU Padé [3/2] post-processing)
# Target PDK: ASAP7 predictive 7nm (RVT standard cells)
#
# Design: sys_top (HDL_Systolic_32x32/)
#
#   Compute core:
#     1024 MACs (32×32 weight-stationary systolic array)
#     LOAD_WT = 32 cycles, STREAM = M_count + ROWS + 2 cycles per K-tile
#     act_stagger: triangular delay buffer for classical staggered M-row dataflow
#     accum_sram:  256 × 32 FP32 accumulator across K-tiles
#
#   Post-processing (fused into readback pipeline):
#     bias_sram:   32 FP32 bias values loaded via AXI (tuser=110, 2 beats)
#     fp32_recip:  FP32 reciprocal via bit-trick + 2 Newton-Raphson iterations
#                  (uses bf16_mul × 4 + bf16_fp32_add × 2)
#     gelu_unit:   GELU(Padé[3/2]) = 12 bf16_mul + 6 bf16_fp32_add + fp32_recip
#                  32 instances run in parallel, one per output column
#
#   Critical path note:
#     The systolic compute path (bf16_mac_unit chain) sets the clock target
#     at 1650ps as before.  The GELU datapath is ~15 bf16_mul stages deep
#     (~6ns combinational).  Genus retiming (enabled below) automatically
#     inserts pipeline registers in the readback → GELU → AXI output path
#     to meet the 1650ps target; this adds 3-4 cycles of readback latency
#     but does not affect throughput.
#
# Expected runtime: 6-10 hours (larger design than plain systolic due to
#   32 GELU instances, each with 512 bf16_mul + 288 bf16_fp32_add total).
#
# Usage:
#   genus -f genus.tcl |& tee genus.log
# =============================================================================

# ── Library configuration ─────────────────────────────────────────────────────
set script_dir   [file dirname [file normalize [info script]]]
set asap7_root   [file normalize $script_dir/../asap7/asap7sc7p5t_28]
set asap7_lib_dir $asap7_root/LIB/NLDM
set asap7_lef_dir $asap7_root/LEF

set lib_tt_simple $asap7_lib_dir/asap7sc7p5t_SIMPLE_RVT_TT_nldm_211120.lib
set lib_tt_invbuf $asap7_lib_dir/asap7sc7p5t_INVBUF_RVT_TT_nldm_220122.lib
set lib_tt_seq    $asap7_lib_dir/asap7sc7p5t_SEQ_RVT_TT_nldm_220123.lib
set lib_tt_ao     $asap7_lib_dir/asap7sc7p5t_AO_RVT_TT_nldm_211120.lib
set lib_tt_oa     $asap7_lib_dir/asap7sc7p5t_OA_RVT_TT_nldm_211120.lib

# ── Genus global settings ─────────────────────────────────────────────────────
set_db init_lib_search_path     $asap7_lib_dir
set_db init_hdl_search_path     ../../HDL_Systolic_32x32

# Higher effort needed: GELU retiming requires more optimisation passes.
set_db syn_generic_effort   high
set_db syn_map_effort       high
set_db syn_opt_effort       high

# Hierarchical synthesis — pe (and its bf16_mac_unit) synthesised once,
# replicated across all 32×32 = 1024 PE instances.
# gelu_unit also synthesised once, replicated across 32 column instances.
set_db auto_ungroup         none

# Enable retiming so Genus can pipeline the deeply combinational GELU path.
# Without this the GELU readback path (~6ns) would fail the 1650ps target.
set_db design:sys_top .retime true

# ── Read libraries ────────────────────────────────────────────────────────────
read_libs [list $lib_tt_simple $lib_tt_invbuf $lib_tt_seq $lib_tt_ao $lib_tt_oa]

# ── Read RTL ──────────────────────────────────────────────────────────────────
# BF16 arithmetic primitives (shared with systolic array and GELU)
read_hdl -sv               ../../HDL_Systolic_32x32/bf16_mul.sv
read_hdl -sv               ../../HDL_Systolic_32x32/bf16_fp32_add.sv
read_hdl -sv               ../../HDL_Systolic_32x32/bf16_mac_unit.sv

# Systolic array
read_hdl -sv               ../../HDL_Systolic_32x32/pe.sv
read_hdl -sv               ../../HDL_Systolic_32x32/systolic_16x32.sv

# Memory blocks
read_hdl -sv               ../../HDL_Systolic_32x32/weight_sram.sv
read_hdl -sv               ../../HDL_Systolic_32x32/act_stagger.sv
read_hdl -sv               ../../HDL_Systolic_32x32/accum_sram.sv
read_hdl -sv               ../../HDL_Systolic_32x32/bias_sram.sv

# GELU post-processing (synthesisable, no DPI-C)
read_hdl -sv               ../../HDL_Systolic_32x32/fp32_recip.sv
read_hdl -sv               ../../HDL_Systolic_32x32/gelu_unit.sv

# Interface and control
read_hdl -sv               ../../HDL_Systolic_32x32/axi.sv
read_hdl -sv               ../../HDL_Systolic_32x32/controller.sv
read_hdl -sv               ../../HDL_Systolic_32x32/axi_readback.sv
read_hdl -sv               ../../HDL_Systolic_32x32/top.sv

# ── Elaborate ─────────────────────────────────────────────────────────────────
elaborate sys_top
check_design -unresolved

# ── Timing constraints ────────────────────────────────────────────────────────
# 1650ps target on the systolic compute path (bf16_mac_unit critical path).
# The GELU readback path is handled by retiming above.
read_sdc constraints.sdc

# ── Synthesis ─────────────────────────────────────────────────────────────────
syn_generic
syn_map
syn_opt

# ── Reports ───────────────────────────────────────────────────────────────────
file mkdir reports
report_timing  -nworst 10        > reports/timing.rpt
report_area                      > reports/area.rpt
report_power                     > reports/power.rpt
report_qor                       > reports/qor.rpt
report_cells                     > reports/cells.rpt

# ── Write outputs ─────────────────────────────────────────────────────────────
file mkdir outputs
write_hdl      > outputs/sys_top_netlist.v
write_sdc      > outputs/sys_top.sdc
write_do_lec   > outputs/lec.do
