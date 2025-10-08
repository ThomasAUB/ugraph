#include "doctest.h"
#include "ugraph.hpp"
#include <vector>
#include <string_view>

// Tests validating basic GraphView ordering, fork-join shape, and for_each/apply equivalence.

struct LStageA { const char* name; int i = 0; };
struct LStageB { const char* name; int i = 0; };
struct LStageC { const char* name; int i = 0; };

TEST_CASE("graph_view basic linear ordering") {
    LStageA sa { "A" };
    LStageB sb { "B" };
    LStageC sc { "C" };

    ugraph::Node<101, LStageA, 0, 1> vA(sa);
    ugraph::Node<102, LStageB, 1, 1> vB(sb);
    ugraph::Node<103, LStageC, 1, 0> vC(sc);

    auto g = ugraph::GraphView(
        vB.out() >> vC.in(),
        vA.out() >> vB.in()
    );

    static_assert(decltype(g)::size() == 3, "Unexpected vertex count");
    auto ids = decltype(g)::ids();
    CHECK(ids.size() == 3);

    std::vector<char> order;
    g.apply([&] (auto&... impls) { (order.push_back(impls.module().name[0]), ...); });

    REQUIRE(order.size() == 3);
    CHECK(order[0] == 'A');
    CHECK(order[1] == 'B');
    CHECK(order[2] == 'C');
}

TEST_CASE("graph_view fork-join ordering") {
    struct Merge { const char* name; };
    LStageA src { "src" };
    LStageB b1 { "b1" };
    LStageB b2 { "b2" };
    Merge m { "m" };
    LStageC sink { "snk" };

    ugraph::Node<201, LStageA, 0, 2> vSrc(src);
    ugraph::Node<202, LStageB, 1, 1> vB1(b1);
    ugraph::Node<203, LStageB, 1, 1> vB2(b2);
    ugraph::Node<204, Merge, 2, 1> vMerge(m);
    ugraph::Node<205, LStageC, 1, 0> vSink(sink);

    auto g = ugraph::GraphView(
        vSrc.out<0>() >> vB1.in(),
        vSrc.out<1>() >> vB2.in(),
        vB1.out() >> vMerge.in<0>(),
        vB2.out() >> vMerge.in<1>(),
        vMerge.out() >> vSink.in()
    );

    std::vector<std::string_view> names;
    g.apply([&] (auto&... impls) { (names.emplace_back(impls.module().name), ...); });

    auto find_pos = [&] (std::string_view s) { for (std::size_t i = 0; i < names.size(); ++i) if (names[i] == s) return i; return names.size(); };

    REQUIRE(names.size() == 5);
    CHECK(find_pos("src") < find_pos("b1"));
    CHECK(find_pos("src") < find_pos("b2"));
    CHECK(find_pos("b1") < find_pos("m"));
    CHECK(find_pos("b2") < find_pos("m"));
    CHECK(find_pos("m") < find_pos("snk"));
}

TEST_CASE("graph_view for_each vs apply equivalence and mutation") {
    LStageA sa { "A" };
    LStageB sb { "B" };

    ugraph::Node<301, LStageA, 0, 1> vA(sa);
    ugraph::Node<302, LStageB, 1, 0> vB(sb);

    auto g = ugraph::GraphView(vA.out() >> vB.in());

    std::vector<char> seq1;
    g.for_each([&] (auto& v) { seq1.push_back(v.module().name[0]); });

    std::vector<char> seq2;
    g.apply([&] (auto&... v) { (seq2.push_back(v.module().name[0]), ...); });

    CHECK(seq1 == seq2);
    CHECK(seq1.size() == 2);
    CHECK(seq1[0] == 'A');
    CHECK(seq1[1] == 'B');

    g.apply([&] (auto&... v) { (v.module().i++, ...); });
    CHECK(sa.i == 1);
    CHECK(sb.i == 1);

    g.for_each([&] (auto& v) { v.module().i++; });
    CHECK(sa.i == 2);
    CHECK(sb.i == 2);
}
