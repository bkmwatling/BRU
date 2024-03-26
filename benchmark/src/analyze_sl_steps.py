import logging
import argparse
from collections import Counter
from collections import OrderedDict
from pathlib import Path

import jsonlines  # type: ignore

from utils import DegreeOfVulnerability as DoV
from utils import trim_dov
from utils import update_dov


def print_vulnerability(
    vulnerability_dict: dict[DoV, int],
    delimiter: str = " & ",
    end: str = " \\\\ \n"
) -> None:
    ordered = OrderedDict(
        sorted(vulnerability_dict.items(), key=lambda x: x[0]))
    print("{}".format(delimiter.join(map(str, ordered.keys()))), end=end)
    print("{}".format(delimiter.join(map(str, ordered.values()))), end=end)


def print_sl_step_statistics(input_path: Path) -> None:
    dataset = jsonlines.open(input_path, mode="r")
    preprocessed_dataset = map(update_dov, dataset)
    logLevel = logging.getLogger().getEffectiveLevel()
    trimmed_dovs = []

    for data in preprocessed_dataset:
        pattern = data["pattern"]
        inputs = data["evil_inputs"]
        steps = data["steps"]
        dov = trim_dov(data["dov"])
        trimmed_dovs.append(dov)

        if dov.is_unknown():
            logging.warning(pattern)
            logging.warning(steps)
            logging.warning(dov)
            logging.getLogger().setLevel(logging.DEBUG)
            DoV.from_steps(steps)
            logging.getLogger().setLevel(logLevel)
        if dov.is_vulnerable() and logLevel <= logging.INFO:
            logging.info(pattern)
            logging.info(steps)
            logging.info(dov)
            logging.info(inputs)
            logging.getLogger().setLevel(logging.DEBUG)
            DoV.from_steps(steps)
            logging.getLogger().setLevel(logLevel)

    vulnerability_dict = dict(Counter(trimmed_dovs))
    cols = (
        [DoV(DoV.Type.POLYNOMIAL, i) for i in range(6)]
        + [
            DoV(DoV.Type.EXPONENTIAL, 2),
            DoV(DoV.Type.FAILED),
            DoV(DoV.Type.UNKNOWN)
        ]
    )
    for dov in cols:
        vulnerability_dict.setdefault(dov, 0)
    print_vulnerability(vulnerability_dict)
    dataset.close()


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser()
    parser.add_argument("--info", action="store_true")
    parser.add_argument("input", type=Path, help="Input file")
    args = parser.parse_args()
    if args.info:
        logging.getLogger().setLevel(logging.INFO)
    print_sl_step_statistics(args.input)
