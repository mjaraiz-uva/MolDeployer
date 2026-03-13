#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "Logger.h"
#include "../InterfaceGUI/InterfaceGUI.h"
#include "../Simulator/Simulator.h"

namespace Logger {

static std::ofstream g_log_file;
static bool g_is_initialized = false;
static bool g_log_to_console = false;

static bool g_enable_info_gui_output = true;
static bool g_enable_debug_gui_output = true;
static bool g_enable_warning_gui_output = true;
static bool g_enable_error_gui_output = true;

// Time only: "13:30:08"
static std::string GetTimeStamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
#ifdef _MSC_VER
    struct tm timeinfo;
    localtime_s(&timeinfo, &in_time_t);
    ss << std::put_time(&timeinfo, "%H:%M:%S");
#else
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
#endif
    return ss.str();
}

static std::string FormatPrefix(const char* level) {
    int current_step = Simulator::GetCurrentStepCalculated();
    if (current_step < 0) {
        return "[" + GetTimeStamp() + "] [" + level + "] ";
    } else {
        return "[" + GetTimeStamp() + "] [step:" + std::to_string(current_step) + "] [" + level + "] ";
    }
}

bool Init(const std::string& log_filename) {
    if (g_is_initialized) {
        return true;
    }
    g_log_file.open(log_filename, std::ios::out);
    if (!g_log_file.is_open()) {
        if (g_log_to_console) {
            std::cerr << "ERROR: Failed to open log file: " << log_filename << std::endl;
        }
        return false;
    }
    g_is_initialized = true;
    Info("Logger initialized. Logging to file: " + log_filename);
    return true;
}

void Shutdown() {
    if (g_is_initialized) {
        Info("Logger shutting down.");
        g_log_file.close();
        g_is_initialized = false;
    }
}

void Info(const std::string& message) {
    std::string full_message = FormatPrefix("INFO") + message;
    if (g_is_initialized && g_enable_info_gui_output) {
        InterfaceGUI::AddInfoLog(full_message);
    }
    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    } else if (!g_is_initialized) {
        if (g_log_to_console) {
            std::cout << "[INFO] " << message << std::endl;
        }
    }
}

void Warning(const std::string& message) {
    std::string full_message = FormatPrefix("WARNING") + message;
    if (g_is_initialized && g_enable_warning_gui_output) {
        InterfaceGUI::AddWarningLog(full_message);
    }
    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    } else if (!g_is_initialized) {
        if (g_log_to_console) {
            std::cout << "[WARNING] " << message << std::endl;
        }
    }
}

void Error(const std::string& message) {
    std::string full_message = FormatPrefix("ERROR") + message;
    if (g_log_to_console) {
        std::cerr << "ERROR: " << message << std::endl;
    }
    if (g_enable_error_gui_output) {
        InterfaceGUI::AddErrorLog(full_message);
    }
    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    }
}

void Debug(const std::string& message) {
    std::string full_message = FormatPrefix("DEBUG") + message;
    if (g_is_initialized && g_enable_debug_gui_output) {
        InterfaceGUI::AddDebugLog(full_message);
    }
    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    } else if (!g_is_initialized) {
        if (g_log_to_console) {
            std::cout << "[DEBUG] " << message << std::endl;
        }
    }
}

void SetConsoleLogging(bool enabled) {
    g_log_to_console = enabled;
}

void SetEnableInfoGUIOutput(bool enable) { g_enable_info_gui_output = enable; }
bool IsInfoGUIOutputEnabled() { return g_enable_info_gui_output; }
void SetEnableDebugGUIOutput(bool enable) { g_enable_debug_gui_output = enable; }
bool IsDebugGUIOutputEnabled() { return g_enable_debug_gui_output; }
void SetEnableWarningGUIOutput(bool enable) { g_enable_warning_gui_output = enable; }
bool IsWarningGUIOutputEnabled() { return g_enable_warning_gui_output; }
void SetEnableErrorGUIOutput(bool enable) { g_enable_error_gui_output = enable; }
bool IsErrorGUIOutputEnabled() { return g_enable_error_gui_output; }

} // namespace Logger
