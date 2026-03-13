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
    m_daemonSpawnedThisStep = 0;
    m_daemonSuccessThisStep = 0;
    m_daemonDeathsThisStep = 0;

    const auto& config = DataManager::GetConfigParameters();

    // 1. Move atoms (Brownian motion)
    MoveAtoms(m_dt);

    // 2. Move daemons (if enabled)
    if (config.enableDaemons) {
        MoveDaemons(m_dt);
        ApplyDaemonBoundaryConditions();
    }

    // 3. Apply boundary conditions for atoms
    ApplyBoundaryConditions();

    // 4. Rebuild spatial grid
    m_spatialGrid.Build(m_atoms);

    // 5. Attempt bond breaking (before formation, so freed valence is available)
    AttemptBondBreaking();

    // 6. Attempt stochastic bond formation (if enabled)
    if (config.enableStochasticBonds) {
        AttemptBondFormation();
    }

    // 7. Run Darwinian daemons (if enabled)
    if (config.enableDaemons) {
        RunDaemons();
        SpawnNewDaemons();
        CullDeadDaemons();
    }

    // 8. Update molecule tracking (union-find connected components)
    UpdateMolecules();

    // 9. Collect statistics
    CalculateStats();
}

void Reactor::Cleanup() {
    // Clear molecules first (they have non-owning pointers to atoms/bonds)
    m_molecules.clear();
    // Clear bonds (they have non-owning pointers to atoms)
    m_bonds.clear();
    // Clear atoms
    m_atoms.clear();

    // Clear daemons
    m_daemons.clear();

    m_initialized = false;
    m_currentStep = 0;
    m_nextBondId = 0;
    m_nextMoleculeId = 0;
    m_nextDaemonId = 0;
    m_totalBondEnergy = 0.0;
    m_moleculeCount = 0;
    m_freeAtomCount = 0;
    m_avgMoleculeSize = 0.0;
    m_maxMoleculeSize = 0;
    m_moleculeCensus.clear();
}

// --- Daemon lifecycle ---

void Reactor::MoveDaemons(double dt) {
    const auto& config = DataManager::GetConfigParameters();
    auto& rng = DataManager::GetGlobalRandomEngine();
    std::normal_distribution<double> noise(0.0, 1.0);

    double speedMul = config.daemonSpeed;
    for (auto& daemon : m_daemons) {
        if (daemon->GetState() == Daemon::State::DEAD) continue;

        // Brownian motion with speed multiplier
        double mass = daemon->GetMass();
        double sigma = std::sqrt(ChemistryConfig::kB * m_temperature / mass) * speedMul;
        double dampingFactor = 0.9;

        Vec3 vel = daemon->GetVelocity() * dampingFactor;
        vel.x += noise(rng) * sigma * 0.1;
        vel.y += noise(rng) * sigma * 0.1;
        vel.z += noise(rng) * sigma * 0.1;
        daemon->SetVelocity(vel);
        daemon->StepActivity(dt);
    }
}

void Reactor::ApplyDaemonBoundaryConditions() {
    for (auto& daemon : m_daemons) {
        Vec3 pos = daemon->GetPosition();
        Vec3 vel = daemon->GetVelocity();

        // Reflective boundaries (same as atoms)
        auto reflect = [](double& p, double& v, double lo, double hi) {
            if (p < lo) { p = lo + (lo - p); v = std::abs(v); }
            if (p > hi) { p = hi - (p - hi); v = -std::abs(v); }
            p = (p < lo) ? lo : (p > hi ? hi : p);
        };

        reflect(pos.x, vel.x, 0.0, m_boxSize.x);
        reflect(pos.y, vel.y, 0.0, m_boxSize.y);
        reflect(pos.z, vel.z, 0.0, m_boxSize.z);

        daemon->SetPosition(pos);
        daemon->SetVelocity(vel);
    }
}

void Reactor::RunDaemons() {
    const auto& config = DataManager::GetConfigParameters();
    double cutoff = config.interactionCutoff;
    std::vector<std::unique_ptr<Daemon>> offspring;

    for (auto& daemon : m_daemons) {
        if (daemon->GetState() != Daemon::State::SEARCHING) continue;

        Atom* atomA = nullptr;
        Atom* atomB = nullptr;
        if (daemon->TryAssemble(m_spatialGrid, m_atoms, cutoff, m_currentStep, atomA, atomB)) {
            // Form bond between the two atoms
            auto& chemConfig = ChemistryConfig::GetInstance();
            double energy = chemConfig.GetBondEnergy(atomA->GetElement(), atomB->GetElement(), BondOrder::Single);
            CreateBond(atomA, atomB, BondOrder::Single, m_currentStep);
            m_formationEventsThisStep++;
            m_daemonSuccessThisStep++;

            daemon->RecordSuccess(m_currentStep);

            // Reproduction: spawn offspring nearby (assisted production)
            if (static_cast<int>(m_daemons.size() + offspring.size()) < config.daemonMaxPopulation) {
                auto& rng = DataManager::GetGlobalRandomEngine();
                auto child = daemon->SpawnOffspring(m_nextDaemonId++, m_currentStep, m_boxSize, rng);
                offspring.push_back(std::move(child));
            }
        }
    }

    // Add offspring to the daemon population
    for (auto& child : offspring) {
        m_daemons.push_back(std::move(child));
        m_daemonSpawnedThisStep++;
    }
}

