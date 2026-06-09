#pragma once

#include "ugraph.hpp"
#include "audio_buffer.hpp"
#include "envelope_generator.hpp"
#include "gain.hpp"
#include "mixer.hpp"
#include "oscillator.hpp"
#include "trigger.hpp"

static auto makeGraph(
    Trigger& trigger,
    Oscillator& osc,
    EnvelopeGenerator& env,
    Gain& gain,
    AudioBuff& outBuff
) {

    auto oscN = ugraph::make_node<__COUNTER__>(osc);
    auto envN = ugraph::make_node<__COUNTER__>(env);
    auto gainN = ugraph::make_node<__COUNTER__>(gain);

    return ugraph::ExternalDataGraph(
        trigger | oscN.input<Trigger>(),
        trigger | envN.input<Trigger>(),
        oscN.output<AudioBuff>() >> gainN.input<AudioBuff>(),
        envN.output<float>() >> gainN.input<float>(),
        gainN.output<AudioBuff>() | outBuff
    );
}

struct Voice {

    using Manifest = ugraph::Manifest<
        ugraph::IO<AudioBuff, 0, 1>,
        ugraph::IO<Trigger, 1, 0>
    >;

    using graph_t =
        decltype(
            makeGraph(
                std::declval<Trigger&>(),
                std::declval<Oscillator&>(),
                std::declval<EnvelopeGenerator&>(),
                std::declval<Gain&>(),
                std::declval<AudioBuff&>()
            )
            );

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

    graph_t mGraph = makeGraph(
        mTrigger,
        mOscillator,
        mEnv,
        mGain,
        mOutput
    );
};
