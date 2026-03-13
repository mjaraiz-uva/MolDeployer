#include <iostream> // For std::cerr in Error, and potentially for Init/Shutdown messages if file fails
#include <chrono>   // For timestamps
#include <iomanip>  // For std::put_time
#include <sstream>  // For formatting timestamps
#include "Logger.h"
#include "../InterfaceGUI/InterfaceGUI.h" // Added for ImGui logging
#include "../Simulator/Simulator.h"     // Added for GetCurrentStepCalculated

namespace Logger {

static std::ofstream g_log_file;
static bool g_is_initialized = false;
static bool g_log_to_console = false; // Default to true

// Static booleans to control GUI output for each log type
static bool g_enable_info_gui_output = true;
static bool g_enable_debug_gui_output = true;
static bool g_enable_warning_gui_output = true;
static bool g_enable_error_gui_output = true;

// Helper function to get current timestamp as string
static std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    // std::put_time is C++11, but might require #include <iomanip>
    // For C++20, std::format would be an option, but sticking to more common features.
#ifdef _MSC_VER // Visual Studio specific
    struct tm timeinfo;
    localtime_s(&timeinfo, &in_time_t);
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
#else // GCC/Clang
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
#endif
    return ss.str();
}

bool Init(const std::string& log_filename) {
    if (g_is_initialized) {
        // Optionally log a warning if already initialized
        return true;
    }
    g_log_file.open(log_filename, std::ios::out);//    std::ios::out | std::ios::app);
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
    int current_step = Simulator::GetCurrentStepCalculated();;
    std::string full_message = "[" + GetTimestamp() + "] [" + std::to_string(current_step) + "] [INFO] " + message;
    if (g_is_initialized && g_enable_info_gui_output) { // Check if InterfaceGUI might be ready and GUI output is enabled
        InterfaceGUI::AddInfoLog(full_message);
    }
    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    } else if (!g_is_initialized) {
        if (g_log_to_console) {
            std::cout << "[INFO] " << message << std::endl; // Fallback if logger not ready (InterfaceGUI also won't be called)
        }
    }
}

void Warning(const std::string& message) {
    int current_step = Simulator::GetCurrentStepCalculated();;
    std::string full_message = "[" + GetTimestamp() + "] [" + std::to_string(current_step) + "] [WARNING] " + message;
    if (g_is_initialized && g_enable_warning_gui_output) { // Check if InterfaceGUI might be ready and GUI output is enabled
        InterfaceGUI::AddWarningLog(full_message);
    }
    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    } else if (!g_is_initialized) {
        if (g_log_to_console) {
            std::cout << "[WARNING] " << message << std::endl; // Fallback
        }
    }
}

void Error(const std::string& message) {
    int current_step = Simulator::GetCurrentStepCalculated();
    std::string full_message = "[" + GetTimestamp() + "] [" + std::to_string(current_step) + "] [ERROR] " + message;
    // We always want errors in cerr and the GUI log if possible
    if (g_log_to_console) {
        std::cerr << "ERROR: " << message << std::endl; 
    }
    if (g_enable_error_gui_output) {
        InterfaceGUI::AddErrorLog(full_message); 
    }

    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    } else if (!g_is_initialized) {
        // Message already printed to cerr if g_log_to_console was true
    }
}

void Debug(const std::string& message) {
    int current_step = Simulator::GetCurrentStepCalculated();
    std::string full_message = "[" + GetTimestamp() + "] [" + std::to_string(current_step) + "] [DEBUG] " + message;
    if (g_is_initialized && g_enable_debug_gui_output) { // Check if InterfaceGUI might be ready and GUI output is enabled
        InterfaceGUI::AddDebugLog(full_message);
    }
    if (g_is_initialized && g_log_file.is_open()) {
        g_log_file << full_message << std::endl;
    } else if (!g_is_initialized) {
        if (g_log_to_console) {
            std::cout << "[DEBUG] " << message << std::endl; // Fallback
        }
    }
}

void SetConsoleLogging(bool enabled) {
    g_log_to_console = enabled;
}

// Implementations for new GUI output control functions
void SetEnableInfoGUIOutput(bool enable) {
    g_enable_info_gui_output = enable;
}
bool IsInfoGUIOutputEnabled() {
    return g_enable_info_gui_output;
}
void SetEnableDebugGUIOutput(bool enable) {
    g_enable_debug_gui_output = enable;
}
bool IsDebugGUIOutputEnabled() {
    return g_enable_debug_gui_output;
}
void SetEnableWarningGUIOutput(bool enable) {
    g_enable_warning_gui_output = enable;
}
bool IsWarningGUIOutputEnabled() {
    return g_enable_warning_gui_output;
}
void SetEnableErrorGUIOutput(bool enable) {
    g_enable_error_gui_output = enable;
}
bool IsErrorGUIOutputEnabled() {
    return g_enable_error_gui_output;
}

} // namespace Logger