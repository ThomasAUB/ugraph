#include "doctest.h"
#include "ugraph.hpp"
#include <chrono>

// Custom AudioPipeline built on top of LightPipelineGraph demonstrating a more
// realistic audio style processing chain with sources, a mixer, gain stage and sink.
// It allocates the minimal number of buffer slots (as computed by LightPipelineGraph)
// and calls module process(...) functions in topological order.

namespace {

    // ===== Audio block & modules ===== //
    constexpr std::size_t kBlockSize = 64; // small block for test determinism

    struct AudioBlock {
        float samples[kBlockSize] {};

        void fill(float v) {
            for (auto& s : samples) s = v;
        }

        void copy_from(const AudioBlock& other) {
            for (std::size_t i = 0; i < kBlockSize; ++i) samples[i] = other.samples[i];
        }
    };

    // Constant value source
    struct ConstantSource {
        float value { 0.f };
        void process(AudioBlock* out) {
            out->fill(value);
        }
    };

    // Simple two-input mixer (sums inputs)
    struct Mixer2 {
        void process(const AudioBlock* in0, const AudioBlock* in1, AudioBlock* out) {
            for (std::size_t i = 0; i < kBlockSize; ++i) {
                out->samples[i] = in0->samples[i] + in1->samples[i];
            }
        }
    };

    // Gain stage (in-place)
    struct Gain {
        float gain { 1.f };
        void process(AudioBlock* inout) {
            for (auto& s : inout->samples) s *= gain;
        }
    };

    // Sink accumulating energy & keeping last sample for quick checks
    struct Sink {
        float last_sample { 0.f };
        float sum { 0.f };
        void process(const AudioBlock* in) {
            sum = 0.f;
            for (auto s : in->samples) sum += s;
            last_sample = in->samples[0];
        }
    };

    // AudioPipeline executor capable of handling:
    //  - sources: 0 inputs / 1 output  -> process(out)
    //  - single input transforms: 1 input / 1 output -> in-place or copy then process(out)
    //  - two input mixer: 2 inputs / 1 output -> process(in0, in1, out)
    //  - sink: 1 input / 0 outputs -> process(in)
    template<typename Graph>
    struct AudioPipeline {
        const Graph& graph;
        static constexpr std::size_t kBufferCount = (Graph::data_instance_count() > 0) ? Graph::data_instance_count() : 1;
        AudioBlock buffers[kBufferCount] {};

        void process_block() {
            graph.for_each([&] (auto& v) {
                using V = std::decay_t<decltype(v)>;
                constexpr auto vid = V::id();
                constexpr std::size_t IN = V::input_count();
                constexpr std::size_t OUT = V::output_count();

                // Helper: invoke module.process with (inputs..., outputs...) parameter order.
                auto invoke_inputs_outputs = [&] <std::size_t... II, std::size_t... OO>(std::index_sequence<II...>, std::index_sequence<OO...>) {
                    v.get_user_type().process(
                        (&buffers[Graph::template data_index_of_input<vid, II>()])...,
                        (&buffers[Graph::template data_index_of_output<vid, OO>()])...
                    );
                };

                // Helper: invoke module.process with outputs only (source or multi-output generator)
                auto invoke_outputs_only = [&] <std::size_t... OO>(std::index_sequence<OO...>) {
                    v.get_user_type().process((&buffers[Graph::template data_index_of_output<vid, OO>()])...);
                };

                // Helper: invoke module.process with inputs only (sink / terminal)
                auto invoke_inputs_only = [&] <std::size_t... II>(std::index_sequence<II...>) {
                    v.get_user_type().process((&buffers[Graph::template data_index_of_input<vid, II>()])...);
                };

                if constexpr (IN == 0 && OUT == 0) {
                    // Degenerate vertex – nothing to do.
                }
                else if constexpr (IN == 0) {
                    // Pure source / generator. Special-case OUT==1 to allow process(AudioBlock*) signatures.
                    if constexpr (OUT == 1) {
                        constexpr std::size_t out0 = Graph::template data_index_of_output<vid, 0>();
                        v.get_user_type().process(&buffers[out0]);
                    }
                    else {
                        invoke_outputs_only(std::make_index_sequence<OUT>{});
                    }
                }
                else if constexpr (OUT == 0) {
                    // Sink. Special-case IN==1 to allow process(const AudioBlock*) signatures.
                    if constexpr (IN == 1) {
                        constexpr std::size_t in0 = Graph::template data_index_of_input<vid, 0>();
                        v.get_user_type().process(&buffers[in0]);
                    }
                    else {
                        invoke_inputs_only(std::make_index_sequence<IN>{});
                    }
                }
                else if constexpr (IN == 1 && OUT == 1) {
                    // In-place transform convenience: copy if mapped to different buffers and call with single pointer.
                    constexpr std::size_t ib = Graph::template data_index_of_input<vid, 0>();
                    constexpr std::size_t ob = Graph::template data_index_of_output<vid, 0>();
                    if constexpr (ib != ob) {
                        buffers[ob].copy_from(buffers[ib]);
                    }
                    // Try to call process(AudioBlock*) by providing output buffer only.
                    v.get_user_type().process(&buffers[ob]);
                }
                else {
                    // General case (inputs..., outputs...). No implicit copying; module decides semantics.
                    invoke_inputs_outputs(std::make_index_sequence<IN>{}, std::make_index_sequence<OUT>{});
                }
                });
        }
    };

} // anonymous namespace

