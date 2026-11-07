#include "TestHarness.hpp"

#include "core/Composer.hpp"
#include "core/ProjectIO.hpp"
#include "dsp/Synth.hpp"
#include "io/WavWriter.hpp"
#include "render/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <vector>

using namespace ec;

namespace {

std::string pathUtf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

Project shortProject(int bars = 2) {
    Project p;
    p.schemaVersion = kProjectSchemaVersion;
    p.title = "Render test";
    p.styleId = "chamber";
    p.seed = 99;
    p.ppq = 960;
    p.timeSignature = {4, 4};
    p.tempoMap = {{0, 90.0}};
    p.key = {5, Mode::NaturalMinor};
    p.curves = defaultExpressionCurves("chamber");
    p.intent = defaultIntent("chamber");
    p.sections = {{"One", "development", "", 0, bars, {0, 0.3f, 0.3f, 0.3f}}};
    p.tracks.push_back({1, "Felt", Role::Melody, Instrument::FeltPiano, 1, 0.0f, 0.0f, 0.25f, false, false, {}});
    p.tracks.push_back({2, "Bowed", Role::Bass, Instrument::BowedString, 2, -6.0f, -0.2f, 0.3f, false, false, {}});
    for (int bar = 0; bar < bars; ++bar) {
        for (int i = 0; i < 4; ++i) {
            Note n;
            n.id = p.allocateNoteId();
            n.start = bar * 4 * 960 + i * 960;
            n.duration = 720;
            n.pitch = 60 + (i % 3) * 4 + (bar % 2) * 2;
            n.velocity = 55 + i * 14;
            n.brightness = 0.45f + 0.1f * static_cast<float>(i);
            n.legato = 0.6f;
            n.channel = 1;
            p.tracks[0].notes.push_back(n);
        }
        Note bass;
        bass.id = p.allocateNoteId();
        bass.start = bar * 4 * 960;
        bass.duration = 4 * 960 - 40;
        bass.pitch = 41 + (bar % 2) * 2;
        bass.velocity = 70;
        bass.channel = 2;
        p.tracks[1].notes.push_back(bass);
    }
    return p;
}

float highFrequencyRatio(const std::vector<float>& stereo) {
    double energy = 0.0;
    double diff = 0.0;
    for (std::size_t i = 2; i < stereo.size(); i += 2) {
        energy += static_cast<double>(stereo[i]) * stereo[i];
        const double d = static_cast<double>(stereo[i]) - stereo[i - 2];
        diff += d * d;
    }
    return energy > 1e-12 ? static_cast<float>(diff / energy) : 0.0f;
}

} // namespace

EC_TEST(synth_felt_note_is_finite_and_velocity_changes_timbre) {
    dsp::SynthEngine engine;
    engine.prepare(48000.0, 512);
    dsp::SynthEvent soft;
    soft.instrument = Instrument::FeltPiano;
    soft.pitch = 60;
    soft.velocity = 30;
    soft.brightness = 0.35f;
    soft.noteId = 1;
    soft.holdSeconds = 1.0f;
    engine.noteOn(soft);
    std::vector<float> l(48000), r(48000), sl(48000), sr(48000);
    engine.render(l.data(), r.data(), sl.data(), sr.data(), 48000);
    CHECK(engine.activeVoiceCount() <= 1);
    double energySoft = 0.0;
    for (int i = 0; i < 48000; ++i) {
        CHECK(std::isfinite(l[static_cast<std::size_t>(i)]));
        energySoft += static_cast<double>(l[static_cast<std::size_t>(i)]) * l[static_cast<std::size_t>(i)];
    }
    CHECK(energySoft > 1e-6);

    dsp::SynthEngine bright;
    bright.prepare(48000.0, 512);
    dsp::SynthEvent loud = soft;
    loud.velocity = 118;
    loud.brightness = 0.85f;
    loud.noteId = 2;
    bright.noteOn(loud);
    engine.reset();
    bright.render(l.data(), r.data(), sl.data(), sr.data(), 48000);
    double energyLoud = 0.0;
    for (int i = 0; i < 48000; ++i) {
        CHECK(std::isfinite(l[static_cast<std::size_t>(i)]));
        energyLoud += static_cast<double>(l[static_cast<std::size_t>(i)]) * l[static_cast<std::size_t>(i)];
    }
    CHECK(energyLoud > energySoft);
}

EC_TEST(synth_survives_extreme_parameters_and_voice_pressure) {
    dsp::SynthEngine engine;
    engine.prepare(44100.0, 257);
    std::vector<float> l(257), r(257), sl(257), sr(257);
    for (int i = 0; i < 220; ++i) {
        dsp::SynthEvent e;
        e.instrument = static_cast<Instrument>(i % 5);
        e.pitch = 24 + (i * 7) % 90;
        e.velocity = 1 + (i * 13) % 127;
        e.trackId = i % 7;
        e.noteId = static_cast<std::uint32_t>(i + 1);
        e.pan = (i % 3 == 0) ? std::numeric_limits<float>::quiet_NaN() : ((i % 5) - 2) * 2.0f;
        e.brightness = (i % 11 == 0) ? std::numeric_limits<float>::infinity() : 0.5f;
        e.gainDb = (i % 9 == 0) ? 1000.0f : -3.0f;
        e.holdSeconds = 0.01f;
        engine.noteOn(e);
        if (i % 77 == 0) {
            engine.render(l.data(), r.data(), sl.data(), sr.data(), 257);
            for (float v : l) CHECK(std::isfinite(v));
            for (float v : r) CHECK(std::isfinite(v));
        }
    }
    CHECK(engine.activeVoiceCount() <= dsp::SynthEngine::kMaxVoices);
    for (int block = 0; block < 8; ++block) {
        engine.render(l.data(), r.data(), sl.data(), sr.data(), 257);
        for (float v : l) CHECK(std::isfinite(v));
        for (float v : r) CHECK(std::isfinite(v));
    }
}

