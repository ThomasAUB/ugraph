set(UGRAPH_UNIT_TESTS ugraph_tests)

include(CTest)

file(GLOB TARGET_SRC "./*.cpp")

add_executable(${UGRAPH_UNIT_TESTS} ${TARGET_SRC})

add_test(${UGRAPH_UNIT_TESTS} ${UGRAPH_UNIT_TESTS})