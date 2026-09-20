#pragma once

#include <string>
#include <mutex>
#include "BluetoothDeviceMonitor.hpp"
#include "LatencyCalibration.hpp"

class AudioOffsetManager {
public:
    static AudioOffsetManager& get();

    void init();

    // Re-evaluates offset when device state changes or user modifies base
    void refreshDeviceState();

    float getBaseOffsetMs() const;
    void setBaseOffsetMs(float offsetMs);

    float getAdaptiveOffsetMs() const;
    float getEffectiveOffsetMs() const;
    float getEffectiveOffsetSeconds() const;

    std::string getActiveSourceName() const;
    std::string getActiveConfidence() const;
    std::string getActiveDeviceName() const;

    void onLevelStart();
    void onLevelEnd();

private:
    AudioOffsetManager();

    mutable std::mutex mutex;
    float baseOffsetMs = 0.0f;
    std::string activeSource = "Default";
    std::string activeConfidence = "Default";
    std::string activeDeviceName = "Headset Speakers";
};
