#!/bin/bash

PERFORMANCE_DIR="./performance"
EXE="./cmake-build-debug/hw2"
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
    (time -p "$@" > /dev/null) 2>&1 | grep real
}

time_and_report "lamac -i " lamac -i "$SORT_LAMA" < "$SORT_INPUT"

time_and_report "lamac -s " lamac -s "$SORT_LAMA" < "$SORT_INPUT"

time_and_report "bytecode interpretation" "$EXE" "$SORT_BC" < "$SORT_INPUT"

echo ""
echo "--- Benchmarking Complete ---"