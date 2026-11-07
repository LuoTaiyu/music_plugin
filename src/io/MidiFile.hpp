#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ec {

struct MidiExportOptions {
    bool includeEmbeddedProject = true; // ECP1 sequencer-specific meta event
    bool includeExpressionCc = true;
    bool includeSectionMarkers = true;
};

struct MidiExportStats {
    int tracks = 0;
    int notes = 0;
    int tempoEvents = 0;
    int markers = 0;
    std::size_t bytes = 0;
};

struct MidiImportResult {
    Project project;
    std::vector<std::string> warnings;
    int trackCount = 0;
    int noteCount = 0;
    bool usedEmbeddedProject = false;
    bool success = false;
    std::string error;
};

bool writeMidiToMemory(const Project& project,
                       const MidiExportOptions& options,
                       std::vector<std::uint8_t>& out,
                       MidiExportStats* stats = nullptr,
                       std::string* error = nullptr);

bool exportMidiFile(const Project& project,
                    const std::string& path,
                    const MidiExportOptions& options = {},
                    MidiExportStats* stats = nullptr,
                    std::string* error = nullptr);

MidiImportResult readMidiFromMemory(const std::vector<std::uint8_t>& bytes,
                                    bool preferEmbeddedProject = true);

MidiImportResult importMidiFile(const std::string& path,
                                bool preferEmbeddedProject = true);

} // namespace ec
