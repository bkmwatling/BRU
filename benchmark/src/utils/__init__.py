import re
import numpy as np  # type: ignore
from functools import total_ordering
from typing import (Optional, )
from enum import Enum


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

    def __init__(self, _type: Type, degree: Optional[int] = None) -> None:
        if (_type == self.Type.UNKNOWN) != (degree is None):
            raise ValueError("Invalid parameters")
        self.type = _type
        self.degree = degree

    def is_vulnerable(self) -> bool:
        if self.is_unknown() or self.is_linear() or self.is_constant():
            return False
        return True

    def is_polynomial(self) -> bool:
        return self.type == DegreeOfVulnerability.Type.POLYNOMIAL

    def is_exponential(self) -> bool:
        return self.type == DegreeOfVulnerability.Type.EXPONENTIAL

    def is_unknown(self) -> bool:
        return self.type == DegreeOfVulnerability.Type.UNKNOWN

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
            return False
        if self.is_unknown():
            return False
        if self.is_exponential():
            if other.is_unknown():
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
            return cls(cls.Type.UNKNOWN)

        # Assuming step[i] = a*(k^i)+b, and return k.
        deltas = [steps[i] - steps[i+1] for i in range(len(steps)-1)]
        # deltas[i] = a*(k^(i+1))+b - a*(k^i)+b = a*(k^i)*(k-1)
        assert len(deltas) > 1
        # deltas[i] // deltas[i-1] = k
        degree = round(deltas[-1] / deltas[-2])
        if degree < 2:
            return cls(cls.Type.UNKNOWN)
        return cls(cls.Type.EXPONENTIAL, degree)

    @classmethod
    def from_steps(cls, steps: list[int]):
        if len(steps) < 2:
            return cls(cls.Type.UNKNOWN)

        n = len(steps)
        # assume steps[i] = sum_{j in n} c_j * i^j
        # then we solve equation for c_j
        a = [[i**j for j in range(n)] for i in range(n)]
        c = np.rint(np.linalg.solve(a, steps)).astype(int).tolist()
        if c[-1] != 0:
            return cls._exponential_vulnerability_from_steps(steps)
        degree = len(c) - [e != 0 for e in reversed(c)].index(True)
        return cls(cls.Type.POLYNOMIAL, degree)