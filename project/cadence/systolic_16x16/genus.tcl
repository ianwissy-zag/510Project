# =============================================================================
# genus.tcl — Cadence Genus synthesis script for sys_top (16×16 BF16 systolic
#             with fused bias addition and GELU Padé [3/2] post-processing)
# Target PDK: ASAP7 predictive 7nm (RVT standard cells)
#
# Design: sys_top (HDL_Systolic_16x16/)
#
#   Compute core:
#     256 MACs (16×16 weight-stationary systolic array)
#     LOAD_WT = 16 cycles, STREAM = M_count + ROWS + 2 cycles per K-tile
#     act_stagger: triangular delay buffer for classical staggered M-row dataflow
#     accum_sram:  256 × 16 FP32 accumulator across K-tiles
#
#   Post-processing (fused into readback pipeline):
#     bias_sram:   16 FP32 bias values loaded via AXI (tuser=110, 2 beats)
#     fp32_recip:  FP32 reciprocal via bit-trick + 2 Newton-Raphson iterations
#                  (uses bf16_mul × 4 + bf16_fp32_add × 2)
#     gelu_unit:   GELU(Padé[3/2]), explicitly pipelined in RTL with 8 register
#                  banks (max 2 sequential bf16_mul per stage ≈ 900 ps).
#                  16 instances run in parallel, one per output column.
#
#   Critical path note:
#     The systolic compute path (bf16_mac_unit chain) sets the clock target
#     at 1650 ps.  The GELU datapath is explicitly pipelined in RTL
#     (gelu_unit has 8 register banks, max 2 bf16_mul per stage ≈ 900 ps);
#     no Genus retiming directive is needed.
#
# Expected runtime: 2-4 hours (16 GELU instances, each with 18 bf16_mul
#   + 8 bf16_fp32_add, pipelined across 8 stages).
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
set_db init_hdl_search_path     ../../HDL_Systolic_16x16

set_db syn_generic_effort   medium
set_db syn_map_effort       medium
set_db syn_opt_effort       medium

# Hierarchical synthesis — pe (and its bf16_mac_unit) synthesised once,
# replicated across all 16×16 = 256 PE instances.
# gelu_unit also synthesised once, replicated across 16 column instances.
set_db auto_ungroup         none

# ── Read libraries ────────────────────────────────────────────────────────────
read_libs [list $lib_tt_simple $lib_tt_invbuf $lib_tt_seq $lib_tt_ao $lib_tt_oa]

# ── Read RTL ──────────────────────────────────────────────────────────────────
# BF16 arithmetic primitives (shared with systolic array and GELU)
read_hdl -sv               ../../HDL_Systolic_16x16/bf16_mul.sv
read_hdl -sv               ../../HDL_Systolic_16x16/bf16_fp32_add.sv
read_hdl -sv               ../../HDL_Systolic_16x16/bf16_mac_unit.sv

# Systolic array
read_hdl -sv               ../../HDL_Systolic_16x16/pe.sv
read_hdl -sv               ../../HDL_Systolic_16x16/systolic_16x16.sv

# Memory blocks
read_hdl -sv               ../../HDL_Systolic_16x16/weight_sram.sv
read_hdl -sv               ../../HDL_Systolic_16x16/act_stagger.sv
read_hdl -sv               ../../HDL_Systolic_16x16/accum_sram.sv
read_hdl -sv               ../../HDL_Systolic_16x16/bias_sram.sv

# GELU post-processing (synthesisable, no DPI-C)
read_hdl -sv               ../../HDL_Systolic_16x16/fp32_recip.sv
read_hdl -sv               ../../HDL_Systolic_16x16/gelu_unit.sv

# Interface and control
read_hdl -sv               ../../HDL_Systolic_16x16/axi.sv
read_hdl -sv               ../../HDL_Systolic_16x16/controller.sv
read_hdl -sv               ../../HDL_Systolic_16x16/axi_readback.sv
read_hdl -sv               ../../HDL_Systolic_16x16/top.sv

# ── Elaborate ─────────────────────────────────────────────────────────────────
elaborate sys_top
check_design -unresolved

# ── Timing constraints ────────────────────────────────────────────────────────
# 1650 ps target.  The GELU readback path is explicitly pipelined in RTL
# (gelu_unit has 8 register banks, max 2 bf16_mul per stage ≈ 900 ps).
# No retiming directive needed — Genus treats each stage independently.
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
