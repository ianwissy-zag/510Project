import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, Timer


async def clk_edge(dut):
    """Wait for rising edge then 1ps for NBA updates to settle."""
    await RisingEdge(dut.clk)
    await Timer(1, unit="ps")


async def reset(dut):
    """Assert synchronous reset for one cycle then release."""
    dut.rst.value = 1
    await clk_edge(dut)
    dut.rst.value = 0


@cocotb.test()
async def test_mac_basic(dut):
    """Basic accumulation: a=3, b=4 → 12, 24, 36 then reset to 0."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())

    dut.rst.value = 1
    await clk_edge(dut)
    dut.rst.value = 0

    dut.a.value = 3
    dut.b.value = 4

    for expected in [12, 24, 36]:
        await clk_edge(dut)
        assert dut.out.value.to_signed() == expected, \
            f"Expected {expected}, got {dut.out.value.to_signed()}"

    dut.rst.value = 1
    await clk_edge(dut)
    assert dut.out.value.to_signed() == 0, \
        f"Reset failed: got {dut.out.value.to_signed()}"


@cocotb.test()
async def test_mac_signed_negative(dut):
    """Signed multiplication: a=-3, b=4 → -12, -24, -36."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.a.value = -3 & 0xFF
    dut.b.value = 4

    for expected in [-12, -24, -36]:
        await clk_edge(dut)
        assert dut.out.value.to_signed() == expected, \
            f"Expected {expected}, got {dut.out.value.to_signed()}"


@cocotb.test()
async def test_mac_signed_both_negative(dut):
    """Both inputs negative: a=-2, b=-3 → +6, +12, +18."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.a.value = -2 & 0xFF
    dut.b.value = -3 & 0xFF

    for expected in [6, 12, 18]:
        await clk_edge(dut)
        assert dut.out.value.to_signed() == expected, \
            f"Expected {expected}, got {dut.out.value.to_signed()}"


@cocotb.test()
async def test_mac_zero_input(dut):
    """Zero input: accumulator should not change."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.a.value = 2
    dut.b.value = 5
    await clk_edge(dut)
    assert dut.out.value.to_signed() == 10

    dut.a.value = 0
    for _ in range(3):
        await clk_edge(dut)
        assert dut.out.value.to_signed() == 10, \
            f"Accumulator changed with zero input: {dut.out.value.to_signed()}"


@cocotb.test()
async def test_mac_max_positive(dut):
    """Boundary: a=127, b=127 (max signed 8-bit) → 16129 per cycle."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.a.value = 127
    dut.b.value = 127
    product = 127 * 127

    acc = 0
    for _ in range(5):
        await clk_edge(dut)
        acc += product
        assert dut.out.value.to_signed() == acc, \
            f"Expected {acc}, got {dut.out.value.to_signed()}"


@cocotb.test()
async def test_mac_reset_mid_stream(dut):
    """Reset mid-stream clears accumulator immediately on next posedge."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.a.value = 5
    dut.b.value = 5
    await clk_edge(dut)
    await clk_edge(dut)
    assert dut.out.value.to_signed() == 50

    dut.rst.value = 1
    await clk_edge(dut)
    assert dut.out.value.to_signed() == 0, \
        f"Mid-stream reset failed: {dut.out.value.to_signed()}"

    dut.rst.value = 0
    dut.a.value = 2
    dut.b.value = 3
    await clk_edge(dut)
    assert dut.out.value.to_signed() == 6


@cocotb.test()
async def test_mac_overflow_positive(dut):
    """Positive overflow: accumulator wraps from max to min signed."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    # Pre-load accumulator to one product below max signed 32-bit.
    # a=1, b=1 → product=1; so set out = 2^31 - 2 and check two cycles:
    #   cycle 1: 2^31 - 2 + 1 = 2^31 - 1 (max positive, no overflow yet)
    #   cycle 2: 2^31 - 1 + 1 = 2^31     → wraps to -2^31 (min negative)
    dut.out.value = 2**31 - 2   # 0x7FFFFFFE
    await Timer(1, unit="ps")

    dut.a.value = 1
    dut.b.value = 1

    await clk_edge(dut)
    assert dut.out.value.to_signed() == 2**31 - 1, \
        f"Expected {2**31 - 1}, got {dut.out.value.to_signed()}"

    await clk_edge(dut)
    assert dut.out.value.to_signed() == -(2**31), \
        f"Expected overflow to {-(2**31)}, got {dut.out.value.to_signed()}"


@cocotb.test()
async def test_mac_overflow_negative(dut):
    """Negative overflow (underflow): accumulator wraps from min to max signed."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    # Pre-load accumulator to min signed + 1; a=-1, b=1 → product=-1
    #   cycle 1: -2^31 + 1 + (-1) = -2^31     (min, no wrap yet)
    #   cycle 2: -2^31 + (-1)     = -2^31 - 1 → wraps to 2^31 - 1 (max positive)
    dut.out.value = (-(2**31) + 1) & 0xFFFFFFFF   # 0x80000001
    await Timer(1, unit="ps")

    dut.a.value = 0xFF   # -1 in 8-bit two's complement
    dut.b.value = 1

    await clk_edge(dut)
    assert dut.out.value.to_signed() == -(2**31), \
        f"Expected {-(2**31)}, got {dut.out.value.to_signed()}"

    await clk_edge(dut)
    assert dut.out.value.to_signed() == 2**31 - 1, \
        f"Expected underflow to {2**31 - 1}, got {dut.out.value.to_signed()}"
