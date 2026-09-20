#include "LatencyCalibration.hpp"
#include "ModConfig.hpp"
#include "main.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <chrono>

LatencyCalibration& LatencyCalibration::get() {
    static LatencyCalibration instance;
    return instance;
}

LatencyCalibration::LatencyCalibration() = default;

void LatencyCalibration::startCalibration(int targetSamples) {
    std::lock_guard<std::mutex> lock(mutex);
    running = true;
    targetSampleCount = targetSamples;
    tapDeltasMs.clear();
    tapDeltasMs.reserve(targetSamples);
    nextTickDspTime = 0.0;
    lastTickDspTime = 0.0;

    currentStats = CalibrationStats{};
    currentStats.targetSamples = targetSamples;
    currentStats.confidence = "In Progress";
    currentStats.source = LatencySource::CALIBRATION;
    currentStats.sourceName = "Calibration";

    LOG_INFO("Started latency calibration for {} target samples", targetSamples);
}

void LatencyCalibration::stopCalibration() {
    std::lock_guard<std::mutex> lock(mutex);
    running = false;
    LOG_INFO("Stopped latency calibration");
}

bool LatencyCalibration::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex);
    return running;
}

bool LatencyCalibration::tick(double currentDspTime) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running) return false;

    if (nextTickDspTime == 0.0) {
        // Schedule first tick 0.5s into the future
        nextTickDspTime = currentDspTime + 0.5;
    }

    if (currentDspTime >= nextTickDspTime) {
        lastTickDspTime = nextTickDspTime;
        // Schedule next tick every 600ms
        nextTickDspTime += 0.6;
        return true; // Request visual and audio pulse
    }
    return false;
}

void LatencyCalibration::recordUserTap(double currentDspTime) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running || lastTickDspTime == 0.0) return;

    // Calculate signed error relative to nearest tick
    double deltaSec = currentDspTime - lastTickDspTime;
    // If tap was slightly early for the upcoming tick, wrap around
    if (deltaSec > 0.3) {
        deltaSec -= 0.6;
    }

    float deltaMs = static_cast<float>(deltaSec * 1000.0);
    tapDeltasMs.push_back(deltaMs);

    calculateStats();

    if (static_cast<int>(tapDeltasMs.size()) >= targetSampleCount) {
        running = false;
        currentStats.isComplete = true;

        // Persist calibration for current Bluetooth device
        auto currentDev = BluetoothDeviceMonitor::get().getCurrentDevice();
        DeviceCalibration devCal;
        devCal.baseOffsetMs = currentStats.finalOffsetMs;
        devCal.stdDev = currentStats.stdDevMs;
        devCal.sampleCount = currentStats.sampleCount;
        devCal.confidence = currentStats.confidence;
        devCal.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        devCal.deviceName = currentDev.deviceName;

        ModConfig::get().setDeviceCalibration(currentDev.deviceId, devCal);
        LOG_INFO("Calibration completed and saved for {}: {}ms (confidence: {})",
            currentDev.deviceId, currentStats.finalOffsetMs, currentStats.confidence);
    }

    if (onStatsUpdate) {
        onStatsUpdate(currentStats);
    }
}

void LatencyCalibration::calculateStats() {
    if (tapDeltasMs.empty()) return;

    currentStats.sampleCount = static_cast<int>(tapDeltasMs.size());

    // Filter outliers using 2-sigma
    float sum = std::accumulate(tapDeltasMs.begin(), tapDeltasMs.end(), 0.0f);
    float mean = sum / static_cast<float>(tapDeltasMs.size());

    float sqSum = 0.0f;
    for (float v : tapDeltasMs) {
        sqSum += (v - mean) * (v - mean);
    }
    float variance = sqSum / static_cast<float>(tapDeltasMs.size());
    float stdDev = std::sqrt(variance);

    std::vector<float> inliers;
    for (float v : tapDeltasMs) {
        if (std::abs(v - mean) <= 2.0f * (stdDev > 5.0f ? stdDev : 5.0f)) {
            inliers.push_back(v);
        }
    }

    if (inliers.empty()) inliers = tapDeltasMs;

    std::sort(inliers.begin(), inliers.end());
    float median = inliers[inliers.size() / 2];
    float inlierMean = std::accumulate(inliers.begin(), inliers.end(), 0.0f) / static_cast<float>(inliers.size());

    currentStats.medianMs = median;
    currentStats.averageMs = inlierMean;
    currentStats.stdDevMs = stdDev;
    currentStats.finalOffsetMs = std::round(median);

    if (currentStats.sampleCount >= 10) {
        if (stdDev < 10.0f) {
            currentStats.confidence = "High";
        } else if (stdDev < 25.0f) {
            currentStats.confidence = "Medium";
        } else {
            currentStats.confidence = "Low";
        }
    } else {
        currentStats.confidence = "Collecting (" + std::to_string(currentStats.sampleCount) + "/" + std::to_string(targetSampleCount) + ")";
    }
}

