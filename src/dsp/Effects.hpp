#pragma once

#include "dsp/Dsp.hpp"

#include <array>
#include <vector>

namespace ec::dsp {

struct MasterParams {
    float inputGainDb = 0.0f;
    float lowShelfDb = 0.8f;
    float highShelfDb = -0.8f;
    float compThresholdDb = -14.0f;
    float compRatio = 2.2f;
    float compAttackMs = 12.0f;
    float compReleaseMs = 140.0f;
    float reverbMix = 0.85f;
    float reverbSize = 0.6f;
    float reverbDamping = 0.45f;
    float width = 1.0f;
    float ceiling = 0.98f;
};

class Reverb {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(const float* sendLeft, const float* sendRight, float* wetLeft, float* wetRight,
                 int frames, float mix, float size, float damping);

private:
    double sampleRate_ = 48000.0;
    static constexpr int kLines = 4;
    std::array<std::vector<float>, kLines> delay_;
    std::array<int, kLines> write_{};
    std::array<int, kLines> length_{};
    std::array<OnePole, kLines> damping_;
    std::vector<float> preDelay_;
    int preWrite_ = 0;
    int preLength_ = 1;
    std::array<float, kLines> lfoPhase_{};
    std::array<float, kLines> lfoInc_{};
};

class MasterBus {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParams(const MasterParams& params);
    void process(float* left, float* right, const float* sendLeft, const float* sendRight, int frames);
    float lastGainReductionDb() const { return lastGainReductionDb_; }
    float lastPeak() const { return lastPeak_; }
    int limiterActivations() const { return limiterActivations_; }

private:
    double sampleRate_ = 48000.0;
    int maxBlockSize_ = 512;
    MasterParams params_{};
    Reverb reverb_;
    Biquad lowL_, lowR_, highL_, highR_;
    std::array<float, 2> dcIn_{};
    std::array<float, 2> dcOut_{};
    float gain_ = 1.0f;
    float gainTarget_ = 1.0f;
    float compEnv_ = 0.0f;
    float compAttack_ = 0.1f;
    float compRelease_ = 0.001f;
    float limiterEnv_ = 0.0f;
    float limiterRelease_ = 0.9998f;
    float lastGainReductionDb_ = 0.0f;
    float lastPeak_ = 0.0f;
    int limiterActivations_ = 0;
    std::vector<float> sendL_;
    std::vector<float> sendR_;
};

} // namespace ec::dsp
