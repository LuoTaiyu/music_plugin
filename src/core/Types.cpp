#include "core/Types.hpp"

#include <algorithm>

namespace ec {

std::string_view toString(Mode mode) {
    switch (mode) {
        case Mode::Major: return "major";
        case Mode::NaturalMinor: return "natural_minor";
        case Mode::Dorian: return "dorian";
        case Mode::Phrygian: return "phrygian";
        case Mode::Lydian: return "lydian";
        case Mode::Mixolydian: return "mixolydian";
        case Mode::HarmonicMinor: return "harmonic_minor";
    }
    return "natural_minor";
}

std::string_view toString(Role role) {
    switch (role) {
        case Role::Melody: return "melody";
        case Role::Harmony: return "harmony";
        case Role::Bass: return "bass";
        case Role::Counter: return "counter";
        case Role::Texture: return "texture";
        case Role::Percussion: return "percussion";
    }
    return "melody";
}

std::string_view toString(Instrument instrument) {
    switch (instrument) {
        case Instrument::FeltPiano: return "felt_piano";
        case Instrument::BowedString: return "bowed_string";
        case Instrument::GlassMallet: return "glass_mallet";
        case Instrument::OrganicBass: return "organic_bass";
        case Instrument::DrumKit: return "drum_kit";
    }
    return "felt_piano";
}

std::string_view toString(Perspective p) {
    switch (p) {
        case Perspective::Monologue: return "monologue";
        case Perspective::Dialogue: return "dialogue";
        case Perspective::Observer: return "observer";
        case Perspective::AbstractAtmosphere: return "abstract_atmosphere";
    }
    return "monologue";
}

std::string_view toString(Scene s) {
    switch (s) {
        case Scene::IntimateRoom: return "intimate_room";
        case Scene::OpenSpace: return "open_space";
        case Scene::Stage: return "stage";
        case Scene::AbstractInterior: return "abstract_interior";
    }
    return "intimate_room";
}

std::string_view toString(Restraint r) {
    switch (r) {
        case Restraint::Restrained: return "restrained";
        case Restraint::OpeningUp: return "opening_up";
        case Restraint::Direct: return "direct";
    }
    return "restrained";
}

std::string_view toString(NarrativeFunction f) {
    switch (f) {
        case NarrativeFunction::Setup: return "setup";
        case NarrativeFunction::Development: return "development";
        case NarrativeFunction::Turn: return "turn";
        case NarrativeFunction::Climax: return "climax";
        case NarrativeFunction::Release: return "release";
        case NarrativeFunction::Coda: return "coda";
    }
    return "development";
}

Mode modeFromString(std::string_view s) {
    if (s == "major") return Mode::Major;
    if (s == "natural_minor") return Mode::NaturalMinor;
    if (s == "dorian") return Mode::Dorian;
    if (s == "phrygian") return Mode::Phrygian;
    if (s == "lydian") return Mode::Lydian;
    if (s == "mixolydian") return Mode::Mixolydian;
    if (s == "harmonic_minor") return Mode::HarmonicMinor;
    return Mode::NaturalMinor;
}

Role roleFromString(std::string_view s) {
    if (s == "melody") return Role::Melody;
    if (s == "harmony") return Role::Harmony;
    if (s == "bass") return Role::Bass;
    if (s == "counter") return Role::Counter;
    if (s == "texture") return Role::Texture;
    if (s == "percussion") return Role::Percussion;
    return Role::Melody;
}

Instrument instrumentFromString(std::string_view s) {
    if (s == "felt_piano") return Instrument::FeltPiano;
    if (s == "bowed_string") return Instrument::BowedString;
    if (s == "glass_mallet") return Instrument::GlassMallet;
    if (s == "organic_bass") return Instrument::OrganicBass;
    if (s == "drum_kit") return Instrument::DrumKit;
    return Instrument::FeltPiano;
}

Perspective perspectiveFromString(std::string_view s) {
    if (s == "monologue") return Perspective::Monologue;
    if (s == "dialogue") return Perspective::Dialogue;
    if (s == "observer") return Perspective::Observer;
    if (s == "abstract_atmosphere") return Perspective::AbstractAtmosphere;
    return Perspective::Monologue;
}

Scene sceneFromString(std::string_view s) {
    if (s == "intimate_room") return Scene::IntimateRoom;
    if (s == "open_space") return Scene::OpenSpace;
    if (s == "stage") return Scene::Stage;
    if (s == "abstract_interior") return Scene::AbstractInterior;
    return Scene::IntimateRoom;
}

Restraint restraintFromString(std::string_view s) {
    if (s == "restrained") return Restraint::Restrained;
    if (s == "opening_up") return Restraint::OpeningUp;
    if (s == "direct") return Restraint::Direct;
    return Restraint::Restrained;
}

Tick Project::totalTicks() const {
    Tick end = 0;
    const auto bar = ticksPerBar(timeSignature, ppq);
    for (const auto& section : sections) {
        end = std::max(end, static_cast<Tick>(section.startBar + section.bars) * bar);
    }
    for (const auto& track : tracks) {
        for (const auto& note : track.notes) {
            end = std::max(end, note.start + note.duration);
        }
    }
    return end;
}

int Project::totalBars() const {
    const auto bar = ticksPerBar(timeSignature, ppq);
    return static_cast<int>((totalTicks() + bar - 1) / bar);
}

Track* Project::findTrack(int id) {
    for (auto& track : tracks) if (track.id == id) return &track;
    return nullptr;
}

const Track* Project::findTrack(int id) const {
    for (const auto& track : tracks) if (track.id == id) return &track;
    return nullptr;
}

Note* Project::findNote(std::uint32_t id) {
    for (auto& track : tracks) {
        for (auto& note : track.notes) if (note.id == id) return &note;
    }
    return nullptr;
}

const Note* Project::findNote(std::uint32_t id) const {
    for (const auto& track : tracks) {
        for (const auto& note : track.notes) if (note.id == id) return &note;
    }
    return nullptr;
}

} // namespace ec
