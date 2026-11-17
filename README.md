# simtlab-gpu-simulator
A cycle-approximate SIMT GPU simulator in C++20 with a custom mini-ISA, warp divergence and reconvergence, memory coalescing and shared memory bank conflict modeling, an occupancy-aware warp scheduler, and performance counters. Benchmarked on naive vs optimized kernels and validated against CUDA runs on a T4.
