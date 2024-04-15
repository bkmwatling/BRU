import logging
from itertools import product
from pathlib import Path
from typing import Any

import jsonlines  # type: ignore
import numpy as np  # type: ignore

from .options import iterate_bru_args
from .options import MatchingType
from .options import ConstructionOption
from .options import MemoSchemeOption
from .options import RegexType
from .options import SchedulerOption


def load_average(filename: Path, input_type: str, key: str) -> np.array:
    logging.debug(f"Loading {input_type}-{key} from {filename}")
    label = f"avg_{input_type}_benchmark_results"
    with jsonlines.open(filename) as dataset:
        return np.array([
            data[label][key] for data in dataset if data[label] is not None])


def load_mask(filename: Path, input_type: str) -> np.array:
    logging.debug(f"Loading {input_type}-mask from {filename}")
    label = f"avg_{input_type}_benchmark_results"
    with jsonlines.open(filename) as dataset:
        return np.array([data[label] is not None for data in dataset])


def load_states_dict(data_dir: Path) -> dict[str, Any]:
    """ Return `states` dict with the following structure:
    `states[matching_type][construction][scheduler][memo_scheme][input_type]`
    """
    states: dict[str, dict] = {}
    iterator = product(MatchingType, iterate_bru_args())
    for matching_type, bru_args in iterator:
        construction = bru_args["construction"]
        scheduler = bru_args["scheduler"]
        memo_scheme = bru_args["memo-scheme"]
        states_memo_scheme = (
            states
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
        filename = data_dir / 'benchmark_results' / basename
        for input_type in ['positive', 'negative']:
            xs = load_average(filename, input_type, 'state')
            states_memo_scheme.setdefault(input_type, xs)
    return states


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
        filename = data_dir / 'benchmark_results' / basename
        for input_type in ['positive', 'negative']:
            xs = load_average(filename, input_type, 'step')
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
        filename = data_dir / 'benchmark_results' / basename
        for input_type in ['positive', 'negative']:
            xs = load_average(filename, input_type, 'memo_entry')
            memo_sizes_memo_scheme[input_type] = xs
    return memo_sizes


def load_memory_usage_dict(data_dir: Path) -> dict[str, Any]:
    """ Return `memory_usage` dict with the following structure:
    `memory_usage[matching_type][construction][scheduler][memo_scheme][input_type]`
    """
    memory_usage: dict[str, dict] = {}
    iterator = product(MatchingType, iterate_bru_args())
    for matching_type, bru_args in iterator:
        construction = bru_args["construction"]
        scheduler = bru_args["scheduler"]
        memo_scheme = bru_args["memo-scheme"]
        memory_usage_memo_scheme = (
            memory_usage
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
        filename = data_dir / 'benchmark_results' / basename
        for input_type in ['positive', 'negative']:
            xs = (
                load_average(filename, input_type, 'memo_entry')
                + load_average(filename, input_type, 'thread')
            )
            memory_usage_memo_scheme.setdefault(input_type, xs)
    return memory_usage


def _load_eliminateds(
    data_dir: Path,
    regex_type: RegexType,
    construction: ConstructionOption,
    memo_scheme: MemoSchemeOption
) -> np.array:
    basename = '-'.join([
        regex_type.value,
        construction.value,
        memo_scheme.value
    ])
    basename += '.jsonl'
    filename = data_dir / 'statistics' / basename
    with jsonlines.open(filename) as dataset:
        return np.array([
            data['statistics']['eliminated']
            if data['statistics'] is not None
            else 0
            for data in dataset
        ])


def load_all_eliminateds_dict(data_dir: Path) -> dict[str, Any]:
    """ Return `eliminateds` dict with the following structure:
    `eliminateds[memo_scheme][input_type]`
    """
    eliminateds: dict[str, dict] = {}
    regex_type = RegexType.ALL
    construction = ConstructionOption.GLUSHKOV
    input_types = ['positive', 'negative']
    matching_type = MatchingType.FULL
    scheduler = SchedulerOption.SPENCER
    for memo_scheme in MemoSchemeOption:
        basename = '-'.join([
            regex_type.value,
            matching_type.value,
            construction.value,
            scheduler.value,
            memo_scheme.value
        ])
        basename += '.jsonl'
        filename = data_dir / 'benchmark_results' / basename
        eliminated = _load_eliminateds(
            data_dir, regex_type, construction, memo_scheme)
        eliminateds_memo_scheme = eliminateds.setdefault(memo_scheme.value, {})
        for input_type in input_types:
            mask = load_mask(filename, input_type)
            eliminateds_memo_scheme[input_type] = eliminated[mask]
    return eliminateds
