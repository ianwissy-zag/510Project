# =============================================================================
# innovus.tcl — Cadence Innovus P&R script for vec_mac_top (custom BF16 FPU)
# Target PDK: ASAP7 predictive 7nm RVT
#
# Usage (run after genus.tcl completes):
#   innovus -files innovus.tcl |& tee innovus.log
#
# Prerequisites:
#   outputs/vec_mac_top_netlist.v must exist (produced by genus.tcl)
# =============================================================================

set script_dir    [file dirname [file normalize [info script]]]
set asap7_root    [file normalize $script_dir/../asap7/asap7sc7p5t_28]
set asap7_lef_dir $asap7_root/LEF
set asap7_tef_dir $asap7_root/techlef_misc
set asap7_lib_dir $asap7_root/LIB/NLDM

# ── Initialise design ─────────────────────────────────────────────────────────
# init_mmmc_file is the correct Innovus 21.x variable for timing setup.
# mmmc.tcl defines library sets, delay corners, constraint modes and views.
set init_verilog    [list [file normalize outputs/vec_mac_top_netlist.v]]
set init_top_cell   vec_mac_top
set init_lef_file   [list \
    $asap7_tef_dir/asap7_tech_1x_201209.lef \
    $asap7_lef_dir/asap7sc7p5t_28_R_1x_220121a.lef]
set init_mmmc_file  [file normalize $script_dir/mmmc.tcl]

init_design

# ── Floorplan ─────────────────────────────────────────────────────────────────
floorPlan -r 1.0 0.45 2.0 2.0 2.0 2.0

# ── Power distribution ────────────────────────────────────────────────────────
addRing -nets {VDD VSS} \
    -type core_rings \
    -layer_top    M7 -layer_bottom M7 \
    -layer_left   M6 -layer_right  M6 \
    -width 0.5 -spacing 0.2

addStripe -nets {VDD VSS} \
    -layer M6 -direction vertical \
    -width 0.2 -spacing 0.1 \
    -set_to_set_distance 20

sroute -nets {VDD VSS}

# ── Placement ─────────────────────────────────────────────────────────────────
place_design

# Save checkpoint after placement so CTS/routing can resume without re-placing
file mkdir outputs
save_design outputs/vec_mac_top_placed.enc

# ── Clock tree synthesis ───────────────────────────────────────────────────────
# With delay corners registered above, ccopt_design should now proceed.
ccopt_design

# ── Routing ───────────────────────────────────────────────────────────────────
routeDesign

# ── Signoff reports ───────────────────────────────────────────────────────────
file mkdir reports
report_timing   -nworst 20 -path_type full_clock > reports/timing_final.rpt
report_power                                     > reports/power_final.rpt
report_area                                      > reports/area_final.rpt
report_congestion                                > reports/congestion_final.rpt

# ── Write outputs ─────────────────────────────────────────────────────────────
file mkdir outputs
write_netlist  outputs/vec_mac_top_final.v
write_sdf      outputs/vec_mac_top.sdf
save_design    outputs/vec_mac_top_final.enc
