VERILATOR ?= verilator
VFLAGS     = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH

.PHONY: all run clean

all: obj_top/Vvec_mac_top

obj_top/Vvec_mac_top: top.sv axi.sv vec_mac_array.sv bf16_mac_unit.sv \
                       weight_sram.sv act_sram.sv output_sram.sv \
                       controller.sv axi_readback.sv tb_top.cpp bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS) --top vec_mac_top --Mdir obj_top \
	    top.sv axi.sv vec_mac_array.sv bf16_mac_unit.sv \
	    weight_sram.sv act_sram.sv output_sram.sv \
	    controller.sv axi_readback.sv tb_top.cpp bf16_mac_dpi.c

run: obj_top/Vvec_mac_top
	@echo "=== Vec MAC top-level testbench ==="
	./obj_top/Vvec_mac_top

clean:
	rm -rf obj_top
