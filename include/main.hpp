#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <iostream>

#include "scotland2/shared/modloader.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "paper2_scotland2/shared/logger.hpp"

#ifndef MOD_ID
#define MOD_ID "AdaptiveAudioLatency"
#endif

#ifndef VERSION
#define VERSION "1.0.0"
#endif

constexpr auto PaperLogger = Paper::ConstLoggerContext("AdaptiveAudioLatency");

#define LOG_INFO(...) PaperLogger.info(__VA_ARGS__)
#define LOG_ERROR(...) PaperLogger.error(__VA_ARGS__)
#define LOG_WARN(...) PaperLogger.warn(__VA_ARGS__)
#define LOG_DEBUG(...) PaperLogger.debug(__VA_ARGS__)
