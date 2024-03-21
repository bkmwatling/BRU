import re
import statistics
import logging
from functools import total_ordering
from typing import (Optional, )
from enum import Enum


class ConstructionOption(Enum):
    THOMPOSON = "thompson"
    GLUSHKOV = "glushkov"


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


class BenchmarkResult():
    def __init__(self, stderr: str):
        parsed: list[tuple[int, int]] = []
        pattern = re.compile(r"[A-Z]+: (\d+) \(FAILED: (\d+)\)")
        for line in stderr.strip().split("\n"):
            match = pattern.match(line)
            assert match is not None
            parsed.append((int(match.group(1)), int(match.group(2))))
        self.match, self.match_failed = parsed[0]
        self.memo, self.memo_failed = parsed[1]
        self.char, self.char_failed = parsed[2]
        self.pred, self.pred_failed = parsed[3]

    @property
    def steps(self) -> int:
        return self.match + self.memo + self.char + self.pred

    @property
    def steps_failed(self) -> int:
        return (
            self.match_failed
            + self.memo_failed
            + self.char_failed
            + self.pred_failed
        )


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

            max_last_half = max(deltas[half:])
            max_first_half = max(deltas[:half])
            # Almost constant
            if max_last_half - max_first_half < half:
                return cls(cls.Type.POLYNOMIAL, degree)

            if len(deltas) == 2:
                break

            deltas = [deltas[i+1] - deltas[i] for i in range(len(deltas)-1)]
            degree += 1

        return cls._exponential_vulnerability_from_steps(steps)


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
