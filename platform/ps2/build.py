#!/usr/bin/env python3
"""Build the game-only PS2 ELF with the official ps2dev CMake toolchain."""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audio", choices=("OFF", "RFAUDS2"), default="OFF")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 8))
    args = parser.parse_args()
    ps2dev = Path(os.environ.get("PS2DEV", ROOT / ".ps2dev")).resolve()
    sdk = Path(os.environ.get("PS2SDK", ps2dev / "ps2sdk")).resolve()
    toolchain = ps2dev / "share/ps2dev.cmake"
    if not toolchain.is_file():
        raise SystemExit("Missing PS2DEV toolchain. Run bootstrap.py and source .ps2deps/env.sh")
    config = sdk / "ports/include/SDL2/SDL_config.h"
    if not config.is_file() or not re.search(r"^#define SDL_AUDIO_DISABLED 1$", config.read_text(), re.M):
        raise SystemExit("Rebuild SDL2 with SDL_AUDIO=OFF (bootstrap.py). Its PS2 audio driver loads audsrv.")
    env = os.environ.copy()
    env.update(PS2DEV=str(ps2dev), PS2SDK=str(sdk), GSKIT=str(ps2dev / "gsKit"))
    env["PATH"] = ":".join(str(ps2dev / d) for d in ("ee/bin", "iop/bin", "bin")) + ":" + env["PATH"]
    output = (args.build_dir or ROOT / "out" / ("ps2" if args.audio == "OFF" else "ps2-rfauds2")).resolve()
    command = ["cmake", "-S", str(ROOT), "-B", str(output),
               f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", "-DCDOGS_PS2=ON",
               f"-DCDOGS_PS2_AUDIO={args.audio}", "-DCMAKE_BUILD_TYPE=Release",
               "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"]
    if args.audio == "RFAUDS2":
        rfa = Path(env.get("RFAUDS2_ROOT", ROOT / ".ps2deps/rfauds2")).resolve()
        command.append(f"-DRFAUDS2_ROOT={rfa}")
    subprocess.run(command, env=env, check=True)
    subprocess.run(["cmake", "--build", str(output), "--parallel", str(args.jobs)], env=env, check=True)
    elf = output / "cdogs-sdl.elf"
    header = elf.read_bytes()[:52]
    if len(header) != 52 or header[:6] != b"\x7fELF\x01\x01":
        raise SystemExit("Build produced no valid ELF")
    if int.from_bytes(header[18:20], "little") != 8 or \
            int.from_bytes(header[36:40], "little") & 0x00FF0000 != 0x00920000:
        raise SystemExit("ELF is not a PS2 R5900 executable")
    nm = subprocess.check_output([str(ps2dev / "ee/bin/mips64r5900el-ps2-elf-nm"), str(elf)], text=True)
    if re.search(r"\baudsrv\w*\b", nm, re.I):
        raise SystemExit("Unexpected audsrv symbols in ELF")
    for entry in ("main", "SDL_main", "CDogsMain", "CDogsPS2InitPaths"):
        if not re.search(r"\b[TW] " + entry + r"$", nm, re.M):
            raise SystemExit(f"Missing startup entry {entry}; SDL2main/IOP/filesystem must not be bypassed")
    print(f"Built {elf} ({elf.stat().st_size} bytes), no audsrv symbols")


if __name__ == "__main__":
    main()
