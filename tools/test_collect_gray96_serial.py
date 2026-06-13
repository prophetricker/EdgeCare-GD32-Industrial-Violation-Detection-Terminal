from pathlib import Path
import shutil
import tempfile

import collect_gray96_serial


def feed_lines(collector: collect_gray96_serial.DumpCollector, lines: list[str]) -> None:
    for line in lines:
        collector.handle_line(line)


def make_dump(kind: str, variant: str, checksum: str, hex_data: str) -> list[str]:
    byte_count = len(hex_data) // 2
    return [
        f"image_dump_begin: dev=edgecare-01 kind={kind} variant={variant} width=4 height=4 bytes={byte_count} checksum=0x{checksum} format=hex8",
        f"image_dump_data: offset=0 hex={hex_data}",
        f"image_dump_end: bytes={byte_count}",
    ]


def test_current_jpeg_to_yuv_ref_gray96_dump_is_saved_under_label_dir() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_collect_test_"))
    try:
        collector = collect_gray96_serial.DumpCollector(
            out_dir=tmp / "raw_gray96",
            diag_out_dir=tmp / "diag",
            label="safe",
            limit=1,
            variant_filter="jpeg_to_yuv_ref_y02",
            kind_filter="gray96",
            echo=False,
        )

        feed_lines(
            collector,
            make_dump("gray96", "jpeg_to_yuv_ref_y02", "1234ABCD", "000102030405060708090A0B0C0D0E0F"),
        )

        sample_dir = tmp / "raw_gray96" / "safe"
        pgm_files = list(sample_dir.glob("*_safe_gray96_jpeg_to_yuv_ref_y02_4x4_1234ABCD.pgm"))
        bmp_files = list(sample_dir.glob("*_safe_gray96_jpeg_to_yuv_ref_y02_4x4_1234ABCD.bmp"))
        json_files = list(sample_dir.glob("*_safe_gray96_jpeg_to_yuv_ref_y02_4x4_1234ABCD.json"))

        assert len(pgm_files) == 1
        assert len(bmp_files) == 1
        assert len(json_files) == 1
        assert pgm_files[0].read_bytes().endswith(bytes(range(16)))
    finally:
        shutil.rmtree(tmp)


def test_byte_plane_dump_is_routed_to_diagnostic_dir() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_collect_test_"))
    try:
        collector = collect_gray96_serial.DumpCollector(
            out_dir=tmp / "raw_gray96",
            diag_out_dir=tmp / "diag",
            label="empty",
            limit=1,
            variant_filter="byte_plane0",
            kind_filter="byte_plane",
            echo=False,
        )

        feed_lines(
            collector,
            make_dump("byte_plane", "byte_plane0", "DEADBEEF", "0F0E0D0C0B0A09080706050403020100"),
        )

        assert not (tmp / "raw_gray96" / "empty").exists()
        assert len(list((tmp / "diag" / "empty").glob("*_empty_byte_plane_byte_plane0_4x4_DEADBEEF.bmp"))) == 1
    finally:
        shutil.rmtree(tmp)


if __name__ == "__main__":
    test_current_jpeg_to_yuv_ref_gray96_dump_is_saved_under_label_dir()
    test_byte_plane_dump_is_routed_to_diagnostic_dir()
