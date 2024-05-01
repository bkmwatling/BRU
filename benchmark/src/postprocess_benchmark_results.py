import argparse
import statistics
from pathlib import Path
from typing import (Any, Optional, )

import jsonlines  # type: ignore

from utils import RegexType
from utils import BenchmarkResult


def get_benchmark_result(
    output: Optional[dict[str, str]]
) -> Optional[dict[str, int]]:
    if output is None:
        return None
    if len(output["stderr"]) == 0:
        return None
    benchmark_result = BenchmarkResult(output["stderr"])
    return benchmark_result.to_dict()


def get_mask(
    optional_lists: list[list[Optional[Any]]]
) -> list[bool]:
    mask = []
    for optionals in zip(*optional_lists):
        mask.append(all(optional is not None for optional in optionals))
    return mask


def postprocess_filename_to_outputs(
    filename_to_outputs: dict[str, list[Optional[dict[str, str]]]],
) -> dict[str, list[Optional[dict[str, int]]]]:

    filename_to_benchmark_results: dict[str, list[Optional[dict[str, int]]]]
    filename_to_benchmark_results = {
        filename: [get_benchmark_result(output) for output in outputs]
        for filename, outputs in filename_to_outputs.items()
    }
    mask = get_mask(list(filename_to_benchmark_results.values()))

    filename_to_filtered_benchmark_results = {
        filename: [
            benchmark_result if mask_value else None
            for benchmark_result, mask_value
            in zip(benchmark_results, mask)
        ]
        for filename, benchmark_results
        in filename_to_benchmark_results.items()
    }
    return filename_to_filtered_benchmark_results


def postprocess_filename_to_data(
    filename_to_data: dict[str, dict[str, Any]],
    input_type: str
) -> None:
    benchmark_results_label = f"{input_type}_benchmark_results"
    avg_benchmark_results_label = f"avg_{input_type}_benchmark_results"
    input_strings_label = f"{input_type}_inputs"

    filename_to_outputs = {
        filename: data.pop(f"{input_type}_outputs")
        for filename, data in filename_to_data.items()
    }

    filename_to_benchmark_results = (
        postprocess_filename_to_outputs(filename_to_outputs))

    first_filename = list(filename_to_outputs.keys())[0]
    input_strings = filename_to_data[first_filename][input_strings_label]
    input_lengths = [len(input_string) for input_string in input_strings]

    for filename, benchmark_results in filename_to_benchmark_results.items():
        data = filename_to_data[filename]

        data["input_lengths"] = input_lengths
        data[benchmark_results_label] = None
        data[avg_benchmark_results_label] = None

        filtered_benchmark_results = [
            benchmark_result for benchmark_result in benchmark_results
            if benchmark_result is not None
        ]
        if len(filtered_benchmark_results) > 0:
            data[benchmark_results_label] = benchmark_results
            keys = filtered_benchmark_results[0].keys()
            data[avg_benchmark_results_label] = {
                key: statistics.mean(
                    result[key] for result in filtered_benchmark_results)
                for key in keys
            }


def postprocess(
    input_paths: list[Path],
    output_dir: Path,
    regex_type: RegexType
) -> None:
    if regex_type == RegexType.SUPER_LINEAR:
        raise NotImplementedError(
            "This script does not support SUPER_LINEAR regex type")

    filename_to_dataset = {
        input_path.name: jsonlines.open(input_path)
        for input_path in input_paths
    }

    filenames = list(filename_to_dataset.keys())
    iterator_of_tuple_of_data = zip(*map(iter, filename_to_dataset.values()))
    filename_to_writer = {
        filename: jsonlines.open(output_dir / filename, mode='w')
        for filename in filenames
    }

    for tuple_of_data in iterator_of_tuple_of_data:
        filename_to_data = {
            filename: data for filename, data in zip(filenames, tuple_of_data)}
        postprocess_filename_to_data(filename_to_data, "positive")
        postprocess_filename_to_data(filename_to_data, "negative")
        for filename in filenames:
            data = filename_to_data[filename]
            writer = filename_to_writer[filename]
            writer.write(data)

    for dataset in filename_to_dataset.values():
        dataset.close()
    for writer in filename_to_writer.values():
        writer.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--regex-type", type=RegexType, choices=list(RegexType))
    parser.add_argument("inputs", nargs="*", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    postprocess(args.inputs, args.output_dir, args.regex_type)