EC_TEST(render_works_at_multiple_sample_rates_and_block_sizes) {
    const Project p = shortProject(2);
    for (int sr : {44100, 48000, 96000}) {
        for (int block : {64, 257, 1024}) {
            RenderOptions options;
            options.sampleRate = sr;
            options.blockSize = block;
            options.tailSeconds = 0.4f;
            options.bitsPerSample = 24;
            std::vector<float> buffer;
            RenderStats stats;
            std::string error;
            CHECK_MSG(renderProjectToBuffer(p, options, buffer, &stats, &error), error);
            CHECK(stats.frames > 0);
            CHECK(stats.allFinite);
            CHECK(stats.peak <= 1.0001f);
            CHECK(stats.rms > 0.0001f);
            CHECK_EQ(buffer.size(), static_cast<std::size_t>(stats.frames * 2));
            for (float v : buffer) CHECK(std::isfinite(v));
        }
    }
}

EC_TEST(render_is_stereo_and_has_no_dc_offset) {
    const Project p = shortProject(3);
    RenderOptions options;
    options.sampleRate = 48000;
    options.blockSize = 512;
    options.tailSeconds = 0.5f;
    std::vector<float> buffer;
    RenderStats stats;
    std::string error;
    CHECK_MSG(renderProjectToBuffer(p, options, buffer, &stats, &error), error);
    double sumL = 0.0;
    double sumR = 0.0;
    double diff = 0.0;
    for (std::size_t i = 0; i < buffer.size(); i += 2) {
        sumL += buffer[i];
        sumR += buffer[i + 1];
        diff += std::abs(buffer[i] - buffer[i + 1]);
    }
    const double n = static_cast<double>(buffer.size() / 2);
    CHECK(std::abs(sumL / n) < 0.002);
    CHECK(std::abs(sumR / n) < 0.002);
    CHECK(diff / n > 1e-6); // panned parts are not identical mono
}

EC_TEST(wav_writer_round_trip_16_and_24_bit) {
    const auto outputDir = std::filesystem::current_path() / "test-output";
    std::filesystem::create_directories(outputDir);
    for (int bits : {16, 24}) {
        WavFileWriter writer;
        std::string error;
        const auto fsPath = outputDir / ("ec_wav_test_" + std::to_string(bits) + ".wav");
        const auto path = pathUtf8(fsPath);
        CHECK_MSG(writer.open(path, 48000, 2, bits, &error), error);
        std::vector<float> samples(2000);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            samples[i] = 0.5f * std::sin(static_cast<float>(i) * 0.03f);
        }
        CHECK_MSG(writer.writeInterleaved(samples.data(), 1000, &error), error);
        CHECK_MSG(writer.close(&error), error);
        WavInfo info = readWavInfo(path, &error);
        CHECK_MSG(info.valid, error);
        CHECK_EQ(info.sampleRate, 48000);
        CHECK_EQ(info.channels, 2);
        CHECK_EQ(info.bitsPerSample, bits);
        CHECK_EQ(info.frames, std::uint64_t(1000));
        std::vector<float> decoded;
        CHECK_MSG(readWavPcm(path, decoded, info, &error), error);
        CHECK_EQ(decoded.size(), samples.size());
        for (std::size_t i = 0; i < std::min<std::size_t>(100, decoded.size()); ++i) {
            CHECK_NEAR(decoded[i], samples[i], bits == 16 ? 0.0001f : 0.00001f);
        }
        std::filesystem::remove(fsPath);
    }
}

EC_TEST(render_writes_real_wav_file) {
    const Project p = shortProject(1);
    RenderOptions options;
    options.sampleRate = 44100;
    options.blockSize = 256;
    options.tailSeconds = 0.2f;
    options.bitsPerSample = 24;
    const auto outputDir = std::filesystem::current_path() / "test-output";
    std::filesystem::create_directories(outputDir);
    const auto fsPath = outputDir / "ec_render_test.wav";
    const auto path = pathUtf8(fsPath);
    RenderStats stats;
    std::string error;
    CHECK_MSG(renderProjectToWav(p, std::string(path), options, &stats, &error), error);
    const auto info = readWavInfo(std::string(path), &error);
    CHECK_MSG(info.valid, error);
    CHECK_EQ(info.channels, 2);
    CHECK_EQ(info.bitsPerSample, 24);
    CHECK(info.frames > 1000);
    std::filesystem::remove(fsPath);
}

