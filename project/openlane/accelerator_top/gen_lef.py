#!/usr/bin/env python3
"""
Generate placeholder LEF abstract files for blackboxed macros.

Produces:
  lef/weight_sram.lef         — DATA_WIDTH=512, ADDR_WIDTH=5
  lef/systolic_array_32x32.lef — ARRAY_SIZE=32, ACT/WT=16b, PSUM=32b

Pin placement strategy (sky130A):
  • All pins on met3 (horizontal preferred, pitch 0.68 µm, min-width 0.30 µm)
  • Input pins on LEFT edge;  output pins on RIGHT edge
  • Obstruct met1/met2 interior so the router stays on upper metals
"""

import os

PIN_LAYER  = "met3"
PIN_W      = 0.30   # met3 minimum width (µm)
PIN_PITCH  = 0.68   # met3 pitch (µm)
FIRST_Y    = 0.34   # centre of first pin above bottom edge (half-pitch)


def _pin_block(name: str, direction: str, use: str,
               x1: float, y1: float, x2: float, y2: float) -> str:
    return (
        f"  PIN {name}\n"
        f"    DIRECTION {direction} ;\n"
        f"    USE {use} ;\n"
        f"    PORT\n"
        f"      LAYER {PIN_LAYER} ;\n"
        f"        RECT {x1:.3f} {y1:.3f} {x2:.3f} {y2:.3f} ;\n"
        f"    END\n"
        f"  END {name}\n\n"
    )


def gen_lef(path: str, macro: str, width: float, height: float,
            left_pins: list, right_pins: list) -> None:
    """
    left_pins  : list of (name, use_str)   — all INPUTs
    right_pins : list of name strings      — all OUTPUTs
    """
    buf = [
        "VERSION 5.8 ;\n",
        'BUSBITCHARS "[]" ;\n',
        'DIVIDERCHAR "/" ;\n\n',
        f"MACRO {macro}\n",
        "  CLASS BLOCK ;\n",
        "  ORIGIN 0.000 0.000 ;\n",
        f"  FOREIGN {macro} 0.000 0.000 ;\n",
        f"  SIZE {width:.3f} BY {height:.3f} ;\n",
        "  SYMMETRY X Y ;\n\n",
    ]

    for i, (name, use) in enumerate(left_pins):
        y_c = FIRST_Y + i * PIN_PITCH
        buf.append(_pin_block(name, "INPUT", use,
                              0.0,       y_c - PIN_W / 2,
                              PIN_W,     y_c + PIN_W / 2))

    for i, name in enumerate(right_pins):
        y_c = FIRST_Y + i * PIN_PITCH
        buf.append(_pin_block(name, "OUTPUT", "SIGNAL",
                              width - PIN_W, y_c - PIN_W / 2,
                              width,         y_c + PIN_W / 2))

    obs = 0.50  # interior obstruction margin
    buf += [
        "  OBS\n",
        "    LAYER met1 ;\n",
        f"      RECT {obs:.3f} {obs:.3f} {width - obs:.3f} {height - obs:.3f} ;\n",
        "    LAYER met2 ;\n",
        f"      RECT {obs:.3f} {obs:.3f} {width - obs:.3f} {height - obs:.3f} ;\n",
        "  END\n\n",
        f"END {macro}\n\n",
        "END LIBRARY\n",
    ]

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as fh:
        fh.writelines(buf)

    n_in  = len(left_pins)
    n_out = len(right_pins)
    print(f"[gen_lef] {path}")
    print(f"          {macro}  SIZE {width:.0f} x {height:.3f} µm  "
          f"({n_in} inputs, {n_out} outputs)")


def main() -> None:
    here    = os.path.dirname(os.path.abspath(__file__))
    lef_dir = os.path.join(here, "lef")

    # ── weight_sram ──────────────────────────────────────────────────────────
    # Parameters used in top.sv: DATA_WIDTH=512, DEPTH=32 → ADDR_WIDTH=5
    # Input ports:  clk, we, addr[4:0], wdata[511:0]
    # Output ports: rdata[511:0]

    wt_left: list = [("clk", "CLOCK"), ("we", "SIGNAL")]
    for b in range(4, -1, -1):           # addr[4] .. addr[0]
        wt_left.append((f"addr[{b}]", "SIGNAL"))
    for b in range(511, -1, -1):         # wdata[511] .. wdata[0]
        wt_left.append((f"wdata[{b}]", "SIGNAL"))

    wt_right: list = [f"rdata[{b}]" for b in range(511, -1, -1)]

    wt_h = FIRST_Y + max(len(wt_left), len(wt_right)) * PIN_PITCH + FIRST_Y
    gen_lef(os.path.join(lef_dir, "weight_sram.lef"),
            "weight_sram",
            200.0, round(wt_h, 3),
            wt_left, wt_right)

    # ── systolic_array_32x32 ─────────────────────────────────────────────────
    # Parameters: ARRAY_SIZE=32, ACT_WIDTH=16, WT_WIDTH=16, PSUM_WIDTH=32
    # Input ports:  clk, rst_n, load_wt,
    #               act_in[511:0], wt_in[511:0], psum_in[1023:0]
    # Output ports: psum_out[1023:0]

    sa_left: list = [("clk", "CLOCK"), ("rst_n", "SIGNAL"), ("load_wt", "SIGNAL")]
    for b in range(511, -1, -1):
        sa_left.append((f"act_in[{b}]", "SIGNAL"))
    for b in range(511, -1, -1):
        sa_left.append((f"wt_in[{b}]", "SIGNAL"))
    for b in range(1023, -1, -1):
        sa_left.append((f"psum_in[{b}]", "SIGNAL"))

    sa_right: list = [f"psum_out[{b}]" for b in range(1023, -1, -1)]

    sa_h = FIRST_Y + max(len(sa_left), len(sa_right)) * PIN_PITCH + FIRST_Y
    gen_lef(os.path.join(lef_dir, "systolic_array_32x32.lef"),
            "systolic_array_32x32",
            800.0, round(sa_h, 3),
            sa_left, sa_right)


if __name__ == "__main__":
    main()
