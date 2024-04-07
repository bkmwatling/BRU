# ---
# jupyter:
#   jupytext:
#     text_representation:
#       extension: .py
#       format_name: light
#       format_version: '1.5'
#       jupytext_version: 1.16.1
#   kernelspec:
#     display_name: Python 3 (ipykernel)
#     language: python
#     name: python3
# ---

# +
import matplotlib
import jsonlines
from pathlib import Path

import numpy as np
from matplotlib import pyplot as plt


# -

def load_memo_sizes(filename: Path, prefix: str) -> np.array:
    with jsonlines.open(filename) as dataset:
        label = f"avg_{prefix}_memo_size"
        xs = [data[label] for data in dataset if data[label] is not None]
    return np.array(xs)


xs = load_memo_sizes(Path('../data/benchmark/memo_size-benchmark-all-thompson-cn-spencer-full.jsonl'), 'positive')
ys = load_memo_sizes(Path('../data/benchmark/memo_size-benchmark-all-glushkov-cn-spencer-full.jsonl'), 'positive')

# Range excluding outliers
total = xs + ys
quartile_1 = np.quantile(total, 0.25)
quartile_3 = np.quantile(total, 0.75)
iqr = quartile_3 - quartile_1
maximum = np.max(total, where=xs <= quartile_3 + 1.5 * iqr, initial=-np.inf)
minimum = np.min(total, where=xs >= quartile_1 - 1.5 * iqr, initial=np.inf)

# Atan Histogram
normalized_theta = (np.arctan2(xs, ys) - np.pi/4) / (np.pi/4)
plt.hist(normalized_theta, bins=100, range=[-1, 1])
plt.xscale('symlog')
plt.yscale('log')
plt.show()

# Scatter plot
plt.scatter(xs, ys)
plt.xscale('log')
plt.yscale('log')
plt.axis('square')
plt.show()

# Histogram 2D
x_space = np.geomspace(1, max(xs), 100)
y_space = np.geomspace(1, max(ys), 100)
h = plt.hist2d(xs, ys, bins=(x_space, y_space), norm = 'log')
plt.colorbar(h[3])
plt.xscale('log')
plt.yscale('log')
plt.axis('square')
plt.show()

# Histogram (excluding outliers)
plt.hist([xs, ys], range=(minimum, maximum), log=True, bins=10)
plt.show()

# Difference of histogram (excluding outliers)
plt1, bin1, _ = plt.hist(xs, range=(minimum, maximum), log=True, bins=100)
plt2, bin2, _ = plt.hist(ys, range=(minimum, maximum), log=True, bins=100)
plt.clf()
plt.bar(bin1[:-1], width = np.diff(bin1), height=plt2 - plt1, align='edge')
plt.show()
