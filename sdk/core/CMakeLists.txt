cmake_minimum_required(VERSION 3.23)

# The OpenUSD-free half of the SDK: the failure vocabulary, the result type,
# introspection over a materialized prefix, and the rule that decides what a
# runtime supports. It links nothing, so a consumer can report which runtime it
# found -- or why it found none -- before any OpenUSD library is loaded, and CI
# can check those contracts on a runner with no composed runtime.
add_library(usdGeospatialCore STATIC
    src/diagnostics.cpp
    src/format_support.cpp
    src/json.cpp
    src/runtime_info.cpp)
add_library(usdgeospatial::core ALIAS usdGeospatialCore)
set_target_properties(usdGeospatialCore PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdGeospatialCore PUBLIC cxx_std_17)
target_include_directories(usdGeospatialCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
    # /utf-8: the sources are UTF-8 and MSVC otherwise reads them in the host
    #   code page, which mangles a non-ASCII literal differently per host.
    # /EHsc: Result throws std::logic_error on misuse and the containers here
    #   allocate, so unwinding must be generated rather than assumed.
    target_compile_options(usdGeospatialCore PRIVATE /utf-8 /EHsc)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS usdGeospatialCore
    EXPORT usdGeospatialCoreTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

set(_usdGeospatialCore_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/usdGeospatialCore")

install(EXPORT usdGeospatialCoreTargets
    FILE usdGeospatialCoreTargets.cmake
    NAMESPACE usdgeospatial::
    DESTINATION "${_usdGeospatialCore_cmakedir}")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/usdGeospatialCoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialCoreConfig.cmake"
    INSTALL_DESTINATION "${_usdGeospatialCore_cmakedir}")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialCoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialCoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialCoreConfigVersion.cmake"
    DESTINATION "${_usdGeospatialCore_cmakedir}")

if(USDGEOSPATIAL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
