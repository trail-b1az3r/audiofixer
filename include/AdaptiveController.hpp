#pragma once

#include <vector>
#include <deque>
#include <string>
#include <mutex>

struct TimingObservation {
    double timestamp;
    float deltaMs;
};

class AdaptiveController {
public:
    static AdaptiveController& get();

    void reset();
    void addObservation(float deltaMs, double currentDspTime);
    void update(float deltaTime);

    float getAdaptiveOffsetMs() const;
    std::string getConfidence() const;
    bool isAdapting() const;
    int getObservationCount() const;
    float getSmoothedSyncErrorMs() const;

private:
    AdaptiveController();

    mutable std::mutex mutex;
    std::deque<TimingObservation> observations;

    float currentAdaptiveOffsetMs = 0.0f;
    float smoothedSyncErrorMs = 0.0f;
    float lastAppliedDirection = 0.0f;
    std::string confidence = "Initializing";
    bool adaptingActive = false;
    double timeSinceLastEval = 0.0;

    void evaluateWindow();
};
