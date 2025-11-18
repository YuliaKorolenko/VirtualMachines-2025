#!/bin/bash
shopt -s nullglob

for test in test*.lama; do
    echo "===> Running $test"
    if lamac -b "$test"; then
        echo "✅ $test OK"
    else
        echo "❌ $test cannot compile"
    fi
    echo
done
