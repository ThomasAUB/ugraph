#pragma once

#include "ugraph.hpp"
#include "trigger.hpp"

struct EnvelopeGenerator {

    using Manifest = ugraph::Manifest<
        ugraph::IO<Trigger, 1, 0>,
        ugraph::IO<float, 0, 1>
    >;

    bool isTerminated() const {
        return mState == eOff;
    }

    void process(ugraph::Context<Manifest>& ctx) {

        switch(ctx.template input<Trigger>().mState) {

            case Trigger::eStateChange::eOn:
                mState = eAttack;
                mValue = 0;
                break;

            case Trigger::eStateChange::eOff:
                mState = eRelease;
                break;

            default:
                break;
        }

        constexpr float adInc = 0.5;
        constexpr float rInc = 0.005;

        switch (mState) {

            case eAttack:
                mValue += adInc;
                if (mValue >= 1.0) {
                    mValue = 1.0;
                    mState++;
                }
                break;

            case eDecay:
                mValue -= rInc;
                if (mValue <= 0.5) {
                    mState++;
                }
                break;

            case eSustain:
                mValue = 0.5;
                break;

            case eRelease:
                mValue -= rInc;
                if (mValue <= 0) {
                    mValue = 0;
                    mState++;
                }
                break;

            default:
                mValue = 0;
                break;

        }

        ctx.output<float>() = mValue;
    }

private:

    enum eState {
        eAttack,
        eDecay,
        eSustain,
        eRelease,
        eOff
    };

    uint8_t mState = eOff;
    float mValue = 0;
};