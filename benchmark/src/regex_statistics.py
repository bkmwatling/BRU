import argparse
import logging
import subprocess
from typing import (TypedDict, Any, Optional, )
from pathlib import Path
from collections import Counter

import jsonlines  # type: ignore

from utils import ConstructionOption
from utils import MemoSchemeOption


BruArgs = TypedDict("BruArgs", {
    "memo-scheme": MemoSchemeOption,
    "construction": ConstructionOption,
}, total=False)


def run_bru(
    pattern: str,
    bru_args: BruArgs,
    timeout: int = 10
) -> str:
    construction = bru_args["construction"]
    memo_scheme = bru_args["memo-scheme"]

    args = [
        'bru', 'compile',
        '-e',  # expand counter
        '-c', construction.value,
        '-m', memo_scheme.value,
        '--',
        pattern
    ]
    completed = subprocess.run(
        args, check=True, timeout=10,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    stdout = completed.stdout.decode("utf-8")
    return stdout


def safe_run_bru(pattern: str, bru_args: BruArgs) -> Optional[str]:
    try:
        return run_bru(pattern, bru_args)
    except subprocess.TimeoutExpired:
        pass
    except Exception as e:
        logging.error(f"Engine error for {pattern}")
        logging.error(e)
    return None


def update_statistics(
    input_path: Path,
    output_path: Path,
    bru_args: BruArgs,
) -> None:

    def update_statistics_data(data: dict[str, Any]) -> dict[str, Any]:
        stdout = safe_run_bru(data["pattern"], bru_args)
        if stdout is None:
            data["statistics"] = None
            return data
        instructions = filter(None, map(str.strip, stdout.strip().split("\n")))
        statistics = dict(Counter(e.split()[1] for e in instructions))
        updated_data = {
            "pattern": data["pattern"],
            "statistics": statistics
        }
        return updated_data

    regex_dataset = jsonlines.open(input_path, mode='r')
    dataset_with_statistics = map(update_statistics_data, regex_dataset)
    output_file = jsonlines.open(output_path, "w")
    output_file.write_all(dataset_with_statistics)
    output_file.close()
    regex_dataset.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(
        description="Measure the matching step of the sl-regexes")
    parser.add_argument(
        "-c", "--construction",
        type=ConstructionOption, choices=list(ConstructionOption))
    parser.add_argument(
        "-m", "--memo-scheme",
        type=MemoSchemeOption, choices=list(MemoSchemeOption))

    parser.add_argument("input", type=Path, help="Input file")
    parser.add_argument("output", type=Path, help="Output file")
    args = parser.parse_args()

    bru_args = BruArgs({
        "memo-scheme": args.memo_scheme,
        "construction": args.construction,
    })
    update_statistics(args.input, args.output, bru_args)
