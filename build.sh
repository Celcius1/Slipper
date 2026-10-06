#!/bin/bash

# Exit immediately if a command exits with a non-zero status to prevent cascading errors
set -e

echo "--- Preparing Slipper Build Environment ---"
# Create the build directory if it does not already exist to keep the source tree clean
mkdir -p build

# Navigate into the build directory
cd build

echo "--- Configuring with CMake ---"
# Generate the Makefiles using the CMakeLists.txt located in the parent directory
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc ..

echo "--- Compiling Slipper ---"
# Compile the project using all available CPU cores to optimise build times
make -j$(nproc)

echo "--- Compilation Complete ---"