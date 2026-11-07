#include "PluginEditor.hpp"

#include "core/Theory.hpp"

#include <algorithm>
#include <cmath>

namespace {

namespace Theme {
const juce::Colour bg = juce::Colour::fromRGB(13, 15, 19);
const juce::Colour panel = juce::Colour::fromRGB(23, 27, 34);
const juce::Colour panel2 = juce::Colour::fromRGB(30, 35, 43);
const juce::Colour line = juce::Colour::fromRGB(49, 56, 66);
const juce::Colour text = juce::Colour::fromRGB(235, 231, 221);
const juce::Colour muted = juce::Colour::fromRGB(135, 143, 155);
const juce::Colour accent = juce::Colour::fromRGB(224, 177, 103);
const juce::Colour accentDim = juce::Colour::fromRGB(113, 91, 54);
const juce::Colour teal = juce::Colour::fromRGB(95, 179, 161);
const juce::Colour danger = juce::Colour::fromRGB(196, 104, 92);
} // namespace Theme

juce::Font titleFont(float size) {
    return juce::Font(juce::FontOptions("Segoe UI", size, juce::Font::bold));
}
juce::Font bodyFont(float size) {
    return juce::Font(juce::FontOptions("Segoe UI", size, juce::Font::plain));
}
juce::Font monoFont(float size) {
    return juce::Font(juce::FontOptions("Consolas", size, juce::Font::plain));
}
juce::String sv(std::string_view value) {
    return juce::String(value.data(), static_cast<int>(value.size()));
}

int beatsPerBarFor(const ec::Project& p) {
    return std::max(1, p.timeSignature.numerator * 4 / std::max(1, p.timeSignature.denominator));
}

} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------
class ExpressiveComposerAudioProcessorEditor::LookAndFeel : public juce::LookAndFeel_V4 {
public:
    LookAndFeel() {
        setColour(juce::TextButton::buttonColourId, Theme::panel2);
        setColour(juce::TextButton::buttonOnColourId, Theme::accentDim);
        setColour(juce::TextButton::textColourOffId, Theme::text);
        setColour(juce::TextButton::textColourOnId, Theme::text);
        setColour(juce::Label::textColourId, Theme::text);
        setColour(juce::Slider::thumbColourId, Theme::accent);
        setColour(juce::Slider::rotarySliderFillColourId, Theme::accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, Theme::line);
        setColour(juce::Slider::trackColourId, Theme::line);
        setColour(juce::Slider::textBoxTextColourId, Theme::text);
        setColour(juce::Slider::textBoxBackgroundColourId, Theme::panel);
        setColour(juce::Slider::textBoxOutlineColourId, Theme::line);
        setColour(juce::ComboBox::backgroundColourId, Theme::panel2);
        setColour(juce::ComboBox::textColourId, Theme::text);
        setColour(juce::ComboBox::outlineColourId, Theme::line);
        setColour(juce::ComboBox::arrowColourId, Theme::accent);
        setColour(juce::PopupMenu::backgroundColourId, Theme::panel2);
        setColour(juce::PopupMenu::textColourId, Theme::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accentDim);
        setColour(juce::PopupMenu::highlightedTextColourId, Theme::text);
        setColour(juce::TextEditor::backgroundColourId, Theme::panel);
        setColour(juce::TextEditor::textColourId, Theme::text);
        setColour(juce::TextEditor::outlineColourId, Theme::line);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
        setColour(juce::TextEditor::highlightColourId, Theme::accentDim);
        setColour(juce::TextEditor::highlightedTextColourId, Theme::text);
        setColour(juce::AlertWindow::backgroundColourId, Theme::panel);
        setColour(juce::AlertWindow::textColourId, Theme::text);
        setColour(juce::AlertWindow::outlineColourId, Theme::line);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int height) override {
        return bodyFont(std::clamp(static_cast<float>(height) * 0.34f, 9.0f, 13.0f));
    }

