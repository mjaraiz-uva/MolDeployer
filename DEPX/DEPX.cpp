// DEPX.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string>
#include <cmath> // For sin function
#include <thread> // Added for potential future use, not strictly needed for this step's main.cpp changes
#include <filesystem>
#include <Windows.h> // Add this include

// --- Backend Specific Includes and Globals ---

#include <glad/glad.h>  // This will now come from vcpkg// GLFW for windowing and input
#include <GLFW/glfw3.h> // Will be initialized by CreateAppWindow

#include "imgui.h" // For ImVec4, ImGui::GetIO() etc.
#include "../InterfaceGUI/InterfaceGUI.h" // Assuming InterfaceGUI.h is in an include path or relative path
#include "../Simulator/Simulator.h"
#include "../DataManager/DataManager.h"
#include "../Logger/Logger.h"
#include "../ThemeManager/ThemeManager.h"
#include "main_control_window.h"

// Global GLFW window object
GLFWwindow* g_Window = nullptr;

static MainControlWindow* g_mainControlWindow = nullptr;

// --- Forward declarations for backend-specific functions ---
bool CreateAppWindow(const char* title, int width, int height); // Changed signature for GLFW
void CleanupAppWindow();
void CleanupGraphicsAPI();
void ProcessPlatformMessages(bool& done); // Handles window messages, sets done to true on quit
void ClearScreen(); // Clears the back buffer
void PresentFrame();  // Swaps buffers

// GLFW window size callback function
static void glfw_window_size_callback(GLFWwindow* window, int width, int height) {
    // Update DataManager with the new window size
    // This ensures that if the application saves its config, it saves the latest size.
    DataManager::UpdateDisplaySize(width, height);
    Logger::Debug("Window resized to: " + std::to_string(width) + "x" + std::to_string(height));
}

// --- Backend-Specific Function Implementations ---
// Error callback for GLFW
static void glfw_error_callback(int error, const char* description) {
    Logger::Error("GLFW Error " + std::to_string(error) + ": " + description);
}

bool CreateAppWindow(const char* title, int width, int height) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        Logger::Error("Failed to initialize GLFW");
        return false;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    g_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (g_Window == nullptr) {
        Logger::Error("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(g_Window);
    glfwSwapInterval(1); // Enable vsync

    // Set the window size callback
    glfwSetWindowSizeCallback(g_Window, glfw_window_size_callback);

    // Apply initial window position from config
    const auto& config_params_for_pos = DataManager::GetConfigParameters();
    glfwSetWindowPos(g_Window, config_params_for_pos.displayPosX, config_params_for_pos.displayPosY);
    Logger::Info("DEPX.cpp: Set initial window position to (" +
        std::to_string(config_params_for_pos.displayPosX) + ", " +
        std::to_string(config_params_for_pos.displayPosY) + ") from configuration.");

    // Initialize GLAD (or your chosen OpenGL loader)
    // This must be done after making the OpenGL context current.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Logger::Error("Failed to initialize GLAD");
        glfwDestroyWindow(g_Window);
        glfwTerminate();
        return false;
    }
    Logger::Info("OpenGL Renderer: " + std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
    Logger::Info("OpenGL Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    Logger::Info("GLSL Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));

    return true;
}

void CleanupAppWindow() {
    if (g_Window) {
        glfwDestroyWindow(g_Window);
        g_Window = nullptr;
    }
}

void CleanupGraphicsAPI() {
    // For GLFW/OpenGL, most graphics cleanup is tied to the window or GLFW termination.
    // Specific OpenGL objects (textures, buffers) created by the app would be cleaned here
    // if not handled by ImGui or Visualizer.
    // For now, this can be minimal as ImGui_ImplOpenGL3_Shutdown handles its objects.
}

void ProcessPlatformMessages(bool& done) {
    glfwPollEvents();
    if (g_Window) { // Check if window still exists
        done = glfwWindowShouldClose(g_Window);
    } else {
        done = true; // If window is gone, we should be done
    }
}

void ClearScreen() {
    // Set clear color (e.g., the ImGui default background color or your choice)
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
}

void PresentFrame() {
    if (g_Window) {
        glfwSwapBuffers(g_Window);
    }
}

