#include "io/WavWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace ec {
namespace {

void writeU16(std::ofstream& out, std::uint16_t v) {
    const char bytes[2]{static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
    out.write(bytes, 2);
}

void writeU32(std::ofstream& out, std::uint32_t v) {
    const char bytes[4]{static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                        static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
    out.write(bytes, 4);
}

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t pos) {
    return static_cast<std::uint16_t>(bytes[pos] | (static_cast<std::uint16_t>(bytes[pos + 1]) << 8));
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t pos) {
    return static_cast<std::uint32_t>(bytes[pos]) |
           (static_cast<std::uint32_t>(bytes[pos + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[pos + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[pos + 3]) << 24);
}

bool readAll(const std::string& path, std::vector<std::uint8_t>& out, std::string* error) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open WAV: " + path;
        return false;
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size < 0) {
        if (error) *error = "cannot size WAV: " + path;
        return false;
    }
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    if (!out.empty()) in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!in && !in.eof()) {
        if (error) *error = "cannot read WAV: " + path;
        return false;
    }
    return true;
}

float decodeSample(const std::uint8_t* data, int bits) {
    if (bits == 16) {
        const auto v = static_cast<std::int16_t>(static_cast<std::uint16_t>(data[0]) |
                                                 (static_cast<std::uint16_t>(data[1]) << 8));
        return static_cast<float>(v) / 32768.0f;
    }
    if (bits == 24) {
        std::int32_t v = static_cast<std::int32_t>(data[0]) |
                         (static_cast<std::int32_t>(data[1]) << 8) |
                         (static_cast<std::int32_t>(data[2]) << 16);
        if (v & 0x800000) v |= static_cast<std::int32_t>(0xFF000000u);
        return static_cast<float>(v) / 8388608.0f;
    }
    if (bits == 32) {
        float v = 0.0f;
        std::memcpy(&v, data, sizeof(v));
        return v;
    }
    return 0.0f;
}

} // namespace

WavFileWriter::~WavFileWriter() {
    close(nullptr);
}

bool WavFileWriter::open(const std::string& path, int sampleRate, int channels, int bitsPerSample,
                         std::string* error) {
    close(nullptr);
    if (sampleRate < 8000 || sampleRate > 384000) {
        if (error) *error = "invalid WAV sample rate";
        return false;
    }
    if (channels < 1 || channels > 16) {
        if (error) *error = "invalid WAV channel count";
        return false;
    }
    if (bitsPerSample != 16 && bitsPerSample != 24) {
        if (error) *error = "only 16-bit and 24-bit PCM WAV are supported";
        return false;
    }
    const auto fsPath = std::filesystem::u8path(path);
    std::error_code ec;
    if (fsPath.has_parent_path()) std::filesystem::create_directories(fsPath.parent_path(), ec);
    auto* file = new std::ofstream(fsPath, std::ios::binary | std::ios::trunc);
    if (!*file) {
        delete file;
        if (error) *error = "cannot open WAV for writing: " + path;
        return false;
    }
    file_ = file;
    sampleRate_ = sampleRate;
    channels_ = channels;
    bits_ = bitsPerSample;
    framesWritten_ = 0;
    dataBytes_ = 0;
    file->write("RIFF", 4);
    writeU32(*file, 0);
    file->write("WAVE", 4);
    file->write("fmt ", 4);
    writeU32(*file, 16);
    writeU16(*file, 1); // PCM
    writeU16(*file, static_cast<std::uint16_t>(channels));
    writeU32(*file, static_cast<std::uint32_t>(sampleRate));
    const std::uint32_t blockAlign = static_cast<std::uint32_t>(channels * bitsPerSample / 8);
    writeU32(*file, static_cast<std::uint32_t>(sampleRate) * blockAlign);
    writeU16(*file, static_cast<std::uint16_t>(blockAlign));
    writeU16(*file, static_cast<std::uint16_t>(bitsPerSample));
    file->write("data", 4);
    writeU32(*file, 0);
    if (!*file) {
        if (error) *error = "WAV header write failed";
        close(nullptr);
        return false;
    }
    return true;
}

bool WavFileWriter::writeInterleaved(const float* samples, std::size_t frames, std::string* error) {
    if (!file_ || !samples) {
        if (error) *error = "WAV writer is not open";
        return false;
    }
    auto* file = static_cast<std::ofstream*>(file_);
    std::vector<char> buffer;
    buffer.reserve(frames * static_cast<std::size_t>(channels_) * static_cast<std::size_t>(bits_ / 8));
    for (std::size_t i = 0; i < frames * static_cast<std::size_t>(channels_); ++i) {
        float sample = samples[i];
        if (!std::isfinite(sample)) sample = 0.0f;
        sample = std::clamp(sample, -1.0f, 0.99999994f);
        if (bits_ == 16) {
            const auto v = static_cast<std::int16_t>(std::lround(sample * 32767.0f));
            buffer.push_back(static_cast<char>(v & 0xFF));
            buffer.push_back(static_cast<char>((v >> 8) & 0xFF));
        } else {
            const auto v = static_cast<std::int32_t>(std::lround(sample * 8388607.0f));
            buffer.push_back(static_cast<char>(v & 0xFF));
            buffer.push_back(static_cast<char>((v >> 8) & 0xFF));
            buffer.push_back(static_cast<char>((v >> 16) & 0xFF));
        }
    }
    file->write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!*file) {
        if (error) *error = "WAV data write failed";
        return false;
    }
    framesWritten_ += frames;
    dataBytes_ += buffer.size();
    return true;
}

