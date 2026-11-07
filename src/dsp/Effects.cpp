#include "dsp/Effects.hpp"

#include <algorithm>
#include <cmath>

namespace ec::dsp {

void Reverb::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    const int maxDelay = static_cast<int>(std::ceil(sampleRate_ * 0.14)) + 8;
    static constexpr float baseSeconds[kLines] = {0.0297f, 0.0371f, 0.0411f, 0.0437f};
    for (int i = 0; i < kLines; ++i) {
        delay_[static_cast<std::size_t>(i)].assign(static_cast<std::size_t>(maxDelay), 0.0f);
        length_[static_cast<std::size_t>(i)] = std::clamp(
            static_cast<int>(baseSeconds[i] * static_cast<float>(sampleRate_)), 8, maxDelay - 1);
        write_[static_cast<std::size_t>(i)] = 0;
        lfoPhase_[static_cast<std::size_t>(i)] = static_cast<float>(i) * 0.23f;
        lfoInc_[static_cast<std::size_t>(i)] = (0.11f + 0.033f * static_cast<float>(i)) /
                                               static_cast<float>(sampleRate_);
    }
    preLength_ = std::clamp(static_cast<int>(0.020 * sampleRate_), 1, maxDelay);
    preDelay_.assign(static_cast<std::size_t>(preLength_), 0.0f);
    preWrite_ = 0;
    reset();
    (void)maxBlockSize;
}

void Reverb::reset() {
    for (auto& line : delay_) std::fill(line.begin(), line.end(), 0.0f);
    for (auto& w : write_) w = 0;
    for (auto& d : damping_) d.reset();
    std::fill(preDelay_.begin(), preDelay_.end(), 0.0f);
    preWrite_ = 0;
}

void Reverb::process(const float* sendLeft, const float* sendRight, float* wetLeft, float* wetRight,
                     int frames, float mix, float size, float damping) {
    if (!sendLeft || !sendRight || !wetLeft || !wetRight || frames <= 0) return;
    const float sizeFactor = 0.65f + 1.15f * std::clamp(size, 0.0f, 1.0f);
    const float rt60 = 1.6f + 3.2f * std::clamp(size, 0.0f, 1.0f);
    const float dampCutoff = 12000.0f - 9000.0f * std::clamp(damping, 0.0f, 1.0f);
    for (auto& d : damping_) d.setCutoff(dampCutoff, static_cast<float>(sampleRate_));
    const float wets = std::clamp(mix, 0.0f, 1.0f);
    for (int i = 0; i < frames; ++i) {
        const float in = (sendLeft[i] + sendRight[i]) * 0.5f;
        preDelay_[static_cast<std::size_t>(preWrite_)] = in;
        preWrite_ = (preWrite_ + 1) % preLength_;
        const float delayed = preDelay_[static_cast<std::size_t>(preWrite_)];

        std::array<float, kLines> taps{};
        for (int l = 0; l < kLines; ++l) {
            auto& line = delay_[static_cast<std::size_t>(l)];
            const int len = length_[static_cast<std::size_t>(l)];
            const float effectiveDelay = static_cast<float>(len) * sizeFactor;
            lfoPhase_[static_cast<std::size_t>(l)] += lfoInc_[static_cast<std::size_t>(l)];
            if (lfoPhase_[static_cast<std::size_t>(l)] >= 1.0f) lfoPhase_[static_cast<std::size_t>(l)] -= 1.0f;
            const float modulation = 0.00035f * sine01(lfoPhase_[static_cast<std::size_t>(l)]);
            float readPos = static_cast<float>(write_[static_cast<std::size_t>(l)]) -
                            effectiveDelay * (1.0f + modulation);
            const float lenF = static_cast<float>(len);
            while (readPos < 0.0f) readPos += lenF;
            while (readPos >= lenF) readPos -= lenF;
            const int i0 = static_cast<int>(readPos);
            const int i1 = (i0 + 1) % len;
            const float frac = readPos - static_cast<float>(i0);
            taps[static_cast<std::size_t>(l)] = line[static_cast<std::size_t>(i0)] * (1.0f - frac) +
                                                line[static_cast<std::size_t>(i1)] * frac;
            taps[static_cast<std::size_t>(l)] = damping_[static_cast<std::size_t>(l)].process(taps[static_cast<std::size_t>(l)]);
        }

        // Hadamard feedback matrix keeps the four lines decorrelated and stable.
        const float a = taps[0], b = taps[1], c = taps[2], d = taps[3];
        const std::array<float, kLines> fb{
            (a + b + c + d) * 0.5f,
            (a - b + c - d) * 0.5f,
            (a + b - c - d) * 0.5f,
            (a - b - c + d) * 0.5f};
        for (int l = 0; l < kLines; ++l) {
            const float seconds = static_cast<float>(length_[static_cast<std::size_t>(l)]) /
                                  static_cast<float>(sampleRate_);
            float gain = std::pow(10.0f, -3.0f * seconds / rt60);
            gain = std::clamp(gain, 0.0f, 0.92f);
            auto& line = delay_[static_cast<std::size_t>(l)];
            const int w = write_[static_cast<std::size_t>(l)];
            line[static_cast<std::size_t>(w)] = flushDenormal(delayed * 0.22f + fb[static_cast<std::size_t>(l)] * gain);
            write_[static_cast<std::size_t>(l)] = (w + 1) % length_[static_cast<std::size_t>(l)];
        }
        wetLeft[i] = (taps[0] + taps[2]) * 0.55f * wets;
        wetRight[i] = (taps[1] + taps[3]) * 0.55f * wets;
    }
}

