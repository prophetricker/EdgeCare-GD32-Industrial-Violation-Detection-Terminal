#!/usr/bin/env python3
"""Collect EdgeCare gray96 UART dumps and save them as PGM samples."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import sys
from pathlib import Path


BEGIN_RE = re.compile(
    r"gray96_dump_begin: .*width=(?P<width>\d+) height=(?P<height>\d+) "
    r"bytes=(?P<bytes>\d+) checksum=0x(?P<checksum>[0-9A-Fa-f]+)"
)
DATA_RE = re.compile(r"gray96_dump_data: offset=(?P<offset>\d+) hex=(?P<hex>[0-9A-Fa-f]+)")
END_RE = re.compile(r"gray96_dump_end: bytes=(?P<bytes>\d+)")


class DumpCollector:
    def __init__(self, out_dir: Path, label: str, limit: int) -> None:
        self.out_dir = out_dir
        self.label = label
        self.limit = limit
        self.saved = 0
        self.current: dict[str, object] | None = None

    def handle_line(self, line: str) -> bool:
        line = line.strip()
        if not line:
            return False

        begin = BEGIN_RE.search(line)
        if begin:
            frame_bytes = int(begin.group("bytes"))
            self.current = {
                "width": int(begin.group("width")),
                "height": int(begin.group("height")),
                "bytes": frame_bytes,
                "checksum": begin.group("checksum").upper(),
                "data": bytearray(frame_bytes),
                "filled": [False] * frame_bytes,
            }
            print(f"begin frame bytes={frame_bytes} checksum=0x{begin.group('checksum').upper()}")
            return False

        data = DATA_RE.search(line)
        if data and self.current is not None:
            offset = int(data.group("offset"))
            chunk = bytes.fromhex(data.group("hex"))
            frame_bytes = int(self.current["bytes"])
            end = offset + len(chunk)
            if offset < 0 or end > frame_bytes:
                raise ValueError(f"chunk outside frame: offset={offset} len={len(chunk)} frame={frame_bytes}")

            buffer = self.current["data"]
            filled = self.current["filled"]
            assert isinstance(buffer, bytearray)
            assert isinstance(filled, list)
            buffer[offset:end] = chunk
            filled[offset:end] = [True] * len(chunk)
            return False

        end = END_RE.search(line)
        if end and self.current is not None:
            expected = int(self.current["bytes"])
            actual = int(end.group("bytes"))
            if expected != actual:
                raise ValueError(f"end bytes mismatch: expected={expected} actual={actual}")
            self._save_current()
            self.current = None
            return self.saved >= self.limit

        return False

    def _save_current(self) -> None:
        assert self.current is not None
        filled = self.current["filled"]
        assert isinstance(filled, list)
        if not all(filled):
            missing = filled.count(False)
            raise ValueError(f"incomplete frame: missing {missing} bytes")

        width = int(self.current["width"])
        height = int(self.current["height"])
        checksum = str(self.current["checksum"])
        data = self.current["data"]
        assert isinstance(data, bytearray)

        sample_dir = self.out_dir / self.label
        sample_dir.mkdir(parents=True, exist_ok=True)

        stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        base = f"{stamp}_{self.label}_gray96_{checksum}"
        pgm_path = sample_dir / f"{base}.pgm"
        json_path = sample_dir / f"{base}.json"

        with pgm_path.open("wb") as fp:
            fp.write(f"P5\n{width} {height}\n255\n".encode("ascii"))
            fp.write(data)

        meta = {
            "label": self.label,
            "width": width,
            "height": height,
            "bytes": len(data),
            "checksum": checksum,
            "format": "pgm_p5_gray8",
            "created_at": dt.datetime.now().isoformat(timespec="seconds"),
        }
        json_path.write_text(json.dumps(meta, indent=2), encoding="utf-8")

        self.saved += 1
        print(f"saved {pgm_path}")


def iter_input_log(path: str):
    if path == "-":
        for line in sys.stdin:
            yield line
        return

    with Path(path).open("r", encoding="utf-8", errors="replace") as fp:
        for line in fp:
            yield line


def iter_serial(port: str, baudrate: int, timeout_sec: float):
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("pyserial is required for live capture: py -m pip install pyserial") from exc

    with serial.Serial(port=port, baudrate=baudrate, timeout=timeout_sec) as ser:
        print(f"listening on {port} {baudrate} 8N1; press RESET on the board")
        while True:
            raw = ser.readline()
            if raw:
                yield raw.decode("utf-8", errors="replace")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True, choices=["empty", "safe", "intrusion"])
    parser.add_argument("--out", default="data/raw_gray96")
    parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout-sec", type=float, default=1.0)
    parser.add_argument("--input-log", help="Parse an existing text log instead of opening a serial port; use '-' for stdin.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    collector = DumpCollector(Path(args.out), args.label, args.count)
    lines = iter_input_log(args.input_log) if args.input_log else iter_serial(args.port, args.baud, args.timeout_sec)

    for line in lines:
        if collector.handle_line(line):
            return 0

    if collector.saved < args.count:
        print(f"only saved {collector.saved}/{args.count} sample(s)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