int main() {
    // 1. Initialize platform (create window and graphics context)
    // Attempt to load a default configuration first to get window size
    // This is a simplified approach; a more robust system might parse command-line args
    // or have a dedicated pre-init config loading step.
    if (!DataManager::Init()) { // DataManager::Init now loads default config or uses struct defaults
        // Minimal logging here as full logger might not be up yet.
        // Consider a pre-logger or stderr output for critical early failures.
        std::cerr << "Critical Error: Failed to initialize DataManager early for config." << std::endl;
        // No easy recovery, proceed with hardcoded defaults for window size if DM init fails early.
        // Default to a common size if config fails before window creation.
        // This size will be used if DefaultSim.json is missing or doesn't specify displaySize.
        // If Viewports are enabled in Visualizer.cpp, ImGui will then take over managing
        // the main window size via imgui.ini on subsequent runs.
        if (!CreateAppWindow("MolecularDeployer", 1280, 720)) {
            return 1;
        }
    } else {
        // Load a specific configuration if you want to override defaults before window creation
        // For example, load "DefaultSim.json" to get its display settings
        try {
            std::cout << "[DEBUG] CWD in main (before LoadConfigParameters): " << std::filesystem::current_path() << std::endl;
            // Ensure Logger is initialized if you use Logger::Info here, otherwise use std::cout or std::cerr
            // Logger::Info("[DEBUG] CWD in main (before LoadConfigParameters): " + std::filesystem::current_path().string());
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "[DEBUG] Error getting CWD in main: " << e.what() << std::endl;
            // Logger::Error("[DEBUG] Error getting CWD in main: " + std::string(e.what()));
        }
        if (!DataManager::LoadConfigParameters("MolSim")) {
            Logger::Warning("DEPX.cpp: Could not load MolSim.json for initial window size. Using defaults from ConfigParameters struct.");
        }
        const auto& config = DataManager::GetConfigParameters();
        if (!CreateAppWindow("MolecularDeployer", config.displaySizeX, config.displaySizeY)) {
            // Error already logged by CreateAppWindow
            Logger::Shutdown(); // Attempt to shutdown logger if it was initialized by DataManager
            return 1;
        }
    }

    // 1.1 Initialize Logger
    if (!Logger::Init("depx_simulation.log")) {
        CleanupGraphicsAPI();
        CleanupAppWindow();
        glfwTerminate();
        return 1;
    }
    Logger::Info("Application starting.");

    // 1.2 Initialize DataManager (or re-confirm, though Init was called for config)
    // DataManager::Init() was called above. If it needs re-init or further setup, do it here.
    // For now, assume early Init was sufficient.
    // if (!DataManager::Init()) {
    //     Logger::Error("Failed to initialize DataManager");
    //     Logger::Shutdown();
    //     CleanupGraphicsAPI();
    //     CleanupAppWindow();
    //     glfwTerminate();
    //     return 1;
    // }

    // 1.5 Initialize Simulator
    if (!Simulator::Init()) {
        Logger::Error("Failed to initialize Simulator");
        DataManager::Shutdown();
        Logger::Shutdown();
        CleanupGraphicsAPI();
        CleanupAppWindow();
        glfwTerminate();
        return 1;
    }

    // 2. Initialize InterfaceGUI (which initializes Visualizer and ImGui)
    // Pass the GLFW window handle. InterfaceGUI will pass it to Visualizer.
    if (!InterfaceGUI::Init(g_Window)) { // Pass GLFWwindow*
        Logger::Error("Failed to initialize InterfaceGUI");
        Simulator::Shutdown();
        DataManager::Shutdown();
        Logger::Shutdown();
        CleanupGraphicsAPI();
        CleanupAppWindow();
        glfwTerminate();
        return 1;
    }

    g_mainControlWindow = new MainControlWindow();
    g_mainControlWindow->loadConfiguration();
    Logger::Info("MainControlWindow initialized.");

    // Chemistry parameters are managed via ConfigParameters, no per-frame variable needed here

    // Run persistence test if configured
    if (DataManager::GetConfigParameters().testPersistence) {
        Simulator::TestPersistence(7000, 8000);
        // Reset the flag so it doesn't run again on next launch
        DataManager::GetMutableConfigParameters().testPersistence = false;
    }

    // Auto-start simulation if configured
    if (DataManager::GetConfigParameters().autoStart) {
        Logger::Info("Auto-starting simulation (autoStart=true in config).");
        Simulator::StartAsyncCalculation();
    }

    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle messages (event loop)
        ProcessPlatformMessages(done);
        if (done) break;

        // Periodically refresh system theme if using system theme
        static float themeRefreshTimer = 0.0f;
        themeRefreshTimer += ImGui::GetIO().DeltaTime;
        if (themeRefreshTimer > 1.0f) { // Check every second
            ThemeManager::Theme::getInstance().refreshSystemTheme();
            themeRefreshTimer = 0.0f;
        }

        // Start the frame for InterfaceGUI (which calls Visualizer::NewFrame -> ImGui_ImplOpenGL3_NewFrame, ImGui_ImplGlfw_NewFrame, ImGui::NewFrame)
        InterfaceGUI::BeginFrame();

        // Manual full-viewport dockspace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags host_window_flags = 0;
        host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        host_window_flags |= ImGuiWindowFlags_NoBackground; // Optional: makes the dockspace area transparent until windows are docked
        host_window_flags |= ImGuiWindowFlags_NoSavedSettings; // Crucial for ensuring it always fills the viewport

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("DockSpaceWindow", nullptr, host_window_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        
        ImGui::End();
        // End of manual full-viewport dockspace

        // Get framerate and ms/frame from ImGuiIO.
        float current_framerate = ImGui::GetIO().Framerate;
        float current_ms_per_frame = ImGui::GetIO().Framerate > 0 ? 1000.0f / ImGui::GetIO().Framerate : 0.0f;

        // Call InterfaceGUI to draw all UI elements
        // Pass data from DataManager to DrawUI
        //bool temp_recalc_flag = false; // This flag's role is diminished
        //MJ InterfaceGUI::DrawUI(WorkersPerMillionActivePerSector, current_framerate, current_ms_per_frame, temp_recalc_flag);

        // In the main loop, replace or add after InterfaceGUI::DrawUI():
        // Option 1: Replace InterfaceGUI::DrawUI with MainControlWindow
        if (g_mainControlWindow) {
            g_mainControlWindow->render();
        }

        // The logic for starting/stopping simulation is now handled by buttons within DrawUI
        // which call Simulator functions directly.

        // Check if new data is available (consumed by UI plotting)
        DataManager::CheckAndResetNewDataFlag();

        // End the frame for InterfaceGUI (which calls Visualizer::Render -> ImGui::Render and ImGui_ImplOpenGL3_RenderDrawData)
        ClearScreen(); // Clear the screen before rendering ImGui
        InterfaceGUI::EndFrame();
        PresentFrame(); // Swap buffers after rendering
    }

    // Cleanup
    Logger::Info("Application shutting down.");

    // Get and update the final window position before saving configuration
    if (g_Window) {
        int xpos, ypos;
        glfwGetWindowPos(g_Window, &xpos, &ypos);
        DataManager::UpdateDisplayPosition(xpos, ypos);
    }

    // Save current configuration (including potentially updated window size and position)
    if (!DataManager::SaveConfigParameters()) {
        Logger::Error("DEPX: Failed to save configuration parameters on shutdown.");
    }

    // Before shutdown, save configuration and cleanup:
    if (g_mainControlWindow) {
        g_mainControlWindow->saveConfiguration();
        delete g_mainControlWindow;
        g_mainControlWindow = nullptr;
    }

    InterfaceGUI::Shutdown(); // Shuts down Visualizer and ImGui
    Simulator::Shutdown();    // Ensure simulator thread is joined before DataManager is shut down
    DataManager::Shutdown();  // Shutdown the DataManager
    CleanupGraphicsAPI();     // Custom graphics cleanup if any
    CleanupAppWindow();       // Destroys GLFW window
    Logger::Shutdown();       // Shutdown the logger last
    glfwTerminate();          // Terminates GLFW

    return 0;
}

// New WinMain entry point for Windows subsystem
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Call your existing main function
    return main();
}