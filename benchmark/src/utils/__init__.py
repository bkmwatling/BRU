import statistics
import subprocess
from typing import Optional

from .options import ConstructionOption as ConstructionOption
from .options import MemoSchemeOption as MemoSchemeOption
from .options import SchedulerOption as SchedulerOption
from .options import RegexType as RegexType
from .options import MatchingType as MatchingType
from .options import BruArgs as BruArgs
from .options import iterate_bru_args as iterate_bru_args
from .benchmark_result import BenchmarkResult as BenchmarkResult
from .benchmark_result import ELIMINATED_LABEL as ELIMINATED_LABEL
from .benchmark_result import MEMOISED_LABEL as MEMOISED_LABEL
from .benchmark_result import THREAD_LABEL as THREAD_LABEL
from .degree_of_vulnerability import (
    DegreeOfVulnerability as DegreeOfVulnerability)
from .degree_of_vulnerability import trim_dov as trim_dov
from .degree_of_vulnerability import update_dov as update_dov


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
