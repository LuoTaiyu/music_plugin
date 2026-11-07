#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace ec {

inline constexpr int kPPQ = 960;
inline constexpr int kDefaultPPQ = 960;
using Tick = std::int64_t;

enum class Mode {
    Major,
    NaturalMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    HarmonicMinor
};

struct Key {
    int tonic = 0; // pitch class, C = 0
    Mode mode = Mode::NaturalMinor;
};

struct TimeSignature {
    int numerator = 4;
    int denominator = 4;

    friend bool operator==(const TimeSignature&, const TimeSignature&) = default;
};

struct TempoPoint {
    Tick tick = 0;
    double bpm = 120.0;
};

inline Tick ticksPerBeat(int ppq = kPPQ) noexcept { return ppq; }
inline Tick ticksPerBar(const TimeSignature& ts, int ppq = kPPQ) noexcept {
    return static_cast<Tick>(ppq) * 4 / std::max(1, ts.denominator) * std::max(1, ts.numerator);
}
inline Tick barToTick(int bar, const TimeSignature& ts, int ppq = kPPQ) noexcept {
    return static_cast<Tick>(bar) * ticksPerBar(ts, ppq);
}

struct AffectState {
    float valence = 0.0f;    // dark (-1) .. bright (+1)
    float arousal = 0.35f;   // calm (0) .. energetic (1)
    float tension = 0.25f;   // relaxed (0) .. suspended/pressing (1)
    float distance = 0.35f;  // intimate (0) .. distant/grand (1)
};

struct CurvePoint {
    double beat = 0.0;
    float value = 0.0f;
};

struct Curve {
    std::vector<CurvePoint> points;

    float at(double beat) const noexcept;
    void addPoint(double beat, float value);
    bool removeNear(double beat, double tolerance = 0.05);
    void sort();
    void setDefault(std::initializer_list<CurvePoint> pts);
};

struct ExpressionCurves {
    Curve valence;
    Curve arousal;
    Curve tension;
    Curve distance;

    AffectState at(double beat) const noexcept;
};

enum class Perspective { Monologue, Dialogue, Observer, AbstractAtmosphere };
enum class Scene { IntimateRoom, OpenSpace, Stage, AbstractInterior };
enum class Restraint { Restrained, OpeningUp, Direct };
enum class NarrativeFunction { Setup, Development, Turn, Climax, Release, Coda };

struct NarrativeEvent {
    double bar = 0.0;
    std::string type;        // "harmonic", "texture", "register", "space", ...
    std::string description;
    bool audible = true;
};

struct Intent {
    Perspective perspective = Perspective::Monologue;
    Scene scene = Scene::IntimateRoom;
    Restraint restraint = Restraint::Restrained;
    std::string culture = "Western tonal (explicit default assumption)";
    std::string styleBrief;
    std::string references;
    std::vector<std::string> prohibitions;
    std::string lyricOrText;
    std::string sourceMaterialNote;
    std::vector<NarrativeEvent> narrative;
};

enum class Role { Melody, Harmony, Bass, Counter, Texture, Percussion };
enum class Instrument { FeltPiano, BowedString, GlassMallet, OrganicBass, DrumKit };

struct Note {
    std::uint32_t id = 0;
    Tick start = 0;
    Tick duration = kPPQ;
    int pitch = 60;
    int velocity = 90;
    int channel = 1;
    float pan = 0.0f;
    float brightness = 0.5f;
    float attack = 0.5f;
    float noise = 0.35f;
    float release = 0.5f;
    float microMs = 0.0f;  // signed, applied at render/MIDI time
    float legato = 0.6f;   // 0 = detached, 1 = overlapping
    bool muted = false;
    std::uint32_t motifInstance = 0;
    int phrase = -1;
};

struct ChordEvent {
    Tick start = 0;
    Tick duration = kPPQ * 4;
    std::string symbol;
    int rootPc = 0;
    std::vector<int> pitches; // concrete MIDI voicing, low-to-high
    float tension = 0.25f;
    int inversion = 0;
    std::string function;     // e.g. "i", "VI", "V7sus"
};

