import logging
import re
import statistics
import subprocess
from itertools import product
from functools import total_ordering
from typing import (Optional, Any, TypedDict, Iterable)
from enum import Enum
from pathlib import Path

import jsonlines
import numpy as np

ELIMINATED_LABEL = "NUMBER OF TRANSITIONS ELIMINATED FROM FLATTENING:"
MEMOISED_LABEL = "NUMBER OF STATES MEMOISED:"
THREAD_LABEL = "TOTAL THREADS IN POOL"


class ConstructionOption(Enum):
    THOMPOSON = "thompson"
    GLUSHKOV = "flat"


class MemoSchemeOption(Enum):
    NONE = "none"
    CLOSURE_NODE = "cn"  # have a back-edge coming in that forms a loop
    IN_DEGREE = "in"  # state that have an in-degree > 1


class SchedulerOption(Enum):
    SPENCER = "spencer"
    LOCKSTEP = "lockstep"


class RegexType(Enum):
    ALL = "all"
    SUPER_LINEAR = "sl"  # super-linear


class MatchingType(Enum):
    FULL = "full"
    PARTIAL = "partial"


BruArgs = TypedDict("BruArgs", {
    "construction": ConstructionOption,
    "scheduler": SchedulerOption,
    "memo-scheme": MemoSchemeOption,
}, total=False)


def iterate_bru_args() -> Iterable[BruArgs]:
    for construction in ConstructionOption:
        for memo_scheme in MemoSchemeOption:
            yield BruArgs({
                "construction": construction,
                "scheduler": SchedulerOption.SPENCER,
                "memo-scheme": memo_scheme,
            })
        yield BruArgs({
            "construction": construction,
            "scheduler": SchedulerOption.LOCKSTEP,
            "memo-scheme": MemoSchemeOption.NONE,
        })


