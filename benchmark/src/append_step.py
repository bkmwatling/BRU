import logging
import argparse
import statistics
from pathlib import Path
from typing import (Any, Optional, )

import jsonlines  # type: ignore

from utils import get_step_from_output
from utils import RegexType


def append_steps_to_data_list(
    data_list: list[dict[str, Any]],
    filenames: list[str],
    inputs_label: str,
    outputs_label: str,
    prefix: str,
) -> list[dict[str, Any]]:
    pattern = data_list[0]["pattern"]
    strings = data_list[0][inputs_label]
    outputs_per_dataset = [
        data.pop(outputs_label) for data in data_list]

    steps_per_dataset: list[list[Optional[int]]] = [
        [] for _ in data_list]

    for string, output_per_dataset in (
        zip(strings, zip(*outputs_per_dataset))
    ):
        step_per_dataset = [
            get_step_from_output(output) for output in output_per_dataset]

        # Filter out the input if any of engine has no output
        if any(step is None for step in step_per_dataset):
            logging.warning(pattern)
            logging.warning(string)
            for filename, step in zip(filenames, step_per_dataset):
                if step is not None:
                    continue
                logging.warning(f"No output for {filename}")
            for steps in steps_per_dataset:
                steps.append(None)
            continue

        for steps, step in zip(steps_per_dataset, step_per_dataset):
            assert step is not None
            steps.append(step)

    for data, steps in zip(data_list, steps_per_dataset):
        data[f"{prefix}_steps"] = steps
        data[f"avg_{prefix}_step"] = None

        filtered_steps = [step for step in steps if step is not None]
        if len(filtered_steps) == 0:
            continue
        data[f"avg_{prefix}_step"] = statistics.mean(filtered_steps)
    return data_list


def process_all_data_list(
    data_list: list[dict[str, Any]],
    filenames: list[str]
) -> list[dict[str, Any]]:
    appended = append_steps_to_data_list(
        data_list, filenames,
        "positive_inputs", "positive_outputs", "positive")
    appended = append_steps_to_data_list(
        appended, filenames,
        "negative_inputs", "negative_outputs", "negative")
    return appended


def process_sl_data_list(
    data_list: list[dict[str, Any]],
    filenames: list[str]
) -> list[dict[str, Any]]:
    raise NotImplementedError()


def append_steps(
    input_paths: list[Path],
    output_dir: Path,
    regex_type: RegexType
) -> None:
    datasets = [
        jsonlines.open(input_path, mode='r') for input_path in input_paths]
    filenames = [input_path.name for input_path in input_paths]
    output_paths = [output_dir / input_path.name for input_path in input_paths]
    outputs = [
        jsonlines.open(output_path, mode='w') for output_path in output_paths]

    process_data_list = (
        process_all_data_list
        if regex_type == RegexType.ALL
        else process_sl_data_list
    )

    data_lists = map(list, zip(*datasets))
    appended_data_lists = map(
        lambda e: process_data_list(e, filenames), data_lists)
    for data_list in appended_data_lists:
        for output, data in zip(outputs, data_list):
            output.write(data)

    for dataset in datasets:
        dataset.close()
    for output in outputs:
        output.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser(
        description="Append steps to benchmark results")
    parser.add_argument(
        "--regex-type", type=RegexType, choices=list(RegexType))
    parser.add_argument("inputs", nargs="*", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    append_steps(args.inputs, args.output_dir, args.regex_type)
