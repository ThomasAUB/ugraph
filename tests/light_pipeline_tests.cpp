#include "doctest.h"
#include "ugraph.hpp"
#include <vector>
#include <string_view>

// Tests for LightPipelineVertex and LightPipelineGraph (no buffers)

struct LStageA { const char* name; int i = 0; };
struct LStageB { const char* name; int i = 0; };
struct LStageC { const char* name; int i = 0; };

TEST_CASE("light pipeline basic ordering") {
    LStageA sa { "A" };
    LStageB sb { "B" };
    LStageC sc { "C" };

    ugraph::LightPipelineVertex<101, LStageA, 0, 1> vA(sa);
    ugraph::LightPipelineVertex<102, LStageB, 1, 1> vB(sb);
    ugraph::LightPipelineVertex<103, LStageC, 1, 0> vC(sc);

    auto lg = ugraph::LightPipelineGraph(
        // vC.out() >> vA.in(),
        vB.out() >> vC.in(),
        vA.out() >> vB.in()
    );

    static_assert(decltype(lg)::size() == 3, "Unexpected vertex count");
    auto ids = decltype(lg)::ids();
    CHECK(ids.size() == 3);

    // Collect order via apply (user impl objects passed in topological order)
    std::vector<char> order;
    lg.apply([&] (auto&... impls) { (order.push_back(impls.get_user_type().name[0]), ...); });

    REQUIRE(order.size() == 3);
    CHECK(order[0] == 'A');
    CHECK(order[1] == 'B');
    CHECK(order[2] == 'C');
}

TEST_CASE("light pipeline fork join ordering") {
    struct Merge { const char* name; };
    LStageA src { "src" };
    LStageB b1 { "b1" };
    LStageB b2 { "b2" };
    Merge m { "m" };
    LStageC sink { "snk" };

    ugraph::LightPipelineVertex<201, LStageA, 0, 2> vSrc(src);
    ugraph::LightPipelineVertex<202, LStageB, 1, 1> vB1(b1);
    ugraph::LightPipelineVertex<203, LStageB, 1, 1> vB2(b2);
    ugraph::LightPipelineVertex<204, Merge, 2, 1> vMerge(m);
    ugraph::LightPipelineVertex<205, LStageC, 1, 0> vSink(sink);

    auto lg = ugraph::LightPipelineGraph(
        vSrc.out<0>() >> vB1.in(),
        vSrc.out<1>() >> vB2.in(),
        vB1.out() >> vMerge.in<0>(),
        vB2.out() >> vMerge.in<1>(),
        vMerge.out() >> vSink.in()
    );

    std::vector<std::string_view> names;
    lg.apply([&] (auto&... impls) { (names.emplace_back(impls.get_user_type().name), ...); });

    auto find_pos = [&] (std::string_view s) {
        for (std::size_t i = 0; i < names.size(); ++i) if (names[i] == s) return i; return names.size(); };

    auto p_src = find_pos("src");
    auto p_b1 = find_pos("b1");
    auto p_b2 = find_pos("b2");
    auto p_m = find_pos("m");
    auto p_snk = find_pos("snk");

    REQUIRE(names.size() == 5);
    CHECK(p_src < p_b1);
    CHECK(p_src < p_b2);
    CHECK(p_b1 < p_m);
    CHECK(p_b2 < p_m);
    CHECK(p_m < p_snk);
}

TEST_CASE("light pipeline for_each execute helper") {
    LStageA sa { "A" };
    LStageB sb { "B" };

    ugraph::LightPipelineVertex<301, LStageA, 0, 1> vA(sa);
    ugraph::LightPipelineVertex<302, LStageB, 1, 0> vB(sb);

    auto lg = ugraph::LightPipelineGraph(
        vA.out() >> vB.in()
    );

    std::vector<char> seq1;
    lg.for_each([&] (auto& v) { seq1.push_back(v.get_user_type().name[0]); });

    std::vector<char> seq2;
    lg.apply([&] (auto&... v) { (seq2.push_back(v.get_user_type().name[0]), ...); });

    CHECK(seq1 == seq2);
    CHECK(seq1.size() == 2);
    CHECK(seq1[0] == 'A');
    CHECK(seq1[1] == 'B');

    lg.apply([&] (auto&... v) { (v.get_user_type().i++, ...); });

    CHECK(sa.i == 1);
    CHECK(sb.i == 1);

    lg.for_each([&] (auto& v) { v.get_user_type().i++; });

    CHECK(sa.i == 2);
    CHECK(sb.i == 2);

}