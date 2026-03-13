// ========== main_control_window.cpp ==========
// Adapted for MolecularDeployer chemistry simulation
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <implot.h>
#include "main_control_window.h"
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h"

MainControlWindow::MainControlWindow()
    : m_configPath("config.json") {
    loadConfiguration();
}

void MainControlWindow::render() {
    renderMenuBar();
    renderSimulatorControls();
    renderParameterControls();
    renderPlotControls();
    renderInterfaceSettings();

    // Demo windows if enabled
    if (m_showImGuiDemo) ImGui::ShowDemoWindow(&m_showImGuiDemo);
    if (m_showImPlotDemo) ImPlot::ShowDemoWindow(&m_showImPlotDemo);

    // Handle theme notification
    if (m_showThemeNotification) {
        m_themeNotificationTimer -= ImGui::GetIO().DeltaTime;
        if (m_themeNotificationTimer <= 0) {
            m_showThemeNotification = false;
        }
    }
}

void MainControlWindow::renderSimulatorControls() {
    if (ImGui::Begin("Simulation Control")) {
        Simulator::SimulationState simState = Simulator::GetSimulationState();

        // Status display
        ImGui::Text("Status: %s", Simulator::StateToString(simState).c_str());
        int currentStep = Simulator::GetCurrentStepCalculated();
        ImGui::Text("Timestep: %d / %d", currentStep, m_maxSteps);

        ImGui::Separator();

        // Control buttons with colors
        const char* runPauseLabel = "Run Simulation";
        ImVec4 runPauseColor = ImVec4(0.2f, 0.5f, 0.2f, 1.0f);

        if (simState == Simulator::RUNNING) {
            runPauseLabel = "Pause";
            runPauseColor = ImVec4(0.5f, 0.5f, 0.2f, 1.0f);
        }
        else if (simState == Simulator::PAUSED) {
            runPauseLabel = "Resume";
            runPauseColor = ImVec4(0.2f, 0.4f, 0.5f, 1.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, runPauseColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(runPauseColor.x * 1.2f, runPauseColor.y * 1.2f, runPauseColor.z * 1.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(runPauseColor.x * 0.8f, runPauseColor.y * 0.8f, runPauseColor.z * 0.8f, 1.0f));
        if (ImGui::Button(runPauseLabel)) {
            if (simState == Simulator::RUNNING) {
                Simulator::PauseCalculation();
            }
            else if (simState == Simulator::PAUSED) {
                Simulator::ResumeCalculation();
            }
            else {
                Simulator::RestartCalculation();
                Simulator::StartAsyncCalculation();
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        // Step button
        bool stepDisabled = !(simState == Simulator::PAUSED);
        if (stepDisabled) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.36f, 0.48f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.24f, 0.32f, 1.0f));
        if (ImGui::Button("Step")) {
            Simulator::StepCalculation();
        }
        ImGui::PopStyleColor(3);
        if (stepDisabled) ImGui::EndDisabled();

        ImGui::SameLine();

        // Reset button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.36f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.24f, 0.16f, 1.0f));
        if (ImGui::Button("Reset")) {
            Simulator::RestartCalculation();
        }
        ImGui::PopStyleColor(3);

        // Calculation delay
        ImGui::Separator();
        ImGui::Text("Delay (s)");
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat("##Delay", &m_plotDelaySec, 0.0f, 0.0f, "%.3f")) {
            if (m_plotDelaySec < 0.001f) m_plotDelaySec = 0.001f;
            if (m_plotDelaySec > 2.0f) m_plotDelaySec = 2.0f;
            Simulator::SetCalculationDelay(m_plotDelaySec);
            saveConfiguration();
        }
    }
    ImGui::End();
}

void MainControlWindow::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Simulation")) {
            if (ImGui::MenuItem("Load Configuration...")) {
                // TODO: File dialog
            }
            if (ImGui::MenuItem("Save Configuration...")) {
                saveConfiguration();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // Set exit flag
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Plots")) {
            if (ImGui::MenuItem("Create Custom Plot...")) {
                Visualizer::TimeSeriesAutoConfig::ShowPlotCreatorDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset All Plot Settings")) {
                ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui Demo", nullptr, &m_showImGuiDemo);
            ImGui::MenuItem("ImPlot Demo", nullptr, &m_showImPlotDemo);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void MainControlWindow::renderParameterControls() {
    if (ImGui::Begin("Simulation Parameters")) {
        ImGui::Text("Chemistry Parameters");
        ImGui::Separator();

        // Temperature
        ImGui::Text("Temperature (K)");
        ImGui::SetNextItemWidth(150);
        float tempFloat = static_cast<float>(m_temperature);
        if (ImGui::InputFloat("##Temperature", &tempFloat, 0.0f, 0.0f, "%.1f")) {
            if (tempFloat < 1.0f) tempFloat = 1.0f;
            if (tempFloat > 100000.0f) tempFloat = 100000.0f;
            m_temperature = tempFloat;
            saveConfiguration();
        }

        ImGui::Spacing();

        // Atom counts
        ImGui::Text("Atom Counts");
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("C atoms##NumCarbon", &m_numCarbon, 0, 0)) {
            if (m_numCarbon < 0) m_numCarbon = 0;
            saveConfiguration();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("H atoms##NumHydrogen", &m_numHydrogen, 0, 0)) {
            if (m_numHydrogen < 0) m_numHydrogen = 0;
            saveConfiguration();
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("O atoms##NumOxygen", &m_numOxygen, 0, 0)) {
            if (m_numOxygen < 0) m_numOxygen = 0;
            saveConfiguration();
        }

        ImGui::Spacing();
        int totalAtoms = m_numCarbon + m_numHydrogen + m_numOxygen;
        ImGui::Text("Total atoms: %d", totalAtoms);

        ImGui::Separator();
        ImGui::Text("Simulation Box");

        float boxX = static_cast<float>(m_boxSizeX);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat("X##BoxX", &boxX, 0.0f, 0.0f, "%.1f")) {
            if (boxX < 1.0f) boxX = 1.0f;
            m_boxSizeX = boxX;
            saveConfiguration();
        }
        ImGui::SameLine();
        float boxY = static_cast<float>(m_boxSizeY);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat("Y##BoxY", &boxY, 0.0f, 0.0f, "%.1f")) {
            if (boxY < 1.0f) boxY = 1.0f;
            m_boxSizeY = boxY;
            saveConfiguration();
        }
        ImGui::SameLine();
        float boxZ = static_cast<float>(m_boxSizeZ);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat("Z##BoxZ", &boxZ, 0.0f, 0.0f, "%.1f")) {
            if (boxZ < 1.0f) boxZ = 1.0f;
            m_boxSizeZ = boxZ;
            saveConfiguration();
        }

        ImGui::Separator();
        ImGui::Text("Kinetic Parameters");

        float cutoff = static_cast<float>(m_interactionCutoff);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat("Cutoff##Cutoff", &cutoff, 0.0f, 0.0f, "%.1f")) {
            if (cutoff < 0.1f) cutoff = 0.1f;
            m_interactionCutoff = cutoff;
            saveConfiguration();
        }

        float dtFloat = static_cast<float>(m_dt);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat("dt##Timestep", &dtFloat, 0.0f, 0.0f, "%.3f")) {
            if (dtFloat < 0.001f) dtFloat = 0.001f;
            m_dt = dtFloat;
            saveConfiguration();
        }

        ImGui::Separator();
        ImGui::Text("Max Steps");
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("##MaxSteps", &m_maxSteps, 0, 0)) {
            if (m_maxSteps < 1) m_maxSteps = 1;
            if (m_maxSteps > 10000000) m_maxSteps = 10000000;
            saveConfiguration();
        }
    }
    ImGui::End();
}

