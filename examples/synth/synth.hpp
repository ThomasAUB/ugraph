#pragma once

#include <cstddef>
#include <array>
#include <vector>
#include <iostream>

#include "graph_helper.hpp"

#include "voice_manager.hpp"
#include "voice.hpp"
#include "mixer.hpp"

static constexpr std::size_t voice_count = 4;

static auto makeSynthGraph(
    std::vector<Trigger>& triggers,
    AudioBuff& outputBuffer,
    VoiceManager<voice_count>& voiceMgr,
    std::array<Voice, voice_count>& inVoices,
    Mixer<voice_count>& mixer
) {

    auto voiceMgrN = ugraph::make_node<__COUNTER__>(voiceMgr);

    auto voiceN1 = ugraph::make_node<__COUNTER__>(inVoices[0]);
    auto voiceN2 = ugraph::make_node<__COUNTER__>(inVoices[1]);
    auto voiceN3 = ugraph::make_node<__COUNTER__>(inVoices[2]);
    auto voiceN4 = ugraph::make_node<__COUNTER__>(inVoices[3]);

    auto mixerN = ugraph::make_node<__COUNTER__>(mixer);

    return ugraph::Graph(

        triggers | voiceMgrN.input<std::vector<Trigger>>(),

        voiceMgrN.output<Trigger, 0>() >> voiceN1.input<Trigger>(),
        voiceMgrN.output<Trigger, 1>() >> voiceN2.input<Trigger>(),
        voiceMgrN.output<Trigger, 2>() >> voiceN3.input<Trigger>(),
        voiceMgrN.output<Trigger, 3>() >> voiceN4.input<Trigger>(),

        voiceN1.output<AudioBuff>() >> mixerN.input<AudioBuff, 0>(),
        voiceN2.output<AudioBuff>() >> mixerN.input<AudioBuff, 1>(),
        voiceN3.output<AudioBuff>() >> mixerN.input<AudioBuff, 2>(),
        voiceN4.output<AudioBuff>() >> mixerN.input<AudioBuff, 3>(),

        mixerN.output<AudioBuff>() | outputBuffer

    );

}

struct Synth {

    Synth() {
        // Reserve triggers to avoid heap allocations on the audio thread
        mTriggers.reserve(128);

        for (auto& v : mVoices) {
            v.getGraph().init(mVoiceData);
        }
    }

    void setBufferSize(std::size_t inBuffSize) {

        auto& data = mGraph.graph_data();

        for (std::size_t i = 0; i < mSynthBufferStorage.size(); i++) {
            mSynthBufferStorage[i].resize(inBuffSize);
            data.slot<AudioBuff>(i) = { mSynthBufferStorage[i].data(), inBuffSize };
        }

        for (std::size_t i = 0; i < mVoiceBufferStorage.size(); i++) {
            mVoiceBufferStorage[i].resize(inBuffSize);
            mVoiceData.slot<AudioBuff>(i) = { mVoiceBufferStorage[i].data(), inBuffSize };
        }

    }

    void addNote(uint8_t noteNumber, uint8_t velocity, bool state) {
        mTriggers.push_back(Trigger { state ? Trigger::eOn : Trigger::eOff, noteNumber });
    }

    void process(float* output, std::size_t size) {

        mOutputBuffer = { output, size };

        mGraph.for_each(
            [] (auto& n, auto& ctx) {
                n.process(ctx);
            }
        );

        mTriggers.clear();
    }

    void print() {
        mGraph.print(std::cout);
        mVoices[0].getGraph().print(std::cout);
    }

private:

    std::array<Voice, voice_count> mVoices;
    VoiceManager<voice_count> mVoiceMgr;
    Mixer<voice_count> mMixer;

    std::vector<Trigger> mTriggers;
    AudioBuff mOutputBuffer;

    using graph_helper_t = GraphHelper<makeSynthGraph>;
    using synth_graph_t = graph_helper_t::graph_t;

    std::array<
        std::vector<float>,
        synth_graph_t::graph_data_t::count<AudioBuff>()
    > mSynthBufferStorage {};

    std::array<
        std::vector<float>,
        Voice::graph_data_t::count<AudioBuff>()
    > mVoiceBufferStorage {};

    Voice::graph_data_t mVoiceData;

    synth_graph_t mGraph = makeSynthGraph(
        mTriggers,
        mOutputBuffer,
        mVoiceMgr,
        mVoices,
        mMixer
    );

};
