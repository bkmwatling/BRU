import argparse
import json
import logging
import subprocess
from typing import (Iterable, Any, )

from pathlib import Path


def preprocess(input_path: Path, output_path: Path, pattern_key: str) -> None:
    def data_loader(path: Path) -> Iterable[dict[str, Any]]:
        with open(path, "r") as file:
            for line in file:
                yield json.loads(line)

    regex_dataset = data_loader(input_path)
    output_file = open(output_path, "w")

    for data in regex_dataset:
        pattern = data[pattern_key]
        try:
            subprocess.run(
                ['bru', 'compile', '-e', pattern], check=True, timeout=10,
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
            )
            output_file.write(json.dumps(data) + "\n")
        except subprocess.CalledProcessError as e:
            logging.debug(f"Failed to parse {pattern}")
            logging.debug(e.stderr)
        except subprocess.TimeoutExpired as e:
            logging.debug(f"Timeout for {pattern}")
            logging.debug(e)
        except (TypeError, ValueError, OSError, ) as e:
            logging.debug(f"Data error for {pattern}")
            logging.debug(e)


def preprocess_regex_data(input_path: Path, output_path: Path) -> None:
    preprocess(input_path, output_path, 'pattern')


def preprocess_sl_regex_data(input_path: Path, output_path: Path) -> None:
    preprocess(input_path, output_path, 'pattern')


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(description="Preprocess the data")
    parser.add_argument("dataset", type=str, choices=['regex', 'sl-regex'])
    parser.add_argument("input", type=str, help="Input file")
    parser.add_argument("output", type=str, help="Output file")
    args = parser.parse_args()
    if args.dataset == 'regex':
        preprocess_regex_data(Path(args.input), Path(args.output))
    elif args.dataset == 'sl-regex':
        preprocess_sl_regex_data(Path(args.input), Path(args.output))
    else:
        raise ValueError(f"Invalid dataset type {args.dataset}")
