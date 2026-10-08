#!/usr/bin/env python3
"""Embed an IRX without depending on a host-specific bin2c executable."""
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:])
data = source.read_bytes()
if not data.startswith(b"\x7fELF"):
    raise SystemExit("RFAuds2 module is not an ELF/IRX")
target.parent.mkdir(parents=True, exist_ok=True)
lines = ["const unsigned char rfauds2_irx[] __attribute__((aligned(64))) = {"]
lines += ["  " + ",".join(f"0x{x:02x}" for x in data[i:i + 16]) + ","
          for i in range(0, len(data), 16)]
lines += ["};", "const unsigned int rfauds2_irx_size = sizeof rfauds2_irx;"]
target.write_text("\n".join(lines) + "\n")
