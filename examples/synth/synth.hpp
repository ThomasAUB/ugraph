#pragma once

#include <cstddef>
#include <array>
#include <utility>
#include <vector>

#include "voice_manager.hpp"
#include "oscillator.hpp"
#include "envelope_generator.hpp"
#include "gain.hpp"
#include "mixer.hpp"

template<std::size_t voice_count>
static auto makeGraph(
    VoiceManager<voice_count>& voiceMgr,
    std::array<Oscillator, voice_count>& oscillators,
    std::array<EnvelopeGenerator, voice_count>& envelopes,
    std::array<Gain, voice_count>& gains,
    Mixer<voice_count>& mixer
) {
    // Stable compile-time IDs for manager, voices and mixer
    constexpr std::size_t mgr_id = 8000;
    constexpr std::size_t voice_base_id = 10000;
    constexpr std::size_t voice_id_stride = 10;
    constexpr std::size_t osc_offset = 0;
    constexpr std::size_t env_offset = 1;
    constexpr std::size_t gain_offset = 2;
    constexpr std::size_t mixer_id = 9000;

    // Single templated lambda that builds the graph for the given index sequence
    auto build_impl = [&]<std::size_t... I>(std::index_sequence<I...>) {
        auto mgr_node = ugraph::make_node<mgr_id>(voiceMgr);

        auto osc_nodes = std::make_tuple(ugraph::make_node<voice_base_id + (I * voice_id_stride) + osc_offset>(oscillators[I])...);
        auto env_nodes = std::make_tuple(ugraph::make_node<voice_base_id + (I * voice_id_stride) + env_offset>(envelopes[I])...);
        auto gain_nodes = std::make_tuple(ugraph::make_node<voice_base_id + (I * voice_id_stride) + gain_offset>(gains[I])...);

        // mixer node
        auto mix_node = ugraph::make_node<mixer_id>(mixer);

        // edges: manager outputs -> oscillator/env inputs
        auto mgr_osc_edges = std::make_tuple((mgr_node.template output<Trigger, I>() >> std::get<I>(osc_nodes).template input<Trigger>())...);
        auto mgr_env_edges = std::make_tuple((mgr_node.template output<Trigger, I>() >> std::get<I>(env_nodes).template input<Trigger>())...);

        // edges: osc + env -> gain
        auto voice_audio_edges = std::make_tuple((std::get<I>(osc_nodes).template output<AudioBuff>() >> std::get<I>(gain_nodes).template input<AudioBuff>())...);
        auto voice_env_edges = std::make_tuple((std::get<I>(env_nodes).template output<float>() >> std::get<I>(gain_nodes).template input<float>())...);

        // edges: gain outputs -> mixer inputs
        auto mix_edges = std::make_tuple((std::get<I>(gain_nodes).template output<AudioBuff>() >> mix_node.template input<AudioBuff, I>())...);

        auto edges = std::tuple_cat(mgr_osc_edges, mgr_env_edges, voice_audio_edges, voice_env_edges, mix_edges);

        return std::apply([] (auto const&... es) { return ugraph::Graph(es...); }, edges);
    };

    return build_impl(std::make_index_sequence<voice_count>{});
}

struct Synth {

    static constexpr std::size_t manager_node_id = 8000;
    static constexpr std::size_t mixer_node_id = 9000;

    Synth() {
        mGraph.init_graph_data(mSynthGraphData);
        mGraph.bind_input<manager_node_id>(mTriggers);
        mGraph.bind_output<mixer_node_id>(mOutputBuffer);
        // Reserve triggers to avoid heap allocations on the audio thread
        mTriggers.reserve(128);

        initAudioBuff();

        print();
    }

    void addNote(uint8_t noteNumber, uint8_t velocity, bool state) {
        mTriggers.push_back(Trigger { state ? Trigger::eOn : Trigger::eOff, noteNumber });
    }

    void process(float* output, std::size_t size) {

        mOutputBuffer = { output, size };

        initAudioBuff(size);

        if (!mGraph.all_ios_connected()) {
            // Consume/clear pending triggers even if graph isn't ready
            mTriggers.clear();
            return;
        }

        mGraph.for_each(
            [] (auto& n, auto& ctx) {
                n.process(ctx);
            }
        );

        mTriggers.clear();
    }

    void print() {
        mGraph.print(std::cout);
    }

private:

    void initAudioBuff(std::size_t size = max_buffer_size) {

        // Clamp requested size to avoid out-of-bounds on fixed storage
        if (size > max_buffer_size) {
            size = max_buffer_size;
        }

        if (size == mPrevBufferSize) {
            return;
        }

        mPrevBufferSize = size;

        for (std::size_t i = 0; i < mSynthBufferStorage.size(); i++) {
            ugraph::data_at<AudioBuff>(mSynthGraphData, i) = { mSynthBufferStorage[i].data(), size };
        }

    }

    static constexpr std::size_t voice_count = 4;

    VoiceManager<voice_count> mVoiceMgr;
    std::array<Oscillator, voice_count> mOscillators;
    std::array<EnvelopeGenerator, voice_count> mEnvelopes;
    std::array<Gain, voice_count> mGains;
    Mixer<voice_count> mMixer;

    using synth_graph_t = 
        decltype(
            makeGraph(
                std::declval<VoiceManager<voice_count>&>(), 
                std::declval<std::array<Oscillator, voice_count>&>(),
                std::declval<std::array<EnvelopeGenerator, voice_count>&>(),
                std::declval<std::array<Gain, voice_count>&>(),
                std::declval<Mixer<voice_count>&>()
            )
        );

    static constexpr auto synth_buffer_count = synth_graph_t::data_count<AudioBuff>();
    static constexpr uint32_t max_buffer_size = 1024;
    using buffer_t = std::array<float, max_buffer_size>;

    std::array<buffer_t, synth_buffer_count> mSynthBufferStorage;

    synth_graph_t mGraph = makeGraph(mVoiceMgr, mOscillators, mEnvelopes, mGains, mMixer);

    typename synth_graph_t::graph_data_t mSynthGraphData;

    std::size_t mPrevBufferSize = 0;

    std::vector<Trigger> mTriggers;

    AudioBuff mOutputBuffer;
};