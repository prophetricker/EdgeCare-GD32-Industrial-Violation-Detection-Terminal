from pathlib import Path
import csv
import json
import shutil
import tempfile

import dataset_quality


def write_pgm(path: Path, width: int, height: int, pixels: bytes) -> None:
    path.write_bytes(f"P5\n{width} {height}\n255\n".encode("ascii") + pixels)


def write_meta(path: Path, label: str, variant: str, scale: str = "raw") -> None:
    path.write_text(
        json.dumps(
            {
                "label": label,
                "kind": "gray96",
                "variant": variant,
                "width": 4,
                "height": 4,
                "scale": scale,
                "checksum": "12345678",
            }
        ),
        encoding="utf-8",
    )


def test_scan_dataset_writes_manifest_and_flags_bad_variant() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_dataset_quality_"))
    try:
        root = tmp / "raw_gray96"
        out = tmp / "out"
        good = root / "empty" / "good_empty.pgm"
        bad = root / "safe" / "bad_safe.pgm"
        good.parent.mkdir(parents=True)
        bad.parent.mkdir(parents=True)

        write_pgm(good, 4, 4, bytes(range(16)))
        write_meta(good.with_suffix(".json"), "empty", "jpeg_to_yuv_ref_y02")
        write_pgm(bad, 4, 4, bytes([255] * 16))
        write_meta(bad.with_suffix(".json"), "safe", "yuv422_y13")

        result = dataset_quality.build_dataset_report(
            root=root,
            out_dir=out,
            expected_variant="jpeg_to_yuv_ref_y02",
            require_raw_scale=True,
            contact_sheet_cols=4,
        )

        assert result.counts == {"empty": 1, "safe": 1}
        assert result.bad_variant == 1
        assert result.bad_scale == 0
        assert (out / "dataset_manifest.csv").exists()
        assert (out / "contact_sheet.pgm").exists()

        rows = list(csv.DictReader((out / "dataset_manifest.csv").open("r", encoding="utf-8")))
        assert rows[0]["label"] == "empty"
        assert rows[0]["variant"] == "jpeg_to_yuv_ref_y02"
        assert "bad_variant" in rows[1]["quality_flags"].split(";")
    finally:
        shutil.rmtree(tmp)


def test_scan_dataset_counts_paired_pgm_bmp_once_prefers_pgm() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_dataset_quality_dedupe_"))
    try:
        root = tmp / "raw_gray96"
        out = tmp / "out"
        sample = root / "empty" / "paired_empty.pgm"
        sample.parent.mkdir(parents=True)

        write_pgm(sample, 4, 4, bytes(range(16)))
        sample.with_suffix(".bmp").write_bytes(b"not a real bmp")
        write_meta(sample.with_suffix(".json"), "empty", "jpeg_to_yuv_ref_y02")

        result = dataset_quality.build_dataset_report(
            root=root,
            out_dir=out,
            expected_variant="jpeg_to_yuv_ref_y02",
            require_raw_scale=True,
            contact_sheet_cols=4,
        )

        assert result.counts == {"empty": 1}
        assert len(result.samples) == 1
        assert result.samples[0].path.suffix == ".pgm"
    finally:
        shutil.rmtree(tmp)


def test_read_pgm_preserves_first_pixel_when_it_is_whitespace_value() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_dataset_quality_pgm_ws_"))
    try:
        path = tmp / "space_first_pixel.pgm"
        write_pgm(path, 2, 2, bytes([32, 33, 34, 35]))

        width, height, pixels = dataset_quality.read_pgm(path)

        assert (width, height) == (2, 2)
        assert pixels == bytes([32, 33, 34, 35])
    finally:
        shutil.rmtree(tmp)


if __name__ == "__main__":
    test_scan_dataset_writes_manifest_and_flags_bad_variant()
    test_scan_dataset_counts_paired_pgm_bmp_once_prefers_pgm()
    test_read_pgm_preserves_first_pixel_when_it_is_whitespace_value()
