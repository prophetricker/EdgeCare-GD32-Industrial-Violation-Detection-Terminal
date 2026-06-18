from pathlib import Path
import shutil
import tempfile

import dataset_quality
import train_gray96_baseline


def write_sample(path: Path, value: int) -> None:
    pixels = bytes([value] * (96 * 96))
    path.parent.mkdir(parents=True, exist_ok=True)
    dataset_quality.write_pgm(path, 96, 96, pixels)


def test_baseline_training_exports_metrics_and_header() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_train_baseline_"))
    try:
        root = tmp / "raw_gray96"
        out = tmp / "model"
        for i, value in enumerate([8, 16, 24]):
            write_sample(root / "empty" / f"empty_{i}.pgm", value)
        for i, value in enumerate([40, 48, 56]):
            write_sample(root / "safe" / f"safe_{i}.pgm", value)
        for i, value in enumerate([180, 196, 220]):
            write_sample(root / "intrusion" / f"intrusion_{i}.pgm", value)

        result = train_gray96_baseline.train_and_export(
            dataset_root=root,
            out_dir=out,
            epochs=30,
            learning_rate=0.05,
            train_ratio=0.67,
        )

        assert result.total_samples == 9
        assert result.export_samples == 9
        assert result.val_accuracy >= 0.66
        assert (out / "baseline_metrics.json").exists()
        assert (out / "edgecare_model_baseline.h").exists()
        header = (out / "edgecare_model_baseline.h").read_text(encoding="utf-8")
        assert "#include <stdint.h>" in header
        assert "EDGECARE_BASELINE_FEATURE_COUNT" in header
    finally:
        shutil.rmtree(tmp)


def test_export_model_is_retrained_on_all_samples() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_train_export_all_"))
    try:
        root = tmp / "raw_gray96"
        out = tmp / "model"
        for i, value in enumerate([8, 16, 24, 32]):
            write_sample(root / "empty" / f"empty_{i}.pgm", value)
        for i, value in enumerate([40, 48, 56, 64]):
            write_sample(root / "safe" / f"safe_{i}.pgm", value)
        for i, value in enumerate([180, 196, 212, 228]):
            write_sample(root / "intrusion" / f"intrusion_{i}.pgm", value)

        result = train_gray96_baseline.train_and_export(
            dataset_root=root,
            out_dir=out,
            epochs=20,
            learning_rate=0.05,
            train_ratio=0.5,
            seed=3,
        )

        dataset = train_gray96_baseline.load_dataset(root)
        split_train_rows, _val_rows = train_gray96_baseline.split_dataset(dataset, 0.5, 3)
        split_weights = train_gray96_baseline.train_logreg(split_train_rows, 20, 0.05)
        all_weights = train_gray96_baseline.train_logreg(dataset, 20, 0.05)
        assert result.export_weights == all_weights
        assert result.export_weights != split_weights
    finally:
        shutil.rmtree(tmp)


def test_shuffled_training_is_deterministic_with_seed() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_train_shuffle_"))
    try:
        root = tmp / "raw_gray96"
        for i, value in enumerate([8, 16, 24, 32]):
            write_sample(root / "empty" / f"empty_{i}.pgm", value)
        for i, value in enumerate([40, 48, 56, 64]):
            write_sample(root / "safe" / f"safe_{i}.pgm", value)
        for i, value in enumerate([180, 196, 212, 228]):
            write_sample(root / "intrusion" / f"intrusion_{i}.pgm", value)

        dataset = train_gray96_baseline.load_dataset(root)
        ordered = train_gray96_baseline.train_logreg(dataset, 20, 0.05)
        shuffled_a = train_gray96_baseline.train_logreg(dataset, 20, 0.05, shuffle_seed=11)
        shuffled_b = train_gray96_baseline.train_logreg(dataset, 20, 0.05, shuffle_seed=11)

        assert shuffled_a == shuffled_b
        assert shuffled_a != ordered
    finally:
        shutil.rmtree(tmp)


def test_training_exports_configured_threshold_to_header_and_metrics() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_train_threshold_"))
    try:
        root = tmp / "raw_gray96"
        out = tmp / "model"
        for i, value in enumerate([8, 16, 24]):
            write_sample(root / "empty" / f"empty_{i}.pgm", value)
        for i, value in enumerate([40, 48, 56]):
            write_sample(root / "safe" / f"safe_{i}.pgm", value)
        for i, value in enumerate([180, 196, 220]):
            write_sample(root / "intrusion" / f"intrusion_{i}.pgm", value)

        train_gray96_baseline.train_and_export(
            dataset_root=root,
            out_dir=out,
            epochs=20,
            learning_rate=0.05,
            train_ratio=0.67,
            threshold=0.4,
        )

        header = (out / "edgecare_model_baseline.h").read_text(encoding="utf-8")
        metrics = (out / "baseline_metrics.json").read_text(encoding="utf-8")
        assert "EDGECARE_BASELINE_THRESHOLD_Q15 13107" in header
        assert "EDGECARE_BASELINE_LOGIT_THRESHOLD_Q15 -13286" in header
        assert '"threshold": 0.4' in metrics
    finally:
        shutil.rmtree(tmp)


def test_metrics_include_export_model_replay_confusion() -> None:
    tmp = Path(tempfile.mkdtemp(prefix="edgecare_train_export_replay_"))
    try:
        root = tmp / "raw_gray96"
        out = tmp / "model"
        for i, value in enumerate([8, 16, 24, 32]):
            write_sample(root / "empty" / f"empty_{i}.pgm", value)
        for i, value in enumerate([40, 48, 56, 64]):
            write_sample(root / "safe" / f"safe_{i}.pgm", value)
        for i, value in enumerate([180, 196, 212, 228]):
            write_sample(root / "intrusion" / f"intrusion_{i}.pgm", value)

        train_gray96_baseline.train_and_export(
            dataset_root=root,
            out_dir=out,
            epochs=20,
            learning_rate=0.05,
            train_ratio=0.5,
            seed=3,
            threshold=0.4,
        )

        metrics = (out / "baseline_metrics.json").read_text(encoding="utf-8")
        assert '"export_accuracy"' in metrics
        assert '"export_confusion"' in metrics
    finally:
        shutil.rmtree(tmp)


if __name__ == "__main__":
    test_baseline_training_exports_metrics_and_header()
    test_export_model_is_retrained_on_all_samples()
    test_shuffled_training_is_deterministic_with_seed()
    test_training_exports_configured_threshold_to_header_and_metrics()
    test_metrics_include_export_model_replay_confusion()
