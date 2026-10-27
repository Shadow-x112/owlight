# The simulation core: a standalone static library, a CLI harness, and tests.

add_library(owlight_sim STATIC
  src/Properties.cpp
  src/Types.cpp
  src/Journal.cpp
  src/Catalog.cpp
  src/CatalogLoad.cpp
  src/Json.cpp
  src/Scenario.cpp
  src/Slime.cpp
)
target_include_directories(owlight_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
  OWLIGHT_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
  OWLIGHT_SCENARIO_DIR="${PROJECT_SOURCE_DIR}/scenarios"
  OWLIGHT_TEST_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/test_scratch")
add_test(NAME owlight_tests COMMAND owlight_tests)

# Every scenario file is also a test: its `expect` actions must all hold (exit code 0).
file(GLOB OWLIGHT_SCENARIOS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/scenarios/*.json)
foreach(scenario ${OWLIGHT_SCENARIOS})
  get_filename_component(scenario_name ${scenario} NAME_WE)
  add_test(NAME scenario_${scenario_name} COMMAND owlight_sim_cli --scenario ${scenario} --quiet)
  add_test(NAME scenario_${scenario_name}_on_data
           COMMAND owlight_sim_cli --data ${PROJECT_SOURCE_DIR}/data --scenario ${scenario} --quiet)
endforeach()