TEST_CASE("audio pipeline sources -> mixer -> gain -> sink") {
    ConstantSource srcA { 0.25f };
    ConstantSource srcB { 0.75f };
    Mixer2 mixer {};
    Gain gain { 0.5f }; // final expected per-sample: (0.25 + 0.75) * 0.5 = 0.5
    Sink sink {};

    // Vertex definitions: <id, meta_type, input_count, output_count>
    ugraph::LightPipelineVertex<3001, ConstantSource, 0, 1> vSrcA(srcA);
    ugraph::LightPipelineVertex<3002, ConstantSource, 0, 1> vSrcB(srcB);
    ugraph::LightPipelineVertex<3003, Mixer2, 2, 1> vMix(mixer);
    ugraph::LightPipelineVertex<3004, Gain, 1, 1> vGain(gain);
    ugraph::LightPipelineVertex<3005, Sink, 1, 0> vSink(sink);

    auto g = ugraph::LightPipelineGraph(
        vSrcA.out() >> vMix.in<0>(),
        vSrcB.out() >> vMix.in<1>(),
        vMix.out() >> vGain.in(),
        vGain.out() >> vSink.in()
    );

    static_assert(g.data_instance_count() == 3, "Unexpected minimal buffer count for audio pipeline");

    AudioPipeline<decltype(g)> pipeline { g };
    pipeline.process_block();

    // Check sink results
    CHECK(sink.last_sample == doctest::Approx(0.5f));
    // Sum over block should be 0.5 * block size
    CHECK(sink.sum == doctest::Approx(0.5f * kBlockSize));
}

TEST_CASE("audio pipeline repeated processing & basic performance sanity") {
    ConstantSource srcA { 0.1f };
    ConstantSource srcB { 0.2f };
    Mixer2 mixer {};
    Gain gain { 2.0f }; // (0.1 + 0.2) * 2 = 0.6
    Sink sink {};

    ugraph::LightPipelineVertex<4001, ConstantSource, 0, 1> vSrcA(srcA);
    ugraph::LightPipelineVertex<4002, ConstantSource, 0, 1> vSrcB(srcB);
    ugraph::LightPipelineVertex<4003, Mixer2, 2, 1> vMix(mixer);
    ugraph::LightPipelineVertex<4004, Gain, 1, 1> vGain(gain);
    ugraph::LightPipelineVertex<4005, Sink, 1, 0> vSink(sink);

    auto g = ugraph::LightPipelineGraph(
        vSrcA.out() >> vMix.in<0>(),
        vSrcB.out() >> vMix.in<1>(),
        vMix.out() >> vGain.in(),
        vGain.out() >> vSink.in()
    );
    AudioPipeline<decltype(g)> pipeline { g };

    constexpr std::size_t iterations = 5000; // modest iterations
    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) pipeline.process_block();
    auto t1 = clock::now();
    auto ns_total = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    INFO("Total ns: " << ns_total << ", ns/iter: " << (double) ns_total / iterations);

    CHECK(sink.last_sample == doctest::Approx(0.6f));
    CHECK(sink.sum == doctest::Approx(0.6f * kBlockSize));
}

