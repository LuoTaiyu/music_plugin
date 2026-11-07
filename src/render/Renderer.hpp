#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ec {

struct RenderOptions {
    int sampleRate = 48000;
    int blockSize = 512;
    int bitsPerSample = 24;
    float tailSeconds = 3.0f;
    float masterGainDb = 0.0f;
    bool includeReverb = true;
    bool normalize = false;
};

struct RenderStats {
    std::uint64_t frames = 0;
    double audioSeconds = 0.0;
    double renderMs = 0.0;
    float peak = 0.0f;
    float rms = 0.0f;
    int clippedSamples = 0;
    bool allFinite = true;
    int maxVoices = 0;
    int noteEvents = 0;
};

bool renderProjectToWav(const Project& project,
                        const std::string& path,
                        const RenderOptions& options,
                        RenderStats* stats = nullptr,
                        std::string* error = nullptr);

bool renderProjectToBuffer(const Project& project,
                           const RenderOptions& options,
                           std::vector<float>& interleavedStereo,
                           RenderStats* stats = nullptr,
                           std::string* error = nullptr);

} // namespace ec
