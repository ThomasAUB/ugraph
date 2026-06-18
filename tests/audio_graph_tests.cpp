#include "doctest.h"
#include "ugraph.hpp"
#include <array>
#include <iostream>
#include <chrono>

// Audio processing oriented Graph executor tests (sources -> mixer -> gain -> sink + perf).
namespace {

    struct ParametersTag {};

    // Size (in samples) of each audio processing block. Kept at namespace scope
    // so the modules can use it as a compile-time loop bound, which lets the
    // optimizer unroll and vectorize the per-sample work even when the buffers
    // are reached through the graph Context's pointer indirection.
    constexpr std::size_t kBlockSize = 64;

    // Fixed-size audio block: the samples live in an inline array with a
    // compile-time size, so accessing samples[i] from a AudioBlock* is a direct
    // offset (no extra pointer chase) and the loop bound is a constant. This is
    // what lets the pipeline path match the manual reference's speed.
    struct AudioBlock {
        float samples[kBlockSize] {};

        void fill(float v) {
            for (auto& s : samples) {
                s = v;
            }
        }
    };

    // Produces a per-sample ramp value[i] = value + i*step each call.
    // The ramp makes every sample in the block distinct, which prevents the
    // compiler from broadcasting a single constant across the block and
    // folding the downstream per-sample work away.
    struct ConstantSource {

        using Manifest = ugraph::Manifest<
            ugraph::IO<AudioBlock, 0, 1>,
            ugraph::TaggedIO<ParametersTag, uint16_t, 1, 0, false>
        >;

        float value { 0.f };
        float step { 0.f };

        void process(ugraph::Context<Manifest>& ctx) {
            process(ctx.output<AudioBlock>().samples, kBlockSize);
        }

        // Pointer-based helper for manual path in tests
        void process(float* out, std::size_t s) {
            for (std::size_t i = 0; i < s; ++i) out[i] = value + static_cast<float>(i) * step;
        }

    };

    // Mixes two input blocks sample-wise (sum) into an output block.
    struct Mixer2 {

        using Manifest = ugraph::Manifest< ugraph::IO<AudioBlock, 2, 1> >;