    juce::Font getLabelFont(juce::Label& label) override {
        return label.getFont();
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto fill = backgroundColour;
        if (button.getToggleState()) fill = Theme::accentDim;
        if (down) fill = fill.brighter(0.12f);
        else if (highlighted) fill = fill.brighter(0.06f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(button.getToggleState() ? Theme::accent : Theme::line);
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override {
        const float radius = static_cast<float>(std::min(width, height)) * 0.5f - 5.0f;
        const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        juce::Path track;
        track.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(Theme::line);
        g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path value;
        value.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(Theme::accent);
        g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(Theme::panel2);
        g.fillEllipse(centreX - radius + 4.0f, centreY - radius + 4.0f,
                      (radius - 4.0f) * 2.0f, (radius - 4.0f) * 2.0f);
        g.setColour(Theme::text);
        const float dx = std::sin(angle) * (radius - 8.0f);
        const float dy = -std::cos(angle) * (radius - 8.0f);
        g.drawLine(centreX, centreY, centreX + dx, centreY + dy, 2.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                      juce::ComboBox& box) override {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        g.setColour(Theme::panel2);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(box.hasKeyboardFocus(false) ? Theme::accent : Theme::line);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        juce::Path arrow;
        const float cx = static_cast<float>(width) - 14.0f;
        const float cy = static_cast<float>(height) * 0.5f;
        arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
        arrow.lineTo(cx, cy + 2.0f);
        arrow.lineTo(cx + 4.0f, cy - 2.0f);
        g.setColour(Theme::accent);
        g.strokePath(arrow, juce::PathStrokeType(1.4f));
    }
};

// ---------------------------------------------------------------------------
// Curve editor
// ---------------------------------------------------------------------------
class ExpressiveComposerAudioProcessorEditor::CurveEditor : public juce::Component {
public:
    std::function<void(const ec::ExpressionCurves&)> onCurvesChanged;
    std::function<void(bool)> onDraggingChanged;

    explicit CurveEditor(ExpressiveComposerAudioProcessor& processor) : processor_(processor) {
        setWantsKeyboardFocus(false);
    }

    void setProject(const ec::Project& project) {
        project_ = project;
        if (!dragging_) {
            curves_ = project_.curves;
            dragLane_ = -1;
            dragPoint_ = -1;
        }
        repaint();
    }

    bool isDragging() const { return dragging_; }

    void paint(juce::Graphics& g) override {
        auto panel = getLocalBounds().toFloat();
        g.setColour(Theme::panel);
        g.fillRoundedRectangle(panel, 8.0f);
        g.setColour(Theme::line);
        g.drawRoundedRectangle(panel.reduced(0.5f), 8.0f, 1.0f);

        g.setFont(bodyFont(10.0f));
        g.setColour(Theme::muted);
        g.drawText("EXPRESSION ARCS  |  edit points, then APPLY ARCS", 14, 8, getWidth() - 28, 16,
                   juce::Justification::centredLeft);

        const auto graph = graphBounds();
        const int totalBeats = std::max(1, static_cast<int>(project_.totalTicks() / std::max(1, project_.ppq)));
        const int bars = std::max(1, project_.totalBars());
        const int lanes = 4;
        const float laneHeight = static_cast<float>(graph.getHeight()) / static_cast<float>(lanes);

        for (int lane = 0; lane < lanes; ++lane) {
            const float top = static_cast<float>(graph.getY()) + laneHeight * static_cast<float>(lane);
            auto laneRect = juce::Rectangle<float>(static_cast<float>(graph.getX()), top,
                                                   static_cast<float>(graph.getWidth()), laneHeight - 4.0f);
            g.setColour(lane % 2 == 0 ? Theme::panel2.withAlpha(0.35f) : Theme::panel.withAlpha(0.4f));
            g.fillRoundedRectangle(laneRect, 3.0f);
            g.setColour(Theme::line.withAlpha(0.55f));
            g.drawRoundedRectangle(laneRect, 3.0f, 1.0f);

            const int laneBars = std::max(1, bars / 8);
            for (int bar = 0; bar <= bars; bar += laneBars) {
                const float x = static_cast<float>(graph.getX()) +
                                static_cast<float>(graph.getWidth()) * static_cast<float>(bar) / static_cast<float>(bars);
                g.setColour(Theme::line.withAlpha(0.28f));
                g.drawVerticalLine(static_cast<int>(x), laneRect.getY(), laneRect.getBottom());
            }
            const float zeroY = (lane == 0)
                                    ? laneRect.getCentreY()
                                    : laneRect.getBottom() - 2.0f;
            g.setColour(Theme::line.withAlpha(0.65f));
            g.drawHorizontalLine(static_cast<int>(zeroY), laneRect.getX(), laneRect.getRight());

            const char* names[4] = {"VALENCE", "AROUSAL", "TENSION", "DISTANCE"};
            const juce::Colour colours[4] = {Theme::accent, Theme::teal, Theme::danger, juce::Colour::fromRGB(115, 146, 205)};
            g.setFont(bodyFont(9.0f));
            g.setColour(colours[lane]);
            g.drawText(names[lane], 8, static_cast<int>(top) + 4, 46, 14, juce::Justification::centredLeft);
            g.setColour(Theme::muted.withAlpha(0.8f));
            g.drawText(lane == 0 ? "-1" : "0", 40, static_cast<int>(laneRect.getBottom()) - 13, 18, 12,
                       juce::Justification::centredRight);
            g.drawText("1", 40, static_cast<int>(top) + 2, 18, 12, juce::Justification::centredRight);

            const ec::Curve& curve = curveFor(lane);
            juce::Path path;
            bool first = true;
            for (const auto& point : curve.points) {
                const float x = xForBeat(point.beat, graph, totalBeats);
                const float y = yForValue(point.value, lane, laneRect);
                if (first) { path.startNewSubPath(x, y); first = false; }
                else path.lineTo(x, y);
            }
            if (!first) {
                juce::Path fill = path;
                fill.lineTo(static_cast<float>(graph.getRight()), laneRect.getBottom());
                fill.lineTo(static_cast<float>(graph.getX()), laneRect.getBottom());
                fill.closeSubPath();
                g.setColour(colours[lane].withAlpha(0.08f));
                g.fillPath(fill);
                g.setColour(colours[lane]);
                g.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            for (std::size_t i = 0; i < curve.points.size(); ++i) {
                const auto& point = curve.points[i];
                const float x = xForBeat(point.beat, graph, totalBeats);
                const float y = yForValue(point.value, lane, laneRect);
                const bool active = lane == dragLane_ && static_cast<int>(i) == dragPoint_;
                g.setColour(active ? juce::Colours::white : colours[lane]);
                g.fillEllipse(x - 3.2f, y - 3.2f, 6.4f, 6.4f);
                g.setColour(Theme::bg);
                g.fillEllipse(x - 1.1f, y - 1.1f, 2.2f, 2.2f);
            }
        }

        // Section boundaries and names.
        g.setFont(bodyFont(8.0f));
        for (const auto& section : project_.sections) {
            const float x = static_cast<float>(graph.getX()) +
                            static_cast<float>(graph.getWidth()) * static_cast<float>(section.startBar) / static_cast<float>(bars);
            g.setColour(Theme::muted.withAlpha(0.25f));
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(graph.getY()), static_cast<float>(graph.getBottom()));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto graph = graphBounds();
        if (!graph.contains(e.getPosition())) return;
        const int lane = laneAt(e.y);
        if (lane < 0) return;
        const auto laneRect = laneBounds(lane);
        const int totalBeats = std::max(1, static_cast<int>(project_.totalTicks() / std::max(1, project_.ppq)));
        const double beat = beatForX(static_cast<float>(e.x), graph, totalBeats);
        const float value = valueForY(static_cast<float>(e.y), lane, laneRect);
        const ec::Curve& curve = curveFor(lane);
        int nearest = -1;
        float nearestDistance = 12.0f;
        for (std::size_t i = 0; i < curve.points.size(); ++i) {
            const float px = xForBeat(curve.points[i].beat, graph, totalBeats);
            const float py = yForValue(curve.points[i].value, lane, laneRect);
            const float d = juce::Point<float>(px, py).getDistanceFrom(juce::Point<float>(
                static_cast<float>(e.x), static_cast<float>(e.y)));
            if (d < nearestDistance) { nearestDistance = d; nearest = static_cast<int>(i); }
        }
        if (e.mods.isPopupMenu()) {
            if (nearest >= 0) {
                auto& mutableCurve = curveForMutable(lane);
                if (mutableCurve.points.size() > 2) {
                    mutableCurve.points.erase(mutableCurve.points.begin() + nearest);
                    project_.curves = curves_;
                    onCurvesChanged(curves_);
                    repaint();
                }
            }
            return;
        }
        if (nearest < 0) {
            auto& mutableCurve = curveForMutable(lane);
            mutableCurve.addPoint(beat, value);
            nearest = 0;
            for (std::size_t i = 0; i < mutableCurve.points.size(); ++i) {
                if (std::abs(mutableCurve.points[i].beat - beat) < 1e-6) nearest = static_cast<int>(i);
            }
            project_.curves = curves_;
            onCurvesChanged(curves_);
        }
        dragLane_ = lane;
        dragPoint_ = nearest;
        dragging_ = true;
        onDraggingChanged(true);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragging_ || dragLane_ < 0 || dragPoint_ < 0) return;
        const auto graph = graphBounds();
        const auto laneRect = laneBounds(dragLane_);
        const int totalBeats = std::max(1, static_cast<int>(project_.totalTicks() / std::max(1, project_.ppq)));
        auto& curve = curveForMutable(dragLane_);
        if (dragPoint_ >= static_cast<int>(curve.points.size())) return;
        curve.points[static_cast<std::size_t>(dragPoint_)].beat = beatForX(static_cast<float>(e.x), graph, totalBeats);
        curve.points[static_cast<std::size_t>(dragPoint_)].value = valueForY(static_cast<float>(e.y), dragLane_, laneRect);
        curve.sort();
        // Re-find the dragged point after sort by nearest beat.
        const double b = beatForX(static_cast<float>(e.x), graph, totalBeats);
        for (std::size_t i = 0; i < curve.points.size(); ++i) {
            if (std::abs(curve.points[i].beat - b) < 1e-6) { dragPoint_ = static_cast<int>(i); break; }
        }
        project_.curves = curves_;
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (!dragging_) return;
        dragging_ = false;
        onDraggingChanged(false);
        project_.curves = curves_;
        onCurvesChanged(curves_);
        repaint();
    }

private:
    juce::Rectangle<int> graphBounds() const {
        return juce::Rectangle<int>(56, 28, std::max(10, getWidth() - 70), std::max(10, getHeight() - 38));
    }

    juce::Rectangle<float> laneBounds(int lane) const {
        const auto graph = graphBounds();
        const float h = static_cast<float>(graph.getHeight()) / 4.0f;
        return {static_cast<float>(graph.getX()), static_cast<float>(graph.getY()) + h * static_cast<float>(lane),
                static_cast<float>(graph.getWidth()), h - 4.0f};
    }

    int laneAt(int y) const {
        const auto graph = graphBounds();
        if (y < graph.getY() || y >= graph.getBottom()) return -1;
        return std::clamp((y - graph.getY()) * 4 / std::max(1, graph.getHeight()), 0, 3);
    }

    static float xForBeat(double beat, juce::Rectangle<int> graph, int totalBeats) {
        return static_cast<float>(graph.getX()) + static_cast<float>(graph.getWidth()) *
                                                   static_cast<float>(std::clamp(beat / std::max(1, totalBeats), 0.0, 1.0));
    }

    static double beatForX(float x, juce::Rectangle<int> graph, int totalBeats) {
        const float t = std::clamp((x - static_cast<float>(graph.getX())) / static_cast<float>(std::max(1, graph.getWidth())), 0.0f, 1.0f);
        return static_cast<double>(t) * static_cast<double>(totalBeats);
    }

    static float yForValue(float value, int lane, juce::Rectangle<float> rect) {
        if (lane == 0) {
            const float norm = (std::clamp(value, -1.0f, 1.0f) + 1.0f) * 0.5f;
            return rect.getBottom() - norm * rect.getHeight();
        }
        return rect.getBottom() - std::clamp(value, 0.0f, 1.0f) * rect.getHeight();
    }

    static float valueForY(float y, int lane, juce::Rectangle<float> rect) {
        const float norm = std::clamp((rect.getBottom() - y) / std::max(1.0f, rect.getHeight()), 0.0f, 1.0f);
        return lane == 0 ? norm * 2.0f - 1.0f : norm;
    }

    const ec::Curve& curveFor(int lane) const {
        switch (lane) {
            case 0: return curves_.valence;
            case 1: return curves_.arousal;
            case 2: return curves_.tension;
            default: return curves_.distance;
        }
    }

    ec::Curve& curveForMutable(int lane) {
        switch (lane) {
            case 0: return curves_.valence;
            case 1: return curves_.arousal;
            case 2: return curves_.tension;
            default: return curves_.distance;
        }
    }

    ExpressiveComposerAudioProcessor& processor_;
    ec::Project project_;
    ec::ExpressionCurves curves_;
    bool dragging_ = false;
    int dragLane_ = -1;
    int dragPoint_ = -1;
};

// ---------------------------------------------------------------------------
// Timeline
// ---------------------------------------------------------------------------
class ExpressiveComposerAudioProcessorEditor::TimelineView : public juce::Component {
public:
    std::function<void(int)> onSectionSelected;

    void setProject(const ec::Project& project) {
        project_ = project;
        repaint();
    }

    void setSelectedSection(int index) {
        selected_ = index;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto panel = getLocalBounds().toFloat();
        g.setColour(Theme::panel);
        g.fillRoundedRectangle(panel, 8.0f);
        g.setColour(Theme::line);
        g.drawRoundedRectangle(panel.reduced(0.5f), 8.0f, 1.0f);
        g.setFont(bodyFont(10.0f));
        g.setColour(Theme::muted);
        g.drawText("ARRANGEMENT  |  click a section to target local regeneration", 14, 8,
                   getWidth() - 28, 16, juce::Justification::centredLeft);
        if (project_.sections.empty()) return;

        const int totalBars = std::max(1, project_.totalBars());
        const int left = 64;
        const int right = getWidth() - 12;
        const int width = std::max(1, right - left);
        const int top = 30;
        const int sectionH = 42;
        const int trackTop = top + sectionH + 8;
        const int trackH = std::max(20, (getHeight() - trackTop - 12) / std::max<int>(1, static_cast<int>(project_.tracks.size())));

        for (std::size_t i = 0; i < project_.sections.size(); ++i) {
            const auto& section = project_.sections[i];
            const int x = left + section.startBar * width / totalBars;
            const int w = std::max(2, section.bars * width / totalBars);
            const bool selected = static_cast<int>(i) == selected_;
            g.setColour(selected ? Theme::accentDim.withAlpha(0.9f) : Theme::panel2);
            g.fillRoundedRectangle(static_cast<float>(x + 1), static_cast<float>(top), static_cast<float>(w - 2),
                                   static_cast<float>(sectionH), 3.0f);
            g.setColour(selected ? Theme::accent : Theme::line);
            g.drawRoundedRectangle(static_cast<float>(x + 1), static_cast<float>(top), static_cast<float>(w - 2),
                                   static_cast<float>(sectionH), 3.0f, 1.0f);
            g.setFont(bodyFont(9.5f));
            g.setColour(selected ? Theme::text : Theme::muted);
            g.drawText(section.name, x + 5, top + 4, w - 10, 14, juce::Justification::centredLeft, true);
            g.setFont(bodyFont(8.0f));
            g.setColour(Theme::muted.withAlpha(0.8f));
            g.drawText("v" + juce::String(section.target.valence, 2) + " t" + juce::String(section.target.tension, 2),
                       x + 5, top + 20, w - 10, 14, juce::Justification::centredLeft, true);
        }

        const int minPitch = 28;
        const int maxPitch = 96;
        for (std::size_t t = 0; t < project_.tracks.size(); ++t) {
            const auto& track = project_.tracks[t];
            const int y = trackTop + static_cast<int>(t) * trackH;
            g.setColour(t == 0 ? Theme::panel2.withAlpha(0.45f) : Theme::panel.withAlpha(0.2f));
            g.fillRect(left, y, width, trackH - 2);
            g.setColour(Theme::line.withAlpha(0.55f));
            g.drawHorizontalLine(y + trackH - 2, static_cast<float>(left), static_cast<float>(right));
            g.setFont(bodyFont(8.5f));
            g.setColour(Theme::muted);
            g.drawText(track.name, 8, y + 2, left - 14, trackH - 4, juce::Justification::centredLeft, true);

            const juce::Colour colours[] = {Theme::accent, Theme::teal, juce::Colour::fromRGB(115, 146, 205),
                                            Theme::danger, juce::Colour::fromRGB(166, 132, 190)};
            const auto colour = colours[t % 5];
            for (const auto& note : track.notes) {
                if (note.muted) continue;
                const int x = left + static_cast<int>(static_cast<double>(note.start) /
                                                        static_cast<double>(project_.totalTicks()) * width);
                const int w = std::max(1, static_cast<int>(static_cast<double>(note.duration) /
                                                           static_cast<double>(project_.totalTicks()) * width));
                const int pitch = std::clamp(note.pitch, minPitch, maxPitch);
                const int ny = y + 3 + (maxPitch - pitch) * (trackH - 8) / (maxPitch - minPitch);
                g.setColour(colour.withAlpha(0.28f + 0.62f * static_cast<float>(note.velocity) / 127.0f));
                g.fillRoundedRectangle(static_cast<float>(x), static_cast<float>(ny),
                                       static_cast<float>(std::max(1, w)), 2.5f, 1.0f);
            }
        }
        g.setFont(bodyFont(8.0f));
        g.setColour(Theme::muted.withAlpha(0.7f));
        for (int bar = 0; bar <= totalBars; bar += std::max(1, totalBars / 8)) {
            const int x = left + bar * width / totalBars;
            g.drawVerticalLine(x, static_cast<float>(top + sectionH + 2), static_cast<float>(getHeight() - 8));
            g.drawText(juce::String(bar + 1), x + 2, getHeight() - 18, 22, 12, juce::Justification::centredLeft);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (project_.sections.empty()) return;
        const int totalBars = std::max(1, project_.totalBars());
        const int left = 64;
        const int width = std::max(1, getWidth() - 12 - left);
        const int bar = std::clamp((e.x - left) * totalBars / width, 0, totalBars - 1);
        for (std::size_t i = 0; i < project_.sections.size(); ++i) {
            const auto& s = project_.sections[i];
            if (bar >= s.startBar && bar < s.startBar + s.bars) {
                selected_ = static_cast<int>(i);
                if (onSectionSelected) onSectionSelected(selected_);
                repaint();
                return;
            }
        }
    }

private:
    ec::Project project_;
    int selected_ = -1;
};

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
ExpressiveComposerAudioProcessorEditor::ExpressiveComposerAudioProcessorEditor(
    ExpressiveComposerAudioProcessor& processor)
    : AudioProcessorEditor(&processor), processor_(processor) {
    lookAndFeel_ = std::make_unique<LookAndFeel>();
    setLookAndFeel(lookAndFeel_.get());
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(1050, 720, 1800, 1100);

    titleLabel_.setText("EXPRESSIVE COMPOSER", juce::dontSendNotification);
    titleLabel_.setFont(titleFont(24.0f));
    titleLabel_.setColour(juce::Label::textColourId, Theme::text);
    addAndMakeVisible(titleLabel_);
    subtitleLabel_.setText("THEORY-DRIVEN COMPOSITION  |  EDITABLE MIDI  |  LOCAL CORE", juce::dontSendNotification);
    subtitleLabel_.setFont(bodyFont(9.5f));
    subtitleLabel_.setColour(juce::Label::textColourId, Theme::muted);
    addAndMakeVisible(subtitleLabel_);

    briefLabel_.setText("CREATIVE BRIEF", juce::dontSendNotification);
    briefLabel_.setFont(bodyFont(10.0f));
    briefLabel_.setColour(juce::Label::textColourId, Theme::accent);
    addAndMakeVisible(briefLabel_);
    briefValue_.setFont(bodyFont(11.5f));
    briefValue_.setColour(juce::Label::textColourId, Theme::text);
    briefValue_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(briefValue_);

    sectionLabel_.setText("REGENERATION RANGE", juce::dontSendNotification);
    sectionLabel_.setFont(bodyFont(9.0f));
    sectionLabel_.setColour(juce::Label::textColourId, Theme::muted);
    addAndMakeVisible(sectionLabel_);

    seedLabel_.setText("SEED", juce::dontSendNotification);
    seedLabel_.setFont(bodyFont(9.0f));
    seedLabel_.setColour(juce::Label::textColourId, Theme::muted);
    addAndMakeVisible(seedLabel_);
    seedEditor_.setText("20261108", false);
    seedEditor_.setFont(monoFont(12.0f));
    seedEditor_.setInputRestrictions(20, "0123456789");
    seedEditor_.onReturnKey = [this] { seed_ = static_cast<std::uint64_t>(seedEditor_.getText().getLargeIntValue()); };
    seedEditor_.onFocusLost = [this] { seed_ = static_cast<std::uint64_t>(seedEditor_.getText().getLargeIntValue()); };
    addAndMakeVisible(seedEditor_);

    variationLabel_.setText("VARIATION", juce::dontSendNotification);
    variationLabel_.setFont(bodyFont(9.0f));
    variationLabel_.setColour(juce::Label::textColourId, Theme::muted);
    addAndMakeVisible(variationLabel_);
    variationSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    variationSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
    variationSlider_.setRange(0.0, 1.0, 0.01);
    variationSlider_.setValue(0.5);
    addAndMakeVisible(variationSlider_);

    humanizeLabel_.setText("HUMANIZE DEPTH", juce::dontSendNotification);
    humanizeLabel_.setFont(bodyFont(9.0f));
    humanizeLabel_.setColour(juce::Label::textColourId, Theme::muted);
    addAndMakeVisible(humanizeLabel_);
    humanizeSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    humanizeSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
    humanizeSlider_.setRange(0.0, 1.0, 0.01);
    humanizeSlider_.setValue(0.8);
    addAndMakeVisible(humanizeSlider_);

    sectionBox_.addItem("Whole piece", 1);
    sectionBox_.setSelectedId(1, juce::dontSendNotification);
    sectionBox_.onChange = [this] {
        const int id = sectionBox_.getSelectedId();
        selectedSection_ = id <= 1 ? -1 : id - 2;
        if (timeline_) timeline_->setSelectedSection(selectedSection_);
    };
    addAndMakeVisible(sectionBox_);

    auto setupButton = [this](juce::TextButton& button, const juce::String& tooltip = {}) {
        button.setColour(juce::TextButton::buttonColourId, Theme::panel2);
        button.setColour(juce::TextButton::textColourOffId, Theme::text);
        if (tooltip.isNotEmpty()) button.setTooltip(tooltip);
        addAndMakeVisible(button);
    };
    for (auto* b : {&chamberButton_, &cinematicButton_, &kineticButton_, &undoButton_, &redoButton_, &abButton_,
                    &playButton_, &generateButton_, &regenerateButton_, &applyArcsButton_, &humanizeButton_,
                    &saveButton_, &loadButton_, &midiButton_}) {
        setupButton(*b);
    }
    const char* lockNames[6] = {"MELODY", "HARMONY", "RHYTHM", "ORCH.", "HUMAN.", "ARCS"};
    for (int i = 0; i < 6; ++i) {
        auto& b = lockButtons_[static_cast<std::size_t>(i)];
        b.setButtonText(lockNames[i]);
        setupButton(b);
        b.setClickingTogglesState(true);
    }
    lockButtons_[0].setToggleState(true, juce::dontSendNotification);
    lockButtons_[1].setToggleState(true, juce::dontSendNotification);

    chamberButton_.onClick = [this] { generateDemo(ec::DemoStyle::Chamber); };
    cinematicButton_.onClick = [this] { generateDemo(ec::DemoStyle::Cinematic); };
    kineticButton_.onClick = [this] { generateDemo(ec::DemoStyle::Kinetic); };
    undoButton_.onClick = [this] { processor_.undo(); refreshFromProcessor(true); };
    redoButton_.onClick = [this] { processor_.redo(); refreshFromProcessor(true); };
    abButton_.onClick = [this] { processor_.switchVariant(); refreshFromProcessor(true); };
    playButton_.onClick = [this] { togglePlay(); };
    generateButton_.onClick = [this] {
        seed_ = static_cast<std::uint64_t>(seedEditor_.getText().getLargeIntValue());
        processor_.generateDemo(ec::demoStyleFromId(project_.styleId), seed_);
    };
    regenerateButton_.onClick = [this] { regenerate(); };
    applyArcsButton_.onClick = [this] { processor_.recomposeWithArcs(); };
    humanizeButton_.onClick = [this] { processor_.humanize(static_cast<float>(humanizeSlider_.getValue())); };
    saveButton_.onClick = [this] { showSaveDialog(); };
    loadButton_.onClick = [this] { showLoadDialog(); };
    midiButton_.onClick = [this] { showMidiDialog(); };

    curveEditor_ = std::make_unique<CurveEditor>(processor_);
    addAndMakeVisible(*curveEditor_);
    curveEditor_->onDraggingChanged = [this](bool dragging) {
        draggingCurve_ = dragging;
    };
    curveEditor_->onCurvesChanged = [this](const ec::ExpressionCurves& curves) {
        auto p = processor_.getProjectCopy();
        p.curves = curves;
        processor_.applyProjectEdit(p);
        setStatusText("Arcs stored in the project. Press APPLY ARCS to recompose with the new shape.");
        refreshFromProcessor(false);
    };

    timeline_ = std::make_unique<TimelineView>();
    addAndMakeVisible(*timeline_);
    timeline_->onSectionSelected = [this](int index) {
        selectedSection_ = index;
        sectionBox_.setSelectedId(index + 2, juce::dontSendNotification);
    };

    statusLabel_.setFont(bodyFont(10.5f));
    statusLabel_.setColour(juce::Label::textColourId, Theme::muted);
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    footerLabel_.setText("EC 0.1  |  local deterministic core  |  no cloud required", juce::dontSendNotification);
    footerLabel_.setFont(bodyFont(8.3f));
    footerLabel_.setColour(juce::Label::textColourId, Theme::muted.withAlpha(0.7f));
    footerLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(footerLabel_);

    statsLabel_.setFont(monoFont(9.0f));
    statsLabel_.setColour(juce::Label::textColourId, Theme::muted);
    statsLabel_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(statsLabel_);

    masterLabel_.setText("MASTER", juce::dontSendNotification);
    reverbLabel_.setText("SPACE", juce::dontSendNotification);
    widthLabel_.setText("WIDTH", juce::dontSendNotification);
    for (auto* l : {&masterLabel_, &reverbLabel_, &widthLabel_}) {
        l->setFont(bodyFont(8.5f));
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, Theme::muted);
        addAndMakeVisible(*l);
    }
    auto setupKnob = [this](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 17);
        slider.setColour(juce::Slider::textBoxTextColourId, Theme::text);
        addAndMakeVisible(slider);
    };
    setupKnob(masterSlider_);
    setupKnob(reverbSlider_);
    setupKnob(widthSlider_);
    masterAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.apvts, "gain", masterSlider_);
    reverbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.apvts, "reverb", reverbSlider_);
    widthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.apvts, "width", widthSlider_);

    hintLabel_.setText("Keys: Space = audition  |  Cmd/Ctrl+Z = undo  |  Cmd/Ctrl+Shift+Z = redo", juce::dontSendNotification);
    hintLabel_.setFont(bodyFont(8.0f));
    hintLabel_.setColour(juce::Label::textColourId, Theme::muted.withAlpha(0.7f));
    addAndMakeVisible(hintLabel_);

    lastRevision_ = 0;
    refreshFromProcessor(true);
    setSize(1200, 800);
    startTimerHz(30);
}

