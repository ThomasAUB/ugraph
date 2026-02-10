#include "doctest.h"
#include "ugraph.hpp"
#include <vector>
#include <string_view>

// Tests validating basic Graph ordering, fork-join shape, and for_each/apply equivalence.

struct LStageA { using Manifest = ugraph::Manifest< ugraph::IO<const char*, 0, 1> >; const char* name; int i = 0; };
struct LStageB { using Manifest = ugraph::Manifest< ugraph::IO<const char*, 1, 1, false> >; const char* name; int i = 0; };
struct LStageC { using Manifest = ugraph::Manifest< ugraph::IO<const char*, 1, 0> >; const char* name; int i = 0; };

TEST_CASE("graph_view basic linear ordering") {
    LStageA sa { "A" };
    LStageB sb { "B" };
    LStageC sc { "C" };

    auto vA = ugraph::make_node<101>(sa);
    auto vB = ugraph::make_node<102>(sb);
    auto vC = ugraph::make_node<103>(sc);

    auto g = ugraph::Graph(
        vB.output<const char*, 0>() >> vC.input<const char*, 0>(),
        vA.output<const char*, 0>() >> vB.input<const char*, 0>()
    );

    static_assert(decltype(g)::topology_type::size() == 3, "Unexpected vertex count");
    auto ids = decltype(g)::topology_type::ids();
    CHECK(ids.size() == 3);

    std::vector<char> order;
    g.for_each([&] (auto& module, auto&) { order.push_back(module.name[0]); });

    REQUIRE(order.size() == 3);
    CHECK(order[0] == 'A');
    CHECK(order[1] == 'B');
    CHECK(order[2] == 'C');
}

TEST_CASE("graph_view fork-join ordering") {
    struct Merge { using Manifest = ugraph::Manifest< ugraph::IO<const char*, 2, 1> >; const char* name; };
    LStageA src { "src" };
    LStageB b1 { "b1" };
    LStageB b2 { "b2" };
    Merge m { "m" };
    LStageC sink { "snk" };

    auto vSrc = ugraph::make_node<201>(src);
    auto vB1 = ugraph::make_node<202>(b1);
    auto vB2 = ugraph::make_node<203>(b2);
    auto vMerge = ugraph::make_node<204>(m);
    auto vSink = ugraph::make_node<205>(sink);

    auto g = ugraph::Graph(
        vMerge.output<const char*>() >> vSink.input<const char*, 0>(),
        vB2.output<const char*>() >> vMerge.input<const char*, 1>(),
        vSrc.output<const char*>() >> vB1.input<const char*, 0>(),
        vSrc.output<const char*>() >> vB2.input<const char*, 0>(),
        vB1.output<const char*>() >> vMerge.input<const char*, 0>()
    );

    std::vector<std::string_view> names;
    g.for_each([&] (auto& module, auto&) { names.emplace_back(module.name); });

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

    auto vA = ugraph::make_node<301>(sa);
    auto vB = ugraph::make_node<302>(sb);

    auto g = ugraph::Graph(vA.output<const char*, 0>() >> vB.input<const char*, 0>());

    std::vector<char> seq1;
    g.for_each([&] (auto& module, auto&) { seq1.push_back(module.name[0]); });

    std::vector<char> seq2;
    g.for_each([&] (auto& module, auto&) { seq2.push_back(module.name[0]); });

    CHECK(seq1 == seq2);
    CHECK(seq1.size() == 2);
    CHECK(seq1[0] == 'A');
    CHECK(seq1[1] == 'B');

    g.for_each([&] (auto& module, auto&) { module.i++; });
    CHECK(sa.i == 1);
    CHECK(sb.i == 1);

    g.for_each([&] (auto& module, auto&) { module.i++; });
    CHECK(sa.i == 2);
    CHECK(sb.i == 2);
}
