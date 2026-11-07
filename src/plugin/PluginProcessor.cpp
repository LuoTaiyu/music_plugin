#include "PluginProcessor.hpp"

#include "PluginEditor.hpp"
#include "core/TimeMap.hpp"
#include "io/MidiFile.hpp"
#include "render/Renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace {
constexpr double kInternalMasterTrimDb = 18.0;

juce::String toJuce(const std::string& s) {
    return juce::String::fromUTF8(s.data(), static_cast<int>(s.size()));
}

std::string toStd(const juce::String& s) {
    return s.toStdString();
}
} // namespace

ExpressiveComposerAudioProcessor::ExpressiveComposerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createLayout()) {
    alive_ = std::make_shared<std::atomic<bool>>(true);
    gainParam_ = apvts.getRawParameterValue("gain");
    reverbParam_ = apvts.getRawParameterValue("reverb");
    widthParam_ = apvts.getRawParameterValue("width");
    project_ = ec::composeProject(ec::demoSpec(ec::DemoStyle::Chamber, 20261108));
    projectA_ = project_;
    projectB_ = project_;
    installSnapshot(project_);
    setStatus("Chamber demo generated. Press Play for an internal audition, or start the host transport.");
    startTimer(1000);
    worker_ = std::thread([this] { workerLoop(); });
}

ExpressiveComposerAudioProcessor::~ExpressiveComposerAudioProcessor() {
    stopTimer();
    *alive_ = false;
    workerQuit_.store(true);
    jobCv_.notify_all();
    if (worker_.joinable()) worker_.join();
    liveSnapshot_.store(nullptr);
    ownedSnapshots_.clear();
}

juce::AudioProcessorValueTreeState::ParameterLayout ExpressiveComposerAudioProcessor::createLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1}, "Master gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverb", 1}, "Space return",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.72f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"width", 1}, "Stereo width",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f), 0.9f));
    return {params.begin(), params.end()};
}

void ExpressiveComposerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    blockSize_ = std::max(1, samplesPerBlock);
    const int capacity = std::max(8192, blockSize_);
    sendScratch_.setSize(2, capacity, false, true, true);
    synth_.prepare(sampleRate_, capacity);
    master_.prepare(sampleRate_, capacity);
    internalBeat_ = 0.0;
    lastHostPpq_ = -1.0;
}

void ExpressiveComposerAudioProcessor::releaseResources() {
    synth_.allNotesOff(true);
    synth_.reset();
    master_.reset();
}

bool ExpressiveComposerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ExpressiveComposerAudioProcessor::reset() {
    synth_.allNotesOff(true);
    internalBeat_ = 0.0;
    lastHostPpq_ = -1.0;
}

std::unique_ptr<ExpressiveComposerAudioProcessor::Snapshot>
ExpressiveComposerAudioProcessor::buildSnapshot(const ec::Project& project) {
    auto snapshot = std::make_unique<Snapshot>();
    const ec::TimeMap timeMap(project.tempoMap, project.ppq);
    bool anySolo = false;
    for (const auto& track : project.tracks) if (track.solo) anySolo = true;
    for (const auto& track : project.tracks) {
        if (track.muted || (anySolo && !track.solo)) continue;
        for (const auto& note : track.notes) {
            if (note.muted) continue;
            SnapshotNote sn;
            const double bpm = timeMap.bpmAtTick(note.start);
            sn.startBeat = static_cast<double>(note.start) / project.ppq +
                           (note.microMs * 0.001) * bpm / 60.0;
            sn.startBeat = std::max(0.0, sn.startBeat);
            sn.durationBeats = static_cast<double>(std::max<ec::Tick>(1, note.duration)) / project.ppq;
            sn.synth.instrument = track.instrument;
            sn.synth.pitch = note.pitch;
            sn.synth.velocity = note.velocity;
            sn.synth.trackId = track.id;
            sn.synth.noteId = note.id;
            sn.synth.pan = std::clamp(track.pan + note.pan, -1.0f, 1.0f);
            sn.synth.brightness = note.brightness;
            sn.synth.noise = note.noise;
            sn.synth.attack = note.attack;
            sn.synth.release = note.release;
            sn.synth.reverbSend = std::clamp(track.reverbSend + 0.10f * (1.0f - note.brightness), 0.0f, 1.0f);
            sn.synth.gainDb = track.gainDb;
            snapshot->notes.push_back(sn);
        }
    }
    std::stable_sort(snapshot->notes.begin(), snapshot->notes.end(), [](const SnapshotNote& a, const SnapshotNote& b) {
        return a.startBeat < b.startBeat;
    });
    snapshot->totalBeats = static_cast<double>(project.totalTicks()) / project.ppq;
    projectBpm_.store(timeMap.bpmAtTick(0));
    return snapshot;
}

