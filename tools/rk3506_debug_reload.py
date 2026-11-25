#!/usr/bin/env python3
"""Prepare/reload a RK3506 ELF through an existing OpenOCD server.

For CLion Remote Debug: run this as a Before launch external tool *after* build.
By default SWD loads a separate tiny monitor into reserved DDR, letting us
quiesce a hung CPU without calling functions in the running LexxOS image.
Fail CLOSED if the monitor cannot prove flat/uncached memory and idle DMAC0.
The helper is intentionally independent of the kernel being overwritten.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONITOR_ADDR = 0x027F0000
STATUS_ADDR = 0x027F0200
READY = 0x52414D4F  # RAMO
REJECT = {0x43414348: "MMU or D-cache enabled", 0x444D4142: "DMAC0 busy",
          0x444D4146: "DMAC0 channel fault"}


def run(argv: list[str], *, timeout: float = 180) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(argv, cwd=ROOT, text=True, capture_output=True,
                              check=True, timeout=timeout)
    except FileNotFoundError as exc:
        raise RuntimeError(f"Tool not found: {argv[0]}") from exc
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f"Command failed: {' '.join(argv)}\n{exc.stdout}\n{exc.stderr}") from exc
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(f"Command timed out: {argv[0]}") from exc


def symbol(nm: str, binary: Path, name: str) -> int:
    result = run([nm, "--defined-only", str(binary)]).stdout
    matches = re.findall(r"^([0-9a-fA-F]+)\s+[A-Za-z]\s+" + re.escape(name) + r"$",
                         result, flags=re.MULTILINE)
    if len(matches) != 1:
        raise RuntimeError(f"Expected exactly one {name} symbol in {binary}")
    return int(matches[0], 16)


def prepare_uart(device: str, seconds: float) -> None:
    try:
        import serial  # pip install pyserial; only needed for --mode uart/auto
    except ImportError as exc:
        raise RuntimeError("pyserial is required for UART mode: pip install pyserial") from exc
    import time
    with serial.Serial(device, 1_500_000, timeout=0.2, write_timeout=1) as port:
        port.reset_input_buffer()
        port.write(b"\rdebug ram prepare\r")
        port.flush()
        deadline = time.monotonic() + seconds
        message = bytearray()
        while time.monotonic() < deadline:
            message.extend(port.read(256))
            if b"no further shell input" in message:
                print("LexxOS acknowledged 'debug ram prepare' over UART1")
                return
    raise RuntimeError("UART1 did not confirm RAM preparation")


def make_gdb_script(binary: Path, monitor_bin: Path, stop: int,
                    host: str, port: int, *, stay_halted: bool = False) -> str:
    """Safe preflight, temporary off-image monitor, and conditional DDR upload.

    No unconditional `program` or `reset`; the status check precedes `load`.
    GDB processes the script in batch and exits nonzero on a failed guard.
    """
    return f"""set pagination off
set confirm off
set architecture arm
set remotetimeout 30
target extended-remote {host}:{port}
monitor halt
# Проверка адресации до первой записи в DDR; ошибка monitor прерывает batch.
monitor eval {{if {{[arm mrc 15 0 1 0 0] & 5}} {{error "MMU or D-cache enabled"}}}}
# Verify SWD owns the UART0 pads; otherwise UART1 would not be restored.
if ((*(unsigned int *)0xff950014 & 0xff00) != 0x2200)
    echo ERROR: GPIO0_C6/C7 are not in SWD mode.\\n
    quit 21
end
# Исключаем исполнение старых инструкций из scratch-области.
monitor arm mcr 15 0 7 5 0 0
monitor arm mcr 15 0 7 5 6 0
# The monitor resides outside the loadable kernel image.
# GDB restore does not strip filename quotes; --cd selects its temporary directory.
restore {monitor_bin.name} binary 0x{MONITOR_ADDR:08x}
set {{unsigned int}}0x{STATUS_ADDR:08x} = 0
hbreak *0x{stop:08x}
# Start the temporary monitor in ARM SVC, IRQ/FIQ disabled.
set $cpsr = ($cpsr & 0xffffff00) | 0xd3
set $pc = 0x{MONITOR_ADDR:08x}
continue
set $reload_status = *(unsigned int *)0x{STATUS_ADDR:08x}
printf "RELOAD_MONITOR_STATUS=0x%08x\\n", $reload_status
if $reload_status != 0x{READY:08x}
    echo ERROR: the target failed the DMA/cache safety preflight.\\n
    quit 22
