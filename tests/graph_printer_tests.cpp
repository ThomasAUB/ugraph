#include "doctest.h"
#include "ugraph.hpp"
#include "ugraph/graph_printer.hpp"
#include <sstream>
#include <string>
#include <iostream>
#include "dbg_print_graph.hpp"


struct MyType;

template<typename T>
struct MyTemplateType;

TEST_CASE("type name test") {

    CHECK(ugraph::type_name<int>() == "int");
    CHECK(ugraph::type_name<MyType>() == "MyType");

    static constexpr auto type_str = ugraph::type_name<MyTemplateType<const MyType**>>();

    CHECK((
        type_str == "MyTemplateType<const MyType**>" ||
        type_str == "MyTemplateType<const MyType **>" ||
        type_str == "MyTemplateType<struct MyType const * *>"
        ));

}


struct Stage { const char* name; };

struct PrinterTagA {};

struct PrinterSource {
    using Manifest = ugraph::Manifest<ugraph::TaggedIO<PrinterTagA, int, 0, 1>>;
};

struct PrinterTagB {};

struct PrinterMultiSource {
    using Manifest = ugraph::Manifest<
        ugraph::TaggedIO<PrinterTagA, int, 0, 1>,
        ugraph::TaggedIO<PrinterTagB, float, 0, 1>
    >;
};

struct PrinterSink {
    using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 0>>;
};

struct PrinterMultiSink {
    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 1, 0>,
        ugraph::IO<float, 1, 0>
    >;
};

TEST_CASE("graph print test") {
    // Use a compile-time Topology for printing.
    using src1 = ugraph::NodeTag<101, Stage>;
    using src2 = ugraph::NodeTag<102, Stage>;
    using m = ugraph::NodeTag<103, Stage>;
    using sink = ugraph::NodeTag<104, Stage>;

    using topo_t = ugraph::Topology<
        std::pair<src2, m>,
        std::pair<m, sink>,
        std::pair<src1, m>
    >;

    {
        std::ostringstream oss;
        ugraph::print_graph<topo_t>(oss);

        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("101(\"Stage\")") != std::string::npos);
        CHECK(out.find("102(\"Stage\")") != std::string::npos);
        CHECK(out.find("103(\"Stage\")") != std::string::npos);
        CHECK(out.find("104(\"Stage\")") != std::string::npos);

        CHECK(out.find("101 --> 103") != std::string::npos);
        CHECK(out.find("102 --> 103") != std::string::npos);
        CHECK(out.find("103 --> 104") != std::string::npos);

    }

    {
        std::ostringstream oss;
        ugraph::print_graph<topo_t>(oss, "", false, true);

        std::string out = oss.str();

        CHECK(out.find("101(\"Stage 101\")") != std::string::npos);
        CHECK(out.find("102(\"Stage 102\")") != std::string::npos);
        CHECK(out.find("103(\"Stage 103\")") != std::string::npos);
        CHECK(out.find("104(\"Stage 104\")") != std::string::npos);
    }

    // test print_pipeline
    {
        std::ostringstream oss;
        ugraph::print_pipeline<topo_t>(oss);
        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);

        CHECK(out.find("101(\"Stage 101\")") != std::string::npos);
        CHECK(out.find("102(\"Stage 102\")") != std::string::npos);
        CHECK(out.find("103(\"Stage 103\")") != std::string::npos);
        CHECK(out.find("104(\"Stage 104\")") != std::string::npos);

        CHECK(out.find("102 --> 101 --> 103 --> 104") != std::string::npos);

    }

    {
        std::ostringstream oss;
        ugraph::print_pipeline<topo_t>(oss, "", false);
        std::string out = oss.str();

        CHECK(out.find("101(\"Stage\")") != std::string::npos);
        CHECK(out.find("102(\"Stage\")") != std::string::npos);
        CHECK(out.find("103(\"Stage\")") != std::string::npos);
        CHECK(out.find("104(\"Stage\")") != std::string::npos);
    }
}

