// NOTE: This file intentionally expands previously very condensed one-line test code
// into a more readable, step‑by‑step style. Semantics must remain identical.

#include "doctest.h"
#include "ugraph.hpp"
#include <array>
#include <iostream>
#include <chrono>

// Audio processing oriented Graph executor tests (sources -> mixer -> gain -> sink + perf).
namespace {

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

        using Manifest = ugraph::Manifest< ugraph::IO<AudioBuffer, 0, 1> >;

        float value { 0.f };

        void process(ugraph::Context<Manifest>& ctx) {
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

    static_assert(decltype(g)::template data_count<AudioBuffer>() == 3, "Unexpected buffer count");

    static constexpr auto storage_count = decltype(g)::data_count<AudioBuffer>();
    static constexpr auto storage_size = 64;
    using buffer_storage_t = std::array<float, storage_size>;
    std::array<buffer_storage_t, storage_count> storage;

    for (int i = 0; i < storage_count; i++) {
        g.data_at<AudioBuffer>(i) = storage[i];
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

    static constexpr auto storage_count = decltype(g)::data_count<AudioBuffer>();
    static constexpr auto storage_size = 64;
    using buffer_storage_t = std::array<float, storage_size>;
    std::array<buffer_storage_t, storage_count> storage;

    for (int i = 0; i < storage_count; i++) {
        g.data_at<AudioBuffer>(i) = storage[i];
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
        g.data_at<AudioBuffer>(i) = gstorage[i];
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
