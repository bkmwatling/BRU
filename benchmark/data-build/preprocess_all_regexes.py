import argparse
import random
import logging
import re
import string
from pathlib import Path
from typing import (Any, Optional, )

import timeout_decorator  # type: ignore
import jsonlines  # type: ignore
import xeger  # type: ignore
import pathos.multiprocessing as mp  # type: ignore


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


def get_positive_input(
    pattern: str,
    xeger_instance: xeger.Xeger,
) -> Optional[str]:

    def is_printable(letters: str) -> bool:
        return all(c in string.printable for c in letters)

    @timeout_decorator.timeout(1, timeout_exception=TimeOutException)
    def _get_positive_input(
        pattern: str,
        xeger_instance: xeger.Xeger,
    ) -> str:
        while True:
            positive_input_candidate = xeger_instance.xeger(pattern)
            if not re.fullmatch(pattern, positive_input_candidate):
                continue
            if not is_printable(positive_input_candidate):
                continue
            return positive_input_candidate

    try:
        positive_input = _get_positive_input(pattern, xeger_instance)
        return positive_input
    except TimeOutException:
        logging.debug("Timeout for negative input")
    except Exception as e:
        logging.debug(e)
    return None


def get_negative_input(
    pattern: str,
    xeger_instance: xeger.Xeger,
) -> Optional[str]:

    @timeout_decorator.timeout(1, timeout_exception=TimeOutException)
    def _get_negative_input(
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

    try:
        negative_input = _get_negative_input(pattern, xeger_instance)
        return negative_input
    except TimeOutException:
        logging.debug("Timeout for negative input")
    except Exception as e:
        logging.debug(e)
    return None


def preprocess_data(
    data: dict[str, Any],
    num_of_positive_inputs: int,
    num_of_negative_inputs: int
) -> Optional[dict[str, Any]]:
    pattern = data["pattern"]
    xeger_instance = get_xeger_instance()
    positive_inputs = [
        get_positive_input(pattern, xeger_instance)
        for _ in range(num_of_positive_inputs)
    ]
    negative_inputs = [
        get_negative_input(pattern, xeger_instance)
        for _ in range(num_of_negative_inputs)
    ]
    if not all(string is not None for string in positive_inputs):
        return None
    if not all(string is not None for string in negative_inputs):
        return None
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
    with mp.ProcessPool() as pool:
        optional_preprocessed_dataset = pool.imap(
            lambda e:
                preprocess_data(
                    e, num_of_negative_inputs, num_of_positive_inputs),
            regex_dataset
        )

    preprocessed_dataset = (
        data for data in optional_preprocessed_dataset if data is not None)
    output_file = jsonlines.open(output_path, "w")
    output_file.write_all(preprocessed_dataset)
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
