set(UGRAPH_UNIT_TESTS ugraph_tests)

include(CTest)

set(TARGET_SRC

    test_main.cpp

    topology_priority_tests.cpp
    topology_compile_time_tests.cpp
    topology_nested_tests.cpp

    graph_ordering_tests.cpp
    graph_printer_tests.cpp
    graph_data_count_tests.cpp
    manual_bind_tests.cpp
    audio_graph_tests.cpp

    compile_time_graph_tests.cpp

)

add_executable(${UGRAPH_UNIT_TESTS} ${TARGET_SRC})
target_link_libraries(${UGRAPH_UNIT_TESTS} PRIVATE ugraph)

add_test(${UGRAPH_UNIT_TESTS} ${UGRAPH_UNIT_TESTS})