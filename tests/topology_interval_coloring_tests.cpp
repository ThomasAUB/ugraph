#include "doctest.h"
#include "ugraph.hpp"

using namespace ugraph;

// Simple graph with three producers and overlapping lifetimes
struct P1 {};
struct P2 {};
struct P3 {};

using V1 = NodeTag<1, P1>;
using V2 = NodeTag<2, P2>;
using V3 = NodeTag<3, P3>;

// Edges: V1 -> V2, V1 -> V3, V2 -> V3
using E1 = std::pair<V1, V2>;
using E2 = std::pair<V1, V3>;
using E3 = std::pair<V2, V3>;

using CTG = Topology<E1, E2, E3>;

TEST_CASE("interval coloring instance count and indices") {
    // There are two distinct producers: outputs of V1 and V2
    CHECK(CTG::data_instance_count() == 2);

    // Verify that data indices resolve for outputs
    constexpr auto idx_v1 = CTG::template output_data_index<V1::id(), 0>();
    constexpr auto idx_v2 = CTG::template output_data_index<V2::id(), 0>();
    CHECK(idx_v1 < CTG::data_instance_count());
    CHECK(idx_v2 < CTG::data_instance_count());

    // Input of V3 should map to an output producer index
    constexpr auto in0 = CTG::template input_data_index<V3::id(), 0>();
    CHECK(in0 < CTG::data_instance_count());
}
