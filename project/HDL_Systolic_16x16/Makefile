VERILATOR  ?= verilator
VFLAGS      = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH
VFLAGS_LIB  = --cc        --build -sv --Wno-fatal --Wno-WIDTH

RTL_SRCS = top.sv axi.sv systolic_16x16.sv pe.sv \
           bf16_mac_unit.sv bf16_mul.sv bf16_fp32_add.sv \
           weight_sram.sv act_sram.sv output_sram.sv \
           act_stagger.sv accum_sram.sv bias_sram.sv \
           fp32_recip.sv gelu_unit.sv \
           controller.sv axi_readback.sv

C_SRCS = bf16_mac_dpi.c

.PHONY: all lib run clean

all: obj_top/Vsys_top

lib: obj_lib/Vsys_top__ALL.a

obj_lib/Vsys_top__ALL.a: $(RTL_SRCS) $(C_SRCS)
	$(VERILATOR) $(VFLAGS_LIB) --top sys_top --Mdir obj_lib \
	    $(RTL_SRCS) $(C_SRCS)

obj_top/Vsys_top: $(RTL_SRCS) $(C_SRCS) tb_top.cpp
	$(VERILATOR) $(VFLAGS) --top sys_top --Mdir obj_top \
	    $(RTL_SRCS) tb_top.cpp $(C_SRCS)

run: obj_top/Vsys_top
	@echo "=== 16x16 BF16 systolic + bias + GELU testbench ==="
	./obj_top/Vsys_top

clean:
	rm -rf obj_top obj_lib
