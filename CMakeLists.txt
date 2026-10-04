cmake_minimum_required(VERSION 3.12)
project(SlipperDaemon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

# Recursively find all .cpp files and store them in the ALL_SRC_FILES variable.
file(GLOB_RECURSE ALL_SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")

# Filter out all entry points from the shared sources to prevent "multiple definition of main" errors.
set(CORE_SRC_FILES ${ALL_SRC_FILES})
list(FILTER CORE_SRC_FILES EXCLUDE REGEX "src/daemon/slipperd\\.cpp$")
list(FILTER CORE_SRC_FILES EXCLUDE REGEX "src/cli/slip\\.cpp$")
list(FILTER CORE_SRC_FILES EXCLUDE REGEX "src/main\\.cpp$") # Kept in case the old file is still present

# 1. Privileged Slipper Daemon (Links the heavy resolution engine)
add_executable(slipperd src/daemon/slipperd.cpp ${CORE_SRC_FILES})

# 2. Unprivileged User-Space CLI (Lightweight socket client, no core linking required)
add_executable(slip src/cli/slip.cpp)

# 3. Standalone Test Suite
add_executable(slipper_tests tests/test_runner.cpp ${CORE_SRC_FILES})