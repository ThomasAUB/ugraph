#include "doctest.h"
#include "ugraph.hpp"
#include <sstream>
#include <string>
#include <iostream>
#include <vector>

using input_data_t = int;

struct Add {

    using Manifest = ugraph::Manifest<
        ugraph::IO<input_data_t, 1, 1, false>
    >;

    void process(ugraph::Context<Manifest>& ctx) {
        ctx.output<input_data_t>() = ctx.input<input_data_t>() + 1;
    }

};

TEST_CASE("manual bind graph") {

    Add addEntry;
    Add addMiddle;
    Add addOut1;
    Add addOut2;

    auto entryNode = ugraph::make_node<100>(addEntry);
    auto middleNode = ugraph::make_node<101>(addMiddle);
    auto outputNode1 = ugraph::make_node<102>(addOut1);
    auto outputNode2 = ugraph::make_node<103>(addOut2);


    ugraph::Graph graph(
        entryNode.output<input_data_t>() >> middleNode.input<input_data_t>(),
        middleNode.output<input_data_t>() >> outputNode1.input<input_data_t>(),
        entryNode.output<input_data_t>() >> outputNode2.input<input_data_t>()
    );

    decltype(graph)::graph_data_t dg;
    graph.init_graph_data(dg);

    CHECK(!graph.all_ios_connected());

    input_data_t entry = 0;
    input_data_t output1 = 0;
    input_data_t output2 = 0;

    graph.bind_input<100>(entry);

    CHECK(!graph.all_ios_connected());

    graph.bind_output<102>(output1);

    CHECK(!graph.all_ios_connected());

    graph.bind_output<103>(output2);

    CHECK(graph.all_ios_connected());

    graph.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );

    CHECK(output1 == 3);
    CHECK(output2 == 2);

    entry = 4;

    graph.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );

    CHECK(output1 == 7);
    CHECK(output2 == 6);
}

TEST_CASE("manual bind node") {

    Add addEntry;
    Add addMiddle;
    Add addOut;

    ugraph::Context<Add::Manifest> ctx[3];
    input_data_t data[2] {};

    ctx[0].set_ios(std::array { &data[0], &data[1] });
    ctx[1].set_ios(std::array { &data[1], &data[0] });
    ctx[2].set_ios(std::array { &data[0], &data[1] });

    auto run = [&] () {
        addEntry.process(ctx[0]);
        addMiddle.process(ctx[1]);
        addOut.process(ctx[2]);
        };

    run();
    CHECK(data[1] == 3);
    data[0] = 7;
    run();
    CHECK(data[1] == 10);
}