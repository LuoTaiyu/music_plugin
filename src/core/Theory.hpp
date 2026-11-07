#pragma once

#include "core/Types.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ec {

enum class ChordQuality {
    Major,
    Minor,
    Diminished,
    Augmented,
    Dominant7,
    Major7,
    Minor7,
    HalfDiminished7,
    MinorMajor7,
    Sus2,
    Sus4,
    Dominant7Sus4
};

struct Scale {
    Key key;
    std::array<int, 7> intervals{};
    std::array<int, 7> pitchClasses{};

    int pitchClassAt(int degree) const noexcept;
    int midiAt(int degree, int tonicMidi) const noexcept;
    bool contains(int pitchClass) const noexcept;
    int nearestMember(int midi, int low = 0, int high = 127) const noexcept;
};

Scale makeScale(const Key& key);
std::string keyName(const Key& key, bool preferFlats = true);

std::vector<int> chordIntervals(ChordQuality quality);
std::vector<int> chordPitchClasses(const Key& key,
                                   int degree,
                                   ChordQuality quality,
                                   bool addNinth = false,
                                   bool addEleventh = false);
std::string chordSymbol(const Key& key,
                        int rootPc,
                        ChordQuality quality,
                        bool addNinth,
                        int inversion = 0,
                        bool preferFlats = true);

std::vector<int> voiceChord(const std::vector<int>& previousVoicing,
                            const std::vector<int>& pitchClasses,
                            int bassPitchClass,
                            int low = 48,
                            int high = 79,
                            int voices = 4,
                            int center = 64);

std::vector<std::string> parallelPerfectWarnings(const std::vector<std::vector<int>>& voicings);

int nearestPitchWithClass(int target, int pitchClass, int low = 0, int high = 127);
int nearestPitchInSet(int target, const std::vector<int>& pitchClasses, int low = 0, int high = 127);
bool isChordTone(int midi, const std::vector<int>& pitchClasses);
std::string pitchClassName(int pitchClass, bool preferFlats = true);
std::string romanNumeral(const Key& key, int degree, ChordQuality quality, int inversion = 0);
float chordTension(const std::vector<int>& pitches, int rootPc);

// Basic species-style checks used by tests and the composer's diagnostics.
struct CounterpointReport {
    int parallelFifths = 0;
    int parallelOctaves = 0;
    int voiceCrossings = 0;
    int excessiveSpacing = 0;
    bool acceptable() const noexcept { return parallelFifths == 0 && parallelOctaves == 0 && voiceCrossings == 0; }
};
CounterpointReport checkVoicings(const std::vector<std::vector<int>>& voicings);

} // namespace ec
