#pragma once

#include <cstddef>
#include <array>
#include <vector>
#include <iostream>

#include "graph_helper.hpp"

#include "voice_manager.hpp"
#include "voice.hpp"
#include "mixer.hpp"

namespace synth_example {

    static constexpr std::size_t voice_count = 4;

    template<typename BufferStorage, typename GraphData>
    void resizeAudioSlots(BufferStorage& storage, GraphData& graphData, std::size_t bufferSize) {
        for (std::size_t i = 0; i < storage.size(); ++i) {
            storage[i].resize(bufferSize);
            graphData.template slot<AudioBuff>(i) = { storage[i].data(), bufferSize };
        }
    }

    static auto makeSynthGraph(
        std::vector<Trigger>& triggers,
        AudioBuff& outputBuffer,
        VoiceManager<voice_count>& voiceMgr,
        std::array<Voice, voice_count>& inVoices,
        Mixer<voice_count>& mixer
    ) {

        auto voiceMgrNode = ugraph::make_node<__COUNTER__>(voiceMgr);

        auto voiceNode1 = ugraph::make_node<__COUNTER__>(inVoices[0]);
        auto voiceNode2 = ugraph::make_node<__COUNTER__>(inVoices[1]);
        auto voiceNode3 = ugraph::make_node<__COUNTER__>(inVoices[2]);
        auto voiceNode4 = ugraph::make_node<__COUNTER__>(inVoices[3]);

        auto mixerNode = ugraph::make_node<__COUNTER__>(mixer);

        return ugraph::Graph(

            triggers | voiceMgrNode.input<std::vector<Trigger>>(),

            voiceMgrNode.output<Trigger, 0>() >> voiceNode1.input<Trigger>(),
            voiceMgrNode.output<Trigger, 1>() >> voiceNode2.input<Trigger>(),
            voiceMgrNode.output<Trigger, 2>() >> voiceNode3.input<Trigger>(),
            voiceMgrNode.output<Trigger, 3>() >> voiceNode4.input<Trigger>(),

            voiceNode1.output<AudioBuff>() >> mixerNode.input<AudioBuff, 0>(),
            voiceNode2.output<AudioBuff>() >> mixerNode.input<AudioBuff, 1>(),
            voiceNode3.output<AudioBuff>() >> mixerNode.input<AudioBuff, 2>(),
            voiceNode4.output<AudioBuff>() >> mixerNode.input<AudioBuff, 3>(),

            mixerNode.output<AudioBuff>() | outputBuffer

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
            resizeAudioSlots(mSynthBufferStorage, mGraph.graph_data(), inBuffSize);
            resizeAudioSlots(mVoiceBufferStorage, mVoiceData, inBuffSize);
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

} // namespace synth_example

using synth_example::Synth;