        void process(ugraph::Context<Manifest>& ctx) {
            process(
                ctx.input<AudioBlock>(0).samples,
                ctx.input<AudioBlock>(1).samples,
                ctx.output<AudioBlock>().samples,
                kBlockSize
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

        using Manifest = ugraph::Manifest<ugraph::IO<AudioBlock, 1, 1>>;

        float gain { 1.f };

        void process(ugraph::Context<Manifest>& ctx) {
            process(
                ctx.input<AudioBlock>().samples,
                ctx.output<AudioBlock>().samples,
                kBlockSize
            );
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

        using Manifest = ugraph::Manifest< ugraph::IO<AudioBlock, 1, 0> >;

        float last_sample { 0.f };
        float sum { 0.f };

        void process(ugraph::Context<Manifest>& ctx) {
            process(ctx.input<AudioBlock>().samples, kBlockSize);
        }

        // Pointer-based helper for manual path in tests
        void process(const float* in, std::size_t s) {
            sum = 0.f;
            for (std::size_t i = 0; i < s; ++i) sum += in[i];
            last_sample = in[0];
        }

    };

}

namespace {

    template<std::size_t id, typename module_t>
    using GraphNode = ugraph::Node<id, module_t, typename module_t::Manifest>;


    static auto makeVoiceGraph(
        ConstantSource& s1,
        ConstantSource& s2,
        Mixer2& m,
        Gain& g,
        Sink& s,
        uint16_t(&params)[2]
    ) {

        auto vA = ugraph::make_node<0>(s1);
        auto vB = ugraph::make_node<1>(s2);
        auto vMix = ugraph::make_node<2>(m);
        auto vGain = ugraph::make_node<3>(g);
        auto vSink = ugraph::make_node<4>(s);

        return ugraph::Graph(
            params[0] | vA.input<ParametersTag>(),
            params[1] | vB.input<ParametersTag>(),
            vA.output<AudioBlock>() >> vMix.input<AudioBlock, 0>(),
            vB.output<AudioBlock>() >> vMix.input<AudioBlock, 1>(),
            vMix.output<AudioBlock>() >> vGain.input<AudioBlock>(),
            vGain.output<AudioBlock>() >> vSink.input<AudioBlock>()
        );
    }

    using voice_graph_t = decltype(
        makeVoiceGraph(
            std::declval<ConstantSource&>(),
            std::declval<ConstantSource&>(),
            std::declval<Mixer2&>(),
            std::declval<Gain&>(),
            std::declval<Sink&>(),
            std::declval<uint16_t(&)[2]>()
        )
        );


    struct AudioTestVoice {

        using graph_data_t = voice_graph_t::graph_data_t;

        AudioTestVoice() :
            mGraph(makeVoiceGraph(sa, sb, mix, gain, sink, mParams)) {}

        void setFreq(uint16_t inFreq) {
            mParams[0] = inFreq;
            mParams[1] = inFreq;
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

        graph_data_t& graph_data() {
            return mGraph.data();
        }

    private:

        ConstantSource sa { 0.25f };
        ConstantSource sb { 0.75f };
        Mixer2        mix {};
        Gain          gain { 0.5f };
        Sink          sink {};

        uint16_t mParams[2];

        voice_graph_t mGraph;
    };

}

TEST_CASE("basic synth voice test") {

    static constexpr auto storage_count = voice_graph_t::graph_data_t::template count<AudioBlock>();
    static_assert(storage_count == 3);

    AudioTestVoice voice;

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
    uint16_t params[2] {};

    auto g = ugraph::Graph(
        params[0] | vA.input<ParametersTag>(),
        params[1] | vB.input<ParametersTag>(),
        vA.output<AudioBlock>() >> vMix.input<AudioBlock, 0>(),
        vB.output<AudioBlock>() >> vMix.input<AudioBlock, 1>(),
        vMix.output<AudioBlock>() >> vGain.input<AudioBlock>(),
        vGain.output<AudioBlock>() >> vSink.input<AudioBlock>()
    );

    static_assert(decltype(g)::graph_data_t::template count<AudioBlock>() == 3, "Unexpected buffer count");

    g.for_each(
        [] (auto& module, auto& ctx) {
            module.process(ctx);
        }
    );

    CHECK(sink.last_sample == doctest::Approx(0.5f));
    CHECK(sink.sum == doctest::Approx(0.5f * kBlockSize));

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
    uint16_t params[2] {};

    auto g = ugraph::Graph(
        params[0] | vA.input<ParametersTag>(),
        params[1] | vB.input<ParametersTag>(),
        vA.output<AudioBlock>() >> vMix.input<AudioBlock, 0>(),
        vB.output<AudioBlock>() >> vMix.input<AudioBlock, 1>(),
        vMix.output<AudioBlock>() >> vGain.input<AudioBlock>(),
        vGain.output<AudioBlock>() >> vSink.input<AudioBlock>()
    );

    constexpr std::size_t iterations = 2500;

    for (std::size_t i = 0; i < iterations; ++i) {
        g.for_each(
            [] (auto& module, auto& ctx) {
                module.process(ctx);
            }
        );
    }

    CHECK(sink.last_sample == doctest::Approx(0.6f));
    CHECK(sink.sum == doctest::Approx(0.6f * kBlockSize));
}

// Clang builds can show larger variance in the simple wall-clock ratio
// measurement, so the ratio assertion is GCC-only. With the fixed-size
// AudioBlock (compile-time block size, inline sample array) both the pipeline
// and the manual reference can be unrolled/vectorized, so the ratio stays
// close to 1x; the threshold leaves headroom for machine/CI variance.
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
    uint16_t params[2] {};

    auto g = ugraph::Graph(
        params[0] | vA.input<ParametersTag>(),
        params[1] | vB.input<ParametersTag>(),
        vA.output<AudioBlock>() >> vMix.input<AudioBlock, 0>(),
        vB.output<AudioBlock>() >> vMix.input<AudioBlock, 1>(),
        vMix.output<AudioBlock>() >> vGain.input<AudioBlock>(),
        vGain.output<AudioBlock>() >> vSink.input<AudioBlock>()
    );

    static constexpr auto graph_storage_count = decltype(g)::graph_data_t::template count<AudioBlock>();
    CHECK(graph_storage_count == 3);

    // Manual reference buffers
    using storage_t = std::array<float, kBlockSize>;
    std::array<storage_t, 3> storage;

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

    // With fixed module parameters across all iterations GCC can prove the
    // whole 6000-iteration computation is constant and fold the manual loop
    // to a handful of constant stores. Two measures defeat this:
    //   - a per-sample ramp (value + i*step) makes every sample in a block
    //     distinct, so the block cannot be replaced by a broadcast, and
    //   - a volatile seed perturbs the source base value each iteration so the
    //     output genuinely differs across iterations.
    // The seed is kept bounded so the recurrence does not diverge. The escape
    // observes sink.sum, which depends on every sample and every iteration,
    // so the per-sample work stays live. Applied symmetrically to both paths.
    sa.step = 1.0f / static_cast<float>(kBlockSize);
    sb.step = 1.0f / static_cast<float>(kBlockSize);
    volatile float seed = 0.f;
    volatile float gEscape = 0.f;

    auto t0 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        sa.value = 0.3f + seed;
        sb.value = 0.4f + seed;
        g.for_each(
            [] (auto& module, auto& ctx) {
                module.process(ctx);
            }
        );
        gEscape = sinkPipe.sum;
        consume += sinkPipe.last_sample;
        seed = sinkPipe.sum - static_cast<float>(static_cast<int>(sinkPipe.sum));
    }
    auto t1 = clock::now();
    auto pipe_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    seed = 0.f;
    auto t2 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        sa.value = 0.3f + seed;
        sb.value = 0.4f + seed;
        sa.process(storage[0].data(), kBlockSize);
        sb.process(storage[1].data(), kBlockSize);
        mix.process(storage[0].data(), storage[1].data(), storage[2].data(), kBlockSize);
        gain.process(storage[2].data(), storage[0].data(), kBlockSize);
        sinkManual.process(storage[0].data(), kBlockSize);
        gEscape = sinkManual.sum;
        consume += sinkManual.last_sample;
        seed = sinkManual.sum - static_cast<float>(static_cast<int>(sinkManual.sum));
    }
    auto t3 = clock::now();
    auto manual_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count();

