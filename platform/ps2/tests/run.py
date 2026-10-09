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
    parser.add_argument("--ps2sdk-source", type=Path, default=ROOT / ".ps2deps/ps2sdk",
                        help="SDK checkout for real CDFS parser regression tests")
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
        subprocess.run([*common, str(PS2 / "tests/directory_test.c"), str(PS2 / "directory.c"),
                        "-o", str(temp / "directory-test")], check=True)
        subprocess.run([str(temp / "directory-test")], cwd=ROOT, check=True)
        subprocess.run([*common, str(PS2 / "tests/rwops_test.c"), str(PS2 / "rwops.c"),
                        "-Wl,--gc-sections", "-Wl,--wrap=SDL_RWFromFile", "-Wl,--export-dynamic",
                        *libs, "-ldl", "-lm", "-o", str(temp / "rwops-test")], check=True)
        subprocess.run([str(temp / "rwops-test"), str(ROOT / "graphics/font.png")], check=True)
        graphics_sources = [PS2 / "tests/graphics_memory_test.c", PS2 / "platform.c", PS2 / "pic_texture.c",
                            PS2 / "graphics_pack.c",
                            *[ROOT / "src/cdogs" / name for name in
                              ("pic.c", "pic_manager.c", "font.c", "utils.c", "cpic.c", "blit.c",
                               "c_array.c", "color.c", "vector.c", "texture.c",
                               "c_hashmap/hashmap.c", "mathc/mathc.c")]]
        subprocess.run([*common, "-I" + str(ROOT / "src/proto/nanopb"),
                        *map(str, graphics_sources), "-Wl,--gc-sections",
                        "-Wl,--wrap=malloc", "-Wl,--wrap=calloc", "-Wl,--wrap=realloc",
                        "-Wl,--wrap=free", "-Wl,--wrap=LoadImgToSurface",
                        *libs, "-lm", "-o", str(temp / "graphics-memory-test")], check=True)
        png_count = sum(1 for p in (ROOT / "graphics").rglob("*") if p.suffix.lower() == ".png")
        subprocess.run([str(temp / "graphics-memory-test"), str(ROOT), str(png_count)],
                       cwd=ROOT, env=env, check=True)
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
        pack_a = temp / "pack-a"; pack_b = temp / "pack-b"
        pack_a.mkdir(); pack_b.mkdir()
        stage.pack_graphics(ROOT, pack_a); stage.pack_graphics(ROOT, pack_b)
        assert (pack_a / stage.GRAPHICS_PACK).read_bytes() == (pack_b / stage.GRAPHICS_PACK).read_bytes()
        (pack_a / "graphics").mkdir()
        shutil.copy2(ROOT / "graphics/font.png", pack_a / "graphics/font.png")
        (pack_a / "data").mkdir()
        shutil.copy2(ROOT / "data/guns.json", pack_a / "data/guns.json")
        subprocess.run([str(temp / "graphics-memory-test"), str(pack_a), str(png_count), "pack"],
                       cwd=ROOT, env=env, check=True)
        subprocess.run([*common, str(PS2 / "tests/graphics_pack_test.c"),
                        str(PS2 / "graphics_pack.c"), *libs, "-lm",
                        "-o", str(temp / "graphics-pack-test")], check=True)
        subprocess.run([str(temp / "graphics-pack-test"), str(pack_a / stage.GRAPHICS_PACK),
                        str(png_count)], check=True)
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
            # Stock CDFS truncates this directory at 256; the game-local module
            # must enumerate and read every file, not merely find known names.
            for i in range(300):
                (fixture / "graphics" / f"entry-{i:03d}.dat").write_bytes(bytes([i % 256]))
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
                if (args.ps2sdk_source / "iop/cdvd/cdfs/src/cdfs_iop.c").is_file():
                    spec = importlib.util.spec_from_file_location("prepare_cdfs", PS2 / "tools/prepare_cdfs.py")
                    cdfs = importlib.util.module_from_spec(spec); spec.loader.exec_module(cdfs)
                    patched = temp / "cdfs"
                    cdfs.prepare(args.ps2sdk_source, patched)
                    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O2", "-Wall", "-Wextra",
                                    "-Wno-unused-parameter", "-D_POSIX_C_SOURCE=200809L",
                                    "-I" + str(PS2 / "tests/cdfs_stubs"), "-I" + str(patched),
                                    str(PS2 / "tests/cdfs_parser_test.c"), str(patched / "cdfs_iop.c"),
                                    "-o", str(temp / "cdfs-parser-test")], check=True)
                    count = sum(p.is_file() for p, _ in disc.package_entries(package)) + 1
                    subprocess.run([str(temp / "cdfs-parser-test"), str(temp / "a.iso"),
                                    "interfere", str(count), str(package)], check=True)
                elif args.require_iso:
                    raise RuntimeError("CDFS regression requires --ps2sdk-source (bootstrap.py provides it)")
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
                (package / "data/GUNS.JSON").unlink()
                oversized = package / "data/oversized"
                oversized.mkdir()
                for i in range(512):
                    (oversized / str(i)).touch()
                try:
                    disc.package_entries(package)
                except ValueError as e:
                    assert "entry limit" in str(e)
                else:
                    raise AssertionError("CDFS directory overflow accepted")
                print("Bootable ISO, full Joliet names, depth, hashes, determinism and guards OK")
            elif args.require_iso:
                raise RuntimeError("Install platform/ps2/requirements-iso.txt for ISO regressions")
            else:
                print("ISO tests skipped (install platform/ps2/requirements-iso.txt to enable)")


if __name__ == "__main__":
    main()
