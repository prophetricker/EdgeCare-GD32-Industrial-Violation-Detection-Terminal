#!/usr/bin/env python3
"""Scan EdgeCare gray96 datasets and write a lightweight quality report."""

from __future__ import annotations

import argparse
import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path


LABELS = ("empty", "safe", "intrusion")
EXPECTED_SIZE = (96, 96)


@dataclass
class ImageSample:
    path: Path
    label: str
    width: int
    height: int
    pixels: bytes
    variant: str
    scale: str
    checksum: str
    quality_flags: list[str]


@dataclass
class DatasetReport:
    samples: list[ImageSample]
    counts: dict[str, int]
    bad_variant: int
    bad_scale: int
    bad_size: int
    out_dir: Path


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    offset = 0

    def next_token() -> bytes:
        nonlocal offset
        while offset < len(data) and data[offset] in b" \t\r\n":
            offset += 1
        if offset < len(data) and data[offset] == ord("#"):
            while offset < len(data) and data[offset] not in b"\r\n":
                offset += 1
            return next_token()
        start = offset
        while offset < len(data) and data[offset] not in b" \t\r\n":
            offset += 1
        return data[start:offset]

    magic = next_token()
    if magic != b"P5":
        raise ValueError(f"{path} is not a binary P5 PGM")
    width = int(next_token())
    height = int(next_token())
    max_value = int(next_token())
    if max_value != 255:
        raise ValueError(f"{path} has unsupported max value {max_value}")
    if offset < len(data) and data[offset] in b" \t\r\n":
        offset += 1
    pixels = data[offset:]
    if len(pixels) != width * height:
        raise ValueError(f"{path} pixel bytes mismatch: got {len(pixels)}, expected {width * height}")
    return width, height, pixels


def read_bmp_gray(path: Path) -> tuple[int, int, bytes]:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Pillow is required to read BMP inputs: py -m pip install pillow") from exc

    with Image.open(path) as image:
        gray = image.convert("L")
        width, height = gray.size
        return width, height, gray.tobytes()


def read_gray_image(path: Path) -> tuple[int, int, bytes]:
    suffix = path.suffix.lower()
    if suffix == ".pgm":
        return read_pgm(path)
    if suffix == ".bmp":
        return read_bmp_gray(path)
    raise ValueError(f"unsupported image type: {path}")


def read_metadata(path: Path) -> dict[str, object]:
    meta_path = path.with_suffix(".json")
    if not meta_path.exists():
        return {}
    return json.loads(meta_path.read_text(encoding="utf-8"))


def scan_samples(root: Path, expected_variant: str, require_raw_scale: bool) -> list[ImageSample]:
    samples: list[ImageSample] = []
    for label_dir in sorted(root.iterdir()) if root.exists() else []:
        if not label_dir.is_dir() or label_dir.name not in LABELS:
            continue
        image_paths: dict[str, Path] = {}
        for path in sorted(label_dir.iterdir()):
            suffix = path.suffix.lower()
            if suffix == ".pgm":
                image_paths[path.stem] = path
            elif suffix == ".bmp" and path.stem not in image_paths:
                image_paths[path.stem] = path

        for path in sorted(image_paths.values()):
            width, height, pixels = read_gray_image(path)
            meta = read_metadata(path)
            variant = str(meta.get("variant", "unknown"))
            scale = str(meta.get("scale", "raw"))
            checksum = str(meta.get("checksum", ""))
            flags: list[str] = []
            if (width, height) != EXPECTED_SIZE:
                flags.append("bad_size")
            if expected_variant != "any" and variant != expected_variant:
                flags.append("bad_variant")
            if require_raw_scale and scale != "raw":
                flags.append("bad_scale")
            samples.append(
                ImageSample(
                    path=path,
                    label=label_dir.name,
                    width=width,
                    height=height,
                    pixels=pixels,
                    variant=variant,
                    scale=scale,
                    checksum=checksum,
                    quality_flags=flags,
                )
            )
    return samples


