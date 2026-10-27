# The simulation core: a standalone static library, a CLI harness, and tests.

add_library(owlight_sim STATIC
  src/Properties.cpp
  src/Types.cpp
  src/Journal.cpp
  src/Catalog.cpp
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
add_test(NAME owlight_tests COMMAND owlight_tests)
