set(CMAKE_BUILD_TYPE Debug)

include(CMakePrintHelpers)

cmake_print_variables(CMAKE_SOURCE_DIR)
cmake_print_variables(PROJECT_SOURCE_DIR)

find_library(MATH_LIBRARY m)

add_executable(azo_test
    unity/unity.c
    test.c
    test_tokenizer.c
    test_parser.c
)

target_compile_definitions(azo_test PRIVATE UNITY_INCLUDE_DOUBLE)
target_include_directories(azo_test PRIVATE ${CMAKE_SOURCE_DIR} ..)

# The arikkei and az targets (drop-in, added by the top-level CMakeLists) resolves the
# library location, name (libarikkei/libarikkeid. libaz/libazd) and per-config subdirectory
target_link_libraries(azo_test PRIVATE az azo arikkei)

if(MATH_LIBRARY)
    target_link_libraries(azo_test PRIVATE ${MATH_LIBRARY})
endif()

add_test(NAME compile COMMAND azo_test compile)
add_test(NAME assign COMMAND azo_test assign)
add_test(NAME comparison COMMAND azo_test comparison)
add_test(NAME function COMMAND azo_test function)
add_test(NAME tokenizer COMMAND azo_test tokenizer)
add_test(NAME parser COMMAND azo_test parser)
