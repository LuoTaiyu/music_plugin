#include "core/Theory.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <sstream>

namespace ec {
namespace {

constexpr std::array<const char*, 12> kFlatNames{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
constexpr std::array<const char*, 12> kSharpNames{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

std::array<int, 7> intervalsFor(Mode mode) {
    switch (mode) {
        case Mode::Major: return {0, 2, 4, 5, 7, 9, 11};
        case Mode::NaturalMinor: return {0, 2, 3, 5, 7, 8, 10};
        case Mode::Dorian: return {0, 2, 3, 5, 7, 9, 10};
        case Mode::Phrygian: return {0, 1, 3, 5, 7, 8, 10};
        case Mode::Lydian: return {0, 2, 4, 6, 7, 9, 11};
        case Mode::Mixolydian: return {0, 2, 4, 5, 7, 9, 10};
        case Mode::HarmonicMinor: return {0, 2, 3, 5, 7, 8, 11};
    }
    return {0, 2, 3, 5, 7, 8, 10};
}

std::string modeLabel(Mode mode) {
    switch (mode) {
        case Mode::Major: return "major";
        case Mode::NaturalMinor: return "minor";
        case Mode::Dorian: return "dorian";
        case Mode::Phrygian: return "phrygian";
        case Mode::Lydian: return "lydian";
        case Mode::Mixolydian: return "mixolydian";
        case Mode::HarmonicMinor: return "harmonic minor";
    }
    return "minor";
}

const char* qualitySuffix(ChordQuality q, bool ninth) {
    switch (q) {
        case ChordQuality::Major: return ninth ? "add9" : "";
        case ChordQuality::Minor: return ninth ? "m(add9)" : "m";
        case ChordQuality::Diminished: return "dim";
        case ChordQuality::Augmented: return "aug";
        case ChordQuality::Dominant7: return ninth ? "9" : "7";
        case ChordQuality::Major7: return ninth ? "maj9" : "maj7";
        case ChordQuality::Minor7: return ninth ? "m9" : "m7";
        case ChordQuality::HalfDiminished7: return ninth ? "m7b5(add9)" : "m7b5";
        case ChordQuality::MinorMajor7: return "mMaj7";
        case ChordQuality::Sus2: return "sus2";
        case ChordQuality::Sus4: return "sus4";
        case ChordQuality::Dominant7Sus4: return "7sus4";
    }
    return "";
}

bool isMinorish(ChordQuality q) {
    return q == ChordQuality::Minor || q == ChordQuality::Minor7 || q == ChordQuality::MinorMajor7 ||
           q == ChordQuality::Diminished || q == ChordQuality::HalfDiminished7;
}

int mod12(int value) {
    const int m = value % 12;
    return m < 0 ? m + 12 : m;
}

} // namespace

int Scale::pitchClassAt(int degree) const noexcept {
    const int octave = degree >= 0 ? degree / 7 : -((-degree + 6) / 7);
    const int step = degree - octave * 7;
    return mod12(key.tonic + intervals[static_cast<std::size_t>(step)]);
}

int Scale::midiAt(int degree, int tonicMidi) const noexcept {
    const int octave = degree >= 0 ? degree / 7 : -((-degree + 6) / 7);
    const int step = degree - octave * 7;
    return tonicMidi + octave * 12 + intervals[static_cast<std::size_t>(step)];
}

bool Scale::contains(int pitchClass) const noexcept {
    for (int pc : pitchClasses) if (pc == mod12(pitchClass)) return true;
    return false;
}

int Scale::nearestMember(int midi, int low, int high) const noexcept {
    int best = std::clamp(midi, low, high);
    int bestDistance = std::numeric_limits<int>::max();
    for (int p = std::max(0, low); p <= std::min(127, high); ++p) {
        if (!contains(p)) continue;
        const int d = std::abs(p - midi);
        if (d < bestDistance) { bestDistance = d; best = p; }
    }
    return best;
}

Scale makeScale(const Key& key) {
    Scale scale;
    scale.key = key;
    scale.intervals = intervalsFor(key.mode);
    for (std::size_t i = 0; i < scale.intervals.size(); ++i) {
        scale.pitchClasses[i] = mod12(key.tonic + scale.intervals[i]);
    }
    return scale;
}

std::string keyName(const Key& key, bool preferFlats) {
    return pitchClassName(mod12(key.tonic), preferFlats) + " " + modeLabel(key.mode);
}

std::vector<int> chordIntervals(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major: return {0, 4, 7};
        case ChordQuality::Minor: return {0, 3, 7};
        case ChordQuality::Diminished: return {0, 3, 6};
        case ChordQuality::Augmented: return {0, 4, 8};
        case ChordQuality::Dominant7: return {0, 4, 7, 10};
        case ChordQuality::Major7: return {0, 4, 7, 11};
        case ChordQuality::Minor7: return {0, 3, 7, 10};
        case ChordQuality::HalfDiminished7: return {0, 3, 6, 10};
        case ChordQuality::MinorMajor7: return {0, 3, 7, 11};
        case ChordQuality::Sus2: return {0, 2, 7};
        case ChordQuality::Sus4: return {0, 5, 7};
        case ChordQuality::Dominant7Sus4: return {0, 5, 7, 10};
    }
    return {0, 4, 7};
}

std::vector<int> chordPitchClasses(const Key& key,
                                   int degree,
                                   ChordQuality quality,
                                   bool addNinth,
                                   bool addEleventh) {
    const Scale scale = makeScale(key);
    const int root = scale.pitchClassAt(degree);
    std::vector<int> pcs;
    for (int interval : chordIntervals(quality)) pcs.push_back(mod12(root + interval));
    if (addNinth) pcs.push_back(mod12(root + 14));
    if (addEleventh) pcs.push_back(mod12(root + 17));
    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
    return pcs;
}

std::string chordSymbol(const Key& key,
                        int rootPc,
                        ChordQuality quality,
                        bool addNinth,
                        int inversion,
                        bool preferFlats) {
    (void)key;
    std::string out = pitchClassName(mod12(rootPc), preferFlats);
    out += qualitySuffix(quality, addNinth);
    if (inversion != 0) {
        auto intervals = chordIntervals(quality);
        if (!intervals.empty()) {
            const int idx = ((inversion % static_cast<int>(intervals.size())) + static_cast<int>(intervals.size())) %
                            static_cast<int>(intervals.size());
            out += "/" + pitchClassName(mod12(rootPc + intervals[static_cast<std::size_t>(idx)]), preferFlats);
        }
    }
    return out;
}

std::vector<int> voiceChord(const std::vector<int>& previousVoicing,
                            const std::vector<int>& pitchClasses,
                            int bassPitchClass,
                            int low,
                            int high,
                            int voices,
                            int center) {
    voices = std::clamp(voices, 2, 6);
    if (pitchClasses.empty()) return {};
    std::vector<int> pcs = pitchClasses;
    for (auto& pc : pcs) pc = mod12(pc);
    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());

