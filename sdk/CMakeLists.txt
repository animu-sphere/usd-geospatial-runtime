cmake_minimum_required(VERSION 3.23)

# The SDK carries its own version. It is not the runtime release version in
# VERSION at the repository root: docs/design/spec.md keeps runtime and
# component versions distinct, and this SDK is unreleased while the runtime it
# opens is at v0.1.0.
project(usd-geospatial-sdk
    VERSION 0.1.0
    DESCRIPTION "A small consumer API over the composed USD geospatial runtime"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USDGEOSPATIAL_BUILD_TESTS "Build the SDK tests" ON)
if(USDGEOSPATIAL_BUILD_TESTS)
    enable_testing()
endif()

# The core lane must configure, build, and test with no OpenUSD anywhere on the
# machine. That is what lets CI check the diagnostics, result, and
# introspection contracts on a runner that has no composed runtime, and it is
# why `runtime_info` reads the composition lock rather than asking OpenUSD:
# describing a runtime must not require loading it. The USD lane is enabled
# only when a caller points at an OpenUSD installation.
set(_usdgeospatial_build_usd_default OFF)
if(DEFINED pxr_ROOT OR DEFINED pxr_DIR OR DEFINED ENV{pxr_ROOT})
    set(_usdgeospatial_build_usd_default ON)
endif()
option(USDGEOSPATIAL_BUILD_USD "Build the OpenUSD lane, which provides open()"
       ${_usdgeospatial_build_usd_default})

add_subdirectory(core)
if(USDGEOSPATIAL_BUILD_USD)
    add_subdirectory(usd)
endif()