void MainControlWindow::renderInterfaceSettings() {
    ImGui::Text("User Interface Settings");
    ImGui::Separator();

    ImGui::Text("Color Theme");

    static const char* themeItems[] = { "Dark", "Light", "System" };
    ThemeManager::Theme& tm = ThemeManager::Theme::getInstance();
    int currentThemeIndex = static_cast<int>(tm.getCurrentTheme());

    if (ImGui::Combo("##Theme", &currentThemeIndex, themeItems, IM_ARRAYSIZE(themeItems))) {
        changeTheme(static_cast<ThemeManager::ThemeType>(currentThemeIndex));
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Dark: Optimized for low-light environments");
        ImGui::Text("Light: Best for bright environments");
        ImGui::Text("System: Follows your OS theme settings");
        ImGui::EndTooltip();
    }

    ImGui::Text("Current: %s", tm.getCurrentThemeName());
    if (tm.getCurrentTheme() == ThemeManager::ThemeType::System) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "(Using %s mode)", tm.isDarkMode() ? "dark" : "light");
    }

    ImGui::Separator();
    ImGui::Text("Display Options");

    static bool showFPS = true;
    ImGui::Checkbox("Show FPS Counter", &showFPS);

    static bool vsync = true;
    ImGui::Checkbox("Enable V-Sync", &vsync);

    static float uiScale = 1.0f;
    if (ImGui::SliderFloat("UI Scale", &uiScale, 0.75f, 2.0f, "%.2fx")) {
        ImGui::GetIO().FontGlobalScale = uiScale;
    }
}

