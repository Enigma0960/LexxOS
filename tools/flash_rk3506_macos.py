#!/usr/bin/env python3
"""Explicit opt-in macOS flasher for the stock Luckfox Lyra SD-card GPT layout.

NEVER run as a build side effect. No whole-disk writes; only boot and optional
U-Boot partitions on removable media with the exact observed Luckfox partition GUIDs.
Fails closed when diskutil data is absent/inconsistent. Backups are mandatory.
"""

import argparse
import hashlib
import os
from pathlib import Path
import plistlib
import re
import struct
import subprocess
import sys
import tempfile

from datetime import datetime, timezone

from rk3506_pack_fit import FitError, parse_properties, get_prop

UBOOT_GUID = "C235E4D3-E8E7-4517-8256-3122254620A2"
BOOT_GUID = "B6E57622-0B69-46AF-AD2C-DADAF4E2E8CA"
ROOTFS_GUID = "5ED76E24-AE0D-4330-A411-71D190D57FA9"
BOOT_OFFSET = 8 * 1024 * 1024
BOOT_SIZE = 12 * 1024 * 1024
UBOOT_OFFSET = 4 * 1024 * 1024
UBOOT_SIZE = 4 * 1024 * 1024
ROOTFS_OFFSET = 32 * 1024 * 1024
DISK_RE = re.compile(r"^/dev/disk([0-9]+)$")


class UnsafeTarget(RuntimeError):
    pass


def command(*args: str, capture=True) -> bytes:
    proc = subprocess.run(args, check=True, stdout=subprocess.PIPE if capture else None)
    return proc.stdout if capture else b""


def plist_command(*args: str) -> dict:
    return plistlib.loads(command(*args))


def disk_info(device: str) -> dict:
    return plist_command("diskutil", "info", "-plist", device)


def content(info: dict) -> str:
    return str(info.get("Content", "")).upper()


def physical_disks() -> list[str]:
    listing = plist_command("diskutil", "list", "-plist")
    result = []
    for disk in listing.get("AllDisksAndPartitions", []):
        name = disk.get("DeviceIdentifier", "")
        if re.fullmatch(r"disk[0-9]+", name):
            result.append("/dev/" + name)
    return result


def check_disk(device: str) -> dict:
    """Only permit an explicitly recognizable physical, removable Lyra SD card."""
    match = DISK_RE.fullmatch(device)
    if not match:
        raise UnsafeTarget("specify a whole disk, e.g. /dev/disk4 (not a partition)")
    disk = match.group(0).removeprefix("/dev/")
    if disk == "disk0":
        raise UnsafeTarget("disk0 is always forbidden (system disk protection)")
    if device not in physical_disks():
        raise UnsafeTarget("device not found in the current physical-disk list")
    whole = disk_info(device)
    if whole.get("DeviceIdentifier") != disk or whole.get("WholeDisk") is not True:
        raise UnsafeTarget("diskutil did not confirm a whole disk")
    if whole.get("RemovableMedia") is not True:
        raise UnsafeTarget("only removable-media SD cards are allowed (not SSDs)")
    if whole.get("Writable") is not True:
        raise UnsafeTarget("media is not writable")
    if content(whole) != "GUID_PARTITION_SCHEME":
        raise UnsafeTarget("wrong disk partitioning; expected GUID_partition_scheme")
    if whole.get("DeviceBlockSize") != 512:
        raise UnsafeTarget("unexpected logical sector size")

    parts = [disk_info(f"/dev/{disk}s{i}") for i in (1, 2, 3)]
    expected = [
        (UBOOT_GUID, UBOOT_OFFSET, UBOOT_SIZE),
        (BOOT_GUID, BOOT_OFFSET, BOOT_SIZE),
        (ROOTFS_GUID, ROOTFS_OFFSET, None),
    ]
    for i, (part, (guid, offset, size)) in enumerate(zip(parts, expected), start=1):
        if part.get("DeviceIdentifier") != f"{disk}s{i}":
            raise UnsafeTarget(f"partition {i}: wrong device identifier")
        if part.get("ParentWholeDisk") != disk or part.get("WholeDisk") is not False:
            raise UnsafeTarget(f"partition {i}: wrong parent disk")
        if content(part) != guid:
            raise UnsafeTarget(f"partition {i}: unexpected partition GUID")
        if part.get("PartitionMapPartitionOffset") != offset:
            raise UnsafeTarget(f"partition {i}: incorrect starting offset")
        if size is not None and part.get("Size") != size:
            raise UnsafeTarget(f"partition {i}: incorrect size")
        if part.get("Writable") is not True:
            raise UnsafeTarget(f"partition {i}: not writable")
    if parts[2].get("Size", 0) < 32 * 1024 * 1024:
        raise UnsafeTarget("rootfs is unexpectedly small")
    if not parts[1].get("DiskUUID"):
        raise UnsafeTarget("boot partition has no stable GPT UUID")
    return {"disk": device, "boot": f"/dev/r{disk}s2", "uuid": parts[1]["DiskUUID"],
            "uboot": f"/dev/r{disk}s1", "uboot_uuid": parts[0].get("DiskUUID"),
            "size": whole.get("Size", 0), "model": whole.get("MediaName", "SD card")}


