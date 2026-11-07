#include "core/ProjectIO.hpp"

#include "core/Expression.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace ec {
namespace {

Json pointToJson(const CurvePoint& p) {
    Json j = Json::object();
    j.set("beat", p.beat);
    j.set("value", p.value);
    return j;
}

CurvePoint pointFromJson(const Json& j) {
    return {j["beat"].asDouble(0.0), static_cast<float>(j["value"].asDouble(0.0))};
}

Json curveToJson(const Curve& c) {
    Json arr = Json::array();
    for (const auto& p : c.points) arr.push(pointToJson(p));
    return arr;
}

Curve curveFromJson(const Json& j) {
    Curve c;
    for (const auto& p : j.arrayItems()) c.points.push_back(pointFromJson(p));
    c.sort();
    return c;
}

Json affectToJson(const AffectState& s) {
    Json j = Json::object();
    j.set("valence", s.valence);
    j.set("arousal", s.arousal);
    j.set("tension", s.tension);
    j.set("distance", s.distance);
    return j;
}

AffectState affectFromJson(const Json& j) {
    AffectState s;
    s.valence = static_cast<float>(j["valence"].asDouble(0.0));
    s.arousal = static_cast<float>(j["arousal"].asDouble(0.35));
    s.tension = static_cast<float>(j["tension"].asDouble(0.25));
    s.distance = static_cast<float>(j["distance"].asDouble(0.35));
    return s;
}

Json noteToJson(const Note& n) {
    Json j = Json::object();
    j.set("id", static_cast<std::uint64_t>(n.id));
    j.set("start", static_cast<std::int64_t>(n.start));
    j.set("duration", static_cast<std::int64_t>(n.duration));
    j.set("pitch", n.pitch);
    j.set("velocity", n.velocity);
    j.set("channel", n.channel);
    j.set("pan", n.pan);
    j.set("brightness", n.brightness);
    j.set("attack", n.attack);
    j.set("noise", n.noise);
    j.set("release", n.release);
    j.set("microMs", n.microMs);
    j.set("legato", n.legato);
    j.set("muted", n.muted);
    if (n.motifInstance) j.set("motifInstance", static_cast<std::uint64_t>(n.motifInstance));
    if (n.phrase >= 0) j.set("phrase", n.phrase);
    return j;
}

Note noteFromJson(const Json& j) {
    Note n;
    n.id = static_cast<std::uint32_t>(j["id"].asInt(0));
    n.start = static_cast<Tick>(j["start"].asInt(0));
    n.duration = static_cast<Tick>(j["duration"].asInt(kPPQ));
    n.pitch = static_cast<int>(j["pitch"].asInt(60));
    n.velocity = static_cast<int>(j["velocity"].asInt(90));
    n.channel = static_cast<int>(j["channel"].asInt(1));
    n.pan = static_cast<float>(j["pan"].asDouble(0.0));
    n.brightness = static_cast<float>(j["brightness"].asDouble(0.5));
    n.attack = static_cast<float>(j["attack"].asDouble(0.5));
    n.noise = static_cast<float>(j["noise"].asDouble(0.35));
    n.release = static_cast<float>(j["release"].asDouble(0.5));
    n.microMs = static_cast<float>(j["microMs"].asDouble(0.0));
    n.legato = static_cast<float>(j["legato"].asDouble(0.6));
    n.muted = j["muted"].asBool(false);
    n.motifInstance = static_cast<std::uint32_t>(j["motifInstance"].asInt(0));
    n.phrase = static_cast<int>(j["phrase"].asInt(-1));
    return n;
}

Json narrativeToJson(const NarrativeEvent& e) {
    Json j = Json::object();
    j.set("bar", e.bar);
    j.set("type", e.type);
    j.set("description", e.description);
    j.set("audible", e.audible);
    return j;
}

NarrativeEvent narrativeFromJson(const Json& j) {
    NarrativeEvent e;
    e.bar = j["bar"].asDouble(0.0);
    e.type = j["type"].asString("event");
    e.description = j["description"].asString();
    e.audible = j["audible"].asBool(true);
    return e;
}

std::uint64_t parseSeed(const Json& j) {
    if (j.isString()) {
        try { return std::stoull(j.asString("1")); } catch (...) { return 1; }
    }
    if (j.isNumber()) return static_cast<std::uint64_t>(std::max(0.0, j.asDouble(1.0)));
    return 1;
}

Json emptyObject() { return Json::object(); }

bool migrateLegacy(const Json& root, Project& p, std::string* error) {
    // Documented legacy sketch format from early prototypes. This is a real
    // migration path, not a placeholder: it preserves notes, pitch and velocity.
    const bool legacy = root["format"].asString() == "EC_SKETCH_0" || root["schemaVersion"].asInt(1) == 0;
    if (!legacy) return false;
    p.schemaVersion = kProjectSchemaVersion;
    p.title = root["title"].asString("Legacy sketch");
    p.seed = root["seed"].isString() ? parseSeed(root["seed"]) : root["seed"].asInt(1);
    p.styleId = "chamber";
    p.timeSignature = {4, 4};
    p.tempoMap = {{0, root["bpm"].asDouble(72.0)}};
    p.key.tonic = static_cast<int>(root["key"]["tonic"].asInt(5));
    p.key.mode = modeFromString(root["key"]["mode"].asString("natural_minor"));
    p.revisionNote = "Migrated from EC_SKETCH_0";
    Section s;
    s.name = "Imported sketch";
    s.startBar = 0;
    s.bars = std::max(1, static_cast<int>(root["bars"].asInt(8)));
    s.target = {0.0f, 0.35f, 0.3f, 0.35f};
    p.sections.push_back(s);
    p.narrative.push_back({0.0, "import", "legacy sketch imported as a single section", true});
    int trackId = 1;
    for (const auto& tj : root["tracks"].arrayItems()) {
        Track t;
        t.id = trackId++;
        t.name = tj["name"].asString("Imported track");
        t.role = Role::Melody;
        t.instrument = Instrument::FeltPiano;
        t.midiChannel = std::clamp(static_cast<int>(tj["channel"].asInt(1)), 1, 16);
        for (const auto& nj : tj["notes"].arrayItems()) {
            Note n;
            n.id = p.allocateNoteId();
            n.start = static_cast<Tick>(nj["t"].asInt(0));
            n.duration = std::max<Tick>(1, static_cast<Tick>(nj["d"].asInt(kPPQ)));
            n.pitch = static_cast<int>(nj["p"].asInt(60));
            n.velocity = static_cast<int>(nj["v"].asInt(90));
            n.channel = t.midiChannel;
            t.notes.push_back(n);
        }
        p.tracks.push_back(std::move(t));
    }
    if (p.tracks.empty()) p.tracks.push_back({1, "Piano", Role::Melody, Instrument::FeltPiano, 1, 0.0f, 0.0f, 0.25f});
    p.curves = defaultExpressionCurves("chamber");
    p.narrative = p.narrative;
    (void)error;
    return true;
}

} // namespace

