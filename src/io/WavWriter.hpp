#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ec {

struct WavInfo {
    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;
    std::uint64_t frames = 0;
    bool valid = false;
};

class WavFileWriter {
public:
    WavFileWriter() = default;
    ~WavFileWriter();
    WavFileWriter(const WavFileWriter&) = delete;
    WavFileWriter& operator=(const WavFileWriter&) = delete;

    bool open(const std::string& path, int sampleRate, int channels, int bitsPerSample,
              std::string* error = nullptr);
    bool writeInterleaved(const float* samples, std::size_t frames, std::string* error = nullptr);
    bool close(std::string* error = nullptr);
    bool isOpen() const { return file_ != nullptr; }
    std::uint64_t framesWritten() const { return framesWritten_; }

private:
    void* file_ = nullptr;
    int sampleRate_ = 0;
    int channels_ = 0;
    int bits_ = 24;
    std::uint64_t framesWritten_ = 0;
    std::uint64_t dataBytes_ = 0;
};

WavInfo readWavInfo(const std::string& path, std::string* error = nullptr);
bool readWavPcm(const std::string& path, std::vector<float>& interleaved, WavInfo& info,
                std::string* error = nullptr);

} // namespace ec
