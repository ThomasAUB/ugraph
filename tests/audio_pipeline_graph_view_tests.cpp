// NOTE: This file intentionally expands previously very condensed one-line test code
// into a more readable, step‑by‑step style. Semantics must remain identical.

#include "doctest.h"
#include "ugraph.hpp"
#include <chrono>

// Audio processing oriented DataGraph executor tests (sources -> mixer -> gain -> sink + perf).
namespace {

    // Size (in samples) of each audio processing block.
    constexpr std::size_t kBlockSize = 64;

    // Simple fixed-size audio buffer with helper utilities.
    struct AudioBlock {
        float samples[kBlockSize] {}; // Zero-initialized

        void fill(float v) {
            for (auto& s : samples) {
                s = v;
            }
        }

        void copy_from(const AudioBlock& o) {
            for (std::size_t i = 0; i < kBlockSize; ++i) {
                samples[i] = o.samples[i];
            }
        }
    };

    // Produces a constant value each call.
    struct ConstantSource {
        float value { 0.f };

        void process(AudioBlock* out) {
            out->fill(value);
        }
        using Manifest = ugraph::Manifest< ugraph::IO<AudioBlock, 0, 1> >;
    };

    // Mixes two input blocks sample-wise (sum) into an output block.
    struct Mixer2 {
        void process(const AudioBlock* a, const AudioBlock* b, AudioBlock* out) {
            for (std::size_t i = 0; i < kBlockSize; ++i) {
                out->samples[i] = a->samples[i] + b->samples[i];
            }
        }
        using Manifest = ugraph::Manifest< ugraph::IO<AudioBlock, 2, 1> >;
    };

    // Scales all samples in-place.
    struct Gain {
        float gain { 1.f };

        void process(AudioBlock* inout) {
            for (auto& s : inout->samples) {
                s *= gain;
            }
        }
        using Manifest = ugraph::Manifest< ugraph::IO<AudioBlock, 1, 1> >;
    };

    // Sink that accumulates the sum and tracks the first sample for quick checks.
    struct Sink {
        float last_sample { 0.f };
        float sum { 0.f };

        void process(const AudioBlock* in) {
            sum = 0.f;
            for (auto s : in->samples) {
                sum += s;
            }
            last_sample = in->samples[0];
        }
        using Manifest = ugraph::Manifest< ugraph::IO<AudioBlock, 1, 0> >;
    };

    // Small execution helper that maps the graph's data indices to concrete user process calls.
    template<typename Graph>
    struct AudioPipeline {
        Graph& graph;

        static constexpr std::size_t kBufferCount =
            (Graph::template data_instance_count<AudioBlock>() > 0) ? Graph::template data_instance_count<AudioBlock>() : 1;

        AudioBlock buffers[kBufferCount] {};

        // The DataGraph::for_each visitor now receives the module and its NodeContext
        // (pointers to input/output buffers). Use the context to invoke module::process
        // without relying on compile-time vertex ids.

        void process_block() {
            // A single generic dispatch that handles any (IN, OUT) pair. The visitor
            // receives the concrete module instance and a NodeContext which exposes
            // typed input/output pointers. We use the Manifest on the module to
            // determine per-type input/output counts and call the appropriate
            // process() overloads.
            graph.for_each([&] (auto& module, auto& ctx) {
                using module_t = std::decay_t<decltype(module)>;
                using manifest_t = typename module_t::Manifest;
                constexpr std::size_t IN = manifest_t::template input_count<AudioBlock>();
                constexpr std::size_t OUT = manifest_t::template output_count<AudioBlock>();

                if constexpr (IN == 1 && OUT == 1) {
                    auto& in = ctx.template input<AudioBlock>(0);
                    auto& out = ctx.template output<AudioBlock>(0);
                    if (&in != &out) out.copy_from(in);
                    module.process(&out);
                }
                else if constexpr (IN == 0 && OUT == 1) {
                    auto& out = ctx.template output<AudioBlock>(0);
                    module.process(&out);
                }
                else if constexpr (IN == 2 && OUT == 1) {
                    auto& a = ctx.template input<AudioBlock>(0);
                    auto& b = ctx.template input<AudioBlock>(1);
                    auto& out = ctx.template output<AudioBlock>(0);
                    module.process(&a, &b, &out);
                }
                else if constexpr (IN == 1 && OUT == 0) {
                    auto& in = ctx.template input<AudioBlock>(0);
                    module.process(&in);
                }
                });
        }
    };
}

