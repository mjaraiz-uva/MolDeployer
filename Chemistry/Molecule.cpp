#include "Molecule.h"
#include "Atom.h"
#include "Bond.h"
#include <algorithm>
#include <map>

namespace Chemistry {

Molecule::Molecule() : m_id(-1) {}

Molecule::Molecule(int id) : m_id(id) {}

Molecule::~Molecule() = default;

void Molecule::AddAtom(Atom* atom) {
    m_atoms.push_back(atom);
    atom->SetMolecule(this);
}

void Molecule::RemoveAtom(Atom* atom) {
    auto it = std::find(m_atoms.begin(), m_atoms.end(), atom);
    if (it != m_atoms.end()) {
        m_atoms.erase(it);
        atom->SetMolecule(nullptr);
    }
}

void Molecule::AddBond(Bond* bond) {
    m_bonds.push_back(bond);
}

void Molecule::RemoveBond(Bond* bond) {
    auto it = std::find(m_bonds.begin(), m_bonds.end(), bond);
    if (it != m_bonds.end()) {
        m_bonds.erase(it);
    }
}

std::string Molecule::GetMolecularFormula() const {
    // Count atoms by element in Hill system order: C first, H second, then O
    std::map<Element, int> counts;
    for (const Atom* atom : m_atoms) {
        counts[atom->GetElement()]++;
    }

    std::string formula;

    // Hill system: C first, then H, then alphabetical (O)
    auto append = [&](Element e) {
        auto it = counts.find(e);
        if (it != counts.end() && it->second > 0) {
            formula += ElementSymbol(e);
            if (it->second > 1) {
                formula += std::to_string(it->second);
            }
        }
    };

    append(Element::C);
    append(Element::H);
    append(Element::O);

    return formula;
}

double Molecule::GetTotalBondEnergy() const {
    double total = 0.0;
    for (const Bond* bond : m_bonds) {
        total += bond->energy;
    }
    return total;
}

double Molecule::GetStabilityScore() const {
    if (m_atoms.empty()) return 0.0;
    return GetTotalBondEnergy() / static_cast<double>(m_atoms.size());
}

} // namespace Chemistry
