/* DPI-C helper: IEEE 754 FP32 multiply-accumulate for bf16_mac_unit.sv
 *
 * Verilator promotes 'shortreal' to 'real' (double), so $shortrealtobits
 * returns the bottom 32 bits of the 64-bit double encoding — wrong for FP32.
 * This C function does the arithmetic with genuine 32-bit floats on the host
 * FPU and returns the exact IEEE 754 single-precision bit pattern.
 *
 * Called as:
 *   import "DPI-C" function int bf16_mac_fp32(input int, input int, input int);
 *   acc_out_bits = bf16_mac_fp32(act_fp32_bits, wt_fp32_bits, acc_fp32_bits);
 */
#include "svdpi.h"
#include <string.h>

/* acc + act * wt, all in FP32.  Arguments are the raw 32-bit bit patterns. */
/* extern "C" so the symbol is unmangled when g++ compiles this file.      */
#ifdef __cplusplus
extern "C"
#endif
int bf16_mac_fp32(int act_bits, int wt_bits, int acc_bits)
{
    float act, wt, acc, result;
    memcpy(&act, &act_bits, sizeof(float));
    memcpy(&wt,  &wt_bits,  sizeof(float));
    memcpy(&acc, &acc_bits, sizeof(float));
    result = acc + act * wt;
    int out;
    memcpy(&out, &result, sizeof(float));
    return out;
}
