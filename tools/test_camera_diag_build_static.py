from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"


def require_contains(path: Path, needle: str) -> None:
    text = path.read_text(encoding="utf-8", errors="ignore")
    if needle not in text:
        raise AssertionError(f"{path} is missing {needle!r}")


def require_absent(path: Path, needle: str) -> None:
    text = path.read_text(encoding="utf-8", errors="ignore")
    if needle in text:
        raise AssertionError(f"{path} unexpectedly contains {needle!r}")


def require_in_order(path: Path, needles: list[str]) -> None:
    text = path.read_text(encoding="utf-8", errors="ignore")
    offset = 0
    for needle in needles:
        index = text.find(needle, offset)
        if index < 0:
            raise AssertionError(f"{path} is missing {needle!r} after offset {offset}")
        offset = index + len(needle)


def main() -> int:
    script = TOOLS / "build_camera_jpeg_yuv_order_sweep.ps1"
    check_script = TOOLS / "check_camera_jpeg_yuv_order_sweep_build.ps1"
    capture_script = TOOLS / "capture_camera_jpeg_yuv_order_sweep_log.ps1"
    verify = TOOLS / "verify_edgecare.ps1"

    require_contains(script, "[switch]$Build")
    require_contains(script, "Dry run")
    require_contains(script, "This script never flashes")
    require_contains(script, "$MacroName = \"EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP\"")
    require_contains(script, "$OriginalText = [System.IO.File]::ReadAllText($BoardConfig)")
    require_contains(script, "build_keil.ps1")
    require_contains(script, "camera_jpeg_yuv_order_sweep_manifest.json")
    require_contains(script, "ConvertTo-Json")
    require_contains(script, "sweep_enabled")
    require_contains(script, "artifact_last_write_utc")
    require_contains(script, "$Pattern = \"(?m)^(\\s*#define\\s+$MacroName\\s+)0U(\\s*)$\"")
    require_contains(script, "$Replacement = \"`${1}1U`${2}\"")
    require_contains(script, "finally")
    require_contains(script, "[System.IO.File]::WriteAllText($BoardConfig, $OriginalText")
    require_contains(script, "Restored board_config.h")
    require_absent(script, "jlink_flash_edgecare.ps1")
    require_absent(script, "-Flash")
    require_in_order(
        script,
        [
            "if(-not $Build)",
            "exit 0",
            "$OriginalText = [System.IO.File]::ReadAllText($BoardConfig)",
            "[System.IO.File]::WriteAllText($BoardConfig, $DiagnosticText",
            "powershell.exe -ExecutionPolicy Bypass -File $BuildScript",
            "$Manifest | ConvertTo-Json",
            "finally",
            "[System.IO.File]::WriteAllText($BoardConfig, $OriginalText",
        ],
    )

    require_contains(check_script, "camera_jpeg_yuv_order_sweep_manifest.json")
    require_contains(check_script, "sweep_enabled")
    require_contains(check_script, "artifact_last_write_utc")
    require_contains(check_script, "current_artifact_matches_manifest")
    require_contains(check_script, "ready_to_flash_sweep=1")
    require_contains(check_script, "ready_to_flash_sweep=0")
    require_absent(check_script, "Set-Content")
    require_absent(check_script, "WriteAllText")
    require_absent(check_script, "jlink_flash_edgecare.ps1")
    require_absent(check_script, "-Flash")

    require_contains(capture_script, "check_camera_jpeg_yuv_order_sweep_build.ps1")
    require_contains(capture_script, "ready_to_flash_sweep=1")
    require_contains(capture_script, "jlink_reset_capture_serial.ps1")
    require_contains(capture_script, "analyze_camera_byte_scale_log.py")
    require_contains(capture_script, "jpeg_yuv_order_capture_sweep=1")
    require_absent(capture_script, "build_keil.ps1")
    require_absent(capture_script, "build_camera_jpeg_yuv_order_sweep.ps1 -Build")
    require_absent(capture_script, "jlink_flash_edgecare.ps1")
    require_absent(capture_script, "-Flash")

    require_contains(verify, "camera diagnostic build script static test")
    require_contains(verify, "test_camera_diag_build_static.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