Json projectToJson(const Project& p) {
    Json root = Json::object();
    root.set("schemaVersion", p.schemaVersion);
    root.set("application", "Expressive Composer");
    root.set("title", p.title);
    root.set("styleId", p.styleId);
    root.set("revisionNote", p.revisionNote);
    root.set("seed", std::to_string(p.seed));
    root.set("ppq", p.ppq);
    root.set("timeSignature", Json::object().set("numerator", p.timeSignature.numerator)
                                              .set("denominator", p.timeSignature.denominator));
    Json tempo = Json::array();
    for (const auto& t : p.tempoMap) {
        tempo.push(Json::object().set("tick", static_cast<std::int64_t>(t.tick)).set("bpm", t.bpm));
    }
    root.set("tempoMap", std::move(tempo));
    root.set("key", Json::object().set("tonic", p.key.tonic).set("mode", std::string(toString(p.key.mode))));

    Json intent = Json::object();
    intent.set("perspective", std::string(toString(p.intent.perspective)));
    intent.set("scene", std::string(toString(p.intent.scene)));
    intent.set("restraint", std::string(toString(p.intent.restraint)));
    intent.set("culture", p.intent.culture);
    intent.set("styleBrief", p.intent.styleBrief);
    intent.set("references", p.intent.references);
    Json prohibitions = Json::array();
    for (const auto& s : p.intent.prohibitions) prohibitions.push(s);
    intent.set("prohibitions", std::move(prohibitions));
    intent.set("lyricOrText", p.intent.lyricOrText);
    intent.set("sourceMaterialNote", p.intent.sourceMaterialNote);
    Json intentNarrative = Json::array();
    for (const auto& e : p.intent.narrative) intentNarrative.push(narrativeToJson(e));
    intent.set("narrative", std::move(intentNarrative));
    root.set("intent", std::move(intent));

    Json curves = Json::object();
    curves.set("valence", curveToJson(p.curves.valence));
    curves.set("arousal", curveToJson(p.curves.arousal));
    curves.set("tension", curveToJson(p.curves.tension));
    curves.set("distance", curveToJson(p.curves.distance));
    root.set("curves", std::move(curves));

    Json narrative = Json::array();
    for (const auto& e : p.narrative) narrative.push(narrativeToJson(e));
    root.set("narrative", std::move(narrative));

    Json sections = Json::array();
    for (const auto& s : p.sections) {
        Json j = Json::object();
        j.set("name", s.name);
        j.set("role", s.role);
        j.set("description", s.description);
        j.set("startBar", s.startBar);
        j.set("bars", s.bars);
        j.set("target", affectToJson(s.target));
        sections.push(std::move(j));
    }
    root.set("sections", std::move(sections));

    Json tracks = Json::array();
    for (const auto& t : p.tracks) {
        Json j = Json::object();
        j.set("id", t.id);
        j.set("name", t.name);
        j.set("role", std::string(toString(t.role)));
        j.set("instrument", std::string(toString(t.instrument)));
        j.set("midiChannel", t.midiChannel);
        j.set("gainDb", t.gainDb);
        j.set("pan", t.pan);
        j.set("reverbSend", t.reverbSend);
        j.set("muted", t.muted);
        j.set("solo", t.solo);
        Json notes = Json::array();
        for (const auto& n : t.notes) notes.push(noteToJson(n));
        j.set("notes", std::move(notes));
        tracks.push(std::move(j));
    }
    root.set("tracks", std::move(tracks));

    Json chords = Json::array();
    for (const auto& c : p.chords) {
        Json j = Json::object();
        j.set("start", static_cast<std::int64_t>(c.start));
        j.set("duration", static_cast<std::int64_t>(c.duration));
        j.set("symbol", c.symbol);
        j.set("rootPc", c.rootPc);
        Json pitches = Json::array();
        for (int pitch : c.pitches) pitches.push(pitch);
        j.set("pitches", std::move(pitches));
        j.set("tension", c.tension);
        j.set("inversion", c.inversion);
        j.set("function", c.function);
        chords.push(std::move(j));
    }
    root.set("chords", std::move(chords));

    Json phrases = Json::array();
    for (const auto& ph : p.phrases) {
        phrases.push(Json::object()
                         .set("section", ph.section)
                         .set("start", static_cast<std::int64_t>(ph.start))
                         .set("end", static_cast<std::int64_t>(ph.end))
                         .set("function", ph.function)
                         .set("peakNoteId", static_cast<std::uint64_t>(ph.peakNoteId))
                         .set("intensity", ph.intensity)
                         .set("breathAtEnd", ph.breathAtEnd));
    }
    root.set("phrases", std::move(phrases));

    Json motifs = Json::array();
    for (const auto& m : p.motifs) {
        Json j = Json::object();
        j.set("id", m.id);
        j.set("name", m.name);
        j.set("description", m.description);
        j.set("baseOctave", m.baseOctave);
        Json notes = Json::array();
        for (const auto& n : m.notes) {
            notes.push(Json::object().set("degree", n.degree)
                                        .set("duration", static_cast<std::int64_t>(n.duration))
                                        .set("accent", n.accent));
        }
        j.set("notes", std::move(notes));
        motifs.push(std::move(j));
    }
    root.set("motifs", std::move(motifs));

    Json uses = Json::array();
    for (const auto& u : p.motifUses) {
        uses.push(Json::object().set("motifId", u.motifId)
                                    .set("instanceId", static_cast<std::uint64_t>(u.instanceId))
                                    .set("start", static_cast<std::int64_t>(u.start))
                                    .set("end", static_cast<std::int64_t>(u.end))
                                    .set("trackId", u.trackId)
                                    .set("transformation", u.transformation));
    }
    root.set("motifUses", std::move(uses));

    Json automation = Json::array();
    for (const auto& lane : p.automation) {
        Json j = Json::object();
        j.set("id", lane.id);
        j.set("target", lane.target);
        j.set("unit", lane.unit);
        Json pts = Json::array();
        for (const auto& pt : lane.points) pts.push(Json::object().set("beat", pt.beat).set("value", pt.value));
        j.set("points", std::move(pts));
        automation.push(std::move(j));
    }
    root.set("automation", std::move(automation));
    root.set("nextNoteId", static_cast<std::uint64_t>(p.nextNoteId));
    root.set("nextMotifInstance", static_cast<std::uint64_t>(p.nextMotifInstance));
    return root;
}

