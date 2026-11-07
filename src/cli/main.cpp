#include "core/Composer.hpp"
#include "core/ProjectIO.hpp"
#include "core/TimeMap.hpp"
#include "io/MidiFile.hpp"
#include "render/Renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ec;

struct Args {
    std::string command;
    std::map<std::string, std::string> options;
    std::vector<std::string> positional;

    bool has(const std::string& key) const { return options.find(key) != options.end(); }
    std::string get(const std::string& key, const std::string& fallback = {}) const {
        const auto it = options.find(key);
        return it == options.end() ? fallback : it->second;
    }
    int getInt(const std::string& key, int fallback) const {
        const auto it = options.find(key);
        if (it == options.end()) return fallback;
        try { return std::stoi(it->second); } catch (...) { return fallback; }
    }
    double getDouble(const std::string& key, double fallback) const {
        const auto it = options.find(key);
        if (it == options.end()) return fallback;
        try { return std::stod(it->second); } catch (...) { return fallback; }
    }
    std::uint64_t getSeed(std::uint64_t fallback) const {
        const auto it = options.find("seed");
        if (it == options.end()) return fallback;
        try { return std::stoull(it->second); } catch (...) { return fallback; }
    }
};

Args parseArgs(int argc, char** argv) {
    Args args;
    if (argc <= 1) {
        args.command = "help";
        return args;
    }
    args.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string token = argv[i];
        if (token.rfind("--", 0) == 0) {
            token = token.substr(2);
            const auto eq = token.find('=');
            if (eq != std::string::npos) {
                args.options[token.substr(0, eq)] = token.substr(eq + 1);
            } else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
                args.options[token] = argv[++i];
            } else {
                args.options[token] = "true";
            }
        } else {
            args.positional.push_back(token);
        }
    }
    return args;
}

bool loadProject(const std::string& path, Project& p, std::string& error) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) {
        error = "cannot open project: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return deserializeProject(ss.str(), p, &error);
}

void ensureParent(const std::string& path) {
    const auto fsPath = std::filesystem::u8path(path);
    if (fsPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(fsPath.parent_path(), ec);
    }
}

bool writeText(const std::string& path, const std::string& text, std::string& error) {
    ensureParent(path);
    std::ofstream out(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot write: " + path;
        return false;
    }
    out << text;
    if (!out) {
        error = "write failed: " + path;
        return false;
    }
    return true;
}

