VERILATOR ?= verilator
VFLAGS     = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH
VFLAGS_TRACE = $(VFLAGS) --trace

TB      = ../tb
SIM     = ../sim
IVERILOG ?= iverilog

.PHONY: all run run_mul run_add run_interface clean

all: obj_mul/Vbf16_mul obj_add/Vbf16_fp32_add obj_top/Vvec_mac_top

obj_mul/Vbf16_mul: bf16_mul.sv $(TB)/tb_bf16_mul.cpp
	$(VERILATOR) $(VFLAGS) --top bf16_mul --Mdir obj_mul \
	    bf16_mul.sv $(TB)/tb_bf16_mul.cpp

run_mul: obj_mul/Vbf16_mul
	@mkdir -p $(SIM)
	@echo "=== BF16 multiplier testbench ===" | tee $(SIM)/bf16_mul_run.log
	./obj_mul/Vbf16_mul | tee -a $(SIM)/bf16_mul_run.log

obj_add/Vbf16_fp32_add: bf16_fp32_add.sv $(TB)/tb_bf16_fp32_add.cpp
	$(VERILATOR) $(VFLAGS) --top bf16_fp32_add --Mdir obj_add \
	    bf16_fp32_add.sv $(TB)/tb_bf16_fp32_add.cpp

run_add: obj_add/Vbf16_fp32_add
	@mkdir -p $(SIM)
	@echo "=== FP32 adder testbench ===" | tee $(SIM)/bf16_fp32_add_run.log
	./obj_add/Vbf16_fp32_add | tee -a $(SIM)/bf16_fp32_add_run.log

obj_top/Vvec_mac_top: top.sv interface.sv compute_core.sv \
                       bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
                       weight_sram.sv act_sram.sv output_sram.sv \
                       controller.sv axi_readback.sv \
                       $(TB)/tb_compute_core.cpp $(TB)/bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS_TRACE) --top vec_mac_top --Mdir obj_top \
	    top.sv interface.sv compute_core.sv \
	    bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
	    weight_sram.sv act_sram.sv output_sram.sv \
	    controller.sv axi_readback.sv \
	    $(TB)/tb_compute_core.cpp $(TB)/bf16_mac_dpi.c

run: obj_top/Vvec_mac_top
	@mkdir -p $(SIM)
	@echo "=== Vec MAC top-level testbench (compute_core) ===" | tee $(SIM)/compute_core_run.log
	./obj_top/Vvec_mac_top | tee -a $(SIM)/compute_core_run.log

run_interface: interface.sv $(TB)/tb_interface.sv
	@mkdir -p $(SIM)
	$(IVERILOG) -g2012 -o $(SIM)/tb_interface_sim \
	    interface.sv $(TB)/tb_interface.sv
	@echo "=== AXI interface testbench ===" | tee $(SIM)/interface_run.log
	vvp $(SIM)/tb_interface_sim | tee -a $(SIM)/interface_run.log

run_all: run_mul run_add run run_interface

clean:
	rm -rf obj_mul obj_add obj_top $(SIM)/tb_interface_sim \
	       $(SIM)/compute_core_run.log $(SIM)/interface_run.log \
	       $(SIM)/bf16_mul_run.log $(SIM)/bf16_fp32_add_run.log \
	       $(SIM)/compute_core.vcd
