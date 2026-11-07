#include "TestHarness.hpp"

#include "core/Composer.hpp"
#include "core/ProjectIO.hpp"
#include "io/MidiFile.hpp"

#include <algorithm>
#include <vector>

using namespace ec;

namespace {

Project smallProject() {
    Project p;
    p.schemaVersion = kProjectSchemaVersion;
    p.title = "MIDI test";
    p.styleId = "chamber";
    p.seed = 123;
    p.ppq = 960;
    p.timeSignature = {7, 8};
    p.tempoMap = {{0, 96.0}, {5 * 7 * 480, 84.0}};
    p.key = {2, Mode::Dorian};
    p.curves = defaultExpressionCurves("chamber");
    p.intent = defaultIntent("chamber");
    p.sections = {{"Opening", "setup", "", 0, 3, {0, 0.2f, 0.2f, 0.3f}},
                  {"Turn", "turn", "", 3, 2, {0, 0.6f, 0.5f, 0.5f}}};
    p.tracks.push_back({1, "Piano", Role::Melody, Instrument::FeltPiano, 1, -2.0f, 0.0f, 0.3f, false, false, {}});
    p.tracks.push_back({2, "Bass", Role::Bass, Instrument::OrganicBass, 2, -4.0f, 0.0f, 0.1f, false, false, {}});
    Note a;
    a.id = 1; a.start = 0; a.duration = 960; a.pitch = 62; a.velocity = 89;
    a.microMs = 3.0f; a.brightness = 0.55f;
    Note b;
    b.id = 2; b.start = 480; b.duration = 960; b.pitch = 62; b.velocity = 64;
    b.microMs = -2.0f;
    Note c;
    c.id = 3; c.start = 7 * 480; c.duration = 1440; c.pitch = 65; c.velocity = 100;
    p.tracks[0].notes = {a, b, c};
    Note d;
    d.id = 4; d.start = 0; d.duration = 7 * 480; d.pitch = 38; d.velocity = 70;
    p.tracks[1].notes = {d};
    p.nextNoteId = 5;
    return p;
}

std::string noteSignature(const Project& p) {
    std::vector<std::string> lines;
    for (const auto& t : p.tracks) {
        for (const auto& n : t.notes) {
            lines.push_back(t.name + ":" + std::to_string(n.start) + ":" +
                            std::to_string(n.duration) + ":" + std::to_string(n.pitch) + ":" +
                            std::to_string(n.velocity));
        }
    }
    std::sort(lines.begin(), lines.end());
    std::string out;
    for (const auto& line : lines) out += line + "\n";
    return out;
}

} // namespace

EC_TEST(midi_embedded_payload_round_trips_exactly) {
    const Project p = smallProject();
    std::vector<std::uint8_t> bytes;
    std::string error;
    MidiExportStats stats;
    CHECK_MSG(writeMidiToMemory(p, {}, bytes, &stats, &error), error);
    CHECK(stats.bytes == bytes.size());
    CHECK(stats.notes == 4);
    CHECK(stats.tracks == 2);
    auto result = readMidiFromMemory(bytes, true);
    CHECK_MSG(result.success, result.error);
    CHECK(result.usedEmbeddedProject);
    CHECK_EQ(result.noteCount, 4);
    CHECK_EQ(serializeProject(result.project, 0), serializeProject(p, 0));
}

EC_TEST(midi_standard_round_trip_preserves_tempo_meter_markers_and_overlaps) {
    Project p = smallProject();
    for (auto& track : p.tracks) for (auto& note : track.notes) note.microMs = 0.0f;
    MidiExportOptions options;
    options.includeEmbeddedProject = false;
    std::vector<std::uint8_t> bytes;
    std::string error;
    CHECK_MSG(writeMidiToMemory(p, options, bytes, nullptr, &error), error);
    auto result = readMidiFromMemory(bytes, false);
    CHECK_MSG(result.success, result.error);
    CHECK(!result.usedEmbeddedProject);
    CHECK_EQ(result.project.timeSignature.numerator, 7);
    CHECK_EQ(result.project.title, "MIDI test");
    CHECK_EQ(result.project.timeSignature.denominator, 8);
    CHECK_EQ(result.project.tempoMap.size(), std::size_t(2));
    CHECK_NEAR(result.project.tempoMap[0].bpm, 96.0, 0.01);
    CHECK_NEAR(result.project.tempoMap[1].bpm, 84.0, 0.01);
    CHECK_EQ(result.project.sections.size(), std::size_t(2));
    CHECK_EQ(result.project.sections[0].name, "Opening");
    CHECK_EQ(result.project.sections[1].name, "Turn");
    CHECK_EQ(result.project.sections[0].startBar, 0);
    CHECK_EQ(result.project.sections[1].startBar, 3);
    CHECK_EQ(noteSignature(result.project), noteSignature(p));
    CHECK_EQ(result.noteCount, 4);
}

EC_TEST(midi_section_markers_do_not_require_embedded_payload) {
    Project p = smallProject();
    p.tracks[1].notes.clear();
    MidiExportOptions options;
    options.includeEmbeddedProject = false;
    std::vector<std::uint8_t> bytes;
    std::string error;
    CHECK_MSG(writeMidiToMemory(p, options, bytes, nullptr, &error), error);
    auto result = readMidiFromMemory(bytes, false);
    CHECK_MSG(result.success, result.error);
    CHECK_EQ(result.project.sections.size(), std::size_t(2));
    CHECK_EQ(result.project.sections[1].name, "Turn");
}

EC_TEST(midi_rejects_malformed_input) {
    std::vector<std::uint8_t> bad{'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 1, 0, 96};
    auto result = readMidiFromMemory(bad, false);
    CHECK(!result.success);
    CHECK(!result.error.empty());

    std::vector<std::uint8_t> smpte{'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 1, 0xE7, 0x28};
    result = readMidiFromMemory(smpte, false);
    CHECK(!result.success);
    CHECK(result.error.find("SMPTE") != std::string::npos);
}

EC_TEST(midi_micro_timing_is_written_into_tick_positions) {
    Project p = smallProject();
    p.tracks[0].notes[0].microMs = 10.0f; // +16 ticks at 96 BPM / 960 PPQ
    MidiExportOptions options;
    options.includeEmbeddedProject = false;
    std::vector<std::uint8_t> bytes;
    std::string error;
    CHECK_MSG(writeMidiToMemory(p, options, bytes, nullptr, &error), error);
    auto result = readMidiFromMemory(bytes, false);
    CHECK_MSG(result.success, result.error);
    bool found = false;
    for (const auto& t : result.project.tracks) {
        for (const auto& n : t.notes) {
            if (n.pitch == 62 && n.velocity == 89) {
                CHECK(n.start > 0);
                found = true;
            }
        }
    }
    CHECK(found);
}

EC_TEST(midi_import_prefers_payload_but_standard_path_still_works) {
    Project p = smallProject();
    std::vector<std::uint8_t> bytes;
    std::string error;
    CHECK_MSG(writeMidiToMemory(p, {}, bytes, nullptr, &error), error);
    auto embedded = readMidiFromMemory(bytes, true);
    CHECK(embedded.success && embedded.usedEmbeddedProject);
    auto standard = readMidiFromMemory(bytes, false);
    CHECK(standard.success);
    CHECK(!standard.usedEmbeddedProject);
    CHECK_EQ(standard.noteCount, 4);
}
