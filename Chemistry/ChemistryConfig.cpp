#include "ChemistryConfig.h"

namespace Chemistry {

ChemistryConfig& ChemistryConfig::GetInstance() {
    static ChemistryConfig instance;
    return instance;
}

ChemistryConfig::ChemistryConfig() {
    InitBondEnergyTable();
}

void ChemistryConfig::InitBondEnergyTable() {
    // Zero-initialize all entries
    for (auto& plane : m_bondEnergy)
        for (auto& row : plane)
            for (auto& val : row)
                val = 0.0;

    // Helper lambda: set energy for element pair (symmetric)
    auto set = [this](Element e1, Element e2, BondOrder order, double energy_kJ) {
        int i = std::min(static_cast<int>(e1), static_cast<int>(e2));
        int j = std::max(static_cast<int>(e1), static_cast<int>(e2));
        int k = static_cast<int>(order) - 1;
        m_bondEnergy[i][j][k] = energy_kJ;
    };

    // Bond dissociation energies from project brief (kJ/mol)
    // C-C bonds
    set(Element::C, Element::C, BondOrder::Single, 346.0);
    set(Element::C, Element::C, BondOrder::Double, 614.0);
    set(Element::C, Element::C, BondOrder::Triple, 839.0);

    // C-H bond (single only)
    set(Element::C, Element::H, BondOrder::Single, 411.0);

    // C-O bonds
    set(Element::C, Element::O, BondOrder::Single, 358.0);
    set(Element::C, Element::O, BondOrder::Double, 799.0);
    set(Element::C, Element::O, BondOrder::Triple, 1072.0);

    // H-H bond (single only)
    set(Element::H, Element::H, BondOrder::Single, 436.0);

    // O-H bond (single only)
    set(Element::H, Element::O, BondOrder::Single, 459.0);

    // O=O bond (double only)
    set(Element::O, Element::O, BondOrder::Double, 498.0);
}

double ChemistryConfig::GetBondEnergy(Element e1, Element e2, BondOrder order) const {
    int i = std::min(static_cast<int>(e1), static_cast<int>(e2));
    int j = std::max(static_cast<int>(e1), static_cast<int>(e2));
    int k = static_cast<int>(order) - 1;
    return m_bondEnergy[i][j][k];
}

double ChemistryConfig::GetFormationProbability(Element e1, Element e2, BondOrder order, double temperature) const {
    double bondEnergy = GetBondEnergy(e1, e2, order);
    if (bondEnergy <= 0.0) return 0.0;  // Bond type not defined
    if (temperature <= 0.0) return 0.0;

    double E_act = activationFraction * bondEnergy;
    double exponent = -E_act / (kB * temperature);
    double prob = A_form * std::exp(exponent);
    return std::min(1.0, prob);
}

double ChemistryConfig::GetBreakingProbability(double bondEnergy, double temperature) const {
    if (bondEnergy <= 0.0) return 1.0;   // Zero-energy bond always breaks
    if (temperature <= 0.0) return 0.0;   // Absolute zero: nothing breaks

    double exponent = -bondEnergy / (kB * temperature);
    double prob = A_break * std::exp(exponent);
    return std::min(1.0, prob);
}

} // namespace Chemistry