void Reactor::SpawnNewDaemons() {
    const auto& config = DataManager::GetConfigParameters();
    auto& rng = DataManager::GetGlobalRandomEngine();

    // Spawn daemonSpawnRate new daemons per step (Poisson-like: use floor + fractional chance)
    double rate = config.daemonSpawnRate;
    int toSpawn = static_cast<int>(rate);
    double fractional = rate - toSpawn;
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    if (prob(rng) < fractional) toSpawn++;

    // Respect population cap
    int available = config.daemonMaxPopulation - static_cast<int>(m_daemons.size());
    toSpawn = (toSpawn < available) ? toSpawn : ((available > 0) ? available : 0);

    auto& recipeBook = RecipeBook::GetInstance();
    if (recipeBook.GetTotalRecipeCount() == 0) return;

    std::uniform_real_distribution<double> posDistX(0.0, m_boxSize.x);
    std::uniform_real_distribution<double> posDistY(0.0, m_boxSize.y);
    std::uniform_real_distribution<double> posDistZ(0.0, m_boxSize.z);

    for (int i = 0; i < toSpawn; ++i) {
        const Recipe& recipe = recipeBook.GetRandomRecipe(rng);
        auto daemon = std::make_unique<Daemon>(m_nextDaemonId++, &recipe, m_currentStep, config.daemonTimeout);

        // Random position in box
        daemon->SetPosition(Vec3(posDistX(rng), posDistY(rng), posDistZ(rng)));

        // Thermal velocity
        double sigma = std::sqrt(ChemistryConfig::kB * m_temperature / daemon->GetMass());
        std::normal_distribution<double> velDist(0.0, sigma);
        daemon->SetVelocity(Vec3(velDist(rng), velDist(rng), velDist(rng)));

        m_daemons.push_back(std::move(daemon));
        m_daemonSpawnedThisStep++;
    }
}

void Reactor::CullDeadDaemons() {
    int deaths = 0;
    auto it = std::remove_if(m_daemons.begin(), m_daemons.end(),
        [&](const std::unique_ptr<Daemon>& d) {
            if (d->GetState() == Daemon::State::DEAD || d->ShouldDie(m_currentStep)) {
                deaths++;
                return true;
            }
            return false;
        });
    m_daemons.erase(it, m_daemons.end());
    m_daemonDeathsThisStep = deaths;
}

// --- Snapshot persistence ---

nlohmann::json Reactor::SaveSnapshot() const {
    nlohmann::json snap;
    snap["currentStep"] = m_currentStep;
    snap["nextBondId"] = m_nextBondId;
    snap["nextMoleculeId"] = m_nextMoleculeId;
    snap["nextDaemonId"] = m_nextDaemonId;

    // Serialize atoms
    auto& atomsArr = snap["atoms"];
    atomsArr = nlohmann::json::array();
    for (const auto& atom : m_atoms) {
        nlohmann::json a;
        a["id"] = atom->GetID();
        a["element"] = static_cast<int>(atom->GetElement());
        const Vec3& p = atom->GetPosition();
        const Vec3& v = atom->GetVelocity();
        a["px"] = p.x; a["py"] = p.y; a["pz"] = p.z;
        a["vx"] = v.x; a["vy"] = v.y; a["vz"] = v.z;
        atomsArr.push_back(a);
    }

    // Serialize bonds
    auto& bondsArr = snap["bonds"];
    bondsArr = nlohmann::json::array();
    for (const auto& bond : m_bonds) {
        nlohmann::json b;
        b["id"] = bond->id;
        b["atom1"] = bond->atom1->GetID();
        b["atom2"] = bond->atom2->GetID();
        b["order"] = static_cast<int>(bond->order);
        b["energy"] = bond->energy;
        b["formationStep"] = bond->formationStep;
        bondsArr.push_back(b);
    }

    // Serialize daemons
    auto& daemonsArr = snap["daemons"];
    daemonsArr = nlohmann::json::array();
    for (const auto& daemon : m_daemons) {
        daemonsArr.push_back(daemon->Save());
    }

    return snap;
}

