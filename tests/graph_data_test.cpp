#include "doctest.h"
#include "ugraph.hpp"
#include <sstream>
#include <string>
#include <iostream>
#include <vector>

using MyData1 = int;

using MyEvent = std::vector<int>;

struct Module1 {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 1, 1>
    >;

    int last_in = 0;
    int last_out = 0;

    void process(ugraph::Context<Manifest>& ctx) {
        last_in = ctx.input<MyData1>();
        last_out = last_in + 1;
        ctx.output<MyData1>() = last_out;
    }

};

struct Source {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 0, 1>,
        ugraph::IO<MyEvent, 0, 1>
    >;

    int out_value = 1;
    int event_value = 789;

    void process(ugraph::Context<Manifest>& ctx) {
        ctx.output<MyData1>() = out_value;
        ctx.output<MyEvent>().push_back(event_value);
    }

};

struct Sink {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 2, 0>,
        ugraph::IO<MyEvent, 1, 0>
    >;

    std::vector<int> inputs;
    bool event_seen = false;
    int event_value = -1;

    void process(ugraph::Context<Manifest>& ctx) {

        inputs.clear();
        for (auto& in : ctx.inputs<MyData1>()) {
            inputs.push_back(in);
        }

        auto& vect = ctx.input<MyEvent>();
        event_seen = !vect.empty();
        event_value = event_seen ? vect.back() : -1;

    }

};

TEST_CASE("graph data propagation") {

    Source src;
    Module1 m1;
    Sink sink;

    auto srcNode = ugraph::make_node<100>(src);
    auto m1Node = ugraph::make_node<101>(m1);
    auto sinkNode = ugraph::make_node<102>(sink);

    ugraph::Graph graph(
        srcNode.output<MyData1>() >> m1Node.input<MyData1>(),
        m1Node.output<MyData1>() >> sinkNode.input<MyData1, 0>(),
        srcNode.output<MyData1>() >> sinkNode.input<MyData1, 1>(),
        srcNode.output<MyEvent>() >> sinkNode.input<MyEvent, 0>()
    );

    graph.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );

    CHECK(graph.data_count<MyData1>() == 2);
    CHECK(graph.data_count<MyEvent>() == 1);
    CHECK(m1.last_in == 1);
    CHECK(m1.last_out == 2);
    REQUIRE(sink.inputs.size() == 2);
    CHECK(sink.inputs[0] == 2);
    CHECK(sink.inputs[1] == 1);
    CHECK(sink.event_seen);
    CHECK(sink.event_value == 789);

}

TEST_CASE("graph print output") {

    Source src;
    Module1 m1;
    Sink sink;

    auto srcNode = ugraph::make_node<100>(src);
    auto m1Node = ugraph::make_node<101>(m1);
    auto sinkNode = ugraph::make_node<102>(sink);

    ugraph::Graph graph(
        srcNode.output<MyData1>() >> m1Node.input<MyData1>(),
        m1Node.output<MyData1>() >> sinkNode.input<MyData1, 0>(),
        srcNode.output<MyData1>() >> sinkNode.input<MyData1, 1>(),
        srcNode.output<MyEvent>() >> sinkNode.input<MyEvent>()
    );

    std::ostringstream oss;
    graph.print(oss);

    const std::string expected =
        "```mermaid\n"
        "flowchart LR\n"
        "100(Source 100)\n"
        "101(Module1 101)\n"
        "102(Sink 102)\n"
        "100 --> 101\n"
        "101 --> 102\n"
        "100 --> 102\n"
        "100 --> 102\n"
        "```\n";

    CHECK(oss.str() == expected);

}