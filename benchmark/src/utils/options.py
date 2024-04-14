from typing import Iterable, TypedDict
from enum import Enum


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
