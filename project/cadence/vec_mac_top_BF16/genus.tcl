# =============================================================================
# genus.tcl — Cadence Genus 2024 synthesis script for vec_mac_top (BF16 custom)
# Target PDK: ASAP7 predictive 7nm (RVT standard cells)
#
# This variant uses the custom BF16×BF16 multiplier (bf16_mul.sv) and FP32
# adder (bf16_fp32_add.sv) instead of the HardFloat core.  Source files are
# from HDL_Vect_BF16/.  The HardFloat bf16_mac_unit_core.v is not used here.
#
# Usage:
#   genus -f genus.tcl |& tee genus.log
#
# ASAP7 is bundled at cadence/asap7/ relative to the project root.
# No environment variables needed.
# =============================================================================

# ── Library configuration ─────────────────────────────────────────────────────
set script_dir   [file dirname [file normalize [info script]]]
set asap7_root   [file normalize $script_dir/../asap7/asap7sc7p5t_28]
set asap7_lib_dir $asap7_root/LIB/NLDM
set asap7_lef_dir $asap7_root/LEF

# TT corner only for synthesis — SS/FF cause cell name collisions when loaded
# together and belong in the Innovus mmmc setup, not here.
# ASAP7 splits cells across five library files by type; all are needed.
set lib_tt_simple $asap7_lib_dir/asap7sc7p5t_SIMPLE_RVT_TT_nldm_211120.lib
set lib_tt_invbuf $asap7_lib_dir/asap7sc7p5t_INVBUF_RVT_TT_nldm_220122.lib
set lib_tt_seq    $asap7_lib_dir/asap7sc7p5t_SEQ_RVT_TT_nldm_220123.lib
set lib_tt_ao     $asap7_lib_dir/asap7sc7p5t_AO_RVT_TT_nldm_211120.lib
set lib_tt_oa     $asap7_lib_dir/asap7sc7p5t_OA_RVT_TT_nldm_211120.lib

# ── Genus global settings ─────────────────────────────────────────────────────
set_db init_lib_search_path     $asap7_lib_dir
set_db init_hdl_search_path     ../../HDL_Vect_BF16

# Medium effort balances runtime vs quality for a first-pass run.
# Increase to high once the flow is validated.
set_db syn_generic_effort   medium
set_db syn_map_effort       medium
set_db syn_opt_effort       medium

# Preserve hierarchy on the MAC array — combined with (* keep_hierarchy *)
# in vec_mac_array.sv this lets Genus synthesize bf16_mac_unit once and
# replicate across all 128 lanes rather than flattening everything.
set_db auto_ungroup         none

# ── Read libraries ────────────────────────────────────────────────────────────
read_libs [list $lib_tt_simple $lib_tt_invbuf $lib_tt_seq $lib_tt_ao $lib_tt_oa]

# ── Read RTL ──────────────────────────────────────────────────────────────────
# Custom BF16 MAC: bf16_mul.sv + bf16_fp32_add.sv replace bf16_mac_unit_core.v.
# All sources are SystemVerilog 2012.
read_hdl -sv               ../../HDL_Vect_BF16/bf16_mul.sv
read_hdl -sv               ../../HDL_Vect_BF16/bf16_fp32_add.sv
read_hdl -sv               ../../HDL_Vect_BF16/bf16_mac_unit.sv
read_hdl -sv               ../../HDL_Vect_BF16/vec_mac_array.sv
read_hdl -sv               ../../HDL_Vect_BF16/weight_sram.sv
read_hdl -sv               ../../HDL_Vect_BF16/act_sram.sv
read_hdl -sv               ../../HDL_Vect_BF16/output_sram.sv
read_hdl -sv               ../../HDL_Vect_BF16/axi.sv
read_hdl -sv               ../../HDL_Vect_BF16/controller.sv
read_hdl -sv               ../../HDL_Vect_BF16/axi_readback.sv
read_hdl -sv               ../../HDL_Vect_BF16/top.sv

# ── Elaborate ─────────────────────────────────────────────────────────────────
elaborate vec_mac_top
check_design -unresolved

# Hierarchy is preserved globally via auto_ungroup none above.

# ── Timing constraints ────────────────────────────────────────────────────────
# 5ns / 200MHz target — conservative for ASAP7 FP32 MAC depth.
# See constraints.sdc for notes on tightening toward 2ns.
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

puts "INFO: Check reports/timing.rpt — if worst slack > 1ns, tighten CLOCK_PERIOD in constraints.sdc"

# ── Write outputs ─────────────────────────────────────────────────────────────
file mkdir outputs
write_hdl      > outputs/vec_mac_top_netlist.v
write_sdc      > outputs/vec_mac_top.sdc
write_do_lec   > outputs/lec.do
