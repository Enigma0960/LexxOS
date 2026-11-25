#!/usr/bin/env python3
"""Pack vendor FIT copies using the slot geometry expected by Rockchip SPL."""
from pathlib import Path


def package(fit: bytes, slot_size: int, copies: int) -> bytes:
    if not fit.startswith(bytes.fromhex('d00dfeed')):
        raise ValueError('Missing FIT header')
    if slot_size != 2 * 1024 * 1024 or copies != 2:
        raise ValueError('Expected Lyra 2 x 2 MiB SPL FIT geometry')
    if len(fit) > slot_size:
        raise ValueError('FIT exceeds SPL slot size')
    return fit.ljust(slot_size, b'\0') * copies


if __name__ == '__main__':
    config = dict(line.split('=', 1) for line in Path('.config').read_text().splitlines()
                  if line.startswith('CONFIG_') and '=' in line)
    if config.get('CONFIG_PICOCALC_LYRA_SPLASH') != 'y':
        raise ValueError('Splash driver is disabled')
    Path('uboot.img').write_bytes(package(
        Path('u-boot.itb').read_bytes(),
        int(config['CONFIG_SPL_FIT_IMAGE_KB']) * 1024,
        int(config['CONFIG_SPL_FIT_IMAGE_MULTIPLE'])))
