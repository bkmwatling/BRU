import argparse
import random
import logging
import re
import string
from pathlib import Path
from typing import (Any, Optional, Callable, )

import timeout_decorator  # type: ignore
import jsonlines  # type: ignore
import xeger  # type: ignore


random.seed(42)


class TimeOutException(Exception):
    pass


def get_xeger_instance() -> xeger.Xeger:
    def filter_printable(letters: str):
        return "".join(c for c in letters if c in string.printable)

    xeger_instance = xeger.Xeger()
    xeger_instance._alphabets = {
        key: filter_printable(value)
        for key, value in xeger_instance._alphabets.items()
    }
    return xeger_instance


@timeout_decorator.timeout(1, timeout_exception=TimeOutException)
def get_positive_input(
    pattern: str,
    xeger_instance: xeger.Xeger,
) -> str:
    while True:
        positive_input_candidate = xeger_instance.xeger(pattern)
        if not re.fullmatch(pattern, positive_input_candidate):
            continue
        return positive_input_candidate


@timeout_decorator.timeout(1, timeout_exception=TimeOutException)
def get_negative_input(
    pattern: str,
    xeger_instance: xeger.Xeger,
) -> str:
    def add_noise(s: str) -> str:
        noise = random.choice(list(string.printable) + [""])
        index = random.randint(0, len(s))
        return s[:index] + noise + s[index+1:]

    positive_input_candidate = xeger_instance.xeger(pattern)
    if not re.fullmatch(pattern, positive_input_candidate):
        negative_input = positive_input_candidate
        return negative_input

    while True:
        negative_input = add_noise(positive_input_candidate)
        if not re.fullmatch(pattern, negative_input):
            return negative_input


def preprocess_data(
    data: dict[str, Any],
    num_of_positive_inputs: int,
    num_of_negative_inputs: int
) -> dict[str, Any]:
    pattern = data["pattern"]
    xeger_instance = get_xeger_instance()

    def generate_inputs(generator: Callable[[], str], num_of_inputs: int):
        inputs = []
        for _ in range(num_of_positive_inputs):
            try:
                inputs.append(generator())
            except Exception:
                continue
        return inputs

    positive_inputs = generate_inputs(
        lambda: get_positive_input(pattern, xeger_instance),
        num_of_positive_inputs
    )
    negative_inputs = generate_inputs(
        lambda: get_negative_input(pattern, xeger_instance),
        num_of_negative_inputs
    )
    if len(positive_inputs) == 0 or len(negative_inputs) == 0:
        raise ValueError("Failed to generate any input for the pattern.")
    return {
        "pattern": pattern,
        "positive_inputs": positive_inputs,
        "negative_inputs": negative_inputs
    }


def write_preprocessed_dataset(
    input_path: Path, output_path: Path,
    num_of_positive_inputs: int, num_of_negative_inputs: int
) -> None:
    regex_dataset = jsonlines.open(input_path, mode='r')

    def safe_preprocess_data(data: dict[str, Any]) -> Optional[dict[str, Any]]:
        try:
            return preprocess_data(
                data, num_of_positive_inputs, num_of_negative_inputs)
        except Exception as e:
            logging.debug(f"Failed to preprocess {data['pattern']}: {e}")
            return None

    optional_preprocessed_dataset = map(safe_preprocess_data, regex_dataset)
    preprocessed_dataset = (
        data for data in optional_preprocessed_dataset if data is not None)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_file = jsonlines.open(output_path, "w")
    for data in preprocessed_dataset:
        try:
            output_file.write(data)
        except UnicodeEncodeError:
            continue
    output_file.close()
    regex_dataset.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(description="Append inputs to regexes")
    parser.add_argument("input", type=Path, help="Input file")
    parser.add_argument("output", type=Path, help="Output file")
    parser.add_argument("--num_of_positive_inputs", "-p", type=int, default=10)
    parser.add_argument("--num_of_negative_inputs", "-n", type=int, default=10)
    args = parser.parse_args()
    write_preprocessed_dataset(
        args.input, args.output,
        args.num_of_positive_inputs, args.num_of_negative_inputs
    )