    const int bass = nearestPitchWithClass(low + 2, bassPitchClass, low, low + 12);
    std::vector<int> candidates;
    for (int p = bass + 3; p <= high; ++p) {
        if (std::find(pcs.begin(), pcs.end(), mod12(p)) != pcs.end()) candidates.push_back(p);
    }
    if (candidates.size() < static_cast<std::size_t>(voices - 1)) {
        for (int p = low + 1; p <= high; ++p) {
            if (std::find(pcs.begin(), pcs.end(), mod12(p)) != pcs.end() &&
                std::find(candidates.begin(), candidates.end(), p) == candidates.end()) {
                candidates.push_back(p);
            }
        }
        std::sort(candidates.begin(), candidates.end());
    }

    int bestScore = std::numeric_limits<int>::max();
    std::vector<int> best{bass};
    const int upperCount = voices - 1;
    std::vector<int> chosen;
    chosen.reserve(static_cast<std::size_t>(upperCount));

    auto evaluate = [&]() {
        std::vector<int> v{bass};
        v.insert(v.end(), chosen.begin(), chosen.end());
        // Sorted voicing: reject crossings and enforce playable spacing.
        for (std::size_t i = 1; i < v.size(); ++i) if (v[i] <= v[i - 1]) return;
        int score = 0;
        if (v[1] - v[0] > 19) score += (v[1] - v[0] - 19) * 4;
        for (std::size_t i = 2; i < v.size(); ++i) {
            const int gap = v[i] - v[i - 1];
            if (gap > 12) score += (gap - 12) * 3;
            if (gap > 7) score += (gap - 7);
        }
        if (!previousVoicing.empty()) {
            const std::size_t n = std::min(previousVoicing.size(), v.size());
            std::vector<int> prev = previousVoicing;
            std::sort(prev.begin(), prev.end());
            for (std::size_t i = 0; i < n; ++i) {
                const int move = std::abs(v[i] - prev[i]);
                score += move;
                if (move > 7) score += (move - 7) * 2;
            }
            // Strongly discourage parallel perfect intervals. The score is a
            // preference, not a proof: some chromatic progressions may still
            // have no alternative within the playable range.
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t j = i + 1; j < n; ++j) {
                    const int before = mod12(prev[j] - prev[i]);
                    const int after = mod12(v[j] - v[i]);
                    const int di = v[i] - prev[i];
                    const int dj = v[j] - prev[j];
                    if (di == 0 || dj == 0 || ((di > 0) != (dj > 0))) continue;
                    if (before == 7 && after == 7) score += 400;
                    if (before == 0 && after == 0) score += 400;
                }
            }
        }
        score += std::abs(v.back() - center);
        if (score < bestScore) {
            bestScore = score;
            best = v;
        }
    };

    std::function<void(int, int)> recurse = [&](int depth, int startIndex) {
        if (depth == upperCount) {
            evaluate();
            return;
        }
        for (int i = startIndex; i < static_cast<int>(candidates.size()); ++i) {
            chosen.push_back(candidates[static_cast<std::size_t>(i)]);
            recurse(depth + 1, i + 1);
            chosen.pop_back();
        }
    };
    if (upperCount > 0 && static_cast<int>(candidates.size()) >= upperCount) {
        recurse(0, 0);
    }
    if (best.size() < static_cast<std::size_t>(voices)) {
        // Deterministic fallback: fill upward by nearest chord tone.
        for (int p = bass + 3; p <= high && static_cast<int>(best.size()) < voices; ++p) {
            if (std::find(pcs.begin(), pcs.end(), mod12(p)) != pcs.end()) best.push_back(p);
        }
    }
    return best;
}

