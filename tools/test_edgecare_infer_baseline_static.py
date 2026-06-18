from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL_DIR = (
    ROOT
    / "EdgeCare_GD32_Industrial_Violation_Terminal"
    / "GD32H759I_START_Demo_Suites"
    / "Projects"
    / "01_EdgeCare_Industrial_Violation_Terminal"
    / "model"
)


def test_firmware_infer_uses_trained_baseline_model() -> None:
    infer_c = (MODEL_DIR / "edgecare_infer.c").read_text(encoding="utf-8")
    assert '#include "edgecare_model_baseline.h"' in infer_c
    assert 'result->model_name = "gray_stats_grid4_logreg_baseline"' in infer_c
    assert "result->is_placeholder = 0U" in infer_c
    assert "(uint64_t)sum" in infer_c
    assert "(uint64_t)grid_sum" in infer_c
    assert "stat_placeholder" not in infer_c


def test_firmware_baseline_header_has_deployable_constants() -> None:
    header = (MODEL_DIR / "edgecare_model_baseline.h").read_text(encoding="utf-8")
    assert "#include <stdint.h>" in header
    assert "#define EDGECARE_BASELINE_FEATURE_COUNT 21" in header
    assert "#define EDGECARE_BASELINE_THRESHOLD_Q15 13107" in header
    assert "#define EDGECARE_BASELINE_LOGIT_THRESHOLD_Q15 -13286" in header
    assert "edgecare_baseline_weights_q15" in header


def test_app_probe_logs_report_real_model_not_placeholder() -> None:
    app_c = (
        ROOT
        / "EdgeCare_GD32_Industrial_Violation_Terminal"
        / "GD32H759I_START_Demo_Suites"
        / "Projects"
        / "01_EdgeCare_Industrial_Violation_Terminal"
        / "app"
        / "edgecare_app.c"
    ).read_text(encoding="utf-8")
    assert "note=trained_baseline" in app_c
    assert "demo_placeholder" not in app_c
    assert "replace_with_trained_model" not in app_c
    assert "vote_probe: threshold=%u.%02u" in app_c


if __name__ == "__main__":
    test_firmware_infer_uses_trained_baseline_model()
    test_firmware_baseline_header_has_deployable_constants()
    test_app_probe_logs_report_real_model_not_placeholder()