std::string formatDouble(double value, int precision = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

std::string secondsToClock(double seconds) {
    const int total = static_cast<int>(std::llround(seconds));
    std::ostringstream ss;
    ss << (total / 60) << ":" << std::setw(2) << std::setfill('0') << (total % 60);
    return ss.str();
}

std::string pageHeader() {
    return "Expressive Composer 0.1.0 — deterministic composition core with editable MIDI\n";
}

void printHelp() {
    std::cout << pageHeader()
              << "\nUsage:\n"
              << "  ec_cli demos\n"
              << "  ec_cli generate --demo chamber|cinematic|kinetic|all [--seed N] [--out DIR]\n"
              << "                    [--no-wav] [--sr 44100] [--bits 24]\n"
              << "  ec_cli render project.json --wav out.wav [--sr 48000] [--block 512] [--bits 24]\n"
              << "  ec_cli export-midi project.json --out out.mid\n"
              << "  ec_cli import-midi in.mid --out project.json [--standard]\n"
              << "  ec_cli inspect project.json\n"
              << "  ec_cli validate project.json\n"
              << "  ec_cli bench [--iterations 5]\n"
              << "  ec_cli version\n"
              << "\nThis CLI is the test entry point. It does not require a DAW or JUCE.\n";
}

int commandDemos() {
    std::cout << "chamber     " << "克制、怀念的室内器乐（F minor, 4/4, 68 BPM, 48 bars）\n";
    std::cout << "cinematic   " << "紧张逐步累积的电影配乐（D phrygian, 5/4, 92 BPM, 44 bars）\n";
    std::cout << "kinetic     " << "节奏驱动的电子律动（E dorian, 4/4, 122 BPM, 36 bars）\n";
    return 0;
}

int commandGenerate(const Args& args) {
    const std::string which = args.get("demo", "all");
    const std::string outDir = args.get("out", "presets/demos");
    const std::uint64_t seed = args.getSeed(20261108);
    const bool renderWav = !args.has("no-wav");
    const int sr = args.getInt("sr", 44100);
    const int bits = args.getInt("bits", 24);
    const int block = args.getInt("block", 512);
    std::vector<DemoStyle> styles;
    if (which == "chamber" || which == "all") styles.push_back(DemoStyle::Chamber);
    if (which == "cinematic" || which == "all") styles.push_back(DemoStyle::Cinematic);
    if (which == "kinetic" || which == "all") styles.push_back(DemoStyle::Kinetic);
    if (styles.empty()) {
        std::cerr << "unknown demo: " << which << "\n";
        return 2;
    }
    int failures = 0;
    for (auto style : styles) {
        const auto id = std::string(demoId(style));
        const auto spec = demoSpec(style, seed);
        GenerationDiagnostics diag;
        const auto started = std::chrono::steady_clock::now();
        const Project project = composeProject(spec, &diag);
        const auto composeMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        const std::string dir = outDir + "/" + id;
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
        const std::string jsonPath = dir + "/project.json";
        const std::string midiPath = dir + "/" + id + ".mid";
        const std::string wavPath = dir + "/" + id + ".wav";
        std::string error;
        if (!writeText(jsonPath, serializeProject(project, 2), error)) {
            std::cerr << error << "\n";
            ++failures;
            continue;
        }
        MidiExportStats midiStats;
        if (!exportMidiFile(project, midiPath, {}, &midiStats, &error)) {
            std::cerr << error << "\n";
            ++failures;
            continue;
        }
        RenderStats renderStats;
        bool rendered = false;
        if (renderWav) {
            RenderOptions ro;
            ro.sampleRate = sr;
            ro.blockSize = block;
            ro.bitsPerSample = bits;
            ro.tailSeconds = 3.0f;
            rendered = renderProjectToWav(project, wavPath, ro, &renderStats, &error);
            if (!rendered) {
                std::cerr << "render failed: " << error << "\n";
                ++failures;
            }
        }
        int notes = 0;
        for (const auto& t : project.tracks) notes += static_cast<int>(t.notes.size());
        std::ostringstream report;
        report << "Expressive Composer demo report\n"
               << "id=" << id << "\n"
               << "title=" << project.title << "\n"
               << "seed=" << project.seed << "\n"
               << "bars=" << project.totalBars() << "\n"
               << "timeSignature=" << project.timeSignature.numerator << "/" << project.timeSignature.denominator << "\n"
               << "key=" << keyName(project.key, true) << "\n"
               << "bpm=" << project.tempoMap.front().bpm << "\n"
               << "notes=" << notes << "\n"
               << "chords=" << project.chords.size() << "\n"
               << "phrases=" << project.phrases.size() << "\n"
               << "motifUses=" << project.motifUses.size() << "\n"
               << "composeMs=" << formatDouble(composeMs, 3) << "\n"
               << "midiBytes=" << midiStats.bytes << "\n";
        if (rendered) {
            report << "audioSeconds=" << formatDouble(renderStats.audioSeconds, 3) << "\n"
                   << "renderMs=" << formatDouble(renderStats.renderMs, 3) << "\n"
                   << "realtimeFactor=" << formatDouble(renderStats.audioSeconds * 1000.0 / std::max(0.001, renderStats.renderMs), 3) << "x\n"
                   << "peak=" << formatDouble(renderStats.peak, 6) << "\n"
                   << "rms=" << formatDouble(renderStats.rms, 6) << "\n"
                   << "clippedSamples={" << renderStats.clippedSamples << "}\n"
                   << "allFinite={" << (renderStats.allFinite ? "true" : "false") << "}\n"
                   << "maxVoices={" << renderStats.maxVoices << "}\n";
        }
        report << "\n[decisions]\n";
        for (const auto& decision : diag.decisions) report << "- " << decision << "\n";
        report << "\n[warnings]\n";
        if (diag.warnings.empty()) report << "- none\n";
        for (const auto& warning : diag.warnings) report << "- " << warning << "\n";
        const std::string reportPath = dir + "/report.txt";
        if (!writeText(reportPath, report.str(), error)) {
            std::cerr << error << "\n";
            ++failures;
        }
        std::cout << id << ": " << project.title << " | bars=" << project.totalBars()
                  << " notes=" << notes << " compose=" << formatDouble(composeMs, 2) << "ms";
        if (rendered) {
            std::cout << " audio=" << secondsToClock(renderStats.audioSeconds)
                      << " rt=" << formatDouble(renderStats.audioSeconds * 1000.0 / std::max(0.001, renderStats.renderMs), 2) << "x"
                      << " peak=" << formatDouble(renderStats.peak, 3);
        }
        std::cout << " -> " << dir << "\n";
    }
    return failures == 0 ? 0 : 1;
}

int commandRender(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "render requires project.json\n";
        return 2;
    }
    Project p;
    std::string error;
    if (!loadProject(args.positional.front(), p, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    const std::string out = args.get("wav", "render.wav");
    RenderOptions options;
    options.sampleRate = args.getInt("sr", 48000);
    options.blockSize = args.getInt("block", 512);
    options.bitsPerSample = args.getInt("bits", 24);
    options.tailSeconds = static_cast<float>(args.getDouble("tail", 3.0));
    options.masterGainDb = static_cast<float>(args.getDouble("gain", 0.0));
    options.normalize = args.has("normalize");
    RenderStats stats;
    if (!renderProjectToWav(p, out, options, &stats, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    std::cout << "wrote " << out << " frames=" << stats.frames
              << " seconds=" << formatDouble(stats.audioSeconds, 3)
              << " renderMs=" << formatDouble(stats.renderMs, 3)
              << " peak=" << formatDouble(stats.peak, 5)
              << " rms=" << formatDouble(stats.rms, 5)
              << " clipped=" << stats.clippedSamples
              << " finite=" << (stats.allFinite ? "true" : "false") << "\n";
    return 0;
}

int commandExportMidi(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "export-midi requires project.json\n";
        return 2;
    }
    Project p;
    std::string error;
    if (!loadProject(args.positional.front(), p, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    const std::string out = args.get("out", "project.mid");
    MidiExportStats stats;
    if (!exportMidiFile(p, out, {}, &stats, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    std::cout << "wrote " << out << " tracks=" << stats.tracks << " notes=" << stats.notes
              << " tempoEvents=" << stats.tempoEvents << " markers=" << stats.markers
              << " bytes=" << stats.bytes << "\n";
    return 0;
}

int commandImportMidi(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "import-midi requires a .mid file\n";
        return 2;
    }
    const bool standard = args.has("standard");
    auto result = importMidiFile(args.positional.front(), !standard);
    if (!result.success) {
        std::cerr << result.error << "\n";
        return 1;
    }
    for (const auto& warning : result.warnings) std::cerr << "warning: " << warning << "\n";
    const std::string out = args.get("out", "imported_project.json");
    std::string error;
    if (!writeText(out, serializeProject(result.project, 2), error)) {
        std::cerr << error << "\n";
        return 1;
    }
    std::cout << "imported " << args.positional.front() << " -> " << out
              << " tracks=" << result.project.tracks.size()
              << " notes=" << result.noteCount
              << " embedded=" << (result.usedEmbeddedProject ? "yes" : "no") << "\n";
    return 0;
}

int commandInspect(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "inspect requires project.json\n";
        return 2;
    }
    Project p;
    std::string error;
    if (!loadProject(args.positional.front(), p, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    const TimeMap tm(p.tempoMap, p.ppq);
    int notes = 0;
    for (const auto& t : p.tracks) notes += static_cast<int>(t.notes.size());
    std::cout << p.title << "\n"
              << "style=" << p.styleId << " seed=" << p.seed << "\n"
              << "key=" << keyName(p.key, true) << " meter=" << p.timeSignature.numerator << "/"
              << p.timeSignature.denominator << " bpm=" << p.tempoMap.front().bpm
              << " bars=" << p.totalBars() << " duration=" << secondsToClock(tm.secondsAtTick(p.totalTicks())) << "\n"
              << "tracks=" << p.tracks.size() << " notes=" << notes << " chords=" << p.chords.size()
              << " phrases=" << p.phrases.size() << " motifs=" << p.motifs.size()
              << " motifUses=" << p.motifUses.size() << "\n\n";
    std::cout << "[sections]\n";
    for (const auto& s : p.sections) {
        std::cout << "  " << s.startBar << "-" << (s.startBar + s.bars) << "  " << s.name
                  << "  v=" << formatDouble(s.target.valence, 2)
                  << " a=" << formatDouble(s.target.arousal, 2)
                  << " t=" << formatDouble(s.target.tension, 2)
                  << " d=" << formatDouble(s.target.distance, 2) << "\n";
    }
    std::cout << "\n[tracks]\n";
    for (const auto& t : p.tracks) {
        int lo = 127, hi = 0;
        for (const auto& n : t.notes) { lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); }
        std::cout << "  " << t.id << "  " << t.name << "  " << toString(t.role) << " / "
                  << toString(t.instrument) << "  notes=" << t.notes.size();
        if (!t.notes.empty()) std::cout << " range=" << lo << "-" << hi;
        std::cout << " pan=" << formatDouble(t.pan, 2) << " send=" << formatDouble(t.reverbSend, 2) << "\n";
    }
    std::cout << "\n[chords]\n";
    for (const auto& c : p.chords) {
        std::cout << "  bar " << (c.start / ticksPerBar(p.timeSignature, p.ppq) + 1)
                  << "  " << c.symbol << "  " << c.function
                  << "  tension=" << formatDouble(c.tension, 2);
        if (!c.pitches.empty()) {
            std::cout << "  voicing=";
            for (int pitch : c.pitches) std::cout << pitch << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n[motifs]\n";
    for (const auto& m : p.motifs) {
        std::cout << "  " << m.id << "  " << m.name << "  notes=" << m.notes.size() << "  " << m.description << "\n";
    }
    std::cout << "\n[automation]\n";
    for (const auto& lane : p.automation) {
        std::cout << "  " << lane.id << " -> " << lane.target << " points=" << lane.points.size() << "\n";
    }
    return 0;
}

int commandValidate(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "validate requires project.json\n";
        return 2;
    }
    Project p;
    std::string error;
    if (!loadProject(args.positional.front(), p, error)) {
        std::cerr << "invalid: " << error << "\n";
        return 1;
    }
    int noteCount = 0;
    for (const auto& t : p.tracks) noteCount += static_cast<int>(t.notes.size());
    std::cout << "valid schemaVersion=" << p.schemaVersion << " tracks=" << p.tracks.size()
              << " notes=" << noteCount << " chords=" << p.chords.size() << "\n";
    return 0;
}

int commandBench(const Args& args) {
    const int iterations = std::clamp(args.getInt("iterations", 5), 1, 50);
    for (auto style : {DemoStyle::Chamber, DemoStyle::Cinematic, DemoStyle::Kinetic}) {
        std::vector<double> times;
        int notes = 0;
        for (int i = 0; i < iterations; ++i) {
            const auto spec = demoSpec(style, 1234 + static_cast<std::uint64_t>(i));
            const auto start = std::chrono::steady_clock::now();
            const Project p = composeProject(spec);
            times.push_back(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count());
            notes = 0;
            for (const auto& t : p.tracks) notes += static_cast<int>(t.notes.size());
        }
        std::sort(times.begin(), times.end());
        const double median = times[times.size() / 2];
        std::cout << demoId(style) << ": notes=" << notes
                  << " compose_min_ms=" << formatDouble(times.front(), 3)
                  << " median_ms=" << formatDouble(median, 3)
                  << " max_ms=" << formatDouble(times.back(), 3) << "\n";
    }
    const Project p = composeProject(demoSpec(DemoStyle::Chamber, 1234));
    RenderOptions options;
    options.sampleRate = 48000;
    options.blockSize = 512;
    options.tailSeconds = 0.2f;
    std::vector<float> buffer;
    RenderStats stats;
    std::string error;
    if (!renderProjectToBuffer(p, options, buffer, &stats, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    std::cout << "render_48k_512: audio_s=" << formatDouble(stats.audioSeconds, 2)
              << " render_ms=" << formatDouble(stats.renderMs, 2)
              << " realtime_factor=" << formatDouble(stats.audioSeconds * 1000.0 / std::max(0.001, stats.renderMs), 2)
              << "x peak=" << formatDouble(stats.peak, 4)
              << " rms=" << formatDouble(stats.rms, 4)
              << " max_voices=" << stats.maxVoices << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    if (args.command == "help" || args.command == "--help" || args.command == "-h") {
        printHelp();
        return 0;
    }
    if (args.command == "version") {
        std::cout << "Expressive Composer 0.1.0\n";
        return 0;
    }
    if (args.command == "demos") return commandDemos();
    if (args.command == "generate") return commandGenerate(args);
    if (args.command == "render") return commandRender(args);
    if (args.command == "export-midi") return commandExportMidi(args);
    if (args.command == "import-midi") return commandImportMidi(args);
    if (args.command == "inspect") return commandInspect(args);
    if (args.command == "validate") return commandValidate(args);
    if (args.command == "bench") return commandBench(args);
    std::cerr << "unknown command: " << args.command << "\n";
    printHelp();
    return 2;
}
