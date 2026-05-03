#!/bin/bash


EXE="$1"

if [ -z "$EXE" ]; then
    EXE="./build/HW2"
fi

PERFORMANCE_DIR="./performance"
SORT_LAMA="$PERFORMANCE_DIR/Sort.lama"
SORT_BC="$PERFORMANCE_DIR/Sort.bc"
SORT_INPUT="$PERFORMANCE_DIR/Sort.input"

echo "--- Performance Benchmarking Script ---"
echo "Test File: $SORT_LAMA"
echo "Input File: $SORT_INPUT"

time_and_report() {
    local label="$1"
    shift
    echo ""
    echo "[$label]"
    local err
    err=$(mktemp)
    { time -p "$@" >/dev/null; } 2>"$err"
    local rc=$?
    grep '^real' "$err"
    if [ "$rc" -ne 0 ]; then
        echo "FAIL: [$label] exit=$rc" >&2
        grep -vE '^(real|user|sys) ' "$err" >&2
    fi
    rm -f "$err"
}

time_and_report "lamac -i " lamac -i "$SORT_LAMA" < "$SORT_INPUT"

time_and_report "lamac -s " lamac -s "$SORT_LAMA" < "$SORT_INPUT"

time_and_report "bytecode interpretation" "$EXE" "$SORT_BC" < "$SORT_INPUT"

echo ""
echo "--- Benchmarking Complete ---"