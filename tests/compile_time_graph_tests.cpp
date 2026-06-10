#include "doctest.h"
#include "ugraph.hpp"

#include <utility>

struct T1 {

    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 0, 1>
    >;

    constexpr T1(int v) : inVal(v) {}

    constexpr void operator()(ugraph::Context<Manifest>& ctx) {
        ctx.output<int>() = inVal;
    }

    int inVal = 0;
};

struct T2 {

    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 1, 0>
    >;

    constexpr void operator()(ugraph::Context<Manifest>& ctx) {
        value = ctx.input<int>() * 2;
    }

    int value = 0;
};

static constexpr auto getGraph(T1& t1, T2& t2) {
    auto nt1 = ugraph::make_node<0>(t1);
    auto nt2 = ugraph::make_node<1>(t2);
    return ugraph::Graph(
        nt1.output<int>() >> nt2.input<int>()
    );
}

static constexpr auto runGraph(int v) {

    T1 t1(v);
    T2 t2;

    auto g = getGraph(t1, t2);

    g.for_each(
        [] (auto& m, auto& ctx) {
            m(ctx);
        }
    );

    return t2.value;
}


TEST_CASE("compile-time graph construction") {
    static_assert(runGraph(16) == 32, "Compile-time run failed");
    CHECK(runGraph(16) == 32);
}

TEST_CASE("graph rejects duplicate input connections at compile time") {

    struct Start {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 0, 1>
        >;
    };

    struct Recursive {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 2, 1>
        >;
    };

    using start_node_t = decltype(ugraph::make_node<10>(std::declval<Start&>()));
    using recursive_node_t = decltype(ugraph::make_node<11>(std::declval<Recursive&>()));
    using internal_edge_t = std::pair<
        typename start_node_t::template OutputPort<int, 0>,
        typename recursive_node_t::template InputPort<int, 0>
    >;
    using external_edge_t = ugraph::InDataBind<int, typename recursive_node_t::template InputPort<int, 0>>;
    using valid_graph_t = ugraph::ExternalDataGraph<internal_edge_t>;
    using invalid_graph_t = ugraph::ExternalDataGraph<internal_edge_t, external_edge_t>;

    static_assert(valid_graph_t::has_unique_input_connections(), "Single input connection should be valid");
    static_assert(!invalid_graph_t::has_unique_input_connections(), "Duplicate input connections should be rejected");

    CHECK(valid_graph_t::has_unique_input_connections());
    CHECK(!invalid_graph_t::has_unique_input_connections());
}