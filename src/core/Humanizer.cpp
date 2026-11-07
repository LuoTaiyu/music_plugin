#include "core/Composer.hpp"

#include "core/Rng.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace ec {
namespace {

int beatsPerBarFor(const Project& p) {
    return std::max(1, p.timeSignature.numerator * 4 / std::max(1, p.timeSignature.denominator));
}

StyleBias biasFor(const std::string& id) {
    if (id == "cinematic") return {0.08f, -0.03f, 0.03f, 0.04f, 0.16f, -0.02f, 0.14f, 54, 22, 4.0};
    if (id == "kinetic") return {0.16f, -0.05f, 0.01f, 0.16f, 0.04f, 0.06f, 0.0f, 64, 19, 1.5};
    return {-0.18f, 0.06f, -0.04f, -0.02f, -0.01f, -0.05f, 0.04f, 63, 14, 3.5};
}

const Phrase* phraseFor(const Project& p, Tick tick) {
    for (const auto& ph : p.phrases) {
        if (tick >= ph.start && tick < ph.end) return &ph;
    }
    return nullptr;
}

int sectionFor(const Project& p, Tick tick) {
    const int barTicks = static_cast<int>(ticksPerBar(p.timeSignature, p.ppq));
    for (std::size_t i = 0; i < p.sections.size(); ++i) {
        const Tick start = static_cast<Tick>(p.sections[i].startBar) * barTicks;
        const Tick end = static_cast<Tick>(p.sections[i].startBar + p.sections[i].bars) * barTicks;
        if (tick >= start && tick < end) return static_cast<int>(i);
    }
    return -1;
}

float smoothDrift(std::uint64_t seed, double t, double phase) {
    Rng rng(seed);
    const double a = rng.nextDouble() * 6.283185307179586;
    const double b = rng.nextDouble() * 6.283185307179586;
    return static_cast<float>(0.55 * std::sin(6.283185307179586 * (0.73 * t + phase) + a) +
                              0.30 * std::sin(6.283185307179586 * (1.61 * t + phase) + b));
}

} // namespace

void exposeProjectMetadata(Project& p) {
    p.automation.clear();
    auto addCurveLane = [&](const std::string& id, const std::string& target, const Curve& curve) {
        AutomationLane lane;
        lane.id = id;
        lane.target = target;
        lane.unit = "normalized";
        lane.points = curve.points;
        p.automation.push_back(std::move(lane));
    };
    addCurveLane("curve.valence", "expression.valence", p.curves.valence);
    addCurveLane("curve.arousal", "expression.arousal", p.curves.arousal);
    addCurveLane("curve.tension", "expression.tension", p.curves.tension);
    addCurveLane("curve.distance", "expression.distance", p.curves.distance);

    const int beats = beatsPerBarFor(p);
    for (const auto& track : p.tracks) {
        AutomationLane lane;
        lane.id = "track." + std::to_string(track.id) + ".reverbSend";
        lane.target = "track.reverbSend";
        lane.unit = "normalized";
        for (const auto& section : p.sections) {
            const double beat = static_cast<double>(section.startBar) * beats;
            const float value = std::clamp(track.reverbSend + 0.18f * (section.target.distance - 0.35f), 0.0f, 1.0f);
            lane.points.push_back({beat, value});
        }
        p.automation.push_back(std::move(lane));
    }
}

