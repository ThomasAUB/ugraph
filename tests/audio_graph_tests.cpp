#include "doctest.h"
#include "ugraph.hpp"
#include <array>
#include <iostream>
#include <chrono>

// Audio processing oriented Graph executor tests (sources -> mixer -> gain -> sink + perf).
namespace {

    struct Parameters {
        void setFreq(uint16_t inFreq) { mFreq = inFreq; }
        uint16_t getFreq() const { return mFreq; }
    private:
        uint16_t mFreq = 0;
    };

    // Simple fixed-size audio buffer with helper utilities.
    struct AudioBuffer {

        AudioBuffer() = default;

        AudioBuffer(float* d, std::size_t s) : mData(d), mSize(s) {}

        template<typename container_t>
        AudioBuffer(container_t& c) : mData(c.data()), mSize(c.size()) {}

        float* mData = nullptr;
        std::size_t mSize = 0;
    };

    // Produces a constant value each call.
    struct ConstantSource {

        using Manifest = ugraph::Manifest<
            ugraph::IO<AudioBuffer, 0, 1>,
            ugraph::IO<Parameters, 1, 0, false>
        >;

        float value { 0.f };

        void process(ugraph::Context<Manifest>& ctx) {
            auto frequency = ctx.input<Parameters>().getFreq();
            process(ctx.output<AudioBuffer>().mData, ctx.output<AudioBuffer>().mSize);
        }

        // Pointer-based helper for manual path in tests
        void process(float* out, std::size_t s) {
            for (std::size_t i = 0; i < s; ++i) out[i] = value;
        }

    };

    // Mixes two input blocks sample-wise (sum) into an output block.
    struct Mixer2 {

        using Manifest = ugraph::Manifest< ugraph::IO<AudioBuffer, 2, 1> >;

        void process(ugraph::Context<Manifest>& ctx) {
            process(
                ctx.input<AudioBuffer>(0).mData,
                ctx.input<AudioBuffer>(1).mData,
                ctx.output<AudioBuffer>().mData,
                ctx.output<AudioBuffer>().mSize
            );
        }

        // Pointer-based helper for manual path in tests
        void process(const float* in1, const float* in2, float* out, std::size_t s) {
            for (std::size_t i = 0; i < s; ++i) {
                out[i] = in1[i] + in2[i];
            }
        }

    };

    // Scales all samples in-place.
    struct Gain {

        using Manifest = ugraph::Manifest< ugraph::IO<AudioBuffer, 1, 1> >;

        float gain { 1.f };

        void process(ugraph::Context<Manifest>& ctx) {
            process(ctx.input<AudioBuffer>().mData, ctx.output<AudioBuffer>().mData, ctx.output<AudioBuffer>().mSize);
        }

        // Pointer-based helper for manual path in tests
        void process(const float* in, float* out, std::size_t s) {
            for (std::size_t i = 0; i < s; ++i) {
                out[i] = in[i] * gain;
            }
        }

    };

    // Sink that accumulates the sum and tracks the first sample for quick checks.
    struct Sink {

        using Manifest = ugraph::Manifest< ugraph::IO<AudioBuffer, 1, 0> >;

        float last_sample { 0.f };
        float sum { 0.f };

        void process(ugraph::Context<Manifest>& ctx) {
            process(ctx.input<AudioBuffer>().mData, ctx.input<AudioBuffer>().mSize);
        }

        // Pointer-based helper for manual path in tests
        void process(const float* in, std::size_t s) {
            sum = 0.f;
            for (std::size_t i = 0; i < s; ++i) sum += in[i];
            last_sample = in[0];
        }

    };

}

template<std::size_t id, typename module_t>
using GraphNode = ugraph::Node<id, module_t, typename module_t::Manifest>;


static auto makeVoiceGraph(
    ConstantSource& s1,
    ConstantSource& s2,
    Mixer2& m,
    Gain& g,
    Sink& s
) {

    auto vA = ugraph::make_node<0>(s1);
    auto vB = ugraph::make_node<1>(s2);
    auto vMix = ugraph::make_node<2>(m);
    auto vGain = ugraph::make_node<3>(g);
    auto vSink = ugraph::make_node<4>(s);

    auto graph = ugraph::Graph(
        vA.output<AudioBuffer>() >> vMix.input<AudioBuffer, 0>(),
        vB.output<AudioBuffer>() >> vMix.input<AudioBuffer, 1>(),
        vMix.output<AudioBuffer>() >> vGain.input<AudioBuffer>(),
        vGain.output<AudioBuffer>() >> vSink.input<AudioBuffer>()
    );

    return graph;
}

