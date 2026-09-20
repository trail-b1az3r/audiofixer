#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <functional>
#include <cstdint>
#include "BluetoothDeviceMonitor.hpp"

enum class LatencySource {
    CALIBRATION,
    MEASURED,
    ESTIMATED,
    DEFAULT
};

struct CalibrationStats {
    int sampleCount = 0;
    int targetSamples = 32;
    float medianMs = 0.0f;
    float averageMs = 0.0f;
    float stdDevMs = 0.0f;
    float finalOffsetMs = 0.0f;
    std::string confidence = "None";
    LatencySource source = LatencySource::DEFAULT;
    std::string sourceName = "Default";
    bool isComplete = false;
};

class LatencyCalibration {
public:
    static LatencyCalibration& get();

    void startCalibration(int targetSamples = 32);
    void stopCalibration();
    bool isRunning() const;

    // Called when the calibration metronome tick fires (returns true if visual flash requested)
    bool tick(double currentDspTime);

    // Called when user records a tap
    void recordUserTap(double currentDspTime);

    CalibrationStats getCurrentStats() const;
    void resetCurrentDeviceCalibration();

    // Resolves current active calibration for given device
    CalibrationStats resolveDeviceCalibration(const BluetoothDeviceInfo& dev);

    void setStatsUpdateCallback(std::function<void(const CalibrationStats&)> cb);

private:
    LatencyCalibration();

    mutable std::mutex mutex;
    bool running = false;
    int targetSampleCount = 32;
    double nextTickDspTime = 0.0;
    double lastTickDspTime = 0.0;
    std::vector<float> tapDeltasMs;
    CalibrationStats currentStats;
    std::function<void(const CalibrationStats&)> onStatsUpdate;

    void calculateStats();
};
