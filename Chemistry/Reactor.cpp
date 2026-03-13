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

    auto& chemConfig = ChemistryConfig::GetInstance();
    chemConfig.A_form = config.A_form;
    chemConfig.A_break = config.A_break;
    chemConfig.activationFraction = config.activationFraction;

    m_spatialGrid.Configure(m_boxSize, config.interactionCutoff);

    auto& rng = DataManager::GetGlobalRandomEngine();
    int id = 0;

    auto createAtoms = [&](Element elem, int count) {
        for (int i = 0; i < count; ++i) {
            auto atom = ParticleFactory::CreateAtom(id++, elem, m_boxSize, m_temperature, rng);
            m_atoms.push_back(std::move(atom));
        }
    };

    m_initialCarbon = config.numCarbon;
    m_initialHydrogen = config.numHydrogen;
    m_initialOxygen = config.numOxygen;

    createAtoms(Element::C, config.numCarbon);
    createAtoms(Element::H, config.numHydrogen);
    createAtoms(Element::O, config.numOxygen);

    Logger::Info("Reactor: Initialized with " + std::to_string(m_atoms.size()) + " atoms ("
        + std::to_string(config.numCarbon) + "C + "
        + std::to_string(config.numHydrogen) + "H + "
        + std::to_string(config.numOxygen) + "O) in box "
        + std::to_string(m_boxSize.x) + "x" + std::to_string(m_boxSize.y) + "x" + std::to_string(m_boxSize.z));

    m_ufParent.resize(m_atoms.size());
    m_ufRank.resize(m_atoms.size());

    // Assign riding daemons to all atoms if enabled
    if (config.enableDaemons) {
        AssignInitialDaemons();
    }

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
    m_rearrangementsThisStep = 0;

    const auto& config = DataManager::GetConfigParameters();

    // 1. Move atoms (Brownian motion)
    MoveAtoms(m_dt);

    // 2. Apply boundary conditions
    ApplyBoundaryConditions();

    // 3. Rebuild spatial grid
    m_spatialGrid.Build(m_atoms);

    // 4. Attempt bond breaking (stochastic)
    AttemptBondBreaking();

    // 5. Attempt stochastic bond formation (if enabled)
    if (config.enableStochasticBonds) {
        AttemptBondFormation();
    }

    // 6. Run riding daemons (if enabled)
    if (config.enableDaemons) {
        RunAtomDaemons();
    }

    // 7. Update molecule tracking (union-find connected components)
    UpdateMolecules();

    // 8. Resupply fresh atoms if enabled
    if (config.atomResupplyInterval > 0 && step > 0 && step % config.atomResupplyInterval == 0) {
        ResupplyAtoms();
    }

    // 9. Collect statistics
    CalculateStats();
}