ExpressiveComposerAudioProcessorEditor::~ExpressiveComposerAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void ExpressiveComposerAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(Theme::bg);
    juce::ColourGradient gradient(Theme::bg, 0.0f, 0.0f, juce::Colour::fromRGB(19, 22, 28), 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillRect(getLocalBounds());
    g.setColour(Theme::accent.withAlpha(0.7f));
    g.fillRect(0, 0, getWidth(), 2);
    g.setColour(Theme::panel);
    const auto header = juce::Rectangle<int>(0, 0, getWidth(), 74).toFloat();
    g.setColour(Theme::panel.withAlpha(0.55f));
    g.fillRect(header);
    g.setColour(Theme::line);
    g.drawHorizontalLine(73, 0.0f, static_cast<float>(getWidth()));
    g.setColour(Theme::line);
    g.drawHorizontalLine(getHeight() - 30, 0.0f, static_cast<float>(getWidth()));
}

void ExpressiveComposerAudioProcessorEditor::resized() {
    const int W = getWidth();
    const int H = getHeight();
    const int header = 74;
    const int footer = 30;
    titleLabel_.setBounds(20, 12, 330, 32);
    subtitleLabel_.setBounds(22, 44, 340, 18);

    int x = 372;
    chamberButton_.setBounds(x, 20, 86, 30); x += 92;
    cinematicButton_.setBounds(x, 20, 86, 30); x += 92;
    kineticButton_.setBounds(x, 20, 86, 30);

    abButton_.setBounds(W - 18 - 62, 20, 62, 30);
    undoButton_.setBounds(W - 18 - 62 - 68, 20, 62, 30);
    redoButton_.setBounds(W - 18 - 62 - 68 * 2, 20, 62, 30);
    playButton_.setBounds(W - 18 - 62 - 68 * 3, 20, 72, 30);

    const int contentY = header + 12;
    const int contentH = H - header - footer - 22;
    const int leftX = 16;
    const int leftW = 292;
    const int rightW = 238;
    const int rightX = W - 16 - rightW;
    const int centreX = leftX + leftW + 14;
    const int centreW = rightX - 14 - centreX;

    briefLabel_.setBounds(leftX + 16, contentY + 12, leftW - 32, 16);
    briefValue_.setBounds(leftX + 16, contentY + 34, leftW - 32, 62);
    // The lock heading is painted directly; reserve its space here.
    const int lockY = contentY + 118;
    for (int i = 0; i < 6; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        lockButtons_[static_cast<std::size_t>(i)].setBounds(leftX + 16 + col * 132, lockY + row * 36, 124, 28);
    }
    seedLabel_.setBounds(leftX + 16, lockY + 118, 40, 18);
    seedEditor_.setBounds(leftX + 62, lockY + 116, leftW - 78, 24);
    variationLabel_.setBounds(leftX + 16, lockY + 150, 70, 18);
    variationSlider_.setBounds(leftX + 90, lockY + 148, leftW - 106, 24);
    humanizeLabel_.setBounds(leftX + 16, lockY + 180, 90, 18);
    humanizeSlider_.setBounds(leftX + 112, lockY + 178, leftW - 128, 24);
    sectionLabel_.setBounds(leftX + 16, lockY + 216, leftW - 32, 16);
    sectionBox_.setBounds(leftX + 16, lockY + 236, leftW - 32, 26);
    generateButton_.setBounds(leftX + 16, lockY + 276, leftW - 32, 32);
    regenerateButton_.setBounds(leftX + 16, lockY + 314, leftW - 32, 32);
    applyArcsButton_.setBounds(leftX + 16, lockY + 352, leftW - 32, 30);
    humanizeButton_.setBounds(leftX + 16, lockY + 388, leftW - 32, 30);

    if (curveEditor_) curveEditor_->setBounds(centreX + 2, contentY + 8, centreW - 4, 276);
    if (timeline_) timeline_->setBounds(centreX + 2, contentY + 8 + 288, centreW - 4, contentH - 300);

    statsLabel_.setBounds(rightX + 16, contentY + 12, rightW - 32, 66);
    masterLabel_.setBounds(rightX + 12, contentY + 100, 66, 15);
    reverbLabel_.setBounds(rightX + 86, contentY + 100, 66, 15);
    widthLabel_.setBounds(rightX + 160, contentY + 100, 60, 15);
    masterSlider_.setBounds(rightX + 10, contentY + 116, 70, 92);
    reverbSlider_.setBounds(rightX + 84, contentY + 116, 70, 92);
    widthSlider_.setBounds(rightX + 158, contentY + 116, 70, 92);
    saveButton_.setBounds(rightX + 16, contentY + 240, 90, 30);
    loadButton_.setBounds(rightX + 116, contentY + 240, 90, 30);
    midiButton_.setBounds(rightX + 16, contentY + 280, 190, 30);
    hintLabel_.setBounds(16, H - footer - 18, W - 32, 14);
    footerLabel_.setBounds(W - 372, H - footer + 5, 356, 18);
    statusLabel_.setBounds(18, H - footer + 4, W - 400, 18);
}

