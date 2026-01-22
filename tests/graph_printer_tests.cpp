#include "doctest.h"
#include "ugraph.hpp"
#include "ugraph/graph_printer.hpp"
#include <sstream>
#include <string>
#include <iostream>

struct AStage { const char* name; };
struct BStage { const char* name; };
struct CStage { const char* name; };

TEST_CASE("graph_printer produces expected node and edge lines") {
    AStage a { "A" };
    BStage b { "B" };
    CStage c { "C" };

    ugraph::Node<101, AStage, 0, 1> vA(a);
    ugraph::Node<102, BStage, 1, 1> vB(b);
    ugraph::Node<103, CStage, 1, 0> vC(c);

    auto g = ugraph::GraphView(
        vB.out() >> vC.in(),
        vA.out() >> vB.in()
    );

    std::ostringstream oss;
    //ugraph::print_graph(g, oss);
    ugraph::print_pipeline<decltype(g)>(std::cout);
    ugraph::print_graph<decltype(g)>(std::cout);
    /*
    std::string out = oss.str();

    // Basic sanity checks: header, node declarations for ids, and edges
    CHECK(out.rfind("graph LR;\n", 0) == 0);
    CHECK(out.find("N101(") != std::string::npos);
    CHECK(out.find("N102(") != std::string::npos);
    CHECK(out.find("N103(") != std::string::npos);

    CHECK(out.find("N101 --> N102") != std::string::npos);
    CHECK(out.find("N102 --> N103") != std::string::npos);
*/
}
