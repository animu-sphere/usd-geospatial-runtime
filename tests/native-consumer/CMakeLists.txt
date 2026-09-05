cmake_minimum_required(VERSION 3.23)
project(GeospatialSdkConsumer LANGUAGES CXX)
enable_testing()

# Both consumers resolve their packages the way an external project does, from
# CMAKE_PREFIX_PATH: the composed runtime prefix supplies usdAssetIo and pxr,
# and the SDK install prefix supplies usdGeospatial. Nothing here refers to a
# build tree.
find_package(usdAssetIo CONFIG REQUIRED)
add_executable(geospatial-sdk-consumer main.cpp)
target_link_libraries(geospatial-sdk-consumer PRIVATE usdasset::io)
target_compile_features(geospatial-sdk-consumer PRIVATE cxx_std_17)
add_test(NAME assetIo_link COMMAND geospatial-sdk-consumer)

find_package(usdGeospatial CONFIG REQUIRED)
add_executable(geospatial-runtime-consumer sdk_consumer.cpp)
target_link_libraries(geospatial-runtime-consumer PRIVATE usdgeospatial::sdk)
target_compile_features(geospatial-runtime-consumer PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(geospatial-runtime-consumer PRIVATE /utf-8 /EHsc)
endif()

# The consumer opens a fixture, so it needs the runtime prefix and a fixture
# path. pxr_DIR is the prefix root for a composed runtime; see
# sdk/usd/tests/CMakeLists.txt for the same derivation.
set(_consumer_prefix "${pxr_DIR}")
set(_consumer_fixture "${CMAKE_CURRENT_SOURCE_DIR}/../../fixtures/three-points.copc")
if(EXISTS "${_consumer_prefix}/metadata/composition.lock.json" AND EXISTS "${_consumer_fixture}")
    add_test(NAME sdk_consumer
             COMMAND geospatial-runtime-consumer "${_consumer_prefix}" "${_consumer_fixture}")
else()
    message(STATUS
        "native-consumer: the SDK consumer test is not registered; "
        "pxr_DIR is not a composed runtime prefix or the fixture is missing")
endif()
