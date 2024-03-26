import argparse
import logging
from collections import Counter
from pathlib import Path
from typing import Any

import jsonlines  # type: ignore

from utils import DegreeOfVulnerability as DoV
from utils import trim_dov
from utils import update_dov


def print_vulnerability(
    dov_pairs: list[tuple[DoV, DoV]],
    delimiter: str = " & ",
    end: str = " \\\\ \n"
) -> None:
    cols = (
        [DoV(DoV.Type.POLYNOMIAL, i) for i in range(6)]
        + [
            DoV(DoV.Type.EXPONENTIAL, 2),
            DoV(DoV.Type.UNKNOWN),
            DoV(DoV.Type.FAILED)
        ]
    )
    counter = Counter(dov_pairs)
    print(f"{format(delimiter.join(map(str, cols)))}", end=end)
    for row in cols:
        for i, col in enumerate(cols):
            print(counter[(row, col)], end="")
            if i != len(cols) - 1:
                print(delimiter, end="")
        print(end=end)


def compare_data(
    data1: dict[str, Any],
    data2: dict[str, Any],
) -> tuple[DoV, DoV]:
    dov1 = data1["dov"]
    dov2 = data2["dov"]
    if dov1 > dov2:
        logging.info
    return trim_dov(dov1), trim_dov(dov2)


def compare_datasets(file1: Path, file2: Path) -> None:
    dataset1 = jsonlines.open(file1, mode='r')
    dataset2 = jsonlines.open(file2, mode='r')
    preprocessed_dataset1 = map(update_dov, dataset1)
    preprocessed_dataset2 = map(update_dov, dataset2)

    dov_pairs = []
    for data1, data2 in zip(preprocessed_dataset1, preprocessed_dataset2):
        dov_pairs.append(compare_data(data1, data2))
    print_vulnerability(dov_pairs)

    dataset1.close()
    dataset2.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("file1", type=Path)
    parser.add_argument("file2", type=Path)
    parser.add_argument("--info", action="store_true")
    args = parser.parse_args()
    if args.info:
        logging.getLogger().setLevel(logging.INFO)
    compare_datasets(args.file1, args.file2)
