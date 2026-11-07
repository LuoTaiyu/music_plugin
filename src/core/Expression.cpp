#include "core/Expression.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ec {
namespace {

float clampUnit(float v) { return std::clamp(v, 0.0f, 1.0f); }
float clampSigned(float v) { return std::clamp(v, -1.0f, 1.0f); }

float roleDensity(Role role) {
    switch (role) {
        case Role::Melody: return 0.62f;
        case Role::Harmony: return 0.55f;
        case Role::Bass: return 0.48f;
        case Role::Counter: return 0.38f;
        case Role::Texture: return 0.35f;
        case Role::Percussion: return 0.70f;
    }
    return 0.5f;
}

float roleRegisterOffset(Role role) {
    switch (role) {
        case Role::Melody: return 7.0f;
        case Role::Harmony: return 0.0f;
        case Role::Bass: return -19.0f;
        case Role::Counter: return -6.0f;
        case Role::Texture: return 4.0f;
        case Role::Percussion: return -12.0f;
    }
    return 0.0f;
}

} // namespace

float Curve::at(double beat) const noexcept {
    if (points.empty()) return 0.0f;
    if (beat <= points.front().beat) return points.front().value;
    if (beat >= points.back().beat) return points.back().value;
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (beat <= points[i].beat) {
            const auto& a = points[i - 1];
            const auto& b = points[i];
            const double span = std::max(1e-9, b.beat - a.beat);
            const float t = static_cast<float>((beat - a.beat) / span);
            // Smoothstep avoids a visible/audible corner at every control point.
            const float s = t * t * (3.0f - 2.0f * t);
            return a.value + (b.value - a.value) * s;
        }
    }
    return points.back().value;
}

void Curve::addPoint(double beat, float value) {
    if (!std::isfinite(beat) || !std::isfinite(value)) return;
    beat = std::max(0.0, beat);
    value = std::clamp(value, -1.0f, 1.0f);
    for (auto& p : points) {
        if (std::abs(p.beat - beat) < 1e-6) {
            p.value = value;
            return;
        }
    }
    points.push_back({beat, value});
    sort();
}

bool Curve::removeNear(double beat, double tolerance) {
    if (points.size() <= 2) return false;
    auto it = std::min_element(points.begin(), points.end(), [beat](const CurvePoint& a, const CurvePoint& b) {
        return std::abs(a.beat - beat) < std::abs(b.beat - beat);
    });
    if (it == points.end() || std::abs(it->beat - beat) > tolerance) return false;
    points.erase(it);
    return true;
}

void Curve::sort() {
    std::stable_sort(points.begin(), points.end(), [](const CurvePoint& a, const CurvePoint& b) {
        return a.beat < b.beat;
    });
}

void Curve::setDefault(std::initializer_list<CurvePoint> pts) {
    points.assign(pts.begin(), pts.end());
    sort();
}

AffectState ExpressionCurves::at(double beat) const noexcept {
    return {valence.at(beat), arousal.at(beat), tension.at(beat), distance.at(beat)};
}

namespace {

Affordances roleBaseline(Role role) {
    Affordances a;
    a.density = roleDensity(role);
    a.registerCenter = 60 + static_cast<int>(roleRegisterOffset(role));
    switch (role) {
        case Role::Melody:
            a.restProbability = 0.16f;
            a.leapProbability = 0.22f;
            a.articulation = 0.65f;
            a.velocityBase = 0.68f;
            a.velocityRange = 0.22f;
            break;
        case Role::Harmony:
            a.restProbability = 0.06f;
            a.leapProbability = 0.05f;
            a.articulation = 0.8f;
            a.velocityBase = 0.48f;
            a.velocityRange = 0.12f;
            break;
        case Role::Bass:
            a.restProbability = 0.1f;
            a.leapProbability = 0.1f;
            a.articulation = 0.55f;
            a.velocityBase = 0.6f;
            a.velocityRange = 0.15f;
            break;
        case Role::Counter:
            a.restProbability = 0.24f;
            a.leapProbability = 0.12f;
            a.articulation = 0.75f;
            a.velocityBase = 0.5f;
            a.velocityRange = 0.14f;
            break;
        case Role::Texture:
            a.restProbability = 0.1f;
            a.leapProbability = 0.02f;
            a.articulation = 0.9f;
            a.velocityBase = 0.38f;
            a.velocityRange = 0.1f;
            break;
        case Role::Percussion:
            a.restProbability = 0.05f;
            a.leapProbability = 0.0f;
            a.articulation = 0.2f;
            a.velocityBase = 0.72f;
            a.velocityRange = 0.2f;
            break;
    }
    return a;
}

} // namespace

