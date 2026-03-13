#include "Reactor.h"
#include "ParticleFactory.h"
#include "../DataManager/DataManager.h"
#include "../Logger/Logger.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Chemistry {

Reactor::Reactor(const std::string& name) : m_name(name) {}

Reactor::~Reactor() {
    Cleanup();
}

bool Reactor::Initialize() {
    Cleanup();

    const auto& config = DataManager::GetConfigParameters();
    m_boxSize = { config.boxSizeX, config.boxSizeY, config.boxSizeZ };
    m_temperature = config.temperature;
    m_dt = config.dt;

    // Configure chemistry kinetic parameters
    auto& chemConfig = ChemistryConfig::GetInstance();
    chemConfig.A_form = config.A_form;
    chemConfig.A_break = config.A_break;
    chemConfig.activationFraction = config.activationFraction;

    // Configure spatial grid
    m_spatialGrid.Configure(m_boxSize, config.interactionCutoff);

    // Create atoms
    auto& rng = DataManager::GetGlobalRandomEngine();
    int id = 0;

    auto createAtoms = [&](Element elem, int count) {
        for (int i = 0; i < count; ++i) {
            auto atom = ParticleFactory::CreateAtom(id++, elem, m_boxSize, m_temperature, rng);
            m_atoms.push_back(std::move(atom));
        }
    };

    createAtoms(Element::C, config.numCarbon);
    createAtoms(Element::H, config.numHydrogen);
    createAtoms(Element::O, config.numOxygen);

    Logger::Info("Reactor: Initialized with " + std::to_string(m_atoms.size()) + " atoms ("
        + std::to_string(config.numCarbon) + "C + "
        + std::to_string(config.numHydrogen) + "H + "
        + std::to_string(config.numOxygen) + "O) in box "
        + std::to_string(m_boxSize.x) + "x" + std::to_string(m_boxSize.y) + "x" + std::to_string(m_boxSize.z));

    // Pre-allocate union-find arrays
    m_ufParent.resize(m_atoms.size());
    m_ufRank.resize(m_atoms.size());

    m_initialized = true;
    m_currentStep = 0;
    return true;
}

void Reactor::RunTimestep(int step) {
    m_currentStep = step;
    m_formationEventsThisStep = 0;
    m_breakingEventsThisStep = 0;

    // 1. Move atoms (Brownian motion)
    MoveAtoms(m_dt);

    // 2. Apply boundary conditions
    ApplyBoundaryConditions();

    // 3. Rebuild spatial grid
    m_spatialGrid.Build(m_atoms);

    // 4. Attempt bond breaking (before formation, so freed valence is available)
    AttemptBondBreaking();

    // 5. Attempt bond formation
    AttemptBondFormation();

    // 6. Update molecule tracking (union-find connected components)
    UpdateMolecules();

    // 7. Collect statistics
    CalculateStats();
}

void Reactor::Cleanup() {
    // Clear molecules first (they have non-owning pointers to atoms/bonds)
    m_molecules.clear();
    // Clear bonds (they have non-owning pointers to atoms)
    m_bonds.clear();
    // Clear atoms
    m_atoms.clear();

    m_initialized = false;
    m_currentStep = 0;
    m_nextBondId = 0;
    m_nextMoleculeId = 0;
    m_totalBondEnergy = 0.0;
    m_moleculeCount = 0;
    m_freeAtomCount = 0;
    m_avgMoleculeSize = 0.0;
    m_maxMoleculeSize = 0;
    m_moleculeCensus.clear();
}

// --- Timestep sub-operations ---

void Reactor::MoveAtoms(double dt) {
    auto& rng = DataManager::GetGlobalRandomEngine();
    std::normal_distribution<double> noise(0.0, 1.0);

    for (auto& atom : m_atoms) {
        // Add random thermal kick (Brownian dynamics)
        // Force ~ sqrt(2 * kB * T * gamma / dt) * noise
        // Simplified: velocity gets a random perturbation scaled by temperature
        double mass = atom->GetMass();
        double sigma = std::sqrt(ChemistryConfig::kB * m_temperature / mass);
        double dampingFactor = 0.9; // Velocity damping to prevent runaway speeds

        Vec3 vel = atom->GetVelocity();
        vel = vel * dampingFactor;
        vel += Vec3{ noise(rng) * sigma * 0.1,
                     noise(rng) * sigma * 0.1,
                     noise(rng) * sigma * 0.1 };
        atom->SetVelocity(vel);

        // Integrate position
        atom->StepActivity(dt);
    }
}

