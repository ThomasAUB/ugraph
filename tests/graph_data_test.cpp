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

    void process(ugraph::NodeContext<Manifest>& ctx) {
        ctx.output<MyData1>() = ctx.input<MyData1>() + 1;
        std::cout << "module : " << ctx.output<MyData1>() << std::endl;
    }

};

struct Source {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 0, 1>,
        ugraph::IO<MyEvent, 0, 1>
    >;

    void process(ugraph::NodeContext<Manifest>& ctx) {
        ctx.output<MyData1>() = 1;
        std::cout << "source 0 : " << ctx.output<MyData1>() << std::endl;
        ctx.output<MyEvent>().push_back(789);
    }

};

struct Sink {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 2, 0>,
        ugraph::IO<MyEvent, 1, 0>
    >;

    void process(ugraph::NodeContext<Manifest>& ctx) {

        int idx = 0;
        for (auto& in : ctx.inputs<MyData1>()) {
            std::cout << "sink " << idx++ << " : " << in << std::endl;
        }

        auto& vect = ctx.input<MyEvent>();

        if (vect.empty()) {
            std::cout << "event empty" << std::endl;
        }
        else {
            std::cout << "received event " << vect.back() << std::endl;
        }

    }

};

TEST_CASE("type name test") {

    Source src;
    Module1 m1;
    Sink sink;

    auto srcNode = ugraph::make_node<100>(src);
    auto m1Node = ugraph::make_node<101>(m1);
    auto sinkNode = ugraph::make_node<102>(sink);

    ugraph::Graph graph(
        srcNode.output<MyData1, 0>() >> m1Node.input<MyData1, 0>(),
        m1Node.output<MyData1, 0>() >> sinkNode.input<MyData1, 0>(),
        srcNode.output<MyData1, 0>() >> sinkNode.input<MyData1, 1>(),
        srcNode.output<MyEvent, 0>() >> sinkNode.input<MyEvent, 0>()
    );

    graph.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );



}

TEST_CASE("graph print output") {

    Source src;
    Module1 m1;
    Sink sink;

    auto srcNode = ugraph::make_node<100>(src);
    auto m1Node = ugraph::make_node<101>(m1);
    auto sinkNode = ugraph::make_node<102>(sink);

    ugraph::Graph graph(
        srcNode.output<MyData1, 0>() >> m1Node.input<MyData1, 0>(),
        m1Node.output<MyData1, 0>() >> sinkNode.input<MyData1, 0>(),
        srcNode.output<MyData1, 0>() >> sinkNode.input<MyData1, 1>(),
        srcNode.output<MyEvent, 0>() >> sinkNode.input<MyEvent, 0>()
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