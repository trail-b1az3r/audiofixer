#pragma once

// Nothing in the rest of the codebase ever calls LatencyCalibration::tick(), so the
// metronome it's supposed to schedule never fires and taps are always ignored
// (recordUserTap bails out while lastTickDspTime == 0.0). This MonoBehaviour is the
// missing driver: while calibration is running it calls tick() every frame, and on
// each tick it plays a short blip and flashes the calibration status text so the
// user has something to actually tap along to.
//
// NOTE: custom-types macro syntax is the one part of this change I couldn't verify
// against the real headers in this environment (extern/ wasn't populated here) - if
// this file doesn't compile, paste the error and I'll fix the macro usage directly.

#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/types.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/AudioSource.hpp"

DECLARE_CLASS_CODEGEN(AdaptiveAudioLatency, CalibrationTicker, UnityEngine::MonoBehaviour,
    DECLARE_INSTANCE_FIELD(UnityEngine::AudioSource*, audioSource);
    DECLARE_INSTANCE_FIELD(float, flashTimer);

    DECLARE_METHOD(void, Awake);
    DECLARE_METHOD(void, Update);
)
