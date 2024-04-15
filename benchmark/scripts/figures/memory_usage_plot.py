# ---
# jupyter:
#   jupytext:
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.16.1
#   kernelspec:
#     display_name: Python 3 (ipykernel)
#     language: python
#     name: python3
# ---

# %%
import sys
import logging
import importlib
import itertools
from pathlib import Path

import numpy as np  # type: ignore
from matplotlib import pyplot as plt  # type: ignore

sys.path.append('../../src/')
import utils.figures  # type: ignore # noqa: E402

# %%
data_dir = Path('../../data/')

# %%
importlib.reload(utils.figures)
from utils.figures import load_all_eliminateds_dict  # noqa: E402
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout, force=True)
eliminateds = load_all_eliminateds_dict(data_dir)
logging.basicConfig(level=logging.WARNING, stream=sys.stderr, force=True)

# %%
importlib.reload(utils.figures)
from utils.figures import load_memory_usage_dict  # noqa: E402
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout, force=True)
memory_usage = load_memory_usage_dict(data_dir)
logging.basicConfig(level=logging.WARNING, stream=sys.stderr, force=True)

# %%
# Histogram 2D
importlib.reload(plt)

logging.basicConfig(level=logging.WARNING, stream=sys.stderr, force=True)
options = itertools.product(
    ['full', 'partial'],
    [('spencer', 'none'), ('spencer', 'cn'),
     ('spencer', 'in'), ('lockstep', 'none')]
)
threshold = 0

for matching_type, (scheduler, memo_scheme) in options:
    input_types = ['positive', 'negative']
    memory_usage_memo_scheme = (
        memory_usage[matching_type]['thompson'][scheduler][memo_scheme])
    xs = np.concatenate([
        memory_usage_memo_scheme[input_type] for input_type in input_types])
    xlabel = "-".join([matching_type, 'thompson', scheduler, memo_scheme])
    ys = np.concatenate([
        memory_usage[matching_type]['flat'][scheduler][memo_scheme][input_type]
        for input_type in input_types
    ])
    ylabel = "-".join([matching_type, 'flat', scheduler, memo_scheme])
    zs = np.concatenate(
        [eliminateds[memo_scheme][input_type] for input_type in input_types])

    # Range excluding outliers
    total = xs + ys
    quartile_1 = np.quantile(total, 0.25)
    quartile_3 = np.quantile(total, 0.75)
    iqr = quartile_3 - quartile_1
    maximum = np.max(
        total, where=xs <= quartile_3 + 1.5 * iqr, initial=-np.inf)
    minimum = np.min(total, where=xs >= quartile_1 - 1.5 * iqr, initial=np.inf)
    x_space = np.geomspace(1, max(xs+ys), 50)
    y_space = np.geomspace(1, max(ys+ys), 50)

    _h1, _, _ = np.histogram2d(
        xs[zs <= threshold], ys[zs <= threshold], bins=(x_space, y_space))
    _h2, _, _ = np.histogram2d(
        xs[zs > threshold], ys[zs > threshold], bins=(x_space, y_space))
    vmax = max(np.max(_h1), np.max(_h2))

    fig, axes = plt.subplots(1, 2, figsize=(12, 6))

    h, _, _, cmap = axes[0].hist2d(
        xs[zs <= threshold], ys[zs <= threshold],
        bins=(x_space, y_space), norm='log', vmin=1, vmax=vmax
    )
    axes[0].set_xscale('symlog')
    axes[0].set_yscale('symlog')
    axes[0].set_xlabel(xlabel + f" (elim. <= {threshold})")
    axes[0].set_ylabel(ylabel)
    axes[0].axis('square')

    axes[1].hist2d(xs[zs > threshold], ys[zs > threshold], bins=(
        x_space, y_space), norm='log', vmin=1, vmax=vmax)
    axes[1].set_xscale('symlog')
    axes[1].set_yscale('symlog')
    axes[1].set_xlabel(xlabel + f" (elim. > {threshold})")
    axes[1].axis('square')
    
    axes[0].plot([0, max(xs+ys)*2], [0, max(xs+ys)*2], alpha=0.5, color='black', linestyle='--')
    axes[1].plot([0, max(xs+ys)*2], [0, max(xs+ys)*2], alpha=0.5, color='black', linestyle='--')

    fig.subplots_adjust(right=0.8)
    axes[1].get_position()
    pos = axes[1].get_position()
    cbar_ax = fig.add_axes(
        [pos.x1 + 0.01, pos.y0 + 0.01, 0.01, pos.y1 - pos.y0 - 0.02])

    try:
        fig.colorbar(cmap, cax=cbar_ax)
        plt.subplots_adjust(wspace=0.3)
        output_dir = Path('./outputs/memory_usage')
        output_dir.mkdir(parents=True, exist_ok=True)
        filename = "-".join([matching_type, scheduler, memo_scheme]) + '.pdf'
        plt.savefig(output_dir / filename)
        plt.show()
    except Exception:
        continue

# %%
