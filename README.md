# SimtLab

SimtLab is an educational, CPU-based SIMT GPU simulator written in modern C++20.

## Status

Phase 0.2: repository skeleton and CI.

## Prerequisites

- CMake 3.20 or newer
- A C++20-compatible compiler

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Run

```sh
./build/simtlab
```
