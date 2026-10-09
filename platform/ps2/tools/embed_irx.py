#!/usr/bin/env python3
"""Embed an IRX without depending on a host-specific bin2c executable."""
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:3])
symbol = sys.argv[3] if len(sys.argv) == 4 else "rfauds2_irx"
if not symbol.isidentifier() or not symbol.isascii():
    raise SystemExit("Invalid embedded module symbol")
data = source.read_bytes()
if not data.startswith(b"\x7fELF"):
    raise SystemExit("Module is not an ELF/IRX")
target.parent.mkdir(parents=True, exist_ok=True)
lines = [f"const unsigned char {symbol}[] __attribute__((aligned(64))) = {{"]
lines += ["  " + ",".join(f"0x{x:02x}" for x in data[i:i + 16]) + ","
          for i in range(0, len(data), 16)]
lines += ["};", f"const unsigned int {symbol}_size = sizeof {symbol};"]
target.write_text("\n".join(lines) + "\n")
