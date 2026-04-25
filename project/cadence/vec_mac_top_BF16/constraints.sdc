# Timing constraints for vec_mac_top
# Target: ASAP7 predictive 7nm, 200 MHz (5 ns period)
#
# 5ns is a conservative starting point for the FP32 MAC combinational depth
# at 7nm.  The HardFloat multiply-add path has significant logic depth; if
# synthesis closes cleanly at 5ns it is worth tightening toward 2ns (500MHz).
# The controller, AXI, and SRAM paths will close well above 1GHz — the FP
# MAC is the sole critical path.

# ── Clock ──────────────────────────────────────────────────────────────────
create_clock -name clk -period 5.0 [get_ports clk]

set_propagated_clock [all_clocks]

# ── Reset is asynchronous ──────────────────────────────────────────────────
set_false_path -from [get_ports rst_n]

# ── Input delays (10% of clock period) ────────────────────────────────────
set axi_slave_inputs [get_ports {
    s_axis_tdata[*]
    s_axis_tuser[*]
    s_axis_tvalid
    s_axis_tlast
}]
set_input_delay -clock clk -max 0.5 $axi_slave_inputs
set_input_delay -clock clk -min 0.1 $axi_slave_inputs

set ctrl_inputs [get_ports {
    start act_buf_sel first_tile last_tile wt_buf_sel rb_start m_axis_tready
}]
set_input_delay -clock clk -max 0.5 $ctrl_inputs
set_input_delay -clock clk -min 0.1 $ctrl_inputs

# ── Output delays ──────────────────────────────────────────────────────────
set axi_master_outputs [get_ports {
    m_axis_tdata[*]
    m_axis_tvalid
    m_axis_tlast
}]
set_output_delay -clock clk -max 0.5 $axi_master_outputs
set_output_delay -clock clk -min 0.1 $axi_master_outputs

set status_outputs [get_ports {s_axis_tready done rb_busy}]
set_output_delay -clock clk -max 0.5 $status_outputs
set_output_delay -clock clk -min 0.1 $status_outputs

# ── Load / drive strength ──────────────────────────────────────────────────
set_load      0.005 [all_outputs]
set_driving_cell -lib_cell BUFx4_ASAP7_75t_R [all_inputs]