void ExpressiveComposerAudioProcessorEditor::timerCallback() {
    refreshFromProcessor(false);
}

bool ExpressiveComposerAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::spaceKey) {
        togglePlay();
        return true;
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z') {
        if (key.getModifiers().isShiftDown()) processor_.redo();
        else processor_.undo();
        refreshFromProcessor(true);
        return true;
    }
    return false;
}

void ExpressiveComposerAudioProcessorEditor::refreshFromProcessor(bool force) {
    const auto rev = processor_.revision();
    if (!force && rev == lastRevision_) {
        updateStatus();
        return;
    }
    lastRevision_ = rev;
    project_ = processor_.getProjectCopy();
    if (!draggingCurve_) {
        if (curveEditor_) curveEditor_->setProject(project_);
    }
    if (timeline_) {
        timeline_->setProject(project_);
        timeline_->setSelectedSection(selectedSection_);
    }
    rebuildSectionBox();

    const auto key = ec::keyName(project_.key, true);
    const auto perspective = sv(ec::toString(project_.intent.perspective)).replaceCharacter('_', ' ');
    const auto scene = sv(ec::toString(project_.intent.scene)).replaceCharacter('_', ' ');
    const auto restraint = sv(ec::toString(project_.intent.restraint)).replaceCharacter('_', ' ');
    briefValue_.setText(perspective + " | " + scene + "\n" + restraint + " | " + key + " | " +
                            juce::String(project_.timeSignature.numerator) + "/" +
                            juce::String(project_.timeSignature.denominator) + " | " +
                            juce::String(project_.tempoMap.empty() ? 0.0 : project_.tempoMap.front().bpm, 0) + " BPM",
                        juce::dontSendNotification);

    int notes = 0;
    for (const auto& track : project_.tracks) notes += static_cast<int>(track.notes.size());
    statsLabel_.setText(juce::String(project_.totalBars()) + " BARS  |  " + juce::String(notes) + " NOTES\n" +
                            juce::String(project_.chords.size()) + " CHORDS  |  " + juce::String(project_.phrases.size()) + " PHRASES\n" +
                            juce::String(project_.motifUses.size()) + " MOTIF USES\nSEED " + juce::String(project_.seed),
                        juce::dontSendNotification);
    abButton_.setButtonText(processor_.activeVariant() == 0 ? "A/B | A" : "A/B | B");
    undoButton_.setEnabled(processor_.canUndo());
    redoButton_.setEnabled(processor_.canRedo());
    if (seedEditor_.getText() != juce::String(static_cast<juce::int64>(project_.seed))) {
        seedEditor_.setText(juce::String(static_cast<juce::int64>(project_.seed)), false);
    }
    seed_ = project_.seed;
    playButton_.setButtonText(processor_.isPreviewing() ? "STOP" : "PLAY");
    updateStatus();
}

