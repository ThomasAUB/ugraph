#include "doctest.h"
#include "ugraph.hpp"
#include <vector>

namespace {

    struct AddStage {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        const char* name;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>() + 1;
        }
    };

}

TEST_CASE("graph can contain nested graph nodes and flatten execution") {

    AddStage innerA { "innerA" };
    AddStage innerB { "innerB" };

    auto innerNodeA = ugraph::make_node<210>(innerA);
    auto innerNodeB = ugraph::make_node<211>(innerB);

    auto innerGraph = ugraph::Graph(
        innerNodeA.output<int>() >> innerNodeB.input<int>()
    );

    AddStage outerSource { "outerSource" };
    AddStage outerSink { "outerSink" };

    auto outerSourceNode = ugraph::make_node<110>(outerSource);
    auto outerNestedNode = ugraph::make_node<111>(innerGraph);
    auto outerSinkNode = ugraph::make_node<112>(outerSink);

    auto graph = ugraph::Graph(
        outerSourceNode.output<int>() >> outerNestedNode.input<int>(),
        outerNestedNode.output<int>() >> outerSinkNode.input<int>()
    );

    decltype(graph)::graph_data_t graph_data;
    graph.init_graph_data(graph_data);

    int inputValue = 3;
    int outputValue = 0;

    graph.bind_input<110>(inputValue);
    graph.bind_output<112>(outputValue);

    CHECK(graph.all_ios_connected());

    std::vector<AddStage*> orderedModules;
    graph.for_each([&] (auto& module, auto& ctx) {
        module.process(ctx);
        orderedModules.push_back(&module);
    });

    CHECK(outputValue == 7);
    REQUIRE(orderedModules.size() == 4);
    CHECK(orderedModules[0] == &outerSource);
    CHECK(orderedModules[1] == &innerA);
    CHECK(orderedModules[2] == &innerB);
    CHECK(orderedModules[3] == &outerSink);

    constexpr auto ids = decltype(graph)::topology_type::ids();
    CHECK(ids.size() == 4);
    CHECK(ids[0] == 110);
    CHECK(ids[1] == 321);
    CHECK(ids[2] == 322);
    CHECK(ids[3] == 112);
}
