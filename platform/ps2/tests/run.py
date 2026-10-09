#!/usr/bin/env python3
"""Native regression tests for the PS2 adapter; these do not emulate PS2."""
import argparse
import importlib.util
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[3]
PS2 = ROOT / "platform/ps2"


def pcm(path, frames, rate=48000):
    with wave.open(str(path), "wb") as f:
        f.setnchannels(2); f.setsampwidth(2); f.setframerate(rate)
        f.writeframes(struct.pack("<hh", -3000, 7000) * frames)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rfa-root", type=Path, required=True)
    parser.add_argument("--sdl-prefix", type=Path)
    parser.add_argument("--require-iso", action="store_true", help="Fail unless pycdlib/ISO regressions are available")
    args = parser.parse_args()
    rfa = args.rfa_root.resolve()
    sdl_config = str(args.sdl_prefix / "bin/sdl2-config") if args.sdl_prefix else shutil.which("sdl2-config")
    if not sdl_config:
        raise SystemExit("Install native SDL2 development or supply --sdl-prefix")
    cflags = shlex.split(subprocess.check_output([sdl_config, "--cflags"], text=True))
    libs = shlex.split(subprocess.check_output([sdl_config, "--libs"], text=True))
    with tempfile.TemporaryDirectory(prefix="cdogs-ps2-tests-") as temporary:
        temp = Path(temporary)
        config = (ROOT / "src/cdogs/sys_config.h.cmake").read_text()
        config = config.replace("@VERSION@", "test").replace("@CDOGS_DATA_DIR@", "./").replace("@CDOGS_CFG_DIR@", "config/")
        (temp / "sys_config.h").write_text(config)
        common = [os.environ.get("CC", "cc"), "-std=gnu99", "-O2", "-Wall", "-Wextra",
                  "-Wno-unused-parameter", "-ffunction-sections", "-fdata-sections", "-DCDOGS_PS2",
                  "-I" + str(PS2 / "include"), "-I" + str(temp), "-I" + str(ROOT / "src"),
                  "-I" + str(ROOT / "src/cdogs"), *cflags]
        platform_sources = [PS2 / "tests/platform_test.c", PS2 / "platform.c", PS2 / "window_context.c",
                            PS2 / "posix_paths.c", ROOT / "src/cdogs/c_array.c", ROOT / "src/cdogs/vector.c",
                            ROOT / "src/cdogs/color.c", ROOT / "src/cdogs/texture.c",
                            ROOT / "src/cdogs/mathc/mathc.c"]
        subprocess.run([*common, "-Dbasename=PS2Basename", "-Ddirname=PS2Dirname",
                        *map(str, platform_sources), "-Wl,--gc-sections", "-Wl,--wrap=getcwd",
                        *libs, "-lm", "-o", str(temp / "platform-test")], check=True)
        env = os.environ.copy(); env["SDL_VIDEODRIVER"] = "dummy"
        subprocess.run([str(temp / "platform-test")], cwd=ROOT, env=env, check=True)
        subprocess.run([*common, str(PS2 / "tests/rwops_test.c"), str(PS2 / "rwops.c"),
                        "-Wl,--gc-sections", "-Wl,--wrap=SDL_RWFromFile", "-Wl,--export-dynamic",
                        *libs, "-ldl", "-lm", "-o", str(temp / "rwops-test")], check=True)
        subprocess.run([str(temp / "rwops-test"), str(ROOT / "graphics/font.png")], check=True)
        pcm(temp / "music.ogg.pcm", 2107)
        pcm(temp / "wrong-rate.wav", 10, 44100)
        pcm(temp / "truncated.wav", 10)
        (temp / "truncated.wav").write_bytes((temp / "truncated.wav").read_bytes()[:-8])
        pcm(temp / "oversize.wav", 1048577)
        subprocess.run([*common, "-DCDOGS_PS2_RFAUDS2", "-I" + str(rfa / "include"),
                        "-I" + str(rfa / "tests/include"), str(PS2 / "tests/mixer_test.c"),
                        str(PS2 / "mixer.c"), str(rfa / "src/ee/core.c"), *libs, "-lm",
                        "-o", str(temp / "mixer-test")], check=True)
        subprocess.run([str(temp / "mixer-test"), str(temp)], check=True)
        spec = importlib.util.spec_from_file_location("stage", PS2 / "stage.py")
        stage = importlib.util.module_from_spec(spec); spec.loader.exec_module(stage)
        # Real ffmpeg round-trip for WAV and a compressed file, plus a tracker
        # from the game. Exercise placeholders, skip source art, deterministic ZIP.
        if shutil.which("ffmpeg"):
            fixture = temp / "source"; fixture.mkdir()
            for directory in stage.ASSETS:
                (fixture / directory).mkdir()
            (fixture / "doc").mkdir(); (fixture / "platform/ps2").mkdir(parents=True)
            shutil.copy2(PS2 / "README.md", fixture / "platform/ps2/README.md")
            shutil.copy2(PS2 / "deps.lock.json", fixture / "platform/ps2/deps.lock.json")
            (fixture / "data/guns.json").write_text("{}\n")
            (fixture / "graphics/font.png").write_bytes(b"\x89PNG\r\n\x1a\n")
            (fixture / "graphics/table_wood_round_terminal_wreck.png").write_bytes(b"full filename")
            deep = fixture / "missions/A.cdogscpn/graphics/enemies/collection/direction/frames"
            deep.mkdir(parents=True)
            (deep / "body (old)-red.png").write_bytes(b"depth and punctuation")
            pcm(fixture / "sounds/tone.wav", 100)
            subprocess.run(["ffmpeg", "-v", "error", "-i", str(fixture / "sounds/tone.wav"),
                            str(fixture / "sounds/tone.ogg")], check=True)
            tracker = next((p for p in (ROOT / "missions").rglob("*")
                            if p.suffix.lower() in {".xm", ".it", ".mod", ".s3m"}), None)
            if tracker:
                shutil.copy2(tracker, fixture / "music" / tracker.name)
            (fixture / "graphics/source.blend").write_text("unused source art")
            elf = temp / "test.elf"
            header = bytearray(64); header[:6] = b"\x7fELF\x01\x01"
            struct.pack_into("<H", header, 18, 8)
            struct.pack_into("<I", header, 36, 0x20920020)
            elf.write_bytes(header)
            package = temp / "package"
            stage.stage(fixture, elf, package, True, rfa)
            assert (package / "sounds/tone.ogg").stat().st_size == 0
            assert (package / "sounds/tone.ogg.pcm").is_file()
            assert not (package / "graphics/source.blend").exists()
            stage.archive(package, temp / "a.zip"); stage.archive(package, temp / "b.zip")
            assert (temp / "a.zip").read_bytes() == (temp / "b.zip").read_bytes()
            try:
                stage.stage(fixture, elf, fixture, False)
            except ValueError:
                pass
            else:
                raise AssertionError("source tree overwrite accepted")
            print("Audio staging and deterministic ZIP OK")
            if importlib.util.find_spec("pycdlib"):
                spec = importlib.util.spec_from_file_location("disc", PS2 / "disc.py")
                disc = importlib.util.module_from_spec(spec); spec.loader.exec_module(disc)
                disc.build_iso(package, temp / "a.iso")
                disc.build_iso(package, temp / "b.iso")
                assert disc.hash_file(temp / "a.iso") == disc.hash_file(temp / "b.iso")
                try:
                    disc.build_iso(package, package / "bad.iso")
                except ValueError:
                    pass
                else:
                    raise AssertionError("ISO inside package accepted")
                (package / "data/GUNS.JSON").write_text("case collision")
                try:
                    disc.package_entries(package)
                except ValueError:
                    pass
                else:
                    raise AssertionError("CDFS case collision accepted")
                print("Bootable ISO, full Joliet names, depth, hashes, determinism and guards OK")
            elif args.require_iso:
                raise RuntimeError("Install platform/ps2/requirements-iso.txt for ISO regressions")
            else:
                print("ISO tests skipped (install platform/ps2/requirements-iso.txt to enable)")


if __name__ == "__main__":
    main()