void ExpressiveComposerAudioProcessor::installSnapshot(const ec::Project& project) {
    auto next = buildSnapshot(project);
    if (!next) return;
    const Snapshot* old = liveSnapshot_.exchange(next.get(), std::memory_order_acq_rel);
    if (old != nullptr) {
        for (auto& owned : ownedSnapshots_) {
            if (owned.snapshot.get() == old) {
                owned.retired = true;
                owned.retireAtMs = juce::Time::getMillisecondCounter() + 5000;
                break;
            }
        }
    }
    ownedSnapshots_.push_back({std::move(next), false, 0});
}

void ExpressiveComposerAudioProcessor::purgeSnapshots() {
    const auto now = juce::Time::getMillisecondCounter();
    ownedSnapshots_.erase(
        std::remove_if(ownedSnapshots_.begin(), ownedSnapshots_.end(), [now](const OwnedSnapshot& owned) {
            return owned.retired && now > owned.retireAtMs;
        }),
        ownedSnapshots_.end());
}

void ExpressiveComposerAudioProcessor::timerCallback() {
    purgeSnapshots();
}

void ExpressiveComposerAudioProcessor::setStatus(const juce::String& text) {
    const juce::ScopedLock lock(statusLock_);
    status_ = text;
    sendChangeMessage();
}

juce::String ExpressiveComposerAudioProcessor::statusMessage() const {
    const juce::ScopedLock lock(statusLock_);
    return status_;
}

double ExpressiveComposerAudioProcessor::lastGenerationMs() const {
    const juce::ScopedLock lock(statusLock_);
    return lastGenerationMs_;
}

ec::Project ExpressiveComposerAudioProcessor::getProjectCopy() const {
    const juce::ScopedLock lock(projectLock_);
    return project_;
}

ec::Project ExpressiveComposerAudioProcessor::makeCurrentProjectCopyLocked() const {
    return project_;
}

void ExpressiveComposerAudioProcessor::commitLocked(ec::Project project, bool addHistory) {
    if (addHistory) {
        undoStack_.push_back(project_);
        if (undoStack_.size() > 40) undoStack_.erase(undoStack_.begin());
        redoStack_.clear();
    }
    project_ = std::move(project);
    if (activeVariant_ == 0) projectA_ = project_;
    else projectB_ = project_;
    installSnapshot(project_);
    revision_.fetch_add(1);
}

void ExpressiveComposerAudioProcessor::applyProjectEdit(const ec::Project& project, bool addHistory) {
    {
        const juce::ScopedLock lock(projectLock_);
        commitLocked(project, addHistory);
    }
    sendChangeMessage();
}

void ExpressiveComposerAudioProcessor::undo() {
    {
        const juce::ScopedLock lock(projectLock_);
        if (undoStack_.empty()) return;
        redoStack_.push_back(project_);
        project_ = undoStack_.back();
        undoStack_.pop_back();
        if (activeVariant_ == 0) projectA_ = project_;
        else projectB_ = project_;
        installSnapshot(project_);
        revision_.fetch_add(1);
    }
    setStatus("Undo.");
    sendChangeMessage();
}

void ExpressiveComposerAudioProcessor::redo() {
    {
        const juce::ScopedLock lock(projectLock_);
        if (redoStack_.empty()) return;
        undoStack_.push_back(project_);
        project_ = redoStack_.back();
        redoStack_.pop_back();
        if (activeVariant_ == 0) projectA_ = project_;
        else projectB_ = project_;
        installSnapshot(project_);
        revision_.fetch_add(1);
    }
    setStatus("Redo.");
    sendChangeMessage();
}

