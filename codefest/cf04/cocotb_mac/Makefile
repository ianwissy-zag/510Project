SIM = icarus
TOPLEVEL_LANG = verilog

# Select implementation: correct | llm_a | llm_b  (default: correct)
IMPL ?= correct
ifeq ($(IMPL), correct)
    VERILOG_SOURCES = $(PWD)/../hdl/mac_correct.sv
else ifeq ($(IMPL), llm_a)
    VERILOG_SOURCES = $(PWD)/../hdl/mac_llm_A.sv
else ifeq ($(IMPL), llm_b)
    VERILOG_SOURCES = $(PWD)/../hdl/mac_llm_B.sv
endif

TOPLEVEL = mac
COCOTB_TEST_MODULES = test_mac

# Icarus needs -g2012 to accept SystemVerilog syntax in .sv files
COMPILE_ARGS = -g2012

# Generate VCD waveform dump (open with: gtkwave dump.vcd)
WAVES = 1

include $(shell cocotb-config --makefiles)/Makefile.sim
