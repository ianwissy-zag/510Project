VERILATOR ?= verilator
VFLAGS     = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH
VFLAGS_LIB = --cc        --build -sv --Wno-fatal --Wno-WIDTH

RTL_SRCS = top.sv axi.sv systolic_16x32.sv pe.sv \
           bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
           weight_sram.sv act_sram.sv output_sram.sv \
           act_stagger.sv accum_sram.sv \
           controller.sv axi_readback.sv

.PHONY: all lib run clean

all: obj_top/Vsys_top

# Library-only build (no testbench) used by Source/llm_accel
lib: obj_lib/Vsys_top__ALL.a

obj_lib/Vsys_top__ALL.a: $(RTL_SRCS) bf16_mac_dpi.c
	$(VERILATOR) $(VFLAGS_LIB) --top sys_top --Mdir obj_lib \
	    $(RTL_SRCS) bf16_mac_dpi.c

obj_top/Vsys_top: $(RTL_SRCS) bf16_mac_dpi.c tb_top.cpp
	$(VERILATOR) $(VFLAGS) --top sys_top --Mdir obj_top \
	    $(RTL_SRCS) tb_top.cpp bf16_mac_dpi.c

run: obj_top/Vsys_top
	@echo "=== 16×32 BF16 systolic testbench ==="
	./obj_top/Vsys_top

clean:
	rm -rf obj_top obj_lib
