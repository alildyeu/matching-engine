# Matching Engine Project

A C++ matching engine implementation for processing market orders.

## System Compatibility

This project is primarily developed for Unix-like systems (Linux, macOS) but can also be built and run on:
- Ubuntu/Debian Linux
- macOS
- Windows using Windows Subsystem for Linux (WSL)

If using Windows, we recommend installing WSL2 with Ubuntu and following the Linux-based instructions.

## Prerequisites

- CMake (version 3.10 or higher)
- C++ compiler with C++17 support
- Make

## Building the Project

1. Create and navigate to the build directory:
```sh
mkdir -p build
cd build
```

2. Generate the build files:
```sh
cmake ..
```

3. Build the project:
```sh
make -j4
```

The `-j4` flag enables parallel compilation with 4 threads. Adjust the number based on your CPU cores.

## Running the Application

The matching engine binary takes two required arguments:
- Input CSV file containing orders
- Output CSV file for results

```sh
./MyMatchingEngine <input_csv> <output_csv>
```

Example:
```sh
./MyMatchingEngine ../input.csv output.csv
```

## Project Structure

```
.
├── generate_synthetic_data.py  # Script to generate test data
├── README.md                  # This file
├── sample_data                # Sample data directory
│   ├── base_input.csv        # Base test input
│   └── expected_output.csv   # Expected test output
├── samplein.csv              # Generated synthetic data
└── src                       # Source code directory
    ├── app_config.cpp
    ├── argparse.cpp
    ├── build                 # Build directory
    ├── CMakeLists.txt
    ├── include              # Header files
    │   ├── app_config.hpp
    │   ├── argparse.hpp
    │   ├── logger.hpp
    │   ├── main.hpp
    │   ├── order.hpp
    │   ├── orderbook.hpp
    │   └── thread_safe_queue.hpp
    ├── main.cpp
    ├── order.cpp
    └── orderbook.cpp
```

## Generating Synthetic Test Data

To generate synthetic order data for testing, use the Python script:

```sh
python3 generate_synthetic_data.py <number_of_orders> -o <output_file>
```

Example to generate 4 million orders:
```sh
python3 generate_synthetic_data.py 4000000 -o samplein.csv
```

## Performance Testing

To measure the execution time of the matching engine:

1. First generate synthetic data as described above
2. Navigate to the build directory:
```sh
cd src/build
```

3. Run the matching engine with the `time` command:
```sh
time ./MyMatchingEngine ../../samplein.csv output.csv
```

The `time` command will output three metrics:
- `real`: Wall clock time elapsed during execution
- `user`: CPU time spent in user space
- `sys`: CPU time spent in kernel space

Run multiple iterations to get consistent performance measurements.

## Clean Build

To clean the build directory and rebuild from scratch:

```sh
make clean
make -j4
```