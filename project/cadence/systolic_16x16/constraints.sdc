# =============================================================================
# constraints.sdc — Synthesis timing constraints for sys_top
# Target: ASAP7 predictive 7nm RVT, 1650 ps clock period (≈ 606 MHz)
#
# The critical path is through the bf16_mac_unit inside the systolic PEs:
#   bf16_mul (~450 ps) + bf16_fp32_add (~450 ps) + register setup ≈ 1100 ps
# leaving ~550 ps for routing and clock skew at 1650 ps.
#
# The GELU readback path is explicitly pipelined in RTL (gelu_unit.sv) with
# at most 2 sequential bf16_mul per pipeline stage (~900 ps); it does not
# set the critical path.
# =============================================================================

# ── Clock ─────────────────────────────────────────────────────────────────────
create_clock -name clk -period 1.650 [get_ports clk]
set_clock_uncertainty -setup 0.050 [get_clocks clk]
set_clock_uncertainty -hold  0.025 [get_clocks clk]
set_clock_transition         0.010 [get_clocks clk]

# ── Reset ─────────────────────────────────────────────────────────────────────
# Asynchronous active-low reset — no functional timing arc through rst_n.
set_false_path -from [get_ports rst_n]

# ── Input delays ──────────────────────────────────────────────────────────────
# 500 ps max / 0 ps min — conservative model for an on-chip block interface.
set all_data_inputs [remove_from_collection [all_inputs] [get_ports {clk rst_n}]]
set_input_delay -clock clk -max 0.500 $all_data_inputs
set_input_delay -clock clk -min 0.000 $all_data_inputs

# ── Output delays ─────────────────────────────────────────────────────────────
set_output_delay -clock clk -max 0.500 [all_outputs]
set_output_delay -clock clk -min 0.000 [all_outputs]

# ── Drive and load assumptions ────────────────────────────────────────────────
set_driving_cell -lib_cell BUFx4_ASAP7_75t_R -pin Y \
    [remove_from_collection [all_inputs] [get_ports {clk rst_n}]]
set_load 5 [all_outputs]
