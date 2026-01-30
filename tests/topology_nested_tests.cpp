#include "doctest.h"
#include "ugraph.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>

// Tests for nested Topology declared as NodeTag::module_type
namespace {

    struct A { static constexpr int v() { return 1; } };
    struct B { static constexpr int v() { return 2; } };
    struct C { static constexpr int v() { return 3; } };

    using IA = ugraph::NodeTag<1001, A>;
    using IB = ugraph::NodeTag<1002, B>;
    using IC = ugraph::NodeTag<1003, C>;

    // inner topology
    using Inner = ugraph::Topology< ugraph::Link<IA, IB>, ugraph::Link<IB, IC> >;

    // Declare a node whose module_type is the Inner topology
    using NestedNode = ugraph::NodeTag<2000, Inner>;

    using X = ugraph::NodeTag<3001, A>;

    // Outer topology uses NestedNode as a vertex
    using Outer = ugraph::Topology< ugraph::Link<NestedNode, X>, ugraph::Link<X, NestedNode> >;

    static_assert(Inner::size() == 3, "Inner size expected");
    static_assert(Outer::size() >= 2, "Outer has at least outer nodes");

    // Compile-time validations for expanded edges
    template<std::size_t N>
    static constexpr std::size_t count_pair(const std::array<std::pair<std::size_t, std::size_t>, N>& arr, std::size_t a, std::size_t b) {
        std::size_t c = 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (arr[i].first == a && arr[i].second == b) ++c;
        }
        return c;
    }

    static_assert(Outer::edges().size() == 4, "Expanded edges count should be 4");
    static_assert(count_pair(Outer::edges(), IA::id(), IB::id()) == 1, "IA->IB must exist");
    static_assert(count_pair(Outer::edges(), IB::id(), IC::id()) == 1, "IB->IC must exist");
    static_assert(count_pair(Outer::edges(), IC::id(), X::id()) == 1, "IC->X must exist");
    static_assert(count_pair(Outer::edges(), X::id(), IA::id()) == 1, "X->IA must exist");

    TEST_CASE("nested topology compile-time ordering") {
        // Ensure inner vertex ids appear in the flattened vertex list
        constexpr auto ids = Outer::ids();
        bool found_inner_id = false;
        for (std::size_t i = 0; i < Outer::size(); ++i) {
            if (ids[i] == IA::id() || ids[i] == IB::id() || ids[i] == IC::id()) {
                found_inner_id = true;
                break;
            }
        }
        ugraph::print_graph<Outer>(std::cout);
        //ugraph::print_graph<Outer>(std::cout);
        CHECK(found_inner_id);
    }

    TEST_CASE("nested topology runtime for_each") {
        char buf[16] {};
        char* w = buf;
        Outer::for_each([&] (auto v) {
            using VT = decltype(v);
            // record a small marker using module_type if available
            if constexpr (std::is_same_v<typename VT::module_type, Inner>) {
                *w++ = 'N';
            }
            else {
                *w++ = 'o';
            }
            });
        CHECK(std::string("oN").find(buf[0]) != std::string::npos);
    }

    TEST_CASE("nested topology expanded edges") {
        constexpr auto e = Outer::edges();
        std::vector<std::pair<std::size_t, std::size_t>> got(e.begin(), e.end());

        std::vector<std::pair<std::size_t, std::size_t>> expected = {
            { IA::id(), IB::id() },
            { IB::id(), IC::id() },
            { IC::id(), X::id() },
            { X::id(), IA::id() }
        };

        // every expected pair must appear in got
        for (auto& p : expected) {
            CHECK(std::find(got.begin(), got.end(), p) != got.end());
        }

        // sizes must match (no unexpected extra expanded edges)
        CHECK(got.size() == expected.size());
    }

    TEST_CASE("print_graph and print_pipeline output") {
        std::ostringstream oss;
        ugraph::print_graph<Outer>(oss);
        const auto g = oss.str();

        const std::string expected_graph =
            "```mermaid\n"
            "flowchart LR\n"
            "1001(A 1001)\n"
            "1002(B 1002)\n"
            "1003(C 1003)\n"
            "3001(A 3001)\n"
            "1001 --> 1002\n"
            "1002 --> 1003\n"
            "1003 --> 3001\n"
            "3001 --> 1001\n"
            "```\n";

        CHECK(g == expected_graph);

        oss.str(""); oss.clear();
        ugraph::print_pipeline<Outer>(oss);
        const auto p = oss.str();

        const std::string expected_pipeline =
            "```mermaid\n"
            "flowchart LR\n"
            "1001(A 1001)\n"
            "1002(B 1002)\n"
            "1003(C 1003)\n"
            "3001(A 3001)\n"
            "1001 --> 1002 --> 1003 --> 3001\n"
            "```\n";

        CHECK(p == expected_pipeline);
    }

}
