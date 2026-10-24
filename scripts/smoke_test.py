#!/usr/bin/env python3
"""On-device smoke test: bond, Control Service opcodes, reconnect loop. Maintainer-run only."""
import argparse
import asyncio
import struct
import sys
from pathlib import Path

DEFAULT_VECTORS = Path(__file__).resolve().parent.parent / "protocol" / "control_vectors.txt"
REPLY_TIMEOUT = 5
SCAN_TIMEOUT = 15
FIXED_INFO_LEN = 16

READ_OPS = [
    "HELP", "GET_GLOBAL_SETTINGS", "GET_LED_STATE", "GET_RUNTIME_STATS", "GET_TASK_LIST",
    "GET_CLIENT_STATES", "GET_CLIENT_SUMMARY", "GET_DEVICE_INFO", "LIST_KNOWN_DEVICES",
    "GET_CRASH_REPORT", "GET_SECRETS", "SAVE_SETTINGS",
]

# (check name, opcode, payload): each must answer ERR_MALFORMED_PAYLOAD and change nothing.
REJECTS = [
    ("set_log_level_empty", "SET_LOG_LEVEL", b""),
    ("set_log_level_zero", "SET_LOG_LEVEL", b"\x00"),
    ("toggle_autospin_bad_slot", "TOGGLE_AUTOSPIN", b"\x04"),
    ("toggle_autocatch_bad_slot", "TOGGLE_AUTOCATCH", b"\x04"),
    ("set_autospin_empty", "SET_AUTOSPIN", b""),
    ("set_autocatch_empty", "SET_AUTOCATCH", b""),
    ("ota_begin_short", "OTA_BEGIN", bytes(3)),
    ("ota_data_short", "OTA_DATA", bytes(4)),
    ("rename_known_device_short", "RENAME_KNOWN_DEVICE", bytes(5)),
    ("get_lifetime_stats_short", "GET_LIFETIME_STATS", bytes(5)),
    ("reset_lifetime_stats_short", "RESET_LIFETIME_STATS", bytes(5)),
]

SKIPPED = [
    "RESTART", "FORGET_PAIRINGS", "RESET_SECRETS", "PROVISION_SECRETS", "ADVERTISE_START",
    "ADVERTISE_STOP", "RESET_CLIENT_STATES", "CLEAR_CRASH_REPORT", "CYCLE_LOG_LEVEL",
    "SET_MAX_CONNECTIONS", "SET_LED_ENABLED", "SET_CATCH_MODE", "SET_AUTOSPIN", "SET_AUTOCATCH",
    "SET_LOG_LEVEL", "TOGGLE_AUTOSPIN", "TOGGLE_AUTOCATCH", "RENAME_KNOWN_DEVICE",
    "FORGET_KNOWN_DEVICE", "RESET_LIFETIME_STATS", "OTA_END", "OTA_ABORT",
]


def parse_vectors(text):
    """Return {"opcode": {name: int}, "status": {name: int}, "uuid": {key: str}, "vector": [dict]}."""
    out = {"opcode": {}, "status": {}, "uuid": {}, "vector": []}
    for line in text.splitlines():
        f = line.split()
        if not f or f[0].startswith("#"):
            continue
        if f[0] in ("opcode", "status"):
            out[f[0]][f[1]] = int(f[2], 16)
        elif f[0] == "uuid":
            out["uuid"][f[1]] = f[2]
        elif f[0] == "vector":
            out["vector"].append({
                "name": f[1], "setup": f[2], "request": bytes.fromhex(f[3]),
                "response": bytes.fromhex(f[4]), "prefix": f[5:] == ["prefix"],
            })
    return out


def error_vectors(vectors):
    return [v for v in vectors["vector"]
            if v["setup"] == "default" and not v["prefix"] and v["response"][0] != 0]


def split_frame(frame):
    if len(frame) < 2:
        raise ValueError(f"frame too short: {frame.hex()}")
    return frame[0], frame[1], frame[2:]


def parse_device_info(payload):
    if len(payload) < FIXED_INFO_LEN:
        raise ValueError(f"device info too short: {len(payload)} bytes")
    version, conns, reset = payload[0], payload[1], payload[2]
    uptime, free, min_free = struct.unpack_from("<III", payload, 3)
    return {"version": version, "conns": conns, "reset": reset, "uptime": uptime,
            "free": free, "min_free": min_free, "flags": payload[15],
            "fw": payload[FIXED_INFO_LEN:].decode("ascii", "replace")}


def check_cycle(baseline, sample, previous_uptime, tolerance):
    """Return failure reasons for one DEVICE_INFO sample against the first one."""
    fails = []
    if sample["version"] != baseline["version"]:
        fails.append(f"protocol version {baseline['version']} -> {sample['version']}")
    if sample["reset"] != baseline["reset"]:
        fails.append(f"reset reason {baseline['reset']} -> {sample['reset']}")
    if sample["uptime"] < previous_uptime:
        fails.append(f"reboot: uptime {previous_uptime}s -> {sample['uptime']}s")
    if baseline["free"] - sample["free"] > tolerance:
        fails.append(f"free heap {baseline['free']} -> {sample['free']} (tolerance {tolerance})")
    return fails


class Report:
    def __init__(self):
        self.passed = self.failed = 0

    def check(self, name, ok, reason=""):
        if ok:
            self.passed += 1
            print(f"PASS {name}")
        else:
            self.failed += 1
            print(f"FAIL {name}: {reason}")
        return ok