    // Both paths run the same chain with the same seed sequence, so their
    // final results must match.
    CHECK(sinkPipe.last_sample == doctest::Approx(sinkManual.last_sample));

    // Clean reference run with a flat (step == 0) block to validate the chain
    // math against a known constant output.
    sa.step = 0.f;
    sb.step = 0.f;
    sa.value = 0.3f;
    sb.value = 0.4f;
    sa.process(storage[0].data(), kBlockSize);
    sb.process(storage[1].data(), kBlockSize);
    mix.process(storage[0].data(), storage[1].data(), storage[2].data(), kBlockSize);
    gain.process(storage[2].data(), storage[0].data(), kBlockSize);
    sinkManual.process(storage[0].data(), kBlockSize);
    CHECK(sinkManual.last_sample == doctest::Approx(0.875f));
    CHECK(sinkManual.sum == doctest::Approx(0.875f * kBlockSize));

    double r = static_cast<double>(pipe_ns) / static_cast<double>(manual_ns);
    INFO("pipe_ns=" << pipe_ns << " manual_ns=" << manual_ns << " ratio=" << r);

    std::cout << "pipe_ns=" << pipe_ns << " manual_ns=" << manual_ns << " ratio=" << r << std::endl;

    CHECK(r < 1.5);

    (void) consume; // silence unused warning for volatile accumulation
    (void) gEscape;
}
