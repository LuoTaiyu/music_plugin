#include "io/MidiFile.hpp"

#include "core/Expression.hpp"
#include "core/ProjectIO.hpp"
#include "core/Rng.hpp"
#include "core/TimeMap.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <queue>
#include <string>
#include <utility>

namespace ec {
namespace {

constexpr std::uint8_t kMeta = 0xFF;
constexpr std::uint8_t kSysex = 0xF0;
constexpr std::uint8_t kSysexEnd = 0xF7;

int beatsPerBar(const TimeSignature& ts) {
    return std::max(1, ts.numerator * 4 / std::max(1, ts.denominator));
}

void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void putVarLen(std::vector<std::uint8_t>& out, std::uint64_t value) {
    std::uint8_t buffer[10]{};
    int count = 0;
    buffer[count++] = static_cast<std::uint8_t>(value & 0x7F);
    while ((value >>= 7) != 0) {
        buffer[count++] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
    }
    while (count > 0) out.push_back(buffer[--count]);
}

std::uint16_t readU16(const std::vector<std::uint8_t>& in, std::size_t pos) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[pos]) << 8) | in[pos + 1]);
}

std::uint32_t readU32(const std::vector<std::uint8_t>& in, std::size_t pos) {
    return (static_cast<std::uint32_t>(in[pos]) << 24) |
           (static_cast<std::uint32_t>(in[pos + 1]) << 16) |
           (static_cast<std::uint32_t>(in[pos + 2]) << 8) |
           static_cast<std::uint32_t>(in[pos + 3]);
}

struct MidiEvent {
    Tick tick = 0;
    int order = 0;
    std::uint64_t sequence = 0;
    std::vector<std::uint8_t> data;
};

void addMetaEvent(std::vector<MidiEvent>& events, Tick tick, int order, std::uint8_t type,
                  const std::vector<std::uint8_t>& payload, std::uint64_t& sequence) {
    MidiEvent e;
    e.tick = tick;
    e.order = order;
    e.sequence = sequence++;
    e.data = {kMeta, type};
    putVarLen(e.data, payload.size());
    e.data.insert(e.data.end(), payload.begin(), payload.end());
    events.push_back(std::move(e));
}

void addMetaText(std::vector<MidiEvent>& events, Tick tick, std::uint8_t type, const std::string& text,
                 std::uint64_t& sequence) {
    addMetaEvent(events, tick, 0, type,
                 std::vector<std::uint8_t>(text.begin(), text.end()), sequence);
}

std::vector<std::uint8_t> makeTrackChunk(const std::vector<MidiEvent>& input) {
    auto events = input;
    std::sort(events.begin(), events.end(), [](const MidiEvent& a, const MidiEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        if (a.order != b.order) return a.order < b.order;
        return a.sequence < b.sequence;
    });
    std::vector<std::uint8_t> body;
    Tick previous = 0;
    for (const auto& e : events) {
        const Tick delta = std::max<Tick>(0, e.tick - previous);
        putVarLen(body, static_cast<std::uint64_t>(delta));
        body.insert(body.end(), e.data.begin(), e.data.end());
        previous = std::max(previous, e.tick);
    }
    putVarLen(body, 0);
    body.push_back(kMeta);
    body.push_back(0x2F);
    body.push_back(0x00);

    std::vector<std::uint8_t> chunk;
    chunk.insert(chunk.end(), {'M', 'T', 'r', 'k'});
    putU32(chunk, static_cast<std::uint32_t>(body.size()));
    chunk.insert(chunk.end(), body.begin(), body.end());
    return chunk;
}

int gmProgramFor(Instrument instrument) {
    switch (instrument) {
        case Instrument::FeltPiano: return 0;
        case Instrument::BowedString: return 48;
        case Instrument::GlassMallet: return 9;
        case Instrument::OrganicBass: return 38;
        case Instrument::DrumKit: return 0;
    }
    return 0;
}

Instrument instrumentForProgram(int program, int channel) {
    if (channel == 10) return Instrument::DrumKit;
    if (program >= 0 && program <= 7) return Instrument::FeltPiano;
    if (program >= 8 && program <= 15) return Instrument::GlassMallet;
    if (program >= 32 && program <= 39) return Instrument::OrganicBass;
    if (program >= 40 && program <= 51) return Instrument::BowedString;
    return Instrument::FeltPiano;
}

