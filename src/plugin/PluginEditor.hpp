#pragma once

#include "PluginProcessor.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <memory>

class ExpressiveComposerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                     private juce::Timer {
public:
    explicit ExpressiveComposerAudioProcessorEditor(ExpressiveComposerAudioProcessor&);
    ~ExpressiveComposerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class LookAndFeel;
    class CurveEditor;
    class TimelineView;

    void timerCallback() override;
    void refreshFromProcessor(bool force);
    void rebuildSectionBox();
    void updateStatus();
    ec::LockDimension lockMask() const;
    void generateDemo(ec::DemoStyle style);
    void regenerate();
    void togglePlay();
    void showSaveDialog();
    void showLoadDialog();
    void showMidiDialog();
    void setStatusText(const juce::String& text);

    ExpressiveComposerAudioProcessor& processor_;
    ec::Project project_;
    std::uint64_t lastRevision_ = 0;
    int selectedSection_ = -1;
    std::uint64_t seed_ = 20261108;
    bool draggingCurve_ = false;

    std::unique_ptr<LookAndFeel> lookAndFeel_;
    std::unique_ptr<CurveEditor> curveEditor_;
    std::unique_ptr<TimelineView> timeline_;

    juce::Label titleLabel_;
    juce::Label subtitleLabel_;
    juce::Label briefLabel_;
    juce::Label briefValue_;
    juce::Label sectionLabel_;
    juce::Label seedLabel_;
    juce::Label variationLabel_;
    juce::Label humanizeLabel_;
    juce::Label statusLabel_;
    juce::Label footerLabel_;
    juce::Label statsLabel_;
    juce::Label masterLabel_;
    juce::Label reverbLabel_;
    juce::Label widthLabel_;
    juce::Label hintLabel_;

    juce::TextButton chamberButton_{"CHAMBER"};
    juce::TextButton cinematicButton_{"CINEMATIC"};
    juce::TextButton kineticButton_{"KINETIC"};
    juce::TextButton undoButton_{"UNDO"};
    juce::TextButton redoButton_{"REDO"};
    juce::TextButton abButton_{"A/B A"};
    juce::TextButton playButton_{"PLAY"};
    juce::TextButton generateButton_{"NEW VARIATION"};
    juce::TextButton regenerateButton_{"REGENERATE"};
    juce::TextButton applyArcsButton_{"APPLY ARCS"};
    juce::TextButton humanizeButton_{"HUMANIZE"};
    juce::TextButton saveButton_{"SAVE"};
    juce::TextButton loadButton_{"LOAD"};
    juce::TextButton midiButton_{"MIDI"};
    std::array<juce::TextButton, 6> lockButtons_;
    juce::TextEditor seedEditor_;
    juce::ComboBox sectionBox_;
    juce::Slider variationSlider_;
    juce::Slider humanizeSlider_;
    juce::Slider masterSlider_;
    juce::Slider reverbSlider_;
    juce::Slider widthSlider_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExpressiveComposerAudioProcessorEditor)
};