void MasterBus::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    maxBlockSize_ = std::max(1, maxBlockSize);
    reverb_.prepare(sampleRate_, maxBlockSize_);
    sendL_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    sendR_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    reset();
}

void MasterBus::reset() {
    reverb_.reset();
    lowL_.reset();
    lowR_.reset();
    highL_.reset();
    highR_.reset();
    dcIn_ = {0.0f, 0.0f};
    dcOut_ = {0.0f, 0.0f};
    gain_ = 1.0f;
    compEnv_ = 1.0f;
    limiterEnv_ = 1.0f;
    lastGainReductionDb_ = 0.0f;
    lastPeak_ = 0.0f;
    limiterActivations_ = 0;
}

void MasterBus::setParams(const MasterParams& params) {
    params_ = params;
    gainTarget_ = dbToGain(params_.inputGainDb);
    lowL_.setLowShelf(140.0f, params_.lowShelfDb, static_cast<float>(sampleRate_));
    lowR_.setLowShelf(140.0f, params_.lowShelfDb, static_cast<float>(sampleRate_));
    highL_.setHighShelf(7800.0f, params_.highShelfDb, static_cast<float>(sampleRate_));
    highR_.setHighShelf(7800.0f, params_.highShelfDb, static_cast<float>(sampleRate_));
    compAttack_ = 1.0f - std::exp(-1.0f / (std::max(0.1f, params_.compAttackMs) * 0.001f * static_cast<float>(sampleRate_)));
    compRelease_ = 1.0f - std::exp(-1.0f / (std::max(1.0f, params_.compReleaseMs) * 0.001f * static_cast<float>(sampleRate_)));
    limiterRelease_ = std::exp(-1.0f / (0.12f * static_cast<float>(sampleRate_)));
}

void MasterBus::process(float* left, float* right, const float* sendLeft, const float* sendRight, int frames) {
    if (!left || !right || !sendLeft || !sendRight || frames <= 0) return;
    if (frames > maxBlockSize_) frames = maxBlockSize_;
    reverb_.process(sendLeft, sendRight, sendL_.data(), sendR_.data(), frames,
                    params_.reverbMix, params_.reverbSize, params_.reverbDamping);
    const float threshold = dbToGain(params_.compThresholdDb);
    const float ratio = std::max(1.0f, params_.compRatio);
    float maxPeak = 0.0f;
    for (int i = 0; i < frames; ++i) {
        float l = left[i] + sendL_[static_cast<std::size_t>(i)];
        float r = right[i] + sendR_[static_cast<std::size_t>(i)];
        gain_ += (gainTarget_ - gain_) * 0.0015f;
        l *= gain_;
        r *= gain_;
        l = highL_.process(lowL_.process(l));
        r = highR_.process(lowR_.process(r));

        const float peak = std::max(std::abs(l), std::abs(r));
        maxPeak = std::max(maxPeak, peak);
        const float desired = peak > threshold ? threshold + (peak - threshold) / ratio : peak;
        const float targetComp = peak > 1.0e-9f ? std::clamp(desired / peak, 0.0f, 1.0f) : 1.0f;
        if (targetComp < compEnv_) compEnv_ += (targetComp - compEnv_) * compAttack_;
        else compEnv_ += (targetComp - compEnv_) * compRelease_;
        l *= compEnv_;
        r *= compEnv_;

        // Mid/side width control; keeps mono compatibility.
        const float mid = (l + r) * 0.5f;
        const float side = (l - r) * 0.5f * std::clamp(params_.width, 0.0f, 2.0f);
        l = mid + side;
        r = mid - side;

        const float limiterPeak = std::max(std::abs(l), std::abs(r));
        const float ceiling = std::clamp(params_.ceiling, 0.5f, 1.0f);
        const float targetLimit = limiterPeak > ceiling ? ceiling / limiterPeak : 1.0f;
        if (targetLimit < limiterEnv_) limiterEnv_ += (targetLimit - limiterEnv_) * 0.35f;
        else limiterEnv_ = limiterEnv_ * limiterRelease_ + (1.0f - limiterRelease_);
        if (limiterEnv_ < 0.999f) ++limiterActivations_;
        l *= limiterEnv_;
        r *= limiterEnv_;

        // DC blocker protects speakers and the limiter from offset accumulation.
        const float dcL = l - dcIn_[0] + 0.995f * dcOut_[0];
        const float dcR = r - dcIn_[1] + 0.995f * dcOut_[1];
        dcIn_[0] = l;
        dcIn_[1] = r;
        dcOut_[0] = flushDenormal(dcL);
        dcOut_[1] = flushDenormal(dcR);
        left[i] = clampSafe(dcL, -1.0f, 1.0f);
        right[i] = clampSafe(dcR, -1.0f, 1.0f);
        if (!std::isfinite(left[i])) left[i] = 0.0f;
        if (!std::isfinite(right[i])) right[i] = 0.0f;
    }
    lastPeak_ = maxPeak;
    lastGainReductionDb_ = compEnv_ > 1.0e-9f ? -20.0f * std::log10(compEnv_) : 0.0f;
}

} // namespace ec::dsp
