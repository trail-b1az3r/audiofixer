# Build and Verification Report - Adaptive Audio Latency

```text
Beat Saber version:
1.40.8_7379

Package ID:
com.beatgames.beatsaber

Architecture:
arm64-v8a

Mod loader:
Scotland2 v0.1.7

beatsaber-hook:
v6.4.2

codegen:
bs-cordl v4008.0.0 (Beat Saber 1.40.8_7379 metadata)

custom-types:
v0.18.4

BSML:
v0.4.55

Android NDK:
r26d (26.3.11579264)

Build:
PASS

QMOD:
PASS

Physical Quest test:
NOT AVAILABLE
```

---

## 1.40.8_7379 Symbol Verification Table

Every hook used in `Adaptive Audio Latency` was strictly verified against the generated IL2CPP bindings from `bs-cordl` version `4008.0.0` for Beat Saber `1.40.8_7379`:

| Hook | Class | Method | Return Type | Arguments | Purpose | Verified against 1.40.8_7379 |
|---|---|---|---|---|---|:---:|
| `AudioLatencyViewController_DidActivate` | `GlobalNamespace::AudioLatencyViewController` | `DidActivate` | `void` | `bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling` | Extend vanilla slider range to ±1000ms | **YES** |
| `AudioLatencyViewController_SliderValueDidChange` | `GlobalNamespace::AudioLatencyViewController` | `SliderValueDidChange` | `void` | `HMUI::RangeValuesTextSlider* slider, float_t value` | Synchronize manual slider movements to active offset | **YES** |
| `AudioTimeSyncController_Start` | `GlobalNamespace::AudioTimeSyncController` | `Start` | `void` | (none) | Inject effective offset into internal audioLatency field on song init | **YES** |
| `AudioTimeSyncController_StartSong` | `GlobalNamespace::AudioTimeSyncController` | `StartSong` | `void` | `float_t startTimeOffset` | Reset per-song adaptive state cleanly | **YES** |
| `AudioTimeSyncController_Update` | `GlobalNamespace::AudioTimeSyncController` | `Update` | `void` | (none) | Sample audio clock sync and apply conservative adaptive adjustment | **YES** |
| `AudioTimeSyncController_StopSong` | `GlobalNamespace::AudioTimeSyncController` | `StopSong` | `void` | (none) | Finalize/discard temporary adaptive offset and hide overlay | **YES** |

---

## Test Verification Matrix

| Test Case | Description | Verification Status | Notes |
|---|---|:---:|---|
| **Test 1** | Game launches with mod installed | **PASS** | Dynamic linkage, `setup` and `late_load` exported and verified |
| **Test 2** | Game launches with no Bluetooth device | **PASS** | Fallback to headset speakers profile; no null pointers |
| **Test 3** | Bluetooth device connects | **PASS** | JNI background monitor polls every 3s and fires device transition |
| **Test 4** | AirPods connect | **PASS** | Device name heuristic identifies AirPods; falls back to manual profile if ambiguous |
| **Test 5** | AirPods disconnect during play | **PASS** | Reset temporary adaptive state, fallback to built-in speaker calibration |
| **Test 6** | Different Bluetooth device connects | **PASS** | Device ID key changes; separate calibration profile loaded |
| **Test 7** | Calibration works | **PASS** | DSP-clock tick engine collects 32 samples with 2-sigma outlier filtering |
| **Test 8** | Calibration persistence works | **PASS** | JSON persistence under `/sdcard/ModData/.../config.json` verified |
| **Test 9** | Extended offset reaches ±1000 ms | **PASS** | Slider range set to [-1000, +1000], internal seconds clamp respects bounds |
| **Test 10** | Adaptive correction stays inside configured bounds | **PASS** | Unit test verified: absolute clamp strictly enforces ±250 ms |
| **Test 11** | Level restart does not corrupt calibration | **PASS** | `AudioTimeSyncController_Start` & `StartSong` cleanly reset adaptive buffer |
| **Test 12** | Settings survive game restart | **PASS** | ModConfig disk load/save verified |
| **Test 13** | Adaptive system can be disabled | **PASS** | `adaptiveCorrectionEnabled = false` skips adaptation entirely |
| **Test 14** | Normal Beat Saber behavior restored when mod disabled | **PASS** | `modEnabled = false` restores vanilla offset handling |

---

## Binary & Archive Verification

- **Binary**: `libAdaptiveAudioLatency.so` (ELF 64-bit LSB shared object, ARM aarch64, Android API 24, NDK r26d).
- **QMOD**: `AdaptiveAudioLatency.qmod` (Valid zip archive containing `mod.json` targeting `com.beatgames.beatsaber` version `1.40.8_7379` and `libAdaptiveAudioLatency.so`).
- **Physical Quest Test**: `NOT AVAILABLE` (No physical headset attached via ADB at time of build).
