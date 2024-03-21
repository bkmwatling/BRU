import argparse
import logging
import random
from pathlib import Path
from typing import (Any, )

import jsonlines  # type: ignore


def preprocess_data(
    data: dict[str, Any],
    number_of_evil_inputs: int
) -> dict[str, Any]:
    pattern = data["pattern"]
    evil_input = random.choice(data["evilInputs"])
    pump_pair = random.choice(evil_input["pumpPairs"])

    prefix = pump_pair["prefix"]
    pump = pump_pair["pump"]
    suffix = evil_input["suffix"]

    evil_inputs = [
        prefix + pump * i + suffix
        for i in range(number_of_evil_inputs)
    ]
    return {"pattern": pattern, "evil_inputs": evil_inputs}


def write_preprocessed_dataset(
    input_path: Path, output_path: Path,
    number_of_evil_inputs: int
) -> None:
    regex_dataset = jsonlines.open(input_path, mode='r')
    preprocessed_dataset = (
        preprocess_data(data, number_of_evil_inputs)
        for data in regex_dataset
    )
    output_file = jsonlines.open(output_path, "w")
    output_file.write_all(preprocessed_dataset)
    output_file.close()
    regex_dataset.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(description="Append inputs to regexes")
    parser.add_argument("input", type=Path, help="Input file")
    parser.add_argument("output", type=Path, help="Output file")
    parser.add_argument("--num_of_evil_inputs", "-e", type=int, default=50)
    args = parser.parse_args()
    write_preprocessed_dataset(
        args.input, args.output, args.num_of_evil_inputs)
