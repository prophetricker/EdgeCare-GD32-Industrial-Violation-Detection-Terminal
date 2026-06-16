import argparse
import re
from dataclasses import dataclass
from pathlib import Path


BYTE_SCALE_RE = re.compile(
    r"camera_byte_scale\[(?P<tag>[^\]]+)\]: "
    r"order=0x(?P<order>[0-9A-Fa-f]{2}) "
    r"hint=(?P<hint>[a-z0-9_]+) "
    r"p0_raw=(?P<p0_min>\d+),(?P<p0_max>\d+),(?P<p0_mean>\d+) "
    r"p0_lshift2=(?P<p0_lmin>\d+),(?P<p0_lmax>\d+),(?P<p0_lmean>\d+) "
    r"p1_raw=(?P<p1_min>\d+),(?P<p1_max>\d+),(?P<p1_mean>\d+) "
    r"p1_lshift2=(?P<p1_lmin>\d+),(?P<p1_lmax>\d+),(?P<p1_lmean>\d+) "
    r"p2_raw=(?P<p2_min>\d+),(?P<p2_max>\d+),(?P<p2_mean>\d+) "
    r"p2_lshift2=(?P<p2_lmin>\d+),(?P<p2_lmax>\d+),(?P<p2_lmean>\d+) "
    r"p3_raw=(?P<p3_min>\d+),(?P<p3_max>\d+),(?P<p3_mean>\d+) "
    r"p3_lshift2=(?P<p3_lmin>\d+),(?P<p3_lmax>\d+),(?P<p3_lmean>\d+)"
)

BYTE_SCALE_TIMEOUT_RE = re.compile(
    r"camera_byte_scale\[(?P<tag>[^\]]+)\]: "
    r"order=0x(?P<order>[0-9A-Fa-f]{2}) "
    r"skipped=capture_timeout"
)


EXPECTED_ORDERS = tuple(range(8))


@dataclass(frozen=True)
class ByteScaleRecord:
    tag: str
    order: int
    hint: str
    p0_mean: int
    p1_mean: int
    p2_mean: int
    p3_mean: int
    p0_max: int
    p1_max: int
    p2_max: int
    p3_max: int

    @property
    def y_mean(self) -> int:
        return (self.p0_mean + self.p2_mean) // 2

    @property
    def chroma_mean(self) -> int:
        return (self.p1_mean + self.p3_mean) // 2

    @property
    def y_max(self) -> int:
        return max(self.p0_max, self.p2_max)

    @property
    def chroma_max(self) -> int:
        return max(self.p1_max, self.p3_max)


def parse_record(line: str) -> ByteScaleRecord | None:
    match = BYTE_SCALE_RE.search(line)
    if not match:
        return None
    groups = match.groupdict()
    return ByteScaleRecord(
        tag=groups["tag"],
        order=int(groups["order"], 16),
        hint=groups["hint"],
        p0_mean=int(groups["p0_mean"]),
        p1_mean=int(groups["p1_mean"]),
        p2_mean=int(groups["p2_mean"]),
        p3_mean=int(groups["p3_mean"]),
        p0_max=int(groups["p0_max"]),
        p1_max=int(groups["p1_max"]),
        p2_max=int(groups["p2_max"]),
        p3_max=int(groups["p3_max"]),
    )


def parse_timeout_order(line: str) -> int | None:
    match = BYTE_SCALE_TIMEOUT_RE.search(line)
    if not match:
        return None
    return int(match.group("order"), 16)


def parse_log(text: str) -> list[ByteScaleRecord]:
    records: dict[int, ByteScaleRecord] = {}
    for line in text.splitlines():
        record = parse_record(line)
        if record is not None:
            records[record.order] = record
    return [records[order] for order in sorted(records)]


def parse_timeout_orders(text: str) -> list[int]:
    orders: set[int] = set()
    for line in text.splitlines():
        order = parse_timeout_order(line)
        if order is not None:
            orders.add(order)
    return sorted(orders)


def format_orders(orders: list[int] | tuple[int, ...]) -> str:
    if not orders:
        return "none"
    return ",".join(f"0x{order:02X}" for order in orders)


def classify_sweep(records: list[ByteScaleRecord], timeout_orders: list[int] | tuple[int, ...] = ()) -> tuple[str, list[int]]:
    present = {record.order for record in records}
    attempted = present | set(timeout_orders)
    missing = [order for order in EXPECTED_ORDERS if order not in attempted]
    if missing or timeout_orders:
        return ("incomplete_sweep_log", [])

    candidates = [
        record.order
        for record in records
        if record.hint != "right_shift2_like"
    ]
    if candidates:
        return ("4745_mapping_candidate_found", candidates)

    return ("all_orders_still_right_shift2_like", [])


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarize firmware camera_byte_scale[...] serial lines from the JPEG-to-YUV 0x4745 sweep."
    )
    parser.add_argument("log", type=Path, help="Serial log captured from reset/run.")
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="ignore")
    records = parse_log(text)
    present = {record.order for record in records}
    timeout_orders = parse_timeout_orders(text)
    attempted = present | set(timeout_orders)
    missing = [order for order in EXPECTED_ORDERS if order not in attempted]
    conclusion, candidates = classify_sweep(records, timeout_orders)

    print(
        f"records={len(records)} expected_orders=8 missing_orders={format_orders(missing)} timeout_orders={format_orders(timeout_orders)}"
    )
    for record in records:
        print(
            f"order=0x{record.order:02X} tag={record.tag} hint={record.hint} "
            f"y_mean={record.y_mean} chroma_mean={record.chroma_mean} "
            f"y_max={record.y_max} chroma_max={record.chroma_max}"
        )
    print(f"candidate_orders={format_orders(candidates)}")
    print(f"conclusion={conclusion}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
