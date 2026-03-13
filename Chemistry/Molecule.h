#pragma once

#include <vector>
#include <string>

namespace Chemistry {

// Forward declarations
class Atom;
struct Bond;

// A Molecule is a connected component of bonded atoms.
// Analogous to Region::Firm in the economic model.
class Molecule {
public:
    Molecule();
    explicit Molecule(int id);
    ~Molecule();

    int GetID() const { return m_id; }

    // Atom membership
    const std::vector<Atom*>& GetAtoms() const { return m_atoms; }
    void AddAtom(Atom* atom);
    void RemoveAtom(Atom* atom);
    size_t GetAtomCount() const { return m_atoms.size(); }

    // Bond membership
    const std::vector<Bond*>& GetBonds() const { return m_bonds; }
    void AddBond(Bond* bond);
    void RemoveBond(Bond* bond);

    // Computed properties
    std::string GetMolecularFormula() const;
    double GetTotalBondEnergy() const;
    double GetStabilityScore() const;  // Total bond energy / atom count

    // Lifecycle tracking
    int GetFormationStep() const { return m_formationStep; }
    void SetFormationStep(int step) { m_formationStep = step; }
    int GetDestructionStep() const { return m_destructionStep; }
    void SetDestructionStep(int step) { m_destructionStep = step; }
    bool IsAlive() const { return m_destructionStep < 0; }

private:
    int m_id;
    std::vector<Atom*> m_atoms;
    std::vector<Bond*> m_bonds;
    int m_formationStep = -1;
    int m_destructionStep = -1;
};

} // namespace Chemistry
