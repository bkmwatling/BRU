import sys
import argparse
import statistics
from pathlib import Path

import jsonlines  # type: ignore
import pandas as pd  # type: ignore


def post_process(data: dict[str, float]) -> dict[str, float]:
    N = sum(data.values())
    ratio_dict = {f"{key}_ratio": value / N for key, value in data.items()}
    data.update(ratio_dict)
    return data


def print_results(args: argparse.Namespace) -> None:
    dataset = jsonlines.open(args.input, mode="r")
    regex_statistics = []
    for data in dataset:
        if data["statistics"] is None:
            print(data["pattern"], file=sys.stderr)
            continue
        if data["statistics"] is not None:
            regex_statistics.append(data["statistics"])
    print(len(regex_statistics))

    post_processed = map(post_process, regex_statistics)
    df = pd.DataFrame(post_processed).fillna(0).T

    d = df.to_dict('tight')
    keys = [
        'char', 'pred', 'begin', 'end', 'match', 'save', 'memo', 'jmp',
        'split', 'tswitch'
    ]
    averages = {k: statistics.mean(v) for k, v in zip(d['index'], d['data'])}
    print(" & ".join(keys), end=" \\\\ \n")
    print(
        " & ".join(
            "{}({})".format(
                round(averages.get(key, 0), 2),
                round(averages.get(f"{key}_ratio", 0) * 100, 2)
            )
            for key in keys
        ),
        end=" \\\\ \n"
    )


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="Input file")
    print_results(parser.parse_args())