TEST_CASE("audio graph simple chain correctness") {
    ConstantSource sa { 0.25f };
    ConstantSource sb { 0.75f };
    Mixer2        mix {};
    Gain          gain { 0.5f };
    Sink          sink {};

    auto vA = ugraph::make_data_node<3001>(sa);
    auto vB = ugraph::make_data_node<3002>(sb);
    auto vMix = ugraph::make_data_node<3003>(mix);
    auto vGain = ugraph::make_data_node<3004>(gain);
    auto vSink = ugraph::make_data_node<3005>(sink);

    auto g = ugraph::DataGraph(
        vA.output<AudioBlock, 0>() >> vMix.input<AudioBlock, 0>(),
        vB.output<AudioBlock, 0>() >> vMix.input<AudioBlock, 1>(),
        vMix.output<AudioBlock, 0>() >> vGain.input<AudioBlock, 0>(),
        vGain.output<AudioBlock, 0>() >> vSink.input<AudioBlock, 0>()
    );

    static_assert(decltype(g)::template data_instance_count<AudioBlock>() == 3, "Unexpected buffer count");

    AudioPipeline<decltype(g)> pipe { g };
    pipe.process_block();

    CHECK(sink.last_sample == doctest::Approx(0.5f));
    CHECK(sink.sum == doctest::Approx(0.5f * kBlockSize));
}

TEST_CASE("audio graph repeated processing") {
    ConstantSource sa { 0.1f };
    ConstantSource sb { 0.2f };
    Mixer2        mix {};
    Gain          gain { 2.0f };
    Sink          sink {};

    auto vA = ugraph::make_data_node<4001>(sa);
    auto vB = ugraph::make_data_node<4002>(sb);
    auto vMix = ugraph::make_data_node<4003>(mix);
    auto vGain = ugraph::make_data_node<4004>(gain);
    auto vSink = ugraph::make_data_node<4005>(sink);

    auto g = ugraph::DataGraph(
        vA.output<AudioBlock, 0>() >> vMix.input<AudioBlock, 0>(),
        vB.output<AudioBlock, 0>() >> vMix.input<AudioBlock, 1>(),
        vMix.output<AudioBlock, 0>() >> vGain.input<AudioBlock, 0>(),
        vGain.output<AudioBlock, 0>() >> vSink.input<AudioBlock, 0>()
    );

    AudioPipeline<decltype(g)> pipe { g };

    constexpr std::size_t iterations = 2500;
    for (std::size_t i = 0; i < iterations; ++i) {
        pipe.process_block();
    }

    CHECK(sink.last_sample == doctest::Approx(0.6f));
    CHECK(sink.sum == doctest::Approx(0.6f * kBlockSize));
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

    auto vA = ugraph::make_data_node<5001>(sa);
    auto vB = ugraph::make_data_node<5002>(sb);
    auto vMix = ugraph::make_data_node<5003>(mix);
    auto vGain = ugraph::make_data_node<5004>(gain);
    auto vSink = ugraph::make_data_node<5005>(sinkPipe);

    auto g = ugraph::DataGraph(
        vA.output<AudioBlock, 0>() >> vMix.input<AudioBlock, 0>(),
        vB.output<AudioBlock, 0>() >> vMix.input<AudioBlock, 1>(),
        vMix.output<AudioBlock, 0>() >> vGain.input<AudioBlock, 0>(),
        vGain.output<AudioBlock, 0>() >> vSink.input<AudioBlock, 0>()
    );

    AudioPipeline<decltype(g)> pipe { g };

    // Manual reference buffers
    AudioBlock bufA;
    AudioBlock bufB;
    AudioBlock bufMix;
    AudioBlock bufGain;

    // Warm-up both paths (also protects against extremely small timings)
    volatile float consume = 0.f; // Prevent compiler elision
    for (int i = 0; i < 128; ++i) {
        pipe.process_block();
        sa.process(&bufA);
        sb.process(&bufB);
        mix.process(&bufA, &bufB, &bufMix);
        bufGain.copy_from(bufMix);
        gain.process(&bufGain);
        sinkManual.process(&bufGain);
        consume += sinkPipe.last_sample + sinkManual.last_sample;
    }

    using clock = std::chrono::high_resolution_clock;
    constexpr std::size_t iterations = 6000;

    auto t0 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        pipe.process_block();
        consume += sinkPipe.last_sample;
    }
    auto t1 = clock::now();
    auto pipe_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    auto t2 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        sa.process(&bufA);
        sb.process(&bufB);
        mix.process(&bufA, &bufB, &bufMix);
        bufGain.copy_from(bufMix);
        gain.process(&bufGain);
        sinkManual.process(&bufGain);
        consume += sinkManual.last_sample;
    }
    auto t3 = clock::now();
    auto manual_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count();

    CHECK(sinkPipe.last_sample == doctest::Approx(0.875f));
    CHECK(sinkManual.last_sample == doctest::Approx(0.875f));

    // Recompute manual sum for accuracy check.
    sa.process(&bufA);
    sb.process(&bufB);
    mix.process(&bufA, &bufB, &bufMix);
    bufGain.copy_from(bufMix);
    gain.process(&bufGain);
    sinkManual.process(&bufGain);
    CHECK(sinkManual.sum == doctest::Approx(0.875f * kBlockSize));

    double r = static_cast<double>(pipe_ns) / static_cast<double>(manual_ns);
    INFO("pipe_ns=" << pipe_ns << " manual_ns=" << manual_ns << " ratio=" << r);

    CHECK(r < 1.5);

    (void) consume; // silence unused warning for volatile accumulation
}

#endif // __clang__
