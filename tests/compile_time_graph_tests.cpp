#include "doctest.h"
#include "ugraph.hpp"

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

    decltype(g)::graph_data_t gd;

    g.init_graph_data(gd);

    g.for_each(
        [] (auto& m, auto& ctx) {
            m(ctx);
        }
    );

    return t2.value;
}


TEST_CASE("compile-time graph constexpr construction") {
    static_assert(runGraph(16) == 32, "iofg");
}