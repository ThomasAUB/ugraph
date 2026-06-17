#include "doctest.h"
#include "ugraph.hpp"

TEST_CASE("GraphInput and GraphOutput basic bridging with process(ctx)") {

    struct InType { int value; };
    struct OutType { int value; };

    struct Passthrough {
        using Manifest = ugraph::Manifest<
            ugraph::IO<InType, 1, 0>,
            ugraph::IO<OutType, 0, 1>
        >;
        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<OutType>().value = ctx.input<InType>().value;
        }
    };

    Passthrough pt;
    auto ptNode = ugraph::make_node<100>(pt);

    auto gIn = ugraph::graph_io::make_input<101>();
    auto gOut = ugraph::graph_io::make_output<102>();

    auto g = ugraph::Graph(
        gIn.output<InType>() >> ptNode.input<InType>(),
        ptNode.output<OutType>() >> gOut.input<OutType>()
    );

    using manifest_t = typename decltype(g)::io_manifest;

    ugraph::Context<manifest_t> ctx;
    InType inVal { 42 };
    OutType outVal { 0 };

    ctx.template set_input_ptr<0, InType>(&inVal);
    ctx.template set_output_ptr<0, OutType>(&outVal);

    g.process(ctx);

    CHECK(outVal.value == 42);
}

TEST_CASE("GraphInput with multiple internal sinks") {

    struct InType { int value; };
    struct OutType { int value; };

    struct Scale {
        using Manifest = ugraph::Manifest<
            ugraph::IO<InType, 1, 0>,
            ugraph::IO<OutType, 0, 1>
        >;
        int factor = 2;
        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<OutType>().value = ctx.input<InType>().value * factor;
        }
    };

    Scale sc;

    auto gIn = ugraph::graph_io::make_input<300>();
    auto scNode = ugraph::make_node<301>(sc);
    auto gOut = ugraph::graph_io::make_output<304>();

    auto g = ugraph::Graph(
        gIn.output<InType>() >> scNode.input<InType>(),
        scNode.output<OutType>() >> gOut.input<OutType>()
    );

    using manifest_t = typename decltype(g)::io_manifest;

    ugraph::Context<manifest_t> ctx;
    InType inVal { 7 };
    OutType outVal { 0 };

    ctx.template set_input_ptr<0, InType>(&inVal);
    ctx.template set_output_ptr<0, OutType>(&outVal);

    g.process(ctx);

    CHECK(outVal.value == 14);
}

TEST_CASE("io_manifest correctly flips GraphInput/GraphOutput specs") {

    struct TagA {};
    struct TagB {};

    using input_specs = ugraph::detail::type_list<
        ugraph::IO<int, 0, 1>,
        ugraph::TaggedIO<TagA, float, 0, 1>
    >;
    using output_specs = ugraph::detail::type_list<
        ugraph::IO<double, 1, 0>,
        ugraph::TaggedIO<TagB, char, 1, 0>
    >;

    using manifest = ugraph::iomifest_t<input_specs, output_specs>;

    static_assert(manifest::contains<int>);
    static_assert(manifest::contains<TagA>);
    static_assert(manifest::contains<double>);
    static_assert(manifest::contains<TagB>);

    static_assert(manifest::input_count<int>() == 1);
    static_assert(manifest::output_count<int>() == 0);
    static_assert(manifest::input_count<TagA>() == 1);
    static_assert(manifest::output_count<TagA>() == 0);
    static_assert(manifest::input_count<double>() == 0);
    static_assert(manifest::output_count<double>() == 1);
    static_assert(manifest::input_count<TagB>() == 0);
    static_assert(manifest::output_count<TagB>() == 1);
}

TEST_CASE("graph_io_keys extracts external inputs and outputs from node types") {

    using input_node = ugraph::Node<1, ugraph::GraphInput<float>, ugraph::Manifest<ugraph::IO<float, 0, 1>>>;
    using output_node = ugraph::Node<2, ugraph::GraphOutput<int>, ugraph::Manifest<ugraph::IO<int, 1, 0>>>;

    struct Dummy { using Manifest = ugraph::Manifest<ugraph::IO<double, 1, 1>>; };
    using regular_node = ugraph::Node<3, Dummy, ugraph::Manifest<ugraph::IO<double, 1, 1>>>;

    using keys = ugraph::graph_io_keys<ugraph::detail::type_list<input_node, output_node, regular_node>>;

    using expected_inputs = ugraph::detail::type_list<ugraph::IO<float, 0, 1>>;
    using expected_outputs = ugraph::detail::type_list<ugraph::IO<int, 1, 0>>;

    static_assert(std::is_same_v<keys::external_inputs, expected_inputs>);
    static_assert(std::is_same_v<keys::external_outputs, expected_outputs>);
}

