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

    struct P_A_d { const char* name; using Manifest = ugraph::Manifest< ugraph::IO<const char*, 0, 1> >; void process(ugraph::NodeContext<Manifest>&) {} };
    struct P_B_d { const char* name; using Manifest = ugraph::Manifest< ugraph::IO<const char*, 0, 1> >; void process(ugraph::NodeContext<Manifest>&) {} };
    struct P_C_d { const char* name; using Manifest = ugraph::Manifest< ugraph::IO<const char*, 2, 0> >; void process(ugraph::NodeContext<Manifest>&) {} };

    P_A_d ad { "A" };
    P_B_d bd { "B" };
    P_C_d cd { "C" };

    auto vA = ugraph::make_node<401, P_A_d, P_A_d::Manifest, 10>(ad);
    auto vB = ugraph::make_node<402, P_B_d, P_B_d::Manifest, 11>(bd);
    auto vC = ugraph::make_node<403>(cd);

    auto g = ugraph::Graph(
        vA.output<const char*, 0>() >> vC.input<const char*, 0>(),
        vB.output<const char*, 0>() >> vC.input<const char*, 1>()
    );

    std::vector<char> order;


    g.for_each([
        &] (auto& module, auto& /* ctx */) {
            order.push_back(module.name[0]);
        });

        REQUIRE(order.size() == 3);
        CHECK(order[0] == 'B');
        CHECK(order[1] == 'A');
        CHECK(order[2] == 'C');
}
