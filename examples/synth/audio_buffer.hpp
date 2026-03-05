#pragma once

#include <cstddef>

struct AudioBuff {

    AudioBuff() = default;

    AudioBuff(float* ptr, std::size_t s) :
        mPtr(ptr), mSize(s) {}

    auto begin() {
        return mPtr;
    }

    auto end() {
        return mPtr + mSize;
    }

    const auto begin() const {
        return mPtr;
    }

    const auto end() const {
        return mPtr + mSize;
    }

    std::size_t size() const {
        return mSize;
    }

    auto& operator [](std::size_t i) {
        return mPtr[i];
    }

    const auto& operator [](std::size_t i) const {
        return mPtr[i];
    }

private:
    float* mPtr = nullptr;
    std::size_t mSize = 0;
};