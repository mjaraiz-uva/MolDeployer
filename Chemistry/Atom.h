#pragma once

#include <vector>
#include <memory>
#include "Particle.h"
#include "ChemistryConfig.h"
#include "Daemon.h"

namespace Chemistry {

// Forward declarations
struct Bond;
class Molecule;

// An Atom is the core agent of MolecularDeployer.
// Analogous to Region::Worker in the economic model.
class Atom : public Particle {
public:
    Atom();
    Atom(int id, Element element);
    ~Atom() override;

    // Element properties
    Element GetElement() const { return m_element; }
    int GetMaxValence() const { return m_maxValence; }
    int GetCurrentBondOrder() const { return m_currentBondOrder; }
    int GetFreeValence() const { return m_maxValence - m_currentBondOrder; }
    bool CanBond() const { return GetFreeValence() > 0; }

    // Bond management (non-owning pointers; Reactor owns all Bonds)
    const std::vector<Bond*>& GetBonds() const { return m_bonds; }
    void AddBond(Bond* bond, int bondOrderValue);
    void RemoveBond(Bond* bond, int bondOrderValue);

    // Molecule membership (nullptr if free atom)
    Molecule* GetMolecule() const { return m_molecule; }
    void SetMolecule(Molecule* mol) { m_molecule = mol; }
    bool IsFree() const { return m_molecule == nullptr; }

    // Check if this atom already has a bond to another specific atom
    Bond* GetBondTo(const Atom* other) const;

    // Daemon state (riding daemon, always non-null when daemons enabled)
    DaemonState* GetDaemon() { return m_daemon.get(); }
    const DaemonState* GetDaemon() const { return m_daemon.get(); }
    bool HasDaemon() const { return m_daemon != nullptr; }
    void SetDaemon(std::unique_ptr<DaemonState> d) { m_daemon = std::move(d); }
    void ClearDaemon() { m_daemon.reset(); }

    // Per-timestep: Brownian motion update
    void StepActivity(double dt) override;

private:
    Element m_element;
    int m_maxValence;
    int m_currentBondOrder = 0;     // Sum of all bond orders on this atom
    std::vector<Bond*> m_bonds;     // Active bonds (non-owning)
    Molecule* m_molecule = nullptr; // Owning molecule (non-owning ptr)
    std::unique_ptr<DaemonState> m_daemon;  // Riding daemon state
};

} // namespace Chemistry
