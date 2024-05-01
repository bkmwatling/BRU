import logging
from itertools import product
from pathlib import Path
from typing import Any

import jsonlines  # type: ignore
import numpy as np  # type: ignore
import matplotlib  # type: ignore
from matplotlib import pyplot as plt  # type: ignore

from .options import iterate_bru_args
from .options import MatchingType
from .options import ConstructionOption
from .options import MemoSchemeOption
from .options import RegexType
from .options import SchedulerOption

logger = logging.getLogger(__name__)


def load_average(filename: Path, input_type: str, key: str) -> np.array:
    logger.info(f"Loading {input_type}-{key} from {filename}")
    label = f"avg_{input_type}_benchmark_results"
    with jsonlines.open(filename) as dataset:
        return np.array([
            data[label][key] for data in dataset if data[label] is not None])


def load_mask(filename: Path, input_type: str) -> np.array:
    logger.info(f"Loading {input_type}-mask from {filename}")
    label = f"avg_{input_type}_benchmark_results"
    with jsonlines.open(filename) as dataset:
        return np.array([data[label] is not None for data in dataset])


def load_input_length(filename: Path) -> np.array:
    average_length = []
    with jsonlines.open(filename) as dataset:
        for data in dataset:
            positive_inputs = data["positive_inputs"]
            negative_inputs = data["negative_inputs"]
            inputs = positive_inputs + negative_inputs
            lengths = np.array([len(s) for s in inputs])

            positive_benchmark_results = data["positive_benchmark_results"]
            negative_benchmark_results = data["negative_benchmark_results"]

            if positive_benchmark_results is None:
                positive_benchmark_results = [None] * len(positive_inputs)
            if negative_benchmark_results is None:
                negative_benchmark_results = [None] * len(negative_inputs)
            benchmark_results = (
                positive_benchmark_results + negative_benchmark_results)
            mask = np.array([e is not None for e in benchmark_results])
            if not np.any(mask):
                continue
            average_length.append(np.mean(lengths[mask]))
    return np.array(average_length)


def load_pattern_length(filename: Path) -> np.array:
    length = []
    with jsonlines.open(filename) as dataset:
        for data in dataset:
            positive_benchmark_results = data["positive_benchmark_results"]
            negative_benchmark_results = data["negative_benchmark_results"]

            if (
                positive_benchmark_results is None
                and negative_benchmark_results is None
            ):
                continue

            length.append(len(data["pattern"]))
    return np.array(length)


def load_normalized_step(filename: Path, input_type: str) -> np.array:
    logger.info(f"Loading input length of {input_type} in {filename}")

    strings_label = f"{input_type}_inputs"
    mask_label = f"{input_type}_benchmark_results"
    normalized_step = []
    with jsonlines.open(filename) as dataset:
        for data in dataset:
            benchmark_results = data[mask_label]
            if benchmark_results is None:
                continue
            mask = np.array([e is not None for e in benchmark_results])

            inputs = data[strings_label]
            lengths = np.array([len(s) for s in inputs])
            steps = np.array([
                e['step'] if e is not None else None
                for e in benchmark_results
            ])
            normalized_step.append(np.mean(steps[mask] / (lengths[mask] + 1)))

    return np.array(normalized_step)


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


def load_normalized_steps_dict(data_dir: Path) -> dict[str, Any]:
    """ Return `normalized_step` dict with the following structure:
    `memory_usage[matching_type][construction][scheduler][memo_scheme][input_type]`
    """
    normalized_steps: dict[str, dict] = {}
    iterator = product(MatchingType, iterate_bru_args())
    for matching_type, bru_args in iterator:
        construction = bru_args["construction"]
        scheduler = bru_args["scheduler"]
        memo_scheme = bru_args["memo-scheme"]
        normalized_steps_memo_scheme = (
            normalized_steps
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
            xs = load_normalized_step(filename, input_type)
            normalized_steps_memo_scheme.setdefault(input_type, xs)
    return normalized_steps


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


def save_hist2d(
    xs: np.array,
    ys: np.array,
    xlabel: str,
    ylabel: str,
    bins: tuple[np.array, np.array],
    filepath: Path,
    dry_run: bool = False
) -> None:
    rcParams = matplotlib.rcParams
    if not dry_run:
        matplotlib.use("pgf")
        matplotlib.rcParams.update({
            "font.family": "serif",
            "font.size": 11,
            "pgf.rcfonts": False,
            "pgf.texsystem": "pdflatex",
            "text.usetex": True,
        })
    plt.close()
    logger.info(f"Saving {filepath}")
    logger.info(f"bad glushkov: {sum(xs < ys)}/{len(xs)}")

    fig, axes = plt.subplots(1, 1, figsize=(2, 2))
    axes.axline((0, 0), slope=1, alpha=0.3, color='black', linewidth=0.2)

    h, _, _, cmap = axes.hist2d(xs, ys, bins=bins, norm='log')
    axes.set_xscale('symlog')
    axes.set_yscale('symlog')
    axes.set_xlabel(xlabel)
    axes.set_ylabel(ylabel)
    axes.axis('square')

    pos = axes.get_position()
    cbar_ax = fig.add_axes([pos.x1 + 0.01, pos.y0, 0.02, pos.y1 - pos.y0])
    fig.colorbar(cmap, cax=cbar_ax)
    if dry_run:
        plt.show()
    else:
        filepath.parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(filepath.with_suffix('.pdf'), bbox_inches='tight')
        plt.savefig(filepath.with_suffix('.pgf'), bbox_inches='tight')
    matplotlib.rcParams.update(rcParams)
