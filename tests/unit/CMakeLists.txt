# =============================================================================
#  PhantomRF — Host-side unit test executable
# ----------------------------------------------------------------------------
#  Builds a single executable `phantomrf-tests` from the native mocks and
#  every test_*.cpp file in the parent tests/ directory. CTest is wired up
#  to invoke the binary and report Unity's pass/fail summary.
#
#  This file is consumed by the top-level tests/CMakeLists.txt.
# =============================================================================

# ---------------------------------------------------------------------------
# Test sources
# ---------------------------------------------------------------------------

set(PHM_TEST_SRCS
    # Mocks — first, so test files can use them.
    ${CMAKE_CURRENT_SOURCE_DIR}/../native_mock.cpp
    # Tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../test_channel_math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../test_frame_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../test_settings.cpp
)

add_executable(phantomrf-tests ${PHM_TEST_SRCS})

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
#
# The tests need to see both the production headers in src/ and the
# production sub-headers (src/core, src/hal, src/utils, etc.). We add
# them with PRIVATE scope because no other library will consume this
# target.

target_include_directories(phantomrf-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
)

# Some tests reference the in-tree project-relative includes (e.g.
# "utils/ChannelMath.h") so we also add src/ as a header search root.
target_include_directories(phantomrf-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# ---------------------------------------------------------------------------
# Compile options
# ---------------------------------------------------------------------------

target_compile_options(phantomrf-tests PRIVATE
    # Activate the in-test mocks.
    -DPHM_NATIVE_TEST=1
    # We want pedantic warnings so the tests are held to the same
    # standard as the production code. The warnings are not errors by
    # default so that local builds stay fast; CI overrides with
    # `-Werror` via CXXFLAGS.
    -Wall
    -Wextra
    -Wpedantic
    # Unity's legacy signatures use implicit conversions that older
    # versions of -Wpedantic flag. Silence just those.
    -Wno-pedantic
    # Mocked Arduino functions are unused in some test files.
    -Wno-unused-function
)

# ---------------------------------------------------------------------------
# Link
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)
target_link_libraries(phantomrf-tests PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# CTest registration
# ---------------------------------------------------------------------------

enable_testing()
add_test(
    NAME phantomrf-tests
    COMMAND phantomrf-tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Surface Unity's own exit code straight through to CTest.
set_tests_properties(phantomrf-tests PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAIL|ERROR"
    PASS_REGULAR_EXPRESSION "OK|Tests Passed"
    TIMEOUT 60
)
