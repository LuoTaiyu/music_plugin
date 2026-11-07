#include "TestHarness.hpp"

#include "core/Composer.hpp"
#include "core/ProjectIO.hpp"

#include <string>

using namespace ec;

EC_TEST(project_json_round_trip_preserves_everything) {
    Project p = composeProject(demoSpec(DemoStyle::Cinematic, 2024));
    p.title = "A title with ünicode 与中文";
    p.revisionNote = "round-trip test";
    const std::string text = serializeProject(p, 2);
    Project q;
    std::string error;
    CHECK_MSG(deserializeProject(text, q, &error), error);
    CHECK_EQ(q.schemaVersion, kProjectSchemaVersion);
    CHECK_EQ(q.title, p.title);
    CHECK_EQ(q.seed, p.seed);
    CHECK_EQ(q.timeSignature.numerator, p.timeSignature.numerator);
    CHECK_EQ(q.timeSignature.denominator, p.timeSignature.denominator);
    CHECK_EQ(q.tempoMap.size(), p.tempoMap.size());
    CHECK_EQ(q.sections.size(), p.sections.size());
    CHECK_EQ(q.tracks.size(), p.tracks.size());
    CHECK_EQ(q.chords.size(), p.chords.size());
    CHECK_EQ(q.phrases.size(), p.phrases.size());
    CHECK_EQ(q.motifUses.size(), p.motifUses.size());
    CHECK_EQ(q.automation.size(), p.automation.size());
    for (std::size_t i = 0; i < p.tracks.size(); ++i) {
        CHECK_EQ(q.tracks[i].name, p.tracks[i].name);
        CHECK_EQ(static_cast<int>(q.tracks[i].instrument), static_cast<int>(p.tracks[i].instrument));
        CHECK_EQ(q.tracks[i].notes.size(), p.tracks[i].notes.size());
        for (std::size_t j = 0; j < p.tracks[i].notes.size(); ++j) {
            CHECK_EQ(q.tracks[i].notes[j].start, p.tracks[i].notes[j].start);
            CHECK_EQ(q.tracks[i].notes[j].pitch, p.tracks[i].notes[j].pitch);
            CHECK_EQ(q.tracks[i].notes[j].velocity, p.tracks[i].notes[j].velocity);
            CHECK_NEAR(q.tracks[i].notes[j].microMs, p.tracks[i].notes[j].microMs, 1e-4f);
        }
    }
    // The serialised forms are byte-identical for this model.
    CHECK_EQ(serializeProject(q, 0), serializeProject(p, 0));
}

EC_TEST(legacy_sketch_migrates_to_current_schema) {
    const std::string legacy = R"({
      "format": "EC_SKETCH_0",
      "title": "Old demo",
      "seed": 17,
      "bpm": 84.0,
      "bars": 4,
      "key": {"tonic": 2, "mode": "dorian"},
      "tracks": [
        {"name": "Piano", "channel": 2, "notes": [
          {"t": 0, "d": 960, "p": 60, "v": 80},
          {"t": 960, "d": 480, "p": 64, "v": 92}
        ]}
      ]
    })";
    Project p;
    std::string error;
    CHECK_MSG(deserializeProject(legacy, p, &error), error);
    CHECK_EQ(p.schemaVersion, kProjectSchemaVersion);
    CHECK_EQ(p.title, "Old demo");
    CHECK_EQ(p.seed, std::uint64_t(17));
    CHECK_EQ(p.tracks.size(), std::size_t(1));
    CHECK_EQ(p.tracks[0].notes.size(), std::size_t(2));
    CHECK_EQ(p.tracks[0].notes[1].pitch, 64);
    CHECK_EQ(p.tracks[0].midiChannel, 2);
    CHECK(p.revisionNote.find("EC_SKETCH_0") != std::string::npos);
}

EC_TEST(newer_schema_is_rejected) {
    Json root = Json::object();
    root.set("schemaVersion", 999);
    root.set("title", "future");
    Project p;
    std::string error;
    CHECK_EQ(projectFromJson(root, p, &error), false);
    CHECK(error.find("newer") != std::string::npos);
}

EC_TEST(validation_clamps_unsafe_note_values) {
    Project p;
    p.ppq = 960;
    p.tracks.push_back({1, "T", Role::Melody, Instrument::FeltPiano, 1, 0, 0, 0.3f, false, false, {}});
    Note n;
    n.pitch = 300;
    n.velocity = -4;
    n.duration = -10;
    n.microMs = 999.0f;
    p.tracks[0].notes.push_back(n);
    std::string error;
    CHECK_MSG(validateProject(p, &error), error);
    CHECK_EQ(p.tracks[0].notes[0].pitch, 127);
    CHECK_EQ(p.tracks[0].notes[0].velocity, 1);
    CHECK(p.tracks[0].notes[0].duration >= 1);
    CHECK_NEAR(p.tracks[0].notes[0].microMs, 100.0f, 1e-6f);
}

EC_TEST(project_total_bars_uses_time_signature) {
    Project p;
    p.timeSignature = {5, 4};
    p.ppq = 960;
    p.sections.push_back({"S", "setup", "", 0, 4, {}});
    CHECK_EQ(p.totalBars(), 4);
    CHECK_EQ(p.totalTicks(), static_cast<Tick>(4 * 5 * 960));
    p.timeSignature = {7, 8};
    p.sections = {{"S", "setup", "", 0, 4, {}}};
    CHECK_EQ(p.totalTicks(), static_cast<Tick>(4 * 7 * 480));
}

EC_TEST(regeneration_outside_range_is_preserved) {
    Project p = composeProject(demoSpec(DemoStyle::Chamber, 314));
    RegenerationOptions o;
    o.seed = 2718;
    o.locks = LockDimension::None;
    o.rangeStartBar = 8;
    o.rangeEndBar = 16;
    Project q = regenerateProject(p, o);
    auto collect = [](const Track& t, int fromBar, int toBar) {
        std::string s;
        for (const auto& n : t.notes) {
            const int bar = static_cast<int>(n.start / (4 * 960));
            if (bar < fromBar || bar >= toBar) s += std::to_string(n.start) + ":" + std::to_string(n.pitch) + ";";
        }
        return s;
    };
    for (std::size_t i = 0; i < p.tracks.size() && i < q.tracks.size(); ++i) {
        CHECK_EQ(collect(p.tracks[i], 8, 16), collect(q.tracks[i], 8, 16));
    }
}
