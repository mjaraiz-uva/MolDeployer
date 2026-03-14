// TimeSeriesDefinitions.cpp - Chemistry domain time series registration
// Replaces Region/TimeSeriesDefinitions.cpp for the MolecularDeployer chemistry simulator.
#include <imgui.h>
#include <implot.h>
#include "../Simulator/Simulator.h"
#include "../DataManager/DataManager.h"
#include "../Logger/Logger.h"
#include "Reactor.h"

// Helper to safely get the Reactor pointer
static Chemistry::Reactor* GetReactorSafe() {
    void* ptr = Simulator::GetReactor();
    return ptr ? static_cast<Chemistry::Reactor*>(ptr) : nullptr;
}

// Helper function to reduce boilerplate
static void RegisterTS(
    const std::string& id,
    const std::string& displayName,
    const std::string& yAxisLabel,
    std::function<float()> valueGetter,
    ImVec4 color,
    ImPlotMarker marker)
{
    DataManager::TimeSeriesDefinition def;
    def.id = id;
    def.displayName = displayName;
    def.yAxisLabel = yAxisLabel;
    def.valueGetter = valueGetter;
    def.color = color;
    def.marker = marker;
    def.markerSize = 2.0f;
    def.enabledByDefault = true;
    DataManager::TimeSeriesRegistry::GetInstance().RegisterTimeSeries(def);
}

// Static registration objects for chemistry time series
namespace {
    struct RegisterAllTimeSeries {
        RegisterAllTimeSeries() {
            // ===== ENERGY =====
            RegisterTS("total_bond_energy", "Total Bond Energy", "kJ/mol",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetTotalBondEnergy()) : 0.0f;
                },
                ImVec4(0.9f, 0.2f, 0.2f, 1.0f),
                ImPlotMarker_Circle);