Affordances ExpressionMapper::derive(const AffectState& state,
                                     Restraint restraint,
                                     Role role,
                                     const StyleBias& style) {
    Affordances a = roleBaseline(role);
    const float v = clampSigned(state.valence);
    const float arousal = clampUnit(state.arousal);
    const float tension = clampUnit(state.tension);
    const float distance = clampUnit(state.distance);

    // Arousal controls event energy and temporal articulation, but never sets
    // mode. Valence shifts colour and resolution tendency instead.
    a.density += 0.30f * (arousal - 0.4f);
    a.density -= 0.18f * (1.0f - tension);
    a.density += style.density;
    a.restProbability += 0.12f * (1.0f - arousal);
    a.restProbability += 0.05f * (1.0f - tension);
    a.restProbability += style.rest;
    a.leapProbability += 0.10f * (arousal - 0.4f) + 0.06f * (1.0f - distance);
    a.leapProbability += style.leap;

    // Tension opens a bounded dissonance budget, a low-register pull and a
    // more uneven pulse. These are engineering heuristics, not laws.
    a.harmonicTension = clampUnit(0.18f + 0.48f * tension + 0.12f * distance + style.tension);
    a.dissonanceBudget = clampUnit(0.05f + 0.45f * tension + style.tension * 0.5f);
    a.syncopation = clampUnit(0.08f + 0.30f * tension + 0.25f * (arousal - 0.5f) + style.syncopation);
    a.harmonicRhythmBeats = std::clamp(style.harmonicRhythm * (1.35 - 0.55 * tension) * (1.15 - 0.25 * arousal),
                                       0.5, 8.0);
    a.harmonicRhythmBeats = std::max(0.5, a.harmonicRhythmBeats);

    a.registerCenter = style.registerCenter + static_cast<int>(roleRegisterOffset(role));
    a.registerCenter += static_cast<int>(std::lround((distance - 0.35f) * 8.0f));
    a.registerCenter += static_cast<int>(std::lround((arousal - 0.5f) * 4.0f));
    a.registerCenter -= static_cast<int>(std::lround(tension * 2.0f));
    a.registerSpan = style.registerSpan + static_cast<int>(std::lround((1.0f - distance) * 3.0f + tension * 4.0f));
    a.registerSpan = std::clamp(a.registerSpan, 6, 30);

    a.velocityBase = clampUnit(0.34f + 0.34f * arousal + 0.10f * tension + (role == Role::Percussion ? 0.08f : 0.0f));
    a.velocityRange = clampUnit(0.08f + 0.20f * (1.0f - distance) + 0.08f * tension);
    a.articulation = clampUnit(0.35f + 0.40f * (1.0f - arousal) + 0.18f * (1.0f - tension));
    a.brightness = clampUnit(0.44f + 0.18f * v + 0.16f * arousal + 0.10f * (1.0f - tension) - 0.22f * distance + style.brightness);
    a.noise = clampUnit(0.20f + 0.18f * (1.0f - v) + 0.10f * tension + 0.08f * (1.0f - arousal));
    a.release = clampUnit(0.42f + 0.25f * distance + 0.10f * (1.0f - arousal));
    a.reverbSend = clampUnit(0.18f + 0.52f * distance + style.reverb);
    a.stereoWidth = clampUnit(0.18f + 0.45f * distance + (role == Role::Melody ? 0.05f : 0.12f));
    a.highFrequencyDamping = clampUnit(0.12f + 0.48f * distance);
    a.layerBudget = std::clamp(1 + static_cast<int>(std::lround(arousal * 2.0f + tension)), 1, 5);
    a.unresolvedEnding = (v < 0.25f) || (tension > 0.55f);

    if (restraint == Restraint::Restrained) {
        a.density *= 0.86f;
        a.velocityBase *= 0.86f;
        a.velocityRange *= 0.75f;
        a.brightness *= 0.90f;
        a.reverbSend *= 0.82f;
    } else if (restraint == Restraint::OpeningUp) {
        a.density *= 1.0f;
        a.velocityRange *= 1.15f;
        a.brightness = clampUnit(a.brightness * 1.06f);
    } else {
        a.density *= 1.12f;
        a.velocityBase = clampUnit(a.velocityBase * 1.08f);
        a.velocityRange *= 1.30f;
        a.brightness = clampUnit(a.brightness * 1.12f);
    }

    a.density = clampUnit(a.density);
    a.restProbability = clampUnit(a.restProbability);
    a.leapProbability = clampUnit(a.leapProbability);
    a.reverbSend = clampUnit(a.reverbSend);
    return a;
}

