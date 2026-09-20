#include "AdaptiveController.hpp"
#include "ModConfig.hpp"
#include "main.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

AdaptiveController& AdaptiveController::get() {
    static AdaptiveController instance;
    return instance;
}

AdaptiveController::AdaptiveController() = default;

void AdaptiveController::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    observations.clear();
    currentAdaptiveOffsetMs = 0.0f;
    smoothedSyncErrorMs = 0.0f;
    lastAppliedDirection = 0.0f;
    confidence = "Idle";
    adaptingActive = false;
    timeSinceLastEval = 0.0;
}

void AdaptiveController::addObservation(float deltaMs, double currentDspTime) {
    auto& config = ModConfig::get();
    if (!config.modEnabled || !config.adaptiveCorrectionEnabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);
    observations.push_back(TimingObservation{currentDspTime, deltaMs});

    // Prune observations older than the observation window
    double cutoff = currentDspTime - config.observationWindowSec;
    while (!observations.empty() && observations.front().timestamp < cutoff) {
        observations.pop_front();
    }
}

void AdaptiveController::update(float deltaTime) {
    auto& config = ModConfig::get();
    if (!config.modEnabled || !config.adaptiveCorrectionEnabled) {
        std::lock_guard<std::mutex> lock(mutex);
        currentAdaptiveOffsetMs = 0.0f;
        adaptingActive = false;
        confidence = "Disabled";
        return;
    }

    timeSinceLastEval += deltaTime;
    // Evaluate every 0.25 seconds to provide smooth, stable adjustments
    if (timeSinceLastEval < 0.25) {
        return;
    }
    timeSinceLastEval = 0.0;

    evaluateWindow();
}

void AdaptiveController::evaluateWindow() {
    std::lock_guard<std::mutex> lock(mutex);
    auto& config = ModConfig::get();

    // Check minimum observations
    constexpr size_t kMinObservations = 20;
    if (observations.size() < kMinObservations) {
        confidence = "Collecting (" + std::to_string(observations.size()) + "/" + std::to_string(kMinObservations) + ")";
        adaptingActive = false;
        return;
    }

    // 1. Extract values
    std::vector<float> values;
    values.reserve(observations.size());
    for (const auto& obs : observations) {
        values.push_back(obs.deltaMs);
    }

    // 2. Statistical analysis & Outlier Rejection
    float sum = std::accumulate(values.begin(), values.end(), 0.0f);
    float mean = sum / static_cast<float>(values.size());

    float sqSum = 0.0f;
    for (float v : values) {
        sqSum += (v - mean) * (v - mean);
    }
    float variance = sqSum / static_cast<float>(values.size());
    float stdDev = std::sqrt(variance);

    // Reject outliers beyond 2 * sigma
    std::vector<float> inliers;
    inliers.reserve(values.size());
    for (float v : values) {
        if (std::abs(v - mean) <= 2.0f * stdDev) {
            inliers.push_back(v);
        }
    }

    if (inliers.size() < 10) {
        confidence = "Unstable (High Outliers)";
        adaptingActive = false;
        return;
    }

    // Median of inliers
    std::sort(inliers.begin(), inliers.end());
    float median = inliers[inliers.size() / 2];
    smoothedSyncErrorMs = 0.8f * smoothedSyncErrorMs + 0.2f * median;

    // Confidence metric
    if (stdDev < 5.0f && inliers.size() >= 25) {
        confidence = "High (±" + std::to_string(static_cast<int>(stdDev)) + "ms)";
    } else if (stdDev < 15.0f) {
        confidence = "Medium (±" + std::to_string(static_cast<int>(stdDev)) + "ms)";
    } else {
        confidence = "Low (±" + std::to_string(static_cast<int>(stdDev)) + "ms)";
    }

    // 3. Apply Deadband
    if (std::abs(smoothedSyncErrorMs) <= config.deadbandMs) {
        // Inside deadband, keep current adjustment steady without jitter
        adaptingActive = true;
        return;
    }

    // 4. Apply Hysteresis
    // If the error direction flips, require error to exceed deadband significantly before reversing
    float errorSign = (smoothedSyncErrorMs > 0.0f) ? 1.0f : -1.0f;
    if (lastAppliedDirection != 0.0f && errorSign != lastAppliedDirection) {
        if (std::abs(smoothedSyncErrorMs) < config.deadbandMs * 1.5f) {
            return;
        }
    }
    lastAppliedDirection = errorSign;

    // 5. Limit correction velocity
    float targetChange = errorSign * config.correctionSpeed;
    // Scale step if error is small
    if (std::abs(smoothedSyncErrorMs) < config.correctionSpeed * 2.0f) {
        targetChange *= 0.5f;
    }

    currentAdaptiveOffsetMs += targetChange;

    // 6. Clamp total adaptive correction within bounds
    float maxBound = std::abs(config.maxAdaptiveCorrectionMs);
    currentAdaptiveOffsetMs = std::clamp(currentAdaptiveOffsetMs, -maxBound, maxBound);

    adaptingActive = true;
}

float AdaptiveController::getAdaptiveOffsetMs() const {
    std::lock_guard<std::mutex> lock(mutex);
    return currentAdaptiveOffsetMs;
}

std::string AdaptiveController::getConfidence() const {
    std::lock_guard<std::mutex> lock(mutex);
    return confidence;
}

bool AdaptiveController::isAdapting() const {
    std::lock_guard<std::mutex> lock(mutex);
    return adaptingActive;
}

int AdaptiveController::getObservationCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(observations.size());
}

float AdaptiveController::getSmoothedSyncErrorMs() const {
    std::lock_guard<std::mutex> lock(mutex);
    return smoothedSyncErrorMs;
}
