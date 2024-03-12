import json
import subprocess
from typing import (TypedDict, Iterable, Optional, )
from pathlib import Path
from enum import Enum


RegexData = TypedDict("RegexData", {
    "pattern": str,
    "supportedLangs": list[str],
    "type": str,
    "useCount_IStype_to_nPosts": dict[str, int],
}, total=False)

PumpPair = TypedDict("PumpPair", {"prefix": str, "pump": str})

EvilInput = TypedDict("EvilInput", {"pumpPairs": list[PumpPair], "type": str})

SlRegexData = TypedDict(
    "SlRegexData", {"pattern": str, "evilInputs": list[EvilInput]})


def open_regex_dataset(path: Path) -> Iterable[RegexData]:
    with open(path, "r") as file:
        for line in file:
            yield json.loads(line)


def open_sl_regex_dataset(path: Path) -> Iterable[SlRegexData]:
    with open(path, "r") as file:
        for line in file:
            yield json.loads(line)


def run_bru_match(args: list[str], timeout=10) -> None:
    subprocess.run(
        ['bru', 'match'] + args, check=True, timeout=timeout,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )


class ConstructionOption(Enum):
    THOMPOSON = "thompson"
    GLUSHKOV = "glushkov"


class MemoSchemeOption(Enum):
    CLOSURE_NODE = "cn"  # have a back-edge coming in that forms a loop
    IN_DEGREE = "in"  # state that have an in-degree > 1
    iar = "iar"  # TODO: What is it?


class SchedulerOption(Enum):
    SPENCER = "cn"
    LOCKSTEP = "thompson"


BruArgs = TypedDict("BruArgs", {
    "memo": MemoSchemeOption,
    "construction": ConstructionOption,
    "scheduler": SchedulerOption
}, total=False)


def run_benchmark(
    pattern: str,
    string: str,
    bru_args: BruArgs,
    timeout: int = 10
) -> None:
    args = [
        'bru', 'match',
        '-m', bru_args["memo"].value,
        '-m', bru_args["construction"].value,
        '-m', bru_args["scheduler"].value,
        pattern, string
    ]
    subprocess.run(
        args, check=True, timeout=10,
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
    )


class DegreeOfVulnerability():
    class Type(Enum):
        CONSTANT = "constant"
        LINEAR = "linear"
        POLYNOMIAL = "polynomial"
        EXPONENTIAL = "exponential"
        UNKNOWN = "unknown"

    def __init__(self, _type: Type, k: Optional[int] = None):

        if _type == DegreeOfVulnerability.Type.POLYNOMIAL:
            if k == 0:
                self.type = DegreeOfVulnerability.Type.CONSTANT
                self.degree = None
                return
            if k == 1:
                self.type = DegreeOfVulnerability.Type.LINEAR
                self.degree = None
                return

        self.type = _type
        # If type is polynomial, the degree denotes the order.
        # If type is exponential, the degree denotes the base.
        self.degree = k


def get_expontential_vulnerability(steps: list[int]):
    if len(steps) < 2:
        return DegreeOfVulnerability(DegreeOfVulnerability.Type.UNKNOWN)

    # Assuming step[n] = a*(k^n)+b, and return k.
    deltas = [steps[i] - steps[i+1] for i in range(len(steps)-1)]
    assert len(deltas) > 1
    degree = deltas[-1] // deltas[-2]
    return DegreeOfVulnerability(
        DegreeOfVulnerability.Type.EXPONENTIAL, degree)


def get_vulnerablility(steps: list[int]):
    if len(steps) < 1:
        return DegreeOfVulnerability(DegreeOfVulnerability.Type.UNKNOWN)

    deltas = [steps[i] - steps[i+1] for i in range(len(steps)-1)]
    degree = 0

    while deltas[-1] != 0:
        degree += 1
        if len(deltas) == 1:
            get_expontential_vulnerability(steps)
        deltas = [deltas[i] - deltas[i+1] for i in range(len(deltas)-1)]

    # Assuming step[n] = a*(n^k)+b, and return k.
    return DegreeOfVulnerability(DegreeOfVulnerability.Type.POLYNOMIAL, degree)