std::vector<std::string> ExpressionMapper::explain(const AffectState& state,
                                                   Restraint restraint,
                                                   Role role,
                                                   const StyleBias& style) {
    const auto a = derive(state, restraint, role, style);
    std::ostringstream ss;
    std::vector<std::string> out;
    ss << "valence=" << state.valence << " arousal=" << state.arousal
       << " tension=" << state.tension << " distance=" << state.distance;
    out.push_back(ss.str());
    ss.str({});
    ss << "density=" << a.density << " rest=" << a.restProbability
       << " leap=" << a.leapProbability << " syncopation=" << a.syncopation;
    out.push_back(ss.str());
    ss.str({});
    ss << "harmonicTension=" << a.harmonicTension
       << " harmonicRhythmBeats=" << a.harmonicRhythmBeats
       << " dissonanceBudget=" << a.dissonanceBudget;
    out.push_back(ss.str());
    ss.str({});
    ss << "registerCenter=" << a.registerCenter << " span=" << a.registerSpan
       << " reverb=" << a.reverbSend << " width=" << a.stereoWidth
       << " damping=" << a.highFrequencyDamping;
    out.push_back(ss.str());
    return out;
}

ExpressionCurves defaultExpressionCurves(const std::string& styleId) {
    ExpressionCurves c;
    if (styleId == "cinematic") {
        c.valence.setDefault({{0, -0.15f}, {20, -0.35f}, {52, -0.52f}, {80, -0.62f}, {104, -0.4f}, {128, -0.28f}});
        c.arousal.setDefault({{0, 0.22f}, {24, 0.36f}, {56, 0.55f}, {84, 0.88f}, {100, 0.82f}, {128, 0.3f}});
        c.tension.setDefault({{0, 0.30f}, {24, 0.42f}, {56, 0.66f}, {84, 0.94f}, {100, 0.72f}, {128, 0.38f}});
        c.distance.setDefault({{0, 0.55f}, {32, 0.62f}, {72, 0.78f}, {96, 0.85f}, {128, 0.55f}});
    } else if (styleId == "kinetic") {
        c.valence.setDefault({{0, 0.05f}, {16, 0.22f}, {40, 0.36f}, {64, 0.48f}, {96, 0.26f}, {128, 0.12f}});
        c.arousal.setDefault({{0, 0.42f}, {16, 0.66f}, {44, 0.74f}, {64, 0.90f}, {88, 0.72f}, {128, 0.34f}});
        c.tension.setDefault({{0, 0.18f}, {24, 0.30f}, {52, 0.48f}, {72, 0.42f}, {96, 0.24f}, {128, 0.14f}});
        c.distance.setDefault({{0, 0.25f}, {24, 0.32f}, {64, 0.30f}, {88, 0.42f}, {128, 0.35f}});
    } else {
        c.valence.setDefault({{0, -0.20f}, {16, 0.02f}, {48, 0.18f}, {64, 0.28f}, {88, 0.02f}, {128, -0.12f}});
        c.arousal.setDefault({{0, 0.18f}, {24, 0.30f}, {52, 0.42f}, {72, 0.50f}, {96, 0.32f}, {128, 0.20f}});
        c.tension.setDefault({{0, 0.26f}, {24, 0.36f}, {52, 0.48f}, {64, 0.55f}, {88, 0.30f}, {128, 0.24f}});
        c.distance.setDefault({{0, 0.22f}, {24, 0.34f}, {52, 0.48f}, {72, 0.62f}, {96, 0.40f}, {128, 0.30f}});
    }
    return c;
}