def benchmark(
    pattern: str,
    string: str,
    bru_args: BruArgs,
    timeout: int = 10
) -> dict[str, str]:
    construction = bru_args["construction"]
    memo_scheme = bru_args["memo-scheme"]
    scheduler = bru_args["scheduler"]

    if scheduler == SchedulerOption.LOCKSTEP:
        if memo_scheme != MemoSchemeOption.NONE:
            raise ValueError(
                "Lockstep scheduler can only be used with no memoization")

    args = [
        'bru', 'match',
        '-b',  # benchmark
        '-e',  # expand counter
        '-c', construction.value,
        '-m', memo_scheme.value,
        '-s', scheduler.value,
        '--mark-states',
        '--',
        pattern, string
    ]
    completed = subprocess.run(
        args, check=True, timeout=10,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    stdout = completed.stdout.decode("utf-8")
    stderr = completed.stderr.decode("utf-8")
    return {"stdout": stdout, "stderr": stderr}


class BenchmarkResult():
    def __init__(self, stderr: str):
        self.parsed: dict[str, tuple[int, int]] = {}
        pattern = re.compile(r"([A-Z]+): (\d+) \(FAILED: (\d+)\)")
        self.eliminated = 0
        self.memoised = 0
        self.thread = 0
        for line in stderr.strip().split("\n"):
            if line.startswith(MEMOISED_LABEL):
                self.eliminated = int(line.split(': ')[1])
                continue
            if line.startswith(ELIMINATED_LABEL):
                self.eliminated = int(line.split(': ')[1])
                continue
            match = pattern.match(line)
            assert match is not None
            total = int(match.group(2))
            failed = int(match.group(3))
            self.parsed[match.group(1)] = (total, failed)

    @property
    def step(self) -> int:
        return self.parsed["CHAR"][0] + self.parsed["PRED"][0]

    @property
    def memo_entry(self) -> int:
        return self.parsed["MEMO"][0] - self.parsed["MEMO"][1]

    @property
    def state(self) -> int:
        return self.parsed["STATE"][0]

    def to_dict(self):
        return {
            'step': self.step,
            'memo_entry': self.memo_entry,
            'state': self.state
        }


@total_ordering
class DegreeOfVulnerability():
    class Type(Enum):
        POLYNOMIAL = "polynomial"
        EXPONENTIAL = "exponential"
        UNKNOWN = "unknown"
        FAILED = "failed"

    def __init__(self, _type: Type, degree: Optional[int] = None) -> None:
        if (
            (_type in {self.Type.UNKNOWN, self.Type.FAILED})
            != (degree is None)
        ):
            raise ValueError("Invalid parameters")
        self.type = _type
        self.degree = degree

    def is_vulnerable(self) -> bool:
        if self.is_polynomial():
            if not (self.is_constant() or self.is_linear()):
                return True
        if self.is_exponential():
            return True
        return False

    def is_polynomial(self) -> bool:
        return self.type == DegreeOfVulnerability.Type.POLYNOMIAL

    def is_exponential(self) -> bool:
        return self.type == DegreeOfVulnerability.Type.EXPONENTIAL

    def is_unknown(self) -> bool:
        return self.type == DegreeOfVulnerability.Type.UNKNOWN

    def is_failed(self) -> bool:
        return self.type == DegreeOfVulnerability.Type.FAILED

    def is_linear(self) -> bool:
        if not self.is_polynomial():
            return False
        return self.degree == 1

    def is_constant(self) -> bool:
        if not self.is_polynomial():
            return False
        return self.degree == 0

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, DegreeOfVulnerability):
            return False
        return self.type == other.type and self.degree == other.degree

    def __le__(self, other: object) -> bool:
        if not isinstance(other, DegreeOfVulnerability):
            return NotImplemented

        if self.is_failed():
            return False
        if self.is_unknown():
            return other.is_failed()
        if self.is_exponential():
            if other.is_unknown() or other.is_failed():
                return True
            if other.is_exponential():
                assert self.degree is not None
                assert other.degree is not None
                return self.degree <= other.degree
            return False
        if other.is_polynomial():
            assert self.degree is not None
            assert other.degree is not None
            return self.degree <= other.degree
        return True

    def __str__(self):
        if self.is_unknown():
            return "UNK"
        if self.is_failed():
            return "FAILED"
        if self.is_constant():
            return "O(1)"
        if self.is_linear():
            return "O(n)"
        if self.is_polynomial():
            return f"O(n^{self.degree})"
        if self.is_exponential():
            return f"O({self.degree}^n)"
        assert False

    def __hash__(self):
        return hash(str(self))

    @classmethod
    def _exponential_vulnerability_from_steps(cls, steps: list[int]):
        if len(steps) < 3:
            return cls(cls.Type.FAILED)

        # Assuming step[i] = a*(k^i)+b, and return k.
        # deltas[i] = a*(k^(i+1))+b - a*(k^i)+b = a*(k^i)*(k-1)
        deltas = [steps[i] - steps[i+1] for i in range(len(steps)-1)]

        # deltas[i] // deltas[i-1] = k
        degree = round(deltas[-1] / deltas[-2])
        if degree < 2:
            return cls(cls.Type.UNKNOWN)
        return cls(cls.Type.EXPONENTIAL, degree)

    @classmethod
    def from_steps(cls, steps: list[int]):
        if len(steps) < 4:
            return cls._exponential_vulnerability_from_steps(steps)

        # Remove the first step since it may not the base
        steps = steps[1:]

        # Find with exact steps
        deltas = steps.copy()
        degree = 0
        while True:
            half = len(deltas) // 2
            logging.debug(deltas)

            last_half = deltas[half:]
            first_half = deltas[:half]

            # It might be constant
            if max(last_half) - max(first_half) < half:
                return cls(cls.Type.POLYNOMIAL, degree)

            if not all(delta >= 0 for delta in deltas):
                average_first_half = statistics.mean(first_half)
                average_last_half = min(
                    statistics.mean(last_half),
                    statistics.mean(last_half[:-1])
                )
                if average_last_half - average_first_half < half:
                    return cls(cls.Type.POLYNOMIAL, degree)
                return cls(cls.Type.UNKNOWN)

            if len(deltas) == 2:
                break

            deltas = [deltas[i+1] - deltas[i] for i in range(len(deltas)-1)]
            degree += 1

        return cls._exponential_vulnerability_from_steps(steps)


def get_step_from_output(output: Optional[dict[str, str]]) -> Optional[int]:
    if output is None:
        return None
    if len(output["stderr"]) == 0:
        return None
    result = BenchmarkResult(output["stderr"])
    return result.step


def get_memo_size_from_output(
    output: Optional[dict[str, str]]
) -> Optional[int]:
    if output is None:
        return None
    if len(output["stderr"]) == 0:
        return None
    result = BenchmarkResult(output["stderr"])
    return result.memo_entry


def trim_dov(dov: DegreeOfVulnerability) -> DegreeOfVulnerability:
    if dov.is_polynomial():
        assert dov.degree is not None
        if dov.degree > 5:
            return DegreeOfVulnerability(
                DegreeOfVulnerability.Type.POLYNOMIAL, 5)
    if dov.is_exponential():
        return DegreeOfVulnerability(
            DegreeOfVulnerability.Type.EXPONENTIAL, 2)
    return dov


def update_dov(data: dict[str, Any]) -> dict[str, Any]:
    outputs = data["outputs"]
    steps = [get_step_from_output(output) for output in outputs]
    filtered_steps = [step for step in steps if step is not None]
    dov = DegreeOfVulnerability.from_steps(filtered_steps)
    data["dov"] = dov
    data["steps"] = filtered_steps
    return data


