#pragma once

#include <cstdint>

struct Trigger {
    enum eStateChange {
        eNone,
        eOn,
        eOff
    };
    eStateChange mState = eNone;
    uint8_t mNoteNumber = 0;
};