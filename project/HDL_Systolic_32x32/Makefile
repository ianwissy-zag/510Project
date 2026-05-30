VERILATOR      ?= verilator
VFLAGS          = --cc --exe --build -sv --Wno-fatal --Wno-WIDTH --trace-fst
VFLAGS_LIB      = --cc        --build -sv --Wno-fatal --Wno-WIDTH
VERILATOR_INC  ?= $(shell $(VERILATOR) --getenv VERILATOR_ROOT)/include
HAL_DIR         = ../Source/llm_accel

RTL_SRCS = top.sv axi.sv systolic_32x32.sv pe.sv \
           bf16_mac_unit.sv bf16_mul.sv fp32_mul.sv bf16_fp32_add.sv \
           weight_sram.sv act_sram.sv output_sram.sv \
           act_stagger.sv accum_sram.sv bias_sram.sv \
           fp32_recip.sv gelu_unit.sv \
           controller.sv axi_readback.sv

C_SRCS = bf16_mac_dpi.c

.PHONY: all lib run hal_tb run_hal_tb clean

all: obj_top/Vsys_top

lib: obj_lib/Vsys_top__ALL.a

obj_lib/Vsys_top__ALL.a: $(RTL_SRCS) $(C_SRCS)
	$(VERILATOR) $(VFLAGS_LIB) --top sys_top --Mdir obj_lib \
	    $(RTL_SRCS) $(C_SRCS)

obj_top/Vsys_top: $(RTL_SRCS) $(C_SRCS) tb_top.cpp
	$(VERILATOR) $(VFLAGS) --top sys_top --Mdir obj_top \
	    $(RTL_SRCS) tb_top.cpp $(C_SRCS)

run: obj_top/Vsys_top
	@echo "=== 32x32 BF16 systolic + bias + GELU testbench ==="
	./obj_top/Vsys_top

# HAL-level GELU testbench — links the Verilator model directly against
# accel_hal_systolic_vrl and tb_gelu_hal without the GPT-2 runtime.
hal_tb: obj_lib/Vsys_top__ALL.a tb_gelu_hal.cpp $(HAL_DIR)/accel_hal_systolic_vrl.cpp
	g++ -O2 -std=c++17 \
	    -DACCEL_BACKEND_SYSTOLIC_VRL \
	    -I$(HAL_DIR) \
	    -I obj_lib \
	    -I$(VERILATOR_INC) -I$(VERILATOR_INC)/vltstd \
	    tb_gelu_hal.cpp \
	    $(HAL_DIR)/accel_hal_systolic_vrl.cpp \
	    obj_lib/Vsys_top__ALL.a \
	    $(VERILATOR_INC)/verilated.cpp \
	    $(VERILATOR_INC)/verilated_threads.cpp \
	    bf16_mac_dpi.c \
	    -lpthread -latomic -lm \
	    -o hal_tb

run_hal_tb: hal_tb
	@echo "=== HAL-level GELU testbench ==="
	./hal_tb

clean:
	rm -rf obj_top obj_lib hal_tb