def print_statistics(
    steps: list[float],
    round_num: int = 2,
    delimiter: str = " & ",
    end: str = " \\\\ \n"
) -> None:
    labels = ["Count", "Mean", "Median", "Stdev"]
    values = [
        len(steps),
        round(statistics.mean(steps), round_num),
        round(statistics.median(steps), round_num),
        round(statistics.stdev(steps), round_num)
    ]
    print("{}".format(delimiter.join(labels)), end=end)
    print("{}".format(delimiter.join(map(str, values))), end=end)


def load_steps(filename: Path, prefix: str) -> np.array:
    logging.debug(f"Loading {prefix} steps from {filename}")
    label = f"avg_{prefix}_step"
    with jsonlines.open(filename) as dataset:
        xs = [data[label] for data in dataset if data[label] is not None]
    return np.array(xs)


def load_memo_sizes(filename: Path, prefix: str) -> np.array:
    with jsonlines.open(filename) as dataset:
        label = f"avg_{prefix}_memo_size"
        xs = [data[label] for data in dataset if data[label] is not None]
    return np.array(xs)


def load_mask(filename: Path, prefix: str) -> np.array:
    label = f"avg_{prefix}_step"
    with jsonlines.open(filename) as dataset:
        return np.array([data[label] is not None for data in dataset])


def load_eliminated(filename: Path) -> np.array:
    logging.debug(f"Loading eliminateds from {filename}")

    def get_eliminated(data: dict[str, Any]) -> Optional[int]:
        try:
            return data['statistics']['eliminated']
        except (KeyError, TypeError):
            return None

    with jsonlines.open(filename) as dataset:
        eliminateds = [get_eliminated(data) for data in dataset]
        return np.array(eliminateds)


def load_steps_dict(data_dir: Path) -> dict[str, Any]:
    """ Return `steps` dict with the following structure:
    `steps[matching_type][construction][scheduler][memo_scheme][input_type]`
    """
    steps: dict[str, dict] = {}
    iterator = product(MatchingType, iterate_bru_args())
    for matching_type, bru_args in iterator:
        construction = bru_args["construction"]
        scheduler = bru_args["scheduler"]
        memo_scheme = bru_args["memo-scheme"]
        steps_memo_scheme = (
            steps
            .setdefault(matching_type.value, {})
            .setdefault(construction.value, {})
            .setdefault(scheduler.value, {})
            .setdefault(memo_scheme.value, {})
        )

        basename = '-'.join([
            'all',
            matching_type.value,
            construction.value,
            scheduler.value,
            memo_scheme.value
        ])
        basename += '.jsonl'
        filename = data_dir / 'step' / basename
        for input_type in ['positive', 'negative']:
            xs = load_steps(filename, input_type)
            steps_memo_scheme.setdefault(input_type, xs)
    return steps


def load_memo_sizes_dict(data_dir: Path) -> dict[str, Any]:
    """ Return `memo_sizes` dict with the following structure:
    `memo_sizes[matching_type][construction][memo_scheme][input_type]`
    """
    memo_sizes: dict[str, dict] = {}
    iterator = product(MatchingType, ConstructionOption, MemoSchemeOption)
    for matching_type, construction, memo_scheme in iterator:
        memo_sizes_memo_scheme = (
            memo_sizes
            .setdefault(matching_type.value, {})
            .setdefault(construction.value, {})
            .setdefault(memo_scheme.value, {})
        )

        basename = '-'.join([
            'all',
            matching_type.value,
            construction.value,
            'spencer',
            memo_scheme.value
        ])
        basename += '.jsonl'
        filename = data_dir / 'memo-size' / basename
        for input_type in ['positive', 'negative']:
            xs = load_memo_sizes(filename, input_type)
            memo_sizes_memo_scheme[input_type] = xs
    return memo_sizes


def load_all_eliminateds_dict(data_dir: Path) -> dict[str, Any]:
    """ Return `eliminateds` dict with the following structure:
    `eliminateds[memo_scheme][input_type]`
    """
    eliminateds: dict[str, dict] = {}
    regex_type = RegexType.ALL
    construction = ConstructionOption.GLUSHKOV
    step_filename = data_dir / 'step' / 'all-full-thompson-lockstep-none.jsonl'
    input_types = ['positive', 'negative']
    mask = {
        input_type: load_mask(step_filename, input_type)
        for input_type in input_types
    }
    for memo_scheme in MemoSchemeOption:
        basename = '-'.join([
            regex_type.value, construction.value, memo_scheme.value])
        basename += '.jsonl'
        filename = Path('../../data/statistics') / basename
        eliminated = load_eliminated(filename)
        eliminateds_memo_scheme = eliminateds.setdefault(memo_scheme.value, {})
        for input_type in input_types:
            eliminateds_memo_scheme[input_type] = eliminated[mask[input_type]]
    return eliminateds
