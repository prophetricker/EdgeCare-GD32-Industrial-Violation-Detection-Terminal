#!/usr/bin/env python3
"""Set HLK-LD2410B no-person duration over UART.

Wiring for a temporary USB-TTL configuration session:
  USB-TTL GND -> LD2410 GND
  USB-TTL RXD -> LD2410 TX
  USB-TTL TXD -> LD2410 RX
  LD2410 VCC  -> 5V

Default LD2410B UART settings: 256000 baud, 8N1.
"""

from __future__ import annotations

import argparse
import sys
import time
from dataclasses import dataclass

import serial


HEADER = bytes.fromhex("FD FC FB FA")
TAIL = bytes.fromhex("04 03 02 01")

CMD_ENABLE_CONFIG = 0x00FF
CMD_END_CONFIG = 0x00FE
CMD_SET_GATE_AND_DURATION = 0x0060
CMD_READ_PARAMS = 0x0061


@dataclass
class RadarParams:
    max_motion_gate: int
    max_static_gate: int
    no_person_duration_s: int


def le16(value: int) -> bytes:
    return value.to_bytes(2, "little")


def le32(value: int) -> bytes:
    return value.to_bytes(4, "little")


def build_frame(command: int, payload: bytes = b"") -> bytes:
    data = le16(command) + payload
    return HEADER + le16(len(data)) + data + TAIL


def read_command_frame(port: serial.Serial, timeout_s: float = 2.0) -> bytes:
    deadline = time.monotonic() + timeout_s
    window = bytearray()

    while time.monotonic() < deadline:
        byte = port.read(1)
        if not byte:
            continue

        window += byte
        if len(window) > len(HEADER):
            del window[0 : len(window) - len(HEADER)]

        if bytes(window) != HEADER:
            continue

        length_raw = port.read(2)
        if len(length_raw) != 2:
            break

        length = int.from_bytes(length_raw, "little")
        data = port.read(length)
        tail = port.read(len(TAIL))
        if len(data) != length or tail != TAIL:
            raise RuntimeError("Received malformed LD2410 command frame")

        return data

    raise TimeoutError("Timed out waiting for LD2410 ACK frame")


def send_command(port: serial.Serial, command: int, payload: bytes = b"", timeout_s: float = 2.0) -> bytes:
    port.write(build_frame(command, payload))
    port.flush()

    data = read_command_frame(port, timeout_s)
    if len(data) < 4:
        raise RuntimeError(f"ACK for 0x{command:04X} is too short: {data.hex(' ')}")

    ack_command = int.from_bytes(data[0:2], "little")
    expected_ack_command = command | 0x0100
    if ack_command != expected_ack_command:
        raise RuntimeError(
            f"Unexpected ACK command 0x{ack_command:04X}, expected 0x{expected_ack_command:04X}"
        )

    status = int.from_bytes(data[2:4], "little")
    if status != 0:
        raise RuntimeError(f"LD2410 command 0x{command:04X} failed with status {status}")

    return data[4:]


def enable_config(port: serial.Serial) -> None:
    send_command(port, CMD_ENABLE_CONFIG, le16(0x0001))


def end_config(port: serial.Serial) -> None:
    send_command(port, CMD_END_CONFIG)


def read_params(port: serial.Serial) -> RadarParams:
    payload = send_command(port, CMD_READ_PARAMS)
    if len(payload) < 24 or payload[0] != 0xAA:
        raise RuntimeError(f"Unexpected parameter payload: {payload.hex(' ')}")

    gate_count = payload[1]
    max_motion_gate = payload[2]
    max_static_gate = payload[3]
    duration_offset = 4 + (gate_count + 1) * 2
    if len(payload) < duration_offset + 2:
        raise RuntimeError(f"Parameter payload is too short: {payload.hex(' ')}")

    no_person_duration_s = int.from_bytes(payload[duration_offset : duration_offset + 2], "little")
    return RadarParams(max_motion_gate, max_static_gate, no_person_duration_s)


def set_gate_and_duration(
    port: serial.Serial,
    max_motion_gate: int,
    max_static_gate: int,
    no_person_duration_s: int,
) -> None:
    payload = (
        le16(0x0000)
        + le32(max_motion_gate)
        + le16(0x0001)
        + le32(max_static_gate)
        + le16(0x0002)
        + le32(no_person_duration_s)
    )
    send_command(port, CMD_SET_GATE_AND_DURATION, payload)


def validate_args(args: argparse.Namespace) -> None:
    if not 0 <= args.seconds <= 65535:
        raise ValueError("--seconds must be in 0..65535")
    if args.motion_gate is not None and not 2 <= args.motion_gate <= 8:
        raise ValueError("--motion-gate must be in 2..8")
    if args.static_gate is not None and not 2 <= args.static_gate <= 8:
        raise ValueError("--static-gate must be in 2..8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Set HLK-LD2410B no-person duration.")
    parser.add_argument("--port", help="Serial port, for example COM8")
    parser.add_argument("--seconds", type=int, default=2, help="No-person duration in seconds, default: 2")
    parser.add_argument("--baud", type=int, default=256000, help="LD2410 UART baud rate, default: 256000")
    parser.add_argument("--motion-gate", type=int, help="Override max motion distance gate, 2..8")
    parser.add_argument("--static-gate", type=int, help="Override max static distance gate, 2..8")
    parser.add_argument("--dry-run", action="store_true", help="Print the write frame without opening a serial port")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    validate_args(args)

    motion_gate = args.motion_gate or 8
    static_gate = args.static_gate or 8
    write_payload = (
        le16(0x0000)
        + le32(motion_gate)
        + le16(0x0001)
        + le32(static_gate)
        + le16(0x0002)
        + le32(args.seconds)
    )

    if args.dry_run:
        print("enable:", build_frame(CMD_ENABLE_CONFIG, le16(0x0001)).hex(" "))
        print("write :", build_frame(CMD_SET_GATE_AND_DURATION, write_payload).hex(" "))
        print("end   :", build_frame(CMD_END_CONFIG).hex(" "))
        return 0

    if not args.port:
        raise ValueError("--port is required unless --dry-run is used")

    with serial.Serial(args.port, args.baud, timeout=0.1) as port:
        port.reset_input_buffer()
        port.reset_output_buffer()

        enable_config(port)
        current = read_params(port)
        print(
            "current:",
            f"motion_gate={current.max_motion_gate}",
            f"static_gate={current.max_static_gate}",
            f"no_person_duration_s={current.no_person_duration_s}",
        )

        motion_gate = args.motion_gate or current.max_motion_gate
        static_gate = args.static_gate or current.max_static_gate
        set_gate_and_duration(port, motion_gate, static_gate, args.seconds)
        updated = read_params(port)
        end_config(port)

    print(
        "updated:",
        f"motion_gate={updated.max_motion_gate}",
        f"static_gate={updated.max_static_gate}",
        f"no_person_duration_s={updated.no_person_duration_s}",
    )

    if updated.no_person_duration_s != args.seconds:
        raise RuntimeError("Readback duration does not match requested value")

    print("done")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
