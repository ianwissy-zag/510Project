# =============================================================================
# innovus.tcl — Cadence Innovus P&R for sys_top (16×16 BF16 systolic
#               with fused bias addition and GELU Padé [3/2])
# Target PDK: ASAP7 predictive 7nm RVT
#
# Usage (run after genus.tcl completes):
#   innovus -no_gui -files innovus.tcl |& tee innovus.log
#
# Prerequisites:
#   outputs/sys_top_netlist.v  (produced by genus.tcl)
#
# Area estimate (ASAP7 ~10M cells/mm²):
#   Systolic array  (256 MACs):            ~57 K µm²
#   GELU/bias units (16 cols × gelu_unit): ~80 K µm²
#   SRAMs + control + AXI:                ~60 K µm²
#   Total:                              ~197 K µm²
#
# Floorplan: 700×700 µm at 40% utilisation (490 K µm² die area).
# Increase utilisation or die size if routing congestion is observed.
# =============================================================================

set script_dir    [file dirname [file normalize [info script]]]
set asap7_root    [file normalize $script_dir/../asap7/asap7sc7p5t_28]
set asap7_lef_dir $asap7_root/LEF
set asap7_tef_dir $asap7_root/techlef_misc
set asap7_lib_dir $asap7_root/LIB/NLDM

# ── Initialise design — legacy EDI-style global variables ─────────────────────
set init_verilog    [list [file normalize outputs/sys_top_netlist.v]]
set init_top_cell   sys_top
set init_lef_file   [list \
    $asap7_tef_dir/asap7_tech_1x_201209.lef \
    $asap7_lef_dir/asap7sc7p5t_28_R_1x_220121a.lef]

# Setup corner: slow-slow
set init_lib        [list \
    $asap7_lib_dir/asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120.lib \
    $asap7_lib_dir/asap7sc7p5t_INVBUF_RVT_SS_nldm_220122.lib \
    $asap7_lib_dir/asap7sc7p5t_SEQ_RVT_SS_nldm_220123.lib \
    $asap7_lib_dir/asap7sc7p5t_AO_RVT_SS_nldm_211120.lib \
    $asap7_lib_dir/asap7sc7p5t_OA_RVT_SS_nldm_211120.lib]

# Hold corner: fast-fast
set init_min_lib    [list \
    $asap7_lib_dir/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib \
    $asap7_lib_dir/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib \
    $asap7_lib_dir/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib \
    $asap7_lib_dir/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib \
    $asap7_lib_dir/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib]

set init_max_lib    $init_lib
set init_sdcfile    [list [file normalize $script_dir/constraints_pnr.sdc]]

init_design

# ── Floorplan ─────────────────────────────────────────────────────────────────
# 700×700 µm die at 40% utilisation.
# The GELU units (~80 K µm²) are the dominant block at this array size;
# 40% leaves headroom for routing and clock-tree buffers.
floorPlan -r 1.0 0.40 2.0 2.0 2.0 2.0   ;# aspect 1.0, 40% util, 2µm margins

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

file mkdir outputs
saveDesign outputs/sys_top_placed.enc

# ── Clock tree synthesis ───────────────────────────────────────────────────────
set_db cts_buffer_cells   {BUFx4_ASAP7_75t_R BUFx6f_ASAP7_75t_R BUFx12f_ASAP7_75t_R}
set_db cts_inverter_cells {INVx1_ASAP7_75t_R INVx2_ASAP7_75t_R INVx4_ASAP7_75t_R}
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
saveNetlist    outputs/sys_top_final.v
writeSDF       outputs/sys_top.sdf
saveDesign     outputs/sys_top_final.enc
