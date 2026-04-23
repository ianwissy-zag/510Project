# Pre-PnR static timing analysis for accelerator_top glue logic.
# Gate delays only — no wire parasitics.  Results are ~20-40% optimistic
# vs post-route.  Run inside the OpenLane Docker container:
#   opensta sta_pre_pnr.tcl

set LIB   /root/.volare/volare/sky130/versions/0fe599b2afb6708d281543108caf8310912f54af/sky130A/libs.ref/sky130_fd_sc_hd/lib/sky130_fd_sc_hd__tt_025C_1v80.lib
set NL    /project/openlane/accelerator_top/runs/RUN_2026-04-21_21-00-56/06-yosys-synthesis/accelerator_top.nl.v
set SDC   /project/openlane/accelerator_top/constraints.sdc

read_liberty $LIB
read_verilog $NL
link_design  accelerator_top
read_sdc     $SDC

# ── Setup timing ─────────────────────────────────────────────────────────
puts "\n===== SETUP (worst negative slack) ====="
report_wns
report_tns

puts "\n===== TOP 5 CRITICAL PATHS (setup) ====="
report_checks -path_delay max -fields {slew cap input_pins net} \
              -format full_clock_expanded -digits 3 -group_count 5

# ── Hold timing ──────────────────────────────────────────────────────────
puts "\n===== HOLD (worst negative slack) ====="
report_checks -path_delay min -fields {slew cap input_pins net} \
              -format full_clock_expanded -digits 3 -group_count 3

# ── Logic depth summary ───────────────────────────────────────────────────
puts "\n===== CELL COUNT / AREA ====="
report_cell_usage
report_design_area
