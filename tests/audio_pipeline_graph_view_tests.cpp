// NOTE: This file intentionally expands previously very condensed one-line test code
// into a more readable, step‑by‑step style. Semantics must remain identical.

#include "doctest.h"
#include "ugraph.hpp"
#include <chrono>

// Audio processing oriented GraphView executor tests (sources -> mixer -> gain -> sink + perf).
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
    };

    // Mixes two input blocks sample-wise (sum) into an output block.
    struct Mixer2 {
        void process(const AudioBlock* a, const AudioBlock* b, AudioBlock* out) {
            for (std::size_t i = 0; i < kBlockSize; ++i) {
                out->samples[i] = a->samples[i] + b->samples[i];
            }
        }
    };

    // Scales all samples in-place.
    struct Gain {
        float gain { 1.f };

        void process(AudioBlock* inout) {
            for (auto& s : inout->samples) {
                s *= gain;
            }
        }
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
    };

    // Small execution helper that maps the graph's data indices to concrete user process calls.
    template<typename Graph>
    struct AudioPipeline {
        const Graph& graph;

        static constexpr std::size_t kBufferCount =
            (Graph::data_instance_count() > 0) ? Graph::data_instance_count() : 1;

        AudioBlock buffers[kBufferCount] {};

        void process_block() {
            // A single generic dispatch that handles any (IN, OUT) pair.
            // Special-case only (1,1) to preserve copy semantics when buffers differ.
            graph.for_each([&] (auto& v) {
                using V = std::decay_t<decltype(v)>;
                constexpr auto vid = V::id();
                constexpr std::size_t IN = V::input_count();
                constexpr std::size_t OUT = V::output_count();

                // Generic pack expansion helper.
                auto invoke = [&]<std::size_t... I, std::size_t... O>(
                    std::index_sequence<I...>, std::index_sequence<O...>) {
                    v.module().process(
                        (&buffers[Graph::template input_data_index<vid, I>()])...,
                        (&buffers[Graph::template output_data_index<vid, O>()])...
                    );
                };

                if constexpr (IN == 1 && OUT == 1) {
                    // copy input to output when distinct, then run in-place.
                    constexpr auto ib = Graph::template input_data_index<vid, 0>();
                    constexpr auto ob = Graph::template output_data_index<vid, 0>();
                    if constexpr (ib != ob) {
                        buffers[ob].copy_from(buffers[ib]);
                    }
                    v.module().process(&buffers[ob]);
                }
                else {
                    invoke(std::make_index_sequence<IN>{}, std::make_index_sequence<OUT>{});
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

    ugraph::Node<3001, ConstantSource, 0, 1> vA(sa);
    ugraph::Node<3002, ConstantSource, 0, 1> vB(sb);
    ugraph::Node<3003, Mixer2, 2, 1> vMix(mix);
    ugraph::Node<3004, Gain, 1, 1> vGain(gain);
    ugraph::Node<3005, Sink, 1, 0> vSink(sink);

    auto g = ugraph::GraphView(
        vA.out() >> vMix.in<0>(),
        vB.out() >> vMix.in<1>(),
        vMix.out() >> vGain.in(),
        vGain.out() >> vSink.in()
    );

    static_assert(g.data_instance_count() == 3, "Unexpected buffer count");

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

    ugraph::Node<4001, ConstantSource, 0, 1> vA(sa);
    ugraph::Node<4002, ConstantSource, 0, 1> vB(sb);
    ugraph::Node<4003, Mixer2, 2, 1> vMix(mix);
    ugraph::Node<4004, Gain, 1, 1> vGain(gain);
    ugraph::Node<4005, Sink, 1, 0> vSink(sink);

    auto g = ugraph::GraphView(
        vA.out() >> vMix.in<0>(),
        vB.out() >> vMix.in<1>(),
        vMix.out() >> vGain.in(),
        vGain.out() >> vSink.in()
    );

    AudioPipeline<decltype(g)> pipe { g };

    constexpr std::size_t iterations = 2500;
    for (std::size_t i = 0; i < iterations; ++i) {
        pipe.process_block();
    }

    CHECK(sink.last_sample == doctest::Approx(0.6f));
    CHECK(sink.sum == doctest::Approx(0.6f * kBlockSize));
}

TEST_CASE("audio graph pipeline vs manual performance ratio") {
    ConstantSource sa { 0.3f };
    ConstantSource sb { 0.4f };
    Mixer2        mix {};
    Gain          gain { 1.25f };
    Sink          sinkPipe {};
    Sink          sinkManual {};

    ugraph::Node<5001, ConstantSource, 0, 1> vA(sa);
    ugraph::Node<5002, ConstantSource, 0, 1> vB(sb);
    ugraph::Node<5003, Mixer2, 2, 1> vMix(mix);
    ugraph::Node<5004, Gain, 1, 1> vGain(gain);
    ugraph::Node<5005, Sink, 1, 0> vSink(sinkPipe);

    auto g = ugraph::GraphView(
        vA.out() >> vMix.in<0>(),
        vB.out() >> vMix.in<1>(),
        vMix.out() >> vGain.in(),
        vGain.out() >> vSink.in()
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
    CHECK(r < 1.2);
    (void) consume; // silence unused warning for volatile accumulation
}