void Reactor::ApplyBoundaryConditions() {
    // Reflective boundary conditions: bounce atoms off box walls
    for (auto& atom : m_atoms) {
        Vec3 pos = atom->GetPosition();
        Vec3 vel = atom->GetVelocity();

        auto reflect = [](double& p, double& v, double lo, double hi) {
            if (p < lo) { p = lo + (lo - p); v = -v; }
            if (p > hi) { p = hi - (p - hi); v = -v; }
            // Clamp as safety net
            p = std::clamp(p, lo, hi);
        };

        reflect(pos.x, vel.x, 0.0, m_boxSize.x);
        reflect(pos.y, vel.y, 0.0, m_boxSize.y);
        reflect(pos.z, vel.z, 0.0, m_boxSize.z);

        atom->SetPosition(pos);
        atom->SetVelocity(vel);
    }
}

void Reactor::AttemptBondBreaking() {
    auto& rng = DataManager::GetGlobalRandomEngine();
    auto& chemConfig = ChemistryConfig::GetInstance();
    std::uniform_real_distribution<double> uniform01(0.0, 1.0);

    // Mark bonds for removal (can't remove while iterating)
    for (auto& bond : m_bonds) {
        double P_break = chemConfig.GetBreakingProbability(bond->energy, m_temperature);
        if (uniform01(rng) < P_break) {
            bond->markedForRemoval = true;
            m_breakingEventsThisStep++;
        }
    }

    // Remove marked bonds
    for (auto it = m_bonds.begin(); it != m_bonds.end(); ) {
        if ((*it)->markedForRemoval) {
            Bond* bond = it->get();
            int orderVal = static_cast<int>(bond->order);
            bond->atom1->RemoveBond(bond, orderVal);
            bond->atom2->RemoveBond(bond, orderVal);
            it = m_bonds.erase(it);
        }
        else {
            ++it;
        }
    }
}

void Reactor::AttemptBondFormation() {
    auto& rng = DataManager::GetGlobalRandomEngine();
    auto& chemConfig = ChemistryConfig::GetInstance();
    std::uniform_real_distribution<double> uniform01(0.0, 1.0);

    // Get all neighbor pairs within cutoff
    auto pairs = m_spatialGrid.GetNeighborPairs();

    for (const auto& [idx1, idx2] : pairs) {
        Atom* a1 = m_atoms[idx1].get();
        Atom* a2 = m_atoms[idx2].get();

        // Both atoms must have free valence
        if (!a1->CanBond() || !a2->CanBond()) continue;

        // Check if they're already bonded
        if (a1->GetBondTo(a2) != nullptr) continue;

        // Attempt single bond formation
        Element e1 = a1->GetElement();
        Element e2 = a2->GetElement();
        double bondEnergy = chemConfig.GetBondEnergy(e1, e2, BondOrder::Single);
        if (bondEnergy <= 0.0) continue;  // Bond type not defined

        double P_form = chemConfig.GetFormationProbability(e1, e2, BondOrder::Single, m_temperature);
        if (uniform01(rng) < P_form) {
            CreateBond(a1, a2, BondOrder::Single, m_currentStep);
            m_formationEventsThisStep++;
        }
    }
}

void Reactor::UpdateMolecules() {
    size_t N = m_atoms.size();

    // Initialize union-find: each atom is its own component
    for (size_t i = 0; i < N; ++i) {
        m_ufParent[i] = static_cast<int>(i);
        m_ufRank[i] = 0;
    }

    // Union bonded atoms
    for (const auto& bond : m_bonds) {
        int i1 = bond->atom1->GetID();
        int i2 = bond->atom2->GetID();
        UFUnion(i1, i2);
    }

    // Clear old molecule assignments
    for (auto& atom : m_atoms) {
        atom->SetMolecule(nullptr);
    }
    m_molecules.clear();

    // Group atoms by their root in the union-find
    std::unordered_map<int, std::vector<int>> components;
    for (size_t i = 0; i < N; ++i) {
        int root = UFFind(static_cast<int>(i));
        // Only create molecules for atoms that have at least one bond
        if (!m_atoms[i]->GetBonds().empty()) {
            components[root].push_back(static_cast<int>(i));
        }
    }

    // Create Molecule objects for each connected component (size >= 2)
    for (auto& [root, atomIndices] : components) {
        if (atomIndices.size() < 2) continue;

        auto mol = CreateMolecule(m_currentStep);
        for (int idx : atomIndices) {
            mol->AddAtom(m_atoms[idx].get());
        }

        // Add bonds that belong to this molecule
        for (const auto& bond : m_bonds) {
            if (UFFind(bond->atom1->GetID()) == UFFind(root)) {
                mol->AddBond(bond.get());
            }
        }
    }
}

