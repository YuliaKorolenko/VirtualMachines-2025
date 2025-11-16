import os
import subprocess
import glob


regression_dir = "./regression"
exe = "./cmake-build-debug/hw2"

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

total_tests = 0
passed_tests = 0
failed_tests = 0

for bc_file in sorted(glob.glob(os.path.join(regression_dir, "test*.bc"))):
    filename = os.path.basename(bc_file).rsplit(".bc", 1)[0]
    input_file = os.path.join(regression_dir, f"{filename}.input")
    answer_file = os.path.join(regression_dir, f"{filename}.t")

    if not os.path.isfile(input_file) or not os.path.isfile(answer_file):
        continue

    if '74' in filename:
        continue

    total_tests += 1

    # Запуск программы с input, stderr отправляем в /dev/null
    result = subprocess.run(
        [exe, bc_file],
        stdin=open(input_file, "r"),
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True
    )

    # Чистим вывод и эталон
    output_clean = clean_lines(result.stdout)
    with open(answer_file, "r", encoding="utf-8") as f:
        answer_clean = clean_lines(f.read())

    if output_clean == answer_clean:
        print(f"{filename}: ✅ OK")
        passed_tests += 1
    else:
        print(f"{filename}: ❌ MISMATCH")
        print("Output:")
        print("\n".join(output_clean))
        print("Expected:")
        print("\n".join(answer_clean))
        print("-" * 40)
        failed_tests += 1

print("\n" + "="*40)
print(f"Total tests: {total_tests}")
print(f"✅ Passed: {passed_tests}")
print(f"❌ Failed: {failed_tests}")
if total_tests > 0:
    print(f"Success rate: {passed_tests/total_tests*100:.2f}%")
print("="*40)
