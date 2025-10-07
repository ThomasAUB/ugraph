#include "doctest.h"
#include "ugraph.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>


struct AudioBuffer {
    // Iteration (mutable)
    auto begin() { return mBuffer; }
    auto end() { return mBuffer + mBufferSize; }

    // Iteration (const)
    const auto begin() const { return mBuffer; }
    const auto end()   const { return mBuffer + mBufferSize; }

    // Element access
    auto& operator[](std::size_t i) { return mBuffer[i]; }
    const auto& operator[](std::size_t i) const { return mBuffer[i]; }

    static constexpr std::size_t mBufferSize = 32;
    float mBuffer[mBufferSize];
};

// Audio pipeline helper types (file-scope so member templates are allowed)
struct Source {
    std::string_view name;
    float            value;
    uint8_t          period;

    uint8_t periodCounter = 0;

    void process(AudioBuffer& out) {
        for (auto& s : out) {
            if (periodCounter < (period / 2)) {
                s = value;
                ++periodCounter;
            }
            else {
                s = -value;
                if (++periodCounter == period) {
                    periodCounter = 0;
                }
            }
        }
    }
};

struct Gain {
    std::string_view name;
    float            gain;

    void process(const AudioBuffer& inBuffer, AudioBuffer& outBuffer) {
        std::size_t i = 0;
        for (auto& s : outBuffer) {
            s = inBuffer[i++] * gain;
        }
    }
};

struct Mixer3to1 {
    std::string_view name;

    void process(
        const AudioBuffer& inBuffer1,
        const AudioBuffer& inBuffer2,
        const AudioBuffer& inBuffer3,
        AudioBuffer& outBuffer
    ) {
        std::size_t i = 0;
        for (auto& s : outBuffer) {
            s = inBuffer1[i] + inBuffer2[i] + inBuffer3[i];
            ++i;
        }
    }
};

struct Sink {
    std::string_view name;
    AudioBuffer      received;

    void process(const AudioBuffer& inBuffer) {
        std::size_t i = 0;
        for (auto s : inBuffer) {
            received[i++] = s;
        }
    }
};

struct Timer {
    void start() {
        exec_start = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        auto exec_end = std::chrono::high_resolution_clock::now();
        duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(exec_end - exec_start).count();
    }

    using type = decltype(std::chrono::high_resolution_clock::now());

    type     exec_start {};
    uint64_t duration_ns {};
};

struct AudioModules {
    // Instantiate user-layer objects
    Source s1 { "src1",  0.5f,   6 };   // constant  0.5
    Source s2 { "src2",  1.0f,  10 };   // constant  1.0
    Source s3 { "src3", -0.25f, 20 };   // constant -0.25

    Gain g1 { "gain1", 0.8f }; //  0.5 * 0.8 =  0.4
    Gain g2 { "gain2", 0.5f }; //  1.0 * 0.5 =  0.5
    Gain g3 { "gain3", 2.0f }; // -0.25 * 2.0 = -0.5

    Mixer3to1 mix { "mixer" };
    Sink      out { "sink" };
};

struct AudioPipeline {
    AudioModules modules;

    ugraph::PipelineVertex<100, Source, 0, 1, AudioBuffer> vsrc1;
    ugraph::PipelineVertex<101, Source, 0, 1, AudioBuffer> vsrc2;
    ugraph::PipelineVertex<102, Source, 0, 1, AudioBuffer> vsrc3;

    ugraph::PipelineVertex<110, Gain, 1, 1, AudioBuffer> vgain1;
    ugraph::PipelineVertex<111, Gain, 1, 1, AudioBuffer> vgain2;
    ugraph::PipelineVertex<112, Gain, 1, 1, AudioBuffer> vgain3;

    ugraph::PipelineVertex<120, Mixer3to1, 3, 1, AudioBuffer> vmix;
    ugraph::PipelineVertex<130, Sink, 1, 0, AudioBuffer> vsink;

    AudioPipeline()
        : vsrc1(modules.s1)
        , vsrc2(modules.s2)
        , vsrc3(modules.s3)
        , vgain1(modules.g1)
        , vgain2(modules.g2)
        , vgain3(modules.g3)
        , vmix(modules.mix)
        , vsink(modules.out) {}

    auto get_graph() {
        return ugraph::PipelineGraph(
            vsrc1.out() >> vgain1.in(),
            vsrc2.out() >> vgain2.in(),
            vsrc3.out() >> vgain3.in(),
            vgain1.out() >> vmix.in<0>(),
            vgain2.out() >> vmix.in<1>(),
            vgain3.out() >> vmix.in<2>(),
            vmix.out() >> vsink.in()
        );
    }
};

// -----------------------------------------------------------------------------
// Refactored audio pipeline tests (structure, correctness, optional benchmark)
// -----------------------------------------------------------------------------

TEST_CASE("audio pipeline structure") {
    AudioPipeline pipeline;
    auto g = pipeline.get_graph();
    static_assert(decltype(g)::size() == 8, "Unexpected compile-time size");
    CHECK(decltype(g)::size() == 8);

    // Expect 4 buffers: 3 (source->gain chains) + 1 (mixer->sink)
    CHECK(g.buffer_count() == 4);
    CHECK(g.buffer_count() <= decltype(g)::size());
}

