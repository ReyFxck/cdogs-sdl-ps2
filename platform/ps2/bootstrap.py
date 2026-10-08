#!/usr/bin/env python3
"""Install the locked ps2dev bundle; build SDL without audio and RFAuds2."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
LOCK = json.loads((ROOT / "platform/ps2/deps.lock.json").read_text())


def run(*args, **kwargs):
    print("+", shlex.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, **kwargs)


def checkout(name, deps):
    pin = LOCK[name]
    path = deps / name
    if not path.exists():
        run("git", "init", path)
        run("git", "-C", path, "remote", "add", "origin", pin["url"])
    # Never reset a caller's modified dependency checkout.
    dirty = subprocess.check_output(["git", "-C", str(path), "status", "--porcelain"])
    if dirty:
        raise SystemExit(f"Modified dependency: {path}; keep it in a separate checkout")
    current = subprocess.run(["git", "-C", str(path), "rev-parse", "HEAD"],
                             capture_output=True, text=True)
    if current.returncode or current.stdout.strip() != pin["commit"]:
        run("git", "-C", path, "fetch", "--depth=1", "origin", pin["commit"])
        run("git", "-C", path, "checkout", "--detach", "FETCH_HEAD")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ps2dev", type=Path, default=ROOT / ".ps2dev")
    parser.add_argument("--deps", type=Path, default=ROOT / ".ps2deps")
    parser.add_argument("--archive", type=Path, help="Previously downloaded locked archive")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 8))
    args = parser.parse_args()
    ps2dev, deps = args.ps2dev.resolve(), args.deps.resolve()
    deps.mkdir(parents=True, exist_ok=True)
    if not (ps2dev / "share/ps2dev.cmake").is_file():
        if platform.system() != "Linux" or platform.machine() not in ("x86_64", "amd64"):
            raise SystemExit("Automatic toolchain install requires Linux x86_64; supply an installed --ps2dev")
        archive = args.archive or deps / "ps2dev-ubuntu-latest.tar.gz"
        if not archive.is_file():
            temporary = archive.with_suffix(".part")
            print("Downloading locked ps2dev bundle...", flush=True)
            urllib.request.urlretrieve(LOCK["toolchain"]["url"], temporary)
            temporary.rename(archive)
        digest = hashlib.file_digest(archive.open("rb"), "sha256").hexdigest()
        if digest != LOCK["toolchain"]["sha256"]:
            raise SystemExit("ps2dev archive SHA256 mismatch. The rolling release changed; use the locked archive.")
        if ps2dev.exists() and any(ps2dev.iterdir()):
            raise SystemExit(f"Refusing to unpack over nonempty {ps2dev}")
        ps2dev.mkdir(parents=True, exist_ok=True)
        # Strip the bundle's single top-level directory; reject traversal/links.
        with tarfile.open(archive) as bundle:
            for member in bundle.getmembers():
                parts = Path(member.name).parts
                if len(parts) < 2:
                    continue
                member.name = str(Path(*parts[1:]))
                target = (ps2dev / member.name).resolve()
                if not target.is_relative_to(ps2dev):
                    raise SystemExit("Unsafe toolchain archive path")
                if member.issym():
                    link = (target.parent / member.linkname).resolve()
                    if not link.is_relative_to(ps2dev):
                        raise SystemExit("Unsafe toolchain archive link")
                elif member.islnk():
                    member.linkname = str(Path(*Path(member.linkname).parts[1:]))
                    if not (ps2dev / member.linkname).resolve().is_relative_to(ps2dev):
                        raise SystemExit("Unsafe toolchain archive hard link")
                bundle.extract(member, ps2dev)
    env = os.environ.copy()
    env["PS2DEV"] = str(ps2dev)
    env["PS2SDK"] = str(ps2dev / "ps2sdk")
    env["GSKIT"] = str(ps2dev / "gsKit")
    env["PATH"] = ":".join(str(ps2dev / d) for d in ("ee/bin", "iop/bin", "bin")) + ":" + env["PATH"]
    sdl = checkout("sdl", deps)
    sdk = checkout("ps2sdk", deps)
    rfa = checkout("rfauds2", deps)
    env["PS2SDKSRC"] = str(sdk)
    run("cmake", "-S", sdl, "-B", deps / "sdl-build",
        f"-DCMAKE_TOOLCHAIN_FILE={ps2dev}/share/ps2dev.cmake",
        f"-DCMAKE_INSTALL_PREFIX={env['PS2SDK']}/ports", "-DCMAKE_BUILD_TYPE=Release",
        "-DSDL_AUDIO=OFF", "-DSDL_SHARED=OFF", "-DSDL_STATIC=ON", "-DSDL_TESTS=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=OFF", env=env)
    run("cmake", "--build", deps / "sdl-build", "--parallel", args.jobs, env=env)
    run("cmake", "--install", deps / "sdl-build", env=env)
    run("make", "-C", rfa, f"-j{args.jobs}", "check", env=env)
    variables = {key: env[key] for key in ("PS2DEV", "PS2SDK", "PS2SDKSRC", "GSKIT")}
    variables["RFAUDS2_ROOT"] = str(rfa)
    exports = "".join(f"export {key}={shlex.quote(value)}\n" for key, value in variables.items())
    exports += 'export PATH="$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/bin:$PATH"\n'
    (deps / "env.sh").write_text(exports)
    (deps / "resolved-deps.json").write_text(json.dumps(LOCK, indent=2) + "\n")
    print(f"Ready. Run: . {shlex.quote(str(deps / 'env.sh'))}")


if __name__ == "__main__":
    main()
