#!/usr/bin/env python3
"""Generate offline previews from an EdgeCare raw camera frame dump."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def write_gray8_bmp(path: Path, width: int, height: int, data: bytes | bytearray) -> None:
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


def write_rgb24_bmp(path: Path, width: int, height: int, data: bytes | bytearray) -> None:
    row_stride = ((width * 3 + 3) // 4) * 4
    pixel_bytes = row_stride * height
    pixel_offset = 14 + 40
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
        fp.write((24).to_bytes(2, "little"))
        fp.write((0).to_bytes(4, "little"))
        fp.write(pixel_bytes.to_bytes(4, "little"))
        fp.write((2835).to_bytes(4, "little", signed=True))
        fp.write((2835).to_bytes(4, "little", signed=True))
        fp.write((0).to_bytes(4, "little"))
        fp.write((0).to_bytes(4, "little"))

        pad = b"\x00" * (row_stride - (width * 3))
        for row in range(height - 1, -1, -1):
            start = row * width * 3
            fp.write(data[start:start + (width * 3)])
            fp.write(pad)


def bit_reverse_byte(value: int) -> int:
    value = ((value & 0x55) << 1) | ((value & 0xAA) >> 1)
    value = ((value & 0x33) << 2) | ((value & 0xCC) >> 2)
    return ((value & 0x0F) << 4) | ((value & 0xF0) >> 4)


def normalize(data: bytes | bytearray) -> bytes:
    if not data:
        return b""
    low = min(data)
    high = max(data)
    if high <= low:
        return bytes(data)
    return bytes(((value - low) * 255) // (high - low) for value in data)


def left_shift2(data: bytes | bytearray) -> bytes:
    return bytes(255 if value > 63 else value << 2 for value in data)


def score_image(data: bytes, width: int, height: int) -> dict[str, int]:
    row_delta = 0
    col_delta = 0
    for y in range(height):
        row = y * width
        for x in range(1, width):
            row_delta += abs(data[row + x] - data[row + x - 1])
    for y in range(1, height):
        row = y * width
        prev = row - width
        for x in range(width):
            col_delta += abs(data[row + x] - data[prev + x])
    return {
        "width": width,
        "height": height,
        "min": min(data) if data else 0,
        "max": max(data) if data else 0,
        "mean": sum(data) // len(data) if data else 0,
        "row_delta": row_delta // max(1, width * height),
        "col_delta": col_delta // max(1, width * height),
    }


def save_candidate(out_dir: Path, name: str, width: int, height: int, data: bytes, metadata: dict[str, object]) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    bmp = out_dir / f"{name}.bmp"
    write_gray8_bmp(bmp, width, height, data)
    (out_dir / f"{name}.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")


def save_rgb_candidate(out_dir: Path, name: str, width: int, height: int, data: bytes, metadata: dict[str, object]) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    bmp = out_dir / f"{name}.bmp"
    write_rgb24_bmp(bmp, width, height, data)
    (out_dir / f"{name}.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")


def reshape_exact(data: bytes, width: int, height: int) -> bytes:
    needed = width * height
    if len(data) < needed:
        raise ValueError(f"not enough data for {width}x{height}: {len(data)} < {needed}")
    return data[:needed]


def every_nth(data: bytes, stride: int, phase: int) -> bytes:
    return data[phase::stride]


def downsample2x(data: bytes, width: int, height: int) -> tuple[int, int, bytes]:
    out_w = width // 2
    out_h = height // 2
    out = bytearray(out_w * out_h)
    for y in range(out_h):
        for x in range(out_w):
            i0 = ((y * 2) * width) + (x * 2)
            out[(y * out_w) + x] = (
                data[i0] + data[i0 + 1] + data[i0 + width] + data[i0 + width + 1]
            ) // 4
    return out_w, out_h, bytes(out)


def nearest_same_color(data: bytes, width: int, height: int, x: int, y: int, color: str, pattern: str) -> int:
    total = 0
    count = 0
    for radius in (0, 1, 2):
        for yy in range(max(0, y - radius), min(height, y + radius + 1)):
            for xx in range(max(0, x - radius), min(width, x + radius + 1)):
                if bayer_color_at(pattern, xx, yy) == color:
                    total += data[(yy * width) + xx]
                    count += 1
        if count:
            return total // count
    return data[(y * width) + x]


def bayer_color_at(pattern: str, x: int, y: int) -> str:
    index = ((y & 1) * 2) + (x & 1)
    return pattern[index]


def demosaic_nearest(data: bytes, width: int, height: int, pattern: str) -> bytes:
    out = bytearray(width * height * 3)
    for y in range(height):
        for x in range(width):
            pixel = (y * width) + x
            out_index = pixel * 3
            r = nearest_same_color(data, width, height, x, y, "R", pattern)
            g = nearest_same_color(data, width, height, x, y, "G", pattern)
            b = nearest_same_color(data, width, height, x, y, "B", pattern)
            out[out_index + 0] = b
            out[out_index + 1] = g
            out[out_index + 2] = r
    return bytes(out)


def rgb_score(data: bytes, width: int, height: int) -> dict[str, int]:
    gray = bytearray(width * height)
    for i in range(width * height):
        base = i * 3
        gray[i] = (int(data[base + 0]) + int(data[base + 1]) + int(data[base + 2])) // 3
    return score_image(bytes(gray), width, height)


def clamp_u8(value: int) -> int:
    if value < 0:
        return 0
    if value > 255:
        return 255
    return value


def yuv_to_bgr(y: int, u: int, v: int) -> tuple[int, int, int]:
    c = y - 16
    d = u - 128
    e = v - 128
    r = (298 * c + 409 * e + 128) >> 8
    g = (298 * c - 100 * d - 208 * e + 128) >> 8
    b = (298 * c + 516 * d + 128) >> 8
    return clamp_u8(b), clamp_u8(g), clamp_u8(r)


def decode_yuv422(data: bytes, width: int, height: int, order: str) -> tuple[bytes, bytes]:
    needed = width * height * 2
    if len(data) < needed:
        raise ValueError(f"not enough data for {width}x{height} YUV422: {len(data)} < {needed}")

    gray = bytearray(width * height)
    rgb = bytearray(width * height * 3)
    pairs_per_row = width // 2
    for y in range(height):
        for pair_x in range(pairs_per_row):
            src = ((y * pairs_per_row) + pair_x) * 4
            b0, b1, b2, b3 = data[src], data[src + 1], data[src + 2], data[src + 3]
            if order == "yuyv":
                y0, u, y1, v = b0, b1, b2, b3
            elif order == "yvyu":
                y0, v, y1, u = b0, b1, b2, b3
            elif order == "uyvy":
                u, y0, v, y1 = b0, b1, b2, b3
            elif order == "vyuy":
                v, y0, u, y1 = b0, b1, b2, b3
            else:
                raise ValueError(f"unsupported YUV422 order: {order}")

            pixel0 = (y * width) + (pair_x * 2)
            pixel1 = pixel0 + 1
            gray[pixel0] = y0
            gray[pixel1] = y1

            b, g, r = yuv_to_bgr(y0, u, v)
            dst0 = pixel0 * 3
            rgb[dst0 + 0] = b
            rgb[dst0 + 1] = g
            rgb[dst0 + 2] = r

            b, g, r = yuv_to_bgr(y1, u, v)
            dst1 = pixel1 * 3
            rgb[dst1 + 0] = b
            rgb[dst1 + 1] = g
            rgb[dst1 + 2] = r

    return bytes(gray), bytes(rgb)


def decode_rgb565(data: bytes, width: int, height: int, byte_order: str) -> tuple[bytes, bytes]:
    needed = width * height * 2
    if len(data) < needed:
        raise ValueError(f"not enough data for {width}x{height} RGB565: {len(data)} < {needed}")

    gray = bytearray(width * height)
    rgb = bytearray(width * height * 3)
    for pixel in range(width * height):
        src = pixel * 2
        if byte_order == "rgb565_be":
            value = (data[src] << 8) | data[src + 1]
        elif byte_order == "rgb565_le":
            value = (data[src + 1] << 8) | data[src]
        else:
            raise ValueError(f"unsupported RGB565 byte order: {byte_order}")

        r = ((value >> 11) & 0x1F) << 3
        g = ((value >> 5) & 0x3F) << 2
        b = (value & 0x1F) << 3
        gray[pixel] = ((r * 77) + (g * 150) + (b * 29)) >> 8
        dst = pixel * 3
        rgb[dst + 0] = b
        rgb[dst + 1] = g
        rgb[dst + 2] = r

    return bytes(gray), bytes(rgb)


def yuv422_gray96(data: bytes, y_phase: int, lshift2: bool = False) -> bytes:
    out_w = 96
    out_h = 96
    src_w = 320
    src_h = 240
    out = bytearray(out_w * out_h)
    for y in range(out_h):
        src_y = (y * src_h) // out_h
        for x in range(out_w):
            src_x = (x * src_w) // out_w
            pixel = (src_y * src_w) + src_x
            pair_offset = (pixel // 2) * 4
            y_offset = pair_offset + (2 if pixel & 1 else 0) + y_phase
            value = data[y_offset]
            if lshift2:
                value = 255 if value > 63 else value << 2
            out[(y * out_w) + x] = value
    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frame", type=Path)
    parser.add_argument("--out", type=Path, default=Path("data/raw8_full_frame/decoded"))
    args = parser.parse_args()

    raw = args.frame.read_bytes()
    bitrev = bytes(bit_reverse_byte(value) for value in raw)
    lshift2 = left_shift2(raw)
    bitrev_lshift2 = left_shift2(bitrev)

    candidates: list[tuple[str, int, int, bytes]] = []
    rgb_candidates: list[tuple[str, int, int, bytes]] = []
    model_candidates: list[tuple[str, int, int, bytes]] = []

    for source_name, source in (("raw", raw), ("lshift2", lshift2), ("bitrev", bitrev), ("bitrev_lshift2", bitrev_lshift2)):
        for width, height in ((640, 240), (320, 240), (320, 120), (160, 240), (160, 120)):
            if width * height <= len(source):
                image = reshape_exact(source, width, height)
                candidates.append((f"{source_name}_{width}x{height}", width, height, image))
                candidates.append((f"{source_name}_{width}x{height}_norm", width, height, normalize(image)))
                if width >= 2 and height >= 2:
                    out_w, out_h, out = downsample2x(image, width, height)
                    candidates.append((f"{source_name}_{width}x{height}_down2", out_w, out_h, normalize(out)))

        for stride in (2, 4):
            for phase in range(stride):
                plane = every_nth(source, stride, phase)
                for width, height in ((320, 240), (160, 240), (160, 120)):
                    if width * height <= len(plane):
                        image = reshape_exact(plane, width, height)
                        candidates.append((f"{source_name}_stride{stride}_phase{phase}_{width}x{height}", width, height, image))
                        candidates.append((f"{source_name}_stride{stride}_phase{phase}_{width}x{height}_norm", width, height, normalize(image)))
                        if stride == 2:
                            for pattern in ("RGGB", "GRBG", "GBRG", "BGGR"):
                                rgb = demosaic_nearest(normalize(image), width, height, pattern)
                                rgb_candidates.append((f"{source_name}_stride2_phase{phase}_{width}x{height}_{pattern.lower()}_rgb", width, height, rgb))

        if 320 * 240 * 2 <= len(source):
            for order in ("yuyv", "yvyu", "uyvy", "vyuy"):
                gray, rgb = decode_yuv422(source, 320, 240, order)
                candidates.append((f"{source_name}_yuv422_{order}_y_320x240", 320, 240, gray))
                candidates.append((f"{source_name}_yuv422_{order}_y_320x240_norm", 320, 240, normalize(gray)))
                rgb_candidates.append((f"{source_name}_yuv422_{order}_rgb_320x240", 320, 240, rgb))
            for order in ("rgb565_be", "rgb565_le"):
                gray, rgb = decode_rgb565(source, 320, 240, order)
                candidates.append((f"{source_name}_{order}_gray_320x240", 320, 240, gray))
                candidates.append((f"{source_name}_{order}_gray_320x240_norm", 320, 240, normalize(gray)))
                rgb_candidates.append((f"{source_name}_{order}_rgb_320x240", 320, 240, rgb))

    if 320 * 240 * 2 <= len(raw):
        model_candidates.append(("gray96_y02_96x96", 96, 96, yuv422_gray96(raw, 0)))
        model_candidates.append(("gray96_y02_lshift2_96x96", 96, 96, yuv422_gray96(raw, 0, True)))
        model_candidates.append(("gray96_y13_96x96", 96, 96, yuv422_gray96(raw, 1)))
        model_candidates.append(("gray96_y13_lshift2_96x96", 96, 96, yuv422_gray96(raw, 1, True)))

    summary = []
    for name, width, height, image in candidates:
        metrics = score_image(image, width, height)
        metrics["name"] = name
        save_candidate(args.out, name, width, height, image, metrics)
        summary.append(metrics)
    for name, width, height, image in model_candidates:
        metrics = score_image(image, width, height)
        metrics["name"] = name
        metrics["type"] = "model_gray96"
        save_candidate(args.out, name, width, height, image, metrics)
        summary.append(metrics)
    for name, width, height, image in rgb_candidates:
        metrics = rgb_score(image, width, height)
        metrics["name"] = name
        metrics["type"] = "rgb24_bayer_nearest"
        save_rgb_candidate(args.out, name, width, height, image, metrics)
        summary.append(metrics)

    summary.sort(key=lambda item: (int(item["col_delta"]), int(item["row_delta"])), reverse=True)
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"wrote {len(candidates)} candidates to {args.out}")
    for item in summary[:12]:
        print(
            f"{item['name']}: {item['width']}x{item['height']} "
            f"min={item['min']} max={item['max']} mean={item['mean']} "
            f"row_delta={item['row_delta']} col_delta={item['col_delta']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
