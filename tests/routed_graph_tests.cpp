#include <iostream>
#include <chrono>
#include <array>
#include <string_view>
#include <vector>
#include <cstdint>

#include "doctest.h"
#include "ugraph.hpp"

// -----------------------------------------------------------------------------
// Helper processor types used in routing tests
// -----------------------------------------------------------------------------
struct Proc1 {
    constexpr Proc1(std::string_view n) : mName(n) {}
    std::string_view mName;
};

struct Proc2 {
    constexpr Proc2(std::string_view n) : mName(n) {}
    std::string_view mName;
};

TEST_CASE("runtime validation") {
    struct StageA { std::string_view name; };
    struct StageB { std::string_view name; };
    struct StageC { std::string_view name; };

    StageA a { "A" };
    StageB b { "B" };
    StageC c { "C" };

    ugraph::PipelineVertex<1, StageA, 0, 1, int> vA(a);
    ugraph::PipelineVertex<2, StageB, 1, 1, int> vB(b);
    ugraph::PipelineVertex<3, StageC, 1, 0, int> vC(c);

    auto g = ugraph::PipelineGraph(
        vB.out() >> vC.in(),
        vA.out() >> vB.in()
    );

    std::vector<char> order;

    g.apply(
        [&] (auto&... impls) {
            (order.push_back(impls.name[0]), ...);
        }
    );

    REQUIRE(order.size() == 3);
    CHECK(order[0] == 'A');
    CHECK(order[1] == 'B');
    CHECK(order[2] == 'C');
    CHECK(g.buffer_count() == 2);
}

TEST_CASE("basic topological sort test") {
    Proc1 pn1("n1");
    Proc1 pn2("n2");
    Proc1 pn4("n4");
    Proc2 pn3("n3");

    ugraph::PipelineVertex<1, Proc1, 1, 1, int> n1(pn1);
    ugraph::PipelineVertex<2, Proc1, 1, 1, int> n2(pn2);
    ugraph::PipelineVertex<3, Proc2, 1, 1, int> n3(pn3);
    ugraph::PipelineVertex<4, Proc1, 1, 1, int> n4(pn4);

    auto g = ugraph::PipelineGraph(
        n1.out() >> n2.in(),
        n2.out() >> n3.in(),
        n1.out() >> n3.in(),
        n1.out() >> n4.in(),
        n2.out() >> n4.in()
    );

    std::vector<std::string_view> order;
    order.reserve(decltype(g)::size());

    g.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.mName), ...);
        }
    );

    std::size_t i_n1 = 0;
    std::size_t i_n2 = 0;
    std::size_t i_n3 = 0;
    std::size_t i_n4 = 0;

    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == "n1")      i_n1 = i;
        else if (order[i] == "n2") i_n2 = i;
        else if (order[i] == "n3") i_n3 = i;
        else if (order[i] == "n4") i_n4 = i;
    }

    CHECK(i_n1 < i_n2);
    CHECK(i_n2 < i_n3);
    CHECK(i_n2 < i_n4);
    CHECK(i_n1 < i_n3);
    CHECK(i_n1 < i_n4);
}

TEST_CASE("topological sort verification") {
    Proc1 psource("source");
    Proc1 psink("sink");
    Proc2 pmiddle("middle");

    ugraph::PipelineVertex<10, Proc1, 0, 1, int> source(psource);
    ugraph::PipelineVertex<11, Proc2, 1, 1, int> middle(pmiddle);
    ugraph::PipelineVertex<12, Proc1, 1, 0, int> sink(psink);

    auto g = ugraph::PipelineGraph(
        source.out() >> middle.in(),
        middle.out() >> sink.in()
    );

    auto ids = decltype(g)::ids();
    CHECK(decltype(g)::size() == 3);

    std::size_t pos10 = 0;
    std::size_t pos12 = 0;
    for (std::size_t i = 0; i < decltype(g)::size(); ++i) {
        if (ids[i] == 10) pos10 = i;
        if (ids[i] == 12) pos12 = i;
    }
    CHECK(pos10 < pos12);

    std::vector<std::string_view> order;

    g.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.mName), ...);
        }
    );

    CHECK(order.front() == "source");
    CHECK(order.back() == "sink");
}

TEST_CASE("complex topological sort test") {
    Proc1 psource("source");
    Proc1 pproc2("proc2");
    Proc1 psink("sink");
    Proc2 pproc1("proc1");
    Proc2 pmerger("merger");

    ugraph::PipelineVertex<20, Proc1, 0, 2, int> source(psource);
    ugraph::PipelineVertex<21, Proc2, 1, 1, int> proc1(pproc1);
    ugraph::PipelineVertex<22, Proc1, 1, 1, int> proc2(pproc2);
    ugraph::PipelineVertex<23, Proc2, 2, 1, int> merger(pmerger);
    ugraph::PipelineVertex<24, Proc1, 1, 0, int> sink(psink);

    auto g = ugraph::PipelineGraph(
        source.out<0>() >> proc1.in(),
        source.out<1>() >> proc2.in(),
        proc1.out() >> merger.in<0>(),
        proc2.out() >> merger.in<1>(),
        merger.out() >> sink.in()
    );

    auto ids2 = decltype(g)::ids();
    CHECK(decltype(g)::size() == 5);
    CHECK(ids2[0] == 20);
    CHECK(ids2[4] == 24);

    std::vector<std::string_view> order;
    order.reserve(decltype(g)::size());
    g.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.mName), ...);
        }
    );

    auto find_pos =
        [&] (std::string_view n) {
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] == n) return i;
        }
        return order.size();
        };

    auto p_source = find_pos("source");
    auto p_proc1 = find_pos("proc1");
    auto p_proc2 = find_pos("proc2");
    auto p_merger = find_pos("merger");
    auto p_sink = find_pos("sink");

    CHECK(p_source < p_proc1);
    CHECK(p_source < p_proc2);
    CHECK(p_proc1 < p_merger);
    CHECK(p_proc2 < p_merger);
    CHECK(p_merger < p_sink);
}

TEST_CASE("OrderedGraph test") {
    Proc1 pstart("start");
    Proc1 pend("end");
    Proc2 pmiddle("middle");

    ugraph::PipelineVertex<30, Proc1, 0, 1, int> start(pstart);
    ugraph::PipelineVertex<40, Proc2, 1, 1, int> middle(pmiddle);
    ugraph::PipelineVertex<50, Proc1, 1, 0, int> end(pend);

    auto ordered_graph = ugraph::PipelineGraph(
        middle.out() >> end.in(),
        start.out() >> middle.in()
    );

    auto ids = decltype(ordered_graph)::ids();
    CHECK(ids[0] == 30);
    CHECK(ids[1] == 40);
    CHECK(ids[2] == 50);

    std::vector<std::string_view> order;

    ordered_graph.apply(
        [&] (auto&... impls) {
            (order.emplace_back(impls.mName), ...);
        }
    );

    CHECK(order.size() == 3);
    CHECK(order[0] == "start");
    CHECK(order[1] == "middle");
    CHECK(order[2] == "end");
}


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
        CHECK(pipeline.modules.out.received[i] == doctest::Approx(expected[i]).epsilon(1e-6f));
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
    CHECK(pct_sum >= 60);
    CHECK(pct_sum <= 125);

    // Provide diagnostic info only on failure
    INFO(
        "pipeline_ns=" << pipeline_ns <<
        ", sum_vertices=" << sum_vertices <<
        ", pct_sum=" << pct_sum
    );
}
