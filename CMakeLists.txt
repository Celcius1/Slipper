cmake_minimum_required(VERSION 3.23)
project(SlipperDaemon CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include standard GNU install directories to dynamically respect Gentoo prefixes (e.g., DESTDIR, /usr)
include(GNUInstallDirs)

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

# Modern CMake: Bind the include directories directly to the targets using FILE_SET
target_sources(slipperd PUBLIC FILE_SET HEADERS BASE_DIRS include)
target_sources(slip PUBLIC FILE_SET HEADERS BASE_DIRS include)
target_sources(slipper_tests PUBLIC FILE_SET HEADERS BASE_DIRS include)

# Native Install Targets mapping to standard Linux paths instead of hardcoded /usr/local/
install(TARGETS slipperd DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(TARGETS slip DESTINATION ${CMAKE_INSTALL_BINDIR})
# Install OpenRC init script with executable permissions
install(PROGRAMS scripts/slipperd DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/init.d)
# Install OpenRC configuration file
install(FILES scripts/slipperd.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/conf.d)
# Install Native Bash Environment
install(PROGRAMS src/execution/slipper-functions.sh DESTINATION ${CMAKE_INSTALL_DATADIR}/slipper)