void Reactor::CalculateStats() {
    // Total bond energy
    m_totalBondEnergy = 0.0;
    for (const auto& bond : m_bonds) {
        m_totalBondEnergy += bond->energy;
    }

    // Molecule statistics
    m_moleculeCount = static_cast<int>(m_molecules.size());
    m_maxMoleculeSize = 0;
    double totalMolSize = 0.0;

    m_moleculeCensus.clear();
    for (const auto& mol : m_molecules) {
        int size = static_cast<int>(mol->GetAtomCount());
        if (size > m_maxMoleculeSize) m_maxMoleculeSize = size;
        totalMolSize += size;

        std::string formula = mol->GetMolecularFormula();
        m_moleculeCensus[formula]++;
    }
    m_avgMoleculeSize = (m_moleculeCount > 0) ? (totalMolSize / m_moleculeCount) : 0.0;

    // Free atom count (atoms not in any molecule)
    m_freeAtomCount = 0;
    for (const auto& atom : m_atoms) {
        if (atom->IsFree()) m_freeAtomCount++;
    }
}

// --- Bond management helpers ---

Bond* Reactor::CreateBond(Atom* a1, Atom* a2, BondOrder order, int step) {
    auto& chemConfig = ChemistryConfig::GetInstance();
    double energy = chemConfig.GetBondEnergy(a1->GetElement(), a2->GetElement(), order);

    auto bond = std::make_unique<Bond>();
    bond->id = m_nextBondId++;
    bond->atom1 = a1;
    bond->atom2 = a2;
    bond->order = order;
    bond->energy = energy;
    bond->formationStep = step;

    int orderVal = static_cast<int>(order);
    a1->AddBond(bond.get(), orderVal);
    a2->AddBond(bond.get(), orderVal);

    Bond* ptr = bond.get();
    m_bonds.push_back(std::move(bond));
    return ptr;
}

void Reactor::RemoveBond(Bond* bond) {
    int orderVal = static_cast<int>(bond->order);
    bond->atom1->RemoveBond(bond, orderVal);
    bond->atom2->RemoveBond(bond, orderVal);

    auto it = std::find_if(m_bonds.begin(), m_bonds.end(),
        [bond](const std::unique_ptr<Bond>& b) { return b.get() == bond; });
    if (it != m_bonds.end()) {
        m_bonds.erase(it);
    }
}

Molecule* Reactor::CreateMolecule(int step) {
    auto mol = std::make_unique<Molecule>(m_nextMoleculeId++);
    mol->SetFormationStep(step);
    Molecule* ptr = mol.get();
    m_molecules.push_back(std::move(mol));
    return ptr;
}

// --- Union-Find ---

int Reactor::UFFind(int x) {
    while (m_ufParent[x] != x) {
        m_ufParent[x] = m_ufParent[m_ufParent[x]]; // Path compression
        x = m_ufParent[x];
    }
    return x;
}

void Reactor::UFUnion(int x, int y) {
    int rx = UFFind(x);
    int ry = UFFind(y);
    if (rx == ry) return;

    // Union by rank
    if (m_ufRank[rx] < m_ufRank[ry]) std::swap(rx, ry);
    m_ufParent[ry] = rx;
    if (m_ufRank[rx] == m_ufRank[ry]) m_ufRank[rx]++;
}

// --- Statistics getters ---

const Atom* Reactor::GetAtomByIndex(size_t index) const {
    if (index < m_atoms.size()) return m_atoms[index].get();
    return nullptr;
}

int Reactor::GetFreeAtomCountByElement(Element e) const {
    int count = 0;
    for (const auto& atom : m_atoms) {
        if (atom->GetElement() == e && atom->IsFree()) count++;
    }
    return count;
}

int Reactor::GetMoleculeCountByFormula(const std::string& formula) const {
    auto it = m_moleculeCensus.find(formula);
    return (it != m_moleculeCensus.end()) ? it->second : 0;
}

} // namespace Chemistry
