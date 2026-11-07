#pragma once

#include "core/Types.hpp"

#include <string>
#include <vector>

namespace ec {

// Musical affordances are deliberately multidimensional. A single emotion label
// never selects a mode, tempo or timbre on its own.
struct Affordances {
    float density = 0.5f;              // events per bar, relative
    float restProbability = 0.18f;
    float leapProbability = 0.25f;
    float syncopation = 0.2f;
    float harmonicTension = 0.3f;
    double harmonicRhythmBeats = 2.0;
    int registerCenter = 60;
    int registerSpan = 14;
    float velocityBase = 0.66f;
    float velocityRange = 0.22f;
    float articulation = 0.6f;         // legato tendency
    float brightness = 0.5f;           // excitation spectrum
    float noise = 0.3f;
    float release = 0.5f;
    float reverbSend = 0.3f;
    float stereoWidth = 0.4f;
    float highFrequencyDamping = 0.25f;
    float dissonanceBudget = 0.2f;
    int layerBudget = 2;
    bool unresolvedEnding = true;
};

struct StyleBias {
    float density = 0.0f;
    float rest = 0.0f;
    float leap = 0.0f;
    float syncopation = 0.0f;
    float tension = 0.0f;
    float brightness = 0.0f;
    float reverb = 0.0f;
    int registerCenter = 60;
    int registerSpan = 14;
    double harmonicRhythm = 2.0;
};

class ExpressionMapper {
public:
    // All mappings are bounded, continuous heuristics. See
    // THEORY_AND_PSYCHOLOGY.md for evidence labels and limitations.
    static Affordances derive(const AffectState& state,
                              Restraint restraint,
                              Role role,
                              const StyleBias& style = {});

    // Human-readable account of the actual mappings used for a state.
    static std::vector<std::string> explain(const AffectState& state,
                                            Restraint restraint,
                                            Role role,
                                            const StyleBias& style = {});
};

ExpressionCurves defaultExpressionCurves(const std::string& styleId);
Intent defaultIntent(const std::string& styleId);

} // namespace ec