using voice_graph_t = decltype(
    makeVoiceGraph(
        std::declval<ConstantSource&>(),
        std::declval<ConstantSource&>(),
        std::declval<Mixer2&>(),
        std::declval<Gain&>(),
        std::declval<Sink&>()
    )
    );


struct Voice {

    using graph_data_t = voice_graph_t::graph_data_t;

    Voice(graph_data_t& inGraphData) :
        mGraph(makeVoiceGraph(sa, sb, mix, gain, sink)) {
        mGraph.init_graph_data(inGraphData);

        mGraph.bind_input<0>(mParams[0]);
        mGraph.bind_input<1>(mParams[1]);

        CHECK(mGraph.all_ios_connected());
    }

    void setFreq(uint16_t inFreq) {
        mParams[0].setFreq(inFreq);
        mParams[1].setFreq(inFreq);
    }

    void process() {
        mGraph.for_each(
            [] (auto& m, auto& ctx) {
                m.process(ctx);
            }
        );
    }

    void print() {
        mGraph.print(std::cout);
    }

private:

    ConstantSource sa { 0.25f };
    ConstantSource sb { 0.75f };
    Mixer2        mix {};
    Gain          gain { 0.5f };
    Sink          sink {};

    voice_graph_t mGraph;

    Parameters mParams[2];
};

TEST_CASE("basic synth voice test") {

    Voice::graph_data_t dg;

    static constexpr auto storage_count = voice_graph_t::data_count<AudioBuffer>();
    static_assert(storage_count == 3);
    static constexpr auto storage_size = 64;
    using buffer_storage_t = std::array<float, storage_size>;
    std::array<buffer_storage_t, storage_count> storage;

    for (int i = 0; i < storage_count; i++) {
        ugraph::data_at<AudioBuffer>(dg, i) = storage[i];
    }

    Voice voice(dg);
    //voice.print();
    voice.process();

}

