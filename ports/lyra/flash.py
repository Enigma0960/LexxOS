#!/usr/bin/env python3
"""macOS whole-card installation: build, confirm, back up, write and read back."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import build as image_builder
import flash_rk3506_macos as disk_tools

UnsafeTarget = disk_tools.UnsafeTarget


def physical_disks():
    listing = disk_tools.plist_command("diskutil", "list", "-plist", "physical")
    return ["/dev/" + item["DeviceIdentifier"]
            for item in listing.get("AllDisksAndPartitions", [])
            if re.fullmatch(r"disk[0-9]+", item.get("DeviceIdentifier", ""))]


def check_disk(device):
    if not disk_tools.DISK_RE.fullmatch(device) or device == "/dev/disk0":
        raise UnsafeTarget("select a whole removable disk other than disk0")
    if device not in physical_disks():
        raise UnsafeTarget("not a physical disk")
    info = disk_tools.disk_info(device)
    name = device.removeprefix("/dev/")
    if (info.get("DeviceIdentifier") != name or info.get("WholeDisk") is not True
            or info.get("RemovableMedia") is not True or info.get("Writable") is not True
            or info.get("DeviceBlockSize") != 512):
        raise UnsafeTarget("expected writable removable whole media with 512-byte sectors")
    # Also protect macOS booted from external/removable media.
    root = disk_tools.disk_info("/")
    stores = root.get("APFSPhysicalStores", [])
    if root.get("APFSContainerReference") and not stores:
        raise UnsafeTarget("cannot resolve physical stores of the running APFS system")
    identifiers = [root.get("ParentWholeDisk", ""), root.get("DeviceIdentifier", "")]
    identifiers += [store.get("APFSPhysicalStore", "") for store in stores]
    if not any(identifiers):
        raise UnsafeTarget("cannot identify the running system disk")
    if any(re.match(r"^" + re.escape(name) + r"(?:s|$)", item) for item in identifiers):
        raise UnsafeTarget("the selected disk contains the running system")
    size = info.get("Size", 0)
    if not isinstance(size, int) or size % 512:
        raise UnsafeTarget("invalid media size")
    image_builder.gpt(size // 512)  # Enforce supported geometry before building.
    return {"disk": device, "raw": "/dev/r" + name, "size": size,
            "model": info.get("MediaName", "SD card"),
            "identity": [info.get(key) for key in
                         ("DeviceTreePath", "BusProtocol", "MediaName", "DiskUUID")]}


def select_disk(requested=None):
    if requested:
        return check_disk(requested)
    candidates = []
    for device in physical_disks():
        try:
            candidates.append(check_disk(device))
        except (UnsafeTarget, ValueError, subprocess.CalledProcessError):
            continue
    if not candidates:
        raise UnsafeTarget("no writable removable SD cards found (blank cards are supported)")
    print("Removable cards for FULL ERASE / installation:")
    for index, card in enumerate(candidates, 1):
        print(f"  {index}. {card['disk']} {card['model']}, {card['size']} bytes")
    answer = input("Choose card number (Enter cancels): ").strip()
    if not answer.isdecimal() or not 1 <= int(answer) <= len(candidates):
        raise UnsafeTarget("cancelled")
    selected = candidates[int(answer) - 1]
    same_card(selected)
    return selected


def same_card(target):
    if check_disk(target["disk"]) != target:
        raise UnsafeTarget("card changed; refusing whole-disk write")


def read_disk(raw, size, destination=None):
    """Stream a complete device, optionally saving an exclusive, durable backup."""
    digest = hashlib.sha256()
    total = 0
    output = destination.open("xb") if destination else None
    try:
        with subprocess.Popen(["sudo", "dd", f"if={raw}", "bs=1m"], stdout=subprocess.PIPE) as proc:
            try:
                while chunk := proc.stdout.read(4 * image_builder.MIB):
                    total += len(chunk)
                    if total > size:
                        raise UnsafeTarget("media size changed while reading")
                    digest.update(chunk)
                    if output:
                        output.write(chunk)
            except BaseException:
                proc.kill()
                raise
            if proc.wait() != 0 or total != size:
                raise UnsafeTarget("full-card backup/readback incomplete")
        if output:
            output.flush()
            os.fsync(output.fileno())
    finally:
        if output:
            output.close()
    return digest.hexdigest()


def flash(image, backup_dir, target):
    image_builder.verify(image)
    manifest = json.loads(image.with_suffix(".json").read_text())
    if image.stat().st_size != target["size"] or manifest["size"] != target["size"]:
        raise UnsafeTarget("image must exactly match selected media size")
    print(f"Selected: {target['disk']}; {target['model']}; {target['size']} bytes")
    print(f"Complete image: {image}; SHA-256 {manifest['sha256']}")
    print("ERASES THE ENTIRE CARD: GPT, DDR/SPL, U-Boot, boot and rootfs/data.")
    print(f"Rootfs: {manifest['rootfs_policy']}. Full backup will be retained in {backup_dir}.")
    token = f"ERASE {target['disk'].removeprefix('/dev/')} ALL"
    if input(f"Type '{token}' to proceed: ").strip() != token:
        raise UnsafeTarget("confirmation not matched; nothing written")
    same_card(target)
    backup_dir.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(backup_dir).free < target["size"] + 64 * image_builder.MIB:
        raise UnsafeTarget("not enough free space for mandatory full-card backup")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    backup = backup_dir / f"sd-{target['disk'].removeprefix('/dev/')}-{stamp}.img"
    disk_tools.command("sudo", "-v", capture=False)
    disk_tools.command("diskutil", "unmountDisk", target["disk"], capture=False)
    same_card(target)
    print(f"Backing up the ENTIRE card: {backup}. Ctrl+T shows dd progress.")
    original_hash = read_disk(target["raw"], target["size"], backup)
    image_builder.atomic_write(backup.with_suffix(".img.sha256"),
                               f"{original_hash}  {backup.name}\n".encode())
    same_card(target)
    # The private staging image cannot be replaced by another CMake build.
    image_builder.verify(image)
    same_card(target)
    print("Writing complete image (including zeroed unused areas) ...")
    disk_tools.command("sudo", "dd", f"if={image}", f"of={target['raw']}", "bs=1m", capture=False)
    disk_tools.command("sync", capture=False)
    # Partition UUIDs change deliberately. Compare the complete device contents instead.
    print("Reading back the ENTIRE card ...")
    actual = read_disk(target["raw"], target["size"])
    if actual != manifest["sha256"]:
        raise UnsafeTarget(f"READBACK MISMATCH. Full backup: {backup}")
    disk_tools.command("diskutil", "eject", target["disk"], capture=False)
    print(f"Full-card SHA-256 verified. Card ejected. Backup: {backup}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path)
    parser.add_argument("--uboot", type=Path)
    parser.add_argument("--rootfs", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--backup-dir", type=Path)
    parser.add_argument("--disk")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("this flasher requires macOS")
    try:
        if args.list:
            for device in physical_disks():
                try:
                    card = check_disk(device)
                    print(f"{device} {card['size']} bytes {card['model']}")
                except (UnsafeTarget, ValueError, subprocess.CalledProcessError):
                    continue
            return 0
        if not all([args.kernel, args.uboot, args.build_dir, args.backup_dir]):
            parser.error("--kernel, --uboot, --build-dir and --backup-dir are required")
        target = select_disk(args.disk)
        args.build_dir.mkdir(parents=True, exist_ok=True)
        stage = Path(tempfile.mkdtemp(prefix="flash-", dir=args.build_dir))
        image = stage / "lexxos-sd.img"
        image_builder.build(args.kernel, args.uboot, image, rootfs=args.rootfs, size_bytes=target["size"])
        flash(image, args.backup_dir, target)
        return 0
    except (UnsafeTarget, ValueError, OSError, subprocess.CalledProcessError) as exc:
        print(f"Full SD flash stopped: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
