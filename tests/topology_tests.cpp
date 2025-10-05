#include <iostream>
#include <array>
#include <string_view>

#include "doctest.h"

#include "ugraph.hpp"

// -----------------------------------------------------------------------------
// Topology (compile-time focused) tests
// This file contains tests that exercise the pure type-level topology utilities
// (Vertex / Edge / Topology) without involving the runtime routed graph layer.
// -----------------------------------------------------------------------------

namespace {

    // Dependency-only vertices (no runtime instances) using custom meta types.
    struct P1 { static void print() { std::cout << "1"; } static constexpr int num() { return 1; } };
    struct P2 { static void print() { std::cout << "2"; } static constexpr int num() { return 2; } };
    struct P3 { static void print() { std::cout << "3"; } static constexpr int num() { return 3; } };
    struct P4 { static void print() { std::cout << "4"; } static constexpr int num() { return 4; } };

    using V1 = ugraph::Vertex<1, P1>;
    using V2 = ugraph::Vertex<2, P2>;
    using V3 = ugraph::Vertex<3, P3>;
    using V4 = ugraph::Vertex<4, P4>;

    // Instantiate the pure topology (compile-time only) with edge types only.
    using CompileTimeGraphT =
        ugraph::Topology<
        ugraph::Edge<V2, V4>,
        ugraph::Edge<V2, V3>,
        ugraph::Edge<V1, V2>,
        ugraph::Edge<V1, V3>,
        ugraph::Edge<V1, V4>
        >;

    static_assert(CompileTimeGraphT::size() == 4, "Graph should have 4 unique vertices");

    constexpr auto ids_ct = CompileTimeGraphT::ids();

    static_assert(ids_ct[0] == 1, "First in topological order should be vertex 1 (a source)");
    static_assert(ids_ct[1] == 2, "Second in topological order should be vertex 2");

    // Use compile-time for_each (type-level) to verify visited order matches ids().
    static constexpr bool verify_type_iteration() {
        std::array<std::size_t, CompileTimeGraphT::size()> collected {};
        std::size_t idx = 0;
        CompileTimeGraphT::for_each(
            [&] (auto vertex_wrapper) {
                collected[idx++] = decltype(vertex_wrapper)::id();
            }
        );
        for (std::size_t i = 0; i < collected.size(); ++i) {
            if (collected[i] != ids_ct[i]) return false;
        }
        return true;
    }
    static_assert(verify_type_iteration(), "Compile-time for_each order mismatch");

    // Variadic apply: single call receiving all vertex tags at once.
    static constexpr auto variadic_ids =
        CompileTimeGraphT::apply(
            [] (auto... tags) {
                return std::array<std::size_t, sizeof...(tags)>{ decltype(tags)::id()... };
            }
        );

    static constexpr int num_sum =
        CompileTimeGraphT::apply(
            [] (auto... tags) {
                return (decltype(tags)::type::num() + ...);
            }
        );
    static_assert(num_sum == 1 + 2 + 3 + 4);


    static constexpr int for_each_test() {
        int sum = 0;
        CompileTimeGraphT::for_each(
            [&sum] (auto tags) {
                sum += decltype(tags)::type::num();
            }
        );
        return sum == 1 + 2 + 3 + 4;
    }
    static_assert(for_each_test());

    static_assert(variadic_ids.size() == CompileTimeGraphT::size(), "Unexpected size from variadic apply");
    static_assert(variadic_ids[0] == ids_ct[0], "Variadic apply order mismatch at 0");
    static_assert(variadic_ids[1] == ids_ct[1], "Variadic apply order mismatch at 1");
}

TEST_CASE("topology callable compile-time Vertex test") {
    // Define callable functors with side effects on a static buffer for test validation
    struct F1 { static void record(char*& w) { *w++ = 'A'; } };
    struct F2 { static void record(char*& w) { *w++ = 'B'; } };
    struct F3 { static void record(char*& w) { *w++ = 'C'; } };

    using D1 = ugraph::Vertex<101, F1>; // source
    using D2 = ugraph::Vertex<102, F2>; // middle
    using D3 = ugraph::Vertex<103, F3>; // sink

    using Topo = ugraph::Topology<
        ugraph::Edge<D2, D3>,
        ugraph::Edge<D1, D2>
    >;

    static_assert(Topo::size() == 3, "Expected three dependency vertices");

    char visited[4] {};
    char* write = visited;

    Topo::for_each([&] (auto vertex_wrapper) {
        using VT = decltype(vertex_wrapper);
        VT::type::record(write);
        }
    );

    CHECK(visited[0] == 'A');
    CHECK(visited[1] == 'B');
    CHECK(visited[2] == 'C');
}
