#pragma once

#include "HMUI/CurvedTextMeshPro.hpp"

namespace SettingsViewController {
    void registerMenu();
    void updateUI();

    // Returns the calibration status text element, or nullptr if the settings
    // menu hasn't been opened yet this session. Used by CalibrationTicker to
    // flash a visual cue on each metronome tick.
    HMUI::CurvedTextMeshPro* getCalibrationIndicator();
}
