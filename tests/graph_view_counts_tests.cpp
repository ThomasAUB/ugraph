#include "doctest.h"
#include "ugraph.hpp"

// Tests validating GraphView input_count, output_count and data_instance_count

struct S0 {};
struct J2 {};
struct A {};

TEST_CASE("graph_view missing inputs and outputs and total buffer sum") {
    S0 s0m;
    J2 j2m;

    ugraph::Node<10, S0, 0, 1> vSrc(s0m);
    ugraph::Node<20, J2, 2, 1> vJoin(j2m);

    auto g = ugraph::GraphView(
        vSrc.out() >> vJoin.in<0>()
    );

    static_assert(decltype(g)::input_count() == 1, "Unexpected input_count");
    static_assert(decltype(g)::output_count() == 1, "Unexpected output_count");
    static_assert(decltype(g)::data_instance_count() == 1, "Unexpected data_instance_count");

    CHECK(decltype(g)::input_count() == 1);
    CHECK(decltype(g)::output_count() == 1);
    CHECK(decltype(g)::data_instance_count() == 1);

    // The total number of data slots required equals producers + external inputs + external outputs
    CHECK(decltype(g)::data_instance_count() + decltype(g)::input_count() + decltype(g)::output_count() == 3);
}

TEST_CASE("graph_view chain producers buffer allocation") {
    A a1; A a2; A a3;

    ugraph::Node<101, A, 0, 1> vA(a1);
    ugraph::Node<102, A, 1, 1> vB(a2);
    ugraph::Node<103, A, 1, 0> vC(a3);

    auto g = ugraph::GraphView(
        vA.out() >> vB.in(),
        vB.out() >> vC.in()
    );

    // Two producers (vA.out, vB.out) with overlapping lifetimes require two buffers.
    static_assert(decltype(g)::data_instance_count() == 2, "Unexpected data_instance_count for chain");
    CHECK(decltype(g)::data_instance_count() == 2);

    // No external inputs/outputs in this linear chain
    CHECK(decltype(g)::input_count() == 0);
    CHECK(decltype(g)::output_count() == 0);
}
