# =============================================================================
# genus.tcl — Cadence Genus synthesis script for vec_mac_top (512-wide BF16)
# Target PDK: ASAP7 predictive 7nm (RVT standard cells)
#
# 512-wide vector MAC using custom BF16×BF16 multiplier (bf16_mul.sv) and
# FP32 adder (bf16_fp32_add.sv).  Source files from HDL_Vect_512/.
#
# Expected runtime: ~16 hours (4× the 128-wide run) — hierarchical synthesis
# synthesises bf16_mac_unit once and replicates, so MAC logic scales O(1);
# SRAM connectivity and top-level routing dominate the additional time.
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
set_db init_hdl_search_path     ../../HDL_Vect_512

set_db syn_generic_effort   medium
set_db syn_map_effort       medium
set_db syn_opt_effort       medium

# Hierarchical synthesis — bf16_mac_unit synthesised once, replicated 512×.
set_db auto_ungroup         none

# ── Read libraries ────────────────────────────────────────────────────────────
read_libs [list $lib_tt_simple $lib_tt_invbuf $lib_tt_seq $lib_tt_ao $lib_tt_oa]

# ── Read RTL ──────────────────────────────────────────────────────────────────
read_hdl -sv               ../../HDL_Vect_512/bf16_mul.sv
read_hdl -sv               ../../HDL_Vect_512/bf16_fp32_add.sv
read_hdl -sv               ../../HDL_Vect_512/bf16_mac_unit.sv
read_hdl -sv               ../../HDL_Vect_512/vec_mac_array.sv
read_hdl -sv               ../../HDL_Vect_512/weight_sram.sv
read_hdl -sv               ../../HDL_Vect_512/act_sram.sv
read_hdl -sv               ../../HDL_Vect_512/output_sram.sv
read_hdl -sv               ../../HDL_Vect_512/axi.sv
read_hdl -sv               ../../HDL_Vect_512/controller.sv
read_hdl -sv               ../../HDL_Vect_512/axi_readback.sv
read_hdl -sv               ../../HDL_Vect_512/top.sv

# ── Elaborate ─────────────────────────────────────────────────────────────────
elaborate vec_mac_top
check_design -unresolved

# ── Timing constraints ────────────────────────────────────────────────────────
# Same target as 128-wide BF16 — critical path is one MAC deep regardless
# of array width, so timing target is identical.
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
write_hdl      > outputs/vec_mac_top_netlist.v
write_sdc      > outputs/vec_mac_top.sdc
write_do_lec   > outputs/lec.do
