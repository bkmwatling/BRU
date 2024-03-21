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


def get_step_from_output(output: Optional[dict[str, str]]) -> Optional[int]:
    if output is None:
        return None
    if len(output["stderr"]) == 0:
        return None
    result = BenchmarkResult(output["stderr"])
    return result.steps


def get_steps_from_all_data(
    data: dict[str, Any],
    inputs_label: str,
    outputs_label: str
) -> list[int]:
    pattern = data["pattern"]
    strings = data[inputs_label]
    outputs = data[outputs_label]
    steps = [get_step_from_output(output) for output in outputs]
    for string, output in zip(strings, outputs):
        if output is None and len(string) > 0:
            logging.info("No output")
            logging.info(pattern)
            logging.info(string)
            # Reject the pattern if one of the outputs is None
            return []
    return [step for step in steps if step is not None]


def get_steps_from_sl_data(data: dict[str, Any]) -> list[int]:
    steps = [
        e for e in map(get_step_from_output, data["outputs"])
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
    inputs = []
    with jsonlines.open(input_path, mode='r') as dataset:
        for data in dataset:
            steps = get_steps_from_sl_data(data)
            stepss.append(steps)
            patterns.append(data["pattern"])
            inputs.append(data["evil_inputs"][:4])

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
    logLevel = logging.getLogger().getEffectiveLevel()
    for pattern, inputs, steps, dov in (
        zip(patterns, inputs, stepss, trimmed_dovs)
    ):
        if dov.is_unknown():
            logging.warning(pattern)
            logging.warning(steps)
            logging.warning(dov)
            logging.getLogger().setLevel(logging.DEBUG)
            DoV.from_steps(steps)
            logging.getLogger().setLevel(logLevel)
        if dov.is_vulnerable() and logLevel <= logging.INFO:
            logging.info(pattern)
            logging.info(steps)
            logging.info(dov)
            logging.info(inputs)
            logging.getLogger().setLevel(logging.DEBUG)
            DoV.from_steps(steps)
            logging.getLogger().setLevel(logLevel)
    vulnerablity_dict = dict(Counter(trimmed_dovs))
    print_vulnerability(vulnerablity_dict)


def get_average_steps(stepss: list[list[int]]) -> tuple[list[float], int]:
    average_steps = []
    failed = 0
    for steps in stepss:
        if len(steps) == 0:
            failed += 1
            continue
        average_steps.append(statistics.mean(steps))
    return average_steps, failed


def print_all_step_statistics(input_path: Path) -> None:
    dataset = jsonlines.open(input_path, mode='r')
    positive_stepss = []
    negative_stepss = []
    for data in dataset:
        positive_stepss.append(get_steps_from_all_data(
                data, "positive_inputs", "positive_outputs"))
        negative_stepss.append(get_steps_from_all_data(
            data, "negative_inputs", "negative_outputs"))

    positive_steps, positive_failed = get_average_steps(positive_stepss)
    negative_steps, negative_failed = get_average_steps(negative_stepss)

    # Avg. of Avg.
    print("Positive")
    print(f"Failed: {positive_failed}")
    print_statistics(positive_steps)
    print("Negative")
    print(f"Failed: {negative_failed}")
    print_statistics(negative_steps)
    dataset.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser(
        description="Extract steps from benchmark results")
    parser.add_argument("--regex-type", type=str, choices=["all", "sl"])
    parser.add_argument("--info", action="store_true")
    parser.add_argument("input", type=Path, help="Input file")
    args = parser.parse_args()
    if args.info:
        logging.basicConfig(level=logging.INFO)
    if args.regex_type == "all":
        print_all_step_statistics(args.input)
    else:
        print_sl_step_statistics(args.input)
