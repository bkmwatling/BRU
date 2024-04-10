# %%
import matplotlib
import jsonlines
import sys
from pathlib import Path
sys.path.append('../../src/')

from utils import benchmark
from utils import iterate_bru_args
from utils import BenchmarkResult
from utils import RegexType
from utils import MatchingType

import numpy as np
import matplotlib.pyplot as plt


# %%
def load_steps(filename: Path, prefix: str) -> np.array:
    with jsonlines.open(filename) as dataset:
        label = f"avg_{prefix}_step"
        xs = [data[label] for data in dataset if data[label] is not None]
    return np.array(xs)

# %%
regex_type = RegexType.ALL
print(regex_type.value)
matching_type = MatchingType.FULL.value()

data = {}
for bru_args in iterate_bru_args():
    label = "-".join([e.value for e in bru_args.values()])
    print(label)
    xs = load_steps(Path(f'../data/step/{regex_type}-{matching_type}-{label}-full.jsonl'), 'positive')
    xs += load_steps(Path(f'../data/step/{regex_type}-{matching_type}-{label}-full.jsonl'), 'negative')
    data[label] = xs

plt.boxplot(data.values(), showmeans=True)
plt.xticks(range(len(data)), data.keys(), rotation=70)
plt.show()

# Hide outliers
plt.boxplot(data.values(), showmeans=True, showfliers=False)
plt.xticks(range(len(data)), data.keys(), rotation=70)
plt.show()

# %%
