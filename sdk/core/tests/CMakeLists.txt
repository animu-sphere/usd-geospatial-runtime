add_executable(usdGeospatialCore_tests test_core.cpp)
target_link_libraries(usdGeospatialCore_tests PRIVATE usdgeospatial::core)
# The JSON reader is private to the implementation, but the filter and the
# strictness of its scanner are behavior the SDK depends on, so the tests reach
# the header directly rather than only through runtime_info.
target_include_directories(usdGeospatialCore_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
if(MSVC)
    target_compile_options(usdGeospatialCore_tests PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME usdGeospatialCore_unit
         COMMAND usdGeospatialCore_tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
