#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "ugraph.hpp"
#include "trigger.hpp"

template<std::size_t voice_count>
struct VoiceManager {

    using Manifest = ugraph::Manifest<
        ugraph::IO<std::vector<Trigger>, 1, 0>,
        ugraph::IO<Trigger, 0, voice_count>
    >;

    void process(ugraph::Context<Manifest>& ctx) {

        for (auto& outTrig : ctx.template outputs<Trigger>()) {
            outTrig = { Trigger::eNone, 0 };
        }

        for (const auto& trigger : ctx.template input<std::vector<Trigger>>()) {

            switch (trigger.mState) {

                case Trigger::eOn:
                {

                    uint8_t freeSlotIdx = -1;

                    for (uint8_t i = 0; i < voice_count; i++) {
                        if (mVoiceData[i].mState) {
                            if (mVoiceData[i].mNoteNumber == trigger.mNoteNumber) {
                                freeSlotIdx = -1;
                                break;
                            }
                        }
                        else {
                            freeSlotIdx = i;
                        }
                    }

                    if (freeSlotIdx < voice_count) {
                        mVoiceData[freeSlotIdx].mNoteNumber = trigger.mNoteNumber;
                        mVoiceData[freeSlotIdx].mState = true;
                        ctx.template output<Trigger>(freeSlotIdx) = { Trigger::eOn, trigger.mNoteNumber };
                    }

                    break;
                }

                case Trigger::eOff:
                {

                    for (uint8_t i = 0; i < voice_count; i++) {

                        if (mVoiceData[i].mState && mVoiceData[i].mNoteNumber == trigger.mNoteNumber) {
                            mVoiceData[i].mNoteNumber = 0;
                            mVoiceData[i].mState = false;
                            ctx.template output<Trigger>(i) = { Trigger::eOff, trigger.mNoteNumber };
                            break;
                        }

                    }

                    break;
                }

                default:
                    break;
            }

        }

    }

private:

    struct VoiceData {
        bool mState = false;
        uint8_t mNoteNumber = 0;
    };

    VoiceData mVoiceData[voice_count];
};