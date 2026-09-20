#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>

struct BluetoothDeviceInfo {
    bool connected = false;
    std::string deviceId = "none";
    std::string deviceName = "No Bluetooth Device";
    std::string detectedModel = "None";
    std::string audioProfile = "None";
    std::string codec = "Unavailable";
    int outputLatencyMs = -1; // -1 if unavailable
    bool isAirPods = false;
    bool isAirPodsPro3 = false;
};

class BluetoothDeviceMonitor {
public:
    static BluetoothDeviceMonitor& get();

    void start();
    void stop();

    BluetoothDeviceInfo getCurrentDevice();
    void setDeviceChangeCallback(std::function<void(const BluetoothDeviceInfo&)> callback);

    // Forces an immediate poll (safe from background thread or main thread)
    void pollNow();

private:
    BluetoothDeviceMonitor();
    ~BluetoothDeviceMonitor();

    void monitorLoop();
    BluetoothDeviceInfo queryAndroidAudioState();

    std::atomic<bool> running{false};
    std::thread workerThread;
    std::mutex stateMutex;
    BluetoothDeviceInfo currentDevice;
    std::function<void(const BluetoothDeviceInfo&)> onDeviceChange;
};
