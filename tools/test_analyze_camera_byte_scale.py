from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "analyze_camera_byte_scale.py"


def run_script(frame: Path) -> str:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), str(frame), "--width", "4", "--height", "2"],
        check=True,
        text=True,
        capture_output=True,
    )
    return result.stdout


def test_reports_right_shift2_like_yuv_frame() -> None:
    # YUYV-like phases: Y around 10..40, U/V around 31/32. After <<2 this
    # resembles normal Y range and neutral chroma near 128.
    data = bytes(
        [
            10,
            31,
            20,
            32,
            15,
            31,
            30,
            32,
            12,
            31,
            22,
            32,
            18,
            31,
            40,
            32,
        ]
    )
    with tempfile.TemporaryDirectory() as tmp_dir:
        frame = Path(tmp_dir) / "frame.bin"
        frame.write_bytes(data)
        output = run_script(frame)

    assert "byte_scale_hint=right_shift2_like" in output
    assert "phase0 raw_min=10 raw_max=18 raw_mean=13 lshift2_min=40 lshift2_max=72 lshift2_mean=55" in output
    assert "phase1 raw_min=31 raw_max=31 raw_mean=31 lshift2_min=124 lshift2_max=124 lshift2_mean=124" in output


if __name__ == "__main__":
    test_reports_right_shift2_like_yuv_frame()
