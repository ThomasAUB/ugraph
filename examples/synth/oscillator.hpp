#pragma once

#include "ugraph.hpp"
#include "audio_buffer.hpp"
#include "trigger.hpp"
#include <cmath>

struct Oscillator {

    using Manifest = ugraph::Manifest<
        ugraph::IO<Trigger, 1, 0>,
        ugraph::IO<AudioBuff, 0, 1>
    >;

    void process(ugraph::Context<Manifest>& ctx) {

        const auto& trigger = ctx.input<Trigger>();

        if(trigger.mState == Trigger::eOn) {
            mFrequency = 440.0 * std::pow(2.0, (static_cast<int>(trigger.mNoteNumber) - 69) / 12.0);
        }

        auto& outBuf = ctx.template output<AudioBuff>();

        const float phaseInc = two_pi * mFrequency / mSampleRate;

        for(auto& s : outBuf) {
            s = mAmplitude * std::sin(mPhase);
            mPhase += phaseInc;
            if (mPhase >= two_pi) {
                mPhase -= two_pi;
            }
        }
    }

private:
    static constexpr float two_pi = 2.0f * 3.14159265358979323846f;
    float mFrequency = 440.0f;
    float mAmplitude = 1.f;
    float mPhase = 0.0f;
    float mSampleRate = 48000.0f;
};