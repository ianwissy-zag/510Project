# Timing constraints for sys_top (32×32 BF16 systolic + fused bias/GELU)
# Target: ASAP7 predictive 7nm, ~606 MHz (1650 ps period)
#
# Clock target set by the BF16 MAC unit critical path (unchanged from the
# plain 32×32 systolic).  The GELU readback path is ~15 bf16_mul stages
# deep and requires retiming in Genus — see genus.tcl.
# Period specified explicitly in picoseconds to avoid tool unit ambiguity.

create_clock -name clk -period 1650 [get_ports clk]
set_propagated_clock [all_clocks]
set_false_path -from [get_ports rst_n]

# ── AXI4-Stream slave inputs ──────────────────────────────────────────────────
set axi_slave_inputs [get_ports {
    s_axis_tdata[*] s_axis_tuser[*] s_axis_tvalid s_axis_tlast
}]
set_input_delay -clock clk -max 165 $axi_slave_inputs
set_input_delay -clock clk -min  33 $axi_slave_inputs

# ── Control inputs ────────────────────────────────────────────────────────────
set ctrl_inputs [get_ports {
    start mode first_k_tile fwd_buf_sel bwd_buf_sel
    M_count[*] apply_bias apply_gelu
    rb_start m_axis_tready
}]
set_input_delay -clock clk -max 165 $ctrl_inputs
set_input_delay -clock clk -min  33 $ctrl_inputs

# ── AXI4-Stream master outputs ────────────────────────────────────────────────
# The retimed GELU pipeline registers are internal; the final combinational
# stage from the last pipeline register to m_axis_tdata must meet this delay.
set axi_master_outputs [get_ports {
    m_axis_tdata[*] m_axis_tvalid m_axis_tlast
}]
set_output_delay -clock clk -max 165 $axi_master_outputs
set_output_delay -clock clk -min  33 $axi_master_outputs

set status_outputs [get_ports {s_axis_tready done rb_busy}]
set_output_delay -clock clk -max 165 $status_outputs
set_output_delay -clock clk -min  33 $status_outputs

set_load      0.005 [all_outputs]
set_driving_cell -lib_cell BUFx4_ASAP7_75t_R [all_inputs]