            // ===== POPULATION DYNAMICS =====
            RegisterTS("molecule_count", "Molecules", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMoleculeCount()) : 0.0f;
                },
                ImVec4(0.2f, 0.7f, 0.2f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("free_atom_count", "Free Atoms", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetFreeAtomCount()) : 0.0f;
                },
                ImVec4(0.8f, 0.4f, 0.2f, 1.0f),
                ImPlotMarker_Square);

            // ===== SPECIES TRACKING =====
            RegisterTS("free_C_count", "Free C", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetFreeAtomCountByElement(Chemistry::Element::C)) : 0.0f;
                },
                ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("free_H_count", "Free H", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetFreeAtomCountByElement(Chemistry::Element::H)) : 0.0f;
                },
                ImVec4(0.9f, 0.9f, 0.9f, 1.0f),
                ImPlotMarker_Square);

            RegisterTS("free_O_count", "Free O", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetFreeAtomCountByElement(Chemistry::Element::O)) : 0.0f;
                },
                ImVec4(0.9f, 0.2f, 0.2f, 1.0f),
                ImPlotMarker_Diamond);

            // ===== BOND KINETICS =====
            RegisterTS("bond_count", "Total Bonds", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetBondCount()) : 0.0f;
                },
                ImVec4(0.2f, 0.5f, 0.9f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("formation_events", "Formations/step", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetFormationEventsThisStep()) : 0.0f;
                },
                ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                ImPlotMarker_Up);

            RegisterTS("breaking_events", "Breakings/step", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetBreakingEventsThisStep()) : 0.0f;
                },
                ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                ImPlotMarker_Down);

            // ===== KNOWN MOLECULES =====
            RegisterTS("H2_count", "H2", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMoleculeCountByFormula("H2")) : 0.0f;
                },
                ImVec4(0.9f, 0.9f, 0.5f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("H2O_count", "H2O", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMoleculeCountByFormula("H2O")) : 0.0f;
                },
                ImVec4(0.3f, 0.5f, 0.9f, 1.0f),
                ImPlotMarker_Square);

            RegisterTS("CH4_count", "CH4", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMoleculeCountByFormula("CH4")) : 0.0f;
                },
                ImVec4(0.5f, 0.9f, 0.5f, 1.0f),
                ImPlotMarker_Diamond);

            RegisterTS("CO2_count", "CO2", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMoleculeCountByFormula("CO2")) : 0.0f;
                },
                ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                ImPlotMarker_Up);

            // ===== HYDROCARBON CHAINS =====
            RegisterTS("C2H6_count", "C2H6", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMoleculeCountByFormula("C2H6")) : 0.0f;
                },
                ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("C3H8_count", "C3H8", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMoleculeCountByFormula("C3H8")) : 0.0f;
                },
                ImVec4(0.2f, 0.9f, 0.6f, 1.0f),
                ImPlotMarker_Square);

            RegisterTS("C4plus_count", "C4+ chains", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    if (!reactor) return 0.0f;
                    // Count all molecules with 4+ carbons
                    int count = 0;
                    const auto& census = reactor->GetMoleculeCensus();
                    for (const auto& [formula, n] : census) {
                        // Check if formula starts with "C" followed by a digit >= 4
                        if (formula.size() >= 2 && formula[0] == 'C' && formula[1] >= '4' && formula[1] <= '9') {
                            count += n;
                        } else if (formula.size() >= 3 && formula[0] == 'C' && formula[1] >= '1' && formula[1] <= '9' && formula[2] >= '0' && formula[2] <= '9') {
                            // C10+
                            count += n;
                        }
                    }
                    return static_cast<float>(count);
                },
                ImVec4(0.9f, 0.2f, 0.5f, 1.0f),
                ImPlotMarker_Diamond);

            // ===== DARWINIAN ASSEMBLY =====
            RegisterTS("searching_daemons", "Searching Daemons", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetSearchingDaemonCount()) : 0.0f;
                },
                ImVec4(0.8f, 0.2f, 0.8f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("holding_daemons", "Holding Daemons", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetHoldingDaemonCount()) : 0.0f;
                },
                ImVec4(0.2f, 0.8f, 0.8f, 1.0f),
                ImPlotMarker_Square);

            RegisterTS("daemon_spawns", "Daemon Spawns/step", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetDaemonSpawnedThisStep()) : 0.0f;
                },
                ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                ImPlotMarker_Up);

            RegisterTS("daemon_successes", "Daemon Successes/step", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetDaemonSuccessThisStep()) : 0.0f;
                },
                ImVec4(0.2f, 0.6f, 0.9f, 1.0f),
                ImPlotMarker_Diamond);

            RegisterTS("daemon_deaths", "Daemon Deaths/step", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetDaemonDeathsThisStep()) : 0.0f;
                },
                ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                ImPlotMarker_Cross);

            RegisterTS("rearrangements", "Rearrangements/step", "Count",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetRearrangementsThisStep()) : 0.0f;
                },
                ImVec4(0.9f, 0.9f, 0.2f, 1.0f),
                ImPlotMarker_Asterisk);

            // ===== COMPLEXITY =====
            RegisterTS("avg_molecule_size", "Avg Molecule Size", "Atoms",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetAverageMoleculeSize()) : 0.0f;
                },
                ImVec4(0.6f, 0.3f, 0.9f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("max_molecule_size", "Max Molecule Size", "Atoms",
                []() {
                    auto reactor = GetReactorSafe();
                    return reactor ? static_cast<float>(reactor->GetMaxMoleculeSize()) : 0.0f;
                },
                ImVec4(0.9f, 0.6f, 0.3f, 1.0f),
                ImPlotMarker_Square);
        }
    } registerAllTimeSeries;

    // ========== REGISTER PLOT WINDOWS ==========
    struct RegisterPlotWindows {
        RegisterPlotWindows() {
            auto& registry = DataManager::TimeSeriesRegistry::GetInstance();

            // Energy Evolution
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Energy Evolution";
                def.plotTitle = "##EnergyEvolution";
                def.seriesIds = { "total_bond_energy" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Population Dynamics
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Population Dynamics";
                def.plotTitle = "##PopulationDynamics";
                def.seriesIds = { "molecule_count", "free_atom_count" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Species Tracking
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Species Tracking";
                def.plotTitle = "##SpeciesTracking";
                def.seriesIds = { "free_C_count", "free_H_count", "free_O_count" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Bond Kinetics
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Bond Kinetics";
                def.plotTitle = "##BondKinetics";
                def.seriesIds = { "bond_count", "formation_events", "breaking_events" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Known Molecules
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Known Molecules";
                def.plotTitle = "##KnownMolecules";
                def.seriesIds = { "H2_count", "H2O_count", "CH4_count", "CO2_count" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Hydrocarbon Chains
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Hydrocarbon Chains";
                def.plotTitle = "##HydrocarbonChains";
                def.seriesIds = { "CH4_count", "C2H6_count", "C3H8_count", "C4plus_count" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Daemon Population
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Daemon Population";
                def.plotTitle = "##DaemonPopulation";
                def.seriesIds = { "searching_daemons", "holding_daemons" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Daemon Events
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Daemon Events";
                def.plotTitle = "##DaemonEvents";
                def.seriesIds = { "daemon_spawns", "daemon_successes", "daemon_deaths", "rearrangements" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Complexity
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Complexity";
                def.plotTitle = "##Complexity";
                def.seriesIds = { "avg_molecule_size", "max_molecule_size" };
                def.legendLocation = ImPlotLocation_SouthEast;
                def.legendOutside = true;
                def.ticksEvery = 1000;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }
        }
    } registerPlotWindows;

} // anonymous namespace

// Force linking and initialization
namespace Chemistry {
    void EnsureTimeSeriesRegistration() {
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
            auto& registry = DataManager::TimeSeriesRegistry::GetInstance();
            auto plots = registry.GetRegisteredPlotWindows();
            Logger::Info("Chemistry::TimeSeriesDefinitions: Registration complete, found " +
                std::to_string(plots.size()) + " plot windows");

            for (const auto& plot : plots) {
                Logger::Debug("  - Registered plot: " + plot.windowName);
            }
        }
    }
}
