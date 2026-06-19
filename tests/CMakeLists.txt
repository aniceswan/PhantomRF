# =============================================================================
#  PhantomRF — Top-level test CMake
# ----------------------------------------------------------------------------
#  Lets IDEs (CLion, VSCode, Qt Creator) build and run the host-side test
#  suite without going through PlatformIO. CI continues to use
#  `pio test -e native`; this file is a developer convenience.
#
#  Layout:
#      tests/
#      ├── CMakeLists.txt           # this file
#      ├── unit/
#      │   └── CMakeLists.txt       # builds the test executable
#      ├── native_mock.{h,cpp}      # the host-side Arduino/ESP32 mocks
#      └── test_*.cpp               # the test files themselves
#
#  Build:
#      cmake -S tests -B build
#      cmake --build build -j
#      ctest --test-dir build --output-on-failure
# =============================================================================

cmake_minimum_required(VERSION 3.16)

project(phantomrf-tests CXX)

# ---------------------------------------------------------------------------
# Global compile settings
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# We pull in some POSIX threading primitives in the native mock.
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Where the production source lives. Adjust if the repo layout changes.
set(PHM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src" CACHE PATH
    "Path to the PhantomRF source tree")

# ---------------------------------------------------------------------------
# Top-level: pull in the unit subdirectory.
# ---------------------------------------------------------------------------

enable_testing()
add_subdirectory(unit)
