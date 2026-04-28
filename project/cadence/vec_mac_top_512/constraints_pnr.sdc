# Timing constraints for vec_mac_top (custom BF16 FPU) — P&R target
# Target: ASAP7 predictive 7nm, ~588 MHz (1700 ps period)
#
# Period specified explicitly in picoseconds to avoid tool unit ambiguity.
# Slightly relaxed from synthesis target (1650ps) to give post-route
# wire delay margin — typical routing adds 5-15% to pre-route estimates.

# ── Clock ──────────────────────────────────────────────────────────────────
create_clock -name clk -period 1700 [get_ports clk]

set_propagated_clock [all_clocks]

# ── Reset is asynchronous ──────────────────────────────────────────────────
set_false_path -from [get_ports rst_n]

# ── Input delays (10% of clock period = 170 ps) ───────────────────────────
set axi_slave_inputs [get_ports {
    s_axis_tdata[*]
    s_axis_tuser[*]
    s_axis_tvalid
    s_axis_tlast
}]
set_input_delay -clock clk -max 170 $axi_slave_inputs
set_input_delay -clock clk -min  34 $axi_slave_inputs

set ctrl_inputs [get_ports {
    start act_buf_sel first_tile last_tile wt_buf_sel rb_start m_axis_tready
}]
set_input_delay -clock clk -max 170 $ctrl_inputs
set_input_delay -clock clk -min  34 $ctrl_inputs

# ── Output delays ──────────────────────────────────────────────────────────
set axi_master_outputs [get_ports {
    m_axis_tdata[*]
    m_axis_tvalid
    m_axis_tlast
}]
set_output_delay -clock clk -max 170 $axi_master_outputs
set_output_delay -clock clk -min  34 $axi_master_outputs

set status_outputs [get_ports {s_axis_tready done rb_busy}]
set_output_delay -clock clk -max 170 $status_outputs
set_output_delay -clock clk -min  34 $status_outputs

# ── Load / drive strength ──────────────────────────────────────────────────
set_load      0.005 [all_outputs]
set_driving_cell -lib_cell BUFx4_ASAP7_75t_R [all_inputs]
