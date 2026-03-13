#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <random>
#include <nlohmann/json.hpp>
#include "Vec3.h"
#include "Atom.h"
#include "Bond.h"
#include "Molecule.h"
#include "SpatialGrid.h"
#include "ChemistryConfig.h"
#include "Daemon.h"

namespace Chemistry {

// Ensures chemistry time series are registered (called during initialization)
void EnsureTimeSeriesRegistration();

class Reactor {
public:
    Reactor(const std::string& name = "DefaultReactor");
    ~Reactor();

    bool Initialize();
    void RunTimestep(int step);
    void Cleanup();

    nlohmann::json SaveSnapshot() const;
    bool LoadSnapshot(const nlohmann::json& snapshot);

    // --- Statistics getters ---

    double GetTotalBondEnergy() const { return m_totalBondEnergy; }
    int GetMoleculeCount() const { return m_moleculeCount; }
    int GetFreeAtomCount() const { return m_freeAtomCount; }
    int GetBondCount() const { return static_cast<int>(m_bonds.size()); }
    double GetAverageMoleculeSize() const { return m_avgMoleculeSize; }
    int GetMaxMoleculeSize() const { return m_maxMoleculeSize; }
    int GetFormationEventsThisStep() const { return m_formationEventsThisStep; }
    int GetBreakingEventsThisStep() const { return m_breakingEventsThisStep; }

    // Daemon statistics (riding daemons)
    int GetActiveDaemonCount() const;
    int GetSearchingDaemonCount() const;
    int GetHoldingDaemonCount() const;
    int GetDaemonSpawnedThisStep() const { return m_daemonSpawnedThisStep; }
    int GetDaemonSuccessThisStep() const { return m_daemonSuccessThisStep; }
    int GetDaemonDeathsThisStep() const { return m_daemonDeathsThisStep; }
    int GetRearrangementsThisStep() const { return m_rearrangementsThisStep; }

    int GetFreeAtomCountByElement(Element e) const;
    int GetMoleculeCountByFormula(const std::string& formula) const;
    const std::map<std::string, int>& GetMoleculeCensus() const { return m_moleculeCensus; }

    size_t GetAtomCount() const { return m_atoms.size(); }
    const Atom* GetAtomByIndex(size_t index) const;
    int GetCurrentStep() const { return m_currentStep; }

private:
    // --- Timestep sub-operations ---

    void MoveAtoms(double dt);
    void ApplyBoundaryConditions();
    void AttemptBondBreaking();
    void AttemptBondFormation();
    void ResupplyAtoms();
    void UpdateMolecules();
    void CalculateStats();

    // --- Riding daemon lifecycle ---
    void AssignInitialDaemons();
    void RunAtomDaemons();
    void HandleSearchingDaemon(Atom* atom, size_t atomIndex, DaemonState* ds);
    void HandleHoldingDaemon(Atom* atom, DaemonState* ds);
    void ReassignDaemon(Atom* atom);
    void SpawnOffspringDaemon(Atom* neighbor, const Recipe* parentRecipe);

    // Thermodynamic rearrangement: break weakest bond on a saturated atom
    // to make room for a new, stronger bond. Returns the freed atom, or nullptr.
    Atom* MakeRoomForBond(Atom* anchor, double newBondEnergy);

    // --- Bond management helpers ---

    Bond* CreateBond(Atom* a1, Atom* a2, BondOrder order, int step);
    void RemoveBond(Bond* bond);

    // --- Molecule management helpers ---

    Molecule* CreateMolecule(int step);

    // --- Data ---

    std::string m_name;
    bool m_initialized = false;
    int m_currentStep = 0;

    // Owned entities
    std::vector<std::unique_ptr<Atom>> m_atoms;
    std::vector<std::unique_ptr<Bond>> m_bonds;
    std::vector<std::unique_ptr<Molecule>> m_molecules;

    // Daemon per-step counters
    int m_daemonSpawnedThisStep = 0;
    int m_daemonSuccessThisStep = 0;
    int m_daemonDeathsThisStep = 0;
    int m_rearrangementsThisStep = 0;

    // Spatial acceleration
    SpatialGrid m_spatialGrid;

    // Simulation parameters
    Vec3 m_boxSize;
    double m_temperature = 1000.0;
    double m_dt = 1.0;
    int m_initialCarbon = 0;
    int m_initialHydrogen = 0;
    int m_initialOxygen = 0;
    int m_nextBondId = 0;
    int m_nextMoleculeId = 0;

    // Per-step statistics
    double m_totalBondEnergy = 0.0;
    int m_moleculeCount = 0;
    int m_freeAtomCount = 0;
    double m_avgMoleculeSize = 0.0;
    int m_maxMoleculeSize = 0;
    int m_formationEventsThisStep = 0;
    int m_breakingEventsThisStep = 0;
    std::map<std::string, int> m_moleculeCensus;

    // Union-find arrays
    std::vector<int> m_ufParent;
    std::vector<int> m_ufRank;

    int UFFind(int x);
    void UFUnion(int x, int y);
};

} // namespace Chemistry
