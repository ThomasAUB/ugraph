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
        Trigger& trigger,
        Oscillator& osc,
        EnvelopeGenerator& env,
        Gain& gain,
        AudioBuff& outBuff
    ) {

        auto oscillatorNode = ugraph::make_node<__COUNTER__>(osc);
        auto envelopeNode = ugraph::make_node<__COUNTER__>(env);
        auto gainNode = ugraph::make_node<__COUNTER__>(gain);

        return ugraph::ExternalDataGraph(
            trigger | oscillatorNode.input<Trigger>(),
            trigger | envelopeNode.input<Trigger>(),
            oscillatorNode.output<AudioBuff>() >> gainNode.input<AudioBuff>(),
            envelopeNode.output<float>() >> gainNode.input<Gain::Parameter>(),
            gainNode.output<AudioBuff>() | outBuff
        );
    }

    struct Voice {

        using Manifest = ugraph::Manifest<
            ugraph::IO<AudioBuff, 0, 1>,
            ugraph::IO<Trigger, 1, 0>
        >;

        using graph_helper_t = GraphHelper<makeVoiceGraph>;
        using graph_t = graph_helper_t::graph_t;
        using graph_data_t = graph_helper_t::graph_data_t;

        graph_t& getGraph() {
            return mGraph;
        }

        void process(ugraph::Context<Manifest>& ctx) {
            mOutput = ctx.output<AudioBuff>();
            mTrigger = ctx.input<Trigger>();

            mGraph.for_each(
                [] (auto& node, auto& graphCtx) {
                    node.process(graphCtx);
                }
            );
        }

    private:

        Oscillator mOscillator;
        EnvelopeGenerator mEnv;
        Gain mGain;

        Trigger mTrigger;
        AudioBuff mOutput;

        graph_t mGraph = makeVoiceGraph(
            mTrigger,
            mOscillator,
            mEnv,
            mGain,
            mOutput
        );
    };

} // namespace synth_example

using synth_example::Voice;
