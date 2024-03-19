import logging
import argparse
import statistics
from collections import Counter
from collections import OrderedDict
from pathlib import Path
from typing import Optional, Any

import jsonlines  # type: ignore

from utils import BenchmarkResult
from utils import DegreeOfVulnerability as DoV
from utils import print_statistics


def get_step_from_outputs(outputs: Optional[dict[str, str]]) -> Optional[int]:
    if outputs is None:
        logging.debug("No outputs")
        return None
    if len(outputs["stderr"]) == 0:
        logging.debug("No stderr")
        return None
    result = BenchmarkResult(outputs["stderr"])
    return result.steps


def get_steps_from_all_data(
    data: dict[str, Any]
) -> tuple[list[int], list[int]]:
    positive_steps = [
        e for e in map(get_step_from_outputs, data["positive_outputs"])
        if e is not None
    ]
    negative_steps = [
        e for e in map(get_step_from_outputs, data["negative_outputs"])
        if e is not None
    ]
    return positive_steps, negative_steps


def get_steps_from_sl_data(data: dict[str, Any]) -> list[int]:
    steps = [
        e for e in map(get_step_from_outputs, data["outputs"])
        if e is not None
    ]
    return steps


def print_vulnerability(
    vulnerability_dict: dict[DoV, int],
    delimiter: str = " & ",
    end: str = " \\\\ \n"
) -> None:
    ordered = OrderedDict(
        sorted(vulnerability_dict.items(), key=lambda x: x[0]))
    print("{}".format(delimiter.join(map(str, ordered.keys()))), end=end)
    print("{}".format(delimiter.join(map(str, ordered.values()))), end=end)


def print_sl_step_statistics(input_path: Path) -> None:
    stepss = []
    patterns = []
    with jsonlines.open(input_path, mode='r') as dataset:
        for data in dataset:
            steps = get_steps_from_sl_data(data)
            stepss.append(steps)
            patterns.append(data["pattern"])

    def trim_dov(dov: DoV) -> DoV:
        if dov.is_polynomial():
            assert dov.degree is not None
            if dov.degree > 5:
                return DoV(DoV.Type.POLYNOMIAL, 5)
        if dov.is_exponential():
            return DoV(DoV.Type.EXPONENTIAL, 2)
        return dov

    dovs = [DoV.from_steps(steps) for steps in stepss]
    trimmed_dovs = [trim_dov(dov) for dov in dovs]
    vulnerablity_dict = dict(Counter(trimmed_dovs))
    print_vulnerability(vulnerablity_dict)


def print_all_step_statistics(input_path: Path) -> None:
    dataset = jsonlines.open(input_path, mode='r')
    steps_tuples = [get_steps_from_all_data(data) for data in dataset]
    positive_average_steps = [
        statistics.mean(e) for e, _ in steps_tuples if len(e) > 0]
    negative_average_steps = [
        statistics.mean(e) for _, e in steps_tuples if len(e) > 0]
    print("Positive")
    print_statistics(positive_average_steps)
    print("Negative")
    print_statistics(negative_average_steps)
    dataset.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser(
        description="Extract steps from benchmark results")
    parser.add_argument("--regex-type", type=str, choices=["all", "sl"])
    parser.add_argument("input", type=Path, help="Input file")
    args = parser.parse_args()
    if args.regex_type == "all":
        print_all_step_statistics(args.input)
    else:
        print_sl_step_statistics(args.input)