TEST_CASE("audio pipeline vs manual performance") {
    // Modules
    ConstantSource srcA { 0.3f };
    ConstantSource srcB { 0.4f };
    Mixer2 mixer {};
    Gain gain { 1.25f }; // expected per-sample: (0.3 + 0.4) * 1.25 = 0.875
    Sink sinkPipeline {};
    Sink sinkManual {};

    // Graph vertices
    ugraph::LightPipelineVertex<5001, ConstantSource, 0, 1> vSrcA(srcA);
    ugraph::LightPipelineVertex<5002, ConstantSource, 0, 1> vSrcB(srcB);
    ugraph::LightPipelineVertex<5003, Mixer2, 2, 1> vMix(mixer);
    ugraph::LightPipelineVertex<5004, Gain, 1, 1> vGain(gain);
    ugraph::LightPipelineVertex<5005, Sink, 1, 0> vSink(sinkPipeline);

    auto g = ugraph::LightPipelineGraph(
        vSrcA.out() >> vMix.in<0>(),
        vSrcB.out() >> vMix.in<1>(),
        vMix.out() >> vGain.in(),
        vGain.out() >> vSink.in()
    );
    AudioPipeline<decltype(g)> pipeline { g };

    // Manual buffers
    AudioBlock bufSrcA, bufSrcB, bufMix, bufGain; // bufGain is final after gain
    volatile float accumulator = 0.f; // prevent over-optimization

    // Warmup both paths a little
    for (int i = 0; i < 256; ++i) {
        pipeline.process_block();
        srcA.process(&bufSrcA);
        srcB.process(&bufSrcB);
        mixer.process(&bufSrcA, &bufSrcB, &bufMix);
        bufGain.copy_from(bufMix);
        gain.process(&bufGain);
        sinkManual.process(&bufGain);
        accumulator += sinkPipeline.last_sample + sinkManual.last_sample;
    }

    constexpr std::size_t iterations = 15000; // reasonably large for timing
    using clock = std::chrono::high_resolution_clock;

    auto t0 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        pipeline.process_block();
        accumulator += sinkPipeline.last_sample; // consume
    }
    auto t1 = clock::now();
    auto pipeline_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    auto t2 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        srcA.process(&bufSrcA);
        srcB.process(&bufSrcB);
        mixer.process(&bufSrcA, &bufSrcB, &bufMix);
        bufGain.copy_from(bufMix); // emulate possible out-of-place mapping
        gain.process(&bufGain);
        sinkManual.process(&bufGain);
        accumulator += sinkManual.last_sample; // consume
    }
    auto t3 = clock::now();
    auto manual_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count();

    // Correctness: both paths should produce identical per-sample value.
    CHECK(sinkPipeline.last_sample == doctest::Approx(0.875f));
    CHECK(sinkManual.last_sample == doctest::Approx(0.875f));
    CHECK(sinkPipeline.sum == doctest::Approx(0.875f * kBlockSize));
    // Manual sink sum computed only in warmup loop; recompute now to validate.
    // (We could recompute manually.)
    srcA.process(&bufSrcA);
    srcB.process(&bufSrcB);
    mixer.process(&bufSrcA, &bufSrcB, &bufMix);
    bufGain.copy_from(bufMix);
    gain.process(&bufGain);
    sinkManual.process(&bufGain);
    CHECK(sinkManual.sum == doctest::Approx(0.875f * kBlockSize));

    double pipeline_per_iter = static_cast<double>(pipeline_ns) / iterations;
    double manual_per_iter = static_cast<double>(manual_ns) / iterations;
    double ratio = pipeline_per_iter / manual_per_iter;

    INFO("AudioPipeline total ns: " << pipeline_ns);
    INFO("Manual total ns: " << manual_ns);
    INFO("AudioPipeline ns/iter: " << pipeline_per_iter);
    INFO("Manual ns/iter: " << manual_per_iter);
    INFO("Relative slowdown (pipeline/manual): " << ratio);

    // Allow generous overhead; should typically be close to 1-3x.
    CHECK(ratio < 1.5);
    (void) accumulator;

}
