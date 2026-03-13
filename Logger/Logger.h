#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream> // For std::ofstream

namespace Logger {

    // Initializes the Logger with a specific log file.
    // Returns true on success, false if the file cannot be opened.
    bool Init(const std::string& log_filename);

    // Shuts down the Logger, closing the log file.
    void Shutdown();

    // Logs an informational message.
    void Info(const std::string& message);

    // Logs a warning message.
    void Warning(const std::string& message);

    // Logs an error message. Also prints to std::cerr.
    void Error(const std::string& message);

    // Logs a debug message.
    void Debug(const std::string& message);

    // Sets whether logging to console is enabled.
    void SetConsoleLogging(bool enabled);

    // New functions to control GUI output for each log type
    void SetEnableInfoGUIOutput(bool enable);
    bool IsInfoGUIOutputEnabled();
    void SetEnableDebugGUIOutput(bool enable);
    bool IsDebugGUIOutputEnabled();
    void SetEnableWarningGUIOutput(bool enable);
    bool IsWarningGUIOutputEnabled();
    void SetEnableErrorGUIOutput(bool enable);
    bool IsErrorGUIOutputEnabled();

} // namespace Logger

#endif // LOGGER_H