Intent defaultIntent(const std::string& styleId) {
    Intent intent;
    intent.styleBrief = styleId;
    if (styleId == "cinematic") {
        intent.perspective = Perspective::Observer;
        intent.scene = Scene::OpenSpace;
        intent.restraint = Restraint::OpeningUp;
        intent.culture = "Western orchestral film-score conventions (explicit assumption)";
        intent.references = "gradual additive tension, pedal points, metric asymmetry";
        intent.prohibitions = {"no constant maximum density", "no generic trailer braam"};
        intent.narrative = {
            {0, "texture", "low pedal and dry pulse establish a cold space", true},
            {40, "harmony", "a semitone neighbour refuses to settle", true},
            {72, "register", "high line rises while low pulse continues", true},
            {84, "texture", "drums stop; the harmony is left exposed", true},
            {96, "climax", "full register arrives, then immediately withdraws", true},
            {108, "space", "layers leave one by one; reverb no longer substitutes for material", true},
        };
    } else if (styleId == "kinetic") {
        intent.perspective = Perspective::AbstractAtmosphere;
        intent.scene = Scene::Stage;
        intent.restraint = Restraint::Direct;
        intent.culture = "Afro-diasporic and electronic club-groove conventions (explicit assumption)";
        intent.references = "syncopated bass, four-on-the-floor, additive layers";
        intent.prohibitions = {"no relentless full-layer loudness", "no random swing on every note"};
        intent.narrative = {
            {0, "pulse", "kick and muted pluck introduce the grid", true},
            {16, "bass", "syncopated bass enters on the off-grid cells", true},
            {44, "texture", "pad and counter line widen the image", true},
            {56, "break", "drums and bass drop out; only the motif remains", true},
            {64, "climax", "all layers return, then hats open", true},
            {96, "release", "low end and pad are removed before the final cell", true},
        };
    } else {
        intent.perspective = Perspective::Monologue;
        intent.scene = Scene::IntimateRoom;
        intent.restraint = Restraint::Restrained;
        intent.culture = "Western tonal chamber music (explicit assumption)";
        intent.references = "narrow lyrical motif, inner-voice chromaticism, unresolved close";
        intent.prohibitions = {"no sudden full-orchestra climax", "no constant pedal wash"};
        intent.narrative = {
            {0, "texture", "solo felt piano states a narrow motif", true},
            {16, "harmony", "inner voice moves under the repeated motif", true},
            {32, "register", "cello answers without doubling the melody", true},
            {56, "harmony", "borrowed colour briefly warms the room", true},
            {72, "texture", "texture thins after the peak; the motif is exposed", true},
            {96, "ending", "the final ninth remains unresolved", true},
        };
    }
    intent.narrative.insert(intent.narrative.begin(), {0, "state", "initial affect established by multiple parameters", true});
    return intent;
}

} // namespace ec
