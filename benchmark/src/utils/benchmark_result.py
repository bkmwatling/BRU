import re

ELIMINATED_LABEL = "NUMBER OF TRANSITIONS ELIMINATED FROM FLATTENING:"
MEMOISED_LABEL = "NUMBER OF STATES MEMOISED:"
THREAD_LABEL = "TOTAL THREADS IN POOL"


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
                self.memoised = int(line.split(': ')[1])
                continue
            if line.startswith(THREAD_LABEL):
                self.thread = int(line.split(': ')[1])
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
            'state': self.state,
            'eliminated': self.eliminated,
            'memoised': self.memoised,
            'thread': self.thread
        }
