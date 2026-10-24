import unittest

import smoke_test as st

VECTORS = st.parse_vectors(st.DEFAULT_VECTORS.read_text())
BASE = {"version": 11, "reset": 3, "uptime": 100, "free": 100000}


class SmokeTestLogic(unittest.TestCase):
    def test_fixture_parses(self):
        self.assertEqual(len(VECTORS["opcode"]), 37)
        self.assertEqual(len(VECTORS["uuid"]), 4)
        self.assertEqual(VECTORS["opcode"]["GET_DEVICE_INFO"], 0x13)

    def test_every_op_name_in_script_exists(self):
        names = set(st.READ_OPS) | set(st.SKIPPED) | {op for _, op, _ in st.REJECTS}
        self.assertEqual(names - set(VECTORS["opcode"]), set())

    def test_error_vectors_filter(self):
        names = {v["name"] for v in st.error_vectors(VECTORS)}
        self.assertIn("unknown_opcode", names)
        self.assertNotIn("led_state", names)
        self.assertNotIn("set_catch_mode_bad_value", names)

    def test_parse_device_info(self):
        v = next(v for v in VECTORS["vector"] if v["name"] == "device_info")
        _, _, payload = st.split_frame(v["response"])
        info = st.parse_device_info(payload)
        self.assertEqual((info["version"], info["conns"], info["reset"], info["uptime"]), (11, 4, 3, 5))
        self.assertEqual(len(payload), 16)

    def test_check_cycle(self):
        ok = dict(BASE, uptime=200, free=99500)
        self.assertEqual(st.check_cycle(BASE, ok, 100, 1024), [])
        self.assertEqual(len(st.check_cycle(BASE, dict(ok, uptime=5), 100, 1024)), 1)
        self.assertEqual(len(st.check_cycle(BASE, dict(ok, reset=1), 100, 1024)), 1)
        self.assertEqual(len(st.check_cycle(BASE, dict(ok, free=98000), 100, 1024)), 1)

    def test_split_frame_short(self):
        with self.assertRaises(ValueError):
            st.split_frame(b"\x00")
        self.assertEqual(st.split_frame(b"\x00\x13\xaa"), (0, 0x13, b"\xaa"))


if __name__ == "__main__":
    unittest.main()
