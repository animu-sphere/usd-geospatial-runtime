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
set(_usdgeospatial_fixture "${CMAKE_CURRENT_SOURCE_DIR}/../../../fixtures/three-points.copc")

if(EXISTS "${_usdgeospatial_prefix}/metadata/composition.lock.json" AND
   EXISTS "${_usdgeospatial_fixture}")
    # The test loads FileFormat plugins and resolvers, so it must run with the
    # composition's environment applied: `ost runtime exec --composition <prefix>
    # -- ctest ...`. Running it outside that environment fails on plugin
    # discovery, not on a contract.
    add_test(NAME usdGeospatial_open
             COMMAND usdGeospatial_tests "${_usdgeospatial_prefix}" "${_usdgeospatial_fixture}")
else()
    message(STATUS
        "usdGeospatial: open() tests are not registered; "
        "pxr_DIR is not a composed runtime prefix or the fixture is missing")
endif()
