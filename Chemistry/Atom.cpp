#include "Atom.h"
#include "Bond.h"
#include <algorithm>

namespace Chemistry {

Atom::Atom() : Particle(), m_element(Element::H), m_maxValence(1) {}

Atom::Atom(int id, Element element)
    : Particle(id)
    , m_element(element)
    , m_maxValence(MaxValence(element))
{
    m_mass = AtomicMass(element);
}

Atom::~Atom() = default;

void Atom::AddBond(Bond* bond, int bondOrderValue) {
    m_bonds.push_back(bond);
    m_currentBondOrder += bondOrderValue;
}

void Atom::RemoveBond(Bond* bond, int bondOrderValue) {
    auto it = std::find(m_bonds.begin(), m_bonds.end(), bond);
    if (it != m_bonds.end()) {
        m_bonds.erase(it);
        m_currentBondOrder -= bondOrderValue;
        if (m_currentBondOrder < 0) m_currentBondOrder = 0;
    }
}

Bond* Atom::GetBondTo(const Atom* other) const {
    for (Bond* b : m_bonds) {
        if (b->atom1 == other || b->atom2 == other) {
            return b;
        }
    }
    return nullptr;
}

void Atom::StepActivity(double dt) {
    // Brownian motion: position update
    // Velocity is updated by the Reactor (random kicks + boundary conditions)
    // Here we just integrate position
    m_position += m_velocity * dt;
}

} // namespace Chemistry
