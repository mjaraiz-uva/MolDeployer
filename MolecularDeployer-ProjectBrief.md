# MolecularDeployer — Project Brief

**Date:** March 2026  
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

**Key shared principle:** Both simulators start from nothing (zero firms / free atoms) and let complex structures (economies / molecular networks) emerge purely from local agent interactions, with no top-down design.

---

## 3. State of the Art (Research Summary, March 2026)

### 3.1 Agent-Based Molecular Self-Assembly

Fortuna & Troisi (PNAS 2005, J. Phys. Chem. B 2009–2010) pioneered ABM for molecular self-assembly. Their agents represent rigid molecular fragments that evolve via stochastic, deterministic, and adaptive rules, finding lower-energy packings than Monte Carlo. **Limitation:** they work with pre-formed molecules assembling into crystals — not atoms forming bonds from scratch.

### 3.2 Prebiotic Reaction Network Generation (Allchemy / NOEL)

Grzybowski et al. (Science 2020) created Allchemy: a forward-synthesis algorithm encoding ~500 known prebiotic reactions, applied iteratively to 6 primordial substrates (CH₄, NH₃, H₂O, HCN, N₂, H₂S). Generated ~35,000 compounds including 82 biotic molecules. Key findings:
- Biotic molecules are more water-soluble and thermodynamically stable than abiotic ones
- Self-regenerating cycles, catalytic emergence, and surfactant formation all appear
- The expanded NOEL network (Chem 2024) reached 4.9 billion reactions from 9 starting materials

**Limitation:** relies on pre-encoded human-known reaction rules, not emergent bond formation.

### 3.3 Reactive Molecular Dynamics (ReaxFF)

ReaxFF (van Duin, Goddard et al.) uses bond-order formalism instead of explicit bonds, allowing continuous bond formation/breaking in MD simulations. Applied to hydrocarbon oxidation, combustion, materials. The **ab initio nanoreactor** (AINR, Martínez et al.) used GPU-accelerated AIMD to discover prebiotic molecules from HCN+water without predefined reactions.

**Limitation:** computationally extremely expensive (quantum-level), limiting scalability to hundreds of atoms / nanoseconds.

### 3.4 Physics-Based Prebiotic Particle Simulation

A recent preprint (Research Square, Dec 2025) describes a physics-based particle simulation using continuous molecular dynamics with literature-derived bond parameters and real-time chemical novelty detection. Ran 30 simulations across Miller-Urey, hydrothermal vent, and formamide scenarios. Detected scenario-specific autocatalytic networks.

**Closest existing work** to MolecularDeployer, but focused on prebiotic validation, not the ABM economic-analogy framework.

### 3.5 Autocatalytic Sets and Chemical Emergence

RAF (Reflexively Autocatalytic F-generated sets) theory (Hordijk, Steel, Kauffman) provides mathematical tools for detecting self-sustaining autocatalytic subsets in reaction networks. Key insight: autocatalytic sets emerge spontaneously in random catalytic networks above a critical complexity threshold. Whitesides et al. (Nature 2016) demonstrated bistability and oscillations in simple organic reaction networks without enzymes.

### 3.6 The Gap MolecularDeployer Fills

No existing system combines:
1. ABM "deploy from scratch" philosophy (atoms as agents, not pre-formed molecules)
2. Simplified but thermodynamically grounded bond-formation rules (not full QM)
3. Emergent reaction network analysis (stability ranking + intermediate detection)
4. The economic-ABM structural analogy (supply/demand ↔ valence satisfaction)

---

## 4. Key Design Decisions

### 4.1 Build on DEPX Framework

- MolecularDeployer will be built on the **DEPX** C++ framework (the same modern C++ ABM engine used for DEPLOYERS development)
- The first task in Claude Code will be to read and understand the DEPX class hierarchy, then adapt it for the chemistry domain
- Target: modern C++ (C++20 minimum)

### 4.2 Physical Realism Level

**Decision pending** — options ranked by complexity:

| Option | Description | Pros | Cons |
|--------|-------------|------|------|
| **Simplified** | Valence + bond-energy table + Boltzmann acceptance | Fast to implement, easy to understand | May miss important geometric effects |
| **Intermediate** | Add steric/geometric constraints (angles, distances) | More realistic molecular shapes | More complex encounter logic |
| **ReaxFF-like** | Simplified bond-order potential | Closest to real chemistry | Heavy implementation, slow |
| **Iterative** | Start simple, add layers as needed | Agile, validates concept early | Potential refactoring cost |

**Recommendation:** Start simple (valence + energy + Boltzmann), validate the emergent behavior, then add geometric constraints in a second pass.

### 4.3 Bond Formation Rules (Simplified Model)

**Bond energy table (kJ/mol):**

