set(UGRAPH_UNIT_TESTS ugraph_tests)

include(CTest)

file(GLOB TARGET_SRC "./*.cpp")

add_executable(${UGRAPH_UNIT_TESTS} ${TARGET_SRC})
target_link_libraries(${UGRAPH_UNIT_TESTS} PRIVATE ugraph)

add_test(${UGRAPH_UNIT_TESTS} ${UGRAPH_UNIT_TESTS})