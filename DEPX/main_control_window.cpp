// ========== main_control_window.cpp ==========
// Adapted for MolecularDeployer chemistry simulation
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <implot.h>
#include "main_control_window.h"
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h"
#include "../Chemistry/Reactor.h"
#include "Screenshot.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

MainControlWindow::MainControlWindow() {
    loadConfiguration();
    // Initialize name buffer from loaded config
    const auto& name = DataManager::GetConfigParameters().simulationName;
    strncpy_s(m_simulationName, sizeof(m_simulationName), name.c_str(), _TRUNCATE);
}

void MainControlWindow::render() {
    renderMenuBar();
    renderSimulatorControls();
    renderParameterControls();
    renderPlotControls();
    renderMoleculeCensusWindow();
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
        else if (simState == Simulator::COMPLETED &&
                 Simulator::GetCurrentStepCalculated() + 1 < m_maxSteps) {
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
            else if (simState == Simulator::COMPLETED &&
                     Simulator::GetCurrentStepCalculated() + 1 < m_maxSteps) {
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

        // Save Snapshot button (new row)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.3f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.36f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.24f, 0.4f, 1.0f));
        if (ImGui::Button("Save")) {
            saveSnapshot();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        // Load Snapshot button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.2f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.24f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.16f, 0.4f, 1.0f));
        if (ImGui::Button("Load")) {
            loadSnapshot();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        // Screenshot button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.48f, 0.48f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.32f, 0.32f, 1.0f));
        if (ImGui::Button("Snap")) {
            int step = Simulator::GetCurrentStepCalculated();
            std::string name = DataManager::GetConfigParameters().simulationName;
            std::string filename = name + "_" + std::to_string(step < 0 ? 0 : step) + ".bmp";
            Screenshot::Request(filename);
        }
        ImGui::PopStyleColor(3);

        // Calculation delay
        ImGui::Separator();
        ImGui::Text("Delay (s)");
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("##Delay", &m_plotDelaySec, 0.0f, 0.0f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_plotDelaySec < 0.001f) m_plotDelaySec = 0.001f;
            if (m_plotDelaySec > 2.0f) m_plotDelaySec = 2.0f;
            Simulator::SetCalculationDelay(m_plotDelaySec);
            saveConfiguration();
        }

        ImGui::Text("Save Every");
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##SnapshotInterval", &m_snapshotInterval, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_snapshotInterval < 0) m_snapshotInterval = 0;
            saveConfiguration();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Auto-save snapshot every N steps (0=disabled)");
        }
    }
    ImGui::End();
}

void MainControlWindow::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Simulation")) {
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
            ImGui::MenuItem("Molecule Census", nullptr, &m_showMoleculeCensus);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &m_showImGuiDemo);
            ImGui::MenuItem("ImPlot Demo", nullptr, &m_showImPlotDemo);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void MainControlWindow::renderParameterControls() {
    if (ImGui::Begin("Simulation Parameters")) {
        // Simulation name
        ImGui::Text("Simulation Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##SimName", m_simulationName, sizeof(m_simulationName));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            auto& config = DataManager::GetMutableConfigParameters();
            config.simulationName = m_simulationName;
            saveConfiguration();
            Logger::Info("Simulation name changed to: " + std::string(m_simulationName));
        }

        ImGui::Separator();
        ImGui::Text("Chemistry Parameters");
        ImGui::Separator();

        // Temperature
        ImGui::Text("Temperature (K)");
        ImGui::SetNextItemWidth(150);
        float tempFloat = static_cast<float>(m_temperature);
        ImGui::InputFloat("##Temperature", &tempFloat, 0.0f, 0.0f, "%.1f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (tempFloat < 1.0f) tempFloat = 1.0f;
            if (tempFloat > 100000.0f) tempFloat = 100000.0f;
            m_temperature = tempFloat;
            saveConfiguration();
        }

        ImGui::Spacing();

        // Atom counts
        ImGui::Text("Atom Counts");
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("C atoms##NumCarbon", &m_numCarbon, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_numCarbon < 0) m_numCarbon = 0;
            saveConfiguration();
        }
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("H atoms##NumHydrogen", &m_numHydrogen, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_numHydrogen < 0) m_numHydrogen = 0;
            saveConfiguration();
        }
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("O atoms##NumOxygen", &m_numOxygen, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_numOxygen < 0) m_numOxygen = 0;
            saveConfiguration();
        }

        ImGui::Spacing();
        int totalAtoms = m_numCarbon + m_numHydrogen + m_numOxygen;
        ImGui::Text("Total atoms: %d", totalAtoms);

        ImGui::Separator();
        ImGui::Text("Initial Molecules");
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("H2O##InitH2O", &m_initialH2O, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_initialH2O < 0) m_initialH2O = 0;
            saveConfiguration();
        }
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("CO2##InitCO2", &m_initialCO2, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_initialCO2 < 0) m_initialCO2 = 0;
            saveConfiguration();
        }
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("O2##InitO2", &m_initialO2, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_initialO2 < 0) m_initialO2 = 0;
            saveConfiguration();
        }

        ImGui::Separator();
        ImGui::Text("Simulation Box");

        float boxX = static_cast<float>(m_boxSizeX);
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("X##BoxX", &boxX, 0.0f, 0.0f, "%.1f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (boxX < 1.0f) boxX = 1.0f;
            m_boxSizeX = boxX;
            saveConfiguration();
        }
        float boxY = static_cast<float>(m_boxSizeY);
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("Y##BoxY", &boxY, 0.0f, 0.0f, "%.1f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (boxY < 1.0f) boxY = 1.0f;
            m_boxSizeY = boxY;
            saveConfiguration();
        }
        float boxZ = static_cast<float>(m_boxSizeZ);
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("Z##BoxZ", &boxZ, 0.0f, 0.0f, "%.1f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (boxZ < 1.0f) boxZ = 1.0f;
            m_boxSizeZ = boxZ;
            saveConfiguration();
        }

        ImGui::Separator();
        ImGui::Text("Kinetic Parameters");

        float cutoff = static_cast<float>(m_interactionCutoff);
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("Cutoff##Cutoff", &cutoff, 0.0f, 0.0f, "%.1f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (cutoff < 0.1f) cutoff = 0.1f;
            m_interactionCutoff = cutoff;
            saveConfiguration();
        }

        float dtFloat = static_cast<float>(m_dt);
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("dt##Timestep", &dtFloat, 0.0f, 0.0f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (dtFloat < 0.001f) dtFloat = 0.001f;
            m_dt = dtFloat;
            saveConfiguration();
        }

        ImGui::Separator();
        ImGui::Text("Ionizing Radiation");

        float radFlux = static_cast<float>(m_radiationFlux);
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("Flux##RadFlux", &radFlux, 0.0f, 0.0f, "%.4f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (radFlux < 0.0f) radFlux = 0.0f;
            if (radFlux > 1.0f) radFlux = 1.0f;
            m_radiationFlux = radFlux;
            saveConfiguration();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Photon flux intensity (0=disabled, ~1e-4 to 1e-2)");
        }

        float radMax = static_cast<float>(m_radiationMaxEnergy);
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("Max E##RadMax", &radMax, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (radMax < 100.0f) radMax = 100.0f;
            m_radiationMaxEnergy = radMax;
            saveConfiguration();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Max photon energy (kJ/mol). 1200 ~ 12.4 eV (Lyman-alpha)");
        }

        ImGui::Separator();
        ImGui::Text("Darwinian Assembly");

        ImGui::Checkbox("Enable Daemons##EnableDaemons", &m_enableDaemons);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            saveConfiguration();
        }

        ImGui::Checkbox("Enable Stochastic Bonds##EnableStochastic", &m_enableStochasticBonds);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            saveConfiguration();
        }

        if (m_enableDaemons) {
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("Timeout##DaemonTimeout", &m_daemonTimeout, 0, 0);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (m_daemonTimeout < 1) m_daemonTimeout = 1;
                saveConfiguration();
            }
        }

        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Resupply##AtomResupply", &m_atomResupplyInterval, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_atomResupplyInterval < 0) m_atomResupplyInterval = 0;
            saveConfiguration();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Inject fresh atoms every N steps (0=disabled)");
        }

        ImGui::Separator();
        ImGui::Text("Max Steps");
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("##MaxSteps", &m_maxSteps, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_maxSteps < 1) m_maxSteps = 1;
            if (m_maxSteps > 10000000) m_maxSteps = 10000000;
            saveConfiguration();
        }
    }
    ImGui::End();
}

