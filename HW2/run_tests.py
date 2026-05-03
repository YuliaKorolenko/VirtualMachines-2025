import os
import subprocess
import glob
import argparse
from pathlib import Path
import sys

regression_dir = "./regression"
exe = "./build/HW2"

def clean_lines(text):
    lines = text.splitlines()
    cleaned = []
    for line in lines:
        line = line.strip()
        if not line or line.startswith("$"):
            continue
        while line.startswith(">"):
            line = line[1:].strip()
        if line:
            cleaned.append(line)
    return cleaned


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run Lama bytecode regression tests."
    )
    parser.add_argument(
        "--exe",
        type=Path,
        default=Path(exe),
        help="Path to the interpreter executable.",
    )

    return parser.parse_args()


def run_test(test_name, args, bc_file):
    input_file = os.path.join(regression_dir, f"{test_name}.input")
    answer_file = os.path.join(regression_dir, f"{test_name}.t")

    if not os.path.isfile(input_file) or not os.path.isfile(answer_file):
        return False

    result = subprocess.run(
        [args.exe, bc_file],
        stdin=open(input_file, "r"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    if result.returncode != 0:
        print(f"[FAIL] {test_name}")
        print(f"  exit code: {result.returncode}")
        if result.stderr.strip():
            print(f"  stderr: {result.stderr.strip()}")

        return False

    output_clean = clean_lines(result.stdout)

    with open(answer_file, "r", encoding="utf-8") as f:
        answer_clean = clean_lines(f.read())

    if output_clean == answer_clean:
        return True
    else:
        print(f"[FAIL] {test_name}")
        print("  Output:")
        print("\n".join(output_clean))
        print("  Expected:")
        print("\n".join(answer_clean))
        return False


def main():
    total_tests = 0
    passed_tests = 0
    failed_tests = 0

    args = parse_args()

    for bc_file in sorted(glob.glob(os.path.join(regression_dir, "test*.bc"))):
        filename = os.path.basename(bc_file).rsplit(".bc", 1)[0]
        total_tests += 1
        result = run_test(filename, args, bc_file)

        if result:
            passed_tests += 1
        else:
            failed_tests += 1
            print("-" * 40)

    print("\n" + "=" * 40)
    print(f"Total tests: {total_tests}")
    print(f"Passed: {passed_tests}")
    print(f"Failed: {failed_tests}")
    if total_tests > 0:
        print(f"Success rate: {passed_tests / total_tests * 100:.2f}%")
    print("=" * 40)
    return 0 if failed_tests == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