end
delete breakpoints
# Transfer only the target ELF's loadable sections. No SD writes.
load
# The standard firmware startup restores UART1 by reading SWD pinmux.
set $cpsr = ($cpsr & 0xffffff00) | 0xd3
set $pc = 0x02000000
# Do not leave the loader GDB connected when CLion attaches.
{'monitor halt' if stay_halted else 'monitor resume'}
disconnect
echo LEXXOS_RAM_RELOAD_OK\\n
quit
"""


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", default="build/picocalc-lyra-debug/lexxos.elf")
    p.add_argument("--configure-preset", help="Configure CMake before building")
    p.add_argument("--stay-halted", action="store_true", help="Leave CPU at _start for CLion to attach before execution")
    p.add_argument("--build-dir", help="Build the lexxos target first (for CLion Before launch)")
    p.add_argument("--mode", choices=("swd", "uart", "auto"), default="swd",
                   help="swd: independent monitor (default); uart: require shell ack; auto: try UART then SWD")
    p.add_argument("--port", help="UART1 device, e.g. /dev/cu.usbmodemXXXX (uart/auto)")
    p.add_argument("--uart-timeout", type=float, default=6)
    p.add_argument("--gdb", default="arm-none-eabi-gdb")
    p.add_argument("--gcc", default="arm-none-eabi-gcc")
    p.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    p.add_argument("--nm", default="arm-none-eabi-nm")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--gdb-port", type=int, default=3333)
    p.add_argument("--dry-run", action="store_true", help="Build nothing; print generated GDB script")
    args = p.parse_args()
    try:
        if args.mode in ("uart", "auto") and not args.port:
            raise RuntimeError("--port is required with --mode uart/auto")
        elf = (ROOT / args.elf).resolve()
        if args.configure_preset and not args.dry_run:
            print(f"Configuring {args.configure_preset} ...", flush=True)
            run(["cmake", "--preset", args.configure_preset], timeout=240)
        if args.build_dir and not args.dry_run:
            print(f"Building {args.build_dir} ...", flush=True)
            run(["cmake", "--build", args.build_dir, "--target", "lexxos"], timeout=240)
        if not elf.is_file() and not args.dry_run:
            raise RuntimeError(f"ELF does not exist: {elf}")
        if not args.dry_run:
            if symbol(args.nm, elf, "_start") != 0x02000000:
                raise RuntimeError("ELF is not linked at RK3506 boot address 0x02000000")
            if symbol(args.nm, elf, "__image_end__") > MONITOR_ADDR:
                raise RuntimeError("ELF overlaps the reserved emergency monitor region")
        if args.mode in ("uart", "auto") and not args.dry_run:
            try:
                prepare_uart(args.port, args.uart_timeout)
            except Exception as exc:
                if args.mode == "uart":
                    raise
                print(f"UART preparation failed: {exc}; trying SWD monitor", file=sys.stderr)
        with tempfile.TemporaryDirectory(prefix="rk3506_reload_") as temp:
            output = Path(temp)
            mon_elf = output / "monitor.elf"
            mon_bin = output / "monitor.bin"
            if not args.dry_run:
                run([args.gcc, "-mcpu=cortex-a7", "-marm", "-mfloat-abi=soft",
                     "-nostdlib", "-static", "-Wl,--build-id=none",
                     "-Wl,-T," + str(ROOT / "tools/debug/monitor/rk3506_reload_monitor.ld"),
                     "-o", str(mon_elf), str(ROOT / "tools/debug/monitor/rk3506_reload_monitor.S")])
                run([args.objcopy, "-O", "binary", str(mon_elf), str(mon_bin)])
                stop = symbol(args.nm, mon_elf, "rk3506_reload_monitor_stop")
                if not (MONITOR_ADDR <= stop < STATUS_ADDR):
                    raise RuntimeError("Invalid off-image debug monitor stop address")
            else:
                stop = MONITOR_ADDR + 0x100  # Example for dry run only.
            script = make_gdb_script(elf, mon_bin, stop, args.host, args.gdb_port, stay_halted=args.stay_halted)
            if args.dry_run:
                print(script)
                return 0
            cmdfile = output / "reload.gdb"
            cmdfile.write_text(script, encoding="utf-8")
            print("Preparing CPU0 with SWD monitor and uploading ELF to DDR ...", flush=True)
            result = run([args.gdb, "-nx", "-q", "--batch", "--cd=" + str(output),
                          "-x", str(cmdfile), str(elf)],
                         timeout=240)
            print(result.stdout, end="")
            if "LEXXOS_RAM_RELOAD_OK" not in result.stdout:
                raise RuntimeError("GDB did not confirm successful RAM reload")
        return 0
    except RuntimeError as exc:
        print(f"RK3506 RAM reload aborted: {exc}", file=sys.stderr)
        print("If GDB reached the load stage DDR may already have changed; cold boot from SD if unsure.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