void MainControlWindow::changeTheme(ThemeManager::ThemeType newTheme) {
    ThemeManager::Theme::getInstance().applyTheme(newTheme);

    auto& config = DataManager::GetMutableConfigParameters();
    config.uiTheme = ThemeManager::Theme::getInstance().themeToString(newTheme);

    DataManager::SaveConfigParameters();

    m_showThemeNotification = true;
    m_themeNotificationTimer = 2.0f;
}

void MainControlWindow::loadConfiguration() {
    try {
        const auto& config = DataManager::GetConfigParameters();
        m_maxSteps = config.maxSteps;
        m_plotDelaySec = config.plotDelaySec;
        m_temperature = config.temperature;
        m_numCarbon = config.numCarbon;
        m_numHydrogen = config.numHydrogen;
        m_numOxygen = config.numOxygen;
        m_boxSizeX = config.boxSizeX;
        m_boxSizeY = config.boxSizeY;
        m_boxSizeZ = config.boxSizeZ;
        m_interactionCutoff = config.interactionCutoff;
        m_dt = config.dt;
        m_A_form = config.A_form;
        m_A_break = config.A_break;
        m_activationFraction = config.activationFraction;

        // Load and apply theme
        ThemeManager::ThemeType theme = ThemeManager::Theme::getInstance().stringToTheme(config.uiTheme);
        ThemeManager::Theme::getInstance().applyTheme(theme);
    }
    catch (const std::exception& e) {
        Logger::Error(std::string("Error loading configuration: ") + e.what());
    }
}

void MainControlWindow::saveConfiguration() {
    try {
        auto& config = DataManager::GetMutableConfigParameters();

        config.maxSteps = m_maxSteps;
        config.plotDelaySec = m_plotDelaySec;
        config.temperature = m_temperature;
        config.numCarbon = m_numCarbon;
        config.numHydrogen = m_numHydrogen;
        config.numOxygen = m_numOxygen;
        config.boxSizeX = m_boxSizeX;
        config.boxSizeY = m_boxSizeY;
        config.boxSizeZ = m_boxSizeZ;
        config.interactionCutoff = m_interactionCutoff;
        config.dt = m_dt;
        config.A_form = m_A_form;
        config.A_break = m_A_break;
        config.activationFraction = m_activationFraction;

        DataManager::SaveConfigParameters();
    }
    catch (const std::exception& e) {
        Logger::Error(std::string("Error saving configuration: ") + e.what());
    }
}

void MainControlWindow::renderPlotControls() {
    // Show all registered chemistry plots
    Visualizer::TimeSeriesAutoConfig::ShowAllRegisteredPlots(m_maxSteps);
}
