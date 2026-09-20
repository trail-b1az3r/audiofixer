# Adaptive Audio Latency - Beat Saber Quest Mod

A Meta Quest / Android ARM64 native Beat Saber mod built strictly and exclusively for **Beat Saber version `1.40.8_7379`**.

Adaptive Audio Latency provides an expanded audio latency offset range (±1000ms), Quest-side Bluetooth audio device detection with specialized AirPods Pro 3 identification and profiling, interactive visual/audio calibration with statistical outlier rejection, and a conservative, real-time adaptive latency controller.

---

## Technical Specifications

- **Beat Saber Version**: `1.40.8`
- **Package Version**: `1.40.8_7379`
- **Package ID**: `com.beatgames.beatsaber`
- **Target Architecture**: `arm64-v8a`
- **Target Hardware**: Meta Quest 2, Meta Quest Pro, Meta Quest 3, Meta Quest 3S
- **Operating System**: Android (API 24+ / Quest OS)
- **Mod Loader**: Scotland2 (`v0.1.7`)
- **Mod ID**: `AdaptiveAudioLatency`
- **Mod Version**: `1.0.0`

---

## Modding Stack & Dependencies

All dependencies are resolved against the verified Beat Saber `1.40.8_7379` ecosystem:

| Component | Version | Role |
|---|---|---|
| **Scotland2** | `0.1.7` | Mod loader and dynamic library supervisor |
| **beatsaber-hook** | `6.4.2` | Quest hooking runtime and inline hook engine |
| **bs-cordl** | `4008.0.0` | Exact type-safe IL2CPP codegen bindings for Beat Saber 1.40.8_7379 |
| **custom-types** | `0.18.4` | C++ IL2CPP custom class and method registration |
| **BSML** | `0.4.55` | BeatSaberMarkupLanguage Quest UI system |
| **paper2_scotland2** | `4.7.0` | PaperLogger structured logging sink |
| **Android NDK** | `r26d (26.3.11579264)` | Unified Clang 17.0.2 cross-compiler |

---

## Core Features

### 1. Extended Audio Offset Range (±1000 ms)
- Modifies Beat Saber's native `AudioLatencyViewController` slider and internal `AudioTimeSyncController` latency storage.
- Extends the adjustment range from vanilla limits to `[-1000 ms, +1000 ms]`, fully configurable up to `±2000 ms`.
- Preserves Beat Saber's native sign convention (positive advances audio relative to gameplay notes).

### 2. Quest-Side Bluetooth Audio Device Detection
- Inspects Android's `AudioManager` and `AudioDeviceInfo` via JNI on a dedicated background worker thread (every 3 seconds).
- Displays real-time device connection state, device name, active audio profile (`A2DP` vs `SCO`), codec (`AAC`, `SBC`, `PCM`), and hardware output latency where available.
- **AirPods Pro 3 Support**:
  - Automatically inspects advertised device names and hardware product strings.
  - Recognizes "AirPods Pro 3", "AirPods Pro (2nd gen)", "AirPods Pro", and "AirPods Max".
  - If model cannot be conclusively confirmed from Android descriptors, reports `Device: AirPods | Model: Unknown` and provides a manual profile override selector.

### 3. Interactive Visual & Audio Calibration
- Controlled calibration engine with synchronized DSP audio clicks and visual metronome events.
- Evaluates 32 samples across repeated taps.
- Statistical analysis:
  - Two-sigma outlier rejection.
  - Median and trimmed mean computation.
  - Standard deviation calculation ($\sigma$).
  - Confidence scoring:
    - **High**: $\sigma < 10\text{ms}$
    - **Medium**: $10\text{ms} \le \sigma < 25\text{ms}$
    - **Low**: $\sigma \ge 25\text{ms}$
- **Per-Device Persistence**:
  - Automatically associates calibration results with the specific Bluetooth Device ID.
  - Seamlessly reloads the saved calibration when reconnecting the same AirPods.

### 4. Conservative Adaptive In-Level Correction
- Real-time adaptive controller (`AdaptiveController`) active during level playback.
- **Safety Guarantee**: Player swing errors and misses are NEVER misattributed to Bluetooth latency.
- Monitors audio DSP clock synchronization and delta drift (`AudioSettings.dspTime` vs `AudioTimeSyncController._songTime`).
- Conservative controller design:
  - **Minimum Sample Threshold**: Minimum 20 observations required before initiating adjustments.
  - **Deadband**: Suppresses adjustments for timing jitter $< 3\text{ms}$.
  - **Hysteresis**: Prevents oscillations between consecutive intervals.
  - **Velocity Limiting**: Clamps adjustment rate to $1\text{--}5\text{ms}$ per evaluation window.
  - **Hard Clamping**: Confines cumulative automatic correction strictly within $\pm 250\text{ms}$.
  - Resets cleanly upon level finish or level restart.

### 5. Diagnostics & Debug Overlay
- Unobtrusive floating HUD in the player's upper periphery during songs showing:
  - Effective audio offset (Base + Adaptive)
  - Active device
  - Active timing source (`Calibration`, `Measured`, `Estimated`, `Default`)
  - Confidence rating
  - Adaptive state (`ON` / `OFF`)

---

## Installation

### Via QuestPatcher (Recommended)
1. Connect your Meta Quest headset via USB with Developer Mode enabled.
2. Launch **QuestPatcher**.
3. Select Beat Saber `1.40.8_7379`.
4. Drag and drop `AdaptiveAudioLatency.qmod` into the Mods tab.

### Via ADB Manual Copy
```bash
adb push AdaptiveAudioLatency.qmod /sdcard/ModData/com.beatgames.beatsaber/Mods/
# Or copy the shared library directly:
adb push libAdaptiveAudioLatency.so /sdcard/ModData/com.beatgames.beatsaber/Mods/
```

---

## Configuration & Storage

Configuration and per-device calibration profiles are persisted in:
```text
/sdcard/ModData/com.beatgames.beatsaber/Mods/AdaptiveAudioLatency/config.json
```

---

## Known Limitations

1. **Android Bluetooth Hardware Reporting**: Android audio HAL does not expose microsecond packet transmission timestamps to user-space apps. The mod queries `AudioManager.getOutputLatency(STREAM_MUSIC)` via reflection; when restricted by Quest OS firmware, the mod falls back seamlessly to the interactive calibration engine or verified device profile baselines.
2. **Bluetooth Reconnection Latency Shifts**: Certain Bluetooth chips renegotiate buffer sizes upon reconnection. If latency shifts noticeably, tap **Recalibrate Current Device** in the settings menu.
