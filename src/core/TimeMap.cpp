#include "core/TimeMap.hpp"

#include <algorithm>
#include <cmath>

namespace ec {

TimeMap::TimeMap(std::vector<TempoPoint> tempo, int ppq) {
    setTempoMap(std::move(tempo), ppq);
}

void TimeMap::setTempoMap(std::vector<TempoPoint> tempo, int ppq) {
    ppq_ = std::clamp(ppq, 24, 9600);
    tempo_ = std::move(tempo);
    if (tempo_.empty()) tempo_.push_back({0, 120.0});
    for (auto& point : tempo_) {
        point.tick = std::max<Tick>(0, point.tick);
        if (!std::isfinite(point.bpm)) point.bpm = 120.0;
        point.bpm = std::clamp(point.bpm, 10.0, 600.0);
    }
    std::stable_sort(tempo_.begin(), tempo_.end(), [](const TempoPoint& a, const TempoPoint& b) {
        return a.tick < b.tick;
    });
    if (tempo_.front().tick != 0) tempo_.insert(tempo_.begin(), {0, tempo_.front().bpm});
    // Remove duplicate ticks while keeping the last value.
    std::vector<TempoPoint> clean;
    for (const auto& p : tempo_) {
        if (!clean.empty() && clean.back().tick == p.tick) clean.back().bpm = p.bpm;
        else clean.push_back(p);
    }
    tempo_ = std::move(clean);
}

double TimeMap::bpmAtTick(Tick tick) const {
    double bpm = tempo_.front().bpm;
    for (const auto& point : tempo_) {
        if (point.tick <= tick) bpm = point.bpm;
        else break;
    }
    return bpm;
}

double TimeMap::bpmAtSeconds(double seconds) const {
    return bpmAtTick(tickAtSeconds(seconds));
}

double TimeMap::secondsAtTick(Tick tick) const {
    if (tick <= 0) return 0.0;
    double seconds = 0.0;
    for (std::size_t i = 0; i < tempo_.size(); ++i) {
        const Tick start = tempo_[i].tick;
        const Tick end = (i + 1 < tempo_.size()) ? tempo_[i + 1].tick
                                                 : std::numeric_limits<Tick>::max();
        if (tick <= start) break;
        const Tick sliceEnd = std::min(tick, end);
        const double ticks = static_cast<double>(sliceEnd - start);
        seconds += ticks * 60.0 / (tempo_[i].bpm * static_cast<double>(ppq_));
        if (tick <= end) break;
    }
    return seconds;
}

Tick TimeMap::tickAtSeconds(double seconds) const {
    if (!std::isfinite(seconds) || seconds <= 0.0) return 0;
    double remaining = seconds;
    for (std::size_t i = 0; i < tempo_.size(); ++i) {
        const Tick start = tempo_[i].tick;
        const Tick end = (i + 1 < tempo_.size()) ? tempo_[i + 1].tick
                                                 : std::numeric_limits<Tick>::max();
        if (end == std::numeric_limits<Tick>::max()) {
            const double ticks = remaining * tempo_[i].bpm * static_cast<double>(ppq_) / 60.0;
            return start + static_cast<Tick>(std::llround(ticks));
        }
        const double segmentSeconds = static_cast<double>(end - start) * 60.0 /
                                      (tempo_[i].bpm * static_cast<double>(ppq_));
        if (remaining < segmentSeconds) {
            const double ticks = remaining * tempo_[i].bpm * static_cast<double>(ppq_) / 60.0;
            return start + static_cast<Tick>(std::llround(ticks));
        }
        remaining -= segmentSeconds;
    }
    return 0;
}

double TimeMap::beatsAtTick(Tick tick) const {
    return static_cast<double>(tick) / static_cast<double>(ppq_);
}

Tick TimeMap::tickAtBeats(double beats) const {
    if (!std::isfinite(beats)) return 0;
    return static_cast<Tick>(std::llround(beats * static_cast<double>(ppq_)));
}

double TimeMap::secondsPerBeatAtTick(Tick tick) const {
    return 60.0 / bpmAtTick(tick);
}

} // namespace ec