bool WavFileWriter::close(std::string* error) {
    if (!file_) return true;
    auto* file = static_cast<std::ofstream*>(file_);
    const std::uint64_t riffBytes = 36 + dataBytes_;
    if (riffBytes > std::numeric_limits<std::uint32_t>::max()) {
        if (error) *error = "WAV exceeds 4 GiB RIFF limit";
        file->close();
        delete file;
        file_ = nullptr;
        return false;
    }
    file->seekp(4, std::ios::beg);
    writeU32(*file, static_cast<std::uint32_t>(riffBytes));
    file->seekp(40, std::ios::beg);
    writeU32(*file, static_cast<std::uint32_t>(dataBytes_));
    file->seekp(0, std::ios::end);
    file->close();
    const bool ok = !file->fail();
    delete file;
    file_ = nullptr;
    if (!ok && error) *error = "failed to finalize WAV";
    return ok;
}

WavInfo readWavInfo(const std::string& path, std::string* error) {
    WavInfo info;
    std::vector<std::uint8_t> bytes;
    if (!readAll(path, bytes, error)) return info;
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        if (error) *error = "not a RIFF/WAVE file";
        return info;
    }
    std::size_t pos = 12;
    int format = 0;
    while (pos + 8 <= bytes.size()) {
        char id[4];
        std::memcpy(id, bytes.data() + pos, 4);
        const std::uint32_t len = readU32(bytes, pos + 4);
        pos += 8;
        if (pos + len > bytes.size()) break;
        if (std::memcmp(id, "fmt ", 4) == 0 && len >= 16) {
            format = readU16(bytes, pos);
            info.channels = readU16(bytes, pos + 2);
            info.sampleRate = static_cast<int>(readU32(bytes, pos + 4));
            info.bitsPerSample = readU16(bytes, pos + 14);
        } else if (std::memcmp(id, "data", 4) == 0) {
            const int bytesPerSample = std::max(1, info.bitsPerSample / 8);
            info.frames = len / static_cast<std::uint32_t>(bytesPerSample * std::max(1, info.channels));
        }
        pos += len + (len & 1u);
    }
    info.valid = format == 1 && info.channels > 0 && info.bitsPerSample > 0 && info.frames > 0;
    if (!info.valid && error && error->empty()) *error = "unsupported or empty WAV";
    return info;
}

bool readWavPcm(const std::string& path, std::vector<float>& interleaved, WavInfo& info,
                std::string* error) {
    std::vector<std::uint8_t> bytes;
    if (!readAll(path, bytes, error)) return false;
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        if (error) *error = "not a RIFF/WAVE file";
        return false;
    }
    std::size_t pos = 12;
    const std::uint8_t* data = nullptr;
    std::uint32_t dataLen = 0;
    int format = 0;
    while (pos + 8 <= bytes.size()) {
        char id[4];
        std::memcpy(id, bytes.data() + pos, 4);
        const std::uint32_t len = readU32(bytes, pos + 4);
        pos += 8;
        if (pos + len > bytes.size()) break;
        if (std::memcmp(id, "fmt ", 4) == 0 && len >= 16) {
            format = readU16(bytes, pos);
            info.channels = readU16(bytes, pos + 2);
            info.sampleRate = static_cast<int>(readU32(bytes, pos + 4));
            info.bitsPerSample = readU16(bytes, pos + 14);
        } else if (std::memcmp(id, "data", 4) == 0) {
            data = bytes.data() + pos;
            dataLen = len;
        }
        pos += len + (len & 1u);
    }
    if (!data || info.channels <= 0 || (info.bitsPerSample != 16 && info.bitsPerSample != 24 && info.bitsPerSample != 32)) {
        if (error) *error = "unsupported WAV layout";
        return false;
    }
    if (info.bitsPerSample == 32 && format != 3) {
        if (error) *error = "32-bit WAV is only supported as IEEE float";
        return false;
    }
    const int bytesPerSample = info.bitsPerSample / 8;
    const std::size_t sampleCount = dataLen / static_cast<std::uint32_t>(bytesPerSample);
    interleaved.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        interleaved[i] = decodeSample(data + i * static_cast<std::size_t>(bytesPerSample), info.bitsPerSample);
    }
    info.frames = sampleCount / static_cast<std::size_t>(info.channels);
    info.valid = true;
    return true;
}

} // namespace ec
