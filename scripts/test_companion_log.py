import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

FAKE_ADB = """#!/bin/sh
echo "$*" >> "$FAKE_ADB_LOG"
case "$1" in
  get-state)
    if [ -n "$FAKE_ADB_TWO_DEVICES" ] && [ -z "$ANDROID_SERIAL" ]; then
      echo "error: more than one device/emulator" >&2
      exit 1
    fi
    echo device ;;
  shell)
    case "$2" in
      pidof) [ -f "$FAKE_ADB_RUNNING" ] && echo 4242 ;;
      am) touch "$FAKE_ADB_RUNNING" ;;
    esac ;;
esac
exit 0
"""


class CompanionLog(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = Path(tmp.name)
        self.bin = self.dir / "bin"
        self.bin.mkdir()
        adb = self.bin / "adb"
        adb.write_text(FAKE_ADB)
        adb.chmod(adb.stat().st_mode | stat.S_IEXEC)
        self.log = self.dir / "adb.log"
        self.running = self.dir / "running"

    def run_log(self, running=False, two_devices=False, serial=None):
        if running:
            self.running.touch()
        env = dict(
            os.environ,
            PATH=f"{self.bin}{os.pathsep}{os.environ['PATH']}",
            FAKE_ADB_LOG=str(self.log),
            FAKE_ADB_RUNNING=str(self.running),
        )
        env.pop("ANDROID_SERIAL", None)
        env.pop("FAKE_ADB_TWO_DEVICES", None)
        if two_devices:
            env["FAKE_ADB_TWO_DEVICES"] = "1"
        if serial:
            env["ANDROID_SERIAL"] = serial
        proc = subprocess.run(
            ["make", "-C", str(REPO), "companion-log"],
            env=env,
            capture_output=True,
            text=True,
            timeout=60,
        )
        calls = self.log.read_text().splitlines() if self.log.exists() else []
        return proc, calls

    def test_starts_app_then_tails_its_pid_when_not_running(self):
        proc, calls = self.run_log()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        start = "shell am start -n com.pgpemu.companion/.MainActivity"
        self.assertIn(start, calls)
        self.assertIn("logcat --pid=4242", calls)
        self.assertLess(calls.index(start), calls.index("logcat --pid=4242"))

    def test_does_not_restart_a_running_app(self):
        proc, calls = self.run_log(running=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertFalse(any("am start" in c for c in calls), calls)
        self.assertIn("logcat --pid=4242", calls)

    def test_two_devices_without_serial_fail_before_starting_app(self):
        proc, calls = self.run_log(two_devices=True)
        self.assertNotEqual(proc.returncode, 0)
        self.assertFalse(any("am start" in c for c in calls), calls)
        self.assertFalse(any("logcat" in c for c in calls), calls)

    def test_two_devices_with_serial_tail_the_app(self):
        proc, calls = self.run_log(two_devices=True, serial="emu-5554")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("logcat --pid=4242", calls)


if __name__ == "__main__":
    unittest.main()
