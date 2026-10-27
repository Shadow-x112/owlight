# The simulation core: a standalone static library, a CLI harness, and tests.

# Embed data/*.json so Catalog::v1() needs no files on disk. The JSON is the only copy of the
# numbers; this regenerates whenever a data file changes (CMAKE_CONFIGURE_DEPENDS re-runs
# configure, and configure_file tracks the template).
set(OWLIGHT_DATA_DIR "${PROJECT_SOURCE_DIR}/data")
foreach(name foods habitats jobs types)
  set(_file "${OWLIGHT_DATA_DIR}/${name}.json")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
  file(READ "${_file}" _text)
  string(TOUPPER "${name}" _upper)
  set(OWLIGHT_${_upper}_JSON "${_text}")
endforeach()
configure_file(src/EmbeddedData.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/EmbeddedData.cpp" @ONLY)

add_library(owlight_sim STATIC
  src/Properties.cpp
  src/Types.cpp
  src/Journal.cpp
  src/Catalog.cpp
  src/CatalogLoad.cpp
  src/Json.cpp
  src/Slime.cpp
  src/Scenario.cpp
  src/SlimeSave.cpp
  src/owlight_c.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/EmbeddedData.cpp"
)
target_include_directories(owlight_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(owlight_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
  target_compile_options(owlight_sim PRIVATE /W4 /permissive-)
else()
  target_compile_options(owlight_sim PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(owlight_sim_cli tools/owlight_sim.cpp)
target_link_libraries(owlight_sim_cli PRIVATE owlight_sim)
set_target_properties(owlight_sim_cli PROPERTIES OUTPUT_NAME owlight_sim)

add_executable(owlight_tests tests/test_main.cpp)
target_link_libraries(owlight_tests PRIVATE owlight_sim)
target_compile_definitions(owlight_tests PRIVATE
  OWLIGHT_DATA_DIR="${OWLIGHT_DATA_DIR}"
  OWLIGHT_SCENARIO_DIR="${PROJECT_SOURCE_DIR}/scenarios"
  OWLIGHT_TEST_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/test_scratch")
add_test(NAME owlight_tests COMMAND owlight_tests)

# The C interface, compiled as plain C so the header is proven to be real C.
add_executable(owlight_c_api_smoke tests/c_api_smoke.c)
target_link_libraries(owlight_c_api_smoke PRIVATE owlight_sim)
if(NOT MSVC)
  target_compile_options(owlight_c_api_smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME owlight_c_api_smoke COMMAND owlight_c_api_smoke)
