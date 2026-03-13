#pragma once

#include <vector> // For std::vector
#include <string> // Required for std::string

// Forward declaration for GLFWwindow to avoid including glfw3.h in a public header
struct GLFWwindow;

namespace InterfaceGUI {

    // Initializes the InterfaceGUI system.
    // window: The application's main window handle (e.g., GLFWwindow*).
    // Returns true on success, false otherwise.
    bool Init(GLFWwindow* window);

    // Shuts down the InterfaceGUI system.
    void Shutdown();

    // Begins a new frame for ImGui.
    void BeginFrame();

    // Draws all UI elements.
    // WorkersPerMillionActivePerSector: The current value of parameter A, to be displayed and potentially modified.
    // framerate: Current application framerate.
    // ms_per_frame: Current milliseconds per frame.
    // should_recalculate_gdp: A reference to a boolean flag that will be set to true if the UI requests a recalculation.
    void DrawUI(
        float& WorkersPerMillionActivePerSector, // Pass by ref to allow modification
        float framerate,
        float ms_per_frame,
        bool& should_recalculate_gdp // This might be replaced by direct calls to Simulator
    );

    // Ends the current ImGui frame and renders it.
    void EndFrame();

    // Adds a message to the categorized log windows.
    void AddInfoLog(const std::string& message);
    void AddDebugLog(const std::string& message);
    void AddWarningLog(const std::string& message);
    void AddErrorLog(const std::string& message);

} // namespace InterfaceGUI