#include "UI/DebugOverlay.hpp"
#include "ModConfig.hpp"
#include "AudioOffsetManager.hpp"
#include "AdaptiveController.hpp"
#include "main.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "bsml/shared/BSML-Lite/Creation/Text.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include <sstream>

namespace {
SafePtrUnity<UnityEngine::GameObject> gOverlayObject;
SafePtrUnity<HMUI::CurvedTextMeshPro> gOverlayText;
float gTimeSinceUpdate = 0.0f;

std::string getOverlayContent() {
    float baseMs = AudioOffsetManager::get().getBaseOffsetMs();
    float adaptMs = AudioOffsetManager::get().getAdaptiveOffsetMs();
    float totalMs = AudioOffsetManager::get().getEffectiveOffsetMs();

    std::ostringstream ss;
    ss << "Audio Offset: " << (totalMs >= 0 ? "+" : "") << static_cast<int>(totalMs) << " ms\n"
       << "Base: " << (baseMs >= 0 ? "+" : "") << static_cast<int>(baseMs) << " ms\n"
       << "Adaptive: " << (adaptMs >= 0 ? "+" : "") << static_cast<int>(adaptMs) << " ms\n"
       << "Device: " << AudioOffsetManager::get().getActiveDeviceName() << "\n"
       << "Source: " << AudioOffsetManager::get().getActiveSourceName() << "\n"
       << "Confidence: " << AudioOffsetManager::get().getActiveConfidence() << "\n"
       << "Adaptive: " << (ModConfig::get().adaptiveCorrectionEnabled ? "ON" : "OFF");
    return ss.str();
}
}

namespace DebugOverlay {

void show() {
    if (!ModConfig::get().showDebugOverlay) return;

    if (!gOverlayObject) {
        auto go = UnityEngine::GameObject::New_ctor(StringW("AdaptiveAudioLatency_DebugOverlay"));
        go->get_transform()->set_position(UnityEngine::Vector3(0.0f, 2.5f, 3.2f));
        go->get_transform()->set_rotation(UnityEngine::Quaternion::Euler(15.0f, 0.0f, 0.0f));

        gOverlayText = BSML::Lite::CreateText(go->get_transform(), StringW(getOverlayContent()));
        if (gOverlayText) {
            gOverlayText->set_fontSize(3.2f);
            gOverlayText->set_alignment(TMPro::TextAlignmentOptions::TopLeft);
        }
        gOverlayObject = go;
        LOG_INFO("Spawned DebugOverlay in level");
    }
}

void hide() {
    if (gOverlayObject) {
        UnityEngine::GameObject::Destroy(gOverlayObject.ptr());
        gOverlayObject = nullptr;
        gOverlayText = nullptr;
        LOG_INFO("Destroyed DebugOverlay");
    }
}

void update() {
    if (!ModConfig::get().showDebugOverlay) {
        if (gOverlayObject) hide();
        return;
    }

    if (!gOverlayObject) {
        show();
    }

    if (gOverlayText) {
        gOverlayText->set_text(StringW(getOverlayContent()));
    }
}

}