void ExpressiveComposerAudioProcessor::switchVariant() {
    {
        const juce::ScopedLock lock(projectLock_);
        if (activeVariant_ == 0) projectA_ = project_;
        else projectB_ = project_;
        activeVariant_ = 1 - activeVariant_;
        project_ = activeVariant_ == 0 ? projectA_ : projectB_;
        installSnapshot(project_);
        revision_.fetch_add(1);
    }
    setStatus(activeVariant_ == 0 ? "Variant A active." : "Variant B active.");
    sendChangeMessage();
}

void ExpressiveComposerAudioProcessor::applyCurvePoint(int lane, double beat, float value) {
    const juce::ScopedLock lock(projectLock_);
    ec::Curve* curve = nullptr;
    switch (lane) {
        case 0: curve = &project_.curves.valence; break;
        case 1: curve = &project_.curves.arousal; break;
        case 2: curve = &project_.curves.tension; break;
        case 3: curve = &project_.curves.distance; break;
        default: return;
    }
    curve->addPoint(beat, std::clamp(value, -1.0f, 1.0f));
    project_.revisionNote = "Curve edit; recompose to apply";
    if (activeVariant_ == 0) projectA_ = project_;
    else projectB_ = project_;
    revision_.fetch_add(1);
    // Curves are metadata until a recomposition; no audio note data changes here.
    sendChangeMessage();
}

void ExpressiveComposerAudioProcessor::postJob(GenerationJob job) {
    job.id = latestJobId_.fetch_add(1) + 1;
    {
        std::lock_guard<std::mutex> lock(jobMutex_);
        pendingJob_ = std::move(job);
    }
    generating_.store(true);
    setStatus("Generating in the background...");
    jobCv_.notify_one();
}

void ExpressiveComposerAudioProcessor::generateDemo(ec::DemoStyle style, std::uint64_t seed) {
    GenerationJob job;
    job.kind = GenerationJob::Kind::Compose;
    job.spec = ec::demoSpec(style, seed);
    postJob(std::move(job));
}

void ExpressiveComposerAudioProcessor::regenerate(ec::LockDimension locks, int startBar, int endBar,
                                                  float variation, std::uint64_t seed) {
    GenerationJob job;
    job.kind = GenerationJob::Kind::Regenerate;
    {
        const juce::ScopedLock lock(projectLock_);
        job.base = project_;
    }
    job.options.locks = locks;
    job.options.rangeStartBar = startBar;
    job.options.rangeEndBar = endBar;
    job.options.variation = variation;
    job.options.seed = seed;
    postJob(std::move(job));
}

void ExpressiveComposerAudioProcessor::humanize(float amount) {
    GenerationJob job;
    job.kind = GenerationJob::Kind::Humanize;
    {
        const juce::ScopedLock lock(projectLock_);
        job.base = project_;
    }
    job.humanizeAmount = std::clamp(amount, 0.0f, 1.0f);
    postJob(std::move(job));
}

void ExpressiveComposerAudioProcessor::recomposeWithArcs() {
    GenerationJob job;
    job.kind = GenerationJob::Kind::Compose;
    {
        const juce::ScopedLock lock(projectLock_);
        job.spec = ec::specFromProject(project_, project_.seed);
        job.spec.curves = project_.curves;
        job.spec.useExplicitCurves = true;
    }
    postJob(std::move(job));
}

void ExpressiveComposerAudioProcessor::handleGenerated(std::uint64_t jobId, ec::Project result,
                                                       ec::GenerationDiagnostics diag, double elapsedMs) {
    if (jobId != latestJobId_.load()) return;
    {
        const juce::ScopedLock lock(projectLock_);
        commitLocked(std::move(result), true);
    }
    {
        const juce::ScopedLock lock(statusLock_);
        lastGenerationMs_ = elapsedMs;
    }
    juce::String status = juce::String("Generated in ") + juce::String(elapsedMs, 1) + " ms - " +
                         juce::String(diag.noteCount) + " notes, " + juce::String(diag.chordCount) +
                         " chords, " + juce::String(diag.motifUseCount) + " motif uses.";
    if (!diag.warnings.empty()) status += " " + toJuce(diag.warnings.front());
    generating_.store(false);
    setStatus(status);
    sendChangeMessage();
}

