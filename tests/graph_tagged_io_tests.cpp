#include "doctest.h"
#include "ugraph.hpp"

TEST_CASE("Graph routes data using TaggedIO tags") {

    struct TagA {};
    struct TagB {};

    struct ProducerA {
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<TagA, int, 0, 1>>;
        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<TagA>() = 10;
        }
    };

    struct ProducerB {
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<TagB, int, 0, 1>>;
        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<TagB>() = 20;
        }
    };

    struct Consumer {

        using Manifest = ugraph::Manifest<
            ugraph::IO<int, 2, 1>
        >;

        int a = -1;
        int b = -1;
        void process(ugraph::Context<Manifest>& ctx) {
            a = ctx.input<int>(0);
            b = ctx.input<int>(1);
            ctx.output<int>() = 42;
        }
    };

    ProducerA pa;
    ProducerB pb;
    Consumer c;

    auto nA = ugraph::make_node<10>(pa);
    auto nB = ugraph::make_node<11>(pb);
    auto nC = ugraph::make_node<12>(c);

    int out;

    ugraph::Graph g(
        nA.output<TagA, 0>() >> nC.input<int, 0>(),
        nB.output<TagB>() >> nC.input<int, 1>(),
        nC.output<int>() | out
    );

    using g_t = decltype(g);

    g.for_each([] (auto& n, auto& ctx) { n.process(ctx); });

    CHECK(c.a == 10);
    CHECK(c.b == 20);
    CHECK(out == 42);
}

TEST_CASE("TaggedIO can bind directly to plain values") {

    struct TagA {};

    struct Injector {
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<TagA, int, 1, 1>>;

        void process(ugraph::Context<Manifest>& ctx) {
            ctx.output<TagA>() = ctx.input<TagA>() + 1;
        }
    };

    struct Consumer {
        using Manifest = ugraph::Manifest<ugraph::IO<int, 1, 0>>;

        int value = 0;

        void process(ugraph::Context<Manifest>& ctx) {
            value = ctx.input<int>();
        }
    };

    Injector injector;
    Consumer consumer;
    auto source = ugraph::make_node<20>(injector);
    auto sink = ugraph::make_node<21>(consumer);

    int in = 32;

    ugraph::Graph g(
        in | source.input<TagA>(),
        source.output<TagA>() >> sink.input<int>()
    );

    g.for_each([] (auto& n, auto& ctx) { n.process(ctx); });

    CHECK(consumer.value == 33);
}
