import json
import tempfile
import unittest
from pathlib import Path

import flasher_manifest as fm


def fake_bins(d: Path, skip=()):
    for n in fm.INPUTS:
        if n not in skip:
            (d / n).write_bytes(n.encode())


class FlasherManifest(unittest.TestCase):
    def run_build(self, skip=()):
        tmp = Path(tempfile.mkdtemp())
        src, out = tmp / "src", tmp / "out"
        src.mkdir()
        fake_bins(src, skip)
        fm.build(src, "1.0.1", out)
        return out

    def test_manifest_parts_and_offsets(self):
        m = json.loads((self.run_build() / "manifest.json").read_text())
        self.assertEqual(m["version"], "1.0.1")
        self.assertIs(m["new_install_prompt_erase"], True)
        build = m["builds"][0]
        self.assertEqual(build["chipFamily"], "ESP32-C3")
        self.assertEqual(
            [(p["path"], p["offset"]) for p in build["parts"]],
            [("bootloader.bin", 0), ("partition-table.bin", 0x8000),
             ("otadata.bin", 0x2D000), ("pgpemu.bin", 0x40000)],
        )

    def test_otadata_blank(self):
        data = (self.run_build() / "otadata.bin").read_bytes()
        self.assertEqual(data, b"\xff" * 8192)

    def test_binaries_copied(self):
        self.assertEqual((self.run_build() / "pgpemu.bin").read_bytes(), b"pgpemu.bin")

    def test_missing_input_fails(self):
        with self.assertRaisesRegex(FileNotFoundError, "missing in .*pgpemu.bin"):
            self.run_build(skip=("pgpemu.bin",))


if __name__ == "__main__":
    unittest.main()
