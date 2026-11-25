#!/usr/bin/env python3
"""Host-only checks for the U-Boot image layout and rebuild cache."""
import importlib.util
import json
import struct
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import rk3506_build_uboot as builder

spec = importlib.util.spec_from_file_location('package_uboot', builder.PORT / 'package.py')
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


class Packaging(unittest.TestCase):
    def test_two_identical_slots_preserve_fit_and_zero_padding(self):
        fit = bytes.fromhex('d00dfeed') + b'firmware'
        image = packer.package(fit, 2 * 1024 * 1024, 2)
        self.assertEqual(len(image), 4 * 1024 * 1024)
        self.assertEqual(image[:len(fit)], fit)
        self.assertEqual(image[:2 * 1024 * 1024], image[2 * 1024 * 1024:])
        self.assertFalse(any(image[len(fit):2 * 1024 * 1024]))

    def test_oversized_fit_and_unknown_geometry_are_rejected(self):
        fit = bytes.fromhex('d00dfeed') + bytes(2 * 1024 * 1024)
        with self.assertRaises(ValueError):
            packer.package(fit, 2 * 1024 * 1024, 2)
        with self.assertRaises(ValueError):
            packer.package(fit[:8], 1024 * 1024, 4)

    def test_vendor_uppercase_uboot_os_is_accepted(self):
        def word(value):
            return struct.pack('>I', value)

        def node(name):
            name = name.encode() + b'\0'
            return word(1) + name.ljust((len(name) + 3) & ~3, b'\0')

        os_name = b'U-Boot\0'
        structure = (node('') + node('images') + node('uboot') +
                     word(3) + word(len(os_name)) + word(0) +
                     os_name.ljust(8, b'\0') + word(2) * 3 + word(9))
        strings = b'os\0'
        header = struct.pack('>10I', 0xd00dfeed, 56 + len(structure) + len(strings),
                             56, 56 + len(structure), 40, 17, 16, 0,
                             len(strings), len(structure))
        fit = (header + bytes(16) + structure + strings).ljust(4096, b'\0')
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / 'uboot.img'
            image.write_bytes(fit)
            self.assertEqual(builder.checked_uboot(image), fit)

    def test_unchanged_inputs_and_output_do_not_download_or_start_docker(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            image = b'previously validated image'
            (folder / 'uboot.img').write_bytes(image)
            (folder / 'build.json').write_text(json.dumps(dict(
                inputs=builder.input_digest(), sha256=builder.sha(image))))
            with patch.object(builder, 'run') as run:
                builder.build(folder, 4)
            run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
