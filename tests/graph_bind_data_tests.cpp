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

    Add add;

    auto entry1Node = ugraph::make_node<100>(add);
    auto end1Node = ugraph::make_node<101>(add);
    auto end2Node = ugraph::make_node<102>(add);
    auto entry2Node = ugraph::make_node<103>(add);

    int a;
    int b;

    ugraph::Graph graph(

        a | entry1Node.input<int>(),

        a | entry2Node.input<int>(),

        entry1Node.output<int>() >> end1Node.input<int>(),
        entry2Node.output<int>() >> end2Node.input<int>(),

        end1Node.output<int>() | b,
        end2Node.output<int>() | a

    );

    a = 5;
    b = 0;

    graph.for_each(
        [&] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(a == 7);
    CHECK(b == 7);

}