void MainControlWindow::renderInterfaceSettings() {
    if (!ImGui::Begin("User Interface Settings")) {
        ImGui::End();
        return;
    }

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
    ImGui::Text("UI Scale");
    ImGui::SetNextItemWidth(160);
    float scale = m_uiScale;
    if (ImGui::SliderFloat("##UIScale", &scale, 0.75f, 2.0f, "%.2fx")) {
        m_uiScale = scale;
        ImGui::GetIO().FontGlobalScale = m_uiScale;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        saveConfiguration();
    }
    ImGui::End();
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
        m_initialH2O = config.initialH2O;
        m_initialCO2 = config.initialCO2;
        m_initialO2 = config.initialO2;
        m_radiationFlux = config.radiationFlux;
        m_radiationMaxEnergy = config.radiationMaxEnergy;
        m_enableDaemons = config.enableDaemons;
        m_enableStochasticBonds = config.enableStochasticBonds;
        m_daemonTimeout = config.daemonTimeout;
        m_atomResupplyInterval = config.atomResupplyInterval;
        m_censusSortMode = config.censusSortMode;
        m_snapshotInterval = config.snapshotInterval;
        m_uiScale = config.uiScale;
        ImGui::GetIO().FontGlobalScale = m_uiScale;

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
        config.initialH2O = m_initialH2O;
        config.initialCO2 = m_initialCO2;
        config.initialO2 = m_initialO2;
        config.radiationFlux = m_radiationFlux;
        config.radiationMaxEnergy = m_radiationMaxEnergy;
        config.enableDaemons = m_enableDaemons;
        config.enableStochasticBonds = m_enableStochasticBonds;
        config.daemonTimeout = m_daemonTimeout;
        config.atomResupplyInterval = m_atomResupplyInterval;
        config.censusSortMode = m_censusSortMode;
        config.snapshotInterval = m_snapshotInterval;
        config.uiScale = m_uiScale;

        DataManager::SaveConfigParameters();
    }
    catch (const std::exception& e) {
        Logger::Error(std::string("Error saving configuration: ") + e.what());
    }
}

