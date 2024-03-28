import argparse
from pathlib import Path

import jsonlines  # type: ignore

from utils import print_statistics


def print_step_statistics(input_path: Path, prefix: str) -> None:
    dataset = jsonlines.open(input_path, mode='r')
    steps = [data[f"avg_{prefix}_memo_size"] for data in dataset]
    steps = [step for step in steps if step is not None]

    # Avg. of Avg.
    print(prefix)
    print_statistics(steps)


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser(
        description="Extract steps from benchmark results")
    parser.add_argument("input", type=Path, help="Input file")
    args = parser.parse_args()
    print_step_statistics(args.input, "positive")
    print_step_statistics(args.input, "negative")
