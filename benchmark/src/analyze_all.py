import argparse
from pathlib import Path

import jsonlines  # type: ignore

from utils import print_statistics


def print_key_statistics(input_path: Path, key: str, prefix: str) -> None:
    dataset = jsonlines.open(input_path, mode='r')
    avg_benchmark_results_label = f"avg_{prefix}_benchmark_results"
    avg_benchmark_results = [
        data[avg_benchmark_results_label] for data in dataset
        if data[avg_benchmark_results_label] is not None
    ]
    xs = [data[key] for data in avg_benchmark_results]

    # Avg. of Avg.
    print(prefix)
    print_statistics(xs)


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("key", type=str)
    args = parser.parse_args()
    print_key_statistics(args.input, args.key, "positive")
    print_key_statistics(args.input, args.key, "negative")