void ExpressiveComposerAudioProcessorEditor::rebuildSectionBox() {
    const int previous = sectionBox_.getSelectedId();
    sectionBox_.clear(juce::dontSendNotification);
    sectionBox_.addItem("Whole piece", 1);
    for (std::size_t i = 0; i < project_.sections.size(); ++i) {
        sectionBox_.addItem(project_.sections[i].name, static_cast<int>(i) + 2);
    }
    if (previous > 1 && previous <= static_cast<int>(project_.sections.size()) + 1) {
        sectionBox_.setSelectedId(previous, juce::dontSendNotification);
    } else {
        sectionBox_.setSelectedId(1, juce::dontSendNotification);
        selectedSection_ = -1;
    }
}

void ExpressiveComposerAudioProcessorEditor::updateStatus() {
    auto text = processor_.statusMessage();
    if (processor_.isGenerating()) {
        text = "Rendering a new version in the background...";
        generateButton_.setEnabled(false);
        regenerateButton_.setEnabled(false);
    } else {
        generateButton_.setEnabled(true);
        regenerateButton_.setEnabled(true);
    }
    statusLabel_.setText(text, juce::dontSendNotification);
    playButton_.setButtonText(processor_.isPreviewing() ? "STOP" : "PLAY");
}

ec::LockDimension ExpressiveComposerAudioProcessorEditor::lockMask() const {
    ec::LockDimension mask = ec::LockDimension::None;
    const ec::LockDimension bits[6] = {
        ec::LockDimension::Melody, ec::LockDimension::Harmony, ec::LockDimension::Rhythm,
        ec::LockDimension::Orchestration, ec::LockDimension::Humanization, ec::LockDimension::Expression};
    for (int i = 0; i < 6; ++i) {
        if (lockButtons_[static_cast<std::size_t>(i)].getToggleState()) mask |= bits[i];
    }
    return mask;
}

