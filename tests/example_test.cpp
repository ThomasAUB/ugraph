#include "doctest.h"

#include "../examples/synth/synth.hpp"

#include "../examples/synth/voice.hpp"

TEST_CASE("example test") {

    static constexpr uint32_t buffer_size = 64;

    float buff[buffer_size] {};
    bool isNoneZero = false;

    Synth synth;

    synth.print();

    synth.setBufferSize(buffer_size);

    synth.process(buff, buffer_size);

    for (auto& s : buff) {
        if (s != 0) {
            isNoneZero = true;
        }
    }

    CHECK(!isNoneZero);

    synth.addNote(64, 32, true);

    synth.process(buff, sizeof(buff) / sizeof(buff[0]));

    for (auto& s : buff) {
        if (s != 0) {
            isNoneZero = true;
        }
    }

    CHECK(isNoneZero);

}
