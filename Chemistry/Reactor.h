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

namespace Chemistry {

// Ensures chemistry time series are registered (called during initialization)
void EnsureTimeSeriesRegistration();

// The Reactor is the simulation container that owns all atoms, bonds, and molecules.
// Analogous to Region::Region in the economic model.
//
// It drives the simulation loop: atom movement, spatial hashing, bond formation/breaking,
// molecule tracking via union-find, and statistics collection.
class Reactor {
public:
    Reactor(const std::string& name = "DefaultReactor");
    ~Reactor();

    // Initialization: creates atoms according to config, sets up spatial grid
    bool Initialize();

    // Run one simulation timestep (replaces Region::RunMonthlyActivity)
    void RunTimestep(int step);

    // Cleanup all entities
    void Cleanup();

    // Snapshot persistence: serialize/deserialize full reactor state to/from JSON
    nlohmann::json SaveSnapshot() const;
    bool LoadSnapshot(const nlohmann::json& snapshot);

    // --- Statistics getters (called by Simulator/DataManager after each step) ---

    double GetTotalBondEnergy() const { return m_totalBondEnergy; }
    int GetMoleculeCount() const { return m_moleculeCount; }
    int GetFreeAtomCount() const { return m_freeAtomCount; }
    int GetBondCount() const { return static_cast<int>(m_bonds.size()); }
    double GetAverageMoleculeSize() const { return m_avgMoleculeSize; }
    int GetMaxMoleculeSize() const { return m_maxMoleculeSize; }
    int GetFormationEventsThisStep() const { return m_formationEventsThisStep; }
    int GetBreakingEventsThisStep() const { return m_breakingEventsThisStep; }

    int GetFreeAtomCountByElement(Element e) const;
    int GetMoleculeCountByFormula(const std::string& formula) const;
    const std::map<std::string, int>& GetMoleculeCensus() const { return m_moleculeCensus; }

    // --- Entity access ---

    size_t GetAtomCount() const { return m_atoms.size(); }
    const Atom* GetAtomByIndex(size_t index) const;

    // Current simulation step
    int GetCurrentStep() const { return m_currentStep; }

private:
    // --- Timestep sub-operations ---

    void MoveAtoms(double dt);
    void ApplyBoundaryConditions();
    void AttemptBondBreaking();
    void AttemptBondFormation();
    void UpdateMolecules();   // Union-find connected components
    void CalculateStats();

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

    // Spatial acceleration
    SpatialGrid m_spatialGrid;

    // Simulation parameters (read from DataManager config at Initialize)
    Vec3 m_boxSize;
    double m_temperature = 1000.0;
    double m_dt = 1.0;
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

    // Union-find arrays (reused each step to avoid allocation)
    std::vector<int> m_ufParent;
    std::vector<int> m_ufRank;

    int UFFind(int x);
    void UFUnion(int x, int y);
};

} // namespace Chemistry
