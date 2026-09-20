#include "UI/CalibrationTicker.hpp"
#include "LatencyCalibration.hpp"
#include "UI/SettingsViewController.hpp"
#include "main.hpp"

#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Color.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include <cmath>

DEFINE_TYPE(AdaptiveAudioLatency, CalibrationTicker);

using namespace UnityEngine;

namespace {
constexpr int kSampleRate = 44100;
constexpr float kDurationSec = 0.08f;
constexpr float kBlipHz = 1000.0f;
constexpr float kFlashSeconds = 0.15f;
}

void AdaptiveAudioLatency::CalibrationTicker::Awake() {
    audioSource = get_gameObject()->AddComponent<AudioSource*>();
    audioSource->set_playOnAwake(false);
    audioSource->set_spatialBlend(0.0f);
    audioSource->set_volume(0.6f);

    int sampleCount = static_cast<int>(kSampleRate * kDurationSec);
    ArrayW<float> data(sampleCount);
    for (int i = 0; i < sampleCount; i++) {
        float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        // Linear fade-out envelope so the blip doesn't click at the tail.
        float envelope = 1.0f - (static_cast<float>(i) / static_cast<float>(sampleCount));
        data[i] = std::sin(2.0f * static_cast<float>(M_PI) * kBlipHz * t) * 0.5f * envelope;
    }

    auto clip = AudioClip::Create(StringW("CalibrationBlip"), sampleCount, 1, kSampleRate, false);
    clip->SetData(data, 0);
    audioSource->set_clip(clip);

    flashTimer = 0.0f;
    LOG_INFO("CalibrationTicker ready");
}

void AdaptiveAudioLatency::CalibrationTicker::Update() {
    if (!LatencyCalibration::get().isRunning()) {
        return;
    }

    double dspNow = AudioSettings::get_dspTime();
    if (LatencyCalibration::get().tick(dspNow)) {
        if (audioSource) {
            audioSource->Play();
        }
        flashTimer = kFlashSeconds;
        if (auto* indicator = SettingsViewController::getCalibrationIndicator()) {
            indicator->set_color(Color(1.0f, 0.85f, 0.2f, 1.0f));
        }
    }

    if (flashTimer > 0.0f) {
        flashTimer -= Time::get_deltaTime();
        if (flashTimer <= 0.0f) {
            flashTimer = 0.0f;
            if (auto* indicator = SettingsViewController::getCalibrationIndicator()) {
                indicator->set_color(Color(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
    }
}