def eligible_disks() -> list[dict]:
    cards = []
    for device in physical_disks():
        try:
            cards.append(check_disk(device))
        except (UnsafeTarget, subprocess.CalledProcessError, ValueError, KeyError):
            continue
    return cards


def select_disk(requested: str | None) -> dict:
    if requested:
        return check_disk(requested)
    disks = eligible_disks()
    if not disks:
        raise UnsafeTarget("no removable Luckfox Lyra SD cards with exact GPT layout found")
    print("Eligible removable Luckfox Lyra SD cards:")
    for i, disk in enumerate(disks, start=1):
        print(f"  {i}. {disk['disk']} {disk['size']} bytes, boot GPT UUID {disk['uuid']}")
    answer = input("Choose card number (Enter cancels): ").strip()
    if not answer.isdecimal() or not 1 <= int(answer) <= len(disks):
        raise UnsafeTarget("cancelled")
    return check_disk(disks[int(answer) - 1]["disk"])


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def checked_image(path: Path) -> bytes:
    image = path.read_bytes()
    if not 4096 <= len(image) <= BOOT_SIZE:
        raise UnsafeTarget("image size is invalid for the 12-MiB boot partition")
    if image[:4] != bytes.fromhex("d00dfeed"):
        raise UnsafeTarget("not a FIT image (missing FDT magic)")
    return image


def checked_uboot(path: Path) -> bytes:
    image = path.read_bytes()
    if not 4096 <= len(image) <= UBOOT_SIZE:
        raise UnsafeTarget("U-Boot image must fit the 4-MiB partition")
    try:
        props = parse_properties(bytearray(image))
        candidates = [node for node in props if len(node) == 3 and node[1] == "images"]
        if not any(get_prop(image, props, node, "os").lower() == b"u-boot\0"
                   for node in candidates if "os" in props[node]):
            raise FitError("FIT has no U-Boot payload (os = u-boot)")
    except (ValueError, KeyError, IndexError, struct.error) as exc:
        raise UnsafeTarget(f"Invalid U-Boot FIT: {exc}") from exc
    return image


def update_uboot(image: bytes, backup_dir: Path, target: dict, stamp: str) -> bool:
    """Compare the actual card, preserve all redundant copies, and verify every byte."""
    if not target.get("uboot_uuid"):
        raise UnsafeTarget("U-Boot partition has no stable GPT UUID")

    def same_card():
        current = check_disk(target["disk"])
        if any(current.get(key) != target.get(key)
               for key in ("uuid", "boot", "uboot", "uboot_uuid")):
            raise UnsafeTarget("card changed before U-Boot access")

    # Deterministic full-partition representation: no stale trailing FIT copies.
    expected = image.ljust(UBOOT_SIZE, b"\0")
    same_card()
    with tempfile.TemporaryDirectory(prefix="lexxos-uboot-") as temp:
        before = Path(temp) / "before.img"
        sudo_read_partition(target["uboot"], before, UBOOT_SIZE)
        existing = before.read_bytes()
        if existing == expected:
            print("U-Boot unchanged on this card; skipping write")
            return False
        backup = backup_dir / f"uboot-{target['uboot_uuid']}-{stamp}.img"
        with backup.open("xb") as output:
            output.write(existing)
            output.flush()
            os.fsync(output.fileno())
        staged = Path(temp) / "uboot.img"
        staged.write_bytes(expected)
        same_card()
        print(f"U-Boot changed; backup: {backup}; sha256={digest(existing)}")
        command("sudo", "dd", f"if={staged}", f"of={target['uboot']}", "bs=1m", capture=False)
        command("sync", capture=False)
        same_card()
        after = Path(temp) / "after.img"
        sudo_read_partition(target["uboot"], after, UBOOT_SIZE)
        if after.read_bytes() != expected:
            raise UnsafeTarget(f"U-Boot READBACK MISMATCH. Backup: {backup}")
        print(f"U-Boot readback verified; sha256={digest(expected)}")
    return True


def sudo_read_partition(raw: str, destination: Path, size: int = BOOT_SIZE) -> None:
    # File is created by the current user; sudo receives only readonly input.
    with destination.open("xb") as output:
        subprocess.run(["sudo", "dd", f"if={raw}", "bs=1m"], check=True, stdout=output)
    if destination.stat().st_size != size:
        raise UnsafeTarget("partition backup/readback is incomplete")


