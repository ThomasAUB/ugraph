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
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<TagA, int, 1, 0>, ugraph::TaggedIO<TagB, int, 1, 0>>;
        int a = -1;
        int b = -1;
        void process(ugraph::Context<Manifest>& ctx) {
            a = ctx.input<TagA>();
            b = ctx.input<TagB>();
        }
    };

    ProducerA pa;
    ProducerB pb;
    Consumer c;

    auto nA = ugraph::make_node<10>(pa);
    auto nB = ugraph::make_node<11>(pb);
    auto nC = ugraph::make_node<12>(c);

    ugraph::Graph g(
        nA.output<TagA>() >> nC.input<TagA>(),
        nB.output<TagB>() >> nC.input<TagB>()
    );

    using g_t = decltype(g);

    g.for_each([] (auto& n, auto& ctx) { n.process(ctx); });

    CHECK(c.a == 10);
    CHECK(c.b == 20);

}
