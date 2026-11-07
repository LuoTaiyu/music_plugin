#include "TestHarness.hpp"

#include "core/Composer.hpp"
#include "core/Expression.hpp"
#include "core/Json.hpp"
#include "core/Motif.hpp"
#include "core/ProjectIO.hpp"
#include "core/Theory.hpp"
#include "core/TimeMap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string>

using namespace ec;

EC_TEST(time_map_is_sample_rate_independent) {
    TimeMap tm({{0, 120.0}, {1920, 60.0}}, 960);
    CHECK_NEAR(tm.secondsAtTick(0), 0.0, 1e-12);
    CHECK_NEAR(tm.secondsAtTick(960), 0.5, 1e-9);
    CHECK_NEAR(tm.secondsAtTick(1920), 1.0, 1e-9);
    CHECK_NEAR(tm.secondsAtTick(3840), 3.0, 1e-9);
    CHECK_EQ(tm.tickAtSeconds(1.0), 1920);
    CHECK_EQ(tm.tickAtSeconds(3.0), 3840);
    CHECK_EQ(tm.tickAtBeats(4.5), 4320);
}

EC_TEST(curves_are_smooth_and_bounded) {
    Curve c;
    c.setDefault({{0, -1.0f}, {4, 1.0f}, {8, -1.0f}});
    CHECK_NEAR(c.at(-1), -1.0f, 1e-6);
    CHECK_NEAR(c.at(0), -1.0f, 1e-6);
    CHECK_NEAR(c.at(4), 1.0f, 1e-6);
    CHECK_NEAR(c.at(8), -1.0f, 1e-6);
    CHECK_NEAR(c.at(2), 0.0f, 1e-6);
    c.addPoint(4.0, 0.5f);
    CHECK_NEAR(c.at(4.0), 0.5f, 1e-6);
    CHECK_EQ(c.removeNear(99.0), false);
    CHECK_EQ(c.removeNear(4.0), true);
}

EC_TEST(f_minor_scale_and_chord) {
    const Key key{5, Mode::NaturalMinor};
    const auto scale = makeScale(key);
    const std::array<int, 7> expected{5, 7, 8, 10, 0, 1, 3};
    for (std::size_t i = 0; i < expected.size(); ++i) CHECK_EQ(scale.pitchClasses[i], expected[i]);
    const auto pcs = chordPitchClasses(key, 0, ChordQuality::Minor, true);
    CHECK(std::find(pcs.begin(), pcs.end(), 5) != pcs.end());
    CHECK(std::find(pcs.begin(), pcs.end(), 8) != pcs.end());
    CHECK(std::find(pcs.begin(), pcs.end(), 0) != pcs.end());
    CHECK(std::find(pcs.begin(), pcs.end(), 7) != pcs.end()); // ninth G
}

EC_TEST(voicing_respects_range_and_crossing) {
    const auto pcs = chordPitchClasses({5, Mode::NaturalMinor}, 0, ChordQuality::Minor7, false);
    const auto v = voiceChord({}, pcs, 5, 48, 76, 4, 62);
    CHECK_EQ(v.size(), std::size_t(4));
    CHECK_EQ(v.front() % 12, 5);
    for (std::size_t i = 1; i < v.size(); ++i) CHECK(v[i] > v[i - 1]);
    const auto report = checkVoicings({v});
    CHECK_EQ(report.voiceCrossings, 0);
    CHECK_EQ(report.excessiveSpacing, 0);
}

EC_TEST(expression_mapping_uses_all_axes) {
    AffectState calm{0.2f, 0.1f, 0.1f, 0.1f};
    AffectState tense{-0.6f, 0.8f, 0.95f, 0.9f};
    const auto a = ExpressionMapper::derive(calm, Restraint::Restrained, Role::Melody);
    const auto b = ExpressionMapper::derive(tense, Restraint::Direct, Role::Melody);
    CHECK(b.density > a.density);
    CHECK(b.harmonicTension > a.harmonicTension);
    CHECK(b.reverbSend > a.reverbSend);
    CHECK(b.leapProbability > a.leapProbability);
    CHECK(b.brightness != a.brightness); // brightness uses valence/arousal/distance, not mode labels
}

EC_TEST(motif_transformations_are_traceable) {
    const Motif m = chamberMotif();
    CHECK_EQ(m.notes.size(), std::size_t(4));
    const auto inverted = transformMotif(m, MotifTransform::Invert, 0);
    CHECK_EQ(inverted.notes.front().degree, m.notes.front().degree);
    const auto retro = transformMotif(m, MotifTransform::Retrograde, 0);
    CHECK_EQ(retro.notes.front().degree, m.notes.back().degree);
    const auto dim = transformMotif(m, MotifTransform::Diminish, 0);
    CHECK(dim.notes.front().duration < m.notes.front().duration);
    const auto aug = transformMotif(m, MotifTransform::Augment, 0);
    CHECK(aug.notes.front().duration > m.notes.front().duration);
}

