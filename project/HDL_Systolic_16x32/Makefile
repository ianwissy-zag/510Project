VERILATOR ?= verilator
VFLAGS     = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH

.PHONY: all run clean

all: obj_top/Vsys_top

obj_top/Vsys_top: top.sv axi.sv systolic_16x32.sv pe.sv \
                   bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
                   weight_sram.sv act_sram.sv output_sram.sv \
                   controller.sv axi_readback.sv \
                   tb_top.cpp bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS) --top sys_top --Mdir obj_top \
	    top.sv axi.sv systolic_16x32.sv pe.sv \
	    bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
	    weight_sram.sv act_sram.sv output_sram.sv \
	    controller.sv axi_readback.sv \
	    tb_top.cpp bf16_mac_dpi.c

run: obj_top/Vsys_top
	@echo "=== 16×32 BF16 systolic testbench ==="
	./obj_top/Vsys_top

clean:
	rm -rf obj_top
