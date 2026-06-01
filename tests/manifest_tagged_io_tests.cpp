#include "doctest.h"
#include "ugraph.hpp"
#include <type_traits>

TEST_CASE("Manifest handles TaggedIO alongside IO without conflict") {

    struct TagA {};
    using Value = int;

    using M = ugraph::Manifest<
        ugraph::IO<Value, 1, 1>,
        ugraph::TaggedIO<TagA, Value, 2, 1>
    >;

    // contains by value and by tag
    CHECK(M::contains<Value>);
    CHECK(M::contains_tag<TagA>);

    // value-based lookup should resolve to the untagged IO
    CHECK(std::is_same_v<typename M::template spec_for<Value>, ugraph::IO<Value, 1, 1>>);

    // tag-based lookup should resolve to the tagged IO
    CHECK(std::is_same_v<typename M::template spec_for<TagA>, ugraph::TaggedIO<TagA, Value, 2, 1>>);

    // counts
    CHECK(M::spec_count_for_value<Value> == 1);
    CHECK(M::spec_count_for_tag<TagA> == 1);

    // indices: first spec is untagged Value, second is tagged
    CHECK(M::index<Value>() == 0);
    CHECK(M::index<TagA>() == 1);

}