class Control:
    def __init__(self, client, uuids, opcodes, status):
        self.client, self.uuids, self.opcodes, self.status = client, uuids, opcodes, status
        self.replies = asyncio.Queue()

    async def open(self):
        await self.client.start_notify(self.uuids["response_char"], lambda _, d: self.replies.put_nowait(bytes(d)))

    async def request(self, frame):
        while not self.replies.empty():
            self.replies.get_nowait()
        await self.client.write_gatt_char(self.uuids["command_char"], frame, response=True)
        return split_frame(await asyncio.wait_for(self.replies.get(), REPLY_TIMEOUT))


async def find_device(args):
    from bleak import BleakScanner
    if args.address:
        return await BleakScanner.find_device_by_address(args.address, timeout=SCAN_TIMEOUT)
    return await BleakScanner.find_device_by_name(args.name, timeout=SCAN_TIMEOUT)


async def device_info(ctl):
    status, op, payload = await ctl.request(bytes([ctl.opcodes["GET_DEVICE_INFO"]]))
    if status != ctl.status["OK"] or op != ctl.opcodes["GET_DEVICE_INFO"]:
        raise ValueError(f"GET_DEVICE_INFO status {status:02x} opcode {op:02x}")
    return parse_device_info(payload)


async def run_checks(ctl, vectors, report):
    ops, st = ctl.opcodes, ctl.status
    for name in READ_OPS:
        try:
            status, op, payload = await ctl.request(bytes([ops[name]]))
        except Exception as e:
            report.check(name, False, repr(e))
            continue
        ok = status == st["OK"] and op == ops[name]
        reason = f"status {status:02x} opcode {op:02x}"
        if ok and name == "LIST_KNOWN_DEVICES":
            ok = bool(payload) and len(payload) == 1 + 27 * payload[0]
            reason = f"bad list length {len(payload)}"
        elif ok and name == "GET_CRASH_REPORT":
            ok = bool(payload) and payload[0] in (0, 1)
            reason = "bad present flag"
        elif ok and name == "GET_DEVICE_INFO":
            ok = len(payload) >= FIXED_INFO_LEN
            reason = f"payload {len(payload)} bytes"
        report.check(name, ok, reason)
    for v in error_vectors(vectors):
        try:
            status, op, payload = await ctl.request(v["request"])
            got = bytes([status, op]) + payload
        except Exception as e:
            report.check(f"vector {v['name']}", False, repr(e))
            continue
        report.check(f"vector {v['name']}", got == v["response"], f"got {got.hex()} want {v['response'].hex()}")
    for name, op, payload in REJECTS:
        try:
            status, rop, _ = await ctl.request(bytes([ops[op]]) + payload)
        except Exception as e:
            report.check(name, False, repr(e))
            continue
        report.check(name, status == st["ERR_MALFORMED_PAYLOAD"] and rop == ops[op], f"status {status:02x} opcode {rop:02x}")
    for name in SKIPPED:
        print(f"SKIP (destructive) {name}")


async def run_cycles(args, ctl_factory, report, baseline):
    previous = baseline["uptime"]
    for i in range(1, args.cycles + 1):
        name = f"reconnect_{i}"
        try:
            await asyncio.sleep(2)
            device = await find_device(args)
            if device is None:
                report.check(name, False, "device not advertising after disconnect")
                continue
            async with ctl_factory(device) as ctl:
                sample = await device_info(ctl)
        except Exception as e:
            report.check(name, False, repr(e))
            continue
        fails = check_cycle(baseline, sample, previous, args.heap_tolerance)
        previous = sample["uptime"]
        report.check(name, not fails, "; ".join(fails))
        print(f"     free heap {sample['free']} B, uptime {sample['uptime']} s")


async def main_async(args):
    from bleak import BleakClient
    vectors = parse_vectors(Path(args.vectors).read_text())
    uuids, opcodes, status = vectors["uuid"], vectors["opcode"], vectors["status"]
    report = Report()

    class Session:
        def __init__(self, device):
            self.client = BleakClient(device)

        async def __aenter__(self):
            await self.client.connect()
            ctl = Control(self.client, uuids, opcodes, status)
            await ctl.open()
            return ctl

        async def __aexit__(self, *exc):
            await self.client.disconnect()

    device = await find_device(args)
    if not report.check("scan", device is not None, "device not found"):
        return report
    print("Bonding happens on the first write; accept the OS pairing dialog if it appears.")
    try:
        async with Session(device) as ctl:
            baseline = await device_info(ctl)
            report.check("bond", True)
            print(f"     fw {baseline['fw']!r}, provisioning={bool(baseline['flags'] & 1)}, free heap {baseline['free']} B")
            await run_checks(ctl, vectors, report)
    except Exception as e:
        report.check("bond", False, repr(e))
        return report
    await run_cycles(args, Session, report, baseline)
    return report


def parse_args(argv):
    p = argparse.ArgumentParser(description=__doc__)
    sel = p.add_mutually_exclusive_group(required=True)
    sel.add_argument("--address", help="BLE address (UUID on macOS)")
    sel.add_argument("--name", help="advertised device name")
    p.add_argument("--cycles", type=int, default=5, help="disconnect/reconnect cycles")
    p.add_argument("--heap-tolerance", type=int, default=1024, help="allowed free-heap drop in bytes")
    p.add_argument("--vectors", default=str(DEFAULT_VECTORS))
    return p.parse_args(argv)


def main(argv=None):
    report = asyncio.run(main_async(parse_args(argv)))
    print(f"{report.passed} passed, {report.failed} failed")
    return 1 if report.failed else 0


if __name__ == "__main__":
    sys.exit(main())
