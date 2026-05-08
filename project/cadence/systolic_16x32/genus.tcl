# =============================================================================
# genus.tcl — Cadence Genus synthesis script for sys_top (32×32 BF16 systolic)
# Target PDK: ASAP7 predictive 7nm (RVT standard cells)
#
# 32-row × 32-column weight-stationary systolic array with classical staggered
# M-row streaming dataflow, custom BF16×BF16 multiplier, FP32 accumulation.
# Supports forward and backward passes via separate weight SRAMs.
#
# Design: sys_top (HDL_Systolic_16x32/)
#   1024 MACs (32×32)
#   LOAD_WT = 32 cycles, STREAM = M_count + ROWS + 2 cycles per K-tile
#   act_stagger: triangular delay buffer for classical systolic activation flow
#   accum_sram:  M×COLS FP32 accumulator across K-tiles
#
# Expected runtime: comparable to the 128-wide BF16 run (~4-6 hours).
# The bf16_mac_unit hierarchy is synthesised once and replicated across all
# 1024 PEs via hierarchical synthesis.
#
# Usage:
#   genus -f genus.tcl |& tee genus.log
#
# ASAP7 is bundled at cadence/asap7/ relative to the project root.
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
set_db init_hdl_search_path     ../../HDL_Systolic_16x32

set_db syn_generic_effort   medium
set_db syn_map_effort       medium
set_db syn_opt_effort       medium

# Hierarchical synthesis — pe (and its bf16_mac_unit) synthesised once,
# replicated across all 32×32=1024 PE instances.
set_db auto_ungroup         none

# ── Read libraries ────────────────────────────────────────────────────────────
read_libs [list $lib_tt_simple $lib_tt_invbuf $lib_tt_seq $lib_tt_ao $lib_tt_oa]

# ── Read RTL ──────────────────────────────────────────────────────────────────
read_hdl -sv               ../../HDL_Systolic_16x32/bf16_mul.sv
read_hdl -sv               ../../HDL_Systolic_16x32/bf16_fp32_add.sv
read_hdl -sv               ../../HDL_Systolic_16x32/bf16_mac_unit.sv
read_hdl -sv               ../../HDL_Systolic_16x32/pe.sv
read_hdl -sv               ../../HDL_Systolic_16x32/systolic_16x32.sv
read_hdl -sv               ../../HDL_Systolic_16x32/weight_sram.sv
read_hdl -sv               ../../HDL_Systolic_16x32/act_stagger.sv
read_hdl -sv               ../../HDL_Systolic_16x32/accum_sram.sv
read_hdl -sv               ../../HDL_Systolic_16x32/axi.sv
read_hdl -sv               ../../HDL_Systolic_16x32/controller.sv
read_hdl -sv               ../../HDL_Systolic_16x32/axi_readback.sv
read_hdl -sv               ../../HDL_Systolic_16x32/top.sv

# ── Elaborate ─────────────────────────────────────────────────────────────────
elaborate sys_top
check_design -unresolved

# ── Timing constraints ────────────────────────────────────────────────────────
# Same timing target as the BF16 vector designs — critical path is through
# one PE's BF16 MAC unit regardless of array shape.
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