void MainControlWindow::saveSnapshot() {
    // Auto-generate filename: simulationname_step.json
    int currentStep = Simulator::GetCurrentStepCalculated();
    std::string name = DataManager::GetConfigParameters().simulationName;
    if (currentStep < 0) currentStep = 0;
    std::string filename = name + "_" + std::to_string(currentStep) + ".json";

    if (Simulator::SaveSnapshot(filename)) {
        Logger::Info("Snapshot saved: " + filename);
    }
}

void MainControlWindow::loadSnapshot() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Snapshot Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Load Snapshot";

    if (GetOpenFileNameA(&ofn)) {
        if (Simulator::LoadSnapshot(filename)) {
            loadConfiguration();
            // Update the simulation name buffer
            const auto& name = DataManager::GetConfigParameters().simulationName;
            strncpy_s(m_simulationName, sizeof(m_simulationName), name.c_str(), _TRUNCATE);
            Logger::Info("Snapshot loaded: " + std::string(filename));
        }
    }
#endif
}

// Count total atoms in a Hill-system formula like "C2H6", "CH4", "H2"
static int CountAtomsInFormula(const std::string& formula) {
    int total = 0;
    for (size_t i = 0; i < formula.size(); ++i) {
        if (std::isupper(formula[i])) {
            // Skip lowercase continuation (e.g. nothing for C,H,O but future-proof)
            size_t j = i + 1;
            while (j < formula.size() && std::islower(formula[j])) ++j;
            // Parse digit count
            int count = 0;
            while (j < formula.size() && std::isdigit(formula[j])) {
                count = count * 10 + (formula[j] - '0');
                ++j;
            }
            total += (count > 0) ? count : 1;
        }
    }
    return total;
}