def flash(image_path: Path, backup_dir: Path, target: dict, image: bytes,
          uboot: bytes | None = None) -> None:
    device = target["disk"]
    uuid = target["uuid"]
    boot = target["boot"]
    print(f"Selected: {device}; boot: {boot}; GPT UUID: {uuid}")
    print(f"New FIT: {image_path}, {len(image)} bytes, sha256={digest(image)}")
    if uboot is not None:
        if not target.get("uboot_uuid"):
            raise UnsafeTarget("U-Boot partition has no stable GPT UUID")
        print(f"U-Boot: {len(uboot)} bytes, sha256={digest(uboot)}")
        print("Writes boot and, ONLY if changed, U-Boot; other partitions preserved.")
    else:
        print("Writes ONLY the boot partition; preserves the rest of the card.")
        print("U-Boot update DISABLED: no --uboot-image supplied; boot splash will not be installed.")
    token = f"FLASH {device.removeprefix('/dev/')}s2" + (" + UBOOT" if uboot is not None else "")
    if input(f"Type '{token}' to proceed: ").strip() != token:
        raise UnsafeTarget("confirmation not matched; nothing was written")

    # Re-read the disk immediately before unmounting and privileged I/O.
    current = check_disk(device)
    if current["uuid"] != uuid or current["boot"] != boot:
        raise UnsafeTarget("card changed during confirmation")
    backup_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup = backup_dir / f"boot-{uuid}-{stamp}.img"
    print(f"Backup destination: {backup}")
    command("diskutil", "unmountDisk", device, capture=False)

    # Probe the exact partition again after unmount. Never call dd on whole disk.
    current = check_disk(device)
    if current["uuid"] != uuid:
        raise UnsafeTarget("card changed after unmount; refusing write")
    print("Creating mandatory full 12-MiB partition backup ...")
    sudo_read_partition(boot, backup)
    existing = backup.read_bytes()
    if existing[:4] != bytes.fromhex("d00dfeed"):
        raise UnsafeTarget(f"backup not FIT; cancelled. Backup saved: {backup}")
    print(f"Backup complete; sha256={digest(existing)}")
    # Confirm the same removable card is still present before writing.
    if check_disk(device)["uuid"] != uuid:
        raise UnsafeTarget("card changed before write; refusing")
    if uboot is not None:
        update_uboot(uboot, backup_dir, target, stamp)
        current = check_disk(device)
        if any(current.get(key) != target.get(key)
               for key in ("uuid", "boot", "uboot", "uboot_uuid")):
            raise UnsafeTarget("card changed after U-Boot update; refusing boot write")
    print("Writing FIT to boot partition ...")
    with tempfile.TemporaryDirectory(prefix="lexxos-rk3506-") as temp:
        # Write the exact bytes validated before confirmation, even if a build
        # replaces image_path while the operator is choosing the SD card.
        staged = Path(temp) / "boot.img"
        staged.write_bytes(image)
        command("sudo", "dd", f"if={staged}", f"of={boot}", "bs=1m", capture=False)
        command("sync", capture=False)
        reread = Path(temp) / "boot-readback.img"
        sudo_read_partition(boot, reread)
        if reread.read_bytes()[:len(image)] != image:
            raise UnsafeTarget(f"READBACK MISMATCH. Restore using backup: {backup}")
    print("Readback SHA-256 verified: identical to lexxos.img")
    print(f"Backup retained: {backup}")
    command("diskutil", "eject", device, capture=False)
    print("Card safely ejected. Ready for RK3506 boot.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, help="FIT image; required unless --list")
    parser.add_argument("--uboot-image", type=Path, help="optional board-compatible U-Boot FIT; written only if changed")
    parser.add_argument("--disk", help="optional exact /dev/diskN; omit for interactive chooser")
    parser.add_argument("--backup-dir", type=Path, required=True)
    parser.add_argument("--list", action="store_true", help="only list eligible SD cards; never write")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("this protected flasher works only on macOS")
    try:
        if args.list:
            disks = eligible_disks()
            if not disks:
                print("No eligible Luckfox Lyra cards found")
            for item in disks:
                print(f"{item['disk']} {item['size']} bytes boot UUID={item['uuid']}")
            return 0
        # Validate the image and destination before displaying a destructive prompt.
        if args.image is None:
            parser.error("--image is required for flashing")
        image = checked_image(args.image)
        uboot = checked_uboot(args.uboot_image) if args.uboot_image else None
        target = select_disk(args.disk)
        flash(args.image, args.backup_dir, target, image, uboot)
        return 0
    except (UnsafeTarget, OSError, subprocess.CalledProcessError) as exc:
        print(f"Refusing to flash: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
