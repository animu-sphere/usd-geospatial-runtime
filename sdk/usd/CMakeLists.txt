cmake_minimum_required(VERSION 3.23)

find_package(pxr CONFIG REQUIRED)

# The OpenUSD lane: everything that needs a loaded runtime. It is one
# translation unit because `open` is the only operation whose contract the
# tests currently prove; `inspect` and `formats` join it when theirs do.
add_library(usdGeospatial STATIC src/open.cpp)
add_library(usdgeospatial::sdk ALIAS usdGeospatial)
set_target_properties(usdGeospatial PROPERTIES
    EXPORT_NAME sdk
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdGeospatial PUBLIC cxx_std_17)
target_include_directories(usdGeospatial
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

# The OpenUSD libraries are PUBLIC because open() returns a UsdStageRefPtr: a
# consumer of this target compiles against pxr headers whether or not it names
# OpenUSD itself.
target_link_libraries(usdGeospatial
    PUBLIC
        usdgeospatial::core
        ar
        sdf
        tf
        usd)

if(MSVC)
    # See the note in ../core/CMakeLists.txt.
    target_compile_options(usdGeospatial PRIVATE /utf-8 /EHsc)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS usdGeospatial
    EXPORT usdGeospatialTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

set(_usdGeospatial_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/usdGeospatial")

install(EXPORT usdGeospatialTargets
    FILE usdGeospatialTargets.cmake
    NAMESPACE usdgeospatial::
    DESTINATION "${_usdGeospatial_cmakedir}")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/usdGeospatialConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialConfig.cmake"
    INSTALL_DESTINATION "${_usdGeospatial_cmakedir}")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeospatialConfigVersion.cmake"
    DESTINATION "${_usdGeospatial_cmakedir}")

if(USDGEOSPATIAL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