EC_TEST(three_demos_are_real_forms_not_four_bar_loops) {
    for (auto style : {DemoStyle::Chamber, DemoStyle::Cinematic, DemoStyle::Kinetic}) {
        const auto spec = demoSpec(style, 4242);
        GenerationDiagnostics diag;
        const Project p = composeProject(spec, &diag);
        CHECK(p.sections.size() >= 6);
        CHECK(p.totalBars() >= 30);
        CHECK(p.chords.size() >= 30);
        CHECK(p.motifs.size() == 1);
        CHECK(p.motifUses.size() >= 10);
        CHECK(p.phrases.size() >= 10);
        int notes = 0;
        for (const auto& t : p.tracks) notes += static_cast<int>(t.notes.size());
        CHECK(notes > 100);
        CHECK(diag.noteCount == notes);
        CHECK(diag.chordCount > 0);
        CHECK_EQ(diag.counterpoint.parallelFifths, 0);
        CHECK_EQ(diag.counterpoint.parallelOctaves, 0);
        CHECK(p.tempoMap.front().bpm >= 60.0);
        CHECK(p.totalTicks() > 0);
    }
}

EC_TEST(composer_is_deterministic_for_same_seed) {
    const auto spec = demoSpec(DemoStyle::Chamber, 777);
    const Project a = composeProject(spec);
    const Project b = composeProject(spec);
    CHECK_EQ(serializeProject(a, 0), serializeProject(b, 0));
}

EC_TEST(composer_changes_with_seed_and_style) {
    const Project a = composeProject(demoSpec(DemoStyle::Chamber, 11));
    const Project b = composeProject(demoSpec(DemoStyle::Chamber, 12));
    CHECK(serializeProject(a, 0) != serializeProject(b, 0));
    const Project c = composeProject(demoSpec(DemoStyle::Cinematic, 11));
    CHECK(c.timeSignature.numerator == 5);
    CHECK(a.timeSignature.numerator == 4);
    CHECK(static_cast<int>(c.key.mode) == static_cast<int>(Mode::Phrygian));
}

EC_TEST(humanization_is_grouped_and_bounded) {
    Project p = composeProject(demoSpec(DemoStyle::Chamber, 99));
    CHECK(!p.tracks.empty());
    int changed = 0;
    std::set<int> velocities;
    for (const auto& t : p.tracks) {
        for (const auto& n : t.notes) {
            velocities.insert(n.velocity);
            CHECK(n.microMs >= -12.0f && n.microMs <= 12.0f);
            CHECK(n.brightness >= 0.0f && n.brightness <= 1.0f);
            CHECK(n.legato >= 0.0f && n.legato <= 1.0f);
            if (std::abs(n.microMs) > 0.01f) ++changed;
        }
    }
    CHECK(velocities.size() > 10);
    CHECK(changed > 10);
}

EC_TEST(all_locks_return_unchanged_copy) {
    Project p = composeProject(demoSpec(DemoStyle::Kinetic, 5));
    RegenerationOptions o;
    o.seed = 6;
    o.locks = LockDimension::Melody | LockDimension::Harmony | LockDimension::Rhythm |
              LockDimension::Orchestration | LockDimension::Humanization | LockDimension::Expression;
    const Project q = regenerateProject(p, o);
    CHECK_EQ(serializeProject(p, 0), serializeProject(q, 0));
}

EC_TEST(partial_locks_keep_melody_while_harmony_changes) {
    Project p = composeProject(demoSpec(DemoStyle::Chamber, 5));
    std::string melodyBefore;
    for (const auto& t : p.tracks) {
        if (t.role == Role::Melody) {
            for (const auto& n : t.notes) melodyBefore += std::to_string(n.start) + ":" + std::to_string(n.pitch) + ";";
        }
    }
    RegenerationOptions o;
    o.seed = 12345;
    o.locks = LockDimension::Melody;
    Project q = regenerateProject(p, o);
    std::string melodyAfter;
    for (const auto& t : q.tracks) {
        if (t.role == Role::Melody) {
            for (const auto& n : t.notes) melodyAfter += std::to_string(n.start) + ":" + std::to_string(n.pitch) + ";";
        }
    }
    CHECK_EQ(melodyBefore, melodyAfter);
    CHECK(serializeProject(p, 0) != serializeProject(q, 0));
}

EC_TEST(parser_rejects_malformed_json) {
    std::string error;
    Project p;
    CHECK_EQ(deserializeProject("{\"title\":", p, &error), false);
    CHECK(!error.empty());
    CHECK_EQ(deserializeProject("[1,2,3]", p, &error), false);
}

EC_TEST(json_unicode_and_escapes_round_trip) {
    Json j = Json::object();
    j.set("title", "安静 room \"one\"\nline");
    const auto text = j.dump(2);
    std::string error;
    const Json parsed = Json::parse(text, &error);
    CHECK(error.empty());
    CHECK_EQ(parsed["title"].asString(), "安静 room \"one\"\nline");
}
