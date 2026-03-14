// ========== main_control_window.h ==========
#pragma once

#include <string>
#include "../ThemeManager/ThemeManager.h"
#include "../Simulator/Simulator.h"
#include "../Visualizer/TimeSeriesAutoConfig.h"

class MainControlWindow {
public:
    MainControlWindow();
    ~MainControlWindow() = default;

    void render();
    void loadConfiguration();
    void saveConfiguration();
    void renderPlotControls();
    bool m_showPlotCreator = false;

private:

    void renderSimulatorControls();
    Simulator::SimulationState m_lastSimState = Simulator::IDLE;

    void renderMenuBar();
    void renderParameterControls();
    void renderSimulationControls();
    void renderInterfaceSettings();
    void changeTheme(ThemeManager::ThemeType newTheme);

    // Snapshot operations (buttons in Simulation Control)
    void saveSnapshot();
    void loadSnapshot();

    // Configuration
    char m_simulationName[256] = {};

    // Theme
    bool m_showThemeNotification = false;
    float m_themeNotificationTimer = 0.0f;

    // Simulation state
    bool m_isSimulationRunning = false;

    // Chemistry simulation parameters (from DataManager config)
    int m_maxSteps = 100000;
    float m_plotDelaySec = 0.01f;
    double m_temperature = 1000.0;
    int m_numCarbon = 100;
    int m_numHydrogen = 400;
    int m_numOxygen = 50;
    double m_boxSizeX = 100.0;
    double m_boxSizeY = 100.0;
    double m_boxSizeZ = 100.0;
    double m_interactionCutoff = 3.0;
    double m_dt = 1.0;
    double m_A_form = 1.0;
    double m_A_break = 1.0e-3;
    double m_activationFraction = 0.1;

    // Darwinian daemon parameters (riding daemons)
    bool m_enableDaemons = false;
    bool m_enableStochasticBonds = true;
    int m_daemonTimeout = 500;
    int m_atomResupplyInterval = 0;

    bool m_showImGuiDemo = false;
    bool m_showImPlotDemo = false;
    bool m_showMoleculeCensus = true;
    int m_censusSortMode = 1;  // 0=formula/size, 1=count

    void renderMoleculeCensusWindow();
};
