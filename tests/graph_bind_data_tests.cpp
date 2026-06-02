#include "doctest.h"
#include "ugraph.hpp"

struct Add {

    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 1, 1>
    >;

    void process(ugraph::Context<Manifest>& ctx) {
        ctx.output<int>() = ctx.input<int>() + 1;
    }

};

struct OptionalOutput {

    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 1, 1, false>
    >;

    void process(ugraph::Context<Manifest>& ctx) {
        mInternal = ctx.input<int>();
        const bool isConnected = ctx.has_output<int>();
        CHECK(!isConnected);
        if (isConnected) {
            ctx.output<int>() = mInternal + 1;
        }
    }

    int mInternal = 0;
};

TEST_CASE("bind_graph_data external") {

    Add entry1, entry2, end1, end2;

    auto entry1Node = ugraph::make_node<100>(entry1);
    auto entry2Node = ugraph::make_node<103>(entry2);
    auto end1Node = ugraph::make_node<101>(end1);
    auto end2Node = ugraph::make_node<102>(end2);

    int a;
    int b;

    ugraph::Graph graph(

        a | entry1Node.input<int>()
        , a | entry2Node.input<int>()
        , entry1Node.output<int>() >> end1Node.input<int>()
        , entry2Node.output<int>() >> end2Node.input<int>()
        , end1Node.output<int>() | b
        , end2Node.output<int>() | a

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

TEST_CASE("strict graph accepts fully bound external data") {

    Add entry;
    OptionalOutput exit;

    auto entryNode = ugraph::make_node<200>(entry);
    auto exitNode = ugraph::make_node<201>(exit);

    int in = 5;
    int out = 0;

    ugraph::Graph graph(
        in | entryNode.input<int>()
        , entryNode.output<int>() >> exitNode.input<int>()
        //, exitNode.output<int>() | out /* ommit one connection */
    );

    graph.for_each(
        [&] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(out == 0);
    CHECK(exit.mInternal == 6);
}