std::vector<std::string> parallelPerfectWarnings(const std::vector<std::vector<int>>& voicings) {
    std::vector<std::string> warnings;
    for (std::size_t c = 1; c < voicings.size(); ++c) {
        const auto& a = voicings[c - 1];
        const auto& b = voicings[c];
        const std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                const int ia = mod12(a[j] - a[i]);
                const int ib = mod12(b[j] - b[i]);
                const int da = b[i] - a[i];
                const int db = b[j] - a[j];
                if (da == 0 || db == 0) continue;
                if ((da > 0) != (db > 0)) continue;
                if (ia == 7 && ib == 7) {
                    warnings.push_back("parallel fifth between voices " + std::to_string(i) + " and " + std::to_string(j));
                }
                if ((ia == 0 || ia == 12) && (ib == 0 || ib == 12)) {
                    warnings.push_back("parallel octave between voices " + std::to_string(i) + " and " + std::to_string(j));
                }
            }
        }
    }
    return warnings;
}

int nearestPitchWithClass(int target, int pitchClass, int low, int high) {
    int best = std::clamp(target, low, high);
    int bestDistance = std::numeric_limits<int>::max();
    for (int p = std::max(0, low); p <= std::min(127, high); ++p) {
        if (mod12(p) != mod12(pitchClass)) continue;
        const int d = std::abs(p - target);
        if (d < bestDistance) { bestDistance = d; best = p; }
    }
    return best;
}

int nearestPitchInSet(int target, const std::vector<int>& pitchClasses, int low, int high) {
    if (pitchClasses.empty()) return std::clamp(target, low, high);
    int best = std::clamp(target, low, high);
    int bestDistance = std::numeric_limits<int>::max();
    for (int p = std::max(0, low); p <= std::min(127, high); ++p) {
        bool match = false;
        for (int pc : pitchClasses) if (mod12(pc) == mod12(p)) { match = true; break; }
        if (!match) continue;
        const int d = std::abs(p - target);
        if (d < bestDistance) { bestDistance = d; best = p; }
    }
    return best;
}

