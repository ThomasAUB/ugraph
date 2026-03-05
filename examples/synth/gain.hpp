#pragma once

#include "ugraph.hpp"
#include "audio_buffer.hpp"
#include <cmath>
#include <algorithm>

struct Gain {

    using Manifest = ugraph::Manifest<
        ugraph::IO<AudioBuff, 1, 1>,
        ugraph::IO<float, 1, 0>
    >;

    void process(ugraph::Context<Manifest>& ctx) {

        auto& out = ctx.template output<AudioBuff>();
        auto& in = ctx.template input<AudioBuff>();
        const auto size = out.size();

        const auto gain = ctx.template input<float>();

        const auto eps = 1e-8f;
        const auto s = static_cast<float>(size);

        // If buffer is empty or gain already equal (within eps), do a quick path
        if (size == 0) return;

        if (std::fabs(gain - mCurrentGain) <= eps) {
            for (std::size_t i = 0; i < size; ++i)
                out[i] = in[i] * gain;
            mCurrentGain = gain;
            return;
        }

        // Use geometric (log-domain) ramp so amplitude changes are perceptually smooth.
        // This computes a per-sample multiplier so the gain changes exponentially
        // from current -> target across the block.
        const float start = std::max(mCurrentGain, eps);
        const float target = std::max(gain, eps);
        const float ratio = target / start;
        const float step = std::pow(ratio, 1.0f / static_cast<float>(size));

        float g = mCurrentGain <= 0.0f ? eps : mCurrentGain;
        for (std::size_t i = 0; i < size; ++i) {
            out[i] = in[i] * g;
            g *= step;
        }

        // Ensure we finish exactly at the target value (avoid accumulated error)
        mCurrentGain = gain;
    }

private:

    float mCurrentGain = 0;

};