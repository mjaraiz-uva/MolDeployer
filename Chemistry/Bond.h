#pragma once

#include "ChemistryConfig.h"

namespace Chemistry {

// Forward declaration
class Atom;

// A covalent bond between two atoms.
// Owned by the Reactor; Atoms hold non-owning pointers.
// No direct analogue in the economic model (bonds are a chemistry-specific concept).
struct Bond {
    int id = -1;
    Atom* atom1 = nullptr;
    Atom* atom2 = nullptr;
    BondOrder order = BondOrder::Single;
    double energy = 0.0;       // Bond dissociation energy (kJ/mol), from lookup table
    int formationStep = -1;    // Timestep when this bond was formed
    bool markedForRemoval = false;  // Flag used during bond-breaking pass
    Atom* daemonHolder = nullptr;   // Atom whose daemon holds this bond (for safe invalidation)

    // Returns the other atom in the bond
    Atom* GetOther(const Atom* a) const {
        return (a == atom1) ? atom2 : atom1;
    }
};

} // namespace Chemistry
