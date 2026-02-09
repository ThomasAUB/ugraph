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
    // Use a compile-time Topology for printing (GraphView removed)
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