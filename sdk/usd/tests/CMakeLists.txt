add_executable(usdGeospatial_tests test_open.cpp)
target_link_libraries(usdGeospatial_tests PRIVATE usdgeospatial::sdk)
if(MSVC)
    target_compile_options(usdGeospatial_tests PRIVATE /utf-8 /EHsc)
endif()

# find_package(pxr) sets pxr_DIR to the directory holding pxrConfig.cmake,
# which for a composed runtime is the prefix root. That is where runtime_info
# looks for metadata/composition.lock.json, so the test needs no separate
# configuration to know which runtime it is testing.
set(_usdgeospatial_prefix "${pxr_DIR}")
set(_usdgeospatial_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/../../../fixtures")
set(_usdgeospatial_fixture "${_usdgeospatial_fixtures}/three-points.copc")
# A malformed asset of a composed format, which is the only way to reach
# stage_open_failed: every check open() makes before it has to pass first.
set(_usdgeospatial_malformed "${_usdgeospatial_fixtures}/malformed.usda")

if(EXISTS "${_usdgeospatial_prefix}/metadata/composition.lock.json" AND
   EXISTS "${_usdgeospatial_fixture}" AND
   EXISTS "${_usdgeospatial_malformed}")
    # The test loads FileFormat plugins and resolvers, so it must run with the
    # composition's environment applied: `ost runtime exec --composition <prefix>
    # -- ctest ...`. Running it outside that environment fails on plugin
    # discovery, not on a contract.
    add_test(NAME usdGeospatial_open
             COMMAND usdGeospatial_tests "${_usdgeospatial_prefix}" "${_usdgeospatial_fixture}"
                     "${_usdgeospatial_malformed}")
else()
    message(STATUS
        "usdGeospatial: open() and formats() tests are not registered; "
        "pxr_DIR is not a composed runtime prefix or a fixture is missing")
endif()
