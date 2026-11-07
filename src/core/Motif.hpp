#pragma once

#include "core/Types.hpp"

#include <string>
#include <string_view>

namespace ec {

enum class MotifTransform {
    Original,
    Transpose,
    Invert,
    Retrograde,
    Augment,
    Diminish,
    Fragment,
    Extend,
    ChangeEnding,
    Ornament
};

std::string_view toString(MotifTransform t);
MotifTransform motifTransformFromString(std::string_view s);

Motif transformMotif(const Motif& source, MotifTransform transform, int parameter = 0);
Motif cloneMotifWithId(const Motif& source, std::string id, std::string name);

// Small, style-specific cells. They are intentionally sparse enough to be
// remembered and transformed, rather than generated fresh at every phrase.
Motif chamberMotif();
Motif cinematicMotif();
Motif kineticMotif();

} // namespace ec