Role roleFromTrackName(const std::string& name, Instrument instrument, int channel) {
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (channel == 10 || instrument == Instrument::DrumKit) return Role::Percussion;
    if (lower.find("bass") != std::string::npos || lower.find("cello") != std::string::npos)
        return Role::Bass;
    if (lower.find("drum") != std::string::npos || lower.find("perc") != std::string::npos)
        return Role::Percussion;
    if (lower.find("pad") != std::string::npos || lower.find("harm") != std::string::npos)
        return Role::Harmony;
    if (lower.find("counter") != std::string::npos) return Role::Counter;
    return Role::Melody;
}

struct ParsedEvent {
    Tick tick = 0;
    std::uint8_t status = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;
    std::uint8_t metaType = 0;
    std::vector<std::uint8_t> payload;
    bool isMeta = false;
};

struct ParsedTrack {
    std::vector<ParsedEvent> events;
    std::string name;
    std::vector<std::uint8_t> embeddedProject;
    bool hasEmbeddedProject = false;
};

std::uint64_t readVarLen(const std::vector<std::uint8_t>& in, std::size_t& pos) {
    std::uint64_t value = 0;
    int count = 0;
    while (pos < in.size() && count < 4) {
        const std::uint8_t b = in[pos++];
        value = (value << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) break;
        ++count;
    }
    return value;
}

bool parseTrack(const std::vector<std::uint8_t>& in, std::size_t begin, std::size_t end, ParsedTrack& out,
                std::string& error) {
    std::size_t pos = begin;
    Tick tick = 0;
    std::uint8_t running = 0;
    std::uint64_t sequence = 0;
    while (pos < end) {
        const std::uint64_t delta = readVarLen(in, pos);
        tick += static_cast<Tick>(delta);
        if (pos >= end) { error = "truncated MIDI event"; return false; }
        std::uint8_t status = in[pos];
        bool usedRunning = false;
        if (status < 0x80) {
            if (running == 0) { error = "running status without a previous status byte"; return false; }
            status = running;
            usedRunning = true;
        } else {
            ++pos;
            if (status < 0xF0) running = status;
            else running = 0;
        }
        ParsedEvent ev;
        ev.tick = tick;
        ev.status = status;
        if (status == kMeta) {
            if (pos >= end) { error = "truncated meta event"; return false; }
            ev.isMeta = true;
            ev.metaType = in[pos++];
            const std::uint64_t len = readVarLen(in, pos);
            if (pos + len > end) { error = "meta event exceeds track length"; return false; }
            ev.payload.assign(in.begin() + static_cast<std::ptrdiff_t>(pos),
                              in.begin() + static_cast<std::ptrdiff_t>(pos + len));
            pos += static_cast<std::size_t>(len);
            if (ev.metaType == 0x03 && out.name.empty()) out.name.assign(ev.payload.begin(), ev.payload.end());
            if (ev.metaType == 0x7F && ev.payload.size() > 4 &&
                std::memcmp(ev.payload.data(), "ECP1", 4) == 0) {
                out.embeddedProject.assign(ev.payload.begin() + 4, ev.payload.end());
                out.hasEmbeddedProject = true;
            }
        } else if (status == kSysex || status == kSysexEnd) {
            const std::uint64_t len = readVarLen(in, pos);
            if (pos + len > end) { error = "sysex event exceeds track length"; return false; }
            pos += static_cast<std::size_t>(len);
            continue;
        } else if (status >= 0xF0) {
            // System common/realtime messages are not part of our model. Skip
            // their known data bytes and continue rather than desynchronising.
            if (status == 0xF2) pos = std::min(end, pos + 2);
            else if (status == 0xF1 || status == 0xF3) pos = std::min(end, pos + 1);
            else if (status == 0xF6 || status == 0xF8 || status == 0xFA || status == 0xFB ||
                     status == 0xFC || status == 0xFE) {
                // no data
            } else {
                error = "unsupported system message 0x" + std::to_string(status);
                return false;
            }
            continue;
        } else {
            const std::uint8_t type = status & 0xF0;
            if (usedRunning) {
                // status byte was not consumed; first data byte is at pos.
            }
            auto readData = [&](std::uint8_t& target) -> bool {
                if (pos >= end) return false;
                target = in[pos++];
                return true;
            };
            if (!readData(ev.data1)) { error = "truncated channel event"; return false; }
            if (type != 0xC0 && type != 0xD0) {
                if (!readData(ev.data2)) { error = "truncated channel event"; return false; }
            }
            (void)sequence;
        }
        out.events.push_back(std::move(ev));
    }
    return true;
}

