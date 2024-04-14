import argparse
from typing import Any
from pathlib import Path

import jsonlines  # type: ignore


def compare_data(
    data1: dict[str, Any],
    data2: dict[str, Any],
    prefix: str,
    threshold: int = 0,
) -> bool:
    pattern = data1["pattern"]
    strings = data1[f"{prefix}_inputs"]

    label = f"{prefix}_benchmark_results"
    if data1[label] is None or data2[label] is None:
        return False

    benchmark_results_1 = [
        e for e in data1[f"{prefix}_benchmark_results"] if e is not None]
    benchmark_results_2 = [
        e for e in data2[f"{prefix}_benchmark_results"] if e is not None]
    steps1 = [result['step'] for result in benchmark_results_1]
    steps2 = [result['step'] for result in benchmark_results_2]

    flag = False
    for string, step1, step2 in zip(strings, steps1, steps2):
        if step1 is None and step2 is None:
            continue
        if step2 - step1 > threshold:
            print(pattern)
            print(string)
            print(step1)
            print(step2)
            print()
            flag = True
    return flag


def compare_datasets(
  file1: Path, file2: Path, prefix: str, threshold: int
) -> None:
    dataset1 = jsonlines.open(file1, mode='r')
    dataset2 = jsonlines.open(file2, mode='r')
    count = 0
    for data1, data2 in zip(dataset1, dataset2):
        if compare_data(data1, data2, prefix, threshold):
            count += 1
    print(f"The average number of steps increased for {count} regexes.")
    dataset1.close()
    dataset2.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("file1", type=Path)
    parser.add_argument("file2", type=Path)
    parser.add_argument(
        "--input-type", type=str, choices=["positive", "negative"])
    parser.add_argument("--threshold", type=int, default=0)
    args = parser.parse_args()
    compare_datasets(args.file1, args.file2, args.input_type, args.threshold)
