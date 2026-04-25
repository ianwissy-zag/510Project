# =============================================================================
# innovus.tcl — Cadence Innovus P&R script for vec_mac_top (custom BF16 FPU)
# Target PDK: ASAP7 predictive 7nm RVT
#
# Usage (run after genus.tcl completes):
#   innovus -files innovus.tcl |& tee innovus.log
#
# Prerequisites:
#   outputs/vec_mac_top_netlist.v and outputs/vec_mac_top.sdc must exist
# =============================================================================

set script_dir    [file dirname [file normalize [info script]]]
set asap7_root    [file normalize $script_dir/../asap7/asap7sc7p5t_28]
set asap7_lef_dir $asap7_root/LEF
set asap7_tef_dir $asap7_root/techlef_misc
set asap7_lib_dir $asap7_root/LIB/NLDM

# ── Initialise design via set_db — passes MMMC, LEF, and netlist to
#    init_design in a single call, which is the reliable path for this
#    Innovus version (sourcing mmmc.tcl before init_design loses timing info).
set_db init_mmmc_files          [list [file normalize $script_dir/mmmc.tcl]]
set_db init_lef_file            [list \
    $asap7_tef_dir/asap7_tech_1x_201209.lef \
    $asap7_lef_dir/asap7sc7p5t_28_R_1x_220121a.lef]
set_db init_verilog             [list [file normalize outputs/vec_mac_top_netlist.v]]
set_db init_top_cell            vec_mac_top
set_db init_lib_search_path     $asap7_lib_dir

init_design

set_analysis_view -setup [list av_setup] -hold [list av_hold]

# ── Floorplan ─────────────────────────────────────────────────────────────────
# 775K cells at ASAP7 density — 1000x1000 um at 45% utilisation.
floorPlan -r 1.0 0.45 2.0 2.0 2.0 2.0

# ── Power distribution ────────────────────────────────────────────────────────
addRing -nets {VDD VSS} \
    -type core_rings \
    -layer_top    M7 \
    -layer_bottom M7 \
    -layer_left   M6 \
    -layer_right  M6 \
    -width 0.5 -spacing 0.2

addStripe -nets {VDD VSS} \
    -layer M6 -direction vertical \
    -width 0.2 -spacing 0.1 \
    -set_to_set_distance 20

sroute -nets {VDD VSS}

# ── Placement ─────────────────────────────────────────────────────────────────
place_design
optDesign -preCTS -outDir reports/preCTS

# ── Clock tree synthesis ───────────────────────────────────────────────────────
# Target: 1700ps (~588 MHz) from constraints_pnr.sdc
ccopt_design
optDesign -postCTS       -outDir reports/postCTS
optDesign -postCTS -hold -outDir reports/postCTS_hold

# ── Routing ───────────────────────────────────────────────────────────────────
routeDesign
optDesign -postRoute       -outDir reports/postRoute
optDesign -postRoute -hold -outDir reports/postRoute_hold

# ── Signoff reports ───────────────────────────────────────────────────────────
file mkdir reports
report_timing -nworst 20 -path_type full_clock > reports/timing_final.rpt
report_power                                   > reports/power_final.rpt
report_area                                    > reports/area_final.rpt
report_congestion                              > reports/congestion_final.rpt

# ── Write outputs ─────────────────────────────────────────────────────────────
file mkdir outputs
write_netlist  outputs/vec_mac_top_final.v
write_sdf      outputs/vec_mac_top.sdf
save_design    outputs/vec_mac_top_final.enc
