#include <iostream>
#include <array>
#include <string_view>

#include "doctest.h"
#include "ugraph.hpp"

struct Proc1 {
    constexpr Proc1(std::string_view inName) : mName(inName) {}
    void identity() { std::cout << "type Proc 1 -> " << mName << std::endl; }
    std::string_view mName;
};

struct Proc2 {
    constexpr Proc2(std::string_view inName) : mName(inName) {}
    void identity() { std::cout << "type Proc 2 -> " << mName << std::endl; }
    std::string_view mName;
};

TEST_CASE("runtime validation") {

    struct StageA { std::string_view name; };
    struct StageB { std::string_view name; };
    struct StageC { std::string_view name; };

    StageA a { "A" };
    StageB b { "B" };
    StageC c { "C" };

    ugraph::RoutingVertex<1, StageA, 0, 1, int> vA(a);
    ugraph::RoutingVertex<2, StageB, 1, 1, int> vB(b);
    ugraph::RoutingVertex<3, StageC, 1, 0, int> vC(c);

    auto g = ugraph::RoutedGraph(
        vB.out() >> vC.in(),
        vA.out() >> vB.in()
    );

    constexpr std::array<char, 3> expected { 'A', 'B', 'C' };
    std::size_t idx = 0;

    g.apply_vertex(

        [&] (auto&... impls) {

            auto validate =
                [&] (auto& impl) {
                CHECK(idx < expected.size());
                CHECK(impl.name[0] == expected[idx++]);
                };

            (validate(impls), ...);
        }
    );

    CHECK(idx == expected.size());
    CHECK(g.buffer_count() == 2);
}

TEST_CASE("basic topological sort test") {

    Proc1 pn1("n1");
    Proc1 pn2("n2");
    Proc2 pn3("n3");
    Proc1 pn4("n4");

    ugraph::RoutingVertex<1, Proc1, 1, 1, int> n1(pn1);
    ugraph::RoutingVertex<2, Proc1, 1, 1, int> n2(pn2);
    ugraph::RoutingVertex<3, Proc2, 1, 1, int> n3(pn3);
    ugraph::RoutingVertex<4, Proc1, 1, 1, int> n4(pn4);

    auto g = ugraph::RoutedGraph(
        n1.out() >> n2.in(),
        n2.out() >> n3.in(),
        n1.out() >> n3.in(),
        n1.out() >> n4.in(),
        n2.out() >> n4.in()
    );

    g.apply_vertex([] (auto&&... user_type) { (user_type.identity(), ...); });
}

TEST_CASE("topological sort verification") {

    Proc1 psource("source");
    Proc2 pmiddle("middle");
    Proc1 psink("sink");

    ugraph::RoutingVertex<10, Proc1, 0, 1, int> source(psource);
    ugraph::RoutingVertex<11, Proc2, 1, 1, int> middle(pmiddle);
    ugraph::RoutingVertex<12, Proc1, 1, 0, int> sink(psink);

    auto g = ugraph::RoutedGraph(
        source.out() >> middle.in(),
        middle.out() >> sink.in()
    );

    auto ids = decltype(g)::ids();
    CHECK(decltype(g)::size() == 3);
    std::size_t pos10 = 0, pos12 = 0;

    for (std::size_t i = 0; i < decltype(g)::size(); ++i) {
        if (ids[i] == 10) pos10 = i;
        if (ids[i] == 12) pos12 = i;
    }

    CHECK(pos10 < pos12);
}

TEST_CASE("complex topological sort test") {

    Proc1 psource("source");
    Proc2 pproc1("proc1");
    Proc1 pproc2("proc2");
    Proc2 pmerger("merger");
    Proc1 psink("sink");

    ugraph::RoutingVertex<20, Proc1, 0, 2, int> source(psource);
    ugraph::RoutingVertex<21, Proc2, 1, 1, int> proc1(pproc1);
    ugraph::RoutingVertex<22, Proc1, 1, 1, int> proc2(pproc2);
    ugraph::RoutingVertex<23, Proc2, 2, 1, int> merger(pmerger);
    ugraph::RoutingVertex<24, Proc1, 1, 0, int> sink(psink);

    auto g = ugraph::RoutedGraph(
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

    std::size_t proc1_pos = 0, proc2_pos = 0, merger_pos = 0;

    for (std::size_t i = 0; i < decltype(g)::size(); ++i) {
        if (ids2[i] == 21) proc1_pos = i; if (ids2[i] == 22) proc2_pos = i; if (ids2[i] == 23) merger_pos = i;
    }

    CHECK(proc1_pos < merger_pos);
    CHECK(proc2_pos < merger_pos);

    g.apply_vertex(
        [] (auto&&... user_type) {
            (user_type.identity(), ...);
        }
    );
}

TEST_CASE("OrderedGraph test") {

    Proc1 pstart("start");
    Proc2 pmiddle("middle");
    Proc1 pend("end");

    ugraph::RoutingVertex<30, Proc1, 0, 1, int> start(pstart);
    ugraph::RoutingVertex<40, Proc2, 1, 1, int> middle(pmiddle);
    ugraph::RoutingVertex<50, Proc1, 1, 0, int> end(pend);

    auto ordered_graph = ugraph::RoutedGraph(
        middle.out() >> end.in(),
        start.out() >> middle.in()
    );

    auto ids3 = decltype(ordered_graph)::ids();

    CHECK(decltype(ordered_graph)::size() == 3);
    CHECK(ids3[0] == 30);
    CHECK(ids3[1] == 40);
    CHECK(ids3[2] == 50);
}
