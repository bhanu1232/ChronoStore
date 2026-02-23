#!/bin/bash
export PATH=/mingw64/bin:$PATH
cd "C:/Users/Bhanu/Downloads/projects/CPP/ChronoStore"

echo "=== Building chronostore ==="
g++ -std=c++17 -O2 -Wall -pthread main.cpp store.cpp -o chronostore.exe
echo "Main build exit: $?"

echo "=== Building benchmark ==="
g++ -std=c++17 -O2 -Wall -pthread benchmark.cpp store.cpp -o chronostore_bench.exe
echo "Bench build exit: $?"

echo "=== Done ==="
