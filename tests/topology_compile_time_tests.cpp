#include <iostream>
#include <array>
#include <string_view>

#include "doctest.h"
#include "ugraph.hpp"

// Compile-time Topology tests (NodeTag / Link only, no runtime Node instances).
namespace {

    // Simple payload types carried by NodeTag for identification in tests.
    struct P1 {
        static void print() { std::cout << "1"; }
        static constexpr int num() { return 1; }
    };
    struct P2 {
        static void print() { std::cout << "2"; }
        static constexpr int num() { return 2; }
    };
    struct P3 {
        static void print() { std::cout << "3"; }
        static constexpr int num() { return 3; }
    };
    struct P4 {
        static void print() { std::cout << "4"; }
        static constexpr int num() { return 4; }
    };

    // Node tag aliases (compile-time only vertices)
    using V1 = ugraph::NodeTag<1, P1>;
    using V2 = ugraph::NodeTag<2, P2>;
    using V3 = ugraph::NodeTag<3, P3>;
    using V4 = ugraph::NodeTag<4, P4>;

    // Define a compile-time graph (edges expressed as Link<Src,Dst>)
    using CTGraph = ugraph::Topology<
        ugraph::Link<V2, V4>,
        ugraph::Link<V2, V3>,
        ugraph::Link<V1, V2>,
        ugraph::Link<V1, V3>,
        ugraph::Link<V1, V4>
    >;

    static_assert(CTGraph::size() == 4, "Unexpected vertex count");

    // Extract ordered ids from the topology.
    constexpr auto ids = CTGraph::ids();
    static_assert(ids[0] == 1 && ids[1] == 2, "Topological ordering assumption failed");

    // Verify for_each visitation order at compile time.
    static constexpr bool for_each_ok() {
        std::array<std::size_t, CTGraph::size()> collected {};
        std::size_t idx = 0;
        CTGraph::for_each([&] (auto v) {
            collected[idx++] = decltype(v)::id();
            });
        for (std::size_t i = 0; i < collected.size(); ++i) {
            if (collected[i] != ids[i]) {
                return false;
            }
        }
        return true;
    };
    static_assert(for_each_ok(), "for_each order mismatch");

    // Collect ids via variadic apply.
    static constexpr auto variadic_ids = CTGraph::apply([] (auto... vs) {
        return std::array<std::size_t, sizeof...(vs)>{ decltype(vs)::id()... };
        });
    static_assert(variadic_ids[0] == ids[0] && variadic_ids[1] == ids[1]);

}

//------------------------------------------------------------------------------
// Runtime echo test of apply/for_each over NodeTag graph.
//------------------------------------------------------------------------------
TEST_CASE("topology tag apply/for_each runtime echo") {
    struct F1 { static void record(char*& w) { *w++ = 'A'; } };
    struct F2 { static void record(char*& w) { *w++ = 'B'; } };
    struct F3 { static void record(char*& w) { *w++ = 'C'; } };

    using D1 = ugraph::NodeTag<101, F1>;
    using D2 = ugraph::NodeTag<102, F2>;
    using D3 = ugraph::NodeTag<103, F3>;

    using G = ugraph::Topology<
        ugraph::Link<D2, D3>,
        ugraph::Link<D1, D2>
    >;

    static_assert(G::size() == 3);

    char  visited[4] {};
    char* w = visited;

    G::for_each([&] (auto v) {
        using VT = decltype(v);
        // module_type is the payload inside NodeTag
        VT::module_type::record(w);
        });

    CHECK(visited[0] == 'A');
    CHECK(visited[1] == 'B');
    CHECK(visited[2] == 'C');
}

TEST_CASE("node tag ports indices compile-time") {
    using A = ugraph::NodeTag<10, P1>;
    using B = ugraph::NodeTag<11, P2>;

    using E1 = ugraph::Link<A::Out<3>, B::In<5>>;
    using G1 = ugraph::Topology<E1>;
    static_assert(G1::edge_traits<E1>::src_port_index == 3, "src port index mismatch");
    static_assert(G1::edge_traits<E1>::dst_port_index == 5, "dst port index mismatch");

    // Ports are optional: when absent, indices default to zero.
    using E2 = ugraph::Link<A, B>;
    using G2 = ugraph::Topology<E2>;
    static_assert(G2::edge_traits<E2>::src_port_index == 0, "default src port index should be 0");
    static_assert(G2::edge_traits<E2>::dst_port_index == 0, "default dst port index should be 0");
}
