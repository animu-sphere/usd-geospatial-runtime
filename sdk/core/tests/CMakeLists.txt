add_executable(usdGeospatialCore_tests test_core.cpp)
target_link_libraries(usdGeospatialCore_tests PRIVATE usdgeospatial::core)
if(MSVC)
    target_compile_options(usdGeospatialCore_tests PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME usdGeospatialCore_unit
         COMMAND usdGeospatialCore_tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
