#pragma once

#include <cstdint>
#include <array>
#include "ugraph.hpp"
#include "mixer.hpp"
#include "oscillator.hpp"
#include "audio_buffer.hpp"
#include "trigger.hpp"
#include <cmath>
#include "gain.hpp"
#include "envelope_generator.hpp"


enum eVoiceNodes {
    eOsc,
    eEnv,
    eGain
};

static auto makeGraph(Oscillator& osc, EnvelopeGenerator& env, Gain& gain) {

    auto oscN = ugraph::make_node<eOsc>(osc);
    auto envN = ugraph::make_node<eEnv>(env);
    auto gainN = ugraph::make_node<eGain>(gain);

    return ugraph::Graph(
        oscN.output<AudioBuff>() >> gainN.input<AudioBuff>(),
        envN.output<float>() >> gainN.input<float>()
    );
}

struct Voice {

    using graph_t =
        decltype(
            makeGraph(
                std::declval<Oscillator&>(),
                std::declval<EnvelopeGenerator&>(),
                std::declval<Gain&>()
            )
            );

    graph_t& getGraph() {
        return mGraph;
    }

    void print() {
        mGraph.print(std::cout);
    }

private:

    Oscillator mOscillator;
    EnvelopeGenerator mEnv;
    Gain mGain;

    graph_t mGraph = makeGraph(mOscillator, mEnv, mGain);

};
