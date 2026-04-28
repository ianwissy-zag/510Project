VERILATOR ?= verilator
VFLAGS     = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH

.PHONY: all run run_mul clean

all: obj_mul/Vbf16_mul obj_add/Vbf16_fp32_add obj_top/Vvec_mac_top

obj_mul/Vbf16_mul: bf16_mul.sv tb_bf16_mul.cpp
	$(VERILATOR) $(VFLAGS) --top bf16_mul --Mdir obj_mul \
	    bf16_mul.sv tb_bf16_mul.cpp

run_mul: obj_mul/Vbf16_mul
	@echo "=== BF16 multiplier testbench ==="
	./obj_mul/Vbf16_mul

obj_add/Vbf16_fp32_add: bf16_fp32_add.sv tb_bf16_fp32_add.cpp
	$(VERILATOR) $(VFLAGS) --top bf16_fp32_add --Mdir obj_add \
	    bf16_fp32_add.sv tb_bf16_fp32_add.cpp

run_add: obj_add/Vbf16_fp32_add
	@echo "=== FP32 adder testbench ==="
	./obj_add/Vbf16_fp32_add

obj_top/Vvec_mac_top: top.sv axi.sv vec_mac_array.sv \
                       bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
                       weight_sram.sv act_sram.sv output_sram.sv \
                       controller.sv axi_readback.sv tb_top.cpp bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS) --top vec_mac_top --Mdir obj_top \
	    top.sv axi.sv vec_mac_array.sv \
	    bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
	    weight_sram.sv act_sram.sv output_sram.sv \
	    controller.sv axi_readback.sv tb_top.cpp bf16_mac_dpi.c

run: obj_top/Vvec_mac_top
	@echo "=== Vec MAC top-level testbench ==="
	./obj_top/Vvec_mac_top

clean:
	rm -rf obj_mul obj_add obj_top