namespace {
    // Generate expected mixed samples analytically (no reuse of pipeline objects)
    std::array<float, AudioBuffer::mBufferSize> expected_audio_mix_once() {
        std::array<float, AudioBuffer::mBufferSize> out {};

        auto gen =
            [] (float val, uint8_t period, std::size_t i) {
            uint8_t half = period / 2; // integer divide consistent with Source logic
            uint8_t pos = static_cast<uint8_t>(i % period);
            return (pos < half) ? val : -val;
            };

        for (std::size_t i = 0; i < out.size(); ++i) {
            // Source waves then gain scaling
            float s1 = gen(0.5f, 6, i) * 0.8f;  //  0.4 / -0.4
            float s2 = gen(1.0f, 10, i) * 0.5f;  //  0.5 / -0.5
            float s3 = gen(-0.25f, 20, i) * 2.0f;  // -0.5 / +0.5
            out[i] = s1 + s2 + s3;                 // mix
        }
        return out;
    }
}

TEST_CASE("audio pipeline correctness single execute") {
    AudioPipeline pipeline; // fresh sources (period counters = 0)
    auto g = pipeline.get_graph();
    auto expected = expected_audio_mix_once();

    g.execute();

    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(pipeline.modules.out.received[i] == expected[i]);
    }
}

template<std::size_t N>
struct TimingInstrumentation {
    static constexpr bool enabled = true;

    Timer                pipeline_timer;
    std::array<Timer, N> vertex_timers;

    void on_pipeline_start() { pipeline_timer.start(); }
    void on_pipeline_end() { pipeline_timer.stop(); }

    template <typename V>
    void on_vertex_start(std::size_t i, V&) { vertex_timers[i].start(); }

    template <typename V>
    void on_vertex_end(std::size_t i, V&) { vertex_timers[i].stop(); }

    auto duration(std::size_t i) { return vertex_timers[i].duration_ns; }
    auto percentage(std::size_t i) { return (vertex_timers[i].duration_ns * 100) / pipeline_timer.duration_ns; }
};

TEST_CASE("pipeline instrumentation validation") {
    AudioPipeline pipeline;
    auto g = pipeline.get_graph();

    static constexpr auto vertex_count = decltype(g)::size();
    TimingInstrumentation<vertex_count> instrument;

    g.execute(instrument);

    uint64_t pipeline_ns = instrument.pipeline_timer.duration_ns;
    CHECK(pipeline_ns > 0);

    uint64_t sum_vertices = 0;
    for (std::size_t i = 0; i < vertex_count; ++i) {
        auto vd = instrument.duration(i);
        CHECK(vd <= pipeline_ns); // each vertex time must not exceed total
        sum_vertices += vd;
    }

    // Allow some overhead (timer calls, loop) up to +25%
    CHECK(sum_vertices <= pipeline_ns + (pipeline_ns / 4));

    // Integer percentages may truncate; ensure plausible aggregate
    uint64_t pct_sum = 0;
    for (std::size_t i = 0; i < vertex_count; ++i) {
        pct_sum += instrument.percentage(i);
    }

    // Allow wider bounds due to truncation + measurement overhead
    CHECK(pct_sum >= 50);
    CHECK(pct_sum <= 125);

    // Provide diagnostic info only on failure
    INFO(
        "pipeline_ns=" << pipeline_ns <<
        ", sum_vertices=" << sum_vertices <<
        ", pct_sum=" << pct_sum
    );
}

// -----------------------------------------------------------------------------
// Compare engine execution vs manual chaining to assess overhead and ensure
// identical output. This is a single-buffer sized micro-benchmark; timing
// variability is expected so assertions are intentionally lenient.
// -----------------------------------------------------------------------------
TEST_CASE("audio pipeline manual equivalence and overhead") {
    // Build pipeline path (fresh state)
    AudioPipeline pipeline;
    auto g = pipeline.get_graph();

    using GraphType = decltype(g);
    TimingInstrumentation<GraphType::size()> inst;
    g.execute(inst);
    uint64_t pipeline_ns = inst.pipeline_timer.duration_ns;
    CHECK(pipeline_ns > 0);

    // Manual path with independently constructed modules (fresh state)
    AudioModules manual; // sources start with periodCounter = 0 like pipeline
    AudioBuffer buf1 {};
    AudioBuffer buf2 {};
    AudioBuffer buf3 {};
    AudioBuffer mixBuf {};

    Timer manual_timer;
    manual_timer.start();
    manual.s1.process(buf1);
    manual.g1.process(buf1, buf1); // in-place gain
    manual.s2.process(buf2);
    manual.g2.process(buf2, buf2);
    manual.s3.process(buf3);
    manual.g3.process(buf3, buf3);
    manual.mix.process(buf1, buf2, buf3, mixBuf);
    manual.out.process(mixBuf);
    manual_timer.stop();
    uint64_t manual_ns = manual_timer.duration_ns;

    // Output equivalence (sample-accurate)
    for (std::size_t i = 0; i < AudioBuffer::mBufferSize; ++i) {
        CHECK(pipeline.modules.out.received[i] == manual.out.received[i]);
    }

    // Overhead evaluation: ratio = engine / manual should be plausible.
    if (manual_ns > 0) {
        double ratio = static_cast<double>(pipeline_ns) / static_cast<double>(manual_ns);
        // Very lenient upper bound; engine may include buffer routing + bookkeeping.
        CHECK(ratio < 50.0);
        INFO("manual_ns=" << manual_ns << ", pipeline_ns=" << pipeline_ns << ", ratio=" << ratio);
    }
    else {
        INFO("manual path time too small to measure, pipeline_ns=" << pipeline_ns);
    }

}