std::vector<std::uint8_t> readFileBytes(const std::string& path, std::string& error) {
    std::error_code ec;
    const auto fsPath = std::filesystem::u8path(path);
    std::ifstream in(fsPath, std::ios::binary);
    if (!in) {
        error = "cannot open " + path;
        return {};
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size < 0) {
        error = "cannot size " + path;
        return {};
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in && !in.eof()) {
        error = "read failure: " + path;
        return {};
    }
    (void)ec;
    return bytes;
}

std::uint32_t denominatorPower(int denominator) {
    int power = 0;
    int value = 1;
    while (value < denominator && power < 8) {
        value *= 2;
        ++power;
    }
    return static_cast<std::uint32_t>(power);
}

std::vector<std::uint8_t> encodeTempo(double bpm) {
    const double micros = 60'000'000.0 / std::clamp(bpm, 10.0, 600.0);
    const auto value = static_cast<std::uint32_t>(std::llround(micros));
    return {static_cast<std::uint8_t>((value >> 16) & 0xFF),
            static_cast<std::uint8_t>((value >> 8) & 0xFF),
            static_cast<std::uint8_t>(value & 0xFF)};
}

} // namespace

bool writeMidiToMemory(const Project& project,
                       const MidiExportOptions& options,
                       std::vector<std::uint8_t>& out,
                       MidiExportStats* stats,
                       std::string* error) {
    if (project.ppq < 24 || project.ppq > 9600) {
        if (error) *error = "invalid PPQ";
        return false;
    }
    Project p = project;
    std::string validationError;
    if (!validateProject(p, &validationError)) {
        if (error) *error = validationError;
        return false;
    }
    out.clear();
    out.insert(out.end(), {'M', 'T', 'h', 'd'});
    putU32(out, 6);
    putU16(out, 1);
    putU16(out, static_cast<std::uint16_t>(p.tracks.size() + 1));
    putU16(out, static_cast<std::uint16_t>(p.ppq));

    const TimeMap timeMap(p.tempoMap, p.ppq);
    const Tick barTicks = ticksPerBar(p.timeSignature, p.ppq);
    MidiExportStats localStats;

    // Conductor track.
    {
        std::vector<MidiEvent> events;
        std::uint64_t seq = 1;
        addMetaText(events, 0, 0x03, "Expressive Composer — conductor", seq);
        addMetaText(events, 0, 0x01, "TITLE: " + p.title, seq);
        addMetaText(events, 0, 0x01, "Expressive Composer / deterministic composition core", seq);
        std::vector<std::uint8_t> tsPayload{
            static_cast<std::uint8_t>(p.timeSignature.numerator),
            static_cast<std::uint8_t>(denominatorPower(p.timeSignature.denominator)),
            24, 8};
        addMetaEvent(events, 0, 1, 0x58, tsPayload, seq);
        for (const auto& tempo : p.tempoMap) {
            addMetaEvent(events, tempo.tick, 1, 0x51, encodeTempo(tempo.bpm), seq);
            ++localStats.tempoEvents;
        }
        if (options.includeSectionMarkers) {
            for (const auto& section : p.sections) {
                addMetaText(events, static_cast<Tick>(section.startBar) * barTicks, 0x06,
                            section.name, seq);
                ++localStats.markers;
            }
        }
        for (const auto& e : p.narrative) {
            if (!e.audible) continue;
            addMetaText(events, static_cast<Tick>(std::llround(e.bar * ticksPerBar(p.timeSignature, p.ppq))),
                        0x01, e.type + ": " + e.description, seq);
        }
        if (options.includeEmbeddedProject) {
            const std::string json = serializeProject(p, 0);
            std::vector<std::uint8_t> payload{'E', 'C', 'P', '1'};
            payload.insert(payload.end(), json.begin(), json.end());
            addMetaEvent(events, 0, 2, 0x7F, payload, seq);
        }
        const auto chunk = makeTrackChunk(events);
        out.insert(out.end(), chunk.begin(), chunk.end());
    }

    for (const auto& track : p.tracks) {
        std::vector<MidiEvent> events;
        std::uint64_t seq = 1;
        addMetaText(events, 0, 0x03, track.name, seq);
        const int channel = std::clamp(track.midiChannel, 1, 16);
        const int program = gmProgramFor(track.instrument);
        events.push_back({0, 1, seq++, {static_cast<std::uint8_t>(0xC0 | ((channel - 1) & 0x0F)),
                                        static_cast<std::uint8_t>(program)}});
        const int volume = std::clamp(static_cast<int>(std::lround(100.0 + track.gainDb)), 0, 127);
        events.push_back({0, 1, seq++, {static_cast<std::uint8_t>(0xB0 | ((channel - 1) & 0x0F)), 7,
                                        static_cast<std::uint8_t>(volume)}});
        events.push_back({0, 1, seq++, {static_cast<std::uint8_t>(0xB0 | ((channel - 1) & 0x0F)), 10,
                                        static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround((track.pan + 1.0f) * 63.5f)), 0, 127))}});
        events.push_back({0, 1, seq++, {static_cast<std::uint8_t>(0xB0 | ((channel - 1) & 0x0F)), 91,
                                        static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(track.reverbSend * 127.0f)), 0, 127))}});

        for (const auto& note : track.notes) {
            if (note.muted) continue;
            const double bpm = timeMap.bpmAtTick(note.start);
            const Tick microTicks = static_cast<Tick>(std::llround(
                static_cast<double>(note.microMs) * bpm * static_cast<double>(p.ppq) / 60000.0));
            Tick onTick = std::max<Tick>(0, note.start + microTicks);
            Tick offTick = onTick + std::max<Tick>(1, note.duration);
            const int noteChannel = std::clamp(note.channel, 1, 16) - 1;
            const auto statusOn = static_cast<std::uint8_t>(0x90 | (noteChannel & 0x0F));
            const auto statusOff = static_cast<std::uint8_t>(0x80 | (noteChannel & 0x0F));
            const auto velocity = static_cast<std::uint8_t>(std::clamp(note.velocity, 1, 127));
            events.push_back({onTick, 4, seq++, {statusOn, static_cast<std::uint8_t>(std::clamp(note.pitch, 0, 127)), velocity}});
            events.push_back({offTick, 2, seq++, {statusOff, static_cast<std::uint8_t>(std::clamp(note.pitch, 0, 127)), 0}});
            if (options.includeExpressionCc) {
                const auto ccStatus = static_cast<std::uint8_t>(0xB0 | (noteChannel & 0x0F));
                events.push_back({onTick, 3, seq++, {ccStatus, 11,
                    static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(note.velocity * 1.15)), 0, 127))}});
                events.push_back({onTick, 3, seq++, {ccStatus, 74,
                    static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(note.brightness * 127.0f)), 0, 127))}});
                events.push_back({onTick, 3, seq++, {ccStatus, 10,
                    static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround((note.pan + 1.0f) * 63.5f)), 0, 127))}});
            }
            ++localStats.notes;
        }
        const auto chunk = makeTrackChunk(events);
        out.insert(out.end(), chunk.begin(), chunk.end());
        ++localStats.tracks;
    }

    localStats.bytes = out.size();
    if (stats) *stats = localStats;
    return true;
}

