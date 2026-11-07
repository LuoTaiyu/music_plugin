#pragma once

#include "core/Json.hpp"
#include "core/Types.hpp"

#include <string>

namespace ec {

inline constexpr int kProjectSchemaVersion = 1;

Json projectToJson(const Project& project);
bool projectFromJson(const Json& root, Project& out, std::string* error = nullptr);
std::string serializeProject(const Project& project, int indent = 2);
bool deserializeProject(std::string_view text, Project& out, std::string* error = nullptr);

// Validates and normalises a project in place. Returns false with a diagnostic
// when the data cannot be made safe for rendering.
bool validateProject(Project& project, std::string* error = nullptr);

} // namespace ec
