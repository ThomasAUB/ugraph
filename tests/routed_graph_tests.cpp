#include <iostream>
#include <vector>
#include <string_view>
#include <cstdint>

#include "doctest.h"
#include "ugraph.hpp"

// -----------------------------------------------------------------------------
// Helper processor types used in routing tests
// -----------------------------------------------------------------------------
struct Proc1 {
    constexpr Proc1(std::string_view n) : mName(n) {}
    std::string_view mName;
};

struct Proc2 {
    constexpr Proc2(std::string_view n) : mName(n) {}
    std::string_view mName;
};

TEST_CASE("runtime validation") {
    struct StageA { std::string_view name; };
    struct StageB { std::string_view name; };
    struct StageC { std::string_view name; };

    StageA a { "A" };
    StageB b { "B" };
    StageC c { "C" };

    ugraph::PipelineVertex<1, StageA, 0, 1, int> vA(a);
    ugraph::PipelineVertex<2, StageB, 1, 1, int> vB(b);
    ugraph::PipelineVertex<3, StageC, 1, 0, int> vC(c);

    auto g = ugraph::PipelineGraph(
        vB.out() >> vC.in(),
        vA.out() >> vB.in()
    );

    std::vector<char> order;

    g.apply(
        [&] (auto&... impls) {
            (order.push_back(impls.get_user_type().name[0]), ...);
        }
    );

    REQUIRE(order.size() == 3);
    CHECK(order[0] == 'A');
    CHECK(order[1] == 'B');
    CHECK(order[2] == 'C');
    CHECK(g.buffer_count() == 2);
}

TEST_CASE("basic topological sort test") {
    Proc1 pn1("n1");
    Proc1 pn2("n2");
    Proc1 pn4("n4");
    Proc2 pn3("n3");

    ugraph::PipelineVertex<1, Proc1, 1, 1, int> n1(pn1);
    ugraph::PipelineVertex<2, Proc1, 1, 1, int> n2(pn2);
    ugraph::PipelineVertex<3, Proc2, 1, 1, int> n3(pn3);
    ugraph::PipelineVertex<4, Proc1, 1, 1, int> n4(pn4);

    auto g = ugraph::PipelineGraph(
        n1.out() >> n2.in(),
        n2.out() >> n3.in(),
        n1.out() >> n3.in(),
        n1.out() >> n4.in(),
        n2.out() >> n4.in()
    );

    std::vector<std::string_view> order;
    order.reserve(decltype(g)::size());

    g.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.get_user_type().mName), ...);
        }
    );

    std::size_t i_n1 = 0;
    std::size_t i_n2 = 0;
    std::size_t i_n3 = 0;
    std::size_t i_n4 = 0;

    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == "n1")      i_n1 = i;
        else if (order[i] == "n2") i_n2 = i;
        else if (order[i] == "n3") i_n3 = i;
        else if (order[i] == "n4") i_n4 = i;
    }

    CHECK(i_n1 < i_n2);
    CHECK(i_n2 < i_n3);
    CHECK(i_n2 < i_n4);
    CHECK(i_n1 < i_n3);
    CHECK(i_n1 < i_n4);
}

TEST_CASE("topological sort verification") {
    Proc1 psource("source");
    Proc1 psink("sink");
    Proc2 pmiddle("middle");

    ugraph::PipelineVertex<10, Proc1, 0, 1, int> source(psource);
    ugraph::PipelineVertex<11, Proc2, 1, 1, int> middle(pmiddle);
    ugraph::PipelineVertex<12, Proc1, 1, 0, int> sink(psink);

    auto g = ugraph::PipelineGraph(
        source.out() >> middle.in(),
        middle.out() >> sink.in()
    );

    auto ids = decltype(g)::ids();
    CHECK(decltype(g)::size() == 3);

    std::size_t pos10 = 0;
    std::size_t pos12 = 0;
    for (std::size_t i = 0; i < decltype(g)::size(); ++i) {
        if (ids[i] == 10) pos10 = i;
        if (ids[i] == 12) pos12 = i;
    }
    CHECK(pos10 < pos12);

    std::vector<std::string_view> order;

    g.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.get_user_type().mName), ...);
        }
    );

    CHECK(order.front() == "source");
    CHECK(order.back() == "sink");
}

TEST_CASE("complex topological sort test") {
    Proc1 psource("source");
    Proc1 pproc2("proc2");
    Proc1 psink("sink");
    Proc2 pproc1("proc1");
    Proc2 pmerger("merger");

    ugraph::PipelineVertex<20, Proc1, 0, 2, int> source(psource);
    ugraph::PipelineVertex<21, Proc2, 1, 1, int> proc1(pproc1);
    ugraph::PipelineVertex<22, Proc1, 1, 1, int> proc2(pproc2);
    ugraph::PipelineVertex<23, Proc2, 2, 1, int> merger(pmerger);
    ugraph::PipelineVertex<24, Proc1, 1, 0, int> sink(psink);

    auto g = ugraph::PipelineGraph(
        source.out<0>() >> proc1.in(),
        source.out<1>() >> proc2.in(),
        proc1.out() >> merger.in<0>(),
        proc2.out() >> merger.in<1>(),
        merger.out() >> sink.in()
    );

    auto ids2 = decltype(g)::ids();
    CHECK(decltype(g)::size() == 5);
    CHECK(ids2[0] == 20);
    CHECK(ids2[4] == 24);

    std::vector<std::string_view> order;
    order.reserve(decltype(g)::size());
    g.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.get_user_type().mName), ...);
        }
    );

    auto find_pos =
        [&] (std::string_view n) {
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] == n) return i;
        }
        return order.size();
        };

    auto p_source = find_pos("source");
    auto p_proc1 = find_pos("proc1");
    auto p_proc2 = find_pos("proc2");
    auto p_merger = find_pos("merger");
    auto p_sink = find_pos("sink");

    CHECK(p_source < p_proc1);
    CHECK(p_source < p_proc2);
    CHECK(p_proc1 < p_merger);
    CHECK(p_proc2 < p_merger);
    CHECK(p_merger < p_sink);
}

TEST_CASE("OrderedGraph test") {
    Proc1 pstart("start");
    Proc1 pend("end");
    Proc2 pmiddle("middle");

    ugraph::PipelineVertex<30, Proc1, 0, 1, int> start(pstart);
    ugraph::PipelineVertex<40, Proc2, 1, 1, int> middle(pmiddle);
    ugraph::PipelineVertex<50, Proc1, 1, 0, int> end(pend);

    auto ordered_graph = ugraph::PipelineGraph(
        middle.out() >> end.in(),
        start.out() >> middle.in()
    );

    auto ids = decltype(ordered_graph)::ids();
    CHECK(ids[0] == 30);
    CHECK(ids[1] == 40);
    CHECK(ids[2] == 50);

    std::vector<std::string_view> order;

    ordered_graph.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.get_user_type().mName), ...);
        }
    );

    CHECK(order.size() == 3);
    CHECK(order[0] == "start");
    CHECK(order[1] == "middle");
    CHECK(order[2] == "end");
}