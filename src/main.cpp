#include "main.hpp"
#include "ModConfig.hpp"
#include "BluetoothDeviceMonitor.hpp"
#include "AudioOffsetManager.hpp"
#include "AudioTimingBridge.hpp"
#include "UI/SettingsViewController.hpp"
#include "custom-types/shared/register.hpp"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

extern "C" [[gnu::visibility("default")]] void setup(CModInfo *info) noexcept {
    *info = modInfo.to_c();
    LOG_INFO("Adaptive Audio Latency setup starting for Beat Saber 1.40.8_7379");

    ModConfig::get().load();
    BluetoothDeviceMonitor::get().start();
    AudioOffsetManager::get().init();

    LOG_INFO("Adaptive Audio Latency setup finished successfully");
}

extern "C" [[gnu::visibility("default")]] void late_load() noexcept {
    LOG_INFO("Adaptive Audio Latency late_load starting");
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();

    AudioTimingBridge::installHooks();
    SettingsViewController::registerMenu();

    LOG_INFO("Adaptive Audio Latency initialization complete for Beat Saber 1.40.8_7379!");
}
