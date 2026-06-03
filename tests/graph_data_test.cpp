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
        ugraph::IO<MyData1, 1, 1, false>,
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
        ugraph::IO<MyData1, 3, 1, false>,
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

namespace {

    std::string expected_graph_print_output(bool showVertexIds) {
        std::ostringstream oss;
        oss << "```mermaid\n";
        oss << "flowchart LR\n";

        if (showVertexIds) {
            oss << "100(Source 100)\n";
            oss << "101(Module1 101)\n";
            oss << "102(Sink 102)\n";
        }
        else {
            oss << "100(Source)\n";
            oss << "101(Module1)\n";
            oss << "102(Sink)\n";
        }

        oss << "100 -->|int| 101\n";
        oss << "101 -->|int| 102\n";
        oss << "100 -->|int| 102\n";
        oss << "100 -->|" << ugraph::type_name<MyEvent>() << "| 102\n";
        oss << "```\n";

        return oss.str();
    }

    std::string expected_external_graph_print_output() {
        std::ostringstream oss;
        oss << "```mermaid\n";
        oss << "flowchart LR\n";
        oss << "100(Source 100)\n";
        oss << "101(Module1 101)\n";
        oss << "102(Sink 102)\n";
        oss << "data_0(( ))\n";
        oss << "100 -->|int| 101\n";
        oss << "101 -->|int| 102\n";
        oss << "100 -->|int| 102\n";
        oss << "100 -->|" << ugraph::type_name<MyEvent>() << "| 102\n";
        oss << "data_0 -->|int| 102\n";
        oss << "```\n";

        return oss.str();
    }

}

TEST_CASE("graph data propagation") {

    Source src;
    Module1 m1;
    Sink sink;
    MyData1 md0 = 0;
    MyData1 md_in = 78;
    MyData1 md1 = 0;

    auto srcNode = ugraph::make_node<100>(src);
    auto m1Node = ugraph::make_node<101>(m1);
    auto sinkNode = ugraph::make_node<102>(sink);

    ugraph::Graph graph(
        md0 | srcNode.input<MyData1>(),
        srcNode.output<MyData1>() >> m1Node.input<MyData1>(),
        m1Node.output<MyData1>() >> sinkNode.input<MyData1, 0>(),
        srcNode.output<MyData1>() >> sinkNode.input<MyData1, 1>(),
        md_in | sinkNode.input<MyData1, 2>(),
        srcNode.output<MyEvent>() >> sinkNode.input<MyEvent, 0>(),
        sinkNode.output<MyData1>() | md1
    );

    using graph_t = decltype(graph);

    graph.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );

    CHECK(graph_t::graph_data_t::template count<MyData1>() == 2);
    CHECK(graph_t::graph_data_t::template count<MyEvent>() == 1);
    CHECK(m1.last_in == 1);
    CHECK(m1.last_out == 2);
    REQUIRE(sink.inputs.size() == 3);
    CHECK(sink.inputs[0] == 2);
    CHECK(sink.inputs[1] == 1);
    CHECK(sink.inputs[2] == 78);
    CHECK(sink.event_seen);
    CHECK(sink.event_value == 789);

    std::ostringstream oss;
    graph.print(oss, "", true, true);
    const std::string out = oss.str();

    CHECK(out.find("data_0(( ))") != std::string::npos);
    CHECK(out.find("data_1(( ))") != std::string::npos);
    CHECK(out.find("data_2(( ))") != std::string::npos);
    CHECK(out.find("data_0 -->|int| 100") != std::string::npos);
    CHECK(out.find("data_1 -->|int| 102") != std::string::npos);
    CHECK(out.find("102 -->|int| data_2") != std::string::npos);

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

    using graph_t = decltype(graph);

    std::ostringstream oss;
    graph.print(oss);

    const std::string expected = expected_graph_print_output(false);

    CHECK(oss.str() == expected);
}

TEST_CASE("graph print output with edge types") {

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
    graph.print(oss, "", true);

    const std::string expected = expected_graph_print_output(false);

    CHECK(oss.str() == expected);
}

TEST_CASE("graph print output without vertex ids") {

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
    graph.print(oss, "", true, false);

    const std::string expected = expected_graph_print_output(false);

    CHECK(oss.str() == expected);
}

