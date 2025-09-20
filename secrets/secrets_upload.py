#!/usr/bin/env python3
# deps: https://python-poetry.org/
# run with: poetry run ./secrets_upload.py secrets.yaml /dev/ttyUSB0

import base64
import binascii
import sys
from typing import Union

import serial
import yaml

class DeviceSecrets:
    @staticmethod
    def from_dict(secret: dict) -> None:
        return DeviceSecrets(**secret)

    def __init__(self, **kwargs):
        self.name = kwargs["name"]
        self.mac = bytes(kwargs["mac"])
        self.key = bytes(kwargs["key"])
        self.blob = bytes(kwargs["blob"])

        if len(self.mac) != 6:
            print("wrong mac len")
        if len(self.key) != 16:
            print("wrong key len")
        if len(self.blob) != 256:
            print("wrong blob len")

    def crc(self) -> int:
        return binascii.crc32(self.mac + self.key + self.blob)

    def dump(self) -> None:
        print("name:", self.name)
        print("mac:", self.mac)
        print("key:", self.key)
        print("blob:", self.blob)

    def __str__(self) -> str:
        return f"DeviceSecrets({self.name})"


def print_line(line: bytes) -> None:
    print("> " + line.strip().decode("utf8"))


def serial_command(ser: serial.Serial, command: str, value: Union[str, bytes]) -> bool:
    if not command in "NMKB":
        print("invalid command", command)
        return False

    ser.write(command.encode("utf8"))
    line = ser.readline().strip()
    print_line(line)
    if not f"set={command}".encode("utf8") in line:
        return False

    send_data = None
    if command == "N" and isinstance(value, str):
        # set name
        if len(value) > 15:
            print("name too long:", value)
            return False

        send_data = value.encode("utf8")
        ser.write(send_data)
        ser.write(b"\n")
    elif isinstance(value, bytes):
        # ensure correct buffer lens
        wanted_lens = {
            "M": 6,
            "K": 16,
            "B": 256,
        }
        if len(value) != wanted_lens[command]:
            print(f"invalid len for {command}: {len(value)}")
            return False

        # send base64 encoded data
        send_data = base64.b64encode(value)
        ser.write(send_data)
        ser.write(b"\n")
    else:
        print("invalid arguments to function")
        return False

    line = ser.readline().strip()
    print_line(line)
    if not f"{command}=[OK]".encode("utf8") in line:
        print("command failed:", command)
        print("value:", value)
        print(f"sent({len(send_data)}):", send_data.decode("utf8"))
        return False

    return True


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <secrets.yaml> </dev/ttyUSBx>")
        sys.exit(1)

    secrets_file = sys.argv[1]
    port = sys.argv[2]

    print("reading input secrets file")
    secrets = []
    with open(secrets_file, "r") as f:
        y = yaml.safe_load(f)

        for secret in y["devices"]:
            d = DeviceSecrets.from_dict(secret)
            print("-", d)
            secrets.append(d)

    if not secrets:
        print("no secrets present")
        return

    with serial.Serial(port=port, baudrate=115200, timeout=2) as ser:
        print("enter secrets mode")
        ser.write(b"X")
        while True:
            line = ser.readline().strip()
            print_line(line)
            if line == b"!":
                break

        print("listing current secrets:")
        ser.write(b"l")
        for line in ser.readlines():
            print(line.strip().decode("utf8"))

        print("uploading secrets")
        for i, secret in enumerate(secrets):
            ser.write(f"{i}".encode("utf8"))
            line = ser.readline().strip()
            print_line(line)
            if not f"slot={i}".encode("utf8") in line:
                print("failed selecting slot:", line)
                continue

            if not serial_command(ser, "N", secret.name):
                break
            if not serial_command(ser, "M", secret.mac):
                break
            if not serial_command(ser, "K", secret.key):
                break
            if not serial_command(ser, "B", secret.blob):
                break

            # get CRC
            ser.write(b"s")
            line = ser.readline().strip()
            print_line(line)

            # check CRC
            wanted_crc_int = secret.crc()
            wanted_crc_str = "%08x" % wanted_crc_int
            if not f"slot=tmp crc={wanted_crc_str}".encode("utf8") in line:
                print("wrong crc in tmp buf! wanted:", wanted_crc_str)
                break

            # write to nvs
            ser.write(b"W")
            line = ser.readline().strip()
            print_line(line)
            if not b"[OK]" in line:
                print("writing failed")
                break
            
            line = ser.readline().strip()
            print_line(line)
            if not b"write=1" in line:
                print("writing result bad")
                break

            print(f"writing to nvs slot {i} OK")

            # read back stored data's CRC
            ser.write(b"S")
            line = ser.readline().strip()
            print_line(line)
            if not f"slot={i} crc={wanted_crc_str}".encode("utf8") in line:
                print("wrong crc in nvs! wanted:", wanted_crc_str)
                break
            
            print("readback OK")

        print()
        print("listing new secrets:")
        ser.write(b"l")
        for line in ser.readlines():
            print(line.strip().decode("utf8"))

        print("leaving secrets mode")
        ser.write(b"q")
        while True:
            line = ser.readline().strip()
            print_line(line)
            if line == b"X":
                break

        print("OK!")


if __name__ == "__main__":
    main()
