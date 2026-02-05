#include "doctest.h"
#include "ugraph.hpp"

namespace {

    struct M0 {};
    struct M1 {};
    struct M2 {};

    template<std::size_t Id, std::size_t In, std::size_t Out, typename Mod>
    using Tag = ugraph::NodePortTag<Id, Mod, In, Out>;



} // namespace

TEST_CASE("interval coloring missing inputs/outputs with tag ports") {

    using A = Tag<1, 0, 1, M0>;
    using B = Tag<2, 2, 1, M1>;

    using G = ugraph::IntervalColoring<
        ugraph::Link<A::OutputPort<0>, B::InputPort<0>>
    >;

    static_assert(G::data_instance_count() == 1, "Expected one producer buffer");
    static_assert(G::input_count() == 1, "Expected one missing input (B.in1)");
    static_assert(G::output_count() == 1, "Expected one missing output (B.out0)");

    CHECK(G::data_instance_count() == 1);
    CHECK(G::input_count() == 1);
    CHECK(G::output_count() == 1);

    static_assert(G::output_data_index<1, 0>() == G::input_data_index<2, 0>());

}

TEST_CASE("interval coloring chain buffer reuse with tag ports") {
    using A = Tag<10, 0, 1, M0>;
    using B = Tag<11, 1, 1, M1>;
    using C = Tag<12, 1, 0, M2>;

    using G = ugraph::IntervalColoring<
        ugraph::Link<A::OutputPort<0>, B::InputPort<0>>,
        ugraph::Link<B::OutputPort<0>, C::InputPort<0>>
    >;

    static_assert(G::data_instance_count() == 2, "Overlapping lifetimes require two buffers");
    static_assert(G::input_count() == 0);
    static_assert(G::output_count() == 0);

    CHECK(G::data_instance_count() == 2);
    CHECK(G::input_count() == 0);
    CHECK(G::output_count() == 0);

    static_assert(G::output_data_index<10, 0>() == G::input_data_index<11, 0>());
    static_assert(G::output_data_index<11, 0>() == G::input_data_index<12, 0>());
}