bool projectFromJson(const Json& root, Project& out, std::string* error) {
    auto setError = [&](const std::string& e) {
        if (error) *error = e;
        return false;
    };
    if (!root.isObject()) return setError("project root must be a JSON object");
    Project p;
    if (migrateLegacy(root, p, error)) {
        if (!validateProject(p, error)) return false;
        out = std::move(p);
        return true;
    }

    p.schemaVersion = static_cast<int>(root["schemaVersion"].asInt(kProjectSchemaVersion));
    if (p.schemaVersion > kProjectSchemaVersion) {
        return setError("project schema is newer than this build supports: " + std::to_string(p.schemaVersion));
    }
    p.title = root["title"].asString("Untitled");
    p.styleId = root["styleId"].asString("chamber");
    p.revisionNote = root["revisionNote"].asString();
    p.seed = parseSeed(root["seed"]);
    p.ppq = static_cast<int>(root["ppq"].asInt(kPPQ));
    p.timeSignature.numerator = static_cast<int>(root["timeSignature"]["numerator"].asInt(4));
    p.timeSignature.denominator = static_cast<int>(root["timeSignature"]["denominator"].asInt(4));
    p.tempoMap.clear();
    for (const auto& j : root["tempoMap"].arrayItems()) {
        p.tempoMap.push_back({static_cast<Tick>(j["tick"].asInt(0)), j["bpm"].asDouble(120.0)});
    }
    if (p.tempoMap.empty()) p.tempoMap.push_back({0, 120.0});
    p.key.tonic = static_cast<int>(root["key"]["tonic"].asInt(0));
    p.key.mode = modeFromString(root["key"]["mode"].asString("natural_minor"));

    const auto& intent = root["intent"];
    p.intent.perspective = perspectiveFromString(intent["perspective"].asString("monologue"));
    p.intent.scene = sceneFromString(intent["scene"].asString("intimate_room"));
    p.intent.restraint = restraintFromString(intent["restraint"].asString("restrained"));
    p.intent.culture = intent["culture"].asString("Western tonal (explicit default assumption)");
    p.intent.styleBrief = intent["styleBrief"].asString();
    p.intent.references = intent["references"].asString();
    for (const auto& j : intent["prohibitions"].arrayItems()) p.intent.prohibitions.push_back(j.asString());
    p.intent.lyricOrText = intent["lyricOrText"].asString();
    p.intent.sourceMaterialNote = intent["sourceMaterialNote"].asString();
    for (const auto& j : intent["narrative"].arrayItems()) p.intent.narrative.push_back(narrativeFromJson(j));

    p.curves.valence = curveFromJson(root["curves"]["valence"]);
    p.curves.arousal = curveFromJson(root["curves"]["arousal"]);
    p.curves.tension = curveFromJson(root["curves"]["tension"]);
    p.curves.distance = curveFromJson(root["curves"]["distance"]);
    if (p.curves.valence.points.empty()) p.curves = defaultExpressionCurves(p.styleId);

    for (const auto& j : root["narrative"].arrayItems()) p.narrative.push_back(narrativeFromJson(j));

    for (const auto& j : root["sections"].arrayItems()) {
        Section s;
        s.name = j["name"].asString("Section");
        s.role = j["role"].asString("development");
        s.description = j["description"].asString();
        s.startBar = static_cast<int>(j["startBar"].asInt(0));
        s.bars = static_cast<int>(j["bars"].asInt(8));
        s.target = affectFromJson(j["target"]);
        p.sections.push_back(std::move(s));
    }

    for (const auto& j : root["tracks"].arrayItems()) {
        Track t;
        t.id = static_cast<int>(j["id"].asInt(static_cast<std::int64_t>(p.tracks.size() + 1)));
        t.name = j["name"].asString("Track");
        t.role = roleFromString(j["role"].asString("melody"));
        t.instrument = instrumentFromString(j["instrument"].asString("felt_piano"));
        t.midiChannel = static_cast<int>(j["midiChannel"].asInt(1));
        t.gainDb = static_cast<float>(j["gainDb"].asDouble(0.0));
        t.pan = static_cast<float>(j["pan"].asDouble(0.0));
        t.reverbSend = static_cast<float>(j["reverbSend"].asDouble(0.3));
        t.muted = j["muted"].asBool(false);
        t.solo = j["solo"].asBool(false);
        for (const auto& nj : j["notes"].arrayItems()) t.notes.push_back(noteFromJson(nj));
        p.tracks.push_back(std::move(t));
    }

    for (const auto& j : root["chords"].arrayItems()) {
        ChordEvent c;
        c.start = static_cast<Tick>(j["start"].asInt(0));
        c.duration = static_cast<Tick>(j["duration"].asInt(kPPQ * 4));
        c.symbol = j["symbol"].asString();
        c.rootPc = static_cast<int>(j["rootPc"].asInt(0));
        for (const auto& pj : j["pitches"].arrayItems()) c.pitches.push_back(static_cast<int>(pj.asInt(60)));
        c.tension = static_cast<float>(j["tension"].asDouble(0.2));
        c.inversion = static_cast<int>(j["inversion"].asInt(0));
        c.function = j["function"].asString();
        p.chords.push_back(std::move(c));
    }

    for (const auto& j : root["phrases"].arrayItems()) {
        Phrase ph;
        ph.section = static_cast<int>(j["section"].asInt(-1));
        ph.start = static_cast<Tick>(j["start"].asInt(0));
        ph.end = static_cast<Tick>(j["end"].asInt(0));
        ph.function = j["function"].asString();
        ph.peakNoteId = static_cast<std::uint32_t>(j["peakNoteId"].asInt(0));
        ph.intensity = static_cast<float>(j["intensity"].asDouble(0.5));
        ph.breathAtEnd = j["breathAtEnd"].asBool(true);
        p.phrases.push_back(std::move(ph));
    }

    for (const auto& j : root["motifs"].arrayItems()) {
        Motif m;
        m.id = j["id"].asString();
        m.name = j["name"].asString();
        m.description = j["description"].asString();
        m.baseOctave = static_cast<int>(j["baseOctave"].asInt(4));
        for (const auto& nj : j["notes"].arrayItems()) {
            m.notes.push_back({static_cast<int>(nj["degree"].asInt(0)),
                               static_cast<Tick>(nj["duration"].asInt(kPPQ / 2)),
                               static_cast<float>(nj["accent"].asDouble(1.0))});
        }
        p.motifs.push_back(std::move(m));
    }

    for (const auto& j : root["motifUses"].arrayItems()) {
        MotifUse u;
        u.motifId = j["motifId"].asString();
        u.instanceId = static_cast<std::uint32_t>(j["instanceId"].asInt(0));
        u.start = static_cast<Tick>(j["start"].asInt(0));
        u.end = static_cast<Tick>(j["end"].asInt(0));
        u.trackId = static_cast<int>(j["trackId"].asInt(0));
        u.transformation = j["transformation"].asString("original");
        p.motifUses.push_back(std::move(u));
    }

    for (const auto& j : root["automation"].arrayItems()) {
        AutomationLane lane;
        lane.id = j["id"].asString();
        lane.target = j["target"].asString();
        lane.unit = j["unit"].asString();
        for (const auto& pj : j["points"].arrayItems()) lane.points.push_back(pointFromJson(pj));
        p.automation.push_back(std::move(lane));
    }
    p.nextNoteId = static_cast<std::uint32_t>(root["nextNoteId"].asInt(1));
    p.nextMotifInstance = static_cast<std::uint32_t>(root["nextMotifInstance"].asInt(1));
    p.schemaVersion = kProjectSchemaVersion;

    if (!validateProject(p, error)) return false;
    out = std::move(p);
    return true;
}