void MainControlWindow::renderMoleculeCensusWindow() {
    if (!m_showMoleculeCensus) return;

    if (ImGui::Begin("Molecule Census", &m_showMoleculeCensus)) {
        auto* reactor = static_cast<Chemistry::Reactor*>(Simulator::GetReactor());
        if (reactor) {
            int step = reactor->GetCurrentStep();
            int molCount = reactor->GetMoleculeCount();
            int freeAtoms = reactor->GetFreeAtomCount();
            ImGui::Text("Step: %d", step);
            ImGui::Text("Molecules: %d  |  Free atoms: %d",
                molCount, freeAtoms);

            // Sort mode selector on same line as summary
            ImGui::Text("Sort by:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            const char* sortItems[] = { "Size (atoms)", "Count" };
            if (ImGui::Combo("##census_sort", &m_censusSortMode, sortItems, IM_ARRAYSIZE(sortItems))) {
                saveConfiguration();
            }

            ImGui::Separator();

            // Copy census to avoid data race with simulation thread
            auto census = reactor->GetMoleculeCensus();
            if (census.empty()) {
                ImGui::TextDisabled("No molecules formed yet.");
            } else {
                // Compute total molecules for percentage
                int totalMols = 0;
                for (const auto& [f, n] : census) totalMols += n;

                // Build sortable list with atom count
                struct Entry { std::string formula; int count; int atoms; };
                std::vector<Entry> entries;
                entries.reserve(census.size());
                for (const auto& [f, n] : census) {
                    entries.push_back({f, n, CountAtomsInFormula(f)});
                }

                if (m_censusSortMode == 0) {
                    // Sort by atom count descending (largest molecules first)
                    std::sort(entries.begin(), entries.end(),
                        [](const Entry& a, const Entry& b) { return a.atoms > b.atoms; });
                } else {
                    // Sort by count descending
                    std::sort(entries.begin(), entries.end(),
                        [](const Entry& a, const Entry& b) { return a.count > b.count; });
                }

                ImGui::Columns(3, "census_cols");
                ImGui::SetColumnWidth(0, 100);
                ImGui::SetColumnWidth(1, 50);
                ImGui::TextUnformatted("Formula"); ImGui::NextColumn();
                ImGui::TextUnformatted("Size"); ImGui::NextColumn();
                ImGui::TextUnformatted("Count"); ImGui::NextColumn();
                ImGui::Separator();

                for (const auto& e : entries) {
                    ImGui::TextUnformatted(e.formula.c_str()); ImGui::NextColumn();
                    ImGui::Text("%d", e.atoms); ImGui::NextColumn();
                    float pct = totalMols > 0 ? 100.0f * e.count / totalMols : 0.0f;
                    ImGui::Text("%d (%.0f%%)", e.count, pct); ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
        } else {
            ImGui::TextDisabled("No reactor active.");
        }
    }
    ImGui::End();
}

void MainControlWindow::renderPlotControls() {
    // Show all registered chemistry plots
    Visualizer::TimeSeriesAutoConfig::ShowAllRegisteredPlots(m_maxSteps);
}
