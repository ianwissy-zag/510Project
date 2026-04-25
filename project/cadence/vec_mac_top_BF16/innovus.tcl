# =============================================================================
# innovus.tcl — Cadence Innovus 2024 P&R script for vec_mac_top
# Target PDK: ASAP7 predictive 7nm RVT
#
# Usage (run after genus.tcl completes):
#   innovus -files innovus.tcl |& tee innovus.log
#
# Prerequisites:
#   outputs/vec_mac_top_netlist.v and outputs/vec_mac_top.sdc must exist
#   (produced by genus.tcl)
# =============================================================================

set script_dir    [file dirname [file normalize [info script]]]
set asap7_root    [file normalize $script_dir/../asap7/asap7sc7p5t_28]
set asap7_lef_dir $asap7_root/LEF
set asap7_tef_dir $asap7_root/techlef_misc

# ── Read design ───────────────────────────────────────────────────────────────
# source mmmc.tcl directly — read_mmmc not available in this Innovus version
source mmmc.tcl

read_physical -lef [list \
    $asap7_tef_dir/asap7_tech_1x_201209.lef \
    $asap7_lef_dir/asap7sc7p5t_28_R_1x_220121a.lef \
]

read_netlist outputs/vec_mac_top_netlist.v -top vec_mac_top

# init_design is implicit in this Innovus version after read_netlist
set_analysis_view -setup [list av_setup] -hold [list av_hold]

# ── Floorplan ─────────────────────────────────────────────────────────────────
# At ASAP7 7nm density the synthesized logic (~1-2M cells) fits in roughly
# 0.5-1 mm².  1000x1000 um at 45% utilization gives comfortable headroom.
# Adjust FP_CORE_UTIL if placement reports congestion.
floorPlan -r 1.0 0.45 2.0 2.0 2.0 2.0

# ── Power distribution ────────────────────────────────────────────────────────
# ASAP7 uses VDD/VSS naming convention.
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

# Pre-CTS timing optimization
optDesign -preCTS -outDir reports/preCTS

# ── Clock tree synthesis ───────────────────────────────────────────────────────
# Targets the 1700ps (~588MHz) clock defined in constraints_pnr.sdc.
# After CTS review reports/postCTS/timing.rpt for slack details.
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

# GDS streamout — requires ASAP7 GDS files and layer map
# Uncomment once you have the PDK GDS:
# streamOut outputs/vec_mac_top.gds \
#     -mapFile  $asap7_lef_dir/../GDS/asap7_gds_layer_map.map \
#     -libName  vec_mac_top \
#     -merge    [list $asap7_lef_dir/../GDS/asap7sc7p5t_28_R.gds]

save_design outputs/vec_mac_top_final.enc
