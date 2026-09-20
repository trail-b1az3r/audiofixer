#include "BluetoothDeviceMonitor.hpp"
#include "main.hpp"
#include <algorithm>
#include <chrono>
#include <jni.h>

namespace {
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

std::string jstringToString(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* utf = env->GetStringUTFChars(jstr, nullptr);
    if (!utf) return "";
    std::string result(utf);
    env->ReleaseStringUTFChars(jstr, utf);
    return result;
}
}

BluetoothDeviceMonitor& BluetoothDeviceMonitor::get() {
    static BluetoothDeviceMonitor instance;
    return instance;
}

BluetoothDeviceMonitor::BluetoothDeviceMonitor() = default;

BluetoothDeviceMonitor::~BluetoothDeviceMonitor() {
    stop();
}

void BluetoothDeviceMonitor::start() {
    if (running.exchange(true)) return;
    workerThread = std::thread(&BluetoothDeviceMonitor::monitorLoop, this);
    LOG_INFO("BluetoothDeviceMonitor thread started");
}

void BluetoothDeviceMonitor::stop() {
    if (!running.exchange(false)) return;
    if (workerThread.joinable()) {
        workerThread.join();
    }
    LOG_INFO("BluetoothDeviceMonitor thread stopped");
}

BluetoothDeviceInfo BluetoothDeviceMonitor::getCurrentDevice() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentDevice;
}

void BluetoothDeviceMonitor::setDeviceChangeCallback(std::function<void(const BluetoothDeviceInfo&)> callback) {
    std::lock_guard<std::mutex> lock(stateMutex);
    onDeviceChange = callback;
}

void BluetoothDeviceMonitor::pollNow() {
    BluetoothDeviceInfo newDev = queryAndroidAudioState();
    bool changed = false;
    std::function<void(const BluetoothDeviceInfo&)> cb;

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (newDev.connected != currentDevice.connected ||
            newDev.deviceId != currentDevice.deviceId ||
            newDev.deviceName != currentDevice.deviceName ||
            newDev.outputLatencyMs != currentDevice.outputLatencyMs) {
            changed = true;
            currentDevice = newDev;
            cb = onDeviceChange;
        }
    }

    if (changed && cb) {
        cb(newDev);
    }
}

