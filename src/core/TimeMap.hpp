#pragma once

#include "core/Types.hpp"

#include <vector>

namespace ec {

class TimeMap {
public:
    TimeMap() = default;
    TimeMap(std::vector<TempoPoint> tempo, int ppq = kPPQ);

    void setTempoMap(std::vector<TempoPoint> tempo, int ppq = kPPQ);
    const std::vector<TempoPoint>& tempoPoints() const { return tempo_; }
    int ppq() const { return ppq_; }

    double bpmAtTick(Tick tick) const;
    double bpmAtSeconds(double seconds) const;
    double secondsAtTick(Tick tick) const;
    Tick tickAtSeconds(double seconds) const;
    double beatsAtTick(Tick tick) const;
    Tick tickAtBeats(double beats) const;
    double secondsPerBeatAtTick(Tick tick) const;

private:
    std::vector<TempoPoint> tempo_;
    int ppq_ = kPPQ;
};

} // namespace ec
