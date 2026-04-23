VERILATOR ?= verilator
VFLAGS     = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH

.PHONY: all run run_pe run_axi run_systolic run_weight_sram run_act_sram \
        run_output_sram run_controller run_axi_readback run_top clean

all: obj_pe/Vmac_pe \
     obj_axi/Vaxis_to_pingpong_buffer \
     obj_systolic/Vsystolic_array_32x32 \
     obj_weight_sram/Vweight_sram \
     obj_act_sram/Vact_sram \
     obj_output_sram/Voutput_sram \
     obj_controller/Vcontroller \
     obj_axi_readback/Vaxi_readback \
     obj_top/Vaccelerator_top

obj_pe/Vmac_pe: pe.sv bf16_mac_unit.sv tb_pe.cpp bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS) --top mac_pe --Mdir obj_pe \
	    pe.sv bf16_mac_unit.sv tb_pe.cpp bf16_mac_dpi.c

obj_axi/Vaxis_to_pingpong_buffer: axi.sv tb_axi.cpp
	$(VERILATOR) $(VFLAGS) --top axis_to_pingpong_buffer --Mdir obj_axi \
	    axi.sv tb_axi.cpp

obj_systolic/Vsystolic_array_32x32: pe.sv bf16_mac_unit.sv systolic.sv tb_systolic.cpp bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS) --top systolic_array_32x32 --Mdir obj_systolic \
	    pe.sv bf16_mac_unit.sv systolic.sv tb_systolic.cpp bf16_mac_dpi.c

obj_weight_sram/Vweight_sram: weight_sram.sv tb_weight_sram.cpp
	$(VERILATOR) $(VFLAGS) --top weight_sram --Mdir obj_weight_sram \
	    weight_sram.sv tb_weight_sram.cpp

obj_act_sram/Vact_sram: act_sram.sv tb_act_sram.cpp
	$(VERILATOR) $(VFLAGS) --top act_sram --Mdir obj_act_sram \
	    act_sram.sv tb_act_sram.cpp

obj_output_sram/Voutput_sram: output_sram.sv tb_output_sram.cpp
	$(VERILATOR) $(VFLAGS) --top output_sram --Mdir obj_output_sram \
	    output_sram.sv tb_output_sram.cpp

run_pe: obj_pe/Vmac_pe
	@echo "=== PE testbench ==="
	./obj_pe/Vmac_pe

run_axi: obj_axi/Vaxis_to_pingpong_buffer
	@echo "=== AXI testbench ==="
	./obj_axi/Vaxis_to_pingpong_buffer

run_systolic: obj_systolic/Vsystolic_array_32x32
	@echo "=== Systolic Array testbench ==="
	./obj_systolic/Vsystolic_array_32x32

run_weight_sram: obj_weight_sram/Vweight_sram
	@echo "=== Weight SRAM testbench ==="
	./obj_weight_sram/Vweight_sram

run_act_sram: obj_act_sram/Vact_sram
	@echo "=== Activation SRAM testbench ==="
	./obj_act_sram/Vact_sram

run_output_sram: obj_output_sram/Voutput_sram
	@echo "=== Output SRAM testbench ==="
	./obj_output_sram/Voutput_sram

obj_controller/Vcontroller: controller.sv tb_controller.cpp
	$(VERILATOR) $(VFLAGS) --top controller --Mdir obj_controller \
	    controller.sv tb_controller.cpp

obj_axi_readback/Vaxi_readback: axi_readback.sv tb_axi_readback.cpp
	$(VERILATOR) $(VFLAGS) --top axi_readback --Mdir obj_axi_readback \
	    axi_readback.sv tb_axi_readback.cpp

run_controller: obj_controller/Vcontroller
	@echo "=== Controller testbench ==="
	./obj_controller/Vcontroller

run_axi_readback: obj_axi_readback/Vaxi_readback
	@echo "=== AXI Readback testbench ==="
	./obj_axi_readback/Vaxi_readback

obj_top/Vaccelerator_top: top.sv axi.sv pe.sv bf16_mac_unit.sv systolic.sv \
                          weight_sram.sv act_sram.sv output_sram.sv \
                          controller.sv axi_readback.sv tb_top.cpp bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS) --top accelerator_top --Mdir obj_top \
	    top.sv axi.sv pe.sv bf16_mac_unit.sv systolic.sv \
	    weight_sram.sv act_sram.sv output_sram.sv \
	    controller.sv axi_readback.sv tb_top.cpp bf16_mac_dpi.c

run_top: obj_top/Vaccelerator_top
	@echo "=== Top-level testbench ==="
	./obj_top/Vaccelerator_top

run: all
	@$(MAKE) --no-print-directory \
	    run_pe run_axi run_systolic run_weight_sram run_act_sram \
	    run_output_sram run_controller run_axi_readback run_top

clean:
	rm -rf obj_pe obj_axi obj_systolic obj_weight_sram obj_act_sram \
	       obj_output_sram obj_controller obj_axi_readback obj_top
