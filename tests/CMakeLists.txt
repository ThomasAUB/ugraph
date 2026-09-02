set(UGRAPH_UNIT_TESTS ugraph_tests)

include(CTest)

set(TARGET_SRC

    test_main.cpp

    topology_priority_tests.cpp
    topology_compile_time_tests.cpp

    graph_ordering_tests.cpp
    graph_printer_tests.cpp
    graph_data_count_tests.cpp
    graph_data_test.cpp
    graph_bind_data_tests.cpp
    manual_bind_tests.cpp
    audio_graph_tests.cpp
    manifest_tagged_io_tests.cpp
    graph_tagged_io_tests.cpp
    graph_io_tests.cpp

    compile_time_graph_tests.cpp

    example_test.cpp
)

add_executable(${UGRAPH_UNIT_TESTS} ${TARGET_SRC})
target_link_libraries(${UGRAPH_UNIT_TESTS} PRIVATE ugraph)

# /Ob3: raise the inline expansion level beyond the /Ob2 that /O2 implies.
# Without this, MSVC's inline budget is exhausted before it collapses the
# template chain in for_each_impl + Context::slot_ptrs, leaving measurable
# per-block function-call overhead that inflates the pipeline/manual ratio.
if(MSVC)
    target_compile_options(${UGRAPH_UNIT_TESTS} PRIVATE /Ob3)
endif()

add_test(${UGRAPH_UNIT_TESTS} ${UGRAPH_UNIT_TESTS})