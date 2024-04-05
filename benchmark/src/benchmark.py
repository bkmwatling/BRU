import argparse
import logging
import subprocess
from typing import (Any, Optional, Callable, )
from pathlib import Path

import jsonlines  # type: ignore

from utils import ConstructionOption
from utils import MemoSchemeOption
from utils import SchedulerOption
from utils import RegexType
from utils import MatchingType
from utils import BruArgs
from utils import benchmark


def get_safe_benchmark(
    pattern: str,
    bru_args: BruArgs
) -> Callable[[str], Optional[dict[str, str]]]:
    def safe_benchmark(string: str) -> Optional[dict[str, str]]:
        try:
            benchmark_result = benchmark(pattern, string, bru_args)
            return benchmark_result
        except subprocess.TimeoutExpired:
            pass
        except Exception as e:
            logging.error(f"Engine error for {pattern} and {string}")
            logging.error(e)
        return None
    return safe_benchmark


def benchmark_sl_data(
    data: dict[str, Any],
    bru_args: BruArgs,
    matching_type: MatchingType,
) -> dict[str, Any]:
    pattern = data["pattern"]
    if matching_type == MatchingType.FULL:
        pattern = f"^(?:{pattern})$"
    evil_inputs = data["evil_inputs"]

    safe_benchmark = get_safe_benchmark(pattern, bru_args)
    outputs: list[Optional[dict[str, str]]] = []
    timed_out = False
    for evil_input in evil_inputs:
        if timed_out:
            outputs.append(None)
            continue
        result = safe_benchmark(evil_input)
        if result is None:
            timed_out = True
        outputs.append(result)
    data["outputs"] = outputs
    return data


def benchmark_all_data(
    data: dict[str, Any],
    bru_args: BruArgs,
    matching_type: MatchingType,
) -> dict[str, Any]:
    pattern = data["pattern"]
    if matching_type == MatchingType.FULL:
        pattern = f"^(?:{pattern})$"
    positive_inputs = data["positive_inputs"]
    negative_inputs = data["negative_inputs"]

    safe_benchmark = get_safe_benchmark(pattern, bru_args)
    positive_outputs = list(map(safe_benchmark, positive_inputs))
    negative_outputs = list(map(safe_benchmark, negative_inputs))
    data["positive_outputs"] = positive_outputs
    data["negative_outputs"] = negative_outputs
    return data


def benchmark_dataset(
    input_path: Path,
    output_path: Path,
    bru_args: BruArgs,
    regex_type: RegexType,
    matching_type: MatchingType
) -> None:
    regex_dataset = jsonlines.open(input_path, mode='r')
    if regex_type == RegexType.ALL:
        benchmark_data = (
            lambda e: benchmark_all_data(e, bru_args, matching_type))
    elif regex_type == RegexType.SUPER_LINEAR:
        benchmark_data = (
            lambda e: benchmark_sl_data(e, bru_args, matching_type))

    benchmarked_dataset = map(benchmark_data, regex_dataset)
    output_file = jsonlines.open(output_path, "w")
    output_file.write_all(benchmarked_dataset)
    output_file.close()
    regex_dataset.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(
        description="Benchmark regexes with Bru.")
    parser.add_argument(
        "-t", "--regex-type", type=RegexType, choices=list(RegexType))
    parser.add_argument(
        "-c", "--construction",
        type=ConstructionOption, choices=list(ConstructionOption))
    parser.add_argument(
        "-s", "--scheduler",
        type=SchedulerOption, choices=list(SchedulerOption))
    parser.add_argument(
        "-m", "--memo-scheme",
        type=MemoSchemeOption, choices=list(MemoSchemeOption))
    parser.add_argument(
        "-f", "--matching_type",
        type=MatchingType, choices=list(MatchingType))

    parser.add_argument("input", type=Path, help="Input file")
    parser.add_argument("output", type=Path, help="Output file")
    args = parser.parse_args()

    bru_args = BruArgs({
        "memo-scheme": args.memo_scheme,
        "construction": args.construction,
        "scheduler": args.scheduler
    })
    benchmark_dataset(
        args.input, args.output, bru_args, args.regex_type, args.matching_type)
