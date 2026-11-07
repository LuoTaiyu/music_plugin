#include "core/Composer.hpp"

#include "core/Motif.hpp"
#include "core/ProjectIO.hpp"
#include "core/Rng.hpp"
#include "core/TimeMap.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ec {
namespace {

using CQ = ChordQuality;

enum LayerBit : unsigned {
    LayerMelody = 1u << 0,
    LayerHarmony = 1u << 1,
    LayerBass = 1u << 2,
    LayerCounter = 1u << 3,
    LayerPercussion = 1u << 4,
    LayerTexture = 1u << 5
};

enum class TrackRecipe { Melody, HarmonyArp, Pad, BassLine, SubBass, CounterLine, Percussion, TexturePad };

struct HarmonyCell {
    int degree = 0;
    int rootOverride = -1;
    CQ quality = CQ::Minor;
    bool ninth = false;
    int inversion = 0;
    double beats = 4.0;
    const char* function = "";
};

struct SectionTemplate {
    const char* name = "";
    const char* role = "development";
    const char* description = "";
    int bars = 8;
    unsigned layers = 0;
    int octaveShift = 0;
    float extraDensity = 0.0f;
    std::vector<HarmonyCell> progression;
};

struct TrackTemplate {
    const char* name = "";
    Role role = Role::Melody;
    Instrument instrument = Instrument::FeltPiano;
    TrackRecipe recipe = TrackRecipe::Melody;
    unsigned layer = LayerMelody;
    float gainDb = 0.0f;
    float pan = 0.0f;
    float reverb = 0.3f;
    int channel = 1;
};

struct StylePlan {
    std::string id;
    std::string title;
    double bpm = 72.0;
    TimeSignature timeSignature;
    Key key;
    StyleBias bias;
    std::vector<SectionTemplate> sections;
    std::vector<TrackTemplate> tracks;
};

StylePlan chamberPlan() {
    StylePlan p;
    p.id = "chamber";
    p.title = "Letters in a Quiet Room";
    p.bpm = 68.0;
    p.timeSignature = {4, 4};
    p.key = {5, Mode::NaturalMinor}; // F minor
    p.bias = {-0.18f, 0.06f, -0.04f, -0.02f, -0.01f, -0.05f, 0.04f, 63, 14, 3.5};

    p.sections = {
        {"Prelude", "setup", "solo piano states a narrow cell; no percussion, no full pad", 8,
         LayerMelody | LayerHarmony | LayerBass, 0, -0.10f,
         {{0, -1, CQ::Minor, true, 0, 8.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 8.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 8.0, "iv7"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 8.0, "V7sus4"}}},

        {"First Memory", "development", "the cell repeats over a moving inner voice", 8,
         LayerMelody | LayerHarmony | LayerBass, 0, 0.0f,
         {{0, -1, CQ::Minor, true, 0, 4.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 1, 4.0, "VI/3"},
          {3, -1, CQ::Minor7, false, 0, 4.0, "iv7"},
          {6, -1, CQ::Major, false, 1, 4.0, "VII/3"},
          {2, -1, CQ::Major7, false, 0, 4.0, "III(maj7)"},
          {5, -1, CQ::Major7, false, 0, 4.0, "VI(maj7)"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 4.0, "V7sus4"},
          {0, -1, CQ::Minor, true, 0, 4.0, "i(add9)"}}},

        {"Inner Motion", "development", "same contour, different bass and inversion; viola enters", 8,
         LayerMelody | LayerHarmony | LayerBass | LayerTexture, 0, 0.04f,
         {{0, -1, CQ::Minor, false, 0, 4.0, "i"},
          {5, -1, CQ::Major7, false, 0, 4.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 1, 4.0, "iv7/3"},
          {0, -1, CQ::Minor, true, 1, 4.0, "i(add9)/3"},
          {2, -1, CQ::Major7, false, 1, 4.0, "III/3"},
          {5, -1, CQ::Major7, false, 0, 4.0, "VI(maj7)"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 4.0, "V7sus4"},
          {0, -1, CQ::Minor, false, 0, 4.0, "i"}}},

        {"Distant Warmth", "turn", "borrowed colour opens briefly; no full climax", 8,
         LayerMelody | LayerHarmony | LayerBass | LayerTexture, 1, 0.02f,
         {{5, -1, CQ::Major7, false, 0, 8.0, "VI(maj7)"},
          {2, -1, CQ::Major, false, 1, 8.0, "III/3"},
          {6, 6, CQ::Major7, false, 0, 8.0, "bII(maj7)"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 8.0, "V7sus4"}}},

        {"Return", "development", "the opening cell returns with a changed ending", 8,
         LayerMelody | LayerHarmony | LayerBass | LayerTexture, 0, 0.0f,
         {{0, -1, CQ::Minor, true, 0, 4.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 4.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 4.0, "iv7"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 4.0, "V7sus4"},
          {0, -1, CQ::Minor, false, 0, 4.0, "i"},
          {5, -1, CQ::Major7, false, 0, 4.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 4.0, "iv7"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 4.0, "V7sus4"}}},

        {"Coda", "coda", "texture withdraws; the last ninth is left unresolved", 8,
         LayerMelody | LayerHarmony | LayerBass, 0, -0.12f,
         {{0, -1, CQ::Minor, true, 0, 8.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 8.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 8.0, "iv7"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 8.0, "V7sus4"}}},
    };

    p.tracks = {
        {"Felt Piano — melody", Role::Melody, Instrument::FeltPiano, TrackRecipe::Melody, LayerMelody, -1.5f, -0.06f, 0.34f, 1},
        {"Piano — inner voice", Role::Harmony, Instrument::FeltPiano, TrackRecipe::HarmonyArp, LayerHarmony, -9.0f, 0.16f, 0.32f, 2},
        {"Cello", Role::Bass, Instrument::BowedString, TrackRecipe::BassLine, LayerBass, -4.0f, -0.22f, 0.44f, 3},
        {"Viola", Role::Texture, Instrument::BowedString, TrackRecipe::TexturePad, LayerTexture, -12.0f, 0.28f, 0.55f, 4},
    };
    return p;
}

StylePlan cinematicPlan() {
    StylePlan p;
    p.id = "cinematic";
    p.title = "Pressure Line";
    p.bpm = 92.0;
    p.timeSignature = {5, 4};
    p.key = {2, Mode::Phrygian}; // D phrygian
    p.bias = {0.08f, -0.03f, 0.03f, 0.04f, 0.16f, -0.02f, 0.14f, 54, 22, 4.0};

    p.sections = {
        {"Cold Open", "setup", "low pedal, dry clock, no melody", 6,
         LayerHarmony | LayerBass, 0, -0.06f,
         {{0, -1, CQ::Minor, false, 0, 8.0, "i"},
          {0, -1, CQ::Minor, false, 0, 7.0, "i"},
          {1, 3, CQ::Major, false, 0, 7.0, "bII"},
          {0, -1, CQ::Minor, false, 0, 8.0, "i"}}},

        {"Pressure", "development", "pulse and timpani harden the 3+2 grouping", 8,
         LayerHarmony | LayerBass | LayerPercussion, 0, 0.03f,
         {{0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {1, 3, CQ::Major, false, 0, 5.0, "bII"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {5, -1, CQ::Major, false, 0, 5.0, "VI"},
          {3, -1, CQ::Minor, false, 0, 5.0, "iv"},
          {4, -1, CQ::Major, false, 0, 5.0, "V"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {1, 3, CQ::Major, false, 0, 5.0, "bII"}}},

        {"Accumulation", "development", "high line and low ostinato accumulate without full density", 10,
         LayerMelody | LayerHarmony | LayerBass | LayerPercussion, 0, 0.06f,
         {{0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {1, 3, CQ::Major, false, 0, 5.0, "bII"},
          {3, -1, CQ::Minor, false, 0, 5.0, "iv"},
          {5, -1, CQ::Major, false, 0, 5.0, "VI"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {4, -1, CQ::Major, false, 0, 5.0, "V"},
          {1, 3, CQ::Major, false, 0, 5.0, "bII"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {3, -1, CQ::Minor, false, 0, 5.0, "iv"},
          {4, -1, CQ::Major, false, 0, 5.0, "V"}}},

        {"Suspension", "turn", "drums disappear; the tritone and high pedal are exposed", 4,
         LayerMelody | LayerHarmony | LayerBass, 0, -0.05f,
         {{5, -1, CQ::Major7, false, 0, 5.0, "VI(maj7)"},
          {1, 3, CQ::Major7, false, 0, 5.0, "bII(maj7)"},
          {0, -1, CQ::Minor, true, 0, 5.0, "i(add9)"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 5.0, "V7sus4"}}},

        {"Climax", "climax", "full register arrives once, then withdraws immediately", 8,
         LayerMelody | LayerHarmony | LayerBass | LayerPercussion | LayerCounter, 1, 0.12f,
         {{0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {1, 3, CQ::Major, false, 0, 5.0, "bII"},
          {5, -1, CQ::Major, false, 0, 5.0, "VI"},
          {3, -1, CQ::Minor, false, 0, 5.0, "iv"},
          {4, -1, CQ::Major, false, 0, 5.0, "V"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {1, 3, CQ::Major, false, 0, 5.0, "bII"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"}}},

        {"Aftermath", "release", "one line remains; distance is created by removal, not more reverb", 8,
         LayerMelody | LayerHarmony | LayerBass, 0, -0.18f,
         {{0, -1, CQ::Minor, true, 0, 5.0, "i(add9)"},
          {5, -1, CQ::Major, false, 0, 5.0, "VI"},
          {3, -1, CQ::Minor, false, 0, 5.0, "iv"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {1, 3, CQ::Major7, false, 0, 5.0, "bII(maj7)"},
          {0, -1, CQ::Minor, false, 0, 5.0, "i"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 5.0, "V7sus4"},
          {0, -1, CQ::Minor, true, 0, 5.0, "i(add9)"}}},
    };

    p.tracks = {
        {"High Strings", Role::Melody, Instrument::BowedString, TrackRecipe::Melody, LayerMelody, -3.0f, 0.08f, 0.42f, 1},
        {"Low Strings", Role::Bass, Instrument::BowedString, TrackRecipe::BassLine, LayerBass, -2.0f, -0.15f, 0.32f, 2},
        {"Glass Pulse", Role::Harmony, Instrument::GlassMallet, TrackRecipe::HarmonyArp, LayerHarmony, -12.0f, 0.28f, 0.55f, 3},
        {"Timpani / Percussion", Role::Percussion, Instrument::DrumKit, TrackRecipe::Percussion, LayerPercussion, -7.0f, 0.0f, 0.40f, 10},
        {"Sub Pedal", Role::Bass, Instrument::OrganicBass, TrackRecipe::SubBass, LayerBass, -10.0f, 0.0f, 0.15f, 4},
        {"Counter / Low Brass", Role::Counter, Instrument::BowedString, TrackRecipe::CounterLine, LayerCounter, -9.0f, -0.25f, 0.38f, 5},
    };
    return p;
}

StylePlan kineticPlan() {
    StylePlan p;
    p.id = "kinetic";
    p.title = "Kinetic Bloom";
    p.bpm = 122.0;
    p.timeSignature = {4, 4};
    p.key = {4, Mode::Dorian}; // E dorian
    p.bias = {0.16f, -0.05f, 0.01f, 0.16f, 0.04f, 0.06f, 0.0f, 64, 19, 1.5};

    p.sections = {
        {"Intro", "setup", "kick and muted pluck establish the grid", 4,
         LayerHarmony | LayerPercussion, 0, -0.08f,
         {{0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {4, -1, CQ::Dominant7, true, 0, 2.0, "V9"},
          {4, -1, CQ::Dominant7, true, 0, 2.0, "V9"}}},

        {"Groove A", "development", "syncopated bass enters; the hook is repeated with a changed ending", 8,
         LayerMelody | LayerHarmony | LayerBass | LayerCounter | LayerPercussion, 0, 0.06f,
         {{0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {4, -1, CQ::Dominant7, true, 0, 2.0, "V9"},
          {0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 1, 2.0, "iv7/3"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 2.0, "V7sus4"}}},

        {"Break", "turn", "drums and bass drop; only the cell and pad remain", 4,
         LayerMelody | LayerHarmony | LayerCounter, 0, -0.12f,
         {{0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 2.0, "V7sus4"}}},

        {"Groove B", "development", "bass displaces the hook; pad opens the image", 8,
         LayerMelody | LayerHarmony | LayerBass | LayerCounter | LayerPercussion, 0, 0.05f,
         {{5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {4, -1, CQ::Dominant7, true, 0, 2.0, "V9"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 2.0, "V7sus4"},
          {0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"}}},

        {"Peak", "climax", "all layers return; open hats and counter line lift the last pass", 8,
         LayerMelody | LayerHarmony | LayerBass | LayerCounter | LayerPercussion, 1, 0.12f,
         {{0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {4, -1, CQ::Dominant7, true, 0, 2.0, "V9"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {4, -1, CQ::Dominant7, true, 0, 2.0, "V9"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"}}},

        {"Outro", "coda", "low end and pad leave before the final cell; no fake fade", 4,
         LayerMelody | LayerCounter, 0, -0.16f,
         {{0, -1, CQ::Minor, true, 0, 2.0, "i(add9)"},
          {5, -1, CQ::Major7, false, 0, 2.0, "VI(maj7)"},
          {3, -1, CQ::Minor7, false, 0, 2.0, "iv7"},
          {4, -1, CQ::Dominant7Sus4, false, 0, 2.0, "V7sus4"}}},
    };

    p.tracks = {
        {"Pluck Hook", Role::Melody, Instrument::GlassMallet, TrackRecipe::Melody, LayerMelody, -4.0f, 0.10f, 0.24f, 1},
        {"Sub Bass", Role::Bass, Instrument::OrganicBass, TrackRecipe::BassLine, LayerBass, -3.0f, 0.0f, 0.08f, 2},
        {"Pad", Role::Harmony, Instrument::BowedString, TrackRecipe::Pad, LayerHarmony, -14.0f, 0.0f, 0.48f, 3},
        {"Drums", Role::Percussion, Instrument::DrumKit, TrackRecipe::Percussion, LayerPercussion, -5.0f, 0.0f, 0.22f, 10},
        {"Off-grid Counter", Role::Counter, Instrument::GlassMallet, TrackRecipe::CounterLine, LayerCounter, -12.0f, -0.30f, 0.38f, 4},
    };
    return p;
}

StylePlan planFor(DemoStyle style) {
    switch (style) {
        case DemoStyle::Chamber: return chamberPlan();
        case DemoStyle::Cinematic: return cinematicPlan();
        case DemoStyle::Kinetic: return kineticPlan();
    }
    return chamberPlan();
}

std::string_view styleId(DemoStyle style) {
    switch (style) {
        case DemoStyle::Chamber: return "chamber";
        case DemoStyle::Cinematic: return "cinematic";
        case DemoStyle::Kinetic: return "kinetic";
    }
    return "chamber";
}

DemoStyle styleFromId(std::string_view id) {
    if (id == "cinematic") return DemoStyle::Cinematic;
    if (id == "kinetic") return DemoStyle::Kinetic;
    return DemoStyle::Chamber;
}

int ticksPerBarFor(const Project& p) {
    return static_cast<int>(ticksPerBar(p.timeSignature, p.ppq));
}

int beatsPerBar(const TimeSignature& ts) {
    return std::max(1, ts.numerator * 4 / std::max(1, ts.denominator));
}

int octaveShiftFor(const std::string& style, int sectionIndex) {
    if (style == "cinematic") {
        if (sectionIndex == 4) return 1; // climax
        return 0;
    }
    if (style == "kinetic") {
        if (sectionIndex == 4) return 1; // peak
        return 0;
    }
    if (sectionIndex == 3) return 1; // distant warmth
    return 0;
}

const ChordEvent* chordAt(const Project& p, Tick tick) {
    const ChordEvent* best = nullptr;
    for (const auto& c : p.chords) {
        if (c.start <= tick && tick < c.start + c.duration) {
            if (!best || c.start > best->start) best = &c;
        }
    }
    return best;
}

std::vector<int> pcsFromRoot(int root, CQ quality, bool ninth, bool eleventh = false) {
    std::vector<int> pcs;
    for (int interval : chordIntervals(quality)) pcs.push_back((root + interval) % 12);
    if (ninth) pcs.push_back((root + 14) % 12);
    if (eleventh) pcs.push_back((root + 17) % 12);
    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
    return pcs;
}

std::string romanFor(const HarmonyCell& cell, const Key& key) {
    if (cell.function && *cell.function) return cell.function;
    return romanNumeral(key, cell.degree, cell.quality, cell.inversion);
}

Note makeNote(Project& p,
              int start,
              int duration,
              int pitch,
              int velocity,
              int channel,
              float brightness,
              float attack,
              float noise,
              float release,
              float legato) {
    Note n;
    n.id = p.allocateNoteId();
    n.start = std::max<Tick>(0, start);
    n.duration = std::max<Tick>(1, duration);
    n.pitch = std::clamp(pitch, 0, 127);
    n.velocity = std::clamp(velocity, 1, 127);
    n.channel = std::clamp(channel, 1, 16);
    n.brightness = std::clamp(brightness, 0.0f, 1.0f);
    n.attack = std::clamp(attack, 0.0f, 1.0f);
    n.noise = std::clamp(noise, 0.0f, 1.0f);
    n.release = std::clamp(release, 0.0f, 1.0f);
    n.legato = std::clamp(legato, 0.0f, 1.0f);
    return n;
}

struct NoteSeed {
    int start = 0;
    int duration = 0;
    int pitch = 60;
    int velocity = 90;
    float accent = 1.0f;
};

void buildChords(Project& p, const StylePlan& plan, GenerationDiagnostics* diagnostics) {
    const Scale scale = makeScale(p.key);
    const int bar = ticksPerBarFor(p);
    const int beatTicks = p.ppq;
    std::vector<int> previous;
    bool preferFlats = p.key.mode != Mode::Lydian && p.key.mode != Mode::Major;
    const int low = plan.id == "cinematic" ? 36 : (plan.id == "kinetic" ? 40 : 48);
    const int high = plan.id == "cinematic" ? 74 : (plan.id == "kinetic" ? 79 : 76);
    const int voices = plan.id == "kinetic" ? 3 : 4;

    for (std::size_t si = 0; si < p.sections.size() && si < plan.sections.size(); ++si) {
        const auto& sec = p.sections[si];
        const auto& tpl = plan.sections[si];
        Tick start = static_cast<Tick>(sec.startBar) * bar;
        const Tick end = static_cast<Tick>(sec.startBar + sec.bars) * bar;
        std::size_t cellIndex = 0;
        while (start < end) {
            const auto& cell = tpl.progression[cellIndex % tpl.progression.size()];
            ++cellIndex;
            const int root = cell.rootOverride >= 0 ? ((cell.rootOverride % 12 + 12) % 12)
                                                    : scale.pitchClassAt(cell.degree);
            const auto pcs = pcsFromRoot(root, cell.quality, cell.ninth);
            const auto intervals = chordIntervals(cell.quality);
            int bassPc = root;
            if (!intervals.empty() && cell.inversion != 0) {
                const int idx = ((cell.inversion % static_cast<int>(intervals.size())) +
                                 static_cast<int>(intervals.size())) % static_cast<int>(intervals.size());
                bassPc = (root + intervals[static_cast<std::size_t>(idx)]) % 12;
            }
            const int center = plan.bias.registerCenter + (plan.id == "cinematic" ? 2 : 0);
            std::vector<int> voicing = voiceChord(previous, pcs, bassPc, low, high, voices, center);
            const Tick duration = std::min<Tick>(static_cast<Tick>(cell.beats * beatTicks), end - start);
            ChordEvent ce;
            ce.start = start;
            ce.duration = std::max<Tick>(1, duration);
            ce.rootPc = root;
            ce.symbol = chordSymbol(p.key, root, cell.quality, cell.ninth, cell.inversion, preferFlats);
            ce.pitches = voicing;
            ce.tension = chordTension(voicing, root);
            ce.inversion = cell.inversion;
            ce.function = romanFor(cell, p.key);
            p.chords.push_back(std::move(ce));
            if (!voicing.empty()) previous = voicing;
            start += duration;
        }
        if (diagnostics) {
            diagnostics->decisions.push_back("section '" + sec.name + "' uses " +
                                             std::to_string(tpl.progression.size()) + " harmony cells, layers=" +
                                             std::to_string(tpl.layers));
        }
    }
}

void generateMelody(Project& p,
                    int trackIndex,
                    const StylePlan& plan,
                    const Motif& motif,
                    const std::vector<TrackTemplate>& trackTemplates,
                    const std::vector<int>& recipeForTrack,
                    GenerationDiagnostics* diagnostics) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(p.tracks.size())) return;
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& tt = trackTemplates[static_cast<std::size_t>(trackIndex)];
    const int bar = ticksPerBarFor(p);
    const int beats = beatsPerBar(p.timeSignature);
    const int phraseBars = 2;
    const Scale scale = makeScale(p.key);
    const int tonicMidi = plan.id == "cinematic" ? 62 : (plan.id == "kinetic" ? 64 : 65);
    Rng rng(mixSeed(p.seed, static_cast<std::uint64_t>(track.id) * 0x9E3779B1u + 7));
    const std::vector<MotifTransform> chamberTransforms{
        MotifTransform::Original, MotifTransform::ChangeEnding, MotifTransform::Ornament,
        MotifTransform::Transpose, MotifTransform::Invert, MotifTransform::Fragment,
        MotifTransform::ChangeEnding, MotifTransform::Extend};
    const std::vector<MotifTransform> cinematicTransforms{
        MotifTransform::Original, MotifTransform::Transpose, MotifTransform::Fragment,
        MotifTransform::Augment, MotifTransform::Invert, MotifTransform::Ornament,
        MotifTransform::Original, MotifTransform::Extend};
    const std::vector<MotifTransform> kineticTransforms{
        MotifTransform::Original, MotifTransform::Retrograde, MotifTransform::Fragment,
        MotifTransform::Transpose, MotifTransform::Ornament, MotifTransform::ChangeEnding,
        MotifTransform::Diminish, MotifTransform::Original};
    const auto& transforms = plan.id == "cinematic" ? cinematicTransforms
                            : (plan.id == "kinetic" ? kineticTransforms : chamberTransforms);

    int phraseOrdinal = 0;
    for (std::size_t si = 0; si < p.sections.size() && si < plan.sections.size(); ++si) {
        const auto& sec = p.sections[si];
        const auto& st = plan.sections[si];
        if ((st.layers & LayerMelody) == 0) continue;
        const int phraseCount = std::max(1, sec.bars / phraseBars);
        for (int pi = 0; pi < phraseCount; ++pi) {
            const int startBar = sec.startBar + pi * phraseBars;
            const int bars = std::min(phraseBars, sec.startBar + sec.bars - startBar);
            if (bars <= 0) break;
            const Tick phraseStart = static_cast<Tick>(startBar) * bar;
            const Tick phraseEnd = phraseStart + static_cast<Tick>(bars) * bar;
            const double centerBeat = (static_cast<double>(startBar) + bars * 0.5) * beats;
            const auto state = p.curves.at(centerBeat);
            const auto aff = ExpressionMapper::derive(state, p.intent.restraint, Role::Melody, plan.bias);
            const int octaveShift = octaveShiftFor(plan.id, static_cast<int>(si));
            const int low = aff.registerCenter - aff.registerSpan / 2;
            const int high = aff.registerCenter + aff.registerSpan / 2;

            // Phrase function: antecedent asks, consequent answers. The answer is
            // deliberately not forced to a triumphant tonic.
            const bool consequent = (phraseOrdinal % 2) == 1;
            const bool unresolved = aff.unresolvedEnding && (si + 1 == p.sections.size());
            MotifTransform baseTransform = transforms[static_cast<std::size_t>(
                (phraseOrdinal + static_cast<int>(rng.nextU32())) % transforms.size())];
            if (phraseOrdinal % 4 == 0) baseTransform = MotifTransform::Original;

            std::vector<Note> phraseNotes;
            int instanceId = 0;
            int repeat = 0;
            std::uint32_t phrasePeakId = 0;
            int peakPitch = -1;
            Tick cursor = phraseStart + (plan.id == "kinetic" ? p.ppq / 4 : p.ppq / 2);
            while (cursor < phraseEnd - p.ppq / 2 && repeat < 5) {
                Motif local = transformMotif(motif, baseTransform, (static_cast<int>(si) % 3) - 1);
                if (repeat > 0) {
                    local = transformMotif(local, repeat % 2 == 0 ? MotifTransform::ChangeEnding
                                                                  : MotifTransform::Fragment,
                                           repeat % 3 - 1);
                }
                local = transformMotif(local, MotifTransform::Transpose, octaveShift * 7);
                instanceId = static_cast<int>(p.nextMotifInstance++);
                const Tick useStart = cursor;

                for (std::size_t ni = 0; ni < local.notes.size(); ++ni) {
                    const auto& mn = local.notes[static_cast<std::size_t>(ni)];
                    Tick d = std::max<Tick>(p.ppq / 8, mn.duration);
                    if (cursor + d > phraseEnd - p.ppq / 4) {
                        d = std::max<Tick>(p.ppq / 8, phraseEnd - p.ppq / 4 - cursor);
                    }
                    if (d <= p.ppq / 16) break;
                    int pitch = scale.midiAt(mn.degree, tonicMidi + octaveShift * 12);
                    while (pitch < low - 5) pitch += 12;
                    while (pitch > high + 5) pitch -= 12;

                    const auto* chord = chordAt(p, cursor);
                    const bool strong = (cursor % p.ppq) == 0;
                    if (chord && !chord->pitches.empty()) {
                        if (strong) {
                            pitch = nearestPitchInSet(pitch, chord->pitches, std::max(0, low - 12), std::min(127, high + 12));
                        } else if (!isChordTone(pitch, chord->pitches) && aff.dissonanceBudget > 0.25f) {
                            const int neighbor = ((pitch % 12) + 12) % 12;
                            if (neighbor == ((chord->rootPc + 11) % 12) || neighbor == ((chord->rootPc + 1) % 12)) {
                                pitch = nearestPitchInSet(pitch, chord->pitches, std::max(0, low - 12),
                                                          std::min(127, high + 12));
                            }
                        }
                    }
                    if (!phraseNotes.empty() && phraseNotes.back().pitch == pitch && ni + 1 < local.notes.size()) {
                        pitch = nearestPitchInSet(pitch + (ni % 2 ? 2 : -2),
                                                  chord ? chord->pitches : std::vector<int>{},
                                                  std::max(0, low - 12), std::min(127, high + 12));
                    }
                    const float contour = static_cast<float>(ni) / static_cast<float>(std::max<std::size_t>(1, local.notes.size() - 1));
                    const int velocity = std::clamp(static_cast<int>(std::lround(127.0f * (aff.velocityBase +
                        aff.velocityRange * (contour - 0.4f) + 0.10f * mn.accent))), 12, 122);
                    Note n = makeNote(p, static_cast<int>(cursor), static_cast<int>(d), pitch, velocity, track.midiChannel,
                                      aff.brightness, 0.55f - 0.20f * aff.articulation, aff.noise, aff.release,
                                      aff.articulation);
                    n.motifInstance = static_cast<std::uint32_t>(instanceId);
                    n.phrase = static_cast<int>(p.phrases.size());
                    n.pan = tt.pan + 0.04f * std::sin(static_cast<float>(cursor) / static_cast<float>(bar));
                    phraseNotes.push_back(n);
                    if (pitch > peakPitch) { peakPitch = pitch; phrasePeakId = n.id; }
                    cursor += d + (aff.density > 0.6f ? 0 : p.ppq / 16);
                }
                p.motifUses.push_back({motif.id, static_cast<std::uint32_t>(instanceId), useStart, cursor,
                                       track.id, std::string(toString(baseTransform))});
                cursor += p.ppq / 2;
                ++repeat;
            }

            // A clear cadence tone gives the phrase a direction. At the end of the
            // album it remains unresolved rather than pretending closure.
            if (!phraseNotes.empty() && cursor < phraseEnd) {
                const auto* chord = chordAt(p, phraseEnd - p.ppq / 2);
                const int cadenceDegree = consequent ? (unresolved ? 2 : 0) : 2;
                int pitch = scale.midiAt(cadenceDegree, tonicMidi + octaveShift * 12);
                if (chord && !chord->pitches.empty()) {
                    pitch = nearestPitchInSet(pitch, chord->pitches, std::max(0, low - 12), std::min(127, high + 12));
                }
                const Tick start = std::max<Tick>(phraseEnd - p.ppq * 5 / 4, phraseNotes.back().start + phraseNotes.back().duration);
                if (start < phraseEnd - p.ppq / 8) {
                    Note n = makeNote(p, static_cast<int>(start), static_cast<int>(phraseEnd - start - p.ppq / 8),
                                      pitch, static_cast<int>(std::lround(127.0f * aff.velocityBase * 0.92f)),
                                      track.midiChannel, aff.brightness, 0.52f - 0.2f * aff.articulation,
                                      aff.noise, aff.release, aff.articulation);
                    n.phrase = static_cast<int>(p.phrases.size());
                    n.motifInstance = static_cast<std::uint32_t>(instanceId);
                    phraseNotes.push_back(n);
                    if (pitch > peakPitch) { peakPitch = pitch; phrasePeakId = n.id; }
                }
            }

            for (auto& n : phraseNotes) track.notes.push_back(n);
            Phrase ph;
            ph.section = static_cast<int>(si);
            ph.start = phraseStart;
            ph.end = phraseEnd;
            ph.function = consequent ? "consequent" : "antecedent";
            ph.peakNoteId = phrasePeakId;
            ph.intensity = 0.25f + 0.55f * aff.density + 0.2f * state.tension;
            ph.breathAtEnd = true;
            p.phrases.push_back(ph);
            if (diagnostics) {
                diagnostics->decisions.push_back("phrase " + std::to_string(phraseOrdinal) + " (" +
                    (consequent ? "consequent" : "antecedent") + ") uses motifs " +
                    std::to_string(repeat) + "x, transform=" + std::string(toString(baseTransform)) +
                    ", unresolved=" + (unresolved ? "yes" : "no"));
            }
            ++phraseOrdinal;
        }
    }
    (void)recipeForTrack;
}

void generateBass(Project& p, int trackIndex, const StylePlan& plan, const std::vector<TrackTemplate>& tt, int si) {
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& sec = p.sections[static_cast<std::size_t>(si)];
    const auto& st = plan.sections[static_cast<std::size_t>(si)];
    if ((st.layers & LayerBass) == 0) return;
    const int beatTicks = p.ppq;
    const int bar = ticksPerBarFor(p);
    const auto state = p.curves.at((sec.startBar + sec.bars * 0.5) * beatsPerBar(p.timeSignature));
    const auto aff = ExpressionMapper::derive(state, p.intent.restraint, Role::Bass, plan.bias);
    const int low = plan.id == "cinematic" ? 26 : (plan.id == "kinetic" ? 28 : 36);
    const int high = plan.id == "cinematic" ? 45 : (plan.id == "kinetic" ? 43 : 52);
    for (const auto& chord : p.chords) {
        if (chord.start < static_cast<Tick>(sec.startBar) * bar ||
            chord.start >= static_cast<Tick>(sec.startBar + sec.bars) * bar) continue;
        const int root = nearestPitchWithClass((low + high) / 2, chord.rootPc, low, high);
        const int fifth = nearestPitchWithClass(root + 7, (chord.rootPc + 7) % 12, low, high);
        if (plan.id == "kinetic") {
            static const double offsets[] = {0.0, 0.75, 1.5, 2.25, 3.0, 3.5};
            int k = 0;
            for (double off : offsets) {
                const Tick s = chord.start + static_cast<Tick>(off * beatTicks);
                if (s >= chord.start + chord.duration) break;
                const int pitch = (k % 4 == 1) ? fifth : (k % 4 == 2 ? root + 12 : root);
                const int vel = std::clamp(static_cast<int>(std::lround(127.0f * (aff.velocityBase + (k == 0 ? 0.14f : -0.02f)))), 20, 120);
                auto n = makeNote(p, static_cast<int>(s), static_cast<int>(std::min<Tick>(beatTicks / 3, chord.duration - (s - chord.start))),
                                  pitch, vel, track.midiChannel, aff.brightness, 0.3f, aff.noise, 0.3f, 0.3f);
                n.pan = tt[static_cast<std::size_t>(trackIndex)].pan;
                track.notes.push_back(n);
                ++k;
            }
        } else if (plan.id == "cinematic") {
            const int meters = p.timeSignature.numerator;
            for (int b = 0; b < meters; ++b) {
                const Tick s = chord.start + static_cast<Tick>(b) * beatTicks;
                if (s >= chord.start + chord.duration) break;
                const int pitch = (b % 5 == 3) ? fifth : (b % 5 == 4 ? root + 12 : root);
                const bool accent = (b == 0 || b == 3);
                const int vel = std::clamp(static_cast<int>(std::lround(127.0f * (aff.velocityBase + (accent ? 0.12f : -0.08f)))), 20, 122);
                auto n = makeNote(p, static_cast<int>(s), static_cast<int>(std::min<Tick>(beatTicks * 3 / 4, chord.duration)),
                                  pitch, vel, track.midiChannel, aff.brightness, 0.25f, aff.noise, 0.35f, 0.35f);
                n.pan = tt[static_cast<std::size_t>(trackIndex)].pan;
                track.notes.push_back(n);
            }
        } else {
            const int vel = std::clamp(static_cast<int>(std::lround(127.0f * (aff.velocityBase * 0.82f))), 18, 104);
            auto n = makeNote(p, static_cast<int>(chord.start), static_cast<int>(chord.duration * 9 / 10),
                              root, vel, track.midiChannel, aff.brightness, 0.4f, aff.noise, 0.6f, 0.55f);
            n.pan = tt[static_cast<std::size_t>(trackIndex)].pan;
            track.notes.push_back(n);
            if (chord.duration >= beatTicks * 3) {
                auto n2 = makeNote(p, static_cast<int>(chord.start + beatTicks * 2),
                                   static_cast<int>(std::max<Tick>(beatTicks / 2, chord.duration / 2)),
                                   fifth, static_cast<int>(vel * 0.72f), track.midiChannel,
                                   aff.brightness * 0.9f, 0.45f, aff.noise, 0.5f, 0.5f);
                n2.pan = tt[static_cast<std::size_t>(trackIndex)].pan;
                track.notes.push_back(n2);
            }
        }
    }
}

void generateSub(Project& p, int trackIndex, const StylePlan& plan, int si) {
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& sec = p.sections[static_cast<std::size_t>(si)];
    const auto& st = plan.sections[static_cast<std::size_t>(si)];
    if ((st.layers & LayerBass) == 0) return;
    const int bar = ticksPerBarFor(p);
    for (const auto& chord : p.chords) {
        if (chord.start < static_cast<Tick>(sec.startBar) * bar ||
            chord.start >= static_cast<Tick>(sec.startBar + sec.bars) * bar) continue;
        const int pitch = nearestPitchWithClass(31, chord.rootPc, 24, 38);
        auto n = makeNote(p, static_cast<int>(chord.start), static_cast<int>(chord.duration * 95 / 100),
                          pitch, 48, track.midiChannel, 0.25f, 0.7f, 0.15f, 0.7f, 0.85f);
        n.pan = 0.0f;
        track.notes.push_back(n);
    }
}

void generateHarmonyArp(Project& p, int trackIndex, const StylePlan& plan, int si) {
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& sec = p.sections[static_cast<std::size_t>(si)];
    const auto& st = plan.sections[static_cast<std::size_t>(si)];
    if ((st.layers & LayerHarmony) == 0) return;
    const int bar = ticksPerBarFor(p);
    const auto state = p.curves.at((sec.startBar + sec.bars * 0.5) * beatsPerBar(p.timeSignature));
    const auto aff = ExpressionMapper::derive(state, p.intent.restraint, Role::Harmony, plan.bias);
    for (const auto& chord : p.chords) {
        if (chord.start < static_cast<Tick>(sec.startBar) * bar ||
            chord.start >= static_cast<Tick>(sec.startBar + sec.bars) * bar || chord.pitches.empty()) continue;
        if (plan.id == "cinematic") {
            const int meter = p.timeSignature.numerator;
            for (int b = 0; b < meter; ++b) {
                if (b == 2) continue; // deliberate hole in the pulse
                const Tick s = chord.start + static_cast<Tick>(b) * p.ppq;
                if (s >= chord.start + chord.duration) break;
                const int pitch = chord.pitches[static_cast<std::size_t>(1 + (b % 2) % chord.pitches.size())] + 12;
                auto n = makeNote(p, static_cast<int>(s), p.ppq / 3, pitch,
                                  static_cast<int>(std::lround(127.0f * (0.34f + 0.1f * (b == 0)))),
                                  track.midiChannel, aff.brightness * 0.9f, 0.2f, 0.55f, 0.35f, 0.2f);
                track.notes.push_back(n);
            }
        } else {
            static const int pattern[] = {1, 2, 3, 2, 1, 2, 3, 2};
            const int steps = static_cast<int>(chord.duration / (p.ppq / 2));
            for (int step = 0; step < steps; ++step) {
                const Tick s = chord.start + static_cast<Tick>(step) * (p.ppq / 2);
                if (s >= chord.start + chord.duration) break;
                const int index = pattern[step % 8] % static_cast<int>(chord.pitches.size());
                const int pitch = chord.pitches[static_cast<std::size_t>(index)] +
                                  (step >= 4 ? 12 : 0);
                auto n = makeNote(p, static_cast<int>(s), p.ppq / 2 - 20, pitch,
                                  static_cast<int>(std::lround(127.0f * (aff.velocityBase * 0.62f +
                                      (step == 0 ? 0.05f : 0.0f)))),
                                  track.midiChannel, aff.brightness * 0.86f, 0.35f, aff.noise, 0.55f, 0.45f);
                track.notes.push_back(n);
            }
        }
    }
}

void generatePad(Project& p, int trackIndex, const StylePlan& plan, int si) {
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& sec = p.sections[static_cast<std::size_t>(si)];
    const auto& st = plan.sections[static_cast<std::size_t>(si)];
    if ((st.layers & LayerHarmony) == 0) return;
    const int bar = ticksPerBarFor(p);
    const auto state = p.curves.at((sec.startBar + sec.bars * 0.5) * beatsPerBar(p.timeSignature));
    const auto aff = ExpressionMapper::derive(state, p.intent.restraint, Role::Harmony, plan.bias);
    for (const auto& chord : p.chords) {
        if (chord.start < static_cast<Tick>(sec.startBar) * bar ||
            chord.start >= static_cast<Tick>(sec.startBar + sec.bars) * bar) continue;
        const int half = static_cast<int>(chord.duration / 2);
        for (int part = 0; part < 2; ++part) {
            const Tick s = chord.start + part * half;
            const Tick d = std::max<Tick>(p.ppq, part == 0 ? half : chord.duration - half);
            for (std::size_t i = 1; i < chord.pitches.size(); ++i) {
                auto n = makeNote(p, static_cast<int>(s), static_cast<int>(d * 98 / 100),
                                  chord.pitches[i] + 12, static_cast<int>(std::lround(127.0f * (0.22f + 0.04f * i))),
                                  track.midiChannel, aff.brightness * 0.8f, 0.15f, 0.2f, 0.85f, 0.95f);
                n.pan = (i % 2 ? 0.18f : -0.18f);
                track.notes.push_back(n);
            }
        }
    }
}

void generateTexture(Project& p, int trackIndex, const StylePlan& plan, int si) {
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& sec = p.sections[static_cast<std::size_t>(si)];
    const auto& st = plan.sections[static_cast<std::size_t>(si)];
    if ((st.layers & LayerTexture) == 0) return;
    const int bar = ticksPerBarFor(p);
    const auto state = p.curves.at((sec.startBar + sec.bars * 0.5) * beatsPerBar(p.timeSignature));
    const auto aff = ExpressionMapper::derive(state, p.intent.restraint, Role::Texture, plan.bias);
    for (const auto& chord : p.chords) {
        if (chord.start < static_cast<Tick>(sec.startBar) * bar ||
            chord.start >= static_cast<Tick>(sec.startBar + sec.bars) * bar || chord.pitches.empty()) continue;
        const int pitch = chord.pitches[static_cast<std::size_t>(std::max<std::size_t>(1, chord.pitches.size() - 1))];
        auto n = makeNote(p, static_cast<int>(chord.start), static_cast<int>(chord.duration * 94 / 100),
                          pitch + 12, static_cast<int>(std::lround(127.0f * 0.26f)), track.midiChannel,
                          aff.brightness * 0.72f, 0.1f, 0.18f, 0.9f, 0.95f);
        n.pan = 0.0f;
        track.notes.push_back(n);
    }
}

void generateCounter(Project& p, int trackIndex, const StylePlan& plan, const Motif& motif, int si) {
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& sec = p.sections[static_cast<std::size_t>(si)];
    const auto& st = plan.sections[static_cast<std::size_t>(si)];
    if ((st.layers & LayerCounter) == 0) return;
    const int bar = ticksPerBarFor(p);
    const Scale scale = makeScale(p.key);
    const auto state = p.curves.at((sec.startBar + sec.bars * 0.5) * beatsPerBar(p.timeSignature));
    const auto aff = ExpressionMapper::derive(state, p.intent.restraint, Role::Counter, plan.bias);
    const int tonic = 52;
    for (int phrase = 0; phrase * 2 < sec.bars; ++phrase) {
        const Tick start = static_cast<Tick>(sec.startBar + phrase * 2) * bar + p.ppq / 2;
        if (start >= static_cast<Tick>(sec.startBar + sec.bars) * bar) break;
        Motif local = transformMotif(motif, phrase % 2 ? MotifTransform::Invert : MotifTransform::Retrograde, 0);
        Tick cursor = start;
        for (const auto& mn : local.notes) {
            if (cursor >= static_cast<Tick>(sec.startBar + sec.bars) * bar) break;
            int pitch = scale.midiAt(mn.degree - 3, tonic);
            if (const auto* chord = chordAt(p, cursor); chord && !chord->pitches.empty()) {
                pitch = nearestPitchInSet(pitch, chord->pitches, 40, 64);
            }
            auto n = makeNote(p, static_cast<int>(cursor), static_cast<int>(mn.duration), pitch,
                              static_cast<int>(std::lround(127.0f * (aff.velocityBase * 0.46f))),
                              track.midiChannel, aff.brightness * 0.7f, 0.3f, 0.4f, 0.4f, 0.35f);
            n.pan = -0.2f;
            track.notes.push_back(n);
            cursor += mn.duration + p.ppq / 4;
        }
    }
}

void generatePercussion(Project& p, int trackIndex, const StylePlan& plan, int si) {
    auto& track = p.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& sec = p.sections[static_cast<std::size_t>(si)];
    const auto& st = plan.sections[static_cast<std::size_t>(si)];
    if ((st.layers & LayerPercussion) == 0) return;
    const int bar = ticksPerBarFor(p);
    const auto state = p.curves.at((sec.startBar + sec.bars * 0.5) * beatsPerBar(p.timeSignature));
    const auto aff = ExpressionMapper::derive(state, p.intent.restraint, Role::Percussion, plan.bias);
    const int beats = beatsPerBar(p.timeSignature);
    const Tick startTick = static_cast<Tick>(sec.startBar) * bar;
    const Tick endTick = static_cast<Tick>(sec.startBar + sec.bars) * bar;

    auto hit = [&](Tick s, int pitch, int velocity, Tick duration) {
        if (s < startTick || s >= endTick) return;
        auto n = makeNote(p, static_cast<int>(s), static_cast<int>(duration), pitch, velocity,
                          track.midiChannel, 0.55f, 0.15f, 0.5f, 0.25f, 0.1f);
        track.notes.push_back(n);
    };

    for (Tick barStart = startTick; barStart < endTick; barStart += bar) {
        if (plan.id == "kinetic") {
            for (int b = 0; b < beats; ++b) {
                hit(barStart + b * p.ppq, 36, static_cast<int>(std::lround(127.0f * (0.82f + 0.08f * aff.velocityBase))), p.ppq / 8);
            }
            hit(barStart + p.ppq, 38, 105, p.ppq / 8);
            hit(barStart + 3 * p.ppq, 38, 112, p.ppq / 8);
            for (int eighth = 0; eighth < beats * 2; ++eighth) {
                const Tick s = barStart + eighth * (p.ppq / 2);
                const bool open = (sec.name == "Peak" && eighth == beats * 2 - 1);
                hit(s, open ? 46 : 42, open ? 96 : (eighth % 2 ? 70 : 52), p.ppq / 8);
            }
            if (sec.name == "Break") continue;
            hit(barStart + 7 * p.ppq / 2, 39, 78, p.ppq / 6);
        } else {
            // 5/4 grouped 3+2: kick/taiko on 1 and 4, tom answer, sparse metal.
            hit(barStart, 36, static_cast<int>(std::lround(127.0f * (0.86f + 0.08f * aff.velocityBase))), p.ppq / 4);
            hit(barStart + 3 * p.ppq, 45, 102, p.ppq / 4);
            if (sec.role == "climax" || sec.role == "development") {
                hit(barStart + 2 * p.ppq, 45, 74, p.ppq / 8);
                hit(barStart + 4 * p.ppq, 39, 62, p.ppq / 8);
            }
        }
    }
}

} // namespace

std::string_view demoId(DemoStyle style) { return styleId(style); }

DemoStyle demoStyleFromId(std::string_view id) { return styleFromId(id); }

GenerationSpec demoSpec(DemoStyle style, std::uint64_t seed) {
    const StylePlan plan = planFor(style);
    GenerationSpec spec;
    spec.title = plan.title;
    spec.style = style;
    spec.seed = seed;
    spec.bars = 0;
    spec.bpm = plan.bpm;
    spec.timeSignature = plan.timeSignature;
    spec.key = plan.key;
    spec.intent = defaultIntent(plan.id);
    spec.curves = defaultExpressionCurves(plan.id);
    spec.useExplicitCurves = true;
    spec.humanize = true;
    spec.humanizeAmount = 1.0f;
    return spec;
}

GenerationSpec specFromProject(const Project& project, std::uint64_t seed) {
    GenerationSpec spec;
    spec.title = project.title;
    spec.style = styleFromId(project.styleId);
    spec.seed = seed;
    spec.bars = 0;
    spec.bpm = project.tempoMap.empty() ? 120.0 : project.tempoMap.front().bpm;
    spec.timeSignature = project.timeSignature;
    spec.key = project.key;
    spec.intent = project.intent;
    spec.curves = project.curves;
    spec.useExplicitCurves = true;
    spec.humanize = true;
    spec.humanizeAmount = 1.0f;
    return spec;
}

Project composeProject(const GenerationSpec& spec, GenerationDiagnostics* diagnostics) {
    const auto started = std::chrono::steady_clock::now();
    GenerationDiagnostics local;
    if (!diagnostics) diagnostics = &local;
    *diagnostics = GenerationDiagnostics{};

    const auto style = spec.style;
    StylePlan plan = planFor(style);
    Project p;
    p.schemaVersion = kProjectSchemaVersion;
    p.title = spec.title.empty() ? plan.title : spec.title;
    p.styleId = plan.id;
    p.seed = spec.seed ? spec.seed : 1;
    p.ppq = kPPQ;
    p.timeSignature = spec.timeSignature.numerator > 0 ? spec.timeSignature : plan.timeSignature;
    p.key = spec.key;
    p.tempoMap = {{0, spec.bpm > 0.0 ? spec.bpm : plan.bpm}};
    p.curves = spec.useExplicitCurves ? spec.curves : defaultExpressionCurves(plan.id);
    p.intent = spec.intent;
    if (p.intent.narrative.empty()) p.intent = defaultIntent(plan.id);
    p.narrative = p.intent.narrative;
    p.revisionNote = "Generated by deterministic composer core";

    // Fit the section plan to an explicitly requested length while preserving
    // its proportions. This keeps form editable instead of hard-coding 48 bars.
    int desiredBars = spec.bars > 0 ? spec.bars : 0;
    int planBars = 0;
    for (const auto& s : plan.sections) planBars += s.bars;
    if (desiredBars > 0 && desiredBars != planBars) {
        int remaining = desiredBars;
        for (std::size_t i = 0; i < plan.sections.size(); ++i) {
            if (i + 1 == plan.sections.size()) {
                plan.sections[i].bars = std::max(1, remaining);
            } else {
                const int scaled = std::max(1, static_cast<int>(std::lround(
                    static_cast<double>(plan.sections[i].bars) * desiredBars / planBars)));
                plan.sections[i].bars = scaled;
                remaining -= scaled;
            }
        }
    }

    int startBar = 0;
    for (std::size_t i = 0; i < plan.sections.size(); ++i) {
        const auto& tpl = plan.sections[i];
        Section s;
        s.name = tpl.name;
        s.role = tpl.role;
        s.description = tpl.description;
        s.startBar = startBar;
        s.bars = std::max(1, tpl.bars);
        const double centerBeat = (startBar + s.bars * 0.5) * beatsPerBar(p.timeSignature);
        s.target = p.curves.at(centerBeat);
        p.sections.push_back(s);
        startBar += s.bars;
    }

    int trackId = 1;
    for (const auto& tt : plan.tracks) {
        Track t;
        t.id = trackId++;
        t.name = tt.name;
        t.role = tt.role;
        t.instrument = tt.instrument;
        t.midiChannel = tt.channel;
        t.gainDb = tt.gainDb;
        t.pan = tt.pan;
        t.reverbSend = std::clamp(tt.reverb + p.curves.distance.at(0.0) * 0.15f, 0.0f, 1.0f);
        p.tracks.push_back(std::move(t));
    }

    buildChords(p, plan, diagnostics);

    Motif motif = plan.id == "cinematic" ? cinematicMotif()
                : (plan.id == "kinetic" ? kineticMotif() : chamberMotif());
    p.motifs.push_back(motif);

    std::vector<int> recipes;
    recipes.reserve(plan.tracks.size());
    for (const auto& tt : plan.tracks) recipes.push_back(static_cast<int>(tt.recipe));

    int melodyTrack = -1;
    for (std::size_t i = 0; i < plan.tracks.size(); ++i) {
        if (plan.tracks[i].recipe == TrackRecipe::Melody) melodyTrack = static_cast<int>(i);
    }
    if (melodyTrack >= 0) {
        generateMelody(p, melodyTrack, plan, motif, plan.tracks, recipes, diagnostics);
    }
    for (std::size_t si = 0; si < p.sections.size(); ++si) {
        for (std::size_t ti = 0; ti < plan.tracks.size(); ++ti) {
            switch (plan.tracks[ti].recipe) {
                case TrackRecipe::BassLine: generateBass(p, static_cast<int>(ti), plan, plan.tracks, static_cast<int>(si)); break;
                case TrackRecipe::SubBass: generateSub(p, static_cast<int>(ti), plan, static_cast<int>(si)); break;
                case TrackRecipe::HarmonyArp: generateHarmonyArp(p, static_cast<int>(ti), plan, static_cast<int>(si)); break;
                case TrackRecipe::Pad: generatePad(p, static_cast<int>(ti), plan, static_cast<int>(si)); break;
                case TrackRecipe::TexturePad: generateTexture(p, static_cast<int>(ti), plan, static_cast<int>(si)); break;
                case TrackRecipe::CounterLine: generateCounter(p, static_cast<int>(ti), plan, motif, static_cast<int>(si)); break;
                case TrackRecipe::Percussion: generatePercussion(p, static_cast<int>(ti), plan, static_cast<int>(si)); break;
                case TrackRecipe::Melody: break;
            }
        }
    }

    exposeProjectMetadata(p);
    if (spec.humanize) humanizeProject(p, std::clamp(spec.humanizeAmount, 0.0f, 1.0f));

    std::string validationError;
    if (!validateProject(p, &validationError)) {
        diagnostics->warnings.push_back("validation: " + validationError);
    }
    for (const auto& t : p.tracks) diagnostics->noteCount += static_cast<int>(t.notes.size());
    diagnostics->chordCount = static_cast<int>(p.chords.size());
    diagnostics->phraseCount = static_cast<int>(p.phrases.size());
    diagnostics->motifUseCount = static_cast<int>(p.motifUses.size());
    std::vector<std::vector<int>> voicings;
    for (const auto& c : p.chords) voicings.push_back(c.pitches);
    diagnostics->counterpoint = checkVoicings(voicings);
    if (diagnostics->counterpoint.parallelFifths || diagnostics->counterpoint.parallelOctaves) {
        diagnostics->warnings.push_back("voice-leading check found parallel perfect intervals; review before release");
    }
    diagnostics->generationMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return p;
}

Project regenerateProject(const Project& base,
                          const RegenerationOptions& options,
                          GenerationDiagnostics* diagnostics) {
    const auto started = std::chrono::steady_clock::now();
    GenerationDiagnostics local;
    if (!diagnostics) diagnostics = &local;
    *diagnostics = GenerationDiagnostics{};

    if (options.locks == (LockDimension::Melody | LockDimension::Harmony | LockDimension::Rhythm |
                          LockDimension::Orchestration | LockDimension::Humanization |
                          LockDimension::Expression)) {
        *diagnostics = GenerationDiagnostics{};
        diagnostics->warnings.push_back("all dimensions locked; returning an unchanged copy");
        return base;
    }

    GenerationSpec spec = specFromProject(base, options.seed);
    if (hasLock(options.locks, LockDimension::Expression)) {
        spec.curves = base.curves;
        spec.useExplicitCurves = true;
    }
    Project candidate = composeProject(spec, diagnostics);
    candidate.title = base.title;
    candidate.revisionNote = "Partial regeneration seed=" + std::to_string(options.seed);

    const Tick barTicks = ticksPerBar(base.timeSignature, base.ppq);
    const int startBar = std::clamp(options.rangeStartBar, 0, std::max(0, base.totalBars()));
    const int endBar = options.rangeEndBar < 0 ? base.totalBars()
                                               : std::clamp(options.rangeEndBar, startBar, base.totalBars());
    const Tick rangeStart = static_cast<Tick>(startBar) * barTicks;
    const Tick rangeEnd = static_cast<Tick>(endBar) * barTicks;

    // Preserve out-of-range material exactly; inside the range merge according
    // to the lock mask. This is intentionally conservative and explainable.
    for (auto& baseTrack : base.tracks) {
        Track* candTrack = candidate.findTrack(baseTrack.id);
        if (!candTrack) {
            candidate.tracks.push_back(baseTrack);
            continue;
        }
        std::vector<Note> merged;
        for (const auto& n : baseTrack.notes) {
            if (n.start < rangeStart || n.start >= rangeEnd) merged.push_back(n);
        }
        const bool lockMelody = hasLock(options.locks, LockDimension::Melody) && baseTrack.role == Role::Melody;
        const bool lockHarmony = hasLock(options.locks, LockDimension::Harmony) &&
                                 (baseTrack.role == Role::Harmony || baseTrack.role == Role::Bass ||
                                  baseTrack.role == Role::Texture);
        const bool lockRhythm = hasLock(options.locks, LockDimension::Rhythm);
        const bool lockHuman = hasLock(options.locks, LockDimension::Humanization);

        if (lockMelody || lockHarmony) {
            for (const auto& n : baseTrack.notes) {
                if (n.start >= rangeStart && n.start < rangeEnd) merged.push_back(n);
            }
        } else {
            std::vector<Note> incoming;
            for (const auto& n : candTrack->notes) {
                if (n.start >= rangeStart && n.start < rangeEnd) incoming.push_back(n);
            }
            if (lockRhythm) {
                std::vector<const Note*> baseInRange;
                for (const auto& n : baseTrack.notes) if (n.start >= rangeStart && n.start < rangeEnd) baseInRange.push_back(&n);
                std::sort(baseInRange.begin(), baseInRange.end(), [](const Note* a, const Note* b) {
                    return a->start < b->start || (a->start == b->start && a->pitch < b->pitch);
                });
                std::sort(incoming.begin(), incoming.end(), [](const Note& a, const Note& b) {
                    return a.start < b.start || (a.start == b.start && a.pitch < b.pitch);
                });
                for (std::size_t i = 0; i < incoming.size(); ++i) {
                    if (i < baseInRange.size()) {
                        incoming[i].start = baseInRange[i]->start;
                        incoming[i].duration = baseInRange[i]->duration;
                    }
                    incoming[i].id = candidate.allocateNoteId();
                }
            }
            for (auto& n : incoming) {
                if (n.id == 0 || candidate.findNote(n.id) != nullptr) n.id = candidate.allocateNoteId();
                merged.push_back(n);
            }
        }

        if (lockHuman) {
            for (auto& n : merged) {
                for (const auto& old : baseTrack.notes) {
                    if (old.id == n.id || (old.start == n.start && old.pitch == n.pitch && old.duration == n.duration)) {
                        n.velocity = old.velocity;
                        n.brightness = old.brightness;
                        n.attack = old.attack;
                        n.noise = old.noise;
                        n.release = old.release;
                        n.microMs = old.microMs;
                        n.legato = old.legato;
                        n.pan = old.pan;
                        if (old.id != 0) n.id = old.id;
                        break;
                    }
                }
            }
        }
        std::stable_sort(merged.begin(), merged.end(), [](const Note& a, const Note& b) {
            return a.start < b.start || (a.start == b.start && a.pitch < b.pitch);
        });
        candTrack->notes = std::move(merged);
    }

    if (hasLock(options.locks, LockDimension::Orchestration)) {
        // Keep the original roster/instruments, but retain the merged notes that
        // were just written into the candidate tracks.
        for (const auto& oldTrack : base.tracks) {
            if (auto* newTrack = candidate.findTrack(oldTrack.id)) {
                newTrack->name = oldTrack.name;
                newTrack->role = oldTrack.role;
                newTrack->instrument = oldTrack.instrument;
                newTrack->midiChannel = oldTrack.midiChannel;
                newTrack->gainDb = oldTrack.gainDb;
                newTrack->pan = oldTrack.pan;
                newTrack->reverbSend = oldTrack.reverbSend;
                newTrack->muted = oldTrack.muted;
                newTrack->solo = oldTrack.solo;
            }
        }
        std::vector<Track> lockedTracks;
        for (const auto& oldTrack : base.tracks) {
            if (auto* newTrack = candidate.findTrack(oldTrack.id)) lockedTracks.push_back(*newTrack);
        }
        candidate.tracks = std::move(lockedTracks);
    }
    if (hasLock(options.locks, LockDimension::Expression)) {
        candidate.curves = base.curves;
        candidate.narrative = base.narrative;
        candidate.sections = base.sections;
    }
    // Motif traceability follows the candidate phrase map.
    candidate.motifs = base.motifs;
    validateProject(candidate, nullptr);
    for (const auto& t : candidate.tracks) diagnostics->noteCount += static_cast<int>(t.notes.size());
    diagnostics->chordCount = static_cast<int>(candidate.chords.size());
    diagnostics->phraseCount = static_cast<int>(candidate.phrases.size());
    diagnostics->motifUseCount = static_cast<int>(candidate.motifUses.size());
    diagnostics->generationMs += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return candidate;
}

} // namespace ec
