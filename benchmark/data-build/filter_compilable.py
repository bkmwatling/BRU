import argparse
import logging
import subprocess
from pathlib import Path

import jsonlines  # type: ignore


def is_compilable(pattern: str) -> bool:
    try:
        subprocess.run(
            ['bru', 'compile', '-e', '-c', 'thompson', '--', pattern],
            check=True, timeout=10,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
        )
        subprocess.run(
            ['bru', 'compile', '-e', '-c', 'glushkov', '--', pattern],
            check=True, timeout=10,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
        )
        return True
    except subprocess.CalledProcessError as e:
        logging.debug(f"Failed to parse {pattern}")
        logging.debug(e.stderr)
    except subprocess.TimeoutExpired as e:
        logging.debug(f"Timeout for {pattern}")
        logging.debug(e)
    except (TypeError, ValueError, OSError, ) as e:
        logging.debug(f"Data error for {pattern}")
        logging.debug(e)
    return False


def write_compilable_dataset(input_path: Path, output_path: Path) -> None:
    regex_dataset = jsonlines.open(input_path, mode='r')
    compilable_dataset = (
        data for data in regex_dataset if is_compilable(data['pattern']))
    output_file = jsonlines.open(output_path, "w")
    output_file.write_all(compilable_dataset)
    output_file.close()
    regex_dataset.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(
        description="Filter out non-compilable regexes")
    parser.add_argument("input", type=Path, help="Input file")
    parser.add_argument("output", type=Path, help="Output file")
    args = parser.parse_args()
    write_compilable_dataset(args.input, args.output)
