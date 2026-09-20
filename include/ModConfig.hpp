#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

struct DeviceCalibration {
    float baseOffsetMs = 0.0f;
    float stdDev = 0.0f;
    int sampleCount = 0;
    std::string confidence = "Unknown";
    int64_t timestamp = 0;
    std::string deviceName = "";
};

class ModConfig {
public:
    static ModConfig& get();

    void load();
    void save();

    bool modEnabled = true;
    float minOffsetMs = -1000.0f;
    float maxOffsetMs = 1000.0f;
    bool autoBluetoothCalibration = true;
    bool adaptiveCorrectionEnabled = true;
    float maxAdaptiveCorrectionMs = 250.0f;
    float correctionSpeed = 2.0f; // ms per adjustment interval
    float deadbandMs = 3.0f;       // ms below which no adjustment occurs
    float observationWindowSec = 3.0f; // observation window
    bool retainAdaptiveAfterLevel = false;
    bool showDebugOverlay = false;
    std::string manualDeviceProfile = "Auto";

    std::unordered_map<std::string, DeviceCalibration> deviceCalibrations;

    bool hasDeviceCalibration(const std::string& deviceId) const;
    DeviceCalibration getDeviceCalibration(const std::string& deviceId) const;
    void setDeviceCalibration(const std::string& deviceId, const DeviceCalibration& cal);
    void removeDeviceCalibration(const std::string& deviceId);

private:
    ModConfig() = default;
    std::string getConfigPath() const;
};
