project(uloop_unit_test CXX)
set(TARGET uloop)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_source_files_properties(../../${TARGET}.c PROPERTIES LANGUAGE CXX)
add_executable(utest_${TARGET} utest_${TARGET}.cpp ../../${TARGET}.c)
target_link_libraries(utest_${TARGET} CppUTest CppUTestExt)
