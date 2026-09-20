#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cassert>

// Test standalone adaptive controller logic
struct TimingObservation {
    double timestamp;
    float deltaMs;
};

class TestAdaptiveController {
public:
    std::deque<TimingObservation> observations;
    float currentAdaptiveOffsetMs = 0.0f;
    float smoothedSyncErrorMs = 0.0f;
    float lastAppliedDirection = 0.0f;
    std::string confidence = "Idle";
    bool adaptingActive = false;

    float deadbandMs = 3.0f;
    float maxAdaptiveCorrectionMs = 250.0f;
    float correctionSpeed = 2.0f;

    void reset() {
        observations.clear();
        currentAdaptiveOffsetMs = 0.0f;
        smoothedSyncErrorMs = 0.0f;
        lastAppliedDirection = 0.0f;
        confidence = "Idle";
        adaptingActive = false;
    }

    void addObservation(float deltaMs, double currentDspTime) {
        observations.push_back({currentDspTime, deltaMs});
        double cutoff = currentDspTime - 3.0;
        while (!observations.empty() && observations.front().timestamp < cutoff) {
            observations.pop_front();
        }
    }

    void evaluateWindow() {
        constexpr size_t kMinObservations = 20;
        if (observations.size() < kMinObservations) {
            confidence = "Collecting";
            adaptingActive = false;
            return;
        }

        std::vector<float> values;
        for (const auto& obs : observations) values.push_back(obs.deltaMs);

        float sum = std::accumulate(values.begin(), values.end(), 0.0f);
        float mean = sum / values.size();
        float sqSum = 0.0f;
        for (float v : values) sqSum += (v - mean) * (v - mean);
        float stdDev = std::sqrt(sqSum / values.size());

        std::vector<float> inliers;
        for (float v : values) {
            if (std::abs(v - mean) <= 2.0f * stdDev) inliers.push_back(v);
        }

        if (inliers.size() < 10) {
            confidence = "Unstable";
            adaptingActive = false;
            return;
        }

        std::sort(inliers.begin(), inliers.end());
        float median = inliers[inliers.size() / 2];
        smoothedSyncErrorMs = 0.8f * smoothedSyncErrorMs + 0.2f * median;

        if (std::abs(smoothedSyncErrorMs) <= deadbandMs) {
            adaptingActive = true;
            return;
        }

        float errorSign = (smoothedSyncErrorMs > 0.0f) ? 1.0f : -1.0f;
        if (lastAppliedDirection != 0.0f && errorSign != lastAppliedDirection) {
            if (std::abs(smoothedSyncErrorMs) < deadbandMs * 1.5f) return;
        }
        lastAppliedDirection = errorSign;

        float targetChange = errorSign * correctionSpeed;
        if (std::abs(smoothedSyncErrorMs) < correctionSpeed * 2.0f) targetChange *= 0.5f;

        currentAdaptiveOffsetMs += targetChange;
        float maxBound = std::abs(maxAdaptiveCorrectionMs);
        currentAdaptiveOffsetMs = std::clamp(currentAdaptiveOffsetMs, -maxBound, maxBound);
        adaptingActive = true;
    }
};

int main() {
    std::cout << "Starting Adaptive & Calibration Logic Unit Tests...\n";

    TestAdaptiveController controller;

    // Test 1: Minimum sample requirement
    for (int i = 0; i < 15; ++i) {
        controller.addObservation(20.0f, i * 0.1);
    }
    controller.evaluateWindow();
    assert(!controller.adaptingActive);
    assert(controller.currentAdaptiveOffsetMs == 0.0f);
    std::cout << "[PASS] Test 1: Minimum observation count enforced (< 20 samples ignored)\n";

    // Test 2: Outlier rejection & adaptation
    for (int i = 15; i < 30; ++i) {
        controller.addObservation(25.0f, i * 0.1);
    }
    // Add one massive outlier
    controller.addObservation(999.0f, 30 * 0.1);
    controller.evaluateWindow();
    assert(controller.adaptingActive);
    assert(controller.currentAdaptiveOffsetMs > 0.0f);
    assert(controller.currentAdaptiveOffsetMs <= controller.correctionSpeed);
    std::cout << "[PASS] Test 2: Outlier rejected, smooth adjustment within speed limit (" << controller.currentAdaptiveOffsetMs << " ms)\n";

    // Test 3: Deadband enforcement
    controller.reset();
    for (int i = 0; i < 25; ++i) {
        controller.addObservation(2.0f, i * 0.1); // 2.0ms < 3.0ms deadband
    }
    controller.evaluateWindow();
    assert(controller.currentAdaptiveOffsetMs == 0.0f);
    std::cout << "[PASS] Test 3: Deadband correctly suppresses micro-jitter < 3ms\n";

    // Test 4: Maximum bound clamping
    for (int step = 0; step < 500; ++step) {
        for (int i = 0; i < 25; ++i) {
            controller.addObservation(100.0f, step * 3.0 + i * 0.1);
        }
        controller.evaluateWindow();
    }
    assert(controller.currentAdaptiveOffsetMs == 250.0f);
    std::cout << "[PASS] Test 4: Absolute bound clamp enforced at exactly +250.0 ms\n";

    // Test 5: Reset cleans state
    controller.reset();
    assert(controller.currentAdaptiveOffsetMs == 0.0f);
    assert(controller.observations.empty());
    std::cout << "[PASS] Test 5: Reset clears all temporary adaptive corrections cleanly\n";

    std::cout << "All automated unit tests passed successfully!\n";
    return 0;
}