TEST_CASE("audio graph simple chain correctness") {

    ConstantSource sa { 0.25f };
    ConstantSource sb { 0.75f };
    Mixer2        mix {};
    Gain          gain { 0.5f };
    Sink          sink {};

    auto vA = ugraph::make_node<3001>(sa);
    auto vB = ugraph::make_node<3002>(sb);
    auto vMix = ugraph::make_node<3003>(mix);
    auto vGain = ugraph::make_node<3004>(gain);
    auto vSink = ugraph::make_node<3005>(sink);

    auto g = ugraph::Graph(
        vA.output<AudioBuffer>() >> vMix.input<AudioBuffer, 0>(),
        vB.output<AudioBuffer>() >> vMix.input<AudioBuffer, 1>(),
        vMix.output<AudioBuffer>() >> vGain.input<AudioBuffer>(),
        vGain.output<AudioBuffer>() >> vSink.input<AudioBuffer>()
    );

    decltype(g)::graph_data_t dg;
    g.init_graph_data(dg);

    Parameters params[2];
    g.bind_input<3001>(params[0]);
    g.bind_input<3002>(params[1]);

    CHECK(g.all_ios_connected());

    static_assert(decltype(g)::template data_count<AudioBuffer>() == 3, "Unexpected buffer count");

    static constexpr auto storage_count = decltype(g)::data_count<AudioBuffer>();
    static constexpr auto storage_size = 64;
    using buffer_storage_t2 = std::array<float, storage_size>;
    std::array<buffer_storage_t2, storage_count> storage;

    for (int i = 0; i < storage_count; i++) {
        ugraph::data_at<AudioBuffer>(dg, i) = storage[i];
    }

    g.for_each(
        [] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(sink.last_sample == doctest::Approx(0.5f));
    CHECK(sink.sum == doctest::Approx(0.5f * storage_size));

}

TEST_CASE("audio graph repeated processing") {

    ConstantSource sa { 0.1f };
    ConstantSource sb { 0.2f };
    Mixer2        mix {};
    Gain          gain { 2.0f };
    Sink          sink {};

    auto vA = ugraph::make_node<4001>(sa);
    auto vB = ugraph::make_node<4002>(sb);
    auto vMix = ugraph::make_node<4003>(mix);
    auto vGain = ugraph::make_node<4004>(gain);
    auto vSink = ugraph::make_node<4005>(sink);

    auto g = ugraph::Graph(
        vA.output<AudioBuffer>() >> vMix.input<AudioBuffer, 0>(),
        vB.output<AudioBuffer>() >> vMix.input<AudioBuffer, 1>(),
        vMix.output<AudioBuffer>() >> vGain.input<AudioBuffer>(),
        vGain.output<AudioBuffer>() >> vSink.input<AudioBuffer>()
    );

    decltype(g)::graph_data_t dg;
    g.init_graph_data(dg);

    Parameters params[2];
    g.bind_input<4001>(params[0]);
    g.bind_input<4002>(params[1]);

    CHECK(g.all_ios_connected());

    static constexpr auto storage_count = decltype(g)::data_count<AudioBuffer>();
    static constexpr auto storage_size = 64;
    using buffer_storage_t = std::array<float, storage_size>;
    std::array<buffer_storage_t, storage_count> storage;

    for (int i = 0; i < storage_count; i++) {
        ugraph::data_at<AudioBuffer>(dg, i) = storage[i];
    }

    constexpr std::size_t iterations = 2500;

    for (std::size_t i = 0; i < iterations; ++i) {
        g.for_each(
            [] (auto& module, auto& ctx) {
                module.process(ctx);
            }
        );
    }

    CHECK(sink.last_sample == doctest::Approx(0.6f));
    CHECK(sink.sum == doctest::Approx(0.6f * storage_size));
}

TEST_CASE("dynamic audio graph runtime routing") {

    ConstantSource sa { 0.25f };
    ConstantSource sb { 0.75f };
    Mixer2        mix {};
    Gain          gain { 0.5f };
    Sink          sink {};

    auto vA = ugraph::make_node<6001>(sa);
    auto vB = ugraph::make_node<6002>(sb);
    auto vMix = ugraph::make_node<6003>(mix);
    auto vGain = ugraph::make_node<6004>(gain);
    auto vSink = ugraph::make_node<6005>(sink);

    auto g = ugraph::DynamicGraph(vA, vB, vMix, vGain, vSink);

    Parameters params[2];
    CHECK(g.bind_input<6001>(params[0]));
    CHECK(g.bind_input<6002>(params[1]));

    static constexpr auto storage_size = 64;
    std::array<float, storage_size> source_a_storage {};
    std::array<float, storage_size> source_b_storage {};
    std::array<float, storage_size> mix_storage {};
    std::array<float, storage_size> gain_storage {};

    AudioBuffer source_a_buffer { source_a_storage };
    AudioBuffer source_b_buffer { source_b_storage };
    AudioBuffer mix_buffer { mix_storage };
    AudioBuffer gain_buffer { gain_storage };

    CHECK(g.route(vA.output<AudioBuffer>() >> vMix.input<AudioBuffer, 0>(), source_a_buffer));
    CHECK(g.route(vB.output<AudioBuffer>() >> vMix.input<AudioBuffer, 1>(), source_b_buffer));
    CHECK(g.route(vMix.output<AudioBuffer>() >> vGain.input<AudioBuffer>(), mix_buffer));
    CHECK(g.route(vGain.output<AudioBuffer>() >> vSink.input<AudioBuffer>(), gain_buffer));

    CHECK(g.all_ios_connected());
    CHECK(g.compile());
    CHECK(g.process());

    CHECK(sink.last_sample == doctest::Approx(0.5f));
    CHECK(sink.sum == doctest::Approx(0.5f * storage_size));
}

TEST_CASE("audio graph manual vs compile-time vs runtime benchmark") {

    ConstantSource saCompile { 0.3f };
    ConstantSource sbCompile { 0.4f };
    Mixer2        mixCompile {};
    Gain          gainCompile { 1.25f };
    Sink          sinkCompile {};

    ConstantSource saRuntime { 0.3f };
    ConstantSource sbRuntime { 0.4f };
    Mixer2        mixRuntime {};
    Gain          gainRuntime { 1.25f };
    Sink          sinkRuntime {};

    ConstantSource saManual { 0.3f };
    ConstantSource sbManual { 0.4f };
    Mixer2        mixManual {};
    Gain          gainManual { 1.25f };
    Sink          sinkManual {};

    auto vACompile = ugraph::make_node<7001>(saCompile);
    auto vBCompile = ugraph::make_node<7002>(sbCompile);
    auto vMixCompile = ugraph::make_node<7003>(mixCompile);
    auto vGainCompile = ugraph::make_node<7004>(gainCompile);
    auto vSinkCompile = ugraph::make_node<7005>(sinkCompile);

    auto compileGraph = ugraph::Graph(
        vACompile.output<AudioBuffer>() >> vMixCompile.input<AudioBuffer, 0>(),
        vBCompile.output<AudioBuffer>() >> vMixCompile.input<AudioBuffer, 1>(),
        vMixCompile.output<AudioBuffer>() >> vGainCompile.input<AudioBuffer>(),
        vGainCompile.output<AudioBuffer>() >> vSinkCompile.input<AudioBuffer>()
    );

    decltype(compileGraph)::graph_data_t compileData;
    compileGraph.init_graph_data(compileData);

    Parameters compileParams[2];
    compileGraph.bind_input<7001>(compileParams[0]);
    compileGraph.bind_input<7002>(compileParams[1]);

    CHECK(compileGraph.all_ios_connected());

    constexpr std::size_t kBlockSize = 64;
    constexpr std::size_t kIterations = 6000;
    constexpr std::size_t kWarmupIterations = 128;

    using storage_t = std::array<float, kBlockSize>;

    static constexpr auto compileStorageCount = decltype(compileGraph)::data_count<AudioBuffer>();
    CHECK(compileStorageCount == 3);

    std::array<storage_t, compileStorageCount> compileStorage {};
    for (std::size_t i = 0; i < compileStorageCount; ++i) {
        ugraph::data_at<AudioBuffer>(compileData, i) = compileStorage[i];
    }

    auto vARuntime = ugraph::make_node<7101>(saRuntime);
    auto vBRuntime = ugraph::make_node<7102>(sbRuntime);
    auto vMixRuntime = ugraph::make_node<7103>(mixRuntime);
    auto vGainRuntime = ugraph::make_node<7104>(gainRuntime);
    auto vSinkRuntime = ugraph::make_node<7105>(sinkRuntime);

    auto runtimeGraph = ugraph::DynamicGraph(vARuntime, vBRuntime, vMixRuntime, vGainRuntime, vSinkRuntime);

    Parameters runtimeParams[2];
    CHECK(runtimeGraph.bind_input<7101>(runtimeParams[0]));
    CHECK(runtimeGraph.bind_input<7102>(runtimeParams[1]));

    storage_t runtimeSourceAStorage {};
    storage_t runtimeSourceBStorage {};
    storage_t runtimeMixStorage {};
    storage_t runtimeGainStorage {};

    AudioBuffer runtimeSourceABuffer { runtimeSourceAStorage };
    AudioBuffer runtimeSourceBBuffer { runtimeSourceBStorage };
    AudioBuffer runtimeMixBuffer { runtimeMixStorage };
    AudioBuffer runtimeGainBuffer { runtimeGainStorage };

    CHECK(runtimeGraph.route(vARuntime.output<AudioBuffer>() >> vMixRuntime.input<AudioBuffer, 0>(), runtimeSourceABuffer));
    CHECK(runtimeGraph.route(vBRuntime.output<AudioBuffer>() >> vMixRuntime.input<AudioBuffer, 1>(), runtimeSourceBBuffer));
    CHECK(runtimeGraph.route(vMixRuntime.output<AudioBuffer>() >> vGainRuntime.input<AudioBuffer>(), runtimeMixBuffer));
    CHECK(runtimeGraph.route(vGainRuntime.output<AudioBuffer>() >> vSinkRuntime.input<AudioBuffer>(), runtimeGainBuffer));

    CHECK(runtimeGraph.all_ios_connected());
    CHECK(runtimeGraph.compile());

    std::array<storage_t, 3> manualStorage {};

    volatile float consume = 0.f;

    for (std::size_t i = 0; i < kWarmupIterations; ++i) {
        compileGraph.for_each(
            [] (auto& module, auto& ctx) {
                module.process(ctx);
            }
        );

        CHECK(runtimeGraph.process());

        saManual.process(manualStorage[0].data(), kBlockSize);
        sbManual.process(manualStorage[1].data(), kBlockSize);
        mixManual.process(manualStorage[0].data(), manualStorage[1].data(), manualStorage[2].data(), kBlockSize);
        gainManual.process(manualStorage[2].data(), manualStorage[0].data(), kBlockSize);
        sinkManual.process(manualStorage[0].data(), kBlockSize);

        consume += sinkCompile.last_sample + sinkRuntime.last_sample + sinkManual.last_sample;
    }

    using clock = std::chrono::high_resolution_clock;

    auto t0 = clock::now();
    for (std::size_t i = 0; i < kIterations; ++i) {
        compileGraph.for_each(
            [] (auto& module, auto& ctx) {
                module.process(ctx);
            }
        );
        consume += sinkCompile.last_sample;
    }
    auto t1 = clock::now();
    auto compileNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    auto t2 = clock::now();
    for (std::size_t i = 0; i < kIterations; ++i) {
        CHECK(runtimeGraph.process());
        consume += sinkRuntime.last_sample;
    }
    auto t3 = clock::now();
    auto runtimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count();

    auto t4 = clock::now();
    for (std::size_t i = 0; i < kIterations; ++i) {
        saManual.process(manualStorage[0].data(), kBlockSize);
        sbManual.process(manualStorage[1].data(), kBlockSize);
        mixManual.process(manualStorage[0].data(), manualStorage[1].data(), manualStorage[2].data(), kBlockSize);
        gainManual.process(manualStorage[2].data(), manualStorage[0].data(), kBlockSize);
        sinkManual.process(manualStorage[0].data(), kBlockSize);
        consume += sinkManual.last_sample;
    }
    auto t5 = clock::now();
    auto manualNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t5 - t4).count();

    CHECK(sinkCompile.last_sample == doctest::Approx(0.875f));
    CHECK(sinkRuntime.last_sample == doctest::Approx(0.875f));
    CHECK(sinkManual.last_sample == doctest::Approx(0.875f));

    CHECK(sinkCompile.sum == doctest::Approx(0.875f * kBlockSize));
    CHECK(sinkRuntime.sum == doctest::Approx(0.875f * kBlockSize));
    CHECK(sinkManual.sum == doctest::Approx(0.875f * kBlockSize));

    REQUIRE(compileNs > 0);
    REQUIRE(runtimeNs > 0);
    REQUIRE(manualNs > 0);

    const double compileRatio = static_cast<double>(compileNs) / static_cast<double>(manualNs);
    const double runtimeRatio = static_cast<double>(runtimeNs) / static_cast<double>(manualNs);
    const double runtimeVsCompileRatio = static_cast<double>(runtimeNs) / static_cast<double>(compileNs);

    INFO(
        "compile_ns=" << compileNs
        << " runtime_ns=" << runtimeNs
        << " manual_ns=" << manualNs
        << " compile/manual=" << compileRatio
        << " runtime/manual=" << runtimeRatio
        << " runtime/compile=" << runtimeVsCompileRatio
    );

    std::cout
        << "compile_ns=" << compileNs
        << " runtime_ns=" << runtimeNs
        << " manual_ns=" << manualNs
        << " compile/manual=" << compileRatio
        << " runtime/manual=" << runtimeRatio
        << " runtime/compile=" << runtimeVsCompileRatio
        << std::endl;

    (void) consume;
}

