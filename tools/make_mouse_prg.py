#!/usr/bin/env python3
"""Writes core/test/c64/mouse.prg, the shared PC boot test's 1351 mouse program for Kairo64
(Kairo64#58, #63): 6502 code behind a BASIC line (10 SYS2061) that reads the 1351 in port 1 each
frame (SID POTX and POTY, $D419 and $D41A: the mouse's position modulo 64 in bits 1-6), moves a
pointer, sprite 0, a pixel per count over the 320 x 200 screen (stopping at its edges), and on a
press of the left button (port 1's fire line, $DC01 bit 4) sets the background colour from where the
pointer is. It selects port 1 for the POT lines itself, with the KERNAL's interrupt off. A mouse program as GEOS-era software reads it, which direct touch (MachineCore::mouse_warp)
and Auto touch mode (the guest's reads of the mouse) are checked on.

    make_mouse_prg.py
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGIN = 0x080D  # after the BASIC line at $0801

# Variables, in the free RAM at $C000.
XLO, XHI, Y, OX, OY, BUTTON, T = (0xC000 + n for n in range(7))
SPRITE = 0x0340  # sprite block 13


class Assembler:
    """6502 code with labels: absolute and immediate operands, relative branches, JMP."""

    IMPLIED = {"sei": 0x78, "sec": 0x38, "clc": 0x18, "tax": 0xAA, "dex": 0xCA, "lsr": 0x4A, "ror": 0x6A}
    IMMEDIATE = {"lda": 0xA9, "ldx": 0xA2, "and": 0x29, "ora": 0x09, "cmp": 0xC9, "adc": 0x69, "sbc": 0xE9}
    ABSOLUTE = {"lda": 0xAD, "sta": 0x8D, "stx": 0x8E, "adc": 0x6D, "sbc": 0xED, "cmp": 0xCD, "jmp": 0x4C}
    ABSOLUTE_X = {"sta": 0x9D}
    BRANCH = {"bne": 0xD0, "beq": 0xF0, "bcc": 0x90, "bcs": 0xB0, "bpl": 0x10, "bmi": 0x30}

    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []  # (offset, label, kind)

    def label(self, name):
        if name in self.labels:
            raise SystemExit(f"label {name} twice")
        self.labels[name] = ORIGIN + len(self.code)

    def op(self, name, operand=None, mode=None):
        if operand is None:
            self.code.append(self.IMPLIED[name])
        elif mode == "#":
            self.code += bytes([self.IMMEDIATE[name], operand & 0xFF])
        elif name in self.BRANCH:
            self.code += bytes([self.BRANCH[name], 0])
            self.fixups.append((len(self.code) - 1, operand, "branch"))
        elif mode == "x":
            self.code += bytes([self.ABSOLUTE_X[name]]) + operand.to_bytes(2, "little")
        elif isinstance(operand, str):
            self.code += bytes([self.ABSOLUTE[name], 0, 0])
            self.fixups.append((len(self.code) - 2, operand, "absolute"))
        else:
            self.code += bytes([self.ABSOLUTE[name]]) + operand.to_bytes(2, "little")

    def build(self):
        for offset, label, kind in self.fixups:
            target = self.labels[label]
            if kind == "branch":
                distance = target - (ORIGIN + offset + 1)
                if not -128 <= distance <= 127:
                    raise SystemExit(f"branch to {label} is {distance} bytes")
                self.code[offset] = distance & 0xFF
            else:
                self.code[offset:offset + 2] = target.to_bytes(2, "little")
        return bytes(self.code)


def program():
    a = Assembler()
    op = a.op
    # The SID's POT lines read the control port CIA 1's port A bits 6-7 select ($DC00): port 1 with
    # bit 6 alone. The KERNAL's keyboard scan rewrites $DC00 every jiffy, so its interrupt goes off,
    # as a 1351 program (GEOS) stops it, and the port stays selected.
    op("sei")
    op("lda", 0x7F, "#")
    op("sta", 0xDC00)
    # The pointer: a 4 x 4 block at the sprite's top left, its hotspot.
    op("ldx", 63, "#")
    op("lda", 0, "#")
    a.label("clear")
    op("sta", SPRITE, "x")
    op("dex")
    op("bpl", "clear")
    op("lda", 0xF0, "#")
    for row in range(4):
        op("sta", SPRITE + row * 3)
    op("lda", SPRITE // 64, "#")
    op("sta", 0x07F8)
    op("lda", 1, "#")
    op("sta", 0xD015)  # sprite 0 on
    op("sta", 0xD027)  # white
    op("lda", 0, "#")
    op("sta", 0xD010)
    op("sta", XHI)
    op("lda", 0x10, "#")
    op("sta", BUTTON)  # released
    op("lda", 160, "#")
    op("sta", XLO)
    op("lda", 100, "#")
    op("sta", Y)
    for pot, old in ((0xD419, OX), (0xD41A, OY)):
        op("lda", pot)
        op("lsr")
        op("and", 0x3F, "#")
        op("sta", old)

    a.label("frame")
    # Once a frame: wait for raster line 250, below the screen.
    op("lda", 0xD012)
    op("cmp", 250, "#")
    op("bne", "frame")

    def delta(pot, old, positive):
        # A = the signed motion since the last frame: the 6-bit position's change, -32 to 31.
        op("lda", pot)
        op("lsr")
        op("and", 0x3F, "#")
        op("tax")
        op("sec")
        op("sbc", old)
        op("and", 0x3F, "#")
        op("cmp", 0x20, "#")
        op("bcc", positive)
        op("ora", 0xC0, "#")
        a.label(positive)
        op("stx", old)
        op("sta", T)

    # X: x += the motion, within 0 to 319.
    delta(0xD419, OX, "x_delta")
    op("clc")
    op("adc", XLO)
    op("sta", XLO)
    op("lda", T)
    op("bmi", "x_negative")
    op("lda", 0, "#")
    op("jmp", "x_add")
    a.label("x_negative")
    op("lda", 0xFF, "#")
    a.label("x_add")
    op("adc", XHI)
    op("sta", XHI)
    op("bpl", "x_not_below")
    op("lda", 0, "#")
    op("sta", XLO)
    op("sta", XHI)
    op("jmp", "x_done")
    a.label("x_not_below")
    op("beq", "x_done")  # XHI 0: 0 to 255
    op("cmp", 1, "#")
    op("bne", "x_clamp")
    op("lda", XLO)
    op("cmp", 0x40, "#")
    op("bcc", "x_done")  # 256 to 319
    a.label("x_clamp")
    op("lda", 1, "#")
    op("sta", XHI)
    op("lda", 0x3F, "#")
    op("sta", XLO)
    a.label("x_done")

    # Y: the 1351's Y counts up as it moves away from the player, so y -= the motion, within 0 to 199.
    delta(0xD41A, OY, "y_delta")
    op("lda", T)
    op("bmi", "y_down")
    op("lda", Y)
    op("sec")
    op("sbc", T)
    op("bcs", "y_store")
    op("lda", 0, "#")
    op("jmp", "y_store")
    a.label("y_down")
    op("lda", 0, "#")
    op("sec")
    op("sbc", T)
    op("clc")
    op("adc", Y)
    op("cmp", 200, "#")
    op("bcc", "y_store")
    op("lda", 199, "#")
    a.label("y_store")
    op("sta", Y)

    # The sprite: the screen's top left is sprite position (24, 50).
    op("lda", XLO)
    op("clc")
    op("adc", 24, "#")
    op("sta", 0xD000)
    op("lda", XHI)
    op("adc", 0, "#")
    op("and", 1, "#")
    op("sta", 0xD010)
    op("lda", Y)
    op("clc")
    op("adc", 50, "#")
    op("sta", 0xD001)

    # The left button: on a press, the background colour 1 + x/32 + y/32 (never the default blue).
    op("lda", 0xDC01)
    op("and", 0x10, "#")
    op("cmp", BUTTON)
    op("beq", "next")
    op("sta", BUTTON)
    op("cmp", 0, "#")
    op("bne", "next")
    op("lda", XHI)
    op("lsr")
    op("lda", XLO)
    op("ror")
    for _ in range(4):
        op("lsr")
    op("sta", T)
    op("lda", Y)
    for _ in range(5):
        op("lsr")
    op("sec")  # + 1
    op("adc", T)
    op("and", 0x0F, "#")
    op("sta", 0xD021)
    a.label("next")
    op("jmp", "frame")
    return a.build()


def prg():
    # 10 SYS2061, then the code at $080D.
    basic = bytes([0x0B, 0x08, 0x0A, 0x00, 0x9E]) + b"2061" + bytes([0x00, 0x00, 0x00])
    if 0x0801 + len(basic) != ORIGIN:
        raise SystemExit(f"the code is at {0x0801 + len(basic):04X}, not {ORIGIN:04X}")
    return (0x0801).to_bytes(2, "little") + basic + program()


def main():
    target = ROOT / "core/test/c64/mouse.prg"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(prg())
    print(f"{target}: {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
