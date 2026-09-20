#include "ModConfig.hpp"
#include "main.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/stringbuffer.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/prettywriter.h"

ModConfig& ModConfig::get() {
    static ModConfig instance;
    return instance;
}

std::string ModConfig::getConfigPath() const {
    return "/sdcard/ModData/com.beatgames.beatsaber/Mods/AdaptiveAudioLatency/config.json";
}

bool ModConfig::hasDeviceCalibration(const std::string& deviceId) const {
    if (deviceId.empty()) return false;
    return deviceCalibrations.find(deviceId) != deviceCalibrations.end();
}

DeviceCalibration ModConfig::getDeviceCalibration(const std::string& deviceId) const {
    auto it = deviceCalibrations.find(deviceId);
    if (it != deviceCalibrations.end()) {
        return it->second;
    }
    return DeviceCalibration{};
}

void ModConfig::setDeviceCalibration(const std::string& deviceId, const DeviceCalibration& cal) {
    if (deviceId.empty()) return;
    deviceCalibrations[deviceId] = cal;
    save();
}

void ModConfig::removeDeviceCalibration(const std::string& deviceId) {
    auto it = deviceCalibrations.find(deviceId);
    if (it != deviceCalibrations.end()) {
        deviceCalibrations.erase(it);
        save();
    }
}

void ModConfig::load() {
    std::string path = getConfigPath();
    if (!std::filesystem::exists(path)) {
        LOG_INFO("Config file not found, creating defaults at {}", path);
        save();
        return;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for reading: {}", path);
        return;
    }

    std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        LOG_ERROR("Config file JSON parse error in {}", path);
        return;
    }

    if (doc.HasMember("modEnabled") && doc["modEnabled"].IsBool())
        modEnabled = doc["modEnabled"].GetBool();
    if (doc.HasMember("minOffsetMs") && doc["minOffsetMs"].IsNumber())
        minOffsetMs = doc["minOffsetMs"].GetFloat();
    if (doc.HasMember("maxOffsetMs") && doc["maxOffsetMs"].IsNumber())
        maxOffsetMs = doc["maxOffsetMs"].GetFloat();
    if (doc.HasMember("autoBluetoothCalibration") && doc["autoBluetoothCalibration"].IsBool())
        autoBluetoothCalibration = doc["autoBluetoothCalibration"].GetBool();
    if (doc.HasMember("adaptiveCorrectionEnabled") && doc["adaptiveCorrectionEnabled"].IsBool())
        adaptiveCorrectionEnabled = doc["adaptiveCorrectionEnabled"].GetBool();
    if (doc.HasMember("maxAdaptiveCorrectionMs") && doc["maxAdaptiveCorrectionMs"].IsNumber())
        maxAdaptiveCorrectionMs = doc["maxAdaptiveCorrectionMs"].GetFloat();
    if (doc.HasMember("correctionSpeed") && doc["correctionSpeed"].IsNumber())
        correctionSpeed = doc["correctionSpeed"].GetFloat();
    if (doc.HasMember("deadbandMs") && doc["deadbandMs"].IsNumber())
        deadbandMs = doc["deadbandMs"].GetFloat();
    if (doc.HasMember("observationWindowSec") && doc["observationWindowSec"].IsNumber())
        observationWindowSec = doc["observationWindowSec"].GetFloat();
    if (doc.HasMember("retainAdaptiveAfterLevel") && doc["retainAdaptiveAfterLevel"].IsBool())
        retainAdaptiveAfterLevel = doc["retainAdaptiveAfterLevel"].GetBool();
    if (doc.HasMember("showDebugOverlay") && doc["showDebugOverlay"].IsBool())
        showDebugOverlay = doc["showDebugOverlay"].GetBool();
    if (doc.HasMember("manualDeviceProfile") && doc["manualDeviceProfile"].IsString())
        manualDeviceProfile = doc["manualDeviceProfile"].GetString();

    if (doc.HasMember("deviceCalibrations") && doc["deviceCalibrations"].IsObject()) {
        deviceCalibrations.clear();
        const auto& cals = doc["deviceCalibrations"];
        for (auto it = cals.MemberBegin(); it != cals.MemberEnd(); ++it) {
            std::string id = it->name.GetString();
            if (it->value.IsObject()) {
                DeviceCalibration cal;
                if (it->value.HasMember("baseOffsetMs") && it->value["baseOffsetMs"].IsNumber())
                    cal.baseOffsetMs = it->value["baseOffsetMs"].GetFloat();
                if (it->value.HasMember("stdDev") && it->value["stdDev"].IsNumber())
                    cal.stdDev = it->value["stdDev"].GetFloat();
                if (it->value.HasMember("sampleCount") && it->value["sampleCount"].IsInt())
                    cal.sampleCount = it->value["sampleCount"].GetInt();
                if (it->value.HasMember("confidence") && it->value["confidence"].IsString())
                    cal.confidence = it->value["confidence"].GetString();
                if (it->value.HasMember("timestamp") && it->value["timestamp"].IsInt64())
                    cal.timestamp = it->value["timestamp"].GetInt64();
                if (it->value.HasMember("deviceName") && it->value["deviceName"].IsString())
                    cal.deviceName = it->value["deviceName"].GetString();
                deviceCalibrations[id] = cal;
            }
        }
    }

    LOG_INFO("Config successfully loaded. ModEnabled: {}, Calibrations count: {}", modEnabled, deviceCalibrations.size());
}

