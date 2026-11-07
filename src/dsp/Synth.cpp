#include "dsp/Synth.hpp"

#include <algorithm>
#include <cmath>

namespace ec::dsp {
namespace {

float polyBlep(float phase, float dt) noexcept {
    if (dt <= 0.0f) return 0.0f;
    if (phase < dt) {
        const float t = phase / dt;
        return t + t - t * t - 1.0f;
    }
    if (phase > 1.0f - dt) {
        const float t = (phase - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float saw(float phase, float dt) noexcept {
    return 2.0f * phase - 1.0f - polyBlep(phase, dt);
}

float clampPan(float p) noexcept {
    return std::clamp(std::isfinite(p) ? p : 0.0f, -1.0f, 1.0f);
}

} // namespace

void SynthEngine::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    maxBlockSize_ = std::max(1, maxBlockSize);
    reset();
}

void SynthEngine::reset() {
    for (auto& voice : voices_) voice = Voice{};
    age_ = 1;
    renderedSamples_ = 0;
}

int SynthEngine::activeVoiceCount() const {
    int count = 0;
    for (const auto& voice : voices_) if (voice.active) ++count;
    return count;
}

int SynthEngine::chooseVoice() {
    int best = -1;
    std::uint64_t bestAge = std::numeric_limits<std::uint64_t>::max();
    for (int i = 0; i < kMaxVoices; ++i) {
        if (!voices_[static_cast<std::size_t>(i)].active) return i;
    }
    for (int i = 0; i < kMaxVoices; ++i) {
        const auto& v = voices_[static_cast<std::size_t>(i)];
        if (!v.released && bestAge == std::numeric_limits<std::uint64_t>::max()) {
            best = i;
            bestAge = v.age;
        }
    }
    if (best >= 0) {
        for (int i = 0; i < kMaxVoices; ++i) {
            const auto& v = voices_[static_cast<std::size_t>(i)];
            if (v.age < bestAge) { best = i; bestAge = v.age; }
        }
    }
    return best >= 0 ? best : 0;
}

void SynthEngine::startVoice(Voice& v, const SynthEvent& event) {
    v = Voice{};
    v.active = true;
    v.instrument = event.instrument;
    v.trackId = event.trackId;
    v.pitch = std::clamp(event.pitch, 0, 127);
    v.noteId = event.noteId;
    v.velocity = std::clamp(event.velocity, 1, 127) / 127.0f;
    const float pan = clampPan(event.pan);
    const float angle = (pan + 1.0f) * 0.25f * kPi;
    v.panL = std::cos(angle);
    v.panR = std::sin(angle);
    v.gain = dbToGain(clampSafe(event.gainDb, -60.0f, 12.0f)) *
             std::pow(v.velocity, 1.35f) * 0.30f;
    v.reverbSend = clampSafe(event.reverbSend, 0.0f, 1.0f);
    v.brightness = clampSafe(event.brightness, 0.0f, 1.0f);
    v.noiseAmount = clampSafe(event.noise, 0.0f, 1.0f);
    v.attackShape = clampSafe(event.attack, 0.0f, 1.0f);
    v.releaseShape = clampSafe(event.release, 0.0f, 1.0f);
    v.age = age_++;
    v.rng = 0x9E3779B9u ^ static_cast<std::uint32_t>(event.noteId * 2654435761u) ^
            static_cast<std::uint32_t>(event.trackId * 40503u) ^ static_cast<std::uint32_t>(v.pitch * 97);
    if (v.rng == 0) v.rng = 0x1234567u;
    if (event.holdSeconds > 0.0f) {
        v.autoRelease = true;
        v.holdRemaining = static_cast<std::uint64_t>(std::max(0.01f, event.holdSeconds) * sampleRate_);
    }
    const float f0 = midiToHz(static_cast<float>(v.pitch));
    const float nyquist = static_cast<float>(sampleRate_) * 0.45f;

    switch (v.instrument) {
        case Instrument::FeltPiano: {
            const float hammer = 0.10f + 0.09f * (1.0f - v.velocity) + 0.01f * (v.rng & 0xFF) / 255.0f;
            const int maxModes = static_cast<int>(std::min<float>(static_cast<float>(kMaxModes), nyquist / std::max(20.0f, f0)));
            v.modes = std::clamp(maxModes, 4, kMaxModes);
            const float stiffness = 0.00022f + 0.00040f * (1.0f - v.velocity);
            float sum = 0.0f;
            for (int i = 0; i < v.modes; ++i) {
                const int n = i + 1;
                const float ratio = static_cast<float>(n) * std::sqrt(1.0f + stiffness * static_cast<float>(n * n));
                float freq = f0 * ratio;
                const bool upperString = (i % 2) == 0;
                const float cents = (upperString ? 1.0f : -1.0f) * (0.35f + 0.55f * v.noiseAmount);
                freq *= std::pow(2.0f, cents / 1200.0f);
                if (freq >= nyquist) { v.modes = i; break; }
                v.phaseInc[static_cast<std::size_t>(i)] = freq / static_cast<float>(sampleRate_);
                v.phase[static_cast<std::size_t>(i)] = static_cast<float>((v.rng >> (i % 16)) & 0xFFFF) / 65536.0f;
                const float comb = std::max(0.18f, std::abs(std::sin(kPi * static_cast<float>(n) * hammer)));
                const float tilt = 1.15f - 0.38f * v.velocity;
                const float amp = comb / std::pow(static_cast<float>(n), tilt);
                v.amp[static_cast<std::size_t>(i)] = amp;
                sum += amp;
                const float baseTau = (3.9f + 2.0f * v.velocity) * std::pow(220.0f / std::max(55.0f, f0), 0.22f);
                const float tau = std::clamp(baseTau / (1.0f + 0.58f * static_cast<float>(n - 1)),
                                             0.05f, 9.0f);
                v.decay[static_cast<std::size_t>(i)] = std::exp(-1.0f / (tau * static_cast<float>(sampleRate_)));
                v.panMod[static_cast<std::size_t>(i)] = ((i & 1) ? 1.0f : -1.0f) * (0.02f + 0.02f * v.noiseAmount);
            }
            if (sum > 0.0f) {
                for (int i = 0; i < v.modes; ++i) v.amp[static_cast<std::size_t>(i)] /= sum;
            }
            v.env = 0.0f;
            const float attack = 0.0022f + 0.0035f * (1.0f - v.velocity);
            v.envAttack = 1.0f - std::exp(-1.0f / (attack * static_cast<float>(sampleRate_)));
            v.transientEnv = 1.0f;
            v.transientDecay = std::exp(-1.0f / (0.010f * static_cast<float>(sampleRate_)));
            v.filter1.setHighpass(1100.0f, 0.7f, static_cast<float>(sampleRate_));
            v.filter2.setPeaking(180.0f, 1.1f, 2.5f, static_cast<float>(sampleRate_));
            v.gain *= 0.92f;
            break;
        }
        case Instrument::BowedString: {
            v.phaseA = static_cast<float>(v.rng & 0xFFFF) / 65536.0f;
            v.phaseB = static_cast<float>((v.rng >> 8) & 0xFFFF) / 65536.0f;
            v.incA = f0 / static_cast<float>(sampleRate_);
            v.incB = f0 * std::pow(2.0f, 3.5f / 1200.0f) / static_cast<float>(sampleRate_);
            const float cutoff = 700.0f + 5200.0f * v.brightness + 1200.0f * v.velocity;
            v.filter1.setLowpass(std::min(nyquist, cutoff), 0.78f, static_cast<float>(sampleRate_));
            v.filter2.setBandpass(310.0f, 0.9f, static_cast<float>(sampleRate_));
            v.filter3.setBandpass(980.0f, 1.4f, static_cast<float>(sampleRate_));
            const float attack = 0.10f + 0.20f * (1.0f - v.velocity);
            v.envAttack = 1.0f - std::exp(-1.0f / (attack * static_cast<float>(sampleRate_)));
            v.vibInc = (4.7f + 0.7f * v.velocity) / static_cast<float>(sampleRate_);
            v.vibDepth = 0.0020f + 0.0040f * v.releaseShape;
            v.ampLfoInc = (0.11f + 0.07f * (1.0f - v.velocity)) / static_cast<float>(sampleRate_);
            v.gain *= 0.55f;
            break;
        }
        case Instrument::GlassMallet: {
            static const float ratios[8] = {1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.8f, 31.7f};
            v.modes = static_cast<int>(std::min<float>(8.0f, nyquist / std::max(30.0f, f0)));
            for (int i = 0; i < v.modes; ++i) {
                float freq = f0 * ratios[static_cast<std::size_t>(i)];
                freq *= std::pow(2.0f, (0.8f * (v.noiseAmount - 0.5f)) / 1200.0f);
                if (freq >= nyquist) { v.modes = i; break; }
                v.phaseInc[static_cast<std::size_t>(i)] = freq / static_cast<float>(sampleRate_);
                v.phase[static_cast<std::size_t>(i)] = static_cast<float>((v.rng >> (i * 3)) & 0xFFFF) / 65536.0f;
                v.amp[static_cast<std::size_t>(i)] = 1.0f / (1.0f + 0.9f * static_cast<float>(i));
                const float tau = std::clamp((1.7f - 0.6f * v.velocity) / (1.0f + 0.7f * static_cast<float>(i)), 0.03f, 2.5f);
                v.decay[static_cast<std::size_t>(i)] = std::exp(-1.0f / (tau * static_cast<float>(sampleRate_)));
            }
            v.envAttack = 1.0f - std::exp(-1.0f / (0.0012f * static_cast<float>(sampleRate_)));
            v.transientEnv = 1.0f;
            v.transientDecay = std::exp(-1.0f / (0.008f * static_cast<float>(sampleRate_)));
            v.filter1.setHighpass(1900.0f, 0.8f, static_cast<float>(sampleRate_));
            v.gain *= 0.5f;
            break;
        }
        case Instrument::OrganicBass: {
            v.subInc = f0 / static_cast<float>(sampleRate_);
            v.incA = f0 / static_cast<float>(sampleRate_);
            const float cutoff = std::min(nyquist, 160.0f + 1500.0f * v.brightness + 500.0f * v.velocity);
            v.filter1.setLowpass(cutoff, 0.9f, static_cast<float>(sampleRate_));
            v.envAttack = 1.0f - std::exp(-1.0f / (0.006f * static_cast<float>(sampleRate_)));
            v.pitchEnv = 1.0f;
            v.pitchEnvDepth = 0.012f + 0.018f * (1.0f - v.velocity);
            v.pitchEnvDecay = std::exp(-1.0f / (0.055f * static_cast<float>(sampleRate_)));
            v.gain *= 0.72f;
            break;
        }
        case Instrument::DrumKit: {
            const int p = v.pitch;
            if (p == 36) { v.drumKind = 0; v.drumPitch = 118.0f; v.drumDecay = 0.42f; v.drumNoiseMix = 0.10f; }
            else if (p == 38) { v.drumKind = 1; v.drumPitch = 185.0f; v.drumDecay = 0.20f; v.drumNoiseMix = 0.85f; }
            else if (p == 42) { v.drumKind = 2; v.drumPitch = 0.0f; v.drumDecay = 0.055f; v.drumNoiseMix = 1.0f; }
            else if (p == 46) { v.drumKind = 3; v.drumPitch = 0.0f; v.drumDecay = 0.33f; v.drumNoiseMix = 1.0f; }
            else if (p == 39) { v.drumKind = 4; v.drumPitch = 0.0f; v.drumDecay = 0.22f; v.drumNoiseMix = 1.0f; }
            else { v.drumKind = 5; v.drumPitch = 150.0f; v.drumDecay = 0.30f; v.drumNoiseMix = 0.2f; }
            v.env = 1.0f;
            v.envAttack = 1.0f;
            v.autoRelease = true;
            v.holdRemaining = static_cast<std::uint64_t>(v.drumDecay * 3.0f * sampleRate_);
            v.decay[0] = std::exp(-1.0f / (v.drumDecay * static_cast<float>(sampleRate_)));
            v.gain *= 1.0f;
            v.filter1.setHighpass(5200.0f, 0.75f, static_cast<float>(sampleRate_));
            break;
        }
    }
}

void SynthEngine::noteOn(const SynthEvent& event) {
    const int index = chooseVoice();
    auto& voice = voices_[static_cast<std::size_t>(index)];
    if (voice.active) {
        // Voice stealing is deterministic and pre-allocated; do not allocate.
        voice = Voice{};
    }
    startVoice(voice, event);
}

void SynthEngine::releaseVoice(Voice& voice, bool immediate) {
    if (!voice.active) return;
    if (immediate) {
        voice.active = false;
        return;
    }
    if (voice.released) return;
    voice.released = true;
    float seconds = 0.20f + 0.65f * voice.releaseShape;
    if (voice.instrument == Instrument::FeltPiano) seconds *= 0.55f;
    if (voice.instrument == Instrument::GlassMallet) seconds *= 0.9f;
    if (voice.instrument == Instrument::DrumKit) seconds = 0.03f;
    voice.releaseCoef = std::exp(-1.0f / (seconds * static_cast<float>(sampleRate_)));
}

void SynthEngine::noteOff(int trackId, int pitch) {
    for (auto& voice : voices_) {
        if (voice.active && !voice.released && voice.trackId == trackId && voice.pitch == pitch) {
            releaseVoice(voice, false);
        }
    }
}

void SynthEngine::allNotesOff(bool immediate) {
    for (auto& voice : voices_) {
        if (voice.active) releaseVoice(voice, immediate);
    }
}

float SynthEngine::renderFelt(Voice& v) {
    float out = 0.0f;
    for (int i = 0; i < v.modes; ++i) {
        auto& phase = v.phase[static_cast<std::size_t>(i)];
        auto& amp = v.amp[static_cast<std::size_t>(i)];
        out += sine01(phase) * amp;
        phase += v.phaseInc[static_cast<std::size_t>(i)];
        if (phase >= 1.0f) phase -= 1.0f;
        amp *= v.decay[static_cast<std::size_t>(i)];
    }
    if (v.transientEnv > 1.0e-5f) {
        const float noise = whiteNoise(v.rng);
        v.noiseLp = v.noiseLp * 0.86f + noise * 0.14f;
        const float hp = noise - v.noiseLp;
        out += hp * v.transientEnv * (0.045f + 0.10f * v.velocity) * (0.35f + v.noiseAmount);
        v.transientEnv *= v.transientDecay;
    }
    return out * 0.72f;
}

float SynthEngine::renderBowed(Voice& v) {
    v.vibPhase += v.vibInc;
    if (v.vibPhase >= 1.0f) v.vibPhase -= 1.0f;
    v.ampLfoPhase += v.ampLfoInc;
    if (v.ampLfoPhase >= 1.0f) v.ampLfoPhase -= 1.0f;
    const float vib = 1.0f + v.vibDepth * sine01(v.vibPhase) * v.env;
    const float aInc = v.incA * vib;
    const float bInc = v.incB * vib;
    const float a = saw(v.phaseA, aInc);
    const float b = saw(v.phaseB, bInc);
    v.phaseA += aInc;
    v.phaseB += bInc;
    if (v.phaseA >= 1.0f) v.phaseA -= 1.0f;
    if (v.phaseB >= 1.0f) v.phaseB -= 1.0f;
    float signal = 0.58f * a + 0.42f * b;
    signal = v.filter1.process(signal);
    const float body = v.filter2.process(signal) * 0.55f + v.filter3.process(signal) * 0.35f;
    signal = signal * 0.55f + body * 0.7f;
    const float noise = whiteNoise(v.rng);
    v.noiseLp = v.noiseLp * 0.985f + noise * 0.015f;
    signal += v.noiseLp * (0.03f + 0.06f * v.noiseAmount) * (0.5f + 0.5f * sine01(v.ampLfoPhase));
    return signal * 0.55f;
}

float SynthEngine::renderGlass(Voice& v) {
    float out = 0.0f;
    for (int i = 0; i < v.modes; ++i) {
        auto& phase = v.phase[static_cast<std::size_t>(i)];
        auto& amp = v.amp[static_cast<std::size_t>(i)];
        out += sine01(phase) * amp;
        phase += v.phaseInc[static_cast<std::size_t>(i)];
        if (phase >= 1.0f) phase -= 1.0f;
        amp *= v.decay[static_cast<std::size_t>(i)];
    }
    if (v.transientEnv > 1.0e-5f) {
        const float noise = whiteNoise(v.rng);
        v.noiseLp = v.noiseLp * 0.55f + noise * 0.45f;
        out += (noise - v.noiseLp) * v.transientEnv * 0.14f * v.noiseAmount;
        v.transientEnv *= v.transientDecay;
    }
    return out * 0.65f;
}

float SynthEngine::renderBass(Voice& v) {
    v.pitchEnv *= v.pitchEnvDecay;
    const float pitchMul = 1.0f + v.pitchEnv * v.pitchEnvDepth;
    const float subInc = v.subInc * pitchMul;
    const float sawInc = v.incA * pitchMul;
    v.subPhase += subInc;
    if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
    v.phaseA += sawInc;
    if (v.phaseA >= 1.0f) v.phaseA -= 1.0f;
    const float sub = sine01(v.subPhase);
    const float s = saw(v.phaseA, sawInc);
    float signal = sub * 0.72f + v.filter1.process(s) * 0.35f;
    signal = std::tanh(signal * 1.25f) * 0.8f;
    return signal;
}

float SynthEngine::renderDrum(Voice& v) {
    const float env = v.decay[0];
    v.env *= env;
    float out = 0.0f;
    if (v.drumKind == 0 || v.drumKind == 5) {
        v.drumPitch *= 0.9993f;
        v.phaseA += v.drumPitch / static_cast<float>(sampleRate_);
        if (v.phaseA >= 1.0f) v.phaseA -= 1.0f;
        out += sine01(v.phaseA) * 0.9f;
        const float noise = whiteNoise(v.rng);
        v.noiseLp = v.noiseLp * 0.6f + noise * 0.4f;
        out += (noise - v.noiseLp) * v.drumNoiseMix * 0.4f;
    } else if (v.drumKind == 1) {
        v.phaseA += v.drumPitch / static_cast<float>(sampleRate_);
        if (v.phaseA >= 1.0f) v.phaseA -= 1.0f;
        const float noise = whiteNoise(v.rng);
        v.noiseLp = v.noiseLp * 0.55f + noise * 0.45f;
        const float hp = noise - v.noiseLp;
        out += (hp * 0.85f + sine01(v.phaseA) * 0.25f) * v.drumNoiseMix;
    } else if (v.drumKind == 2 || v.drumKind == 3) {
        const float noise = whiteNoise(v.rng);
        const float hp = v.filter1.process(noise);
        out += hp * 0.55f;
    } else if (v.drumKind == 4) {
        const float noise = whiteNoise(v.rng);
        v.hpState += 0.25f * (noise - v.hpState);
        const float hp = noise - v.hpState;
        const float bursts = std::max(0.0f, std::sin(kPi * std::min(1.0f, (1.0f - v.env) * 3.0f)));
        out += hp * (0.35f + 0.65f * bursts) * 0.8f;
    }
    if (v.env < 1.0e-4f) v.active = false;
    return out * 0.9f;
}

float SynthEngine::renderVoice(Voice& v, float& send) {
    if (!v.active) return 0.0f;
    if (v.autoRelease && v.holdRemaining > 0) {
        --v.holdRemaining;
        if (v.holdRemaining == 0 && !v.released) releaseVoice(v, false);
    }
    if (v.released && v.instrument != Instrument::DrumKit) {
        v.env *= v.releaseCoef;
        for (int i = 0; i < v.modes; ++i) v.amp[static_cast<std::size_t>(i)] *= v.releaseCoef;
        if (v.env < 1.0e-5f) {
            v.active = false;
            return 0.0f;
        }
    }
    if (!v.released && v.env < 1.0f) v.env += (1.0f - v.env) * v.envAttack;
    float out = 0.0f;
    switch (v.instrument) {
        case Instrument::FeltPiano: out = renderFelt(v); break;
        case Instrument::BowedString: out = renderBowed(v); break;
        case Instrument::GlassMallet: out = renderGlass(v); break;
        case Instrument::OrganicBass: out = renderBass(v); break;
        case Instrument::DrumKit: out = renderDrum(v); break;
    }
    if (!std::isfinite(out)) {
        out = 0.0f;
        v.active = false;
    }
    out *= v.env * v.gain;
    send = out * v.reverbSend;
    if (v.instrument == Instrument::FeltPiano || v.instrument == Instrument::GlassMallet) {
        bool alive = false;
        for (int i = 0; i < v.modes; ++i) {
            if (v.amp[static_cast<std::size_t>(i)] > 1.0e-5f) { alive = true; break; }
        }
        if (!alive && !v.released) releaseVoice(v, false);
        if (!alive && v.env < 1.0e-4f) v.active = false;
    }
    return out;
}

void SynthEngine::render(float* dryLeft, float* dryRight, float* sendLeft, float* sendRight, int frames) {
    if (!dryLeft || !dryRight || !sendLeft || !sendRight || frames <= 0) return;
    std::fill(dryLeft, dryLeft + frames, 0.0f);
    std::fill(dryRight, dryRight + frames, 0.0f);
    std::fill(sendLeft, sendLeft + frames, 0.0f);
    std::fill(sendRight, sendRight + frames, 0.0f);
    for (auto& voice : voices_) {
        if (!voice.active) continue;
        // The render loop advances one sample at a time; the voice is processed
        // per sample because its LFOs and release are sample-dependent.
        for (int i = 0; i < frames; ++i) {
            if (!voice.active) break;
            float send = 0.0f;
            const float mono = renderVoice(voice, send);
            const float l = mono * voice.panL;
            const float r = mono * voice.panR;
            dryLeft[i] += flushDenormal(l);
            dryRight[i] += flushDenormal(r);
            sendLeft[i] += flushDenormal(send * voice.panL);
            sendRight[i] += flushDenormal(send * voice.panR);
        }
    }
    for (int i = 0; i < frames; ++i) {
        dryLeft[i] = clampSafe(dryLeft[i], -16.0f, 16.0f);
        dryRight[i] = clampSafe(dryRight[i], -16.0f, 16.0f);
        sendLeft[i] = clampSafe(sendLeft[i], -16.0f, 16.0f);
        sendRight[i] = clampSafe(sendRight[i], -16.0f, 16.0f);
    }
    renderedSamples_ += static_cast<std::uint64_t>(frames);
}

} // namespace ec::dsp
