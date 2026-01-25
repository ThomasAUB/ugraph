#include "doctest.h"
#include "ugraph.hpp"
#include "ugraph/graph_printer.hpp"
#include <sstream>
#include <string>
#include <iostream>

struct MyType;

template<typename T>
struct MyTemplateType;

TEST_CASE("type name test") {

    CHECK(ugraph::type_name<int>() == "int");
    CHECK(ugraph::type_name<MyType>() == "MyType");

    static constexpr auto type_str = ugraph::type_name<MyTemplateType<const MyType**>>();
    std::cout << "Type name test: " << type_str << std::endl;

    CHECK((
        type_str == "MyTemplateType<const MyType**>" ||
        type_str == "MyTemplateType<const MyType **>" ||
        type_str == "MyTemplateType<struct MyType const * *>"
        ));

}


struct Stage { const char* name; };

TEST_CASE("graph view print test") {

    Stage a { "A" };
    Stage b { "B" };
    Stage c { "C" };
    Stage d { "D" };

    ugraph::Node<101, Stage, 0, 1> vA(a);
    ugraph::Node<102, Stage, 0, 1> vB(b);
    ugraph::Node<103, Stage, 2, 1> vC(c);
    ugraph::Node<104, Stage, 1, 0> vD(d);

    auto g = ugraph::GraphView(
        vB.out() >> vC.in<1>(),
        vC.out() >> vD.in(),
        vA.out() >> vC.in<0>()
    );

    {
        std::ostringstream oss;
        ugraph::print_graph<decltype(g)>(oss);

        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("101(Stage 101)") != std::string::npos);
        CHECK(out.find("102(Stage 102)") != std::string::npos);
        CHECK(out.find("103(Stage 103)") != std::string::npos);
        CHECK(out.find("104(Stage 104)") != std::string::npos);

        CHECK(out.find("101 --> 103") != std::string::npos);
        CHECK(out.find("102 --> 103") != std::string::npos);
        CHECK(out.find("103 --> 104") != std::string::npos);
    }

    // test print_pipeline
    {
        std::ostringstream oss;
        ugraph::print_pipeline<decltype(g)>(oss);
        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);

        CHECK(out.find("101(Stage 101)") != std::string::npos);
        CHECK(out.find("102(Stage 102)") != std::string::npos);
        CHECK(out.find("103(Stage 103)") != std::string::npos);
        CHECK(out.find("104(Stage 104)") != std::string::npos);

        CHECK(out.find("102 --> 101 --> 103 --> 104") != std::string::npos);
    }
}

TEST_CASE("topology print test") {

    using src1 = ugraph::NodeTag<101, Stage>;
    using src2 = ugraph::NodeTag<102, Stage>;
    using m = ugraph::NodeTag<103, Stage>;
    using sink = ugraph::NodeTag<104, Stage>;

    using topo_t = ugraph::Topology<
        ugraph::Link<src2, m>,
        ugraph::Link<m, sink>,
        ugraph::Link<src1, m>
    >;

    {
        std::ostringstream oss;
        ugraph::print_graph<topo_t>(oss);

        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("101(Stage 101)") != std::string::npos);
        CHECK(out.find("102(Stage 102)") != std::string::npos);
        CHECK(out.find("103(Stage 103)") != std::string::npos);
        CHECK(out.find("104(Stage 104)") != std::string::npos);

        CHECK(out.find("101 --> 103") != std::string::npos);
        CHECK(out.find("102 --> 103") != std::string::npos);
        CHECK(out.find("103 --> 104") != std::string::npos);
    }

    // test print_pipeline
    {
        std::ostringstream oss;
        ugraph::print_pipeline<topo_t>(oss);
        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("102 --> 101 --> 103 --> 104") != std::string::npos);
    }
}

TEST_CASE("split topology print test") {

    using src1 = ugraph::NodeTag<101, Stage>;
    using src2 = ugraph::NodeTag<102, Stage>;
    using m = ugraph::NodeTag<103, Stage>;

    using sec1 = ugraph::NodeTag<104, Stage>;
    using sec2 = ugraph::NodeTag<105, Stage>;

    using topo_t = ugraph::Topology<
        ugraph::Link<src2, m>,
        ugraph::Link<src1, m>,

        ugraph::Link<sec1, sec2>
    >;

    {
        std::ostringstream oss;
        ugraph::print_graph<topo_t>(oss);

        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("101(Stage 101)") != std::string::npos);
        CHECK(out.find("102(Stage 102)") != std::string::npos);
        CHECK(out.find("103(Stage 103)") != std::string::npos);
        CHECK(out.find("104(Stage 104)") != std::string::npos);
        CHECK(out.find("105(Stage 105)") != std::string::npos);

        CHECK(out.find("101 --> 103\n") != std::string::npos);
        CHECK(out.find("102 --> 103\n") != std::string::npos);

        CHECK(out.find("104 --> 105\n") != std::string::npos);
    }

    // test print_pipeline
    {
        std::ostringstream oss;
        ugraph::print_pipeline<topo_t>(oss);
        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("102 --> 101 --> 103 --> 104 --> 105") != std::string::npos);
    }

}