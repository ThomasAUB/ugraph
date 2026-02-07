#include "doctest.h"
#include "ugraph.hpp"
#include <sstream>
#include <string>
#include <iostream>
#include <vector>
// uMesh
/*
struct MyData1 {
    int mVal;
};
*/

using MyData1 = int;

using MyEvent = std::vector<int>;

struct Module1 {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 1, 1>
    >;

    void process(ugraph::NodeContext<Manifest>& ctx) {
        ctx.output<MyData1>(0) = ctx.input<MyData1>(0) + 1;
        std::cout << "module : " << ctx.output<MyData1>(0) << std::endl;
    }

};

struct Source {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 0, 1>,
        ugraph::IO<MyEvent, 0, 1>
    >;

    void process(ugraph::NodeContext<Manifest>& ctx) {

        ctx.output<MyData1>(0) = 1;
        //ctx.get<MyData1>().output<1>() = 14;

        std::cout << "source 0 : " << ctx.output<MyData1>(0) << std::endl;
        //std::cout << "source 1 : " << ctx.get<MyData1>().output<1>() << std::endl;

        ctx.output<MyEvent>(0).push_back(789);
    }

};

struct Sink {

    using Manifest = ugraph::Manifest<
        ugraph::IO<MyData1, 2, 0>,
        ugraph::IO<MyEvent, 1, 0>
    >;

    void process(ugraph::NodeContext<Manifest>& ctx) {

        int idx = 0;
        for (auto& in : ctx.inputs<MyData1>()) {
            std::cout << "sink " << idx++ << " : " << in << std::endl;
        }

        auto& vect = ctx.input<MyEvent>(0);

        if (vect.empty()) {
            std::cout << "event empty" << std::endl;
        }
        else {
            std::cout << "received event " << vect.back() << std::endl;
        }

    }

};

TEST_CASE("type name test") {

    Source src;
    Module1 m1;
    Sink sink;

    auto srcNode = ugraph::make_data_node<100>(src);
    auto m1Node = ugraph::make_data_node<101>(m1);
    auto sinkNode = ugraph::make_data_node<102>(sink);

    ugraph::DataGraph graph(
        srcNode.output<MyData1, 0>() >> m1Node.input<MyData1, 0>(),
        m1Node.output<MyData1, 0>() >> sinkNode.input<MyData1, 0>(),
        srcNode.output<MyData1, 0>() >> sinkNode.input<MyData1, 1>(),
        srcNode.output<MyEvent, 0>() >> sinkNode.input<MyEvent, 0>()
    );

    std::cout << "data1 " << graph.data_instance_count<MyData1>() << std::endl;
    std::cout << "events " << graph.data_instance_count<MyEvent>() << std::endl;

    graph.for_each(
        [] (auto& n, auto& ctx) {
            n.process(ctx);
        }
    );

}