void ExpressiveComposerAudioProcessor::workerLoop() {
    for (;;) {
        GenerationJob job;
        {
            std::unique_lock<std::mutex> lock(jobMutex_);
            jobCv_.wait(lock, [this] { return workerQuit_.load() || pendingJob_.has_value(); });
            if (workerQuit_.load()) return;
            job = std::move(*pendingJob_);
            pendingJob_.reset();
        }
        const auto started = std::chrono::steady_clock::now();
        ec::GenerationDiagnostics diag;
        ec::Project result;
        try {
            switch (job.kind) {
                case GenerationJob::Kind::Compose:
                    result = ec::composeProject(job.spec, &diag);
                    break;
                case GenerationJob::Kind::Regenerate:
                    result = ec::regenerateProject(job.base, job.options, &diag);
                    break;
                case GenerationJob::Kind::Humanize:
                    result = job.base;
                    ec::humanizeProject(result, job.humanizeAmount);
                    diag.noteCount = 0;
                    for (const auto& track : result.tracks) diag.noteCount += static_cast<int>(track.notes.size());
                    break;
            }
        } catch (...) {
            if (auto alive = alive_; alive->load()) {
                juce::MessageManager::callAsync([this, alive] {
                    if (!alive->load()) return;
                    generating_.store(false);
                    setStatus("Generation failed; previous version kept.");
                });
            }
            continue;
        }
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        if (auto alive = alive_; alive->load()) {
            juce::MessageManager::callAsync([this, alive, jobId = job.id, result = std::move(result),
                                             diag = std::move(diag), elapsed]() mutable {
                if (!alive->load()) return;
                handleGenerated(jobId, std::move(result), std::move(diag), elapsed);
            });
        }
    }
}

void ExpressiveComposerAudioProcessor::setPreviewing(bool shouldPreview) {
    previewing_.store(shouldPreview);
    if (shouldPreview) resetPreview_.store(true);
    setStatus(shouldPreview ? "Internal audition playing. Host transport overrides it when running."
                            : "Internal audition stopped.");
}

void ExpressiveComposerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (numSamples <= 0) return;
    if (buffer.getNumChannels() < 2) return;
    if (numSamples > sendScratch_.getNumSamples()) return;

    const auto* snapshot = liveSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr) return;

    ec::dsp::MasterParams params;
    params.inputGainDb = (gainParam_ ? gainParam_->load() : 0.0f) + static_cast<float>(kInternalMasterTrimDb);
    params.reverbMix = reverbParam_ ? reverbParam_->load() : 0.72f;
    params.width = widthParam_ ? widthParam_->load() : 0.9f;
    master_.setParams(params);

    struct PlayEvent {
        int offset = 0;
        int order = 1;
        bool noteOff = false;
        ec::dsp::SynthEvent note;
    };
    std::array<PlayEvent, 512> events{};
    int eventCount = 0;
    auto push = [&](int offset, int order, bool off, ec::dsp::SynthEvent note) {
        if (eventCount >= static_cast<int>(events.size())) return;
        PlayEvent e;
        e.offset = std::clamp(offset, 0, numSamples - 1);
        e.order = order;
        e.noteOff = off;
        e.note = note;
        events[static_cast<std::size_t>(eventCount++)] = e;
    };

    bool hostPlaying = false;
    double hostPpq = 0.0;
    double bpm = projectBpm_.load();
    if (auto* playHead = getPlayHead()) {
        if (const auto pos = playHead->getPosition()) {
            if (const auto ppq = pos->getPpqPosition()) hostPpq = *ppq;
            if (const auto hostBpm = pos->getBpm()) bpm = *hostBpm;
            hostPlaying = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }
    }
    bpm = std::clamp(bpm, 10.0, 600.0);
    const double beatsPerBlock = static_cast<double>(numSamples) * bpm / (60.0 * sampleRate_);
    auto scheduleRange = [&](double startBeat, double endBeat) {
        auto it = std::lower_bound(snapshot->notes.begin(), snapshot->notes.end(), startBeat,
                                   [](const SnapshotNote& n, double beat) { return n.startBeat < beat; });
        for (; it != snapshot->notes.end() && it->startBeat < endBeat; ++it) {
            const double offsetSeconds = (it->startBeat - startBeat) * 60.0 / bpm;
            const int offset = static_cast<int>(std::llround(offsetSeconds * sampleRate_));
            if (offset < 0 || offset >= numSamples) continue;
            auto note = it->synth;
            note.holdSeconds = static_cast<float>(it->durationBeats * 60.0 / bpm);
            if (note.holdSeconds < 0.03f) note.holdSeconds = 0.03f;
            push(offset, 2, false, note);
        }
    };

    if (hostPlaying) {
        if (lastHostPpq_ >= 0.0 && (hostPpq < lastHostPpq_ - 0.5 ||
                                    hostPpq > lastHostPpq_ + beatsPerBlock * 2.0 + 1.0)) {
            synth_.allNotesOff(true);
        }
        lastHostPpq_ = hostPpq;
        scheduleRange(hostPpq, hostPpq + beatsPerBlock);
        internalBeat_ = hostPpq + beatsPerBlock;
    } else {
        if (lastHostPpq_ >= 0.0) {
            synth_.allNotesOff(false);
            lastHostPpq_ = -1.0;
        }
        if (previewing_.load()) {
            if (resetPreview_.exchange(false)) {
                internalBeat_ = 0.0;
                synth_.allNotesOff(true);
            }
            if (snapshot->totalBeats > 0.0 && internalBeat_ >= snapshot->totalBeats) {
                internalBeat_ = 0.0;
                synth_.allNotesOff(true);
            }
            scheduleRange(internalBeat_, internalBeat_ + beatsPerBlock);
            internalBeat_ += beatsPerBlock;
        } else {
            internalBeat_ = 0.0;
        }
    }

    // MIDI input is auditioned as a live felt piano layer. It is not written into
    // the composition; generated MIDI export remains deterministic.
    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        const int offset = std::clamp(metadata.samplePosition, 0, numSamples - 1);
        if (message.isNoteOn()) {
            ec::dsp::SynthEvent note;
            note.instrument = ec::Instrument::FeltPiano;
            note.pitch = message.getNoteNumber();
            note.velocity = message.getVelocity();
            note.trackId = 9999;
            note.noteId = 900000u + (previewLiveNotes_++);
            note.pan = 0.0f;
            note.brightness = 0.62f;
            note.reverbSend = 0.25f;
            note.holdSeconds = 0.0f;
            push(offset, 3, false, note);
        } else if (message.isNoteOff()) {
            ec::dsp::SynthEvent note;
            note.trackId = 9999;
            note.pitch = message.getNoteNumber();
            push(offset, 0, true, note);
        }
    }
    midiMessages.clear();

    std::sort(events.begin(), events.begin() + eventCount, [](const PlayEvent& a, const PlayEvent& b) {
        if (a.offset != b.offset) return a.offset < b.offset;
        return a.order < b.order;
    });

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    float* sendL = sendScratch_.getWritePointer(0);
    float* sendR = sendScratch_.getWritePointer(1);
    int cursor = 0;
    for (int i = 0; i < eventCount; ++i) {
        const auto& event = events[static_cast<std::size_t>(i)];
        if (event.offset > cursor) {
            synth_.render(left + cursor, right + cursor, sendL + cursor, sendR + cursor, event.offset - cursor);
            cursor = event.offset;
        }
        if (event.noteOff) synth_.noteOff(event.note.trackId, event.note.pitch);
        else synth_.noteOn(event.note);
    }
    if (cursor < numSamples) {
        synth_.render(left + cursor, right + cursor, sendL + cursor, sendR + cursor, numSamples - cursor);
    }
    master_.process(left, right, sendL, sendR, numSamples);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, numSamples);
}

void ExpressiveComposerAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree state;
    {
        const juce::ScopedLock lock(projectLock_);
        state = apvts.copyState();
        state.setProperty("projectA", toJuce(ec::serializeProject(projectA_, 0)), nullptr);
        state.setProperty("projectB", toJuce(ec::serializeProject(projectB_, 0)), nullptr);
        state.setProperty("activeVariant", activeVariant_, nullptr);
        state.setProperty("ecSchema", 1, nullptr);
    }
    if (auto xml = state.createXml()) {
        juce::MemoryOutputStream out(destData, false);
        xml->writeTo(out);
    }
}

void ExpressiveComposerAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) return;
    const std::string text(static_cast<const char*>(data), static_cast<std::size_t>(sizeInBytes));
    auto xml = juce::parseXML(toJuce(text));
    if (!xml) return;
    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid()) return;
    if (state.hasProperty("ecSchema")) apvts.replaceState(state);
    auto loadProjectProperty = [&](const juce::Identifier& name, ec::Project& target) {
        if (!state.hasProperty(name)) return false;
        ec::Project p;
        std::string error;
        if (!ec::deserializeProject(toStd(state[name].toString()), p, &error)) return false;
        target = std::move(p);
        return true;
    };
    ec::Project a, b;
    const bool loadedA = loadProjectProperty("projectA", a);
    const bool loadedB = loadProjectProperty("projectB", b);
    if (!loadedA) a = ec::composeProject(ec::demoSpec(ec::DemoStyle::Chamber, 20261108));
    if (!loadedB) b = a;
    {
        const juce::ScopedLock lock(projectLock_);
        projectA_ = std::move(a);
        projectB_ = std::move(b);
        activeVariant_ = static_cast<int>(state.getProperty("activeVariant", 0)) == 1 ? 1 : 0;
        project_ = activeVariant_ == 0 ? projectA_ : projectB_;
        installSnapshot(project_);
        revision_.fetch_add(1);
    }
    generating_.store(false);
    setStatus("Project state restored.");
    sendChangeMessage();
}

void ExpressiveComposerAudioProcessor::saveProjectFile(const juce::File& file, juce::String& error) {
    const juce::ScopedLock lock(projectLock_);
    const auto json = ec::serializeProject(project_, 2);
    if (!file.replaceWithText(toJuce(json))) error = "Could not write " + file.getFullPathName();
}

void ExpressiveComposerAudioProcessor::loadProjectFile(const juce::File& file, juce::String& error) {
    if (!file.existsAsFile()) {
        error = "File not found: " + file.getFullPathName();
        return;
    }
    ec::Project p;
    const std::string text = toStd(file.loadFileAsString());
    std::string parseError;
    if (!ec::deserializeProject(text, p, &parseError)) {
        error = toJuce(parseError);
        return;
    }
    applyProjectEdit(std::move(p), true);
    setStatus("Project loaded from " + file.getFileName());
}

void ExpressiveComposerAudioProcessor::exportMidiFile(const juce::File& file, juce::String& error) {
    ec::Project p;
    {
        const juce::ScopedLock lock(projectLock_);
        p = project_;
    }
    ec::MidiExportStats stats;
    std::string exportError;
    if (!ec::exportMidiFile(p, file.getFullPathName().toStdString(), {}, &stats, &exportError)) {
        error = toJuce(exportError);
        return;
    }
    setStatus("MIDI exported: " + file.getFileName() + " (" + juce::String(stats.notes) + " notes)");
}

void ExpressiveComposerAudioProcessor::exportWavFile(const juce::File& file, int sampleRate, int bits,
                                                     juce::String& error,
                                                     std::function<void(double, double)> progress) {
    ec::Project p;
    {
        const juce::ScopedLock lock(projectLock_);
        p = project_;
    }
    if (progress) progress(0.0, 1.0);
    ec::RenderOptions options;
    options.sampleRate = sampleRate;
    options.bitsPerSample = bits;
    options.blockSize = 512;
    options.tailSeconds = 3.0f;
    ec::RenderStats stats;
    std::string renderError;
    if (!ec::renderProjectToWav(p, file.getFullPathName().toStdString(), options, &stats, &renderError)) {
        error = toJuce(renderError);
        return;
    }
    if (progress) progress(1.0, 1.0);
    setStatus("Audio rendered: " + file.getFileName() + " (" + juce::String(stats.audioSeconds, 1) + " s)");
}

juce::AudioProcessorEditor* ExpressiveComposerAudioProcessor::createEditor() {
    return new ExpressiveComposerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ExpressiveComposerAudioProcessor();
}
