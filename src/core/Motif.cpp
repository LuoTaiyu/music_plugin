#include "core/Motif.hpp"

#include <algorithm>

namespace ec {

std::string_view toString(MotifTransform t) {
    switch (t) {
        case MotifTransform::Original: return "original";
        case MotifTransform::Transpose: return "transpose";
        case MotifTransform::Invert: return "invert";
        case MotifTransform::Retrograde: return "retrograde";
        case MotifTransform::Augment: return "augment";
        case MotifTransform::Diminish: return "diminish";
        case MotifTransform::Fragment: return "fragment";
        case MotifTransform::Extend: return "extend";
        case MotifTransform::ChangeEnding: return "change_ending";
        case MotifTransform::Ornament: return "ornament";
    }
    return "original";
}

MotifTransform motifTransformFromString(std::string_view s) {
    if (s == "transpose") return MotifTransform::Transpose;
    if (s == "invert") return MotifTransform::Invert;
    if (s == "retrograde") return MotifTransform::Retrograde;
    if (s == "augment") return MotifTransform::Augment;
    if (s == "diminish") return MotifTransform::Diminish;
    if (s == "fragment") return MotifTransform::Fragment;
    if (s == "extend") return MotifTransform::Extend;
    if (s == "change_ending") return MotifTransform::ChangeEnding;
    if (s == "ornament") return MotifTransform::Ornament;
    return MotifTransform::Original;
}

Motif transformMotif(const Motif& source, MotifTransform transform, int parameter) {
    Motif out = source;
    switch (transform) {
        case MotifTransform::Original:
            break;
        case MotifTransform::Transpose:
            for (auto& n : out.notes) n.degree += parameter;
            break;
        case MotifTransform::Invert: {
            if (out.notes.empty()) break;
            const int pivot = out.notes.front().degree;
            for (auto& n : out.notes) n.degree = pivot - (n.degree - pivot);
            break;
        }
        case MotifTransform::Retrograde:
            std::reverse(out.notes.begin(), out.notes.end());
            break;
        case MotifTransform::Augment:
            for (auto& n : out.notes) n.duration *= 2;
            break;
        case MotifTransform::Diminish:
            for (auto& n : out.notes) n.duration = std::max<Tick>(kPPQ / 8, n.duration / 2);
            break;
        case MotifTransform::Fragment:
            if (out.notes.size() > 3) out.notes.resize(out.notes.size() / 2 + 1);
            break;
        case MotifTransform::Extend:
            if (!out.notes.empty()) {
                const auto last = out.notes.back();
                out.notes.push_back({last.degree + ((parameter & 1) ? -1 : 1), last.duration, last.accent * 0.8f});
                out.notes.push_back({out.notes.front().degree, last.duration * 2, last.accent * 0.6f});
            }
            break;
        case MotifTransform::ChangeEnding:
            if (!out.notes.empty()) out.notes.back().degree += (parameter == 0 ? 1 : parameter);
            break;
        case MotifTransform::Ornament:
            if (out.notes.size() >= 2) {
                const auto target = out.notes[out.notes.size() - 1];
                out.notes.insert(out.notes.end() - 1, {target.degree - 1, std::max<Tick>(kPPQ / 8, target.duration / 3), 0.55f});
            }
            break;
    }
    out.name += " / " + std::string(toString(transform));
    return out;
}

Motif cloneMotifWithId(const Motif& source, std::string id, std::string name) {
    Motif out = source;
    out.id = std::move(id);
    out.name = std::move(name);
    return out;
}

Motif chamberMotif() {
    Motif m;
    m.id = "chamber_cell";
    m.name = "Three-note turn";
    m.description = "Scale degrees 0-2-1-4 with a long final tone; narrow, speech-like, no heroic leap.";
    m.baseOctave = 4;
    m.notes = {
        {0, kPPQ / 2, 0.85f},
        {2, kPPQ / 4, 0.90f},
        {1, kPPQ / 2, 0.78f},
        {4, kPPQ, 0.92f},
    };
    return m;
}

Motif cinematicMotif() {
    Motif m;
    m.id = "cinematic_cell";
    m.name = "Semi-tone clock";
    m.description = "Neighbour-note cell in irregular 3+2 grouping; it repeats while the harmony tightens.";
    m.baseOctave = 3;
    m.notes = {
        {0, kPPQ / 2, 0.95f},
        {1, kPPQ / 4, 0.65f},
        {0, kPPQ / 2, 0.88f},
        {6, kPPQ / 2, 0.80f},
        {5, kPPQ, 0.72f},
    };
    return m;
}

Motif kineticMotif() {
    Motif m;
    m.id = "kinetic_cell";
    m.name = "Off-grid spark";
    m.description = "Syncopated 16th-cell with a repeated note; designed for displacement, not constant new material.";
    m.baseOctave = 4;
    m.notes = {
        {0, kPPQ / 4, 0.92f},
        {0, kPPQ / 8, 0.62f},
        {3, kPPQ / 4, 0.88f},
        {2, kPPQ / 4, 0.74f},
        {4, kPPQ / 2, 0.84f},
    };
    return m;
}

} // namespace ec