bool Reactor::LoadSnapshot(const nlohmann::json& snapshot) {
    Cleanup();

    // Read config parameters (same as Initialize, but without creating random atoms)
    const auto& config = DataManager::GetConfigParameters();
    m_boxSize = { config.boxSizeX, config.boxSizeY, config.boxSizeZ };
    m_temperature = config.temperature;
    m_dt = config.dt;

    auto& chemConfig = ChemistryConfig::GetInstance();
    chemConfig.A_form = config.A_form;
    chemConfig.A_break = config.A_break;
    chemConfig.activationFraction = config.activationFraction;

    m_spatialGrid.Configure(m_boxSize, config.interactionCutoff);

    // Restore step counters
    m_currentStep = snapshot["currentStep"].get<int>();
    m_nextBondId = snapshot["nextBondId"].get<int>();
    m_nextMoleculeId = snapshot["nextMoleculeId"].get<int>();
    if (snapshot.contains("nextDaemonId")) {
        m_nextDaemonId = snapshot["nextDaemonId"].get<int>();
    }

    // Restore atoms
    for (const auto& atomData : snapshot["atoms"]) {
        int id = atomData["id"].get<int>();
        Element elem = static_cast<Element>(atomData["element"].get<int>());
        auto atom = std::make_unique<Atom>(id, elem);
        atom->SetPosition({ atomData["px"].get<double>(),
                            atomData["py"].get<double>(),
                            atomData["pz"].get<double>() });
        atom->SetVelocity({ atomData["vx"].get<double>(),
                            atomData["vy"].get<double>(),
                            atomData["vz"].get<double>() });
        m_atoms.push_back(std::move(atom));
    }

    // Restore bonds (atoms must exist first; atoms are indexed by ID = vector index)
    for (const auto& bondData : snapshot["bonds"]) {
        auto bond = std::make_unique<Bond>();
        bond->id = bondData["id"].get<int>();
        bond->atom1 = m_atoms[bondData["atom1"].get<int>()].get();
        bond->atom2 = m_atoms[bondData["atom2"].get<int>()].get();
        bond->order = static_cast<BondOrder>(bondData["order"].get<int>());
        bond->energy = bondData["energy"].get<double>();
        bond->formationStep = bondData["formationStep"].get<int>();
        bond->markedForRemoval = false;

        int orderVal = static_cast<int>(bond->order);
        bond->atom1->AddBond(bond.get(), orderVal);
        bond->atom2->AddBond(bond.get(), orderVal);

        m_bonds.push_back(std::move(bond));
    }

    // Pre-allocate union-find arrays
    m_ufParent.resize(m_atoms.size());
    m_ufRank.resize(m_atoms.size());

    // Restore daemons
    if (snapshot.contains("daemons") && snapshot["daemons"].is_array()) {
        auto& recipeBook = RecipeBook::GetInstance();
        for (const auto& dData : snapshot["daemons"]) {
            std::string target = dData["targetFormula"].get<std::string>();
            std::string compA = dData["componentA"].get<std::string>();
            std::string compB = dData["componentB"].get<std::string>();
            double energyGain = dData["energyGain"].get<double>();

            // Find matching recipe in book
            const Recipe* recipe = nullptr;
            const auto& recipes = recipeBook.GetRecipesFor(target);
            for (const auto& r : recipes) {
                if (r.componentA == compA && r.componentB == compB) {
                    recipe = &r;
                    break;
                }
            }
            if (!recipe) continue;  // Recipe no longer exists

            int id = dData["id"].get<int>();
            int creationStep = dData["creationStep"].get<int>();
            int timeoutSteps = dData["timeoutSteps"].get<int>();

            auto daemon = std::make_unique<Daemon>(id, recipe, creationStep, timeoutSteps);
            daemon->SetPosition({ dData["px"].get<double>(),
                                   dData["py"].get<double>(),
                                   dData["pz"].get<double>() });
            daemon->SetVelocity({ dData["vx"].get<double>(),
                                   dData["vy"].get<double>(),
                                   dData["vz"].get<double>() });

            // Restore success tracking
            int successCount = dData.value("successCount", 0);
            int lastSuccessStep = dData.value("lastSuccessStep", creationStep);
            for (int s = 0; s < successCount; ++s) daemon->RecordSuccess(lastSuccessStep);

            m_daemons.push_back(std::move(daemon));
        }
    }

    // Rebuild molecules and stats from restored bonds
    UpdateMolecules();
    CalculateStats();

    m_initialized = true;

    Logger::Info("Reactor: Loaded snapshot at step " + std::to_string(m_currentStep)
        + " with " + std::to_string(m_atoms.size()) + " atoms, "
        + std::to_string(m_bonds.size()) + " bonds, and "
        + std::to_string(m_daemons.size()) + " daemons.");

    return true;
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
