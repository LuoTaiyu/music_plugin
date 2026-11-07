#pragma once

#include "core/Types.hpp"
#include "dsp/Dsp.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace ec::dsp {

struct SynthEvent {
    Instrument instrument = Instrument::FeltPiano;
    int pitch = 60;
    int velocity = 90;
    int trackId = 0;
    std::uint32_t noteId = 0;
    float pan = 0.0f;
    float brightness = 0.5f;
    float noise = 0.3f;
    float attack = 0.5f;
    float release = 0.5f;
    float reverbSend = 0.3f;
    float gainDb = 0.0f;
    float holdSeconds = 0.0f; // 0 = held until noteOff
};

class SynthEngine {
public:
    static constexpr int kMaxVoices = 96;
    static constexpr int kMaxModes = 18;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void noteOn(const SynthEvent& event);
    void noteOff(int trackId, int pitch);
    void allNotesOff(bool immediate = false);
    void render(float* dryLeft, float* dryRight, float* sendLeft, float* sendRight, int frames);

    int activeVoiceCount() const;
    double sampleRate() const { return sampleRate_; }
    std::uint64_t renderedSamples() const { return renderedSamples_; }

private:
    struct Voice {
        bool active = false;
        bool released = false;
        Instrument instrument = Instrument::FeltPiano;
        int trackId = 0;
        int pitch = 60;
        std::uint32_t noteId = 0;
        float velocity = 0.7f;
        float panL = 0.707f;
        float panR = 0.707f;
        float gain = 0.2f;
        float reverbSend = 0.2f;
        float brightness = 0.5f;
        float noiseAmount = 0.3f;
        float attackShape = 0.5f;
        float releaseShape = 0.5f;
        std::uint64_t age = 0;
        std::uint64_t holdRemaining = 0;
        bool autoRelease = false;

        // Modal bank / partials.
        std::array<float, kMaxModes> phase{};
        std::array<float, kMaxModes> phaseInc{};
        std::array<float, kMaxModes> amp{};
        std::array<float, kMaxModes> decay{};
        std::array<float, kMaxModes> panMod{};
        int modes = 0;

        // Bowed / bass oscillators.
        float phaseA = 0.0f;
        float phaseB = 0.0f;
        float incA = 0.0f;
        float incB = 0.0f;
        float subPhase = 0.0f;
        float subInc = 0.0f;
        float sawMix = 0.5f;
        float bodyGain = 0.2f;

        Biquad filter1;
        Biquad filter2;
        Biquad filter3;
        OnePole damping;
        float noiseLp = 0.0f;

        float env = 0.0f;
        float envAttack = 0.0f;
        float envRelease = 0.0f;
        float releaseCoef = 0.999f;
        float transientEnv = 0.0f;
        float transientDecay = 0.0f;
        float pitchEnv = 0.0f;
        float pitchEnvDecay = 0.0f;
        float pitchEnvDepth = 0.0f;
        float ampLfoPhase = 0.0f;
        float ampLfoInc = 0.0f;
        float vibPhase = 0.0f;
        float vibInc = 0.0f;
        float vibDepth = 0.0f;
        float drift = 0.0f;
        float driftInc = 0.0f;
        std::uint32_t rng = 0x12345678u;

        // Percussion.
        float drumPitch = 60.0f;
        float drumDecay = 0.001f;
        float drumNoiseMix = 0.0f;
        int drumKind = 0;
        float hpState = 0.0f;
        float hpPrev = 0.0f;
    };

    void startVoice(Voice& voice, const SynthEvent& event);
    void releaseVoice(Voice& voice, bool immediate);
    float renderVoice(Voice& voice, float& send);
    float renderFelt(Voice& voice);
    float renderBowed(Voice& voice);
    float renderGlass(Voice& voice);
    float renderBass(Voice& voice);
    float renderDrum(Voice& voice);
    int chooseVoice();

    double sampleRate_ = 48000.0;
    int maxBlockSize_ = 512;
    std::array<Voice, kMaxVoices> voices_{};
    std::uint64_t age_ = 1;
    std::uint64_t renderedSamples_ = 0;
};

} // namespace ec::dsp
