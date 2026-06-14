#pragma once

#include "ugraph.hpp"
#include "graph_helper.hpp"

#include "audio_buffer.hpp"
#include "envelope_generator.hpp"
#include "gain.hpp"
#include "mixer.hpp"
#include "oscillator.hpp"
#include "trigger.hpp"

namespace synth_example {

    static auto makeVoiceGraph(
        Oscillator& osc,
        EnvelopeGenerator& env,
        Gain& gain
    ) {

        auto graphInput = ugraph::graph_io::make_input<__COUNTER__>();
        auto graphOutput = ugraph::graph_io::make_output<__COUNTER__>();

        auto oscillatorNode = ugraph::make_node<__COUNTER__>(osc);
        auto envelopeNode = ugraph::make_node<__COUNTER__>(env);
        auto gainNode = ugraph::make_node<__COUNTER__>(gain);

        return ugraph::ExternalDataGraph(
            graphInput.output<Trigger>() >> oscillatorNode.input<Trigger>(),
            graphInput.output<Trigger>() >> envelopeNode.input<Trigger>(),
            oscillatorNode.output<AudioBuff>() >> gainNode.input<AudioBuff>(),
            envelopeNode.output<float>() >> gainNode.input<Gain::Parameter>(),
            gainNode.output<AudioBuff>() >> graphOutput.input<AudioBuff>()
        );

    }

    struct Voice {

        using graph_helper_t = GraphHelper<makeVoiceGraph>;
        using graph_t = graph_helper_t::graph_t;
        using graph_data_t = graph_helper_t::graph_data_t;

        // The Voice's manifest is deduced from the graph's declared IOs.
        // This allows Voice to be used as a module in an outer graph.
        using Manifest = graph_t::io_manifest;

        graph_t& getGraph() {
            return mGraph;
        }

    private:

        Oscillator mOscillator;
        EnvelopeGenerator mEnv;
        Gain mGain;

        graph_t mGraph = makeVoiceGraph(
            mOscillator,
            mEnv,
            mGain
        );
    };

} // namespace synth_example

using synth_example::Voice;
