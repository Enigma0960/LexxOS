#!/usr/bin/env python3
"""Install the opt-in splash port into a clean Rockchip U-Boot source tree."""
import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uboot", type=Path)
    parser.add_argument("--check", action="store_true", help="check without modifying files")
    args = parser.parse_args()
    root = args.uboot.resolve()
    here = Path(__file__).resolve().parent
    target = root / "board/rockchip/evb_rk3506/picocalc_splash.c"
    if target.exists():
        parser.error(f"Refusing to overwrite {target}; port may already be installed")
    if not target.parent.is_dir():
        parser.error("Expected Rockchip U-Boot with board/rockchip/evb_rk3506")
    patch = here / "integration.patch"
    subprocess.run(["git", "apply", "--check", str(patch)], cwd=root, check=True)
    if args.check:
        print("Integration patch applies; no files changed")
        return
    subprocess.run(["git", "apply", str(patch)], cwd=root, check=True)
    try:
        shutil.copyfile(here / "picocalc_splash.c", target)
    except OSError:
        subprocess.run(["git", "apply", "--reverse", str(patch)], cwd=root, check=True)
        raise
    print("Port installed. Enable CONFIG_PICOCALC_LYRA_SPLASH=y in the board config.")


if __name__ == "__main__":
    main()
