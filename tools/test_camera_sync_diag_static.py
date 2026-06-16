from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "EdgeCare_GD32_Industrial_Violation_Terminal" / "GD32H759I_START_Demo_Suites" / "Projects" / "01_EdgeCare_Industrial_Violation_Terminal"


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


def require_between(path: Path, start_needle: str, end_needle: str, needles: list[str], absent: list[str] | None = None) -> None:
    text = path.read_text(encoding="utf-8", errors="ignore")
    start = text.find(start_needle)
    if start < 0:
        raise AssertionError(f"{path} is missing section start {start_needle!r}")
    end = text.find(end_needle, start + len(start_needle))
    if end < 0:
        raise AssertionError(f"{path} is missing section end {end_needle!r}")
    section = text[start:end]
    offset = 0
    for needle in needles:
        index = section.find(needle, offset)
        if index < 0:
            raise AssertionError(f"{path} section {start_needle!r} is missing {needle!r} after offset {offset}")
        offset = index + len(needle)
    for needle in absent or []:
        if needle in section:
            raise AssertionError(f"{path} section {start_needle!r} unexpectedly contains {needle!r}")


def main() -> int:
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_SYNC_OUTPUT_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_TIMING_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_DVP_MODE_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_RAW_PCLK_SAMPLE")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_PATTERN_SOURCE_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_OUTPUT_MUX_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_RAW_DCI_MATRIX")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_ISP_PATH_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_TUNE_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_WINDOW_READBACK")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_OV5640_ST_DVP_REFERENCE_PATH")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_DATA_PAD_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_CAPTURE_MODE_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_OV5640_FULL_REFERENCE_INIT")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_DATA_ORDER_SOURCE_SWEEP")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_ENABLE_CAMERA_RAW_DATA_ORDER_CAPTURE_SWEEP")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_sync_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_sync_output_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_timing_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_timing_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_dvp_mode_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_dvp_mode_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_dvp_mode_sweep_best")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_raw_pclk_sample")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_raw_pclk_sample_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_pattern_source_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_pattern_source_sweep_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_data_order_source_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_data_order_source_sweep_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_raw_data_order_capture_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_raw_data_order_capture_sweep_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_output_mux_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_output_mux_sweep_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "mux_isp_yuv422")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "mux_isp_raw_dpc")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "mux_snr_raw")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "mux_isp_raw_cip")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_raw_dci_matrix")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_raw_dci_matrix_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_apply_raw_capture_path")
    require_contains(PROJECT / "vision" / "edgecare_preprocess.h", "edgecare_preprocess_gray96_from_raw8_stride")
    require_contains(PROJECT / "vision" / "edgecare_preprocess.c", "edgecare_preprocess_gray96_from_raw8_stride")
    require_contains(PROJECT / "vision" / "edgecare_preprocess.c", "edgecare_preprocess_gray96_apply_lshift2")
    require_contains(PROJECT / "app" / "edgecare_app.c", "OV5640_SNR_RAW8")
    require_contains(PROJECT / "board" / "board_config.h", "#define CAMERA_RAW8_BYTE_STRIDE 2U")
    require_contains(PROJECT / "board" / "board_config.h", "#define CAMERA_RAW8_BYTE_PHASE 0U")
    require_in_order(
        PROJECT / "app" / "edgecare_app.c",
        [
            "edgecare_preprocess_gray96_from_raw8_stride(bsp_camera_ov5640_frame(),",
            "CAMERA_RAW8_BYTE_STRIDE,",
            "CAMERA_RAW8_BYTE_PHASE,",
            'printf("preprocess_gray96: source=OV5640_SNR_RAW8',
        ],
    )
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_isp_path_regs")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_isp_path_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_isp_path_sweep_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_jpeg_to_yuv_tune_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_jpeg_to_yuv_tune_sweep_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_jpeg_to_yuv_data_order_capture_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_jpeg_to_yuv_data_order_capture_sweep_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_jpeg_to_yuv_byte_scale_log")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "jpeg_yuv_order_capture_sweep=%u")
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP",
            "camera_jpeg_to_yuv_force_8bit_dvp",
            "(void)camera_sccb_write_reg16(0x3034U, 0x18U);",
            "(void)camera_sccb_read_reg16(0x3034U, &read_3034);",
        ],
    )
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_apply_st_dvp_reference_path")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_st_dvp_reference")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "clock_reset_release")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "manual_bright")
    for reg in ("0x3000U", "0x3002U", "0x3004U", "0x3006U", "0x3007U", "0x3008U", "0x3500U", "0x3501U", "0x3502U", "0x3503U", "0x350AU", "0x350BU", "0x3A00U", "0x4000U", "0x4001U", "0x4004U", "0x4005U", "0x5000U", "0x5001U"):
        require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", reg)
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "isp_colorbar")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "dvp_pattern")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "real_scene")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_data_pad_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_ov5640_data_pad_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_dci_status_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_dci_dma_summary")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "dmamux_id=%lu")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "dma_en=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "mem_inc=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "periph_inc=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "pwidth=%lu")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "mwidth=%lu")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "dci_en=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "cap=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "snap=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "dcif=%lu")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "dma_req_ok=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "dcif_ok=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_forced_sync_dci_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_forced_sync_dci")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_dci_sync_matrix_probe")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_dci_sync_matrix")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_capture_mode_sweep")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "capture_mode=")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "window_ms=")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "delay_1ms(1U)")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "DCI_STAT0")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "DCI_STAT1")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "DCI_INTF")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "DMAMUX_RM_CHXCFG((uint32_t)DMA_CH7 + 8U)")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "read_down=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "GPIO_PUPD_PULLDOWN")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "GPIO_PUPD_PULLUP")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_init_path")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_dvp_ext_regs")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_CAMERA_PWON_ACTIVE_HIGH")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_CAMERA_NORMAL_OUTPUT_RGB565")
    require_contains(PROJECT / "board" / "board_config.h", "EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "EDGECARE_CAMERA_PWON_ACTIVE_HIGH")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "PWON_active_high=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_apply_rgb565_capture_path")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_apply_jpeg_to_yuv_ref_capture_path")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_window_readback_log")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "OV5640_RGB565")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "OV5640_JPEG_TO_YUV_REF")
    require_contains(PROJECT / "vision" / "edgecare_preprocess.h", "edgecare_preprocess_gray96_from_rgb565")
    require_contains(PROJECT / "vision" / "edgecare_preprocess.c", "edgecare_preprocess_gray96_from_rgb565")
    require_contains(PROJECT / "app" / "edgecare_app.c", "OV5640_RGB565")
    require_contains(ROOT / "tools" / "decode_camera_frame.py", "decode_rgb565")
    require_contains(ROOT / "tools" / "decode_camera_frame.py", "rgb565_be")
    require_contains(ROOT / "tools" / "decode_camera_frame.py", "rgb565_le")
    for reg in ("0x3007U", "0x3019U", "0x301BU", "0x302EU", "0x4709U", "0x470AU", "0x470BU", "0x4713U", "0x471CU", "0x471FU"):
        require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", reg)
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "(void)camera_sccb_write_reg16(0x3051U, saved_3051);",
            'camera_sync_sweep_log("restored", 2U, 2U);',
        ],
    )
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "CAMERA_TIMING_SWEEP_MIN_HREF_EDGES")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "best_sample.href_edges >= CAMERA_TIMING_SWEEP_MIN_HREF_EDGES")
    for reg in ("0x3017U", "0x3018U", "0x301AU", "0x301BU", "0x301DU", "0x301EU"):
        require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", reg)
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "read_301e=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "strong_match=%u")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "camera_data_pad_summary")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "present_mask=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "strong_mask=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "missing_mask=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "extra_mask=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "zero_float_mask=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "ff_missing_mask=0x%02X")
    require_contains(PROJECT / "bsp" / "bsp_camera_ov5640.c", "301e:0xFC")
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static const uint8_t patterns[]",
            "0x00U",
            "0xFFU",
            "0xAAU",
            "0x55U",
            "0x01U",
            "0x02U",
            "0x04U",
            "0x08U",
            "0x10U",
            "0x20U",
            "0x40U",
            "0x80U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "(void)camera_sccb_read_reg16(0x3017U, &saved_3017);",
            "(void)camera_sccb_read_reg16(0x3018U, &saved_3018);",
            "(void)camera_sccb_read_reg16(0x301AU, &saved_301a);",
            "(void)camera_sccb_read_reg16(0x301BU, &saved_301b);",
            "(void)camera_sccb_read_reg16(0x301DU, &saved_301d);",
            "(void)camera_sccb_read_reg16(0x301EU, &saved_301e);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "(void)camera_sccb_write_reg16(0x301BU, saved_301b);",
            "(void)camera_sccb_write_reg16(0x301AU, saved_301a);",
            "(void)camera_sccb_write_reg16(0x301EU, saved_301e);",
            "(void)camera_sccb_write_reg16(0x301DU, saved_301d);",
            "(void)camera_sccb_write_reg16(0x3018U, saved_3018);",
            "(void)camera_sccb_write_reg16(0x3017U, saved_3017);",
            'printf("camera_data_pad_sweep: restored',
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "(void)camera_sccb_write_reg16(0x300EU, saved_300e);",
            "(void)camera_sccb_write_reg16(0x302EU, saved_302e);",
            "(void)camera_sccb_write_reg16(0x4713U, saved_4713);",
            'printf("camera_dvp_mode_sweep_best: tag=none apply=0',
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "#if EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS",
            "camera_ov5640_sync_output_sweep();",
            "camera_ov5640_timing_sweep();",
            "#endif",
            "camera_ov5640_dvp_mode_sweep();",
            "camera_dci_sync_matrix_probe();",
            'camera_log_dvp_registers("after_dvp_mode_sweep");',
            "camera_ov5640_data_pad_sweep();",
            "camera_raw_pclk_sample_probe();",
            "camera_capture_mode_sweep_probe();",
            "camera_pattern_source_sweep_probe();",
            "camera_ov5640_data_order_source_sweep_probe();",
            "camera_ov5640_raw_data_order_capture_sweep_probe();",
            "camera_ov5640_jpeg_to_yuv_data_order_capture_sweep_probe();",
            "camera_ov5640_output_mux_sweep_probe();",
            "camera_ov5640_raw_dci_matrix_probe();",
            "camera_ov5640_isp_path_sweep_probe();",
            "camera_ov5640_jpeg_to_yuv_tune_sweep_probe();",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_data_order_source_sweep[isp_colorbar]",
            'camera_data_order_source_sweep_apply("isp_colorbar"',
            "0x80U",
            "0x00U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_data_order_source_sweep[real_scene]",
            'camera_data_order_source_sweep_apply("real_scene"',
            "0x00U",
            "0x00U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static const uint8_t data_orders[]",
            "0x00U",
            "0x01U",
            "0x02U",
            "0x03U",
            "0x04U",
            "0x05U",
            "0x06U",
            "0x07U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_data_order_source_sweep_apply",
            "(void)camera_sccb_write_reg16(0x501FU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4745U, data_order);",
            "camera_raw_pclk_sample_probe_tag(raw_tag, all_tag, href_high_tag, href_low_tag);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "(void)camera_sccb_write_reg16(0x4745U, saved_4745);",
            "(void)camera_sccb_write_reg16(0x501FU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x471DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4740U, 0x20U);",
            'camera_isp_path_regs_log("data_order_source_sweep_restored");',
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_raw_data_order_capture_sweep: enabled source=OV5640_501f_snr_raw",
            "camera_apply_raw_capture_path();",
            'edgecare_camera_capture_attempt("raw_order_00"',
            'edgecare_camera_capture_attempt("raw_order_07"',
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            'edgecare_camera_capture_attempt("raw_order_00"',
            "0x00U",
            'edgecare_camera_capture_attempt("raw_order_01"',
            "0x01U",
            'edgecare_camera_capture_attempt("raw_order_02"',
            "0x02U",
            'edgecare_camera_capture_attempt("raw_order_03"',
            "0x03U",
            'edgecare_camera_capture_attempt("raw_order_04"',
            "0x04U",
            'edgecare_camera_capture_attempt("raw_order_05"',
            "0x05U",
            'edgecare_camera_capture_attempt("raw_order_06"',
            "0x06U",
            'edgecare_camera_capture_attempt("raw_order_07"',
            "0x07U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_pattern_source_sweep[isp_colorbar]",
            'camera_pattern_source_capture("source_isp_colorbar_p20"',
            "0x80U",
            "0x00U",
            "0x20U",
            'camera_pattern_source_capture("source_isp_colorbar_p22"',
            "0x80U",
            "0x00U",
            "0x22U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_pattern_source_sweep[dvp_pattern]",
            'camera_pattern_source_capture("source_dvp_pattern_p20"',
            "0x00U",
            "0x05U",
            "0x20U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_ov5640_raw_dci_matrix_probe",
            "raw_matrix_fall_hs_low_vs_low",
            "raw_matrix_fall_hs_low_vs_high",
            "raw_matrix_fall_hs_high_vs_low",
            "raw_matrix_fall_hs_high_vs_high",
            "raw_matrix_rise_hs_low_vs_low",
            "raw_matrix_rise_hs_low_vs_high",
            "raw_matrix_rise_hs_high_vs_low",
            "raw_matrix_rise_hs_high_vs_high",
            "camera_raw_dci_matrix: enabled source=OV5640_501f_snr_raw",
            "camera_apply_raw_capture_path();",
            "(void)edgecare_camera_capture_attempt(variants[i].tag,",
            "camera_raw_dci_matrix: restored",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_apply_raw_capture_path",
            "(void)camera_sccb_write_reg16(0x501FU, 0x04U);",
            "(void)camera_sccb_write_reg16(0x503DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4741U, 0x00U);",
            "(void)camera_sccb_write_reg16(0x471DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4740U, 0x22U);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_apply_rgb565_capture_path(void)\n{",
            "OV5640_RGB565",
            "(void)camera_sccb_write_reg16(0x4300U, 0x6FU);",
            "(void)camera_sccb_write_reg16(0x501FU, 0x01U);",
            "(void)camera_sccb_write_reg16(0x471DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4740U, 0x20U);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            'printf("camera_capture_normal: source=OV5640_RGB565',
            "camera_apply_rgb565_capture_path();",
            'edgecare_camera_capture_attempt("normal_rgb565",',
            "DCI_CK_POLARITY_FALLING",
            "DCI_HSYNC_POLARITY_LOW",
            "DCI_VSYNC_POLARITY_HIGH",
        ],
    )
    require_in_order(
        PROJECT / "app" / "edgecare_app.c",
        [
            "edgecare_preprocess_gray96_from_rgb565(bsp_camera_ov5640_frame(),",
            'printf("preprocess_gray96: source=OV5640_RGB565',
        ],
    )
    require_in_order(
        ROOT / "tools" / "decode_camera_frame.py",
        [
            "def decode_rgb565",
            '"rgb565_be"',
            '"rgb565_le"',
            "rgb_candidates.append",
        ],
    )
    require_in_order(
        ROOT / "tools" / "decode_camera_frame.py",
        [
            "def yuv422_gray96",
            "model_candidates.append",
            '"gray96_y02_96x96"',
            '"gray96_y13_96x96"',
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_apply_isp_yuv_capture_path",
            "(void)camera_sccb_write_reg16(0x501FU, 0x00U);",
            "#if EDGECARE_ENABLE_CAMERA_TEST_PATTERN",
            "#if EDGECARE_CAMERA_TEST_PATTERN_MODE == 1U",
            "(void)camera_sccb_write_reg16(0x503DU, 0x80U);",
            "(void)camera_sccb_write_reg16(0x4741U, 0x00U);",
            "#elif EDGECARE_CAMERA_TEST_PATTERN_MODE == 2U",
            "(void)camera_sccb_write_reg16(0x503DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4741U, 0x05U);",
            "#else",
            "(void)camera_sccb_write_reg16(0x503DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4741U, 0x00U);",
            "#endif",
            "#else",
            "(void)camera_sccb_write_reg16(0x503DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4741U, 0x00U);",
            "#endif",
            "(void)camera_sccb_write_reg16(0x471DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4740U, 0x22U);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            'printf("camera_capture_normal: source=OV5640_SNR_RAW8',
            "camera_apply_raw_capture_path();",
            'edgecare_camera_capture_attempt("normal_snr_raw8",',
            "DCI_CK_POLARITY_RISING",
            "DCI_HSYNC_POLARITY_HIGH",
            "DCI_VSYNC_POLARITY_HIGH",
        ],
    )
    for macro in (
        "#define EDGECARE_ENABLE_CAMERA_CAPTURE_SWEEP 0U",
        "#define EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS 0U",
        "#define EDGECARE_ENABLE_CAMERA_RAW_PCLK_SAMPLE 0U",
        "#define EDGECARE_ENABLE_CAMERA_PATTERN_SOURCE_SWEEP 0U",
        "#define EDGECARE_ENABLE_CAMERA_DATA_ORDER_SOURCE_SWEEP 0U",
        "#define EDGECARE_ENABLE_CAMERA_ISP_PATH_SWEEP 0U",
        "#define EDGECARE_ENABLE_CAMERA_OUTPUT_MUX_SWEEP 0U",
        "#define EDGECARE_ENABLE_CAMERA_RAW_DCI_MATRIX 0U",
        "#define EDGECARE_ENABLE_CAMERA_DATA_PAD_SWEEP 0U",
        "#define EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE 0U",
        "#define EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_TUNE_SWEEP 0U",
        "#define EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE 0U",
        "#define EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES 0x0375U",
        "#define EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_GAIN 0x02AU",
        "#define EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING 1U",
        "#define EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE 0U",
        "#define EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE 0U",
        "#define EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP 0U",
        "#define EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF 1U",
        "#define EDGECARE_ENABLE_CAMERA_WINDOW_READBACK 1U",
        "#define EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION 0U",
        "#define EDGECARE_CAMERA_DATA_ORDER_DEFAULT 0x00U",
    ):
        require_contains(PROJECT / "board" / "board_config.h", macro)
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_ov5640_jpeg_to_yuv_data_order_capture_sweep_probe",
            "capture_ok = edgecare_camera_capture_attempt(tags[i],",
            "camera_jpeg_to_yuv_byte_scale_log(tags[i], data_orders[i]);",
            "camera_byte_scale[%s]",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_pattern_source_sweep[real_scene]",
            'camera_pattern_source_capture("source_real_scene_p20"',
            "0x00U",
            "0x00U",
            "0x20U",
            'camera_pattern_source_capture("source_real_scene_p22"',
            "0x00U",
            "0x00U",
            "0x22U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_pattern_source_apply",
            "(void)camera_sccb_write_reg16(0x501FU, 0x00U);",
            "if(0U != reg_503d) {",
            "(void)camera_sccb_write_reg16(0x5584U, 0x40U);",
            "} else {",
            "(void)camera_sccb_write_reg16(0x5584U, 0x10U);",
            "}",
            "(void)camera_sccb_write_reg16(0x503DU, reg_503d);",
            "(void)camera_sccb_write_reg16(0x4741U, reg_4741);",
            "(void)camera_sccb_write_reg16(0x471DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4740U, polarity_reg);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_output_mux_capture",
            "(void)camera_sccb_write_reg16(0x501FU, reg_501f);",
            "(void)camera_sccb_write_reg16(0x503DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4741U, 0x00U);",
            "(void)camera_sccb_write_reg16(0x471DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4740U, 0x20U);",
            "camera_raw_pclk_sample_probe_tag",
            "camera_isp_path_regs_log(tag);",
            "edgecare_camera_capture_attempt(tag,",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_output_mux_sweep[mux_isp_yuv422]",
            'camera_output_mux_capture("mux_isp_yuv422"',
            "0x00U",
            "camera_output_mux_sweep[mux_isp_raw_dpc]",
            'camera_output_mux_capture("mux_isp_raw_dpc"',
            "0x03U",
            "camera_output_mux_sweep[mux_snr_raw]",
            'camera_output_mux_capture("mux_snr_raw"',
            "0x04U",
            "camera_output_mux_sweep[mux_isp_raw_cip]",
            'camera_output_mux_capture("mux_isp_raw_cip"',
            "0x05U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_pattern_source_capture",
            "camera_pattern_source_apply(tag, reg_503d, reg_4741, polarity_reg);",
            "camera_raw_pclk_sample_probe_tag(raw_tag, all_tag, href_high_tag, href_low_tag);",
            "camera_isp_path_regs_log(tag);",
            "edgecare_camera_capture_attempt(tag,",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_isp_path_regs_read",
            "0x3821U",
            "0x3824U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_isp_path_regs_restore",
            "0x3821U",
            "0x3824U",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_isp_path_sweep[jpeg_to_yuv_ref]",
            "camera_apply_jpeg_to_yuv_ref_capture_path();",
            'camera_pattern_source_capture("isp_path_jpeg_to_yuv_ref"',
        ],
    )
    require_between(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        "static void camera_apply_jpeg_to_yuv_ref_capture_path(void)\n{",
        "static void camera_apply_jpeg_to_yuv_ref_tune_variant",
        [
            "(void)camera_sccb_write_reg16(0x3002U, 0x1CU);",
            "(void)camera_sccb_write_reg16(0x3006U, 0xC3U);",
            "(void)camera_sccb_write_reg16(0x3821U, 0x07U);",
            "(void)camera_sccb_write_reg16(0x4300U, 0x30U);",
            "(void)camera_sccb_write_reg16(0x501FU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x460CU, 0x20U);",
            "(void)camera_sccb_write_reg16(0x3824U, 0x04U);",
            "(void)camera_sccb_write_reg16(0x460BU, 0x37U);",
            "(void)camera_sccb_write_reg16(0x503DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4741U, 0x00U);",
            "(void)camera_sccb_write_reg16(0x471DU, 0x00U);",
            "(void)camera_sccb_write_reg16(0x4740U, 0x20U);",
            "#if EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE",
            "camera_jpeg_to_yuv_manual_exposure: request 3503=0x07",
            "(void)camera_sccb_write_reg16(0x3503U, 0x07U);",
            "(void)camera_sccb_write_reg16(0x3500U, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES >> 12U) & 0x0FU));",
            "(void)camera_sccb_write_reg16(0x3501U, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES >> 4U) & 0xFFU));",
            "(void)camera_sccb_write_reg16(0x3502U, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES & 0x0FU) << 4U));",
            "(void)camera_sccb_write_reg16(0x350AU, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_GAIN >> 8U) & 0x03U));",
            "(void)camera_sccb_write_reg16(0x350BU, (uint8_t)(EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_GAIN & 0xFFU));",
            "#endif",
        ],
        absent=[
            "(void)camera_sccb_write_reg16(0x4740U, 0x24U);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_window_readback_log",
            "0x3800U",
            "0x3801U",
            "0x3802U",
            "0x3803U",
            "0x3804U",
            "0x3805U",
            "0x3806U",
            "0x3807U",
            "0x3808U",
            "0x3809U",
            "0x380AU",
            "0x380BU",
            "0x380CU",
            "0x380DU",
            "0x380EU",
            "0x380FU",
            "0x3810U",
            "0x3811U",
            "0x3812U",
            "0x3813U",
            "0x3814U",
            "0x3815U",
            "camera_window_readback[%s]: crop=",
            "output=%ux%u",
            "hts=%u vts=%u",
            "offset=%u,%u",
            "inc=0x%02X,0x%02X",
        ],
    )
    require_between(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        "static void camera_apply_jpeg_to_yuv_ref_capture_path(void)\n{",
        "static void camera_apply_jpeg_to_yuv_ref_tune_variant",
        [
            "#if EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE",
            "camera_jpeg_to_yuv_vfifo_ref_candidate",
            "(void)camera_sccb_write_reg16(0x460CU, 0x22U);",
            "(void)camera_sccb_write_reg16(0x3824U, 0x04U);",
            "(void)camera_sccb_write_reg16(0x460BU, 0x35U);",
            "#else",
            "(void)camera_sccb_write_reg16(0x460CU, 0x20U);",
            "(void)camera_sccb_write_reg16(0x3824U, 0x04U);",
            "(void)camera_sccb_write_reg16(0x460BU, 0x37U);",
            "#endif",
        ],
    )
    require_between(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        "static void camera_apply_jpeg_to_yuv_ref_capture_path(void)\n{",
        "static void camera_apply_jpeg_to_yuv_ref_tune_variant",
        [
            "#elif EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE",
            "camera_jpeg_to_yuv_pclkdiv08_candidate",
            "(void)camera_sccb_write_reg16(0x460CU, 0x20U);",
            "(void)camera_sccb_write_reg16(0x3824U, 0x08U);",
            "(void)camera_sccb_write_reg16(0x460BU, 0x37U);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_apply_jpeg_to_yuv_ref_tune_variant",
            "camera_apply_jpeg_to_yuv_ref_capture_path();",
            "(void)camera_sccb_write_reg16(0x460CU, reg_460c);",
            "(void)camera_sccb_write_reg16(0x3824U, reg_3824);",
            "(void)camera_sccb_write_reg16(0x460BU, reg_460b);",
            "(void)camera_sccb_write_reg16(0x471DU, reg_471d);",
            "(void)camera_sccb_write_reg16(0x4740U, reg_4740);",
            "camera_isp_path_regs_log(tag);",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "static void camera_ov5640_jpeg_to_yuv_tune_sweep_probe",
            "jpeg_tune_base_ref",
            "jpeg_tune_vfifo_init",
            "jpeg_tune_pclkdiv08",
            "jpeg_tune_pclkdiv02",
            "jpeg_tune_polarity22_rise_hh",
            "jpeg_tune_href_gate24",
            'edgecare_camera_capture_attempt(variants[i].tag,',
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "camera_apply_jpeg_to_yuv_ref_capture_path();",
            "#if EDGECARE_ENABLE_CAMERA_WINDOW_READBACK",
            'camera_window_readback_log("normal_jpeg_to_yuv_ref");',
            "#endif",
            "#if EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING",
            'printf("camera_capture_normal: source=OV5640_JPEG_TO_YUV_REF',
            "dci=pclk_rising_hs_blank_low_vs_blank_high",
            'edgecare_camera_capture_attempt("normal_jpeg_to_yuv_ref",',
            "DCI_CK_POLARITY_RISING",
            "DCI_HSYNC_POLARITY_LOW",
            "DCI_VSYNC_POLARITY_HIGH",
            "#else",
            'printf("camera_capture_normal: source=OV5640_JPEG_TO_YUV_REF',
            "dci=pclk_falling_hs_blank_low_vs_blank_high",
            'edgecare_camera_capture_attempt("normal_jpeg_to_yuv_ref",',
            "DCI_CK_POLARITY_FALLING",
            "DCI_HSYNC_POLARITY_LOW",
            "DCI_VSYNC_POLARITY_HIGH",
            "#endif",
        ],
    )
    require_in_order(
        PROJECT / "app" / "edgecare_app.c",
        [
            "#elif (EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV || EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF) && !EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN",
            "edgecare_preprocess_gray96_from_yuyv(bsp_camera_ov5640_frame(),",
            'printf("preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02',
            "scale=raw",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            "dci_capture_enable();",
            "camera_dci_status_probe_begin",
            "camera_dci_dma_summary_log",
            "camera_dci_status_probe_poll",
            "dci_capture_disable();",
            "camera_dci_status_probe_log",
        ],
    )
    require_in_order(
        PROJECT / "bsp" / "bsp_camera_ov5640.c",
        [
            'camera_sync_sweep_log(force_tags[i],',
            "camera_forced_sync_dci_probe(force_tags[i],",
        ],
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