bool exportMidiFile(const Project& project,
                    const std::string& path,
                    const MidiExportOptions& options,
                    MidiExportStats* stats,
                    std::string* error) {
    std::vector<std::uint8_t> bytes;
    if (!writeMidiToMemory(project, options, bytes, stats, error)) return false;
    const auto fsPath = std::filesystem::u8path(path);
    std::error_code ec;
    if (fsPath.has_parent_path()) {
        std::filesystem::create_directories(fsPath.parent_path(), ec);
    }
    std::ofstream out(fsPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "cannot open for writing: " + path;
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        if (error) *error = "write failed: " + path;
        return false;
    }
    return true;
}

MidiImportResult readMidiFromMemory(const std::vector<std::uint8_t>& bytes,
                                    bool preferEmbeddedProject) {
    MidiImportResult result;
    if (bytes.size() < 14 || std::memcmp(bytes.data(), "MThd", 4) != 0) {
        result.error = "not a Standard MIDI File (missing MThd)";
        return result;
    }
    const std::uint32_t headerLength = readU32(bytes, 4);
    if (headerLength < 6 || bytes.size() < 8 + headerLength) {
        result.error = "invalid MThd length";
        return result;
    }
    const std::uint16_t format = readU16(bytes, 8);
    const std::uint16_t declaredTracks = readU16(bytes, 10);
    const std::uint16_t division = readU16(bytes, 12);
    if (format > 2) {
        result.error = "unsupported MIDI format " + std::to_string(format);
        return result;
    }
    if (division & 0x8000) {
        result.error = "SMPTE time division is not supported; use PPQ";
        return result;
    }
    if (division == 0) {
        result.error = "MIDI division must be non-zero";
        return result;
    }

    std::vector<ParsedTrack> tracks;
    std::size_t pos = 8 + headerLength;
    while (pos + 8 <= bytes.size() && tracks.size() < declaredTracks) {
        if (std::memcmp(bytes.data() + pos, "MTrk", 4) != 0) {
            result.error = "expected MTrk at byte " + std::to_string(pos);
            return result;
        }
        const std::uint32_t length = readU32(bytes, pos + 4);
        pos += 8;
        if (pos + length > bytes.size()) {
            result.error = "truncated MTrk chunk";
            return result;
        }
        ParsedTrack track;
        std::string parseError;
        if (!parseTrack(bytes, pos, pos + length, track, parseError)) {
            result.error = parseError;
            return result;
        }
        tracks.push_back(std::move(track));
        pos += length;
    }
    if (tracks.empty()) {
        result.error = "no MTrk chunks";
        return result;
    }
    result.trackCount = static_cast<int>(tracks.size());

    // Prefer the explicit, versioned project payload when present and valid,
    // but only if it agrees with the standard note count in the file.
    if (preferEmbeddedProject) {
        for (const auto& track : tracks) {
            if (!track.hasEmbeddedProject) continue;
            Project payload;
            std::string error;
            const std::string json(track.embeddedProject.begin(), track.embeddedProject.end());
            if (deserializeProject(json, payload, &error)) {
                int payloadNotes = 0;
                for (const auto& t : payload.tracks) payloadNotes += static_cast<int>(t.notes.size());
                int midiNotes = 0;
                for (const auto& tr : tracks) {
                    for (const auto& e : tr.events) {
                        if (!e.isMeta && (e.status & 0xF0) == 0x90 && e.data2 > 0) ++midiNotes;
                    }
                }
                if (payloadNotes == midiNotes) {
                    result.project = std::move(payload);
                    result.noteCount = payloadNotes;
                    result.usedEmbeddedProject = true;
                    result.success = true;
                    result.warnings.push_back("embedded ECP1 project payload restored; standard MIDI was cross-checked");
                    return result;
                }
                result.warnings.push_back("ECP1 payload note count does not match MIDI; standard import used");
            } else {
                result.warnings.push_back("ECP1 payload exists but cannot be parsed: " + error);
            }
        }
    }

    Project p;
    p.schemaVersion = kProjectSchemaVersion;
    p.title = "Imported MIDI";
    p.styleId = "imported";
    p.seed = 1;
    p.ppq = division;
    p.timeSignature = {4, 4};
    p.key = {0, Mode::Major};
    p.tempoMap.clear();
    p.sections.clear();
    p.narrative.clear();
    p.tracks.clear();
    p.chords.clear();
    p.phrases.clear();
    p.motifs.clear();
    p.motifUses.clear();
    p.automation.clear();
    p.curves = defaultExpressionCurves("chamber");
    p.intent = defaultIntent("chamber");
    p.intent.styleBrief = "imported MIDI; key and harmony were not inferred";

    struct Marker { Tick tick; std::string name; };
    std::vector<Marker> markers;
    for (const auto& track : tracks) {
        for (const auto& e : track.events) {
            if (!e.isMeta) continue;
            if (e.metaType == 0x51 && e.payload.size() >= 3) {
                const std::uint32_t micros = (static_cast<std::uint32_t>(e.payload[0]) << 16) |
                                             (static_cast<std::uint32_t>(e.payload[1]) << 8) |
                                             static_cast<std::uint32_t>(e.payload[2]);
                if (micros > 0) p.tempoMap.push_back({e.tick, 60'000'000.0 / static_cast<double>(micros)});
            } else if (e.metaType == 0x58 && e.payload.size() >= 2) {
                const int numerator = e.payload[0];
                const int denominator = 1 << std::min<int>(e.payload[1], 6);
                if (numerator > 0) p.timeSignature = {numerator, denominator};
            } else if (e.metaType == 0x06) {
                markers.push_back({e.tick, std::string(e.payload.begin(), e.payload.end())});
            } else if (e.metaType == 0x01) {
                const std::string text(e.payload.begin(), e.payload.end());
                if (text.rfind("TITLE: ", 0) == 0) p.title = text.substr(7);
            } else if (e.metaType == 0x03 && !track.name.empty() && p.title == "Imported MIDI") {
                const bool conductor = track.name.find("conductor") != std::string::npos ||
                                       track.name.find("Conductor") != std::string::npos;
                if (!conductor) p.title = track.name;
            }
            if (e.metaType == 0x03 && !e.payload.empty() && track.name.empty()) {
                // handled by parser
            }
        }
    }
    if (p.tempoMap.empty()) {
        p.tempoMap.push_back({0, 120.0});
        result.warnings.push_back("no tempo event found; assumed 120 BPM");
    }
    std::stable_sort(p.tempoMap.begin(), p.tempoMap.end(), [](const TempoPoint& a, const TempoPoint& b) {
        return a.tick < b.tick;
    });
    if (p.tempoMap.front().tick != 0) p.tempoMap.insert(p.tempoMap.begin(), {0, p.tempoMap.front().bpm});

    const Tick barTicks = ticksPerBar(p.timeSignature, p.ppq);
    Tick maxTick = 0;
    int projectTrackId = 1;
    for (const auto& parsed : tracks) {
        struct Pending {
            Tick start;
            int velocity;
            std::uint8_t cc11;
            std::uint8_t cc74;
            std::uint8_t cc10;
        };
        std::map<int, std::queue<Pending>> pending;
        Track outTrack;
        outTrack.id = projectTrackId++;
        outTrack.name = parsed.name.empty() ? ("Track " + std::to_string(outTrack.id)) : parsed.name;
        outTrack.role = Role::Melody;
        outTrack.instrument = Instrument::FeltPiano;
        outTrack.midiChannel = 1;
        outTrack.reverbSend = 0.25f;
        std::uint8_t cc11 = 100, cc74 = 64, cc10 = 64;
        int program = 0;
        int channel = 1;
        int noteCount = 0;
        for (const auto& e : parsed.events) {
            if (e.isMeta) continue;
            const int statusType = e.status & 0xF0;
            const int eventChannel = (e.status & 0x0F) + 1;
            if (statusType == 0xC0) {
                program = e.data1;
                if (noteCount == 0) outTrack.instrument = instrumentForProgram(program, eventChannel);
            } else if (statusType == 0xB0) {
                if (e.data1 == 11) cc11 = e.data2;
                else if (e.data1 == 74) cc74 = e.data2;
                else if (e.data1 == 10) cc10 = e.data2;
            } else if (statusType == 0x90 && e.data2 > 0) {
                channel = eventChannel;
                pending[eventChannel * 128 + e.data1].push({e.tick, e.data2, cc11, cc74, cc10});
            } else if (statusType == 0x80 || (statusType == 0x90 && e.data2 == 0)) {
                const int key = eventChannel * 128 + e.data1;
                auto it = pending.find(key);
                if (it == pending.end() || it->second.empty()) {
                    result.warnings.push_back("unmatched note-off at tick " + std::to_string(e.tick));
                    continue;
                }
                const auto start = it->second.front();
                it->second.pop();
                Note note;
                note.id = p.allocateNoteId();
                note.start = start.start;
                note.duration = std::max<Tick>(1, e.tick - start.start);
                note.pitch = e.data1;
                note.velocity = start.velocity;
                note.channel = eventChannel;
                note.brightness = static_cast<float>(start.cc74) / 127.0f;
                note.legato = 0.55f;
                note.pan = static_cast<float>(start.cc10) / 63.5f - 1.0f;
                (void)start.cc11;
                outTrack.notes.push_back(note);
                ++noteCount;
                maxTick = std::max(maxTick, note.start + note.duration);
            }
        }
        for (auto& [key, queue] : pending) {
            while (!queue.empty()) {
                const auto start = queue.front();
                queue.pop();
                Note note;
                note.id = p.allocateNoteId();
                note.start = start.start;
                note.duration = p.ppq;
                note.pitch = key % 128;
                note.velocity = start.velocity;
                note.channel = channel;
                note.brightness = static_cast<float>(start.cc74) / 127.0f;
                note.pan = static_cast<float>(start.cc10) / 63.5f - 1.0f;
                outTrack.notes.push_back(note);
                ++noteCount;
                maxTick = std::max(maxTick, note.start + note.duration);
                result.warnings.push_back("open note closed at +1 quarter at end of track");
            }
        }
        if (noteCount == 0) continue;
        outTrack.midiChannel = channel;
        outTrack.instrument = instrumentForProgram(program, channel);
        outTrack.role = roleFromTrackName(outTrack.name, outTrack.instrument, channel);
        std::stable_sort(outTrack.notes.begin(), outTrack.notes.end(), [](const Note& a, const Note& b) {
            return a.start < b.start || (a.start == b.start && a.pitch < b.pitch);
        });
        result.noteCount += noteCount;
        p.tracks.push_back(std::move(outTrack));
    }

    if (p.tracks.empty()) {
        result.error = "MIDI contains no playable notes";
        return result;
    }

    std::stable_sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
        return a.tick < b.tick;
    });
    if (!markers.empty()) {
        for (std::size_t i = 0; i < markers.size(); ++i) {
            const Tick start = markers[i].tick;
            const Tick end = i + 1 < markers.size() ? markers[i + 1].tick : std::max(maxTick, start + barTicks);
            const int startBar = static_cast<int>(start / barTicks);
            const int endBar = std::max(startBar + 1, static_cast<int>((end + barTicks - 1) / barTicks));
            Section section;
            section.name = markers[i].name.empty() ? ("Section " + std::to_string(i + 1)) : markers[i].name;
            section.role = i == 0 ? "setup" : (i + 1 == markers.size() ? "coda" : "development");
            section.startBar = startBar;
            section.bars = endBar - startBar;
            section.target = p.curves.at(static_cast<double>(startBar) * beatsPerBar(p.timeSignature));
            p.sections.push_back(section);
            p.narrative.push_back({static_cast<double>(startBar), "marker", section.name, true});
        }
    } else {
        Section section;
        section.name = "Imported MIDI";
        section.role = "development";
        section.startBar = 0;
        section.bars = std::max(1, static_cast<int>((maxTick + barTicks - 1) / barTicks));
        section.target = p.curves.at(0.0);
        p.sections.push_back(section);
        result.warnings.push_back("no section markers found; created one section");
    }
    p.revisionNote = "Imported from Standard MIDI File";
    result.warnings.push_back("key, chord symbols and motifs are not inferred from MIDI; edit or regenerate them explicitly");
    if (!validateProject(p, &result.error)) return result;
    result.project = std::move(p);
    result.success = true;
    result.usedEmbeddedProject = false;
    return result;
}

MidiImportResult importMidiFile(const std::string& path, bool preferEmbeddedProject) {
    MidiImportResult result;
    std::string error;
    const auto bytes = readFileBytes(path, error);
    if (!error.empty()) {
        result.error = error;
        return result;
    }
    return readMidiFromMemory(bytes, preferEmbeddedProject);
}

} // namespace ec
