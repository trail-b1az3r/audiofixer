#include "UI/SettingsViewController.hpp"
#include "ModConfig.hpp"
#include "BluetoothDeviceMonitor.hpp"
#include "AudioOffsetManager.hpp"
#include "LatencyCalibration.hpp"
#include "AdaptiveController.hpp"
#include "main.hpp"

#include "bsml/shared/BSML/Settings/BSMLSettings.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML-Lite/Creation/Buttons.hpp"
#include "bsml/shared/BSML-Lite/Creation/Text.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include <iomanip>
#include <sstream>

namespace {
SafePtrUnity<HMUI::CurvedTextMeshPro> gDeviceInfoText;
SafePtrUnity<HMUI::CurvedTextMeshPro> gCalibrationStatusText;
SafePtrUnity<BSML::IncrementSetting> gOffsetSetting;

std::string formatDeviceInfo() {
    auto dev = BluetoothDeviceMonitor::get().getCurrentDevice();
    std::ostringstream ss;
    ss << "Bluetooth: " << (dev.connected ? "Connected" : "Disconnected") << "\n"
       << "Device: " << dev.deviceName << "\n"
       << "Model: " << dev.detectedModel << "\n"
       << "Profile: " << dev.audioProfile << " | Codec: " << dev.codec << "\n"
       << "Hardware Latency: " << (dev.outputLatencyMs > 0 ? std::to_string(dev.outputLatencyMs) + " ms" : "Unavailable");
    return ss.str();
}

std::string formatCalibrationInfo() {
    auto stats = LatencyCalibration::get().getCurrentStats();
    if (!stats.isComplete && stats.sampleCount == 0) {
        auto dev = BluetoothDeviceMonitor::get().getCurrentDevice();
        stats = LatencyCalibration::get().resolveDeviceCalibration(dev);
    }

    std::ostringstream ss;
    ss << "Samples: " << stats.sampleCount << "/" << stats.targetSamples
       << " | Median: " << static_cast<int>(stats.medianMs) << " ms"
       << " | Std Dev: " << static_cast<int>(stats.stdDevMs) << " ms\n"
       << "Final Offset: " << (stats.finalOffsetMs >= 0 ? "+" : "") << static_cast<int>(stats.finalOffsetMs) << " ms"
       << " | Source: " << stats.sourceName << "\n"
       << "Confidence: " << stats.confidence;
    return ss.str();
}
}

