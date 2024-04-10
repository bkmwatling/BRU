import logging
import json
import argparse
import statistics
from pathlib import Path
from typing import (Any, Optional, Iterator)

import jsonlines  # type: ignore

from utils import get_memo_size_from_output
from utils import RegexType


def append_memo_size_to_data_list(
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

    memo_sizes_per_dataset: list[list[Optional[int]]] = [
        [] for _ in data_list]

    for string, output_per_dataset in (
        zip(strings, zip(*outputs_per_dataset))
    ):
        memo_size_per_dataset = [
            get_memo_size_from_output(output) for output in output_per_dataset]

        # Filter out the input if any of engine has no output
        if any(memo_size is None for memo_size in memo_size_per_dataset):
            logging.warning(pattern)
            logging.warning(string)
            for filename, memo_size in zip(filenames, memo_size_per_dataset):
                if memo_size is not None:
                    continue
                logging.warning(f"No output for {filename}")
            for memo_sizes in memo_sizes_per_dataset:
                memo_sizes.append(None)
            continue

        for memo_sizes, memo_size in zip(
            memo_sizes_per_dataset, memo_size_per_dataset
        ):
            assert memo_size is not None
            memo_sizes.append(memo_size)

    for data, memo_sizes in zip(data_list, memo_sizes_per_dataset):
        data[f"{prefix}_memo_sizes"] = memo_size
        data[f"avg_{prefix}_memo_size"] = None

        filtered_memo_size = [
            memo_size for memo_size in memo_sizes
            if memo_size is not None
        ]
        if len(filtered_memo_size) == 0:
            continue
        data[f"avg_{prefix}_memo_size"] = statistics.mean(filtered_memo_size)
    return data_list


def proccess_all_data_list(
    data_list: list[dict[str, Any]],
    filenames: list[str]
) -> list[dict[str, Any]]:
    appended = append_memo_size_to_data_list(
        data_list, filenames,
        "positive_inputs", "positive_outputs", "positive")
    appended = append_memo_size_to_data_list(
        appended, filenames,
        "negative_inputs", "negative_outputs", "negative")
    return appended


def proccess_sl_data_list(
    data_list: list[dict[str, Any]],
    filenames: list[str]
) -> list[dict[str, Any]]:
    raise NotImplementedError


def read_jsonlines(input_path: Path) -> Iterator[dict[str, Any]]:
    with open(input_path, 'r', errors='ignore') as f:
        for i, line in enumerate(f):
            try:
                error_entry = {
                    "pattern": "",
                    "positive_inputs": [""], "positive_outputs": [None],
                    "negative_inputs": [""], "negative_outputs": [None],
                    "input_path": str(input_path),
                    "line_number": i
                }
                yield json.loads(line)
            except json.JSONDecodeError:
                yield error_entry


def append_memo_size(
    input_paths: list[Path],
    output_dir: Path,
    regex_type: RegexType
) -> None:
    datasets = [read_jsonlines(input_path) for input_path in input_paths]
    filenames = [input_path.name for input_path in input_paths]
    output_paths = [output_dir / input_path.name for input_path in input_paths]
    outputs = [
        jsonlines.open(output_path, mode='w') for output_path in output_paths]

    process_data_list = (
        proccess_all_data_list
        if regex_type == RegexType.ALL
        else proccess_sl_data_list
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
        description="Append memo size to benchmark results")
    parser.add_argument(
        "--regex-type", type=RegexType, choices=list(RegexType))
    parser.add_argument("inputs", nargs="*", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    append_memo_size(args.inputs, args.output_dir, args.regex_type)
