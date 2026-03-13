#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace Chemistry {

// Element types for C, H, O atoms
enum class Element : int {
    C = 0,  // Carbon
    H = 1,  // Hydrogen
    O = 2   // Oxygen
};

constexpr int NUM_ELEMENTS = 3;

// Bond order: single, double, triple
enum class BondOrder : int {
    Single = 1,
    Double = 2,
    Triple = 3
};

// Atomic mass in atomic mass units (u)
constexpr double AtomicMass(Element e) {
    switch (e) {
    case Element::C: return 12.011;
    case Element::H: return 1.008;
    case Element::O: return 15.999;
    default: return 1.0;
    }
}

// Maximum valence for each element
constexpr int MaxValence(Element e) {
    switch (e) {
    case Element::C: return 4;
    case Element::H: return 1;
    case Element::O: return 2;
    default: return 0;
    }
}

// Element symbol for display
inline const char* ElementSymbol(Element e) {
    switch (e) {
    case Element::C: return "C";
    case Element::H: return "H";
    case Element::O: return "O";
    default: return "?";
    }
}

// Singleton configuration holding bond energy tables and kinetic parameters
class ChemistryConfig {
public:
    static ChemistryConfig& GetInstance();

    // Bond dissociation energy in kJ/mol for a given element pair and bond order.
    // Returns 0.0 if the bond is not defined (e.g., H=H double bond).
    double GetBondEnergy(Element e1, Element e2, BondOrder order) const;

    // Formation probability: P = min(1, A_form * exp(-E_act / (kB * T)))
    // where E_act = activationFraction * bondEnergy
    double GetFormationProbability(Element e1, Element e2, BondOrder order, double temperature) const;

    // Breaking probability: P = A_break * exp(-E_bond / (kB * T))
    double GetBreakingProbability(double bondEnergy, double temperature) const;

    // Kinetic parameters (settable from config)
    double A_form = 1.0;              // Pre-exponential factor for formation
    double A_break = 1.0e-3;          // Pre-exponential factor for breaking
    double activationFraction = 0.1;  // E_activation = activationFraction * E_bond

    // Boltzmann constant in kJ/(mol*K)
    static constexpr double kB = 8.314462618e-3;  // R in kJ/(mol*K), used as kB for molar energies

private:
    ChemistryConfig();

    // Bond energy lookup: indexed by [min(e1,e2)][max(e1,e2)][order-1]
    // For 3 elements: [3][3][3] array
    double m_bondEnergy[NUM_ELEMENTS][NUM_ELEMENTS][3]{};

    void InitBondEnergyTable();
};

} // namespace Chemistry
