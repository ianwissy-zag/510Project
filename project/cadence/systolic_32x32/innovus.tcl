# =============================================================================
# innovus.tcl — Cadence Innovus P&R for sys_top (32×32 BF16 systolic
#               with fused bias addition and GELU Padé [3/2])
# Target PDK: ASAP7 predictive 7nm RVT
#
# Usage (run after genus.tcl completes):
#   innovus -no_gui -files innovus.tcl |& tee innovus.log
#
# Prerequisites:
#   outputs/sys_top_netlist.v  (produced by genus.tcl, includes retimed GELU)
#
# Area estimate (Genus-measured, ASAP7 RVT TT):
#   Systolic array  (1024 MACs, u_array):              ~171 K µm²
#   Readback/GELU   (32 col instances, u_rb):           ~34 K µm²
#   Accum SRAMs     (2× ping-pong 256×32 FP32):        ~290 K µm²
#   Controller + AXI slave + act_stagger + bias_sram:   ~35 K µm²
#   Total:                                             ~530 K µm²
#
# The double-buffered accumulator (u_accum_0 + u_accum_1) is the dominant
# area block at ~55% of total.  Each bank is 256 rows × 32 columns × 32-bit
# FP32 = 256 KB of register-file storage at ASAP7 cell density.
#
# Floorplan: 1100×1100 µm at 44% utilisation (1210 K µm² die area).
# Routing headroom is reduced vs. the single-bank design; increase die size
# to 1200×1200 µm (55% → 37%) if congestion is observed near the accum banks.
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
# 1100×1100 µm die at 44% utilisation (~530 K µm² / 1210 K µm² die area).
# The two ping-pong accum SRAMs (~290 K µm² combined) are the area dominant
# blocks; place them centrally between the systolic array and the readback AXI
# master to minimise the critical rdata bus routing distance.
floorPlan -r 1.0 0.44 2.0 2.0 2.0 2.0   ;# aspect 1.0, 44% util, 2µm margins

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
# The retimed GELU pipeline registers and the double-buffered accum write-enable
# flops are all on the main clock; CTS treats them identically to the PE regs.
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
