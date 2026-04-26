# =============================================================================
# mmmc.tcl — Multi-Mode Multi-Corner timing config for Innovus 2024
# Target PDK: ASAP7 predictive 7nm RVT
#
# Three corners:
#   Setup (worst-case): SS  100C  0.63V
#   Hold  (best-case):  FF  0C    0.77V
#   Nominal:            TT  25C   0.70V
# =============================================================================

# asap7_lib_dir may be pre-set by the calling script (innovus.tcl).
# If not already defined, derive it from this file's location.
if {![info exists asap7_lib_dir]} {
    set script_dir    [file dirname [file normalize [info script]]]
    set asap7_lib_dir [file normalize $script_dir/../asap7/asap7sc7p5t_28/LIB/NLDM]
}

# ── Library sets ──────────────────────────────────────────────────────────────
create_library_set -name libs_tt \
    -timing [list $asap7_lib_dir/asap7sc7p5t_SIMPLE_RVT_TT_nldm_211120.lib]

create_library_set -name libs_ss \
    -timing [list $asap7_lib_dir/asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120.lib]

create_library_set -name libs_ff \
    -timing [list $asap7_lib_dir/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib]

# ── RC corners ────────────────────────────────────────────────────────────────
# ASAP7 wire RC values from the PDK documentation.
# Adjust postRoute values after extracting with Quantus if available.
create_rc_corner -name rc_tt \
    -preRoute_res  1.0  -preRoute_cap  1.0 \
    -postRoute_res 1.0  -postRoute_cap 1.0

create_rc_corner -name rc_ss \
    -preRoute_res  1.2  -preRoute_cap  1.1 \
    -postRoute_res 1.2  -postRoute_cap 1.1

create_rc_corner -name rc_ff \
    -preRoute_res  0.9  -preRoute_cap  0.9 \
    -postRoute_res 0.9  -postRoute_cap 0.9

# ── Delay corners ─────────────────────────────────────────────────────────────
create_delay_corner -name dc_tt -library_set libs_tt -rc_corner rc_tt
create_delay_corner -name dc_ss -library_set libs_ss -rc_corner rc_ss
create_delay_corner -name dc_ff -library_set libs_ff -rc_corner rc_ff

# ── Constraint mode ───────────────────────────────────────────────────────────
# Derive absolute SDC path — relative paths fail when Innovus CWD != script dir
if {![info exists script_dir]} {
    set script_dir [file dirname [file normalize [info script]]]
}
create_constraint_mode -name cm_func \
    -sdc_files [list [file normalize $script_dir/constraints_pnr.sdc]]

# ── Analysis views ────────────────────────────────────────────────────────────
create_analysis_view -name av_setup -constraint_mode cm_func -delay_corner dc_ss
create_analysis_view -name av_hold  -constraint_mode cm_func -delay_corner dc_ff
create_analysis_view -name av_tt    -constraint_mode cm_func -delay_corner dc_tt

set_analysis_view -setup [list av_setup] -hold [list av_hold]