namespace SettingsViewController {

void updateUI() {
    if (gDeviceInfoText) {
        gDeviceInfoText->set_text(StringW(formatDeviceInfo()));
    }
    if (gCalibrationStatusText) {
        gCalibrationStatusText->set_text(StringW(formatCalibrationInfo()));
    }
    if (gOffsetSetting) {
        gOffsetSetting->set_Value(AudioOffsetManager::get().getBaseOffsetMs());
    }
}

void registerMenu() {
    BSML::BSMLSettings::get_instance()->TryAddSettingsMenu(
        [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
            if (!firstActivation || viewController == nullptr) return;

            auto* container = BSML::Lite::CreateScrollableSettingsContainer(viewController->get_transform());
            if (container == nullptr) return;

            auto transform = container->get_transform();

            // 1. Mod Enable Toggle
            BSML::Lite::CreateToggle(
                transform, u"Enable Mod", ModConfig::get().modEnabled,
                [](bool val) {
                    ModConfig::get().modEnabled = val;
                    ModConfig::get().save();
                    AudioOffsetManager::get().refreshDeviceState();
                    updateUI();
                });

            // 2. Bluetooth Device Info Header
            BSML::Lite::CreateText(transform, u"<b>--- Bluetooth & Audio Device ---</b>");
            gDeviceInfoText = BSML::Lite::CreateText(transform, StringW(formatDeviceInfo()));

            // Refresh Device Button
            BSML::Lite::CreateUIButton(transform, u"Refresh Bluetooth Status", []() {
                BluetoothDeviceMonitor::get().pollNow();
                AudioOffsetManager::get().refreshDeviceState();
                updateUI();
            });

            // 3. Audio Offset Controls
            BSML::Lite::CreateText(transform, u"<b>--- Audio Latency Offset ---</b>");

            gOffsetSetting = BSML::Lite::CreateIncrementSetting(
                transform, u"Audio Offset (ms)", 0, 5.0f,
                AudioOffsetManager::get().getBaseOffsetMs(),
                true, true,
                ModConfig::get().minOffsetMs, ModConfig::get().maxOffsetMs,
                [](float val) {
                    AudioOffsetManager::get().setBaseOffsetMs(val);
                    updateUI();
                });

            BSML::Lite::CreateIncrementSetting(
                transform, u"Minimum Offset Limit (ms)", 0, 100.0f,
                ModConfig::get().minOffsetMs,
                true, true, -2000.0f, -100.0f,
                [](float val) {
                    ModConfig::get().minOffsetMs = val;
                    ModConfig::get().save();
                });

            BSML::Lite::CreateIncrementSetting(
                transform, u"Maximum Offset Limit (ms)", 0, 100.0f,
                ModConfig::get().maxOffsetMs,
                true, true, 100.0f, 2000.0f,
                [](float val) {
                    ModConfig::get().maxOffsetMs = val;
                    ModConfig::get().save();
                });

            // 4. Calibration Section
            BSML::Lite::CreateText(transform, u"<b>--- AirPods / Device Calibration ---</b>");
            gCalibrationStatusText = BSML::Lite::CreateText(transform, StringW(formatCalibrationInfo()));

            LatencyCalibration::get().setStatsUpdateCallback([](const CalibrationStats&) {
                updateUI();
            });

            BSML::Lite::CreateToggle(
                transform, u"Automatic Bluetooth Calibration", ModConfig::get().autoBluetoothCalibration,
                [](bool val) {
                    ModConfig::get().autoBluetoothCalibration = val;
                    ModConfig::get().save();
                });

            BSML::Lite::CreateUIButton(transform, u"Calibrate Now (Tap to Audio)", []() {
                if (!LatencyCalibration::get().isRunning()) {
                    LatencyCalibration::get().startCalibration(32);
                } else {
                    LatencyCalibration::get().recordUserTap(UnityEngine::AudioSettings::get_dspTime());
                }
                updateUI();
            });

            BSML::Lite::CreateUIButton(transform, u"Reset Device Calibration", []() {
                LatencyCalibration::get().resetCurrentDeviceCalibration();
                AudioOffsetManager::get().refreshDeviceState();
                updateUI();
            });

            // 5. Adaptive Correction
            BSML::Lite::CreateText(transform, u"<b>--- Adaptive In-Level Correction ---</b>");

            BSML::Lite::CreateToggle(
                transform, u"Adaptive In-Level Correction", ModConfig::get().adaptiveCorrectionEnabled,
                [](bool val) {
                    ModConfig::get().adaptiveCorrectionEnabled = val;
                    ModConfig::get().save();
                });

            BSML::Lite::CreateIncrementSetting(
                transform, u"Max Auto Correction (±ms)", 0, 25.0f,
                ModConfig::get().maxAdaptiveCorrectionMs,
                true, true, 25.0f, 500.0f,
                [](float val) {
                    ModConfig::get().maxAdaptiveCorrectionMs = val;
                    ModConfig::get().save();
                });

            BSML::Lite::CreateIncrementSetting(
                transform, u"Correction Speed (ms/step)", 0, 1.0f,
                ModConfig::get().correctionSpeed,
                true, true, 1.0f, 10.0f,
                [](float val) {
                    ModConfig::get().correctionSpeed = val;
                    ModConfig::get().save();
                });

            BSML::Lite::CreateToggle(
                transform, u"Retain Adaptive After Level", ModConfig::get().retainAdaptiveAfterLevel,
                [](bool val) {
                    ModConfig::get().retainAdaptiveAfterLevel = val;
                    ModConfig::get().save();
                });

            // 6. Debug Overlay
            BSML::Lite::CreateText(transform, u"<b>--- Diagnostics ---</b>");
            BSML::Lite::CreateToggle(
                transform, u"Debug Overlay", ModConfig::get().showDebugOverlay,
                [](bool val) {
                    ModConfig::get().showDebugOverlay = val;
                    ModConfig::get().save();
                });
        },
        "Adaptive Audio Latency", false);

    LOG_INFO("Registered Adaptive Audio Latency Settings menu in BSML");
}

}