struct Section {
    std::string name;
    std::string role;
    std::string description;
    int startBar = 0;
    int bars = 8;
    AffectState target;
};

struct Phrase {
    int section = -1;
    Tick start = 0;
    Tick end = 0;
    std::string function; // "antecedent", "consequent", "fragment", ...
    std::uint32_t peakNoteId = 0;
    float intensity = 0.5f;
    bool breathAtEnd = true;
};

struct MotifNote {
    int degree = 0;          // scale degree, can exceed [0, 7)
    Tick duration = kPPQ / 2;
    float accent = 1.0f;
};

struct Motif {
    std::string id;
    std::string name;
    std::string description;
    int baseOctave = 4;
    std::vector<MotifNote> notes;
};

struct MotifUse {
    std::string motifId;
    std::uint32_t instanceId = 0;
    Tick start = 0;
    Tick end = 0;
    int trackId = 0;
    std::string transformation;
};

struct Track {
    int id = 0;
    std::string name;
    Role role = Role::Melody;
    Instrument instrument = Instrument::FeltPiano;
    int midiChannel = 1;
    float gainDb = 0.0f;
    float pan = 0.0f;
    float reverbSend = 0.3f;
    bool muted = false;
    bool solo = false;
    std::vector<Note> notes;
};

struct AutomationLane {
    std::string id;
    std::string target;
    std::string unit;
    std::vector<CurvePoint> points;
};

struct Project {
    int schemaVersion = 1;
    std::string title = "Untitled";
    std::string styleId = "chamber";
    std::string revisionNote;
    std::uint64_t seed = 1;
    int ppq = kPPQ;
    TimeSignature timeSignature;
    std::vector<TempoPoint> tempoMap{{0, 68.0}};
    Key key;
    Intent intent;
    ExpressionCurves curves;
    std::vector<NarrativeEvent> narrative;
    std::vector<Section> sections;
    std::vector<Track> tracks;
    std::vector<ChordEvent> chords;
    std::vector<Phrase> phrases;
    std::vector<Motif> motifs;
    std::vector<MotifUse> motifUses;
    std::vector<AutomationLane> automation;
    std::uint32_t nextNoteId = 1;
    std::uint32_t nextMotifInstance = 1;

    Tick totalTicks() const;
    int totalBars() const;
    Track* findTrack(int id);
    const Track* findTrack(int id) const;
    Note* findNote(std::uint32_t id);
    const Note* findNote(std::uint32_t id) const;
    std::uint32_t allocateNoteId() { return nextNoteId++; }
};

enum class LockDimension : std::uint32_t {
    None = 0,
    Melody = 1u << 0,
    Harmony = 1u << 1,
    Rhythm = 1u << 2,
    Orchestration = 1u << 3,
    Humanization = 1u << 4,
    Expression = 1u << 5
};

inline LockDimension operator|(LockDimension a, LockDimension b) noexcept {
    return static_cast<LockDimension>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
inline LockDimension& operator|=(LockDimension& a, LockDimension b) noexcept { return a = a | b; }
inline bool hasLock(LockDimension mask, LockDimension bit) noexcept {
    return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(bit)) != 0;
}

std::string_view toString(Mode mode);
std::string_view toString(Role role);
std::string_view toString(Instrument instrument);
std::string_view toString(Perspective p);
std::string_view toString(Scene s);
std::string_view toString(Restraint r);
std::string_view toString(NarrativeFunction f);
Mode modeFromString(std::string_view s);
Role roleFromString(std::string_view s);
Instrument instrumentFromString(std::string_view s);
Perspective perspectiveFromString(std::string_view s);
Scene sceneFromString(std::string_view s);
Restraint restraintFromString(std::string_view s);

inline int clampPitch(int p) noexcept { return std::clamp(p, 0, 127); }
inline int clampVelocity(int v) noexcept { return std::clamp(v, 1, 127); }
inline float clamp01(float v) noexcept { return std::clamp(v, 0.0f, 1.0f); }

} // namespace ec
