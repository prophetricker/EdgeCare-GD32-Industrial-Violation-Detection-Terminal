#!/usr/bin/env python3
"""Collect EdgeCare UART image dumps and save them as viewable image files."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import sys
from pathlib import Path


IMAGE_BEGIN_RE = re.compile(
    r"image_dump_begin: .*?kind=(?P<kind>\S+) variant=(?P<variant>\S+) "
    r"width=(?P<width>\d+) height=(?P<height>\d+) "
    r"bytes=(?P<bytes>\d+) checksum=0x(?P<checksum>[0-9A-Fa-f]+)"
)
LEGACY_BEGIN_RE = re.compile(
    r"gray96_dump_begin: .*?(?:variant=(?P<variant>\S+) )?"
    r"width=(?P<width>\d+) height=(?P<height>\d+) "
    r"bytes=(?P<bytes>\d+) checksum=0x(?P<checksum>[0-9A-Fa-f]+)"
)
DATA_RE = re.compile(r"(?:image|gray96)_dump_data: offset=(?P<offset>\d+) hex=(?P<hex>[0-9A-Fa-f]+)")
END_RE = re.compile(r"(?:image|gray96)_dump_end: bytes=(?P<bytes>\d+)")


class DumpCollector:
    def __init__(
        self,
        out_dir: Path,
        diag_out_dir: Path,
        label: str,
        limit: int,
        variant_filter: str,
        kind_filter: str,
        echo: bool,
    ) -> None:
        self.out_dir = out_dir
        self.diag_out_dir = diag_out_dir
        self.label = label
        self.limit = limit
        self.variant_filter = variant_filter
        self.kind_filter = kind_filter
        self.echo = echo
        self.saved = 0
        self.current: dict[str, object] | None = None
        self.last_progress_at = dt.datetime.now()

    def handle_line(self, line: str) -> bool:
        line = line.strip()
        if not line:
            return False

        begin = IMAGE_BEGIN_RE.search(line)
        legacy_begin = None if begin else LEGACY_BEGIN_RE.search(line)
        if legacy_begin:
            begin = legacy_begin

        if begin:
            kind = begin.groupdict().get("kind") or "gray96"
            variant = begin.group("variant") or "legacy"
            if self.kind_filter != "any" and kind != self.kind_filter:
                self.current = None
                print(f"skip frame kind={kind} variant={variant}")
                return False
            if self.variant_filter != "any" and variant != self.variant_filter:
                self.current = None
                print(f"skip frame kind={kind} variant={variant}")
                return False

            frame_bytes = int(begin.group("bytes"))
            self.current = {
                "kind": kind,
                "variant": variant,
                "width": int(begin.group("width")),
                "height": int(begin.group("height")),
                "bytes": frame_bytes,
                "checksum": begin.group("checksum").upper(),
                "data": bytearray(frame_bytes),
                "filled": [False] * frame_bytes,
            }
            print(f"begin frame kind={kind} variant={variant} bytes={frame_bytes} checksum=0x{begin.group('checksum').upper()}")
            self.last_progress_at = dt.datetime.now()
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
            self.last_progress_at = dt.datetime.now()
            return False

        end = END_RE.search(line)
        if end and self.current is not None:
            expected = int(self.current["bytes"])
            actual = int(end.group("bytes"))
            if expected != actual:
                raise ValueError(f"end bytes mismatch: expected={expected} actual={actual}")
            self._save_current()
            self.current = None
            self.last_progress_at = dt.datetime.now()
            return self.saved >= self.limit

        if self.echo and (
            line.startswith("edgecare-01 ")
            or line.startswith("camera_")
            or line.startswith("preprocess_")
            or line.startswith("infer_")
            or line.startswith("[")
        ):
            print(f"serial: {line}")
            self.last_progress_at = dt.datetime.now()

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
        variant = str(self.current["variant"])
        data = self.current["data"]
        assert isinstance(data, bytearray)

        kind = str(self.current["kind"])
        sample_root = self.out_dir if kind == "gray96" else self.diag_out_dir
        sample_dir = sample_root / self.label
        sample_dir.mkdir(parents=True, exist_ok=True)

        stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        base = f"{stamp}_{self.label}_{kind}_{variant}_{width}x{height}_{checksum}"
        pgm_path = sample_dir / f"{base}.pgm"
        bmp_path = sample_dir / f"{base}.bmp"
        json_path = sample_dir / f"{base}.json"

        with pgm_path.open("wb") as fp:
            fp.write(f"P5\n{width} {height}\n255\n".encode("ascii"))
            fp.write(data)
        write_gray8_bmp(bmp_path, width, height, data)

        meta = {
            "label": self.label,
            "kind": kind,
            "variant": variant,
            "width": width,
            "height": height,
            "bytes": len(data),
            "checksum": checksum,
            "formats": ["pgm_p5_gray8", "bmp_gray8"],
            "created_at": dt.datetime.now().isoformat(timespec="seconds"),
        }
        json_path.write_text(json.dumps(meta, indent=2), encoding="utf-8")

        self.saved += 1
        print(f"saved {pgm_path}")
        print(f"saved {bmp_path}")


def write_gray8_bmp(path: Path, width: int, height: int, data: bytes | bytearray) -> None:
    """Write an uncompressed 8-bit grayscale BMP for easy Windows preview."""
    row_stride = ((width + 3) // 4) * 4
    pixel_bytes = row_stride * height
    palette_bytes = 256 * 4
    pixel_offset = 14 + 40 + palette_bytes
    file_size = pixel_offset + pixel_bytes

    with path.open("wb") as fp:
        fp.write(b"BM")
        fp.write(file_size.to_bytes(4, "little"))
        fp.write((0).to_bytes(4, "little"))
        fp.write(pixel_offset.to_bytes(4, "little"))

        fp.write((40).to_bytes(4, "little"))
        fp.write(width.to_bytes(4, "little", signed=True))
        fp.write(height.to_bytes(4, "little", signed=True))
        fp.write((1).to_bytes(2, "little"))
        fp.write((8).to_bytes(2, "little"))
        fp.write((0).to_bytes(4, "little"))
        fp.write(pixel_bytes.to_bytes(4, "little"))
        fp.write((2835).to_bytes(4, "little", signed=True))
        fp.write((2835).to_bytes(4, "little", signed=True))
        fp.write((256).to_bytes(4, "little"))
        fp.write((256).to_bytes(4, "little"))

        for value in range(256):
            fp.write(bytes((value, value, value, 0)))

        pad = b"\x00" * (row_stride - width)
        for row in range(height - 1, -1, -1):
            start = row * width
            fp.write(data[start:start + width])
            fp.write(pad)


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

    try:
        ser = serial.Serial(port=port, baudrate=baudrate, timeout=timeout_sec)
    except serial.SerialException as exc:
        raise SystemExit(
            f"could not open {port}: {exc}\n"
            "Close VS Code Serial Monitor, Keil/uVision serial windows, HLK tools, "
            "PuTTY/MobaXterm, or any previous collect_gray96_serial.py process, then retry. "
            "On Windows a COM port is usually exclusive."
        ) from exc

    with ser:
        print(f"listening on {port} {baudrate} 8N1; press RESET on the board")
        while True:
            raw = ser.readline()
            if raw:
                yield raw.decode("utf-8", errors="replace")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True, choices=["empty", "safe", "intrusion"])
    parser.add_argument("--out", default="data/raw_gray96")
    parser.add_argument("--diag-out", default="data/camera_diagnostics")
    parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout-sec", type=float, default=1.0)
    parser.add_argument("--max-wait-sec", type=float, default=30.0, help="Maximum idle time without serial progress before exiting with diagnostics.")
    parser.add_argument("--input-log", help="Parse an existing text log instead of opening a serial port; use '-' for stdin.")
    parser.add_argument("--variant", default="any", help="Dump variant to save, for example yuv422_y02 or byte_plane0; default saves any variant.")
    parser.add_argument("--kind", default="any", help="Dump kind to save, for example gray96 or byte_plane; default saves any kind.")
    parser.add_argument("--quiet", action="store_true", help="Do not echo non-dump EdgeCare serial lines while waiting.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    collector = DumpCollector(
        Path(args.out),
        Path(args.diag_out),
        args.label,
        args.count,
        args.variant,
        args.kind,
        not args.quiet,
    )
    lines = iter_input_log(args.input_log) if args.input_log else iter_serial(args.port, args.baud, args.timeout_sec)
    for line in lines:
        if collector.handle_line(line):
            return 0
        if not args.input_log and (dt.datetime.now() - collector.last_progress_at).total_seconds() > args.max_wait_sec:
            print(
                f"timed out after {args.max_wait_sec:.1f}s without serial progress: saved {collector.saved}/{args.count} sample(s). "
                "If no 'serial:' lines appeared, check TX/RX wiring, COM port, baudrate, and whether the latest dump-enabled firmware was burned.",
                file=sys.stderr,
            )
            return 2

    if collector.saved < args.count:
        print(f"only saved {collector.saved}/{args.count} sample(s)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