bool isChordTone(int midi, const std::vector<int>& pitchClasses) {
    for (int pc : pitchClasses) if (mod12(pc) == mod12(midi)) return true;
    return false;
}

std::string pitchClassName(int pitchClass, bool preferFlats) {
    return (preferFlats ? kFlatNames : kSharpNames)[static_cast<std::size_t>(mod12(pitchClass))];
}

std::string romanNumeral(const Key& key, int degree, ChordQuality quality, int inversion) {
    static const std::array<const char*, 7> upper{"I", "II", "III", "IV", "V", "VI", "VII"};
    static const std::array<const char*, 7> lower{"i", "ii", "iii", "iv", "v", "vi", "vii"};
    const int idx = ((degree % 7) + 7) % 7;
    std::string out = isMinorish(quality) ? lower[static_cast<std::size_t>(idx)] : upper[static_cast<std::size_t>(idx)];
    switch (quality) {
        case ChordQuality::Diminished: out += "°"; break;
        case ChordQuality::HalfDiminished7: out += "ø7"; break;
        case ChordQuality::Augmented: out += "+"; break;
        case ChordQuality::Dominant7: out += "7"; break;
        case ChordQuality::Major7: out += "maj7"; break;
        case ChordQuality::Minor7: out += "7"; break;
        case ChordQuality::MinorMajor7: out += "maj7"; break;
        case ChordQuality::Sus2: out += "sus2"; break;
        case ChordQuality::Sus4: out += "sus4"; break;
        case ChordQuality::Dominant7Sus4: out += "7sus4"; break;
        default: break;
    }
    if (inversion == 1) out += "6";
    else if (inversion >= 2) out += "/" + std::to_string(inversion + 1);
    (void)key;
    return out;
}

float chordTension(const std::vector<int>& pitches, int rootPc) {
    if (pitches.empty()) return 0.0f;
    float tension = 0.0f;
    for (std::size_t i = 0; i < pitches.size(); ++i) {
        for (std::size_t j = i + 1; j < pitches.size(); ++j) {
            const int interval = mod12(pitches[j] - pitches[i]);
            switch (interval) {
                case 1: case 11: tension += 0.80f; break;
                case 2: case 10: tension += 0.30f; break;
                case 3: case 9: tension += 0.12f; break;
                case 5: case 7: tension += 0.05f; break;
                case 6: tension += 0.65f; break;
                default: break;
            }
            // Low-register minor seconds and tritones are perceptually rough.
            if (pitches[i] < 52 && (interval == 1 || interval == 6 || interval == 11)) tension += 0.35f;
        }
    }
    const float rootBass = mod12(pitches.front() - rootPc) == 0 ? 0.0f : 0.1f;
    (void)rootBass;
    const float norm = static_cast<float>(std::max<std::size_t>(1, pitches.size() * (pitches.size() - 1) / 2));
    return std::clamp(tension / norm, 0.0f, 1.0f);
}

CounterpointReport checkVoicings(const std::vector<std::vector<int>>& voicings) {
    CounterpointReport report;
    for (const auto& v : voicings) {
        for (std::size_t i = 1; i < v.size(); ++i) if (v[i] <= v[i - 1]) ++report.voiceCrossings;
        for (std::size_t i = 1; i < v.size(); ++i) {
            const int gap = v[i] - v[i - 1];
            if (gap > (i == 1 ? 19 : 12)) ++report.excessiveSpacing;
        }
    }
    for (std::size_t c = 1; c < voicings.size(); ++c) {
        const auto& a = voicings[c - 1];
        const auto& b = voicings[c];
        const std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                const int ia = mod12(a[j] - a[i]);
                const int ib = mod12(b[j] - b[i]);
                const int da = b[i] - a[i];
                const int db = b[j] - a[j];
                if (da == 0 || db == 0 || ((da > 0) != (db > 0))) continue;
                if (ia == 7 && ib == 7) ++report.parallelFifths;
                if ((ia == 0) && (ib == 0)) ++report.parallelOctaves;
            }
        }
    }
    return report;
}

} // namespace ec
