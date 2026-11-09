import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

FAKE_GRADLE = """#!/bin/sh
echo "$KEYSTORE_PATH $KEY_ALIAS $STORE_PASSWORD $KEY_PASSWORD $*" >> "$FAKE_LOG"
"""

FAKE_ADB = """#!/bin/sh
echo "adb $*" >> "$FAKE_LOG"
"""


def make_executable(path: Path, body: str) -> None:
    path.write_text(body)
    path.chmod(path.stat().st_mode | stat.S_IEXEC)


class CompanionInstallRelease(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = Path(tmp.name)
        self.home = self.dir / "home"
        self.bin = self.dir / "bin"
        self.bin.mkdir()
        self.home.mkdir()
        self.log = self.dir / "calls.log"
        self.gradle = self.dir / "fake-gradlew"
        make_executable(self.gradle, FAKE_GRADLE)
        make_executable(self.bin / "adb", FAKE_ADB)

    def run_target(self):
        env = dict(
            os.environ,
            HOME=str(self.home),
            PATH=f"{self.bin}{os.pathsep}{os.environ['PATH']}",
            FAKE_LOG=str(self.log),
        )
        proc = subprocess.run(
            ["make", "-C", str(REPO), "companion-install-release", f"GRADLEW={self.gradle}"],
            env=env,
            capture_output=True,
            text=True,
            timeout=60,
        )
        calls = self.log.read_text().splitlines() if self.log.exists() else []
        return proc, calls

    def test_builds_release_signed_with_debug_key_then_installs_with_replace(self):
        proc, calls = self.run_target()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        keystore = self.home / ".android" / "debug.keystore"
        self.assertEqual(
            calls,
            [
                f"{keystore} androiddebugkey android android :app:assembleRelease",
                "adb install -r companion-app/app/build/outputs/apk/release/app-release.apk",
            ],
        )

    def test_failed_build_does_not_install(self):
        make_executable(self.gradle, "#!/bin/sh\necho build failed >> \"$FAKE_LOG\"\nexit 1\n")
        proc, calls = self.run_target()
        self.assertNotEqual(proc.returncode, 0)
        self.assertFalse(any(c.startswith("adb") for c in calls), calls)


if __name__ == "__main__":
    unittest.main()
