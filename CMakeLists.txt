cmake_minimum_required(VERSION 3.10)
project(ThreadedMemoryManagementFramework)

# Set the path to your local Google Benchmark installation
set(BENCHMARK_ROOT /home/logesh-pt7689/local)
set(benchmark_DIR ${BENCHMARK_ROOT}/lib/cmake/benchmark)

# Find the Benchmark package
find_package(benchmark REQUIRED)

# Add C library
add_library(my_memory STATIC NoFootThread.c)

# Add the C++ executable
add_executable(out bench.cpp)

# Link Benchmark and your C library
target_link_libraries(out PRIVATE benchmark::benchmark my_memory)