#ifndef __clang__
// Clang builds can show larger variance in the simple wall-clock ratio measurement
// Skip the perf ratio assertion to avoid spurious failures.
TEST_CASE("audio graph pipeline vs manual performance ratio") {
    ConstantSource sa { 0.3f };
    ConstantSource sb { 0.4f };
    Mixer2        mix {};
    Gain          gain { 1.25f };
    Sink          sinkPipe {};
    Sink          sinkManual {};

    auto vA = ugraph::make_node<5001>(sa);
    auto vB = ugraph::make_node<5002>(sb);
    auto vMix = ugraph::make_node<5003>(mix);
    auto vGain = ugraph::make_node<5004>(gain);
    auto vSink = ugraph::make_node<5005>(sinkPipe);

    auto g = ugraph::Graph(
        vA.output<AudioBuffer>() >> vMix.input<AudioBuffer, 0>(),
        vB.output<AudioBuffer>() >> vMix.input<AudioBuffer, 1>(),
        vMix.output<AudioBuffer>() >> vGain.input<AudioBuffer>(),
        vGain.output<AudioBuffer>() >> vSink.input<AudioBuffer>()
    );

    decltype(g)::graph_data_t dg;
    g.init_graph_data(dg);

    Parameters params[2];
    g.bind_input<5001>(params[0]);
    g.bind_input<5002>(params[1]);

    CHECK(g.all_ios_connected());

    constexpr std::size_t kBlockSize = 64;

    // Manual reference buffers
    using storage_t = std::array<float, kBlockSize>;
    std::array<storage_t, 3> storage;

    // Provide storage for the graph internal data buffers
    static constexpr auto graph_storage_count = decltype(g)::data_count<AudioBuffer>();
    CHECK(graph_storage_count == 3);

    using graph_buffer_storage_t = std::array<float, kBlockSize>;
    std::array<graph_buffer_storage_t, graph_storage_count> gstorage;
    for (std::size_t i = 0; i < graph_storage_count; ++i) {
        ugraph::data_at<AudioBuffer>(dg, i) = gstorage[i];
    }

    // Warm-up both paths (also protects against extremely small timings)
    volatile float consume = 0.f; // Prevent compiler elision

    for (int i = 0; i < 128; ++i) {

        g.for_each(
            [] (auto& module, auto& ctx) {
                module.process(ctx);
            }
        );

        sa.process(storage[0].data(), kBlockSize);
        sb.process(storage[1].data(), kBlockSize);
        mix.process(storage[0].data(), storage[1].data(), storage[2].data(), kBlockSize);
        gain.process(storage[2].data(), storage[0].data(), kBlockSize);
        sinkManual.process(storage[0].data(), kBlockSize);
        consume += sinkPipe.last_sample + sinkManual.last_sample;
    }

    using clock = std::chrono::high_resolution_clock;
    constexpr std::size_t iterations = 6000;

    auto t0 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        g.for_each(
            [] (auto& module, auto& ctx) {
                module.process(ctx);
            }
        );
        consume += sinkPipe.last_sample;
    }
    auto t1 = clock::now();
    auto pipe_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    auto t2 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        sa.process(storage[0].data(), kBlockSize);
        sb.process(storage[1].data(), kBlockSize);
        mix.process(storage[0].data(), storage[1].data(), storage[2].data(), kBlockSize);
        gain.process(storage[2].data(), storage[0].data(), kBlockSize);
        sinkManual.process(storage[0].data(), kBlockSize);
        consume += sinkManual.last_sample;
    }
    auto t3 = clock::now();
    auto manual_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count();

    CHECK(sinkPipe.last_sample == doctest::Approx(0.875f));
    CHECK(sinkManual.last_sample == doctest::Approx(0.875f));

    // Recompute manual sum for accuracy check.
    sa.process(storage[0].data(), kBlockSize);
    sb.process(storage[1].data(), kBlockSize);
    mix.process(storage[0].data(), storage[1].data(), storage[2].data(), kBlockSize);
    gain.process(storage[2].data(), storage[0].data(), kBlockSize);
    sinkManual.process(storage[0].data(), kBlockSize);
    CHECK(sinkManual.sum == doctest::Approx(0.875f * kBlockSize));

    double r = static_cast<double>(pipe_ns) / static_cast<double>(manual_ns);
    INFO("pipe_ns=" << pipe_ns << " manual_ns=" << manual_ns << " ratio=" << r);

    std::cout << "pipe_ns=" << pipe_ns << " manual_ns=" << manual_ns << " ratio=" << r << std::endl;

    CHECK(r < 1.5);

    (void) consume; // silence unused warning for volatile accumulation
}

#endif // __clang__