def write_manifest(path: Path, samples: list[ImageSample]) -> None:
    with path.open("w", encoding="utf-8", newline="") as fp:
        writer = csv.DictWriter(
            fp,
            fieldnames=[
                "path",
                "label",
                "width",
                "height",
                "variant",
                "scale",
                "checksum",
                "quality_flags",
            ],
        )
        writer.writeheader()
        for sample in samples:
            writer.writerow(
                {
                    "path": sample.path.as_posix(),
                    "label": sample.label,
                    "width": sample.width,
                    "height": sample.height,
                    "variant": sample.variant,
                    "scale": sample.scale,
                    "checksum": sample.checksum,
                    "quality_flags": ";".join(sample.quality_flags),
                }
            )


def write_pgm(path: Path, width: int, height: int, pixels: bytes) -> None:
    path.write_bytes(f"P5\n{width} {height}\n255\n".encode("ascii") + pixels)


def resize_nearest(pixels: bytes, width: int, height: int, out_w: int, out_h: int) -> bytes:
    out = bytearray(out_w * out_h)
    for y in range(out_h):
        src_y = min(height - 1, (y * height) // out_h)
        for x in range(out_w):
            src_x = min(width - 1, (x * width) // out_w)
            out[y * out_w + x] = pixels[src_y * width + src_x]
    return bytes(out)


def write_contact_sheet(path: Path, samples: list[ImageSample], cols: int, thumb: int = 48) -> None:
    if not samples:
        write_pgm(path, 1, 1, b"\x00")
        return
    cols = max(1, cols)
    rows = int(math.ceil(len(samples) / cols))
    width = cols * thumb
    height = rows * thumb
    canvas = bytearray([32] * width * height)
    for index, sample in enumerate(samples):
        x0 = (index % cols) * thumb
        y0 = (index // cols) * thumb
        tile = resize_nearest(sample.pixels, sample.width, sample.height, thumb, thumb)
        if sample.quality_flags:
            tile = bytes(255 - value for value in tile)
        for y in range(thumb):
            start = (y0 + y) * width + x0
            canvas[start:start + thumb] = tile[y * thumb:(y + 1) * thumb]
    write_pgm(path, width, height, bytes(canvas))


def build_dataset_report(
    root: Path,
    out_dir: Path,
    expected_variant: str,
    require_raw_scale: bool,
    contact_sheet_cols: int,
) -> DatasetReport:
    out_dir.mkdir(parents=True, exist_ok=True)
    samples = scan_samples(root, expected_variant, require_raw_scale)
    counts = {label: 0 for label in LABELS}
    for sample in samples:
        counts[sample.label] = counts.get(sample.label, 0) + 1
    write_manifest(out_dir / "dataset_manifest.csv", samples)
    write_contact_sheet(out_dir / "contact_sheet.pgm", samples, contact_sheet_cols)
    summary = {
        "root": str(root),
        "samples": len(samples),
        "counts": counts,
        "bad_variant": sum("bad_variant" in sample.quality_flags for sample in samples),
        "bad_scale": sum("bad_scale" in sample.quality_flags for sample in samples),
        "bad_size": sum("bad_size" in sample.quality_flags for sample in samples),
    }
    (out_dir / "dataset_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return DatasetReport(
        samples=samples,
        counts={label: count for label, count in counts.items() if count > 0},
        bad_variant=int(summary["bad_variant"]),
        bad_scale=int(summary["bad_scale"]),
        bad_size=int(summary["bad_size"]),
        out_dir=out_dir,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default="data/raw_gray96")
    parser.add_argument("--out", default="data/reports/latest")
    parser.add_argument("--expected-variant", default="jpeg_to_yuv_ref_y02")
    parser.add_argument("--allow-nonraw-scale", action="store_true")
    parser.add_argument("--contact-sheet-cols", type=int, default=8)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report = build_dataset_report(
        root=Path(args.root),
        out_dir=Path(args.out),
        expected_variant=args.expected_variant,
        require_raw_scale=not args.allow_nonraw_scale,
        contact_sheet_cols=args.contact_sheet_cols,
    )
    print(f"samples={len(report.samples)}")
    print(f"counts={report.counts}")
    print(f"bad_variant={report.bad_variant} bad_scale={report.bad_scale} bad_size={report.bad_size}")
    print(f"manifest={report.out_dir / 'dataset_manifest.csv'}")
    print(f"contact_sheet={report.out_dir / 'contact_sheet.pgm'}")
    return 0 if report.samples else 1


if __name__ == "__main__":
    raise SystemExit(main())
