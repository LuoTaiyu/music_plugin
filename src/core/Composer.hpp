#pragma once

#include "core/Expression.hpp"
#include "core/Theory.hpp"
#include "core/Types.hpp"

#include <string>
#include <vector>

namespace ec {

enum class DemoStyle { Chamber, Cinematic, Kinetic };

struct GenerationSpec {
    std::string title;
    DemoStyle style = DemoStyle::Chamber;
    std::uint64_t seed = 1;
    int bars = 0; // 0 = style default
    double bpm = 0.0;
    TimeSignature timeSignature;
    Key key;
    Intent intent;
    ExpressionCurves curves;
    bool useExplicitCurves = false;
    bool humanize = true;
    float humanizeAmount = 1.0f;
};

struct GenerationDiagnostics {
    double generationMs = 0.0;
    int noteCount = 0;
    int chordCount = 0;
    int phraseCount = 0;
    int motifUseCount = 0;
    CounterpointReport counterpoint;
    std::vector<std::string> decisions;
    std::vector<std::string> warnings;
};

struct RegenerationOptions {
    std::uint64_t seed = 1;
    LockDimension locks = LockDimension::None;
    float variation = 0.5f;
    int rangeStartBar = 0;
    int rangeEndBar = -1; // exclusive; -1 = whole project
};

GenerationSpec demoSpec(DemoStyle style, std::uint64_t seed = 1);
GenerationSpec specFromProject(const Project& project, std::uint64_t seed);

Project composeProject(const GenerationSpec& spec, GenerationDiagnostics* diagnostics = nullptr);

// Regenerates part of a project while respecting locked dimensions. This is a
// merge over a freshly generated candidate, not a destructive edit.
Project regenerateProject(const Project& base,
                          const RegenerationOptions& options,
                          GenerationDiagnostics* diagnostics = nullptr);

// Deterministic, phrase-aware performance shaping. It is separated from
// composition so a user can lock composition and regenerate only expression.
void humanizeProject(Project& project, float amount = 1.0f);
void exposeProjectMetadata(Project& project);

std::string_view demoId(DemoStyle style);
DemoStyle demoStyleFromId(std::string_view id);

} // namespace ec
