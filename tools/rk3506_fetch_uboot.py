#!/usr/bin/env python3
"""Fetch a pinned Rockchip U-Boot checkout and install the PicoCalc splash port."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
URL = "https://github.com/rockchip-linux/u-boot.git"
REVISION = "1c535d65b8509f388d09e49fb6961f49fda35a1d"


def run(*args, cwd):
    subprocess.run(args, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path,
                        default=ROOT / "build/third_party/rk3506-u-boot")
    args = parser.parse_args()
    dest = args.destination.resolve()
    if dest.exists():
        parser.error(f"Destination already exists: {dest}; refusing to reset local changes")
    dest.mkdir(parents=True)
    run("git", "init", cwd=dest)
    run("git", "remote", "add", "origin", URL, cwd=dest)
    run("git", "fetch", "--depth=1", "origin", REVISION, cwd=dest)
    run("git", "checkout", "--detach", "FETCH_HEAD", cwd=dest)
    actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=dest, text=True).strip()
    if actual != REVISION:
        raise RuntimeError(f"Unexpected revision: {actual}")
    run(sys.executable, str(ROOT / "ports/uboot/picocalc_lyra/install.py"), str(dest), cwd=ROOT)
    print(f"Prepared {dest}\nRevision: {REVISION}")
    print("This is an integration baseline, not a factory-matched flashable image.")


if __name__ == "__main__":
    main()
