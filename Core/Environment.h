#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <random>
#include <nlohmann/json.hpp>
#include "Vec3.h"
#include "Agent.h"
#include "Bond.h"
#include "Assembly.h"
#include "Species.h"
#include "Recipe.h"
#include "SpatialGrid.h"
#include "IDomainRules.h"

namespace DABM {

// Ensures time series are registered (called during initialization)
void EnsureTimeSeriesRegistration();

// Environment: the container and timestep orchestrator for a D-ABM simulation.
// Replaces Chemistry::Reactor with domain-agnostic naming.
// Domain-specific physics are delegated to IDomainRules.
class Environment {
public:
    Environment(const std::string& name = "DefaultEnvironment");
    ~Environment();

    bool Initialize();
    void RunTimestep(int step);
    void Cleanup();

    nlohmann::json SaveSnapshot() const;
    bool LoadSnapshot(const nlohmann::json& snapshot);

    // --- Domain rules injection ---
    void SetDomainRules(IDomainRules* rules) { m_rules = rules; }
    IDomainRules* GetDomainRules() const { return m_rules; }

    // --- Registries (public for domain rules to populate) ---
    SpeciesRegistry& GetSpeciesRegistry() { return m_species; }
    const SpeciesRegistry& GetSpeciesRegistry() const { return m_species; }
    RecipeBook& GetRecipeBook() { return m_recipes; }
    const RecipeBook& GetRecipeBook() const { return m_recipes; }

    // --- Agent/Bond/Assembly creation (called by domain rules and internally) ---
    Agent* CreateAgent(int speciesId, const Vec3& position, const Vec3& velocity);
    Bond* CreateBond(Agent* a1, Agent* a2, int order, int step);
    void RemoveBond(Bond* bond);
    Assembly* CreateAssembly(int step);

    // --- RNG access ---
    std::mt19937& GetRNG() { return m_rng; }

    // --- Statistics getters ---
    double GetTotalBondEnergy() const { return m_totalBondEnergy; }
    int GetAssemblyCount() const { return m_assemblyCount; }
    int GetFreeAgentCount() const { return m_freeAgentCount; }
    int GetBondCount() const { return static_cast<int>(m_bonds.size()); }
    double GetAverageAssemblySize() const { return m_avgAssemblySize; }
    int GetMaxAssemblySize() const { return m_maxAssemblySize; }
    int GetFormationEventsThisStep() const { return m_formationEventsThisStep; }
    int GetBreakingEventsThisStep() const { return m_breakingEventsThisStep; }
    int GetRadiationBreakingEventsThisStep() const { return m_radiationBreakingThisStep; }

    // Daemon statistics
    int GetActiveDaemonCount() const;
    int GetSearchingDaemonCount() const;
    int GetHoldingDaemonCount() const;
    int GetDaemonSpawnedThisStep() const { return m_daemonSpawnedThisStep; }
    int GetDaemonSuccessThisStep() const { return m_daemonSuccessThisStep; }
    int GetDaemonDeathsThisStep() const { return m_daemonDeathsThisStep; }
    int GetRearrangementsThisStep() const { return m_rearrangementsThisStep; }

    // Per-species free agent count
    int GetFreeAgentCountBySpecies(int speciesId) const;

    // Assembly census
    int GetAssemblyCountByFormula(const std::string& formula) const;
    std::map<std::string, int> GetAssemblyCensus() const;

    size_t GetAgentCount() const { return m_agents.size(); }
    const Agent* GetAgentByIndex(size_t index) const;
    int GetCurrentStep() const { return m_currentStep; }

    // --- Simulation parameters (set from config) ---
    Vec3 m_boxSize;
    double m_temperature = 1000.0;
    double m_dt = 1.0;
    std::string m_boundaryType = "reflective";

    // Per-step counters (public for domain rules to increment)
    int m_formationEventsThisStep = 0;
    int m_breakingEventsThisStep = 0;
    int m_radiationBreakingThisStep = 0;
    int m_daemonSpawnedThisStep = 0;
    int m_daemonSuccessThisStep = 0;
    int m_daemonDeathsThisStep = 0;
    int m_rearrangementsThisStep = 0;

private:
    // --- Timestep sub-operations ---
    void MoveAgents(double dt);
    void ApplyBoundaryConditions();
    void AttemptBondBreaking();
    void AttemptBondFormation();
    void UpdateAssemblies();
    void CalculateStats();

    // --- Riding daemon lifecycle ---
    void AssignInitialDaemons();
    void RunAgentDaemons();
    void HandleSearchingDaemon(Agent* agent, size_t agentIndex, AgentState* ds);
    void HandleHoldingDaemon(Agent* agent, AgentState* ds);
    void ReassignDaemon(Agent* agent);
    void SpawnOffspringDaemon(Agent* neighbor, const Recipe* parentRecipe);

    // Thermodynamic rearrangement
    Agent* MakeRoomForBond(Agent* anchor, double newBondEnergy);

    // --- Data ---
    std::string m_name;
    bool m_initialized = false;
    int m_currentStep = 0;

    // Owned entities
    std::vector<std::unique_ptr<Agent>> m_agents;
    std::vector<std::unique_ptr<Bond>> m_bonds;
    std::vector<std::unique_ptr<Assembly>> m_assemblies;

    // Registries
    SpeciesRegistry m_species;
    RecipeBook m_recipes;

    // Domain-specific rules (non-owning, injected)
    IDomainRules* m_rules = nullptr;

    // Spatial acceleration
    SpatialGrid m_spatialGrid;

    // Internal counters
    int m_nextBondId = 0;
    int m_nextAssemblyId = 0;

    // Per-step statistics
    double m_totalBondEnergy = 0.0;
    int m_assemblyCount = 0;
    int m_freeAgentCount = 0;
    double m_avgAssemblySize = 0.0;
    int m_maxAssemblySize = 0;
    std::map<std::string, int> m_assemblyCensus;

    // Union-find arrays
    std::vector<int> m_ufParent;
    std::vector<int> m_ufRank;
    int UFFind(int x);
    void UFUnion(int x, int y);

    // RNG
    std::mt19937 m_rng;
};

} // namespace DABM