void ModConfig::save() {
    std::string path = getConfigPath();
    std::filesystem::path dir = std::filesystem::path(path).parent_path();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    doc.AddMember("modEnabled", modEnabled, allocator);
    doc.AddMember("minOffsetMs", minOffsetMs, allocator);
    doc.AddMember("maxOffsetMs", maxOffsetMs, allocator);
    doc.AddMember("autoBluetoothCalibration", autoBluetoothCalibration, allocator);
    doc.AddMember("adaptiveCorrectionEnabled", adaptiveCorrectionEnabled, allocator);
    doc.AddMember("maxAdaptiveCorrectionMs", maxAdaptiveCorrectionMs, allocator);
    doc.AddMember("correctionSpeed", correctionSpeed, allocator);
    doc.AddMember("deadbandMs", deadbandMs, allocator);
    doc.AddMember("observationWindowSec", observationWindowSec, allocator);
    doc.AddMember("retainAdaptiveAfterLevel", retainAdaptiveAfterLevel, allocator);
    doc.AddMember("showDebugOverlay", showDebugOverlay, allocator);
    doc.AddMember("manualDeviceProfile", rapidjson::Value(manualDeviceProfile.c_str(), allocator), allocator);

    rapidjson::Value calsObj(rapidjson::kObjectType);
    for (const auto& [id, cal] : deviceCalibrations) {
        rapidjson::Value calObj(rapidjson::kObjectType);
        calObj.AddMember("baseOffsetMs", cal.baseOffsetMs, allocator);
        calObj.AddMember("stdDev", cal.stdDev, allocator);
        calObj.AddMember("sampleCount", cal.sampleCount, allocator);
        calObj.AddMember("confidence", rapidjson::Value(cal.confidence.c_str(), allocator), allocator);
        calObj.AddMember("timestamp", cal.timestamp, allocator);
        calObj.AddMember("deviceName", rapidjson::Value(cal.deviceName.c_str(), allocator), allocator);

        calsObj.AddMember(rapidjson::Value(id.c_str(), allocator), calObj, allocator);
    }
    doc.AddMember("deviceCalibrations", calsObj, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream file(path);
    if (file.is_open()) {
        file << buffer.GetString();
        file.close();
        LOG_INFO("Saved config to {}", path);
    } else {
        LOG_ERROR("Failed to open config file for writing: {}", path);
    }
}
