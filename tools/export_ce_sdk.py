#!/usr/bin/env python3
"""Экспорт готового ESP32 SDK: архив, публичные заголовки и проверка ABI."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from check_ce_build import check

ENTRY_HEADERS = (
    "lexxos/core/bootstrap.h",
    "lexxos/core/core_system.h",
    "lexxos/core/core_context.h",
    "lexxos/devices/bus/bus_transaction_queue.h",
    "lexxos/devices/bus/i2c_bus.h",
    "lexxos/devices/bus/spi_transfer.h",
    "lexxos/devices/input/digital_input.h",
    "lexxos/devices/input/analog_input.h",
    "lexxos/devices/output/pwm_output.h",
    "lexxos/devices/output/analog_output.h",
    "lexxos/devices/output/digital_output.h",
    "lexxos/kernel/scheduler.h",
    "lexxos/kernel/cooperative/cooperative_loop.h",
    "lexxos/kernel/synchronization.h",
    "lexxos/kernel/system.h",
    "lexxos/platform/platform_services.h",
)
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
ABI_CONFIG = re.compile(r"CONFIG_FREERTOS_.*|CONFIG_COMPILER_CXX_(EXCEPTIONS|RTTI)|CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE")


def headers(root: Path) -> dict[str, Path]:
    pending = list(ENTRY_HEADERS)
    result = {}
    while pending:
        name = pending.pop()
        if name in result:
            continue
        source = root / ("src" if name.startswith("lexxos/") else "config") / name
        if not source.is_file():
            raise ValueError(f"Missing public header: {name}")
        result[name] = source
        for included in INCLUDE.findall(source.read_text()):
            if included.startswith(("lexxos/", "memory/")):
                pending.append(included)
    forbidden = ("lexxos/shell/", "lexxos/vfs/", "lexxos/program/", "lexxos/os/", "lexxos/gameboy/")
    for name in result:
        if name.startswith(forbidden):
            raise ValueError(f"Standard header leaked into CE API: {name}")
    return result


def abi_config(path: Path) -> dict[str, str | None]:
    result = {}
    for line in path.read_text().splitlines():
        defined = re.fullmatch(r'(CONFIG_\w+)=(.*)', line)
        disabled = re.fullmatch(r'# (CONFIG_\w+) is not set', line)
        if defined and ABI_CONFIG.fullmatch(defined[1]):
            value = defined[2]
            if value.startswith('"'):
                continue  # Имена служебных задач не влияют на ABI.
            result[defined[1]] = '1' if value == 'y' else value
        elif disabled and ABI_CONFIG.fullmatch(disabled[1]):
            result[disabled[1]] = None
    return result


def export(args) -> None:
    check(args.build, args.archive, portable=args.idf_path is None)
    public = headers(args.root)
    compiler = subprocess.check_output([str(args.compiler), '-dumpfullversion' if args.idf_path else '--version'],
                                       text=True).splitlines()[0].strip()
    compiler_parts = compiler.split('.')
    if args.idf_path and (len(compiler_parts) != 3 or not all(part.isdigit() for part in compiler_parts)):
        raise ValueError(f"Unsupported compiler version: {compiler}")
    version = []
    config = {}
    if args.idf_path:
        version_header = (args.idf_path / 'components/esp_common/include/esp_idf_version.h').read_text()
        version = [re.search(rf'#define ESP_IDF_VERSION_{part}\s+(\d+)', version_header)[1]
                   for part in ('MAJOR', 'MINOR', 'PATCH')]
        config = abi_config(args.build / 'sdkconfig')
    output = args.output
    (output / 'lib').mkdir(parents=True, exist_ok=True)
    for name, source in public.items():
        destination = output / 'include' / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    sources = {'lexxos_ce': args.archive, 'lexxos_ce_cooperative': args.cooperative_archive}
    if args.idf_path:
        if not args.espidf_archive or not args.freertos_archive:
            raise ValueError("ESP-IDF SDK requires platform and FreeRTOS component archives")
        sources.update(lexxos_ce_espidf=args.espidf_archive, lexxos_ce_freertos=args.freertos_archive)
    archives = {}
    for name, source in sources.items():
        destination = output / 'lib' / f'lib{name}.a'
        shutil.copy2(source, destination)
        archives[name] = {'path': f'lib/lib{name}.a',
                          'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}
    lines = ['#pragma once', '', '// Сгенерировано вместе с .a: проверка бинарного контракта SDK.']
    definitions = ['LEXXOS_EDITION_CE']
    if args.idf_path:
        definitions.append('LEXXOS_PLATFORM_ESPIDF')
        lines += ['#include <sdkconfig.h>', '#include <esp_idf_version.h>', '',
                  '#if !defined(CONFIG_IDF_TARGET_ESP32) || CONFIG_IDF_TARGET_ESP32 != 1',
                  '#error "This LexxCE archive targets ESP32"', '#endif',
                  f'#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL({", ".join(version)})',
                  '#error "LexxCE SDK: ESP-IDF version mismatch"', '#endif',
                  f'#if __GNUC__ != {compiler_parts[0]} || __GNUC_MINOR__ != {compiler_parts[1]} || __GNUC_PATCHLEVEL__ != {compiler_parts[2]}',
                  '#error "LexxCE SDK: GCC version mismatch"', '#endif']
    for macro in definitions:
        lines += [f'#if defined({macro}) && {macro} != 1', '#error "LexxCE SDK: conflicting platform definition"',
                  '#endif', f'#ifndef {macro}', f'#define {macro} 1', '#endif']
    for name, value in sorted(config.items()):
        lines += [f'#ifdef {name}' if value is None else f'#if !defined({name}) || {name} != ({value})',
                  f'#error "LexxCE SDK: incompatible {name}; use matching SDK or restore its FreeRTOS configuration"',
                  '#endif']
    if args.idf_path:
        lines += ['', 'static_assert(sizeof(void*) == 4, "LexxCE SDK requires 32-bit pointers");', '']
    destination = output / 'include/lexxos/ce/build_config.h'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text('\n'.join(lines))
    for name in ('LICENSE', 'NOTICE'):
        shutil.copy2(args.root / name, output / name)
    (output / 'manifest.json').write_text(json.dumps({
        'schema': 2, 'target': 'esp32' if args.idf_path else 'portable',
        'idf_version': '.'.join(version) if version else None, 'compiler_version': compiler,
        'gcc_version': compiler if args.idf_path else None,
        'compiler_default_target': subprocess.check_output([str(args.compiler), '-dumpmachine'], text=True).strip(),
        'archives': archives, 'default_execution': 'cooperative',
        'archive': 'lib/liblexxos_ce.a', 'sha256': archives['lexxos_ce']['sha256'],
        'headers': sorted([*public, 'lexxos/ce/build_config.h']), 'abi_config': config,
    }, indent=2) + '\n')
    print(f'CE SDK exported: {output} ({len(public)} public headers, {len(archives)} archives)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build', 'archive', 'cooperative-archive', 'compiler', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    for name in ('idf-path', 'espidf-archive', 'freertos-archive'):
        parser.add_argument('--' + name, type=Path)
    try:
        export(parser.parse_args())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'CE SDK export failed: {error}\n')
