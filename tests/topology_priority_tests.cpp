#include "doctest.h"
#include "ugraph.hpp"
#include <vector>


TEST_CASE("topology priority tie-breaking") {

    struct P_A { const char* name; };
    struct P_B { const char* name; };
    struct P_C { const char* name; };

    P_A a { "A" };
    P_B b { "B" };
    P_C c { "C" };

    ugraph::Node<401, P_A, 0, 1, 10> vA(a);
    ugraph::Node<402, P_B, 0, 1, 11> vB(b);
    ugraph::Node<403, P_C, 2, 0> vC(c);

    auto g = ugraph::GraphView(
        vA.out() >> vC.in<0>(),
        vB.out() >> vC.in<1>()
    );

    std::vector<char> order;
    g.apply(
        [&] (auto&... impls) {
            (order.push_back(impls.module().name[0]), ...);
        }
    );

    REQUIRE(order.size() == 3);
    CHECK(order[0] == 'B');
    CHECK(order[1] == 'A');
    CHECK(order[2] == 'C');
}
