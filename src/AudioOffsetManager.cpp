#include "AudioOffsetManager.hpp"
#include "ModConfig.hpp"
#include "AdaptiveController.hpp"
#include "main.hpp"
#include <algorithm>

AudioOffsetManager& AudioOffsetManager::get() {
    static AudioOffsetManager instance;
    return instance;
}

AudioOffsetManager::AudioOffsetManager() = default;

void AudioOffsetManager::init() {
    BluetoothDeviceMonitor::get().setDeviceChangeCallback([this](const BluetoothDeviceInfo& newDev) {
        LOG_INFO("Bluetooth device transition detected: {} (connected: {})", newDev.deviceName, newDev.connected);
        refreshDeviceState();
    });
    refreshDeviceState();
}

void AudioOffsetManager::refreshDeviceState() {
    std::lock_guard<std::mutex> lock(mutex);
    auto dev = BluetoothDeviceMonitor::get().getCurrentDevice();
    activeDeviceName = dev.deviceName;

    auto stats = LatencyCalibration::get().resolveDeviceCalibration(dev);
    baseOffsetMs = stats.finalOffsetMs;
    activeSource = stats.sourceName;
    activeConfidence = stats.confidence;

    LOG_INFO("Refreshed AudioOffset: Base={}ms, Source={}, Confidence={}, Device={}",
        baseOffsetMs, activeSource, activeConfidence, activeDeviceName);
}

float AudioOffsetManager::getBaseOffsetMs() const {
    std::lock_guard<std::mutex> lock(mutex);
    return baseOffsetMs;
}

void AudioOffsetManager::setBaseOffsetMs(float offsetMs) {
    auto& config = ModConfig::get();
    offsetMs = std::clamp(offsetMs, config.minOffsetMs, config.maxOffsetMs);

    {
        std::lock_guard<std::mutex> lock(mutex);
        baseOffsetMs = offsetMs;
        activeSource = "User Configured";
        activeConfidence = "Manual";
    }

    auto dev = BluetoothDeviceMonitor::get().getCurrentDevice();
    DeviceCalibration cal;
    cal.baseOffsetMs = offsetMs;
    cal.confidence = "Manual";
    cal.deviceName = dev.deviceName;
    cal.sampleCount = 1;
    cal.stdDev = 0.0f;
    config.setDeviceCalibration(dev.deviceId, cal);
}

float AudioOffsetManager::getAdaptiveOffsetMs() const {
    return AdaptiveController::get().getAdaptiveOffsetMs();
}

float AudioOffsetManager::getEffectiveOffsetMs() const {
    auto& config = ModConfig::get();
    if (!config.modEnabled) {
        return 0.0f;
    }

    float base = getBaseOffsetMs();
    float adaptive = (config.adaptiveCorrectionEnabled) ? getAdaptiveOffsetMs() : 0.0f;
    float effective = base + adaptive;
    return std::clamp(effective, config.minOffsetMs, config.maxOffsetMs);
}

float AudioOffsetManager::getEffectiveOffsetSeconds() const {
    return getEffectiveOffsetMs() / 1000.0f;
}

std::string AudioOffsetManager::getActiveSourceName() const {
    std::lock_guard<std::mutex> lock(mutex);
    return activeSource;
}

std::string AudioOffsetManager::getActiveConfidence() const {
    if (ModConfig::get().adaptiveCorrectionEnabled && AdaptiveController::get().isAdapting()) {
        return AdaptiveController::get().getConfidence();
    }
    std::lock_guard<std::mutex> lock(mutex);
    return activeConfidence;
}

std::string AudioOffsetManager::getActiveDeviceName() const {
    std::lock_guard<std::mutex> lock(mutex);
    return activeDeviceName;
}

void AudioOffsetManager::onLevelStart() {
    AdaptiveController::get().reset();
    LOG_INFO("Level started: effective audio offset is {} ms", getEffectiveOffsetMs());
}

void AudioOffsetManager::onLevelEnd() {
    auto& config = ModConfig::get();
    if (config.retainAdaptiveAfterLevel && config.adaptiveCorrectionEnabled) {
        float adapt = AdaptiveController::get().getAdaptiveOffsetMs();
        if (std::abs(adapt) > 1.0f) {
            setBaseOffsetMs(getBaseOffsetMs() + adapt);
            LOG_INFO("Retained adaptive correction of {}ms into base calibration", adapt);
        }
    }
    AdaptiveController::get().reset();
    LOG_INFO("Level ended: adaptive controller reset");
}