void ExpressiveComposerAudioProcessorEditor::generateDemo(ec::DemoStyle style) {
    seed_ = static_cast<std::uint64_t>(seedEditor_.getText().getLargeIntValue());
    processor_.generateDemo(style, seed_);
    updateStatus();
}

void ExpressiveComposerAudioProcessorEditor::regenerate() {
    const auto mask = lockMask();
    int start = 0;
    int end = -1;
    if (selectedSection_ >= 0 && selectedSection_ < static_cast<int>(project_.sections.size())) {
        start = project_.sections[static_cast<std::size_t>(selectedSection_)].startBar;
        end = start + project_.sections[static_cast<std::size_t>(selectedSection_)].bars;
    }
    seed_ = static_cast<std::uint64_t>(seedEditor_.getText().getLargeIntValue());
    processor_.regenerate(mask, start, end, static_cast<float>(variationSlider_.getValue()), seed_);
    updateStatus();
}

void ExpressiveComposerAudioProcessorEditor::togglePlay() {
    processor_.setPreviewing(!processor_.isPreviewing());
    playButton_.setButtonText(processor_.isPreviewing() ? "STOP" : "PLAY");
    updateStatus();
}

void ExpressiveComposerAudioProcessorEditor::setStatusText(const juce::String& text) {
    statusLabel_.setText(text, juce::dontSendNotification);
}

