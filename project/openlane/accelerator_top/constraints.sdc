# Timing constraints for accelerator_top
# Target: sky130A 130nm, 50 MHz (20 ns period)

# ── Clock ──────────────────────────────────────────────────────────────────
create_clock -name clk -period 20.0 [get_ports clk]

# Allow OpenROAD to add clock buffers
set_propagated_clock [all_clocks]

# ── Reset is asynchronous – treat as false path for timing analysis ────────
set_false_path -from [get_ports rst_n]

# ── Input delays (relative to clk) ────────────────────────────────────────
# AXI slave inputs: assume data is valid 0.2 ns after the driving clock edge
set axi_slave_inputs [get_ports {
    s_axis_tdata[*]
    s_axis_tuser[*]
    s_axis_tvalid
    s_axis_tlast
}]
set_input_delay -clock clk -max 2.0 $axi_slave_inputs
set_input_delay -clock clk -min 0.5 $axi_slave_inputs

# Control inputs
set ctrl_inputs [get_ports {start act_buf_sel first_tile last_tile wt_buf_sel rb_start m_axis_tready}]
set_input_delay -clock clk -max 2.0 $ctrl_inputs
set_input_delay -clock clk -min 0.5 $ctrl_inputs

# ── Output delays ──────────────────────────────────────────────────────────
# AXI master outputs: must be stable 2 ns before the next clock edge
set axi_master_outputs [get_ports {
    m_axis_tdata[*]
    m_axis_tvalid
    m_axis_tlast
}]
set_output_delay -clock clk -max 2.0 $axi_master_outputs
set_output_delay -clock clk -min 0.5 $axi_master_outputs

# Status outputs
set status_outputs [get_ports {s_axis_tready done rb_busy}]
set_output_delay -clock clk -max 2.0 $status_outputs
set_output_delay -clock clk -min 0.5 $status_outputs

# ── Load / drive strength ──────────────────────────────────────────────────
set_load      0.01 [all_outputs]
set_driving_cell -lib_cell sky130_fd_sc_hd__buf_4 [all_inputs]