| Bond | Energy | Bond | Energy |
|------|--------|------|--------|
| C–H  | 411    | C–C  | 346    |
| O–H  | 459    | C=C  | 614    |
| C–O  | 358    | C=O  | 799    |
| C≡O  | 1072   | O=O  | 498    |
| H–H  | 436    | C≡C  | 839    |

**Valence constraints:** C=4, O=2, H=1

**Bond formation probability:**
```
P_form = min(1, A_form * exp(-E_activation / (k_B * T)))
```
where E_activation is a fraction of the bond dissociation energy (lower for stronger bonds).

**Bond breaking probability (Arrhenius-like):**
```
P_break = A_break * exp(-E_bond / (k_B * T))
```
Stronger bonds break less often. Temperature T is the key control parameter.

### 4.4 Emergent Phenomena to Track

All four categories are of equal interest (full reaction network analysis):

1. **Thermodynamic stability ranking** — which molecules survive longest, lowest energy per atom
2. **Intermediate usage / hub detection** — which species appear most often as reactants in further reactions  
3. **Autocatalytic cycle emergence** — closed loops where products enable their own formation
4. **Hierarchical complexity** — tiers of increasing molecular size and stability

### 4.5 Relationship to DEPLOYERS Architecture

**Decision pending** — options:

| Option | Description |
|--------|-------------|
| **Same class hierarchy** | Adapt DEPLOYERS Agent/Firm/Market classes directly to Atom/Molecule/BondingSpace |
| **Inspired but independent** | Use DEPX framework conventions but design chemistry-specific classes |
| **Shared ABM engine** | Abstract a common DEPX core with domain-specific modules for economy vs. chemistry |

**Recommendation:** Share a common DEPX engine with domain-specific modules — this is architecturally elegant and reinforces the cross-domain analogy, and enables future comparisons.

---

## 5. Phased Implementation Plan

### Phase 1 — Core Atomic Agent Engine (Weeks 1–5)
- Atom agents (C, H, O) with position, velocity, valence state, bond list
- 3D spatial movement (Brownian / kinetic) in bounded volume
- Encounter detection via spatial hashing / cell-list algorithm
- Bond formation rules (valence + energy table + Boltzmann)
- Molecule tracking via union-find / connected components
- Discrete time-step simulation loop

### Phase 2 — Bond Breaking, Stability & Decomposition (Weeks 6–9)
- Thermal decomposition (Arrhenius-like P_break)
- Collision-induced fragmentation
- Stability scoring (total bond energy, structural features, ring closure bonus)
- Intermediate usage counting

### Phase 3 — Reaction Network Emergence & Analysis (Weeks 10–14)
- Reaction logger (every bond event → directed graph edge)
- Network analysis: autocatalytic cycles, hub molecules, hierarchical tiers
- Molecular census (population dynamics over time)
- Cross-reference with known C/H/O compounds (NIST WebBook, PubChem subset)

### Phase 4 — Tunable Physical Parameters & Scenarios (Weeks 15–18)
- Temperature ramps (cooling from chaotic to ordered)
- Atom ratio variations (C:H:O composition)
- Volume/density control
- Energy injection pulses (UV-like)

### Phase 5 — Performance & Visualization (Weeks 19–23)
- OpenMP parallelism for spatial decomposition
- Optional CUDA kernels for atom movement / neighbor lists
- 3D visualization export (VMD/Ovito format or WebGL)
- Reaction network visualization (Graphviz/Gephi)
- Python bindings (pybind11) for Jupyter analysis

### Phase 6 — Validation & Cross-Comparison (Weeks 24–28)
- Validate against NIST thermodynamic data
- Compare network topology with Allchemy/NOEL prebiotic networks
- Formal cross-model comparison with DEPLOYERS economic network
- Sensitivity analysis (initial conditions → emergent molecular "economies")

---

## 6. Key References

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

## 7. First Steps in Claude Code / VS Code

When opening this project in Claude Code:

1. **Read the DEPX framework** — examine all `.h` and `.cpp` files to understand the class hierarchy, simulation loop, agent model, and build system.
2. **Identify reusable components** — spatial management, agent base class, time-stepping, I/O.
3. **Design the Atom class** — adapt from the DEPX agent model: position (3D), velocity, element type (C/H/O), valence state, bond list.
4. **Design the Molecule class** — analogous to DEPLOYERS' Firm: a connected set of bonded atoms, with computed properties (formula, energy, stability score).
5. **Implement encounter detection** — spatial hashing for efficient neighbor queries.
6. **Implement the bond formation/breaking loop** — the core simulation step.
7. **Build and test with a minimal scenario** — e.g., 100 C + 400 H + 50 O atoms at a moderate temperature.

---

*This document was generated from a comprehensive research and planning session in claude.ai (March 2026). It is intended to provide full context for the Claude Code development session.*
