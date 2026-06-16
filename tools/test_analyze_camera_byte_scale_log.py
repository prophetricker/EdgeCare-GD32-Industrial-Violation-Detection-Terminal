from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "analyze_camera_byte_scale_log.py"


def run_script(log_path: Path) -> str:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), str(log_path)],
        check=True,
        text=True,
        capture_output=True,
    )
    return result.stdout


def test_summarizes_jpeg_yuv_order_sweep_and_prefers_non_shifted_order() -> None:
    lines = [
        "camera_capture_probe: start jpeg_yuv_order_capture_sweep=1",
        "camera_byte_scale[jpeg_yuv_order_00]: order=0x00 hint=not_right_shift2_like p0_raw=16,220,96 p0_lshift2=64,255,180 p1_raw=90,170,128 p1_lshift2=255,255,255 p2_raw=18,230,104 p2_lshift2=72,255,190 p3_raw=88,168,127 p3_lshift2=255,255,255",
        "camera_byte_scale[jpeg_yuv_order_01]: order=0x01 hint=not_right_shift2_like p0_raw=0,255,110 p0_lshift2=0,255,170 p1_raw=0,255,120 p1_lshift2=0,255,180 p2_raw=0,255,111 p2_lshift2=0,255,171 p3_raw=0,255,121 p3_lshift2=0,255,181",
        "camera_byte_scale[jpeg_yuv_order_02]: order=0x02 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_03]: order=0x03 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_04]: order=0x04 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_05]: order=0x05 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_06]: order=0x06 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_07]: order=0x07 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
    ]
    with tempfile.TemporaryDirectory() as tmp_dir:
        log_path = Path(tmp_dir) / "sweep.log"
        log_path.write_text("\n".join(lines), encoding="utf-8")
        output = run_script(log_path)

    assert "records=8 expected_orders=8 missing_orders=none" in output
    assert "order=0x00 tag=jpeg_yuv_order_00 hint=not_right_shift2_like y_mean=100 chroma_mean=127" in output
    assert "order=0x02 tag=jpeg_yuv_order_02 hint=right_shift2_like y_mean=21 chroma_mean=31" in output
    assert "candidate_orders=0x00,0x01" in output
    assert "conclusion=4745_mapping_candidate_found" in output


def test_reports_missing_orders() -> None:
    text = "\n".join(
        [
            "camera_byte_scale[jpeg_yuv_order_00]: order=0x00 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
            "camera_byte_scale[jpeg_yuv_order_02]: order=0x02 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        ]
    )
    with tempfile.TemporaryDirectory() as tmp_dir:
        log_path = Path(tmp_dir) / "partial.log"
        log_path.write_text(text, encoding="utf-8")
        output = run_script(log_path)

    assert "records=2 expected_orders=8 missing_orders=0x01,0x03,0x04,0x05,0x06,0x07" in output
    assert "conclusion=incomplete_sweep_log" in output


def test_reports_capture_timeout_orders_separately_from_missing_orders() -> None:
    lines = [
        "camera_byte_scale[jpeg_yuv_order_00]: order=0x00 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_01]: order=0x01 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_02]: order=0x02 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_03]: order=0x03 skipped=capture_timeout",
        "camera_byte_scale[jpeg_yuv_order_04]: order=0x04 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_05]: order=0x05 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_06]: order=0x06 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
        "camera_byte_scale[jpeg_yuv_order_07]: order=0x07 hint=right_shift2_like p0_raw=2,52,21 p0_lshift2=8,208,84 p1_raw=28,34,31 p1_lshift2=112,136,124 p2_raw=3,51,22 p2_lshift2=12,204,88 p3_raw=29,33,31 p3_lshift2=116,132,124",
    ]
    with tempfile.TemporaryDirectory() as tmp_dir:
        log_path = Path(tmp_dir) / "timeout.log"
        log_path.write_text("\n".join(lines), encoding="utf-8")
        output = run_script(log_path)

    assert "records=7 expected_orders=8 missing_orders=none timeout_orders=0x03" in output
    assert "conclusion=incomplete_sweep_log" in output


if __name__ == "__main__":
    test_summarizes_jpeg_yuv_order_sweep_and_prefers_non_shifted_order()
    test_reports_missing_orders()
    test_reports_capture_timeout_orders_separately_from_missing_orders()
