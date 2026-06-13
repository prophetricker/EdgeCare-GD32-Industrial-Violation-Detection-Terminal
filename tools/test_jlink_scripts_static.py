from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"


def require_contains(path: Path, needle: str) -> None:
    text = path.read_text(encoding="utf-8", errors="ignore")
    if needle not in text:
        raise AssertionError(f"{path} is missing {needle!r}")


def require_in_order(path: Path, needles: list[str]) -> None:
    text = path.read_text(encoding="utf-8", errors="ignore")
    offset = 0
    for needle in needles:
        index = text.find(needle, offset)
        if index < 0:
            raise AssertionError(f"{path} is missing {needle!r} after offset {offset}")
        offset = index + len(needle)


def main() -> int:
    probe = TOOLS / "jlink_probe.ps1"
    flash = TOOLS / "jlink_flash_edgecare.ps1"
    gdbserver = TOOLS / "jlink_gdbserver_edgecare.ps1"
    reset_capture = TOOLS / "jlink_reset_capture_serial.ps1"
    photo_evidence = TOOLS / "camera_photo_evidence_run.ps1"
    verify = TOOLS / "verify_edgecare.ps1"

    require_contains(probe, "ShowEmuList")
    require_contains(probe, "mem32 0xE000ED00,1")
    require_contains(probe, "Cortex-M7")
    require_contains(probe, "-NoGui")
    require_contains(probe, "CPUID")

    require_contains(flash, "[switch]$Flash")
    require_contains(flash, "Dry run")
    require_contains(flash, "loadfile")
    require_contains(flash, "EdgeCare_GD32_Terminal.axf")
    require_contains(flash, "GD32H759IMT6")
    require_contains(flash, "Refusing to flash with generic Device=Cortex-M7")
    require_contains(flash, "JLink.exe")
    require_contains(flash, "-Flash")
    require_contains(flash, "TimeoutSec")
    require_contains(flash, "Start-Process")
    require_contains(flash, "RedirectStandardOutput")
    require_contains(flash, "WaitForExit")

    require_contains(gdbserver, "JLinkGDBServerCL.exe")
    require_contains(gdbserver, "GD32H759IMT6")
    require_contains(gdbserver, "-port")
    require_contains(gdbserver, "2331")

    require_contains(reset_capture, "System.IO.Ports.SerialPort")
    require_contains(reset_capture, "COM8")
    require_contains(reset_capture, "115200")
    require_contains(reset_capture, "GD32H759IMT6")
    require_contains(reset_capture, "camera_data_pad_sweep")
    require_contains(reset_capture, "camera_data_pad_summary")
    require_contains(reset_capture, "camera_raw_pclk_sample")
    require_contains(reset_capture, "camera_forced_sync_dci")
    require_contains(reset_capture, "camera_dci_sync_matrix")
    require_contains(reset_capture, "camera_dci_dma_summary")
    require_contains(reset_capture, "camera_capture_mode_sweep")
    require_contains(reset_capture, "edgecare_reset_capture_")
    require_contains(reset_capture, "Reset-halt target via J-Link before opening serial")
    require_contains(reset_capture, "Running target via J-Link after serial is ready")
    require_in_order(
        reset_capture,
        [
            "Reset-halt target via J-Link before opening serial",
            "$serial.Open()",
            "Running target via J-Link after serial is ready",
            "Capturing serial for $DurationSec seconds",
        ],
    )

    require_contains(photo_evidence, "[switch]$Flash")
    require_contains(photo_evidence, "Dry run")
    require_contains(photo_evidence, "Re-run with -Flash only after the user explicitly authorizes")
    require_contains(photo_evidence, "jlink_flash_edgecare.ps1")
    require_contains(photo_evidence, "jlink_reset_capture_serial.ps1")
    require_contains(photo_evidence, "jlink_save_camera_frame.ps1")
    require_contains(photo_evidence, "decode_camera_frame.py")
    require_contains(photo_evidence, "photo_evidence_real_raw8")
    require_contains(TOOLS / "decode_camera_frame.py", "lshift2")
    require_contains(TOOLS / "decode_camera_frame.py", "left_shift2")
    require_in_order(
        photo_evidence,
        [
            "if(-not $Flash)",
            "exit 0",
            "== Flash firmware ==",
            "== Reset and capture serial ==",
            "== Save camera RAM frame ==",
            "== Decode frame candidates ==",
        ],
    )

    require_contains(verify, "J-Link script static test")
    require_contains(verify, "test_jlink_scripts_static.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
