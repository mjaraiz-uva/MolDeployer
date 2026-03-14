# MolecularDeployer — Project Brief

**Date:** March 2026 (updated 2026-03-14)
**Author:** Martín Jaraíz (UVa) — with Claude research assistance
**Framework basis:** DEPX (C++ ABM framework, ~9 months old)
**Companion project:** DEPLOYERS (macroeconomic ABM simulator)

---

## 1. Project Goal

**MolecularDeployer** is an agent-based simulator where individual **C, H, O atoms** move in a simulated 3D space, encounter each other stochastically, form covalent bonds according to simplified but physically grounded rules, and build increasingly complex molecular structures from scratch.

The simulation tracks which molecules emerge as:
- **Most thermodynamically stable** (long-lived, low total energy)
- **Most used as intermediates** (high throughput — frequently formed and consumed in further reactions)

This is a direct transposition of the **DEPLOYERS** economic ABM logic into chemistry.

---

## 2. DEPLOYERS ↔ MolecularDeployer Analogy

| DEPLOYERS (Economy)              | MolecularDeployer (Chemistry)             |
|----------------------------------|-------------------------------------------|
| Worker agent                     | Atom agent (C, H, O)                      |
| PurchaseList (needs)             | Unsatisfied valence (bonding capacity)     |
| Worker starts a new Firm         | Atoms form a new Molecule (bond creation)  |
| Product / Intermediate goods     | Molecules / Intermediate species           |
| Firm closure (no demand)         | Molecular decomposition (instability)      |
| Active population cap            | Conservation of atoms (fixed pool)         |
| Market price / demand signals    | Bond energy / thermodynamic stability      |
| Emergent economy from zero firms | Emergent reaction network from free atoms  |
| Daemon (firm agent)              | DaemonState riding an atom                 |
| Firm death → decomposition       | Timeout → bond breaks → molecule decomposes|

**Key shared principle:** Both simulators start from nothing (zero firms / free atoms) and let complex structures (economies / molecular networks) emerge purely from local agent interactions, with no top-down design.

---

## 3. Current Architecture (as of March 2026)

### 3.1 Module Structure

