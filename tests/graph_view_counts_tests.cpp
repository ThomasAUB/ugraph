#include "doctest.h"
#include "ugraph.hpp"
#include <iostream>

// Tests validating Graph input_count, output_count and data_count

// Create small module types that expose a Manifest suitable for Graph

struct JoinModule {
    using Manifest = ugraph::Manifest< ugraph::IO<int, 2, 1, false> >;
    void process(ugraph::Context<Manifest>&) {}
};

struct A_src {
    using Manifest = ugraph::Manifest< ugraph::IO<int, 0, 1> >;
    void process(ugraph::Context<Manifest>&) {}
};

struct A_mid {
    using Manifest = ugraph::Manifest< ugraph::IO<int, 1, 1> >;
    void process(ugraph::Context<Manifest>&) {}
};

struct A_sink {
    using Manifest = ugraph::Manifest< ugraph::IO<int, 1, 0> >;
    void process(ugraph::Context<Manifest>&) {}
};


TEST_CASE("data_graph missing inputs and outputs and total buffer sum") {
    A_src s0m;
    JoinModule j2m;

    auto vSrc = ugraph::make_node<10>(s0m);
    auto vJoin = ugraph::make_node<12>(j2m);

    auto g = ugraph::Graph(
        vSrc.output<int, 0>() >> vJoin.input<int, 0>()
    );

    using G = decltype(g);

    CHECK(G::template data_count<int>() == 1);
}

TEST_CASE("data_graph chain producers buffer allocation") {
    A_src a1;
    A_mid a2;
    A_sink a3;

    auto vA = ugraph::make_node<101>(a1);
    auto vB = ugraph::make_node<102>(a2);
    auto vC = ugraph::make_node<103>(a3);

    auto g = ugraph::Graph(
        vA.output<int, 0>() >> vB.input<int, 0>(),
        vB.output<int, 0>() >> vC.input<int, 0>()
    );

    using G = decltype(g);

    CHECK(G::template data_count<int>() == 2);
}
