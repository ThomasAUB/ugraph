#include "doctest.h"
#include "ugraph.hpp"

struct Add {

    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 1, 1, false>
    >;

    void process(ugraph::Context<Manifest>& ctx) {
        ctx.output<int>() = ctx.input<int>() + 1;
    }

};

TEST_CASE("bind_graph_data external") {

    Add addEntry;
    Add addMid;

    auto entryNode = ugraph::make_node<100>(addEntry);
    auto midNode = ugraph::make_node<101>(addMid);

    int a;
    int b;

    ugraph::Graph graph(
        a | entryNode.input<int>(),
        entryNode.output<int>() >> midNode.input<int>(),
        midNode.output<int>() | b
    );

    using graph_t = decltype(graph);
    graph_t::graph_data_t data;

    graph.init_graph_data(data);

    a = 5;
    b = 0;

    graph.for_each(
        [&] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(b == 7);

}