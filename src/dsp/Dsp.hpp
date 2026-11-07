#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ec::dsp {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampSafe(float v, float lo, float hi) noexcept {
    if (!std::isfinite(v)) return 0.0f;
    return std::clamp(v, lo, hi);
}

inline float flushDenormal(float v) noexcept {
    return (std::abs(v) < 1.0e-25f) ? 0.0f : v;
}

inline float dbToGain(float db) noexcept {
    return std::pow(10.0f, db / 20.0f);
}

inline float midiToHz(float midi) noexcept {
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

inline std::uint32_t nextRandom(std::uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

inline float whiteNoise(std::uint32_t& state) noexcept {
    const std::uint32_t r = nextRandom(state);
    return static_cast<float>(r & 0xFFFFFF) / 8388608.0f - 1.0f;
}

class SineTable {
public:
    static const SineTable& instance() {
        static const SineTable table;
        return table;
    }

    float at(float phase01) const noexcept {
        phase01 -= std::floor(phase01);
        const float p = phase01 * static_cast<float>(size);
        const auto index = static_cast<int>(p);
        const float frac = p - static_cast<float>(index);
        const float a = data[static_cast<std::size_t>(index & (size - 1))];
        const float b = data[static_cast<std::size_t>((index + 1) & (size - 1))];
        return a + (b - a) * frac;
    }

private:
    static constexpr int size = 4096;
    std::array<float, size + 1> data{};
    SineTable() {
        for (int i = 0; i <= size; ++i) {
            data[static_cast<std::size_t>(i)] = std::sin(kTwoPi * static_cast<float>(i) /
                                                         static_cast<float>(size));
        }
    }
};

inline float sine01(float phase) noexcept {
    return SineTable::instance().at(phase);
}

class Biquad {
public:
    void reset() noexcept {
        z1_ = z2_ = 0.0f;
    }

    void setLowpass(float frequency, float q, float sampleRate) noexcept {
        const float w0 = kTwoPi * std::clamp(frequency, 10.0f, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * std::max(0.05f, q));
        const float a0 = 1.0f + alpha;
        b0_ = (1.0f - cosw) * 0.5f / a0;
        b1_ = (1.0f - cosw) / a0;
        b2_ = b0_;
        a1_ = -2.0f * cosw / a0;
        a2_ = (1.0f - alpha) / a0;
    }

    void setHighpass(float frequency, float q, float sampleRate) noexcept {
        const float w0 = kTwoPi * std::clamp(frequency, 10.0f, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * std::max(0.05f, q));
        const float a0 = 1.0f + alpha;
        b0_ = (1.0f + cosw) * 0.5f / a0;
        b1_ = -(1.0f + cosw) / a0;
        b2_ = b0_;
        a1_ = -2.0f * cosw / a0;
        a2_ = (1.0f - alpha) / a0;
    }

    void setBandpass(float frequency, float q, float sampleRate) noexcept {
        const float w0 = kTwoPi * std::clamp(frequency, 10.0f, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * std::max(0.05f, q));
        const float a0 = 1.0f + alpha;
        b0_ = alpha / a0;
        b1_ = 0.0f;
        b2_ = -alpha / a0;
        a1_ = -2.0f * cosw / a0;
        a2_ = (1.0f - alpha) / a0;
    }

    void setPeaking(float frequency, float q, float gainDb, float sampleRate) noexcept {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = kTwoPi * std::clamp(frequency, 10.0f, sampleRate * 0.45f) / sampleRate;
        const float alpha = std::sin(w0) / (2.0f * std::max(0.05f, q));
        const float a0 = 1.0f + alpha / A;
        b0_ = (1.0f + alpha * A) / a0;
        b1_ = -2.0f * std::cos(w0) / a0;
        b2_ = (1.0f - alpha * A) / a0;
        a1_ = -2.0f * std::cos(w0) / a0;
        a2_ = (1.0f - alpha / A) / a0;
    }

    void setLowShelf(float frequency, float gainDb, float sampleRate) noexcept {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = kTwoPi * std::clamp(frequency, 10.0f, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos(w0);
        const float sinw = std::sin(w0);
        const float alpha = sinw / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        const float a0 = (A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha;
        b0_ = A * ((A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha) / a0;
        b1_ = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
        b2_ = A * ((A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha) / a0;
        a1_ = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
        a2_ = ((A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha) / a0;
    }

    void setHighShelf(float frequency, float gainDb, float sampleRate) noexcept {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = kTwoPi * std::clamp(frequency, 10.0f, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos(w0);
        const float sinw = std::sin(w0);
        const float alpha = sinw / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        const float a0 = (A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha;
        b0_ = A * ((A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha) / a0;
        b1_ = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
        b2_ = A * ((A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha) / a0;
        a1_ = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
        a2_ = ((A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha) / a0;
    }

    float process(float x) noexcept {
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        z1_ = flushDenormal(z1_);
        z2_ = flushDenormal(z2_);
        return y;
    }

private:
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

class OnePole {
public:
    void setCutoff(float hz, float sampleRate) noexcept {
        const float x = std::exp(-kTwoPi * std::clamp(hz, 1.0f, sampleRate * 0.45f) / sampleRate);
        a_ = 1.0f - x;
        b_ = x;
    }
    float process(float v) noexcept {
        z_ = flushDenormal(a_ * v + b_ * z_);
        return z_;
    }
    void reset() noexcept { z_ = 0.0f; }

private:
    float a_ = 1.0f, b_ = 0.0f, z_ = 0.0f;
};

} // namespace ec::dsp