| Module | Purpose |
|--------|---------|
| **Chemistry/** | Atom, Bond, Molecule, Reactor, SpatialGrid, Recipe/RecipeBook, DaemonState, ChemistryConfig |
| **Simulator/** | Simulation loop, timestep orchestration, log reporting |
| **DataManager/** | Config load/save (JSON), snapshot persistence |
| **Visualizer/** | ImGui/ImPlot time-series plots with INI persistence |
| **InterfaceGUI/** | GLFW + OpenGL window management |
| **Logger/** | Thread-safe logging (console + file) |
| **ThemeManager/** | ImGui theme management |
| **DEPX/** | Main entry point, control window UI |

### 3.2 Riding Daemons System

The v2 daemon system (riding daemons) is the core innovation. Every atom carries a `DaemonState`:

```
struct DaemonState {
    const Recipe* recipe;       // What this daemon is trying to build
    int assignedStep;           // When daemon was assigned
    int timeoutSteps;           // Steps until timeout
    int successCount = 0;
    Bond* heldBond = nullptr;   // null = SEARCHING, non-null = HOLDING
};
```

**Lifecycle:**
- **SEARCHING**: Atom's daemon checks spatial neighbors for a complementary component. On match → form bond → become HOLDING.
- **HOLDING**: Bond persists. On timeout → bond breaks → molecule decomposes → both atoms get fresh daemons → back to SEARCHING.

**Darwinian Selection:**
- On success, an offspring daemon spawns on a neighbor atom with a recipe that uses the product as input (e.g., H+H→H₂ success → offspring seeks H₂+O→H₂O).
- This creates emergent supply chains and Darwinian competition for recipes.

### 3.3 Bond Formation

Two parallel mechanisms:
1. **Stochastic bonds** — Arrhenius-like probability based on bond energy and temperature
2. **Daemon-directed bonds** — daemons search for recipe-matching neighbors within cutoff radius

**Bond energy table (kJ/mol):**

| Bond | Energy | Bond | Energy |
|------|--------|------|--------|
| C–H  | 411    | C–C  | 346    |
| O–H  | 459    | C=C  | 614    |
| C–O  | 358    | C=O  | 799    |
| C≡O  | 1072   | O=O  | 498    |
| H–H  | 436    | C≡C  | 839    |

**Valence constraints:** C=4, O=2, H=1

### 3.4 Visualization

- **Time-series plots** (ImPlot): atom counts, bond counts, energy, daemon stats (searching/holding/spawns/successes/deaths)
- **Per-plot controls**: Show Line, Show Markers, Log Y scale, Marker Size — all persisted via custom ImGui INI handler
- **Molecule Census window**: real-time list of all molecular species, sortable by size (atom count) or abundance (count %)
- **Log reporting**: species sorted by size (largest first), per-atom bond energy

---

## 4. State of the Art (Research Summary)

### 4.1 Agent-Based Molecular Self-Assembly

Fortuna & Troisi (PNAS 2005, J. Phys. Chem. B 2009–2010) pioneered ABM for molecular self-assembly. Their agents represent rigid molecular fragments that evolve via stochastic, deterministic, and adaptive rules. **Limitation:** they work with pre-formed molecules assembling into crystals — not atoms forming bonds from scratch.

### 4.2 Prebiotic Reaction Network Generation (Allchemy / NOEL)

Grzybowski et al. (Science 2020) created Allchemy: a forward-synthesis algorithm encoding ~500 known prebiotic reactions, applied iteratively to 6 primordial substrates. Generated ~35,000 compounds including 82 biotic molecules. The expanded NOEL network (Chem 2024) reached 4.9 billion reactions from 9 starting materials.

**Limitation:** relies on pre-encoded human-known reaction rules, not emergent bond formation.

### 4.3 Reactive Molecular Dynamics (ReaxFF)

ReaxFF (van Duin, Goddard et al.) uses bond-order formalism instead of explicit bonds, allowing continuous bond formation/breaking in MD simulations. The **ab initio nanoreactor** (AINR, Martínez et al.) used GPU-accelerated AIMD to discover prebiotic molecules from HCN+water without predefined reactions.

**Limitation:** computationally extremely expensive (quantum-level), limiting scalability.

### 4.4 The Gap MolecularDeployer Fills

No existing system combines:
1. ABM "deploy from scratch" philosophy (atoms as agents, not pre-formed molecules)
2. Simplified but thermodynamically grounded bond-formation rules (not full QM)
3. Darwinian daemon selection creating emergent supply chains
4. The economic-ABM structural analogy (supply/demand ↔ valence satisfaction)

---

## 5. Key Design Decisions

### 5.1 Build on DEPX Framework

- Built on the **DEPX** C++ framework (the same modern C++ ABM engine used for DEPLOYERS development)
- Target: modern C++ (C++20), Visual Studio 2025, MSBuild, vcpkg for dependencies
- Solution file: `DEPX.sln`

### 5.2 Physical Realism Level

**Current approach: Iterative** — started with simplified valence + energy + Boltzmann, validated emergent behavior. Geometric/steric constraints are the next planned addition.

### 5.3 Bond Formation Rules

**Bond formation probability:**
```
P_form = min(1, A_form * exp(-E_activation / (k_B * T)))
```

**Bond breaking probability (Arrhenius-like):**
```
P_break = A_break * exp(-E_bond / (k_B * T))
```

Stronger bonds break less often. Temperature T is the key control parameter.

---

## 6. What Works, What's Next

### Implemented and Working

- Full atom movement in 3D box with reflective/periodic boundaries
- Stochastic bond formation and breaking (Arrhenius-like)
- Riding daemon system with Darwinian offspring selection
- Recipe book with forward index (componentA + componentB → product) and reverse index (product → recipes)
- Spatial grid for efficient neighbor queries
- Molecule tracking, census, and formula computation (Hill system)
- Atom resupply (periodic injection of fresh atoms)
- Snapshot save/load with full daemon state persistence
- Time-series visualization with persistent per-plot settings
- Molecule Census UI window
- Detailed log reporting (species by size, per-atom energy)

### Known Issue: Blob Accretion

Without steric/geometric constraints, `MakeRoomForBond` allows unchecked accretion — one molecule can absorb most atoms into a heavily cross-linked blob (e.g., C₉₄₃H₁₀₈₂ with H/C ratio ~1.15, indicating dense cross-linking rather than chains).

### Next Task: 3D Strain Constraints

Adding steric constraints to prevent unrealistic blob growth. The user has design ideas for this. Expected approach: penalize or reject bonds that would create geometrically implausible structures (e.g., too many bonds in too small a volume, unrealistic bond angles).

---

## 7. Key References

- Axtell & Farmer (2025). "Agent-Based Modeling in Economics and Finance." *J. Econ. Lit.* 63(1), 197–287.
- Fortuna & Troisi (2005). "An agent-based approach for modeling molecular self-organization." *PNAS* 102(2), 410–415.
- Wołos, Roszak et al. (2020). "Synthetic connectivity, emergence, and self-regeneration in the network of prebiotic chemistry." *Science* 369, eaaw1955.
- Roszak et al. (2024). "Emergence of metabolic-like cycles in blockchain-orchestrated reaction networks." *Chem*.
- van Duin et al. (2001/2016). ReaxFF reactive force field. *npj Comput. Mater.* 2, 15011.
- Hordijk (2022). "Autocatalytic Sets: Complexity at the Interface of Chemistry and Biology." Templeton Foundation.
- Semenov et al. (2016). "Autocatalytic, bistable, oscillatory networks of biologically relevant organic reactions." *Nature* 537, 656–660.
- Research Square preprint (Dec 2025). "Emergent Molecular Complexity in Prebiotic Chemistry Simulations."
- Pérez-Villa, Pietrucci, Saitta (2020). "Prebiotic chemistry and origins of life research with atomistic computer simulations." *Phys. Life Rev.* 34-35, 105–135.

---

*This document is maintained alongside the codebase and updated as the project evolves.*