void Reactor::Cleanup() {
    m_molecules.clear();
    m_bonds.clear();
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

// --- Riding daemon lifecycle ---

void Reactor::AssignInitialDaemons() {
    const auto& config = DataManager::GetConfigParameters();
    auto& rng = DataManager::GetGlobalRandomEngine();
    auto& recipeBook = RecipeBook::GetInstance();

    if (recipeBook.GetTotalRecipeCount() == 0) return;

    for (auto& atom : m_atoms) {
        if (!atom->HasDaemon()) {
            auto ds = std::make_unique<DaemonState>();
            ds->recipe = &recipeBook.GetRandomRecipe(rng);
            ds->assignedStep = m_currentStep;
            ds->timeoutSteps = config.daemonTimeout;
            atom->SetDaemon(std::move(ds));
        }
    }

    Logger::Info("Reactor: Assigned riding daemons to " + std::to_string(m_atoms.size()) + " atoms");
}

void Reactor::RunAtomDaemons() {
    for (size_t i = 0; i < m_atoms.size(); ++i) {
        Atom* atom = m_atoms[i].get();
        DaemonState* ds = atom->GetDaemon();
        if (!ds) continue;

        if (ds->IsSearching()) {
            HandleSearchingDaemon(atom, i, ds);
        } else {
            HandleHoldingDaemon(atom, ds);
        }
    }
}

Atom* Reactor::MakeRoomForBond(Atom* anchor, double /*newBondEnergy*/) {
    if (!anchor) return nullptr;

    // Thermodynamic rearrangement over sidereal time: break the weakest bond
    // in this entity to free valence for a new bond. Natural selection via
    // stochastic breaking and daemon timeout eliminates unfavorable products.
    Molecule* mol = anchor->GetMolecule();
    std::vector<Atom*> candidates;
    if (mol) {
        candidates = mol->GetAtoms();
    } else {
        candidates.push_back(anchor);
    }

    Bond* weakest = nullptr;
    Atom* bestAtom = nullptr;
    for (Atom* a : candidates) {
        for (Bond* b : a->GetBonds()) {
            if (b->markedForRemoval) continue;
            if (!weakest || b->energy < weakest->energy) {
                weakest = b;
                bestAtom = a;
            }
        }
    }

    if (!weakest || !bestAtom) return nullptr;

    // Clear daemon holder if this bond was held by a daemon
    if (weakest->daemonHolder) {
        DaemonState* holderDaemon = weakest->daemonHolder->GetDaemon();
        if (holderDaemon && holderDaemon->heldBond == weakest) {
            holderDaemon->heldBond = nullptr;
        }
        weakest->daemonHolder = nullptr;
    }

    // Break the weak bond to make room
    RemoveBond(weakest);
    m_breakingEventsThisStep++;
    m_rearrangementsThisStep++;

    // Return the atom that now has free valence
    return FindBondableAtomInEntity(bestAtom);
}

void Reactor::HandleSearchingDaemon(Atom* atom, size_t atomIndex, DaemonState* ds) {
    // Timeout while searching → reassign with new random recipe
    if (ds->HasTimedOut(m_currentStep)) {
        ReassignDaemon(atom);
        m_daemonDeathsThisStep++;
        return;
    }

    // Check if my entity formula still matches one of the recipe components
    std::string myFormula = GetEntityFormula(atom);
    bool iAmCompA = (myFormula == ds->recipe->componentA);
    bool iAmCompB = (myFormula == ds->recipe->componentB);

    if (!iAmCompA && !iAmCompB) {
        // Formula changed (stochastic bond event altered my molecule). Reassign.
        ReassignDaemon(atom);
        return;
    }

    const std::string& targetComp = iAmCompA ? ds->recipe->componentB : ds->recipe->componentA;

    // Search spatial neighbors
    auto neighbors = m_spatialGrid.GetNeighbors(static_cast<int>(atomIndex));

    for (int neighborIdx : neighbors) {
        Atom* neighbor = m_atoms[neighborIdx].get();

        // Must be different entity (not same molecule)
        if (!atom->IsFree() && !neighbor->IsFree() &&
            atom->GetMolecule() == neighbor->GetMolecule()) {
            continue;
        }

        std::string neighborFormula = GetEntityFormula(neighbor);
        if (neighborFormula != targetComp) continue;

        // Found a match! Try to bond.
        // First try free-valence path (fast, no rearrangement needed)
        Atom* bondableA = FindBondableAtomInEntity(atom);
        Atom* bondableB = FindBondableAtomInEntity(neighbor);

        // Thermodynamic rearrangement: if either side is saturated,
        // try breaking the weakest bond to make room for the new (stronger) one.
        double newBondEnergy = ds->recipe->energyGain;
        if (!bondableA) bondableA = MakeRoomForBond(atom, newBondEnergy);
        if (!bondableB) bondableB = MakeRoomForBond(neighbor, newBondEnergy);

        if (!bondableA || !bondableB) continue;
        if (bondableA->GetBondTo(bondableB)) continue;  // already bonded

        // Form the bond
        Bond* newBond = CreateBond(bondableA, bondableB, BondOrder::Single, m_currentStep);
        m_formationEventsThisStep++;
        m_daemonSuccessThisStep++;

        // Transition to HOLDING: daemon owns this bond
        ds->heldBond = newBond;
        ds->assignedStep = m_currentStep;  // reset timer for holding phase
        ds->successCount++;
        newBond->daemonHolder = atom;

        // Reproduction: spawn offspring daemon on the other side
        SpawnOffspringDaemon(neighbor, ds->recipe);
        m_daemonSpawnedThisStep++;

        break;  // one action per atom per step
    }
}

void Reactor::HandleHoldingDaemon(Atom* atom, DaemonState* ds) {
    // Check if held bond was already removed by stochastic breaking
    // (AttemptBondBreaking clears heldBond via daemonHolder back-pointer)
    if (!ds->heldBond) {
        // Bond was broken externally. Reassign.
        ReassignDaemon(atom);
        return;
    }

    // Timeout while holding → daemon dies → bond breaks → decomposition
    if (ds->HasTimedOut(m_currentStep)) {
        Bond* bond = ds->heldBond;
        Atom* otherAtom = bond->GetOther(atom);

        // If otherAtom is same as atom (shouldn't happen), find the actual other
        if (otherAtom == atom) {
            otherAtom = (bond->atom1 == atom) ? bond->atom2 : bond->atom1;
        }

        // Clear daemon's hold before removing bond
        ds->heldBond = nullptr;
        bond->daemonHolder = nullptr;

        // Break the bond
        RemoveBond(bond);
        m_breakingEventsThisStep++;
        m_daemonDeathsThisStep++;

        // Both sides get fresh daemons
        ReassignDaemon(atom);
        if (otherAtom && otherAtom->HasDaemon()) {
            ReassignDaemon(otherAtom);
        }
    }
}

void Reactor::ReassignDaemon(Atom* atom) {
    auto& rng = DataManager::GetGlobalRandomEngine();
    auto& recipeBook = RecipeBook::GetInstance();
    const auto& config = DataManager::GetConfigParameters();

    if (recipeBook.GetTotalRecipeCount() == 0) return;

    auto ds = std::make_unique<DaemonState>();
    ds->recipe = &recipeBook.GetRandomRecipe(rng);
    ds->assignedStep = m_currentStep;
    ds->timeoutSteps = config.daemonTimeout;
    atom->SetDaemon(std::move(ds));
}

void Reactor::SpawnOffspringDaemon(Atom* neighbor, const Recipe* parentRecipe) {
    if (!neighbor) return;

    // Only overwrite SEARCHING daemons, never HOLDING ones
    DaemonState* existingDs = neighbor->GetDaemon();
    if (existingDs && existingDs->IsHolding()) return;

    auto& rng = DataManager::GetGlobalRandomEngine();
    auto& recipeBook = RecipeBook::GetInstance();
    const auto& config = DataManager::GetConfigParameters();

    // Offspring gets a recipe that uses the parent's product as input
    const std::string& productFormula = parentRecipe->targetFormula;
    const auto& recipesUsingProduct = recipeBook.GetRecipesUsing(productFormula);

    const Recipe* offspringRecipe = nullptr;
    if (!recipesUsingProduct.empty()) {
        std::uniform_int_distribution<size_t> dist(0, recipesUsingProduct.size() - 1);
        offspringRecipe = recipesUsingProduct[dist(rng)];
    } else {
        offspringRecipe = &recipeBook.GetRandomRecipe(rng);
    }

    auto ds = std::make_unique<DaemonState>();
    ds->recipe = offspringRecipe;
    ds->assignedStep = m_currentStep;
    ds->timeoutSteps = config.daemonTimeout;
    neighbor->SetDaemon(std::move(ds));
}

// --- Daemon statistics ---

int Reactor::GetActiveDaemonCount() const {
    int count = 0;
    for (const auto& atom : m_atoms) {
        if (atom->HasDaemon()) count++;
    }
    return count;
}

int Reactor::GetSearchingDaemonCount() const {
    int count = 0;
    for (const auto& atom : m_atoms) {
        const DaemonState* ds = atom->GetDaemon();
        if (ds && ds->IsSearching()) count++;
    }
    return count;
}

int Reactor::GetHoldingDaemonCount() const {
    int count = 0;
    for (const auto& atom : m_atoms) {
        const DaemonState* ds = atom->GetDaemon();
        if (ds && ds->IsHolding()) count++;
    }
    return count;
}

// --- Snapshot persistence ---

nlohmann::json Reactor::SaveSnapshot() const {
    nlohmann::json snap;
    snap["currentStep"] = m_currentStep;
    snap["nextBondId"] = m_nextBondId;
    snap["nextMoleculeId"] = m_nextMoleculeId;

    // Serialize atoms (with inline daemon state)
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

        // Inline daemon state
        if (atom->HasDaemon()) {
            a["daemon"] = atom->GetDaemon()->Save();
        }

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

    return snap;
}

bool Reactor::LoadSnapshot(const nlohmann::json& snapshot) {
    Cleanup();

    const auto& config = DataManager::GetConfigParameters();
    m_boxSize = { config.boxSizeX, config.boxSizeY, config.boxSizeZ };
    m_temperature = config.temperature;
    m_dt = config.dt;

    auto& chemConfig = ChemistryConfig::GetInstance();
    chemConfig.A_form = config.A_form;
    chemConfig.A_break = config.A_break;
    chemConfig.activationFraction = config.activationFraction;

    m_spatialGrid.Configure(m_boxSize, config.interactionCutoff);

    m_currentStep = snapshot["currentStep"].get<int>();
    m_nextBondId = snapshot["nextBondId"].get<int>();
    m_nextMoleculeId = snapshot["nextMoleculeId"].get<int>();

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

    // Restore bonds
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

    m_ufParent.resize(m_atoms.size());
    m_ufRank.resize(m_atoms.size());

    // Restore per-atom daemon state
    auto& recipeBook = RecipeBook::GetInstance();
    const auto& atomsJson = snapshot["atoms"];
    for (size_t i = 0; i < m_atoms.size(); ++i) {
        if (atomsJson[i].contains("daemon")) {
            const auto& dj = atomsJson[i]["daemon"];
            std::string target = dj["targetFormula"].get<std::string>();
            std::string compA = dj["componentA"].get<std::string>();
            std::string compB = dj["componentB"].get<std::string>();

            // Find matching recipe
            const Recipe* recipe = nullptr;
            const auto& recipes = recipeBook.GetRecipesFor(target);
            for (const auto& r : recipes) {
                if (r.componentA == compA && r.componentB == compB) {
                    recipe = &r;
                    break;
                }
            }
            if (!recipe) {
                // Recipe not found; assign random
                recipe = &recipeBook.GetRandomRecipe(DataManager::GetGlobalRandomEngine());
            }

            auto ds = std::make_unique<DaemonState>();
            ds->recipe = recipe;
            ds->assignedStep = dj.value("assignedStep", m_currentStep);
            ds->timeoutSteps = dj.value("timeoutSteps", config.daemonTimeout);
            ds->successCount = dj.value("successCount", 0);

            // Relink heldBond
            int heldBondId = dj.value("heldBondId", -1);
            if (heldBondId >= 0) {
                for (auto& bond : m_bonds) {
                    if (bond->id == heldBondId) {
                        ds->heldBond = bond.get();
                        bond->daemonHolder = m_atoms[i].get();
                        break;
                    }
                }
            }

            m_atoms[i]->SetDaemon(std::move(ds));
        }
    }

    // Ensure all atoms have daemons if enabled (backward compat with old snapshots)
    if (config.enableDaemons) {
        AssignInitialDaemons();  // Only assigns to atoms without daemons
    }

    // Rebuild molecules and stats
    UpdateMolecules();
    CalculateStats();

    m_initialized = true;

    Logger::Info("Reactor: Loaded snapshot at step " + std::to_string(m_currentStep)
        + " with " + std::to_string(m_atoms.size()) + " atoms and "
        + std::to_string(m_bonds.size()) + " bonds.");

    return true;
}

// --- Timestep sub-operations ---

void Reactor::MoveAtoms(double dt) {
    auto& rng = DataManager::GetGlobalRandomEngine();
    std::normal_distribution<double> noise(0.0, 1.0);

    for (auto& atom : m_atoms) {
        double mass = atom->GetMass();
        double sigma = std::sqrt(ChemistryConfig::kB * m_temperature / mass);
        double dampingFactor = 0.9;

        Vec3 vel = atom->GetVelocity();
        vel = vel * dampingFactor;
        vel += Vec3{ noise(rng) * sigma * 0.1,
                     noise(rng) * sigma * 0.1,
                     noise(rng) * sigma * 0.1 };
        atom->SetVelocity(vel);
        atom->StepActivity(dt);
    }
}

void Reactor::ApplyBoundaryConditions() {
    for (auto& atom : m_atoms) {
        Vec3 pos = atom->GetPosition();
        Vec3 vel = atom->GetVelocity();

        auto reflect = [](double& p, double& v, double lo, double hi) {
            if (p < lo) { p = lo + (lo - p); v = -v; }
            if (p > hi) { p = hi - (p - hi); v = -v; }
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

    // Mark bonds for removal
    for (auto& bond : m_bonds) {
        double P_break = chemConfig.GetBreakingProbability(bond->energy, m_temperature);
        if (uniform01(rng) < P_break) {
            bond->markedForRemoval = true;
            m_breakingEventsThisStep++;
        }
    }

    // Remove marked bonds — invalidate daemon holders first
    for (auto it = m_bonds.begin(); it != m_bonds.end(); ) {
        if ((*it)->markedForRemoval) {
            Bond* bond = it->get();

            // Invalidate daemon that holds this bond
            if (bond->daemonHolder) {
                DaemonState* ds = bond->daemonHolder->GetDaemon();
                if (ds && ds->heldBond == bond) {
                    ds->heldBond = nullptr;
                }
                bond->daemonHolder = nullptr;
            }

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

    auto pairs = m_spatialGrid.GetNeighborPairs();

    for (const auto& [idx1, idx2] : pairs) {
        Atom* a1 = m_atoms[idx1].get();
        Atom* a2 = m_atoms[idx2].get();

        if (!a1->CanBond() || !a2->CanBond()) continue;
        if (a1->GetBondTo(a2) != nullptr) continue;

        Element e1 = a1->GetElement();
        Element e2 = a2->GetElement();
        double bondEnergy = chemConfig.GetBondEnergy(e1, e2, BondOrder::Single);
        if (bondEnergy <= 0.0) continue;

        double P_form = chemConfig.GetFormationProbability(e1, e2, BondOrder::Single, m_temperature);
        if (uniform01(rng) < P_form) {
            CreateBond(a1, a2, BondOrder::Single, m_currentStep);
            m_formationEventsThisStep++;
        }
    }
}

void Reactor::ResupplyAtoms() {
    // Inject fresh free atoms to replace those consumed into molecules.
    // Brings free atom counts back up to 50% of initial counts.
    auto& rng = DataManager::GetGlobalRandomEngine();
    const auto& config = DataManager::GetConfigParameters();
    int nextId = static_cast<int>(m_atoms.size());

    auto countFree = [&](Element e) {
        int count = 0;
        for (const auto& a : m_atoms) {
            if (a->GetElement() == e && a->IsFree()) count++;
        }
        return count;
    };

    auto resupply = [&](Element e, int initial) {
        int target = initial / 2;  // Replenish up to 50% of initial
        int free = countFree(e);
        int toAdd = target - free;
        if (toAdd <= 0) return 0;

        for (int i = 0; i < toAdd; ++i) {
            auto atom = ParticleFactory::CreateAtom(nextId++, e, m_boxSize, m_temperature, rng);
            if (config.enableDaemons) {
                auto& recipeBook = RecipeBook::GetInstance();
                auto daemon = std::make_unique<DaemonState>();
                daemon->recipe = &recipeBook.GetRandomRecipe(rng);
                daemon->assignedStep = m_currentStep;
                daemon->timeoutSteps = config.daemonTimeout;
                atom->SetDaemon(std::move(daemon));
            }
            m_atoms.push_back(std::move(atom));
        }
        return toAdd;
    };

    int addedC = resupply(Element::C, m_initialCarbon);
    int addedH = resupply(Element::H, m_initialHydrogen);
    int addedO = resupply(Element::O, m_initialOxygen);

    if (addedC + addedH + addedO > 0) {
        // Resize union-find arrays
        m_ufParent.resize(m_atoms.size());
        m_ufRank.resize(m_atoms.size());

        // Rebuild spatial grid with new atoms
        m_spatialGrid.Build(m_atoms);

        Logger::Info("Reactor: Resupplied +" + std::to_string(addedC) + "C +"
            + std::to_string(addedH) + "H +" + std::to_string(addedO) + "O"
            + " (total atoms: " + std::to_string(m_atoms.size()) + ")");
    }
}

void Reactor::UpdateMolecules() {
    size_t N = m_atoms.size();

    for (size_t i = 0; i < N; ++i) {
        m_ufParent[i] = static_cast<int>(i);
        m_ufRank[i] = 0;
    }

    for (const auto& bond : m_bonds) {
        int i1 = bond->atom1->GetID();
        int i2 = bond->atom2->GetID();
        UFUnion(i1, i2);
    }

    for (auto& atom : m_atoms) {
        atom->SetMolecule(nullptr);
    }
    m_molecules.clear();

    std::unordered_map<int, std::vector<int>> components;
    for (size_t i = 0; i < N; ++i) {
        int root = UFFind(static_cast<int>(i));
        if (!m_atoms[i]->GetBonds().empty()) {
            components[root].push_back(static_cast<int>(i));
        }
    }

    for (auto& [root, atomIndices] : components) {
        if (atomIndices.size() < 2) continue;

        auto mol = CreateMolecule(m_currentStep);
        for (int idx : atomIndices) {
            mol->AddAtom(m_atoms[idx].get());
        }

        for (const auto& bond : m_bonds) {
            if (UFFind(bond->atom1->GetID()) == UFFind(root)) {
                mol->AddBond(bond.get());
            }
        }
    }
}

void Reactor::CalculateStats() {
    m_totalBondEnergy = 0.0;
    for (const auto& bond : m_bonds) {
        m_totalBondEnergy += bond->energy;
    }

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
        m_ufParent[x] = m_ufParent[m_ufParent[x]];
        x = m_ufParent[x];
    }
    return x;
}

void Reactor::UFUnion(int x, int y) {
    int rx = UFFind(x);
    int ry = UFFind(y);
    if (rx == ry) return;

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
