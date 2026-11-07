#include "render/Renderer.hpp"

#include "core/ProjectIO.hpp"
#include "core/TimeMap.hpp"
#include "dsp/Effects.hpp"
#include "dsp/Synth.hpp"
#include "io/WavWriter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace ec {
namespace {

struct ScheduledNote {
    std::uint64_t sample = 0;
    dsp::SynthEvent event;
};

bool trackAudible(const Project& p, const Track& track) {
    bool anySolo = false;
    for (const auto& t : p.tracks) if (t.solo) anySolo = true;
    if (track.muted) return false;
    if (anySolo && !track.solo) return false;
    return true;
}

} // namespace

bool renderProjectToBuffer(const Project& project,
                           const RenderOptions& options,
                           std::vector<float>& interleavedStereo,
                           RenderStats* statsOut,
                           std::string* error) {
    const auto started = std::chrono::steady_clock::now();
    interleavedStereo.clear();
    if (options.sampleRate < 8000 || options.sampleRate > 384000) {
        if (error) *error = "sampleRate must be in [8000, 384000]";
        return false;
    }
    if (options.blockSize < 1 || options.blockSize > 8192) {
        if (error) *error = "blockSize must be in [1, 8192]";
        return false;
    }
    Project p = project;
    std::string validationError;
    if (!validateProject(p, &validationError)) {
        if (error) *error = "invalid project: " + validationError;
        return false;
    }
    const int sr = options.sampleRate;
    const int blockSize = options.blockSize;
    const TimeMap timeMap(p.tempoMap, p.ppq);
    const Tick totalTicks = p.totalTicks();
    const double musicSeconds = timeMap.secondsAtTick(totalTicks);
    const double totalSeconds = musicSeconds + std::max(0.0f, options.tailSeconds);
    const auto totalFrames = static_cast<std::uint64_t>(std::ceil(totalSeconds * sr));
    if (totalFrames == 0) {
        if (error) *error = "project has zero duration";
        return false;
    }
    if (totalFrames > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 2)) {
        if (error) *error = "render is too large";
        return false;
    }

    std::vector<ScheduledNote> scheduled;
    for (const auto& track : p.tracks) {
        if (!trackAudible(p, track)) continue;
        for (const auto& note : track.notes) {
            if (note.muted) continue;
            const double rawStart = timeMap.secondsAtTick(note.start) + note.microMs * 0.001;
            const double start = std::max(0.0, rawStart);
            const Tick endTick = note.start + std::max<Tick>(1, note.duration);
            const double end = std::max(start + 0.025, timeMap.secondsAtTick(endTick));
            ScheduledNote sn;
            sn.sample = static_cast<std::uint64_t>(std::llround(start * sr));
            sn.event.instrument = track.instrument;
            sn.event.pitch = note.pitch;
            sn.event.velocity = note.velocity;
            sn.event.trackId = track.id;
            sn.event.noteId = note.id;
            sn.event.pan = std::clamp(track.pan + note.pan, -1.0f, 1.0f);
            sn.event.brightness = note.brightness;
            sn.event.noise = note.noise;
            sn.event.attack = note.attack;
            sn.event.release = note.release;
            sn.event.reverbSend = std::clamp(track.reverbSend + 0.10f * (1.0f - note.brightness), 0.0f, 1.0f);
            sn.event.gainDb = track.gainDb;
            sn.event.holdSeconds = static_cast<float>(end - start);
            if (track.instrument == Instrument::DrumKit) sn.event.holdSeconds = 2.0f;
            scheduled.push_back(sn);
        }
    }
    if (scheduled.empty()) {
        if (error) *error = "no audible notes (check mute/solo and notes)";
        return false;
    }
    std::stable_sort(scheduled.begin(), scheduled.end(), [](const ScheduledNote& a, const ScheduledNote& b) {
        return a.sample < b.sample;
    });

    dsp::SynthEngine synth;
    synth.prepare(sr, blockSize);
    dsp::MasterBus master;
    master.prepare(sr, blockSize);
    dsp::MasterParams params;
    // The voice engine is calibrated for headroom; +18 dB restores a healthy
    // listening level before the compressor/limiter, not a loudness-war target.
    params.inputGainDb = options.masterGainDb + 18.0f;
    params.reverbMix = options.includeReverb ? 0.85f : 0.0f;
    master.setParams(params);

    interleavedStereo.resize(static_cast<std::size_t>(totalFrames * 2), 0.0f);
    std::vector<float> dryL(static_cast<std::size_t>(blockSize), 0.0f);
    std::vector<float> dryR(static_cast<std::size_t>(blockSize), 0.0f);
    std::vector<float> sendL(static_cast<std::size_t>(blockSize), 0.0f);
    std::vector<float> sendR(static_cast<std::size_t>(blockSize), 0.0f);

    RenderStats stats;
    stats.noteEvents = static_cast<int>(scheduled.size());
    double sumSquares = 0.0;
    std::size_t eventIndex = 0;
    std::uint64_t frame = 0;
    while (frame < totalFrames) {
        const int frames = static_cast<int>(std::min<std::uint64_t>(blockSize, totalFrames - frame));
        const std::uint64_t blockEnd = frame + static_cast<std::uint64_t>(frames);
        while (eventIndex < scheduled.size() && scheduled[eventIndex].sample < blockEnd) {
            synth.noteOn(scheduled[eventIndex].event);
            ++eventIndex;
        }
        synth.render(dryL.data(), dryR.data(), sendL.data(), sendR.data(), frames);
        stats.maxVoices = std::max(stats.maxVoices, synth.activeVoiceCount());
        master.process(dryL.data(), dryR.data(), sendL.data(), sendR.data(), frames);
        for (int i = 0; i < frames; ++i) {
            const float l = dryL[static_cast<std::size_t>(i)];
            const float r = dryR[static_cast<std::size_t>(i)];
            if (!std::isfinite(l) || !std::isfinite(r)) stats.allFinite = false;
            const float absL = std::abs(l);
            const float absR = std::abs(r);
            stats.peak = std::max(stats.peak, std::max(absL, absR));
            if (absL > 0.999f || absR > 0.999f) ++stats.clippedSamples;
            sumSquares += static_cast<double>(l) * l + static_cast<double>(r) * r;
            interleavedStereo[static_cast<std::size_t>((frame + static_cast<std::uint64_t>(i)) * 2)] = l;
            interleavedStereo[static_cast<std::size_t>((frame + static_cast<std::uint64_t>(i)) * 2 + 1)] = r;
        }
        frame = blockEnd;
    }

    const double n = static_cast<double>(totalFrames) * 2.0;
    stats.rms = n > 0.0 ? static_cast<float>(std::sqrt(sumSquares / n)) : 0.0f;
    if (options.normalize && stats.peak > 1.0e-6f) {
        const float scale = 0.98f / stats.peak;
        for (auto& sample : interleavedStereo) sample *= scale;
        stats.rms *= scale;
        stats.peak = 0.98f;
    }
    stats.frames = totalFrames;
    stats.audioSeconds = static_cast<double>(totalFrames) / sr;
    stats.renderMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (statsOut) *statsOut = stats;
    return true;
}

bool renderProjectToWav(const Project& project,
                        const std::string& path,
                        const RenderOptions& options,
                        RenderStats* stats,
                        std::string* error) {
    std::vector<float> buffer;
    RenderStats local;
    if (!renderProjectToBuffer(project, options, buffer, &local, error)) return false;
    WavFileWriter writer;
    if (!writer.open(path, options.sampleRate, 2, options.bitsPerSample, error)) return false;
    const auto frames = buffer.size() / 2;
    if (!writer.writeInterleaved(buffer.data(), frames, error)) {
        writer.close(nullptr);
        return false;
    }
    if (!writer.close(error)) return false;
    if (stats) *stats = local;
    return true;
}

} // namespace ec