TEST_CASE("graph print test shows tagged io names") {
    PrinterSource sourceModule;
    PrinterSink sinkModule;

    auto source = ugraph::make_node<201>(sourceModule);
    auto sink = ugraph::make_node<202>(sinkModule);

    ugraph::Graph g(
        source.output<PrinterTagA>() >> sink.input<int>()
    );

    std::ostringstream oss;
    ugraph::print_graph(g, oss);

    std::string out = oss.str();

    CHECK(out.find("201(\"PrinterSource\")") != std::string::npos);
    CHECK(out.find("202(\"PrinterSink\")") != std::string::npos);
    CHECK(out.find("201 -->|PrinterTagA: int| 202") != std::string::npos);

    dbgPrintGraph(g, "Tagged");
}

TEST_CASE("graph print test shows several tagged io names") {
    PrinterMultiSource sourceModule;
    PrinterMultiSink sinkModule;

    auto source = ugraph::make_node<203>(sourceModule);
    auto sink = ugraph::make_node<204>(sinkModule);

    ugraph::Graph g(
        source.output<PrinterTagA>() >> sink.input<int>(),
        source.output<PrinterTagB>() >> sink.input<float>()
    );

    std::ostringstream oss;
    ugraph::print_graph(g, oss);

    std::string out = oss.str();

    CHECK(out.find("203(\"PrinterMultiSource\")") != std::string::npos);
    CHECK(out.find("204(\"PrinterMultiSink\")") != std::string::npos);
    CHECK(out.find("203 -->|PrinterTagA: int| 204") != std::string::npos);
    CHECK(out.find("203 -->|PrinterTagB: float| 204") != std::string::npos);

    dbgPrintGraph(g, "Tagged");
}

TEST_CASE("graph print test shows source and destination tag names when they differ") {
    struct PrinterSourceTag {};
    struct PrinterDestTag {};

    struct TaggedSource {
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<PrinterSourceTag, int, 0, 1>>;
    };

    struct TaggedSink {
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<PrinterDestTag, int, 1, 0>>;
    };

    TaggedSource sourceModule;
    TaggedSink sinkModule;

    auto source = ugraph::make_node<205>(sourceModule);
    auto sink = ugraph::make_node<206>(sinkModule);

    ugraph::Graph g(
        source.output<PrinterSourceTag>() >> sink.input<PrinterDestTag>()
    );

    std::ostringstream oss;
    ugraph::print_graph(g, oss);

    std::string out = oss.str();

    CHECK(out.find("205 -->|PrinterSourceTag to PrinterDestTag: int| 206") != std::string::npos);

    dbgPrintGraph(g, "Tagged");
}

TEST_CASE("topology print test") {

    using src1 = ugraph::NodeTag<101, Stage>;
    using src2 = ugraph::NodeTag<102, Stage>;
    using m = ugraph::NodeTag<103, Stage>;
    using sink = ugraph::NodeTag<104, Stage>;

    using topo_t = ugraph::Topology<
        std::pair<src2, m>,
        std::pair<m, sink>,
        std::pair<src1, m>
    >;

    {
        std::ostringstream oss;
        ugraph::print_graph<topo_t>(oss);

        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("101(\"Stage\")") != std::string::npos);
        CHECK(out.find("102(\"Stage\")") != std::string::npos);
        CHECK(out.find("103(\"Stage\")") != std::string::npos);
        CHECK(out.find("104(\"Stage\")") != std::string::npos);

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
        std::pair<src2, m>,
        std::pair<src1, m>,
        std::pair<sec1, sec2>
    >;

    {
        std::ostringstream oss;
        ugraph::print_graph<topo_t>(oss);

        std::string out = oss.str();

        CHECK(out.rfind("```mermaid\nflowchart LR\n", 0) == 0);
        CHECK(out.find("101(\"Stage\")") != std::string::npos);
        CHECK(out.find("102(\"Stage\")") != std::string::npos);
        CHECK(out.find("103(\"Stage\")") != std::string::npos);
        CHECK(out.find("104(\"Stage\")") != std::string::npos);
        CHECK(out.find("105(\"Stage\")") != std::string::npos);

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