void LatencyCalibration::resetCurrentDeviceCalibration() {
    auto currentDev = BluetoothDeviceMonitor::get().getCurrentDevice();
    ModConfig::get().removeDeviceCalibration(currentDev.deviceId);
    std::lock_guard<std::mutex> lock(mutex);
    currentStats = resolveDeviceCalibration(currentDev);
    if (onStatsUpdate) {
        onStatsUpdate(currentStats);
    }
}

CalibrationStats LatencyCalibration::getCurrentStats() const {
    std::lock_guard<std::mutex> lock(mutex);
    return currentStats;
}

CalibrationStats LatencyCalibration::resolveDeviceCalibration(const BluetoothDeviceInfo& dev) {
    CalibrationStats stats;
    auto& config = ModConfig::get();

    // Priority 1: Persistent user calibration
    if (config.hasDeviceCalibration(dev.deviceId)) {
        auto saved = config.getDeviceCalibration(dev.deviceId);
        stats.sampleCount = saved.sampleCount;
        stats.targetSamples = 32;
        stats.finalOffsetMs = saved.baseOffsetMs;
        stats.medianMs = saved.baseOffsetMs;
        stats.averageMs = saved.baseOffsetMs;
        stats.stdDevMs = saved.stdDev;
        stats.confidence = saved.confidence;
        stats.source = LatencySource::CALIBRATION;
        stats.sourceName = "Calibration";
        stats.isComplete = true;
        return stats;
    }

    // Priority 2: Actual Android-reported output latency
    if (dev.outputLatencyMs > 0) {
        stats.sampleCount = 1;
        stats.targetSamples = 1;
        stats.finalOffsetMs = static_cast<float>(dev.outputLatencyMs);
        stats.medianMs = stats.finalOffsetMs;
        stats.averageMs = stats.finalOffsetMs;
        stats.stdDevMs = 0.0f;
        stats.confidence = "Hardware Measured";
        stats.source = LatencySource::MEASURED;
        stats.sourceName = "Android OS / Measured";
        stats.isComplete = true;
        return stats;
    }

    // Priority 3: Manual or Identified Model Estimate
    std::string profile = config.manualDeviceProfile;
    if (profile == "Auto") {
        if (dev.isAirPodsPro3) {
            profile = "AirPods Pro 3";
        } else if (dev.detectedModel == "AirPods Pro (2nd gen)") {
            profile = "AirPods Pro 2";
        } else if (dev.isAirPods) {
            profile = "AirPods Pro";
        }
    }

    if (profile == "AirPods Pro 3") {
        stats.finalOffsetMs = 185.0f;
        stats.confidence = "Estimated (Profile: AirPods Pro 3)";
        stats.source = LatencySource::ESTIMATED;
        stats.sourceName = "Estimated";
    } else if (profile == "AirPods Pro 2" || profile == "AirPods Pro") {
        stats.finalOffsetMs = 175.0f;
        stats.confidence = "Estimated (Profile: AirPods Pro)";
        stats.source = LatencySource::ESTIMATED;
        stats.sourceName = "Estimated";
    } else if (profile == "AirPods Max") {
        stats.finalOffsetMs = 165.0f;
        stats.confidence = "Estimated (Profile: AirPods Max)";
        stats.source = LatencySource::ESTIMATED;
        stats.sourceName = "Estimated";
    } else if (profile == "Generic Low Latency") {
        stats.finalOffsetMs = 100.0f;
        stats.confidence = "Estimated (Low Latency BT)";
        stats.source = LatencySource::ESTIMATED;
        stats.sourceName = "Estimated";
    } else if (profile == "Generic High Latency") {
        stats.finalOffsetMs = 250.0f;
        stats.confidence = "Estimated (High Latency BT)";
        stats.source = LatencySource::ESTIMATED;
        stats.sourceName = "Estimated";
    } else {
        stats.finalOffsetMs = 0.0f;
        stats.confidence = "Default (Headset Speakers)";
        stats.source = LatencySource::DEFAULT;
        stats.sourceName = "Default";
    }

    stats.medianMs = stats.finalOffsetMs;
    stats.averageMs = stats.finalOffsetMs;
    stats.stdDevMs = 0.0f;
    stats.sampleCount = 0;
    stats.targetSamples = 32;
    stats.isComplete = false;
    return stats;
}

void LatencyCalibration::setStatsUpdateCallback(std::function<void(const CalibrationStats&)> cb) {
    std::lock_guard<std::mutex> lock(mutex);
    onStatsUpdate = cb;
}
