#include "AudioTimingBridge.hpp"
#include "main.hpp"
#include "ModConfig.hpp"
#include "AudioOffsetManager.hpp"
#include "AdaptiveController.hpp"
#include "UI/DebugOverlay.hpp"

#include "GlobalNamespace/AudioLatencyViewController.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "HMUI/RangeValuesTextSlider.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp"

// Hook 1: GlobalNamespace::AudioLatencyViewController.DidActivate
MAKE_HOOK_MATCH(AudioLatencyViewController_DidActivate,
    &GlobalNamespace::AudioLatencyViewController::DidActivate,
    void,
    GlobalNamespace::AudioLatencyViewController* self,
    bool firstActivation,
    bool addedToHierarchy,
    bool screenSystemEnabling) {

    AudioLatencyViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if (!ModConfig::get().modEnabled) return;

    if (self->____slider) {
        float minVal = ModConfig::get().minOffsetMs;
        float maxVal = ModConfig::get().maxOffsetMs;
        self->____slider->set_minValue(minVal);
        self->____slider->set_maxValue(maxVal);

        float currentOffset = AudioOffsetManager::get().getBaseOffsetMs();
        self->____slider->set_value(currentOffset);
        LOG_INFO("Extended AudioLatency slider configured: min={}ms, max={}ms, current={}ms", minVal, maxVal, currentOffset);
    }
}

// Hook 2: GlobalNamespace::AudioLatencyViewController.SliderValueDidChange
MAKE_HOOK_MATCH(AudioLatencyViewController_SliderValueDidChange,
    &GlobalNamespace::AudioLatencyViewController::SliderValueDidChange,
    void,
    GlobalNamespace::AudioLatencyViewController* self,
    HMUI::RangeValuesTextSlider* slider,
    float_t value) {

    AudioLatencyViewController_SliderValueDidChange(self, slider, value);

    if (ModConfig::get().modEnabled) {
        AudioOffsetManager::get().setBaseOffsetMs(value);
    }
}

// Hook 3: GlobalNamespace::AudioTimeSyncController.Start
MAKE_HOOK_MATCH(AudioTimeSyncController_Start,
    &GlobalNamespace::AudioTimeSyncController::Start,
    void,
    GlobalNamespace::AudioTimeSyncController* self) {

    AudioTimeSyncController_Start(self);

    if (!ModConfig::get().modEnabled) return;

    AudioOffsetManager::get().onLevelStart();

    // Apply effective offset in seconds
    float effectiveSec = AudioOffsetManager::get().getEffectiveOffsetSeconds();
    self->____audioLatency = effectiveSec;
    LOG_INFO("AudioTimeSyncController::Start - Applied effective audioLatency: {}s ({}ms)",
        effectiveSec, AudioOffsetManager::get().getEffectiveOffsetMs());

    DebugOverlay::show();
}

// Hook 4: GlobalNamespace::AudioTimeSyncController.StartSong
MAKE_HOOK_MATCH(AudioTimeSyncController_StartSong,
    &GlobalNamespace::AudioTimeSyncController::StartSong,
    void,
    GlobalNamespace::AudioTimeSyncController* self,
    float_t startTimeOffset) {

    AudioTimeSyncController_StartSong(self, startTimeOffset);

    if (ModConfig::get().modEnabled) {
        AdaptiveController::get().reset();
        self->____audioLatency = AudioOffsetManager::get().getEffectiveOffsetSeconds();
    }
}

// Hook 5: GlobalNamespace::AudioTimeSyncController.Update
MAKE_HOOK_MATCH(AudioTimeSyncController_Update,
    &GlobalNamespace::AudioTimeSyncController::Update,
    void,
    GlobalNamespace::AudioTimeSyncController* self) {

    AudioTimeSyncController_Update(self);

    if (!ModConfig::get().modEnabled) return;

    // Only monitor and adapt during active song playback (State 0 = Playing)
    if (self->____state.value__ == 0 && self->____audioStarted) {
        double dspNow = UnityEngine::AudioSettings::get_dspTime();
        double dspOffset = self->____dspTimeOffset;
        float songTime = self->____songTime;

        // Calculate discrepancy between DSP clock progression and song time
        double elapsedDsp = dspNow - dspOffset;
        float syncDeltaMs = static_cast<float>((elapsedDsp - static_cast<double>(songTime)) * 1000.0);

        AdaptiveController::get().addObservation(syncDeltaMs, dspNow);
        AdaptiveController::get().update(UnityEngine::Time::get_deltaTime());

        // Update effective audio latency dynamically if adaptive correction is active
        if (ModConfig::get().adaptiveCorrectionEnabled) {
            self->____audioLatency = AudioOffsetManager::get().getEffectiveOffsetSeconds();
        }

        DebugOverlay::update();
    }
}

// Hook 6: GlobalNamespace::AudioTimeSyncController.StopSong
MAKE_HOOK_MATCH(AudioTimeSyncController_StopSong,
    &GlobalNamespace::AudioTimeSyncController::StopSong,
    void,
    GlobalNamespace::AudioTimeSyncController* self) {

    AudioTimeSyncController_StopSong(self);

    if (ModConfig::get().modEnabled) {
        DebugOverlay::hide();
        AudioOffsetManager::get().onLevelEnd();
    }
}

namespace AudioTimingBridge {
    void installHooks() {
        LOG_INFO("Installing AudioTimingBridge hooks for Beat Saber 1.40.8_7379...");
        INSTALL_HOOK(PaperLogger, AudioLatencyViewController_DidActivate);
        INSTALL_HOOK(PaperLogger, AudioLatencyViewController_SliderValueDidChange);
        INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Start);
        INSTALL_HOOK(PaperLogger, AudioTimeSyncController_StartSong);
        INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Update);
        INSTALL_HOOK(PaperLogger, AudioTimeSyncController_StopSong);
        LOG_INFO("AudioTimingBridge hooks successfully installed!");
    }
}
