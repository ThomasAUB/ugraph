#include "doctest.h"
#include "ugraph.hpp"
#include <sstream>

struct Add {

    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 1, 1>
    >;

    void process(ugraph::Context<Manifest>& ctx) {
        ctx.output<int>() = ctx.input<int>() + 1;
    }

};

struct OptionalOutput {

    using Manifest = ugraph::Manifest<
        ugraph::IO<int, 1, 1, false>
    >;

    void process(ugraph::Context<Manifest>& ctx) {
        mInternal = ctx.input<int>();
        const bool isConnected = ctx.has_output<int>();
        CHECK(!isConnected);
        if (isConnected) {
            ctx.output<int>() = mInternal + 1;
        }
    }

    int mInternal = 0;
};

TEST_CASE("bind_graph_data external") {

    Add entry1, entry2, end1, end2;

    auto entry1Node = ugraph::make_node<100>(entry1);
    auto entry2Node = ugraph::make_node<103>(entry2);
    auto end1Node = ugraph::make_node<101>(end1);
    auto end2Node = ugraph::make_node<102>(end2);

    int a;
    int b;

    ugraph::Graph graph(

        a | entry1Node.input<int>()
        , a | entry2Node.input<int>()
        , entry1Node.output<int>() >> end1Node.input<int>()
        , entry2Node.output<int>() >> end2Node.input<int>()
        , end1Node.output<int>() | b
        , end2Node.output<int>() | a

    );

    a = 5;
    b = 0;

    graph.for_each(
        [&] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(a == 7);
    CHECK(b == 7);

}

TEST_CASE("strict graph accepts fully bound external data") {

    Add entry;
    OptionalOutput exit;

    auto entryNode = ugraph::make_node<200>(entry);
    auto exitNode = ugraph::make_node<201>(exit);

    int in = 5;
    int out = 0;

    ugraph::Graph graph(
        in | entryNode.input<int>()
        , entryNode.output<int>() >> exitNode.input<int>()
        //, exitNode.output<int>() | out /* ommit one connection */
    );

    graph.for_each(
        [&] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(out == 0);
    CHECK(exit.mInternal == 6);
}

TEST_CASE("external data graph member rebinds external data after construction from temporary") {

    struct Pass {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct Holder {
        using entry_node_t = decltype(ugraph::make_node<300>(std::declval<Pass&>()));
        using end_node_t = decltype(ugraph::make_node<301>(std::declval<Pass&>()));
        using input_edge_t = ugraph::InDataBind<int, typename entry_node_t::template InputPort<int, 0>>;
        using internal_edge_t = std::pair<
            typename entry_node_t::template OutputPort<int, 0>,
            typename end_node_t::template InputPort<int, 0>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename end_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::Graph<input_edge_t, internal_edge_t, output_edge_t>;

        static graph_t makeGraph(int& in, int& out, Pass& entry, Pass& end) {
            auto entryNode = ugraph::make_node<300>(entry);
            auto endNode = ugraph::make_node<301>(end);
            return graph_t(
                in | entryNode.input<int>(),
                entryNode.output<int>() >> endNode.input<int>(),
                endNode.output<int>() | out
            );
        }

        Holder() : graph(makeGraph(in, out, entry, end)) {}

        int in = 17;
        int out = 0;
        Pass entry;
        Pass end;
        graph_t graph;
    };

    Holder holder;

    CHECK(holder.graph.template binding_ptr<Holder::input_edge_t>() == static_cast<const void*>(&holder.in));
    CHECK(holder.graph.template binding_ptr<Holder::output_edge_t>() == static_cast<const void*>(&holder.out));

    holder.graph.for_each(
        [] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(holder.out == 17);
}

TEST_CASE("external data graph member keeps external bindings after construction from temporary") {

    struct Pass {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct Holder {
        using entry_node_t = decltype(ugraph::make_node<320>(std::declval<Pass&>()));
        using end_node_t = decltype(ugraph::make_node<321>(std::declval<Pass&>()));
        using input_edge_t = ugraph::InDataBind<int, typename entry_node_t::template InputPort<int, 0>>;
        using internal_edge_t = std::pair<
            typename entry_node_t::template OutputPort<int, 0>,
            typename end_node_t::template InputPort<int, 0>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename end_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::ExternalDataGraph<input_edge_t, internal_edge_t, output_edge_t>;

        static graph_t makeGraph(int& in, int& out, Pass& entry, Pass& end) {
            auto entryNode = ugraph::make_node<320>(entry);
            auto endNode = ugraph::make_node<321>(end);
            return graph_t(
                in | entryNode.input<int>(),
                entryNode.output<int>() >> endNode.input<int>(),
                endNode.output<int>() | out
            );
        }

        Holder() : graph(makeGraph(in, out, entry, end)) {
            graph.init(data);
        }

        int in = 23;
        int out = 0;
        Pass entry;
        Pass end;
        graph_t::graph_data_t data;
        graph_t graph;
    };

    Holder holder;

    CHECK(holder.graph.template binding_ptr<Holder::input_edge_t>() == static_cast<const void*>(&holder.in));
    CHECK(holder.graph.template binding_ptr<Holder::output_edge_t>() == static_cast<const void*>(&holder.out));

    holder.graph.for_each(
        [] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(holder.out == 23);
}

TEST_CASE("external data graph member keeps repeated external bindings to the same object") {

    struct Pass {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct Sum {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 2, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>(0) + ctx.input<int>(1);
        }
    };

    struct Holder {
        using pass_node_t = decltype(ugraph::make_node<400>(std::declval<Pass&>()));
        using sum_node_t = decltype(ugraph::make_node<401>(std::declval<Sum&>()));

        using pass_input_edge_t = ugraph::InDataBind<int, typename pass_node_t::template InputPort<int, 0>>;
        using sum_input_edge_t = ugraph::InDataBind<int, typename sum_node_t::template InputPort<int, 0>>;
        using internal_edge_t = std::pair<
            typename pass_node_t::template OutputPort<int, 0>,
            typename sum_node_t::template InputPort<int, 1>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename sum_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::ExternalDataGraph<pass_input_edge_t, sum_input_edge_t, internal_edge_t, output_edge_t>;

        static graph_t makeGraph(int& in, int& out, Pass& pass, Sum& sum) {
            auto passNode = ugraph::make_node<400>(pass);
            auto sumNode = ugraph::make_node<401>(sum);

            return graph_t(
                in | passNode.input<int>(),
                in | sumNode.input<int, 0>(),
                passNode.output<int>() >> sumNode.input<int, 1>(),
                sumNode.output<int>() | out
            );
        }

        Holder() : graph(makeGraph(in, out, pass, sum)) {
            graph.init(graphData);
        }

        int in = 9;
        int out = 0;
        Pass pass;
        Sum sum;
        graph_t::graph_data_t graphData;
        graph_t graph;
    };

    Holder holder;

    CHECK(holder.graph.template binding_ptr<Holder::pass_input_edge_t>() == static_cast<const void*>(&holder.in));
    CHECK(holder.graph.template binding_ptr<Holder::sum_input_edge_t>() == static_cast<const void*>(&holder.in));
    CHECK(holder.graph.template binding_ptr<Holder::output_edge_t>() == static_cast<const void*>(&holder.out));

    holder.graph.for_each(
        [] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(holder.out == 18);
}

TEST_CASE("module with inner external data graph keeps bindings to its own members") {

    struct InnerSource {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct InnerSink {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>() + 1;
        }
    };

    struct NestedModule {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        using source_node_t = decltype(ugraph::make_node<500>(std::declval<InnerSource&>()));
        using sink_node_t = decltype(ugraph::make_node<501>(std::declval<InnerSink&>()));
        using input_edge_t = ugraph::InDataBind<int, typename source_node_t::template InputPort<int, 0>>;
        using internal_edge_t = std::pair<
            typename source_node_t::template OutputPort<int, 0>,
            typename sink_node_t::template InputPort<int, 0>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename sink_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::ExternalDataGraph<input_edge_t, internal_edge_t, output_edge_t>;

        static graph_t makeGraph(int& inputValue, int& outputValue, InnerSource& source, InnerSink& sink) {
            auto sourceNode = ugraph::make_node<500>(source);
            auto sinkNode = ugraph::make_node<501>(sink);
            return graph_t(
                inputValue | sourceNode.input<int>(),
                sourceNode.output<int>() >> sinkNode.input<int>(),
                sinkNode.output<int>() | outputValue
            );
        }

        void process(ugraph::Context<Manifest>& ctx) {
            outputValue = ctx.output<int>();
            inputValue = ctx.input<int>();
            graph.for_each(
                [] (auto& module, auto& graphCtx) {
                    module.process(graphCtx);
                }
            );
            ctx.output<int>() = outputValue;
        }

        InnerSource source;
        InnerSink sink;
        int inputValue = 0;
        int outputValue = 0;
        graph_t::graph_data_t graphData;
        graph_t graph = makeGraph(inputValue, outputValue, source, sink);

        NestedModule() {
            graph.init(graphData);
        }
    };

    NestedModule nested;

    CHECK(nested.graph.template binding_ptr<NestedModule::input_edge_t>() == static_cast<const void*>(&nested.inputValue));
    CHECK(nested.graph.template binding_ptr<NestedModule::output_edge_t>() == static_cast<const void*>(&nested.outputValue));

    int in = 41;
    int out = 0;
    ugraph::Context<NestedModule::Manifest> ctx;
    ctx.template set_input_ptr<0, int>(&in);
    ctx.template set_output_ptr<0, int>(&out);

    nested.process(ctx);

    CHECK(out == 42);
}

TEST_CASE("graph output can fan out to several module inputs") {

    struct Source {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct Sink {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 0>>;

        void process(ugraph::Context<Manifest>& ctx) {
            value = ctx.input<int>() + 1;
        }

        int value = 0;
    };

    using source_node_t = decltype(ugraph::make_node<525>(std::declval<Source&>()));
    using sink_node_t = decltype(ugraph::make_node<526>(std::declval<Sink&>()));

    Source source;
    Sink sink1;
    Sink sink2;
    int input = 7;

    auto sourceNode = ugraph::make_node<525>(source);
    auto sinkNode1 = ugraph::make_node<526>(sink1);
    auto sinkNode2 = ugraph::make_node<527>(sink2);

    ugraph::Graph graph(
        input | sourceNode.input<int>(),
        sourceNode.output<int>() >> sinkNode1.input<int>(),
        sourceNode.output<int>() >> sinkNode2.input<int>()
    );

    graph.for_each(
        [] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(sink1.value == 8);
    CHECK(sink2.value == 8);
}

TEST_CASE("std array construction preserves nested external graph bindings") {

    struct InnerSource {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct InnerSink {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>() + 1;
        }
    };

    struct NestedModule {
        using source_node_t = decltype(ugraph::make_node<530>(std::declval<InnerSource&>()));
        using sink_node_t = decltype(ugraph::make_node<531>(std::declval<InnerSink&>()));
        using input_edge_t = ugraph::InDataBind<int, typename source_node_t::template InputPort<int, 0>>;
        using internal_edge_t = std::pair<
            typename source_node_t::template OutputPort<int, 0>,
            typename sink_node_t::template InputPort<int, 0>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename sink_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::ExternalDataGraph<input_edge_t, internal_edge_t, output_edge_t>;

        static graph_t makeGraph(int& inputValue, int& outputValue, InnerSource& source, InnerSink& sink) {
            auto sourceNode = ugraph::make_node<530>(source);
            auto sinkNode = ugraph::make_node<531>(sink);
            return graph_t(
                inputValue | sourceNode.input<int>(),
                sourceNode.output<int>() >> sinkNode.input<int>(),
                sinkNode.output<int>() | outputValue
            );
        }

        NestedModule() {
            graph.init(graphData);
        }

        InnerSource source;
        InnerSink sink;
        int inputValue = 0;
        int outputValue = 0;
        graph_t::graph_data_t graphData;
        graph_t graph = makeGraph(inputValue, outputValue, source, sink);
    };

    std::array<NestedModule, 4> modules;

    for (auto& module : modules) {
        CHECK(module.graph.template binding_ptr<NestedModule::input_edge_t>() == static_cast<const void*>(&module.inputValue));
        CHECK(module.graph.template binding_ptr<NestedModule::output_edge_t>() == static_cast<const void*>(&module.outputValue));
    }
}

TEST_CASE("printing a nested external graph does not break later processing") {

    struct InnerSource {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct InnerSink {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>() + 1;
        }
    };

    struct NestedModule {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;
        using source_node_t = decltype(ugraph::make_node<540>(std::declval<InnerSource&>()));
        using sink_node_t = decltype(ugraph::make_node<541>(std::declval<InnerSink&>()));
        using input_edge_t = ugraph::InDataBind<int, typename source_node_t::template InputPort<int, 0>>;
        using internal_edge_t = std::pair<
            typename source_node_t::template OutputPort<int, 0>,
            typename sink_node_t::template InputPort<int, 0>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename sink_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::ExternalDataGraph<input_edge_t, internal_edge_t, output_edge_t>;

        static graph_t makeGraph(int& inputValue, int& outputValue, InnerSource& source, InnerSink& sink) {
            auto sourceNode = ugraph::make_node<540>(source);
            auto sinkNode = ugraph::make_node<541>(sink);
            return graph_t(
                inputValue | sourceNode.input<int>(),
                sourceNode.output<int>() >> sinkNode.input<int>(),
                sinkNode.output<int>() | outputValue
            );
        }

        NestedModule() {
            graph.init(graphData);
        }

        void process(ugraph::Context<Manifest>& ctx) {
            outputValue = ctx.output<int>();
            inputValue = ctx.input<int>();
            graph.for_each(
                [] (auto& module, auto& graphCtx) {
                    module.process(graphCtx);
                }
            );
            ctx.output<int>() = outputValue;
        }

        InnerSource source;
        InnerSink sink;
        int inputValue = 0;
        int outputValue = 0;
        graph_t::graph_data_t graphData;
        graph_t graph = makeGraph(inputValue, outputValue, source, sink);
    };

    NestedModule nested;
    std::ostringstream oss;
    nested.graph.print(oss);

    int in = 6;
    int out = 0;
    ugraph::Context<NestedModule::Manifest> ctx;
    ctx.template set_input_ptr<0, int>(&in);
    ctx.template set_output_ptr<0, int>(&out);

    nested.process(ctx);

    CHECK(out == 7);
}

TEST_CASE("outer graph can run a module that owns an inner external data graph") {

    struct InnerSource {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct InnerSink {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>() + 1;
        }
    };

    struct NestedModule {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 1>>;
        using source_node_t = decltype(ugraph::make_node<550>(std::declval<InnerSource&>()));
        using sink_node_t = decltype(ugraph::make_node<551>(std::declval<InnerSink&>()));
        using input_edge_t = ugraph::InDataBind<int, typename source_node_t::template InputPort<int, 0>>;
        using internal_edge_t = std::pair<
            typename source_node_t::template OutputPort<int, 0>,
            typename sink_node_t::template InputPort<int, 0>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename sink_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::ExternalDataGraph<input_edge_t, internal_edge_t, output_edge_t>;

        static graph_t makeGraph(int& inputValue, int& outputValue, InnerSource& source, InnerSink& sink) {
            auto sourceNode = ugraph::make_node<550>(source);
            auto sinkNode = ugraph::make_node<551>(sink);
            return graph_t(
                inputValue | sourceNode.input<int>(),
                sourceNode.output<int>() >> sinkNode.input<int>(),
                sinkNode.output<int>() | outputValue
            );
        }

        graph_t& getGraph() {
            return graph;
        }

        void process(ugraph::Context<Manifest>& ctx) {
            outputValue = ctx.output<int>();
            inputValue = ctx.input<int>();
            graph.for_each(
                [] (auto& module, auto& graphCtx) {
                    module.process(graphCtx);
                }
            );
            ctx.output<int>() = outputValue;
        }

        InnerSource source;
        InnerSink sink;
        int inputValue = 0;
        int outputValue = 0;
        graph_t graph = makeGraph(inputValue, outputValue, source, sink);
    };

    struct OuterSink {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 0>>;

        void process(ugraph::Context<Manifest>& ctx) {
            value = ctx.input<int>();
        }

        int value = 0;
    };

    NestedModule nested;
    NestedModule::graph_t::graph_data_t nestedData;
    nested.getGraph().init(nestedData);

    OuterSink sink;

    auto node = ugraph::make_node<560>(nested);
    auto sinkNode = ugraph::make_node<561>(sink);
    int in = 8;

    ugraph::Graph outer(
        in | node.input<int>(),
        node.output<int>() >> sinkNode.input<int>()
    );

    outer.for_each(
        [] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(sink.value == 9);
}


TEST_CASE("module with fanout external input and mixed inner edge types stays valid") {

    struct CopyInt {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>();
        }
    };

    struct ToFloat {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 0>,
            ugraph::IO<float, 0, 1>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<float>() = static_cast<float>(ctx.input<int>() + 0.5f);
        }
    };

    struct Mix {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>,
            ugraph::IO<float, 1, 0>
        >;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<int>() = ctx.input<int>() + static_cast<int>(ctx.input<float>());
        }
    };

    struct NestedModule {
        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 1, 1>
        >;

        using copy_node_t = decltype(ugraph::make_node<520>(std::declval<CopyInt&>()));
        using float_node_t = decltype(ugraph::make_node<521>(std::declval<ToFloat&>()));
        using mix_node_t = decltype(ugraph::make_node<522>(std::declval<Mix&>()));

        using copy_input_edge_t = ugraph::InDataBind<int, typename copy_node_t::template InputPort<int, 0>>;
        using float_input_edge_t = ugraph::InDataBind<int, typename float_node_t::template InputPort<int, 0>>;
        using int_internal_edge_t = std::pair<
            typename copy_node_t::template OutputPort<int, 0>,
            typename mix_node_t::template InputPort<int, 0>
        >;
        using float_internal_edge_t = std::pair<
            typename float_node_t::template OutputPort<float, 0>,
            typename mix_node_t::template InputPort<float, 0>
        >;
        using output_edge_t = ugraph::OutDataBind<int, typename mix_node_t::template OutputPort<int, 0>>;
        using graph_t = ugraph::ExternalDataGraph<
            copy_input_edge_t,
            float_input_edge_t,
            int_internal_edge_t,
            float_internal_edge_t,
            output_edge_t
        >;

        static graph_t makeGraph(int& inputValue, int& outputValue, CopyInt& copy, ToFloat& toFloat, Mix& mix) {
            auto copyNode = ugraph::make_node<520>(copy);
            auto floatNode = ugraph::make_node<521>(toFloat);
            auto mixNode = ugraph::make_node<522>(mix);
            return graph_t(
                inputValue | copyNode.input<int>(),
                inputValue | floatNode.input<int>(),
                copyNode.output<int>() >> mixNode.input<int>(),
                floatNode.output<float>() >> mixNode.input<float>(),
                mixNode.output<int>() | outputValue
            );
        }

        NestedModule() {
            graph.init(graphData);
        }

        void process(ugraph::Context<Manifest>& ctx) {
            outputValue = ctx.output<int>();
            inputValue = ctx.input<int>();
            graph.for_each(
                [] (auto& module, auto& graphCtx) {
                    module.process(graphCtx);
                }
            );
            ctx.output<int>() = outputValue;
        }

        CopyInt copy;
        ToFloat toFloat;
        Mix mix;
        int inputValue = 0;
        int outputValue = 0;
        graph_t::graph_data_t graphData;
        graph_t graph = makeGraph(inputValue, outputValue, copy, toFloat, mix);
    };

    NestedModule nested;

    CHECK(nested.graph.template binding_ptr<NestedModule::copy_input_edge_t>() == static_cast<const void*>(&nested.inputValue));
    CHECK(nested.graph.template binding_ptr<NestedModule::float_input_edge_t>() == static_cast<const void*>(&nested.inputValue));
    CHECK(nested.graph.template binding_ptr<NestedModule::output_edge_t>() == static_cast<const void*>(&nested.outputValue));

    int in = 10;
    int out = 0;
    ugraph::Context<NestedModule::Manifest> ctx;
    ctx.template set_input_ptr<0, int>(&in);
    ctx.template set_output_ptr<0, int>(&out);

    nested.process(ctx);

    CHECK(out == 20);
}