TEST_CASE("graph print output with explicit vertex ids") {

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
    graph.print(oss, "", true, true);

    const std::string expected = expected_graph_print_output(true);

    CHECK(oss.str() == expected);
}

TEST_CASE("external data graph reuses external graph data") {

    Source srcA;
    Module1 m1A;
    Sink sinkA;
    MyData1 mdInA = 78;

    Source srcB;
    Module1 m1B;
    Sink sinkB;
    MyData1 mdInB = 91;

    auto srcNodeA = ugraph::make_node<100>(srcA);
    auto m1NodeA = ugraph::make_node<101>(m1A);
    auto sinkNodeA = ugraph::make_node<102>(sinkA);

    using shared_graph_t = ugraph::ExternalDataGraph<
        decltype(srcNodeA.output<MyData1>() >> m1NodeA.input<MyData1>()),
        decltype(m1NodeA.output<MyData1>() >> sinkNodeA.input<MyData1, 0>()),
        decltype(srcNodeA.output<MyData1>() >> sinkNodeA.input<MyData1, 1>()),
        decltype(mdInA | sinkNodeA.input<MyData1, 2>()),
        decltype(srcNodeA.output<MyEvent>() >> sinkNodeA.input<MyEvent>())
    >;

    shared_graph_t::graph_data_t sharedData;

    shared_graph_t graphA(
        srcNodeA.output<MyData1>() >> m1NodeA.input<MyData1>(),
        m1NodeA.output<MyData1>() >> sinkNodeA.input<MyData1, 0>(),
        srcNodeA.output<MyData1>() >> sinkNodeA.input<MyData1, 1>(),
        mdInA | sinkNodeA.input<MyData1, 2>(),
        srcNodeA.output<MyEvent>() >> sinkNodeA.input<MyEvent>()
    );
    graphA.init(sharedData);

    auto srcNodeB = ugraph::make_node<100>(srcB);
    auto m1NodeB = ugraph::make_node<101>(m1B);
    auto sinkNodeB = ugraph::make_node<102>(sinkB);

    shared_graph_t graphB(
        srcNodeB.output<MyData1>() >> m1NodeB.input<MyData1>(),
        m1NodeB.output<MyData1>() >> sinkNodeB.input<MyData1, 0>(),
        srcNodeB.output<MyData1>() >> sinkNodeB.input<MyData1, 1>(),
        mdInB | sinkNodeB.input<MyData1, 2>(),
        srcNodeB.output<MyEvent>() >> sinkNodeB.input<MyEvent>()
    );
    graphB.init(sharedData);

    srcA.event_value = 123;
    srcB.event_value = 456;

    graphA.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );

    graphB.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );

    auto& sharedEvents = sharedData.template slot<MyEvent>(0);
    REQUIRE(sharedEvents.size() == 2);
    CHECK(sharedEvents[0] == 123);
    CHECK(sharedEvents[1] == 456);

    std::ostringstream oss;
    graphA.print(oss, "", true, true);

    const std::string expected = expected_external_graph_print_output();

    CHECK(oss.str() == expected);

    //graphA.print(std::cout);
}

TEST_CASE("external data graph feedback graph data") {

    struct Start {
        using Manifest = ugraph::Manifest <
            ugraph::IO<int, 0, 1>
        >;
    };

    struct Recursive {
        using Manifest = ugraph::Manifest <
            ugraph::IO<int, 2, 1>
        >;
    };


    Start s;
    Recursive r;

    auto sNode = ugraph::make_node<100>(s);
    auto rNode = ugraph::make_node<101>(r);

    int feedback;

    ugraph::Graph graph(
        sNode.output<int>() >> rNode.input<int, 0>(),
        rNode.output<int>() | feedback,
        feedback | rNode.input<int, 1>()
    );

    std::ostringstream oss;
    graph.print(oss, "", true, true);

    const std::string expected =
        "```mermaid\n"
        "flowchart LR\n"
        "100(Start 100)\n"
        "101(Recursive 101)\n"
        "data_0(( ))\n"
        "100 -->|int| 101\n"
        "101 -->|int| data_0\n"
        "data_0 -->|int| 101\n"
        "```\n";

    CHECK(oss.str() == expected);

    //graph.print(std::cout);
}