void BluetoothDeviceMonitor::monitorLoop() {
    while (running.load()) {
        pollNow();
        // Poll every 3 seconds to avoid CPU drain while maintaining responsiveness
        for (int i = 0; i < 30 && running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

BluetoothDeviceInfo BluetoothDeviceMonitor::queryAndroidAudioState() {
    BluetoothDeviceInfo info;
    info.connected = false;
    info.deviceName = "Headset Speakers";
    info.audioProfile = "Built-in Speaker";
    info.codec = "PCM";
    info.outputLatencyMs = -1;

    if (!modloader_jvm) {
        return info;
    }

    JNIEnv* env = nullptr;
    jint getEnvRes = modloader_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    bool attached = false;
    if (getEnvRes == JNI_EDETACHED) {
        if (modloader_jvm->AttachCurrentThread(&env, nullptr) == 0) {
            attached = true;
        } else {
            return info;
        }
    } else if (getEnvRes != JNI_OK) {
        return info;
    }

    auto cleanup = [&]() {
        if (attached && modloader_jvm) {
            modloader_jvm->DetachCurrentThread();
        }
    };

    jclass unityPlayerClass = env->FindClass("com/unity3d/player/UnityPlayer");
    if (!unityPlayerClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        cleanup();
        return info;
    }

    jfieldID actField = env->GetStaticFieldID(unityPlayerClass, "currentActivity", "Landroid/app/Activity;");
    if (!actField || env->ExceptionCheck()) {
        env->ExceptionClear();
        cleanup();
        return info;
    }

    jobject activity = env->GetStaticObjectField(unityPlayerClass, actField);
    if (!activity || env->ExceptionCheck()) {
        env->ExceptionClear();
        cleanup();
        return info;
    }

    jclass contextClass = env->FindClass("android/content/Context");
    jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring audioConst = env->NewStringUTF("audio");
    jobject audioManager = env->CallObjectMethod(activity, getSystemService, audioConst);
    env->DeleteLocalRef(audioConst);

    if (!audioManager || env->ExceptionCheck()) {
        env->ExceptionClear();
        cleanup();
        return info;
    }

    jclass audioManagerClass = env->GetObjectClass(audioManager);

    // 1. Try to query AudioManager.getOutputLatency(int streamType) via reflection
    // STREAM_MUSIC = 3
    jmethodID getOutputLatencyMethod = env->GetMethodID(audioManagerClass, "getOutputLatency", "(I)I");
    if (getOutputLatencyMethod && !env->ExceptionCheck()) {
        jint latency = env->CallIntMethod(audioManager, getOutputLatencyMethod, 3);
        if (!env->ExceptionCheck() && latency > 0) {
            info.outputLatencyMs = latency;
        } else {
            env->ExceptionClear();
        }
    } else {
        env->ExceptionClear();
    }

    // 2. Query audio devices: AudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
    // GET_DEVICES_OUTPUTS = 2
    jmethodID getDevicesMethod = env->GetMethodID(audioManagerClass, "getDevices", "(I)[Landroid/media/AudioDeviceInfo;");
    if (getDevicesMethod && !env->ExceptionCheck()) {
        jobjectArray deviceArray = (jobjectArray)env->CallObjectMethod(audioManager, getDevicesMethod, 2);
        if (deviceArray && !env->ExceptionCheck()) {
            jsize len = env->GetArrayLength(deviceArray);
            jclass audioDeviceInfoClass = env->FindClass("android/media/AudioDeviceInfo");
            jmethodID getTypeMethod = audioDeviceInfoClass ? env->GetMethodID(audioDeviceInfoClass, "getType", "()I") : nullptr;
            jmethodID getProductNameMethod = audioDeviceInfoClass ? env->GetMethodID(audioDeviceInfoClass, "getProductName", "()Ljava/lang/CharSequence;") : nullptr;
            jmethodID getIdMethod = audioDeviceInfoClass ? env->GetMethodID(audioDeviceInfoClass, "getId", "()I") : nullptr;

            for (jsize i = 0; i < len; ++i) {
                jobject dev = env->GetObjectArrayElement(deviceArray, i);
                if (!dev) continue;

                jint type = getTypeMethod ? env->CallIntMethod(dev, getTypeMethod) : 0;
                // TYPE_BLUETOOTH_A2DP = 8, TYPE_BLUETOOTH_SCO = 7
                if (type == 8 || type == 7) {
                    info.connected = true;
                    info.audioProfile = (type == 8) ? "A2DP (High Quality Stereo)" : "SCO (Voice/Handsfree)";
                    info.codec = (type == 8) ? "AAC / SBC" : "mSBC / CVSD";

                    if (getIdMethod) {
                        jint id = env->CallIntMethod(dev, getIdMethod);
                        info.deviceId = "BT_" + std::to_string(id);
                    }

                    if (getProductNameMethod) {
                        jobject pName = env->CallObjectMethod(dev, getProductNameMethod);
                        if (pName && !env->ExceptionCheck()) {
                            jclass charSeqClass = env->GetObjectClass(pName);
                            jmethodID toStringMethod = env->GetMethodID(charSeqClass, "toString", "()Ljava/lang/String;");
                            if (toStringMethod) {
                                jstring s = (jstring)env->CallObjectMethod(pName, toStringMethod);
                                std::string nameStr = jstringToString(env, s);
                                if (!nameStr.empty()) {
                                    info.deviceName = nameStr;
                                    info.deviceId = nameStr; // Use product name as persistent key
                                }
                                if (s) env->DeleteLocalRef(s);
                            }
                            env->DeleteLocalRef(pName);
                        } else {
                            env->ExceptionClear();
                        }
                    }
                    env->DeleteLocalRef(dev);
                    break;
                }
                env->DeleteLocalRef(dev);
            }
            env->DeleteLocalRef(deviceArray);
        } else {
            env->ExceptionClear();
        }
    } else {
        env->ExceptionClear();
    }

    // 3. Model Identification and AirPods Detection
    if (info.connected) {
        std::string lowerName = toLower(info.deviceName);
        if (lowerName.find("airpods") != std::string::npos) {
            info.isAirPods = true;
            if (lowerName.find("pro 3") != std::string::npos || lowerName.find("pro (3rd") != std::string::npos) {
                info.isAirPodsPro3 = true;
                info.detectedModel = "AirPods Pro 3";
            } else if (lowerName.find("pro 2") != std::string::npos || lowerName.find("pro (2nd") != std::string::npos) {
                info.detectedModel = "AirPods Pro (2nd gen)";
            } else if (lowerName.find("pro") != std::string::npos) {
                info.detectedModel = "AirPods Pro";
            } else if (lowerName.find("max") != std::string::npos) {
                info.detectedModel = "AirPods Max";
            } else {
                info.detectedModel = "AirPods (Unknown Model)";
            }
        } else {
            info.isAirPods = false;
            info.detectedModel = "Standard Bluetooth Audio";
        }
    } else {
        info.deviceId = "built_in_speaker";
        info.deviceName = "Headset Built-in Audio";
        info.detectedModel = "Meta Quest DAC / Speaker";
        info.audioProfile = "Internal Speaker";
        info.codec = "Direct PCM";
    }

    cleanup();
    return info;
}
