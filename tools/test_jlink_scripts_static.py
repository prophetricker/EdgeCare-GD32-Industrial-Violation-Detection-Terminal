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


def require_not_contains(path: Path, needle: str) -> None:
    text = path.read_text(encoding="utf-8", errors="ignore").replace("\r\n", "\n")
    if needle in text:
        raise AssertionError(f"{path} must not contain {needle!r}")


def main() -> int:
    probe = TOOLS / "jlink_probe.ps1"
    flash = TOOLS / "jlink_flash_edgecare.ps1"
    gdbserver = TOOLS / "jlink_gdbserver_edgecare.ps1"
    reset_capture = TOOLS / "jlink_reset_capture_serial.ps1"
    health = TOOLS / "jlink_swd_health_check.ps1"
    save_frame = TOOLS / "jlink_save_camera_frame.ps1"
    sweep_capture = TOOLS / "capture_camera_jpeg_yuv_order_sweep_log.ps1"
    photo_evidence = TOOLS / "camera_photo_evidence_run.ps1"
    verify = TOOLS / "verify_edgecare.ps1"

    require_contains(probe, "ShowEmuList")
    require_contains(probe, "mem32 0xE000ED00,1")
    require_contains(probe, "Cortex-M7")
    require_contains(probe, "-NoGui")
    require_contains(probe, "CPUID")
    require_contains(probe, "$JLinkSerial")
    require_contains(probe, "Get-JLinkUsbArgs")
    require_contains(probe, '"-USB", $JLinkSerial')
    require_contains(probe, "TimeoutSec")
    require_contains(probe, "Start-Process")
    require_contains(probe, "WaitForExit")

    require_contains(flash, "[switch]$Flash")
    require_contains(flash, "Dry run")
    require_contains(flash, "loadfile")
    require_contains(flash, "EdgeCare_GD32_Terminal.axf")
    require_contains(flash, "GD32H759IMT6")
    require_contains(flash, "Refusing to flash with generic Device=Cortex-M7")
    require_contains(flash, "JLink.exe")
    require_contains(flash, "-Flash")
    require_contains(flash, "$JLinkSerial")
    require_contains(flash, "Get-JLinkUsbArgs")
    require_contains(flash, '"-USB", $JLinkSerial')
    require_contains(flash, "TimeoutSec")
    require_contains(flash, "Start-Process")
    require_contains(flash, "RedirectStandardOutput")
    require_contains(flash, "WaitForExit")
    require_contains(flash, "flash_program_verify_ok=1")
    require_contains(flash, "Programming flash")
    require_contains(flash, "Verifying flash")
    require_not_contains(flash, '("loadfile ""{0}""" -f $ResolvedAxf),\n    "r",\n    "g"')
    require_not_contains(flash, '    "h",')
    require_not_contains(flash, '    "g",')

    require_contains(gdbserver, "JLinkGDBServerCL.exe")
    require_contains(gdbserver, "GD32H759IMT6")
    require_contains(gdbserver, "-port")
    require_contains(gdbserver, "2331")

    require_contains(health, "ShowHWStatus")
    require_contains(health, "ReadDP 0")
    require_contains(health, "ReadDP 3")
    require_contains(health, "Get-JLinkUsbArgs")
    require_contains(health, '"-USB", $JLinkSerial')
    require_contains(health, "ClrRESET")
    require_contains(health, "SetRESET")
    require_contains(health, "System.IO.Ports.SerialPort")
    require_contains(health, "COM8")
    require_contains(health, "ready_to_flash_sweep")
    require_contains(health, "swd_dp_ok=")
    require_contains(health, "reset_line_observed=")
    require_contains(health, "serial_lines=")
    require_contains(health, "fix_swdio_swclk_pa13_pa14_connection")
    require_contains(health, "no flash performed")
    require_contains(health, "Stop-Process")
    require_in_order(
        health,
        [
            "== Sweep artifact ==",
            "== J-Link SWD DP ==",
            "== J-Link RESET line ==",
            "== COM serial ==",
            "== Health summary ==",
        ],
    )

    require_contains(reset_capture, "System.IO.Ports.SerialPort")
    require_contains(reset_capture, "COM8")
    require_contains(reset_capture, "115200")
    require_contains(reset_capture, "GD32H759IMT6")
    require_contains(reset_capture, "Get-JLinkUsbArgs")
    require_contains(reset_capture, '"-USB", $JLinkSerial')
    require_contains(reset_capture, "JLinkTimeoutSec")
    require_contains(reset_capture, "Start-Process")
    require_contains(reset_capture, "WaitForExit")
    require_contains(reset_capture, "camera_data_pad_sweep")
    require_contains(reset_capture, "camera_data_pad_summary")
    require_contains(reset_capture, "camera_raw_pclk_sample")
    require_contains(reset_capture, "camera_forced_sync_dci")
    require_contains(reset_capture, "camera_dci_sync_matrix")
    require_contains(reset_capture, "camera_dci_dma_summary")
    require_contains(reset_capture, "camera_capture_mode_sweep")
    require_contains(reset_capture, "jpeg_yuv_order_capture_sweep")
    require_contains(reset_capture, "camera_byte_scale")
    require_contains(reset_capture, "analyze_camera_byte_scale_log.py")
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

    require_contains(save_frame, "$JLinkSerial")
    require_contains(save_frame, "Get-JLinkUsbArgs")
    require_contains(save_frame, '"-USB", $JLinkSerial')
    require_contains(save_frame, "Start-Process")
    require_contains(save_frame, "WaitForExit")

    require_contains(sweep_capture, "$JLinkSerial")
    require_contains(sweep_capture, "-JLinkSerial")

    require_contains(photo_evidence, "[switch]$Flash")
    require_contains(photo_evidence, "Dry run")
    require_contains(photo_evidence, "Re-run with -Flash only after the user explicitly authorizes")
    require_contains(photo_evidence, "jlink_flash_edgecare.ps1")
    require_contains(photo_evidence, "-JLinkSerial $JLinkSerial")
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
    require_contains(verify, "jlink_swd_health_check.ps1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
