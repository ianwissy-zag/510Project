# Timing constraints for vec_mac_top (custom BF16 FPU)
# Target: ASAP7 predictive 7nm, ~606 MHz (1650 ps period)
#
# Period specified explicitly in picoseconds to avoid tool unit ambiguity.
# Prior run used "5.0" which was interpreted as 5ps rather than 5ns.
#
# Target derived from synthesis: WNS = -1594.2ps with 5ps clock →
#   actual path delay ≈ 1572ps. 1650ps gives ~78ps positive slack margin.

# ── Clock ──────────────────────────────────────────────────────────────────
create_clock -name clk -period 1650 [get_ports clk]

set_propagated_clock [all_clocks]

# ── Reset is asynchronous ──────────────────────────────────────────────────
set_false_path -from [get_ports rst_n]

# ── Input delays (10% of clock period = 165 ps) ───────────────────────────
set axi_slave_inputs [get_ports {
    s_axis_tdata[*]
    s_axis_tuser[*]
    s_axis_tvalid
    s_axis_tlast
}]
set_input_delay -clock clk -max 165 $axi_slave_inputs
set_input_delay -clock clk -min  33 $axi_slave_inputs

set ctrl_inputs [get_ports {
    start act_buf_sel first_tile last_tile wt_buf_sel rb_start m_axis_tready
}]
set_input_delay -clock clk -max 165 $ctrl_inputs
set_input_delay -clock clk -min  33 $ctrl_inputs

# ── Output delays ──────────────────────────────────────────────────────────
set axi_master_outputs [get_ports {
    m_axis_tdata[*]
    m_axis_tvalid
    m_axis_tlast
}]
set_output_delay -clock clk -max 165 $axi_master_outputs
set_output_delay -clock clk -min  33 $axi_master_outputs

set status_outputs [get_ports {s_axis_tready done rb_busy}]
set_output_delay -clock clk -max 165 $status_outputs
set_output_delay -clock clk -min  33 $status_outputs

# ── Load / drive strength ──────────────────────────────────────────────────
set_load      0.005 [all_outputs]
set_driving_cell -lib_cell BUFx4_ASAP7_75t_R [all_inputs]