std::string serializeProject(const Project& project, int indent) {
    return projectToJson(project).dump(indent);
}

bool deserializeProject(std::string_view text, Project& out, std::string* error) {
    std::string jsonError;
    Json root = Json::parse(text, &jsonError);
    if (!jsonError.empty()) {
        if (error) *error = "JSON parse error: " + jsonError;
        return false;
    }
    return projectFromJson(root, out, error);
}

bool validateProject(Project& p, std::string* error) {
    auto setError = [&](const std::string& e) {
        if (error) *error = e;
        return false;
    };
    if (p.ppq < 24 || p.ppq > 9600) return setError("ppq must be in [24, 9600]");
    if (p.timeSignature.numerator < 1 || p.timeSignature.numerator > 32) return setError("invalid time signature numerator");
    switch (p.timeSignature.denominator) {
        case 1: case 2: case 4: case 8: case 16: case 32: break;
        default: return setError("invalid time signature denominator");
    }
    if (!std::isfinite(p.seed)) p.seed = 1; // seed is integral; kept for clarity
    if (p.tempoMap.empty()) p.tempoMap.push_back({0, 120.0});
    for (auto& t : p.tempoMap) {
        t.tick = std::max<Tick>(0, t.tick);
        if (!std::isfinite(t.bpm)) t.bpm = 120.0;
        t.bpm = std::clamp(t.bpm, 10.0, 600.0);
    }
    std::stable_sort(p.tempoMap.begin(), p.tempoMap.end(), [](const TempoPoint& a, const TempoPoint& b) {
        return a.tick < b.tick;
    });
    if (p.tempoMap.front().tick != 0) p.tempoMap.insert(p.tempoMap.begin(), {0, p.tempoMap.front().bpm});
    if (p.sections.empty()) {
        Section s;
        s.name = "Section 1";
        s.bars = 8;
        s.target = p.curves.at(0.0);
        p.sections.push_back(std::move(s));
    }
    std::sort(p.sections.begin(), p.sections.end(), [](const Section& a, const Section& b) {
        return a.startBar < b.startBar;
    });
    std::set<std::uint32_t> ids;
    for (auto& track : p.tracks) {
        if (track.id <= 0) track.id = 1;
        track.midiChannel = std::clamp(track.midiChannel, 1, 16);
        track.gainDb = std::clamp(track.gainDb, -60.0f, 24.0f);
        track.pan = std::clamp(track.pan, -1.0f, 1.0f);
        track.reverbSend = std::clamp(track.reverbSend, 0.0f, 1.0f);
        for (auto& n : track.notes) {
            if (n.id == 0 || ids.count(n.id)) n.id = p.nextNoteId++;
            ids.insert(n.id);
            if (n.start < 0) n.start = 0;
            n.duration = std::max<Tick>(1, n.duration);
            n.pitch = std::clamp(n.pitch, 0, 127);
            n.velocity = std::clamp(n.velocity, 1, 127);
            n.channel = std::clamp(n.channel, 1, 16);
            n.brightness = std::clamp(n.brightness, 0.0f, 1.0f);
            n.attack = std::clamp(n.attack, 0.0f, 1.0f);
            n.noise = std::clamp(n.noise, 0.0f, 1.0f);
            n.release = std::clamp(n.release, 0.0f, 1.0f);
            n.legato = std::clamp(n.legato, 0.0f, 1.0f);
            if (!std::isfinite(n.microMs)) n.microMs = 0.0f;
            n.microMs = std::clamp(n.microMs, -100.0f, 100.0f);
            if (n.phrase >= static_cast<int>(p.phrases.size())) n.phrase = -1;
        }
        std::stable_sort(track.notes.begin(), track.notes.end(), [](const Note& a, const Note& b) {
            if (a.start != b.start) return a.start < b.start;
            return a.pitch < b.pitch;
        });
    }
    for (auto& c : p.chords) {
        c.duration = std::max<Tick>(1, c.duration);
        for (int& pitch : c.pitches) pitch = std::clamp(pitch, 0, 127);
        std::sort(c.pitches.begin(), c.pitches.end());
        c.tension = std::clamp(c.tension, 0.0f, 1.0f);
    }
    for (auto& ph : p.phrases) {
        if (ph.end < ph.start) std::swap(ph.start, ph.end);
        ph.intensity = std::clamp(ph.intensity, 0.0f, 1.0f);
    }
    p.curves.valence.sort();
    p.curves.arousal.sort();
    p.curves.tension.sort();
    p.curves.distance.sort();
    if (p.nextNoteId <= 1) {
        std::uint32_t maxId = 0;
        for (const auto& t : p.tracks) for (const auto& n : t.notes) maxId = std::max(maxId, n.id);
        p.nextNoteId = maxId + 1;
    }
    if (p.nextMotifInstance <= 1) {
        std::uint32_t maxId = 0;
        for (const auto& u : p.motifUses) maxId = std::max(maxId, u.instanceId);
        p.nextMotifInstance = maxId + 1;
    }
    p.schemaVersion = kProjectSchemaVersion;
    return true;
}

} // namespace ec