void ExpressiveComposerAudioProcessorEditor::showSaveDialog() {
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Save Expressive Composer project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("expressive-composer.json"),
        "*.json");
    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode |
                                  juce::FileBrowserComponent::canSelectFiles |
                                  juce::FileBrowserComponent::warnAboutOverwriting,
                              [this](const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File{}) return;
                                  juce::String error;
                                  processor_.saveProjectFile(file, error);
                                  if (error.isNotEmpty()) {
                                      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                                             "Save failed", error);
                                  } else {
                                      setStatusText("Project saved: " + file.getFileName());
                                  }
                              });
}

void ExpressiveComposerAudioProcessorEditor::showLoadDialog() {
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Load Expressive Composer project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json");
    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode |
                                  juce::FileBrowserComponent::canSelectFiles,
                              [this](const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File{}) return;
                                  juce::String error;
                                  processor_.loadProjectFile(file, error);
                                  if (error.isNotEmpty()) {
                                      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                                             "Load failed", error);
                                  } else {
                                      refreshFromProcessor(true);
                                      setStatusText("Project loaded: " + file.getFileName());
                                  }
                              });
}

void ExpressiveComposerAudioProcessorEditor::showMidiDialog() {
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Export generated MIDI",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("expressive-composer.mid"),
        "*.mid");
    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode |
                                  juce::FileBrowserComponent::canSelectFiles |
                                  juce::FileBrowserComponent::warnAboutOverwriting,
                              [this](const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File{}) return;
                                  juce::String error;
                                  processor_.exportMidiFile(file, error);
                                  if (error.isNotEmpty()) {
                                      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                                             "MIDI export failed", error);
                                  } else {
                                      setStatusText("MIDI exported: " + file.getFileName());
                                  }
                              });
}