TEST_CASE("ExternalDataGraph with GraphInput/GraphOutput bridging") {

    struct InType { int value; };
    struct OutType { int value; };

    struct Scale {
        using Manifest = ugraph::Manifest<
            ugraph::IO<InType, 1, 0>,
            ugraph::IO<OutType, 0, 1>
        >;
        int factor = 3;
        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<OutType>().value = ctx.input<InType>().value * factor;
        }
    };

    Scale sc;
    auto gIn = ugraph::graph_io::make_input<400>();
    auto scNode = ugraph::make_node<401>(sc);
    auto gOut = ugraph::graph_io::make_output<402>();

    auto g = ugraph::Graph(
        gIn.output<InType>() >> scNode.input<InType>(),
        scNode.output<OutType>() >> gOut.input<OutType>()
    );

    using manifest_t = typename decltype(g)::io_manifest;
    ugraph::Context<manifest_t> ctx;
    InType inVal { 10 };
    OutType outVal { 0 };
    ctx.template set_input_ptr<0, InType>(&inVal);
    ctx.template set_output_ptr<0, OutType>(&outVal);

    g.process(ctx);
    CHECK(outVal.value == 30);
}

TEST_CASE("Graph with no GraphInput/GraphOutput nodes still processes correctly") {

    struct Source {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 0, 1>>;
        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = 99;
        }
    };

    struct Sink {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 0>>;
        int value = 0;
        void process(ugraph::Context<Manifest>& ctx) {
            value = ctx.input<int>();
        }
    };

    Source src;
    Sink sink;
    auto srcNode = ugraph::make_node<600>(src);
    auto sinkNode = ugraph::make_node<601>(sink);

    auto g = ugraph::Graph(
        srcNode.output<int>() >> sinkNode.input<int>()
    );

    g.for_each([](auto& m, auto& ctx) { m.process(ctx); });

    CHECK(sink.value == 99);
}

struct InTag {};
struct OutTag {};

struct Transform {
    using Manifest = ugraph::Manifest<
        ugraph::TaggedIO<InTag, int, 1, 0>,
        ugraph::TaggedIO<OutTag, int, 0, 1>
    >;
    void process(ugraph::Context<Manifest>& ctx) {
        ctx.output<OutTag>() = ctx.input<InTag>() * 10;
    }
};

TEST_CASE("GraphInputTag/GraphOutputTag with TaggedIO in io_manifest") {

    Transform t;
    ugraph::GraphInputTag<InTag, int> gInModule;
    ugraph::GraphOutputTag<OutTag, int> gOutModule;

    auto gIn = ugraph::make_node<700>(gInModule);
    auto tNode = ugraph::make_node<701>(t);
    auto gOut = ugraph::make_node<702>(gOutModule);

    auto g = ugraph::Graph(
        gIn.output<InTag>() >> tNode.input<InTag>(),
        tNode.output<OutTag>() >> gOut.input<OutTag>()
    );

    using manifest_t = typename decltype(g)::io_manifest;

    static_assert(manifest_t::contains<InTag>);
    static_assert(manifest_t::contains<OutTag>);
    static_assert(manifest_t::input_count<InTag>() == 1);
    static_assert(manifest_t::output_count<InTag>() == 0);
    static_assert(manifest_t::input_count<OutTag>() == 0);
    static_assert(manifest_t::output_count<OutTag>() == 1);

    ugraph::Context<manifest_t> ctx;
    int inVal = 5;
    int outVal = 0;

    ctx.template set_input_ptr<0, InTag>(&inVal);
    ctx.template set_output_ptr<0, OutTag>(&outVal);

    g.process(ctx);

    CHECK(outVal == 50);
}

TEST_CASE("process(ctx) called twice updates outputs correctly") {

    struct InType { int value; };
    struct OutType { int value; };

    struct Accumulate {
        using Manifest = ugraph::Manifest<
            ugraph::IO<InType, 1, 0>,
            ugraph::IO<OutType, 0, 1>
        >;
        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<OutType>().value = ctx.input<InType>().value + 1;
        }
    };

    Accumulate a;
    auto gIn = ugraph::graph_io::make_input<900>();
    auto aNode = ugraph::make_node<901>(a);
    auto gOut = ugraph::graph_io::make_output<902>();

    auto g = ugraph::Graph(
        gIn.output<InType>() >> aNode.input<InType>(),
        aNode.output<OutType>() >> gOut.input<OutType>()
    );

    using manifest_t = typename decltype(g)::io_manifest;
    ugraph::Context<manifest_t> ctx;
    InType inVal { 100 };
    OutType outVal { 0 };

    ctx.template set_input_ptr<0, InType>(&inVal);
    ctx.template set_output_ptr<0, OutType>(&outVal);

    g.process(ctx);
    CHECK(outVal.value == 101);

    inVal.value = 200;
    g.process(ctx);
    CHECK(outVal.value == 201);
}