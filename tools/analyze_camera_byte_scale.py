import argparse
from pathlib import Path


def lshift2(value: int) -> int:
    return 255 if value > 63 else value << 2


def stats(values: bytes) -> tuple[int, int, int]:
    if not values:
        return (0, 0, 0)
    return (min(values), max(values), sum(values) // len(values))


def phase_values(data: bytes, phase: int) -> bytes:
    return data[phase::4]


def classify_byte_scale(data: bytes) -> str:
    phase0 = phase_values(data, 0)
    phase1 = phase_values(data, 1)
    phase2 = phase_values(data, 2)
    phase3 = phase_values(data, 3)
    _, y0_max, y0_mean = stats(phase0)
    _, y2_max, y2_mean = stats(phase2)
    u_min, u_max, u_mean = stats(phase1)
    v_min, v_max, v_mean = stats(phase3)

    y_low_range = y0_max <= 64 and y2_max <= 64 and max(y0_mean, y2_mean) >= 8
    chroma_near_32 = 24 <= u_mean <= 40 and 24 <= v_mean <= 40 and u_max <= 64 and v_max <= 64
    chroma_stable = (u_max - u_min) <= 16 and (v_max - v_min) <= 16
    if y_low_range and chroma_near_32 and chroma_stable:
        return "right_shift2_like"
    return "not_right_shift2_like"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarize OV5640 QVGA YUV-like byte phases and detect right-shift-by-2 byte-scale symptoms."
    )
    parser.add_argument("frame", type=Path, help="Raw captured frame buffer.")
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=240)
    args = parser.parse_args()

    data = args.frame.read_bytes()
    expected = args.width * args.height * 2
    print(f"frame={args.frame} bytes={len(data)} expected={expected}")
    if len(data) != expected:
        print("size_warning=unexpected_frame_size")

    print(f"byte_scale_hint={classify_byte_scale(data)}")
    for phase in range(4):
        raw = phase_values(data, phase)
        shifted = bytes(lshift2(value) for value in raw)
        raw_min, raw_max, raw_mean = stats(raw)
        shifted_min, shifted_max, shifted_mean = stats(shifted)
        print(
            f"phase{phase} raw_min={raw_min} raw_max={raw_max} raw_mean={raw_mean} "
            f"lshift2_min={shifted_min} lshift2_max={shifted_max} lshift2_mean={shifted_mean}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
