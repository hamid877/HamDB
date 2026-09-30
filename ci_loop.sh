#!/bin/bash
set -e
for i in {1..10}; do
    echo "Loop $i"
    rm -rf build
    make configure
    make build
    make lint
    make test
done
echo "All 10 loops passed!"
