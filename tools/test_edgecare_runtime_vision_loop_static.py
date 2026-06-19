from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = (
    ROOT
    / "EdgeCare_GD32_Industrial_Violation_Terminal"
    / "GD32H759I_START_Demo_Suites"
    / "Projects"
    / "01_EdgeCare_Industrial_Violation_Terminal"
)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require_contains(text: str, needle: str, path: Path) -> None:
    assert needle in text, f"{path} missing {needle!r}"


def test_runtime_loop_uses_camera_model_vote_instead_of_radar_direct_alarm() -> None:
    app_path = PROJECT / "app" / "edgecare_app.c"
    app_c = read_text(app_path)

    require_contains(app_c, "static edgecare_vote_state_t g_vote_state;", app_path)
    require_contains(app_c, "static uint8_t edgecare_run_vision_frame(void)", app_path)
    require_contains(app_c, "bsp_camera_ov5640_capture_frame()", app_path)
    require_contains(app_c, "edgecare_preprocess_current_frame(&stats)", app_path)
    require_contains(app_c, "edgecare_vote_update(&g_vote_state, &infer_result)", app_path)
    require_contains(app_c, "vision_probe: capture=ok model=%s", app_path)
    require_contains(app_c, "uint8_t vision_alarm = edgecare_run_vision_frame();", app_path)
    require_contains(app_c, "edgecare_alarm_set(vision_alarm);", app_path)

    direct_radar_alarm = (
        "if(g_edgecare.radar_triggered) {\n"
        "        g_edgecare.state = EDGECARE_STATE_ALARM;\n"
        "        g_edgecare.confidence_percent = 100U;\n"
        "        edgecare_alarm_set(1U);"
    )
    assert direct_radar_alarm not in app_c


def test_camera_runtime_capture_api_is_declared_and_implemented() -> None:
    header_path = PROJECT / "bsp" / "bsp_camera_ov5640.h"
    source_path = PROJECT / "bsp" / "bsp_camera_ov5640.c"
    header = read_text(header_path)
    source = read_text(source_path)

    require_contains(header, "uint8_t bsp_camera_ov5640_capture_frame(void);", header_path)
    require_contains(source, "uint8_t bsp_camera_ov5640_capture_frame(void)", source_path)
    require_contains(source, 'edgecare_camera_capture_attempt("runtime_jpeg_to_yuv_ref"', source_path)


def test_runtime_camera_capture_uses_quiet_capture_attempt() -> None:
    source_path = PROJECT / "bsp" / "bsp_camera_ov5640.c"
    source = read_text(source_path)

    require_contains(
        source,
        "camera_frame_quality_t *quality,\n                                               uint8_t verbose)",
        source_path,
    )
    require_contains(source, "if(verbose) {\n        printf(\"camera_capture[%s]:", source_path)
    require_contains(
        source,
        'edgecare_camera_capture_attempt("normal_jpeg_to_yuv_ref",\n'
        "                                           DCI_CK_POLARITY_RISING,\n"
        "                                           DCI_HSYNC_POLARITY_LOW,\n"
        "                                           DCI_VSYNC_POLARITY_HIGH,\n"
        "                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,\n"
        "                                           &normal_quality,\n"
        "                                           1U)",
        source_path,
    )
    require_contains(
        source,
        'edgecare_camera_capture_attempt("runtime_jpeg_to_yuv_ref",\n'
        "                                           DCI_CK_POLARITY_RISING,\n"
        "                                           DCI_HSYNC_POLARITY_LOW,\n"
        "                                           DCI_VSYNC_POLARITY_HIGH,\n"
        "                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,\n"
        "                                           &quality,\n"
        "                                           0U)",
        source_path,
    )
