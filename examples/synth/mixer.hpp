#pragma once

#include <cstddef>
#include "ugraph.hpp"
#include "audio_buffer.hpp"

template<std::size_t input_count>
struct Mixer {

    using Manifest = ugraph::Manifest<
        ugraph::IO<AudioBuff, input_count, 1>
    >;

    void process(ugraph::Context<Manifest>& ctx) {
        // sum inputs into output buffer

        auto& outBuf = ctx.template output<AudioBuff>();
        std::size_t size = outBuf.size();

        for (std::size_t i = 0; i < size; ++i) {
            float sum = 0.0f;
            for (const auto& in : ctx.template inputs<AudioBuff>()) {
                sum += in[i];
            }
            outBuf[i] = sum;
        }
    }

};
