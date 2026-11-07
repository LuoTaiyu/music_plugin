#pragma once

#include "core/Composer.hpp"
#include "core/ProjectIO.hpp"
#include "dsp/Effects.hpp"
#include "dsp/Synth.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class ExpressiveComposerAudioProcessor final : public juce::AudioProcessor,
                                               public juce::ChangeBroadcaster,
                                               private juce::Timer {
public:
    ExpressiveComposerAudioProcessor();
    ~ExpressiveComposerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Expressive Composer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    void reset() override;

    // ---- Editor-facing API (message thread only unless stated) ----
    ec::Project getProjectCopy() const;
    std::uint64_t revision() const { return revision_.load(); }
    bool isGenerating() const { return generating_.load(); }
    juce::String statusMessage() const;
    double lastGenerationMs() const;
    int activeVariant() const { return activeVariant_; }
    bool isPreviewing() const { return previewing_.load(); }

    void generateDemo(ec::DemoStyle style, std::uint64_t seed);
    void regenerate(ec::LockDimension locks, int startBar, int endBar, float variation, std::uint64_t seed);
    void humanize(float amount);
    void recomposeWithArcs();
    void applyCurvePoint(int lane, double beat, float value);
    void applyProjectEdit(const ec::Project& project, bool addHistory = true);
    void undo();
    void redo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }
    void switchVariant();
    void setPreviewing(bool shouldPreview);
    void saveProjectFile(const juce::File& file, juce::String& error);
    void loadProjectFile(const juce::File& file, juce::String& error);
    void exportMidiFile(const juce::File& file, juce::String& error);
    void exportWavFile(const juce::File& file, int sampleRate, int bits, juce::String& error,
                       std::function<void(double, double)> progress = {});

    juce::AudioProcessorValueTreeState apvts;

private:
    struct SnapshotNote {
        double startBeat = 0.0;
        double durationBeats = 1.0;
        ec::dsp::SynthEvent synth;
    };
    struct Snapshot {
        std::vector<SnapshotNote> notes;
        double totalBeats = 0.0;
    };
    struct OwnedSnapshot {
        std::unique_ptr<Snapshot> snapshot;
        bool retired = false;
        juce::uint32 retireAtMs = 0;
    };
    struct GenerationJob {
        enum class Kind { Compose, Regenerate, Humanize } kind = Kind::Compose;
        std::uint64_t id = 0;
        ec::Project base;
        ec::GenerationSpec spec;
        ec::RegenerationOptions options;
        float humanizeAmount = 1.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void installSnapshot(const ec::Project& project);
    void purgeSnapshots();
    void timerCallback() override;
    void setStatus(const juce::String& text);
    void postJob(GenerationJob job);
    void workerLoop();
    void handleGenerated(std::uint64_t jobId, ec::Project result, ec::GenerationDiagnostics diag,
                         double elapsedMs);
    ec::Project makeCurrentProjectCopyLocked() const;
    void commitLocked(ec::Project project, bool addHistory);
    std::unique_ptr<Snapshot> buildSnapshot(const ec::Project& project);

    mutable juce::CriticalSection projectLock_;
    ec::Project project_;
    ec::Project projectA_;
    ec::Project projectB_;
    int activeVariant_ = 0;
    std::vector<ec::Project> undoStack_;
    std::vector<ec::Project> redoStack_;
    std::atomic<std::uint64_t> revision_{1};
    std::atomic<bool> previewing_{false};
    std::atomic<bool> resetPreview_{false};

    std::atomic<const Snapshot*> liveSnapshot_{nullptr};
    std::vector<OwnedSnapshot> ownedSnapshots_;
    std::atomic<double> projectBpm_{68.0};
    juce::AudioBuffer<float> sendScratch_;

    ec::dsp::SynthEngine synth_;
    ec::dsp::MasterBus master_;
    double sampleRate_ = 48000.0;
    int blockSize_ = 512;
    std::atomic<float>* gainParam_ = nullptr;
    std::atomic<float>* reverbParam_ = nullptr;
    std::atomic<float>* widthParam_ = nullptr;
    double internalBeat_ = 0.0;
    double lastHostPpq_ = -1.0;
    std::uint32_t previewLiveNotes_ = 0;

    std::thread worker_;
    std::mutex jobMutex_;
    std::condition_variable jobCv_;
    std::optional<GenerationJob> pendingJob_;
    std::atomic<bool> workerQuit_{false};
    std::atomic<std::uint64_t> latestJobId_{0};
    std::atomic<bool> generating_{false};
    std::shared_ptr<std::atomic<bool>> alive_;

    mutable juce::CriticalSection statusLock_;
    juce::String status_;
    double lastGenerationMs_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExpressiveComposerAudioProcessor)
};