void humanizeProject(Project& p, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (amount <= 0.0f) return;
    const StyleBias bias = biasFor(p.styleId);
    const int barTicks = static_cast<int>(ticksPerBar(p.timeSignature, p.ppq));
    const int beatsPerBar = beatsPerBarFor(p);

    for (auto& track : p.tracks) {
        if (track.notes.empty()) continue;
        for (std::size_t ni = 0; ni < track.notes.size(); ++ni) {
            Note& note = track.notes[ni];
            const Phrase* phrase = phraseFor(p, note.start);
            const int sectionIndex = sectionFor(p, note.start);
            const auto state = p.curves.at(static_cast<double>(note.start) / p.ppq);
            const auto aff = ExpressionMapper::derive(
                state, p.intent.restraint, track.role, bias);
            const Tick start = phrase ? phrase->start : static_cast<Tick>(std::max(0, sectionIndex)) * barTicks * 2;
            const Tick end = phrase ? phrase->end : start + barTicks * 2;
            const double t = std::clamp(static_cast<double>(note.start - start) /
                                        static_cast<double>(std::max<Tick>(1, end - start)), 0.0, 1.0);
            const std::uint64_t phraseSeed = mixSeed(p.seed, static_cast<std::uint64_t>(track.id) * 131 +
                                                               static_cast<std::uint64_t>(phrase ? (phrase - p.phrases.data()) : 500 + sectionIndex));
            const double phase = static_cast<double>((phraseSeed >> 32) & 0xFFFF) / 65535.0;
            const float drift = smoothDrift(phraseSeed, t, phase);
            const float arch = static_cast<float>(std::sin(3.141592653589793 * t));
            const float endTaper = static_cast<float>(t * t);
            const bool percussion = track.instrument == Instrument::DrumKit || track.role == Role::Percussion;
            const float baseVel = static_cast<float>(note.velocity);
            float velocity = baseVel + amount * (8.0f * arch - 5.0f * endTaper + 3.0f * drift);
            if (phrase && note.id == phrase->peakNoteId) velocity += 5.0f * amount;
            if (percussion) {
                // Keep the groove grid stable; vary groups, not each hit.
                velocity = baseVel + amount * (4.0f * drift + (note.pitch == 38 ? 2.0f : 0.0f));
            }
            note.velocity = std::clamp(static_cast<int>(std::lround(velocity)), 8, 125);
            const float velNorm = static_cast<float>(note.velocity) / 127.0f;
            note.brightness = std::clamp(note.brightness + amount * (0.13f * (velNorm - 0.55f) + 0.04f * arch),
                                         0.0f, 1.0f);
            note.attack = std::clamp(note.attack + amount * (0.06f * (0.6f - velNorm) - 0.03f * aff.articulation),
                                     0.0f, 1.0f);
            note.noise = std::clamp(note.noise + amount * (0.04f * (velNorm - 0.5f) + 0.02f * aff.noise), 0.0f, 1.0f);
            note.release = std::clamp(aff.release + amount * 0.06f * (1.0f - static_cast<float>(t)), 0.0f, 1.0f);

            const float articulation = percussion ? 0.18f : std::clamp(aff.articulation + 0.08f * arch - 0.10f * endTaper, 0.05f, 0.98f);
            note.legato = note.legato * (1.0f - 0.35f * amount) + articulation * (0.35f * amount);
            if (!percussion) {
                const Tick extension = static_cast<Tick>(amount * (note.legato - 0.45f) * 0.09 * p.ppq);
                note.duration = std::max<Tick>(p.ppq / 16, note.duration + extension);
            }

            float micro = 0.0f;
            if (percussion) {
                if (note.pitch == 36) micro = 0.0f;
                else if (note.pitch == 38) micro = 4.0f * amount;
                else micro = -2.0f * amount;
            } else if (p.styleId == "kinetic") {
                const double beatInBar = static_cast<double>(note.start) / p.ppq;
                const double fractional = beatInBar - std::floor(beatInBar);
                const float push = fractional > 0.45 && fractional < 0.55 ? -1.8f : 0.8f;
                micro = amount * (push + 1.4f * drift - 1.2f * arch);
            } else {
                micro = amount * (1.6f + 4.8f * static_cast<float>(t) - 7.0f * arch + 1.6f * drift);
            }
            note.microMs = std::clamp(micro, -12.0f, 12.0f);

            const float width = aff.stereoWidth * 0.16f;
            note.pan = std::clamp(track.pan + amount * width * std::sin(3.141592653589793f * static_cast<float>(t) + drift), -1.0f, 1.0f);
            note.phrase = phrase ? static_cast<int>(phrase - p.phrases.data()) : -1;

            // Keep the note inside its bar/section for safety; micro-timing is
            // applied at render/export time and does not corrupt the edit grid.
            if (note.duration <= 0) note.duration = p.ppq / 16;
            (void)beatsPerBar;
            (void)barTicks;
        }
    }
}

} // namespace ec
