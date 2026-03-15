#pragma once

#include <vector>
#include <memory>
#include "Vec3.h"
#include "AgentState.h"

namespace DABM {

struct Bond;
class Assembly;

// An Agent is the primary entity in the D-ABM simulation.
// Chemistry: Atom (C, H, O) with valence, bonds, Brownian motion.
// Economics: Worker/Individual that buys, produces, sells.
// Robotics: Homunculus executing sub-tasks.
//
// Every Agent has a riding AgentState (daemon) that drives its goal-seeking behavior.
class Agent {
public:
    Agent();
    Agent(int id, int speciesId, int maxValence, double mass);
    ~Agent();

    // Identity
    int GetID() const { return m_id; }
    int GetSpeciesId() const { return m_speciesId; }

    // Spatial state
    const Vec3& GetPosition() const { return m_position; }
    void SetPosition(const Vec3& p) { m_position = p; }
    const Vec3& GetVelocity() const { return m_velocity; }
    void SetVelocity(const Vec3& v) { m_velocity = v; }
    double GetMass() const { return m_mass; }

    // Brownian motion step
    void StepMotion(double dt);

    // Valence / bonding capacity
    int GetMaxValence() const { return m_maxValence; }
    int GetCurrentBondOrder() const { return m_currentBondOrder; }
    int GetFreeValence() const { return m_maxValence - m_currentBondOrder; }
    bool CanBond() const { return GetFreeValence() > 0; }

    // Bond management (non-owning pointers; Environment owns all Bonds)
    const std::vector<Bond*>& GetBonds() const { return m_bonds; }
    void AddBond(Bond* bond, int bondOrderValue);
    void RemoveBond(Bond* bond, int bondOrderValue);
    Bond* GetBondTo(const Agent* other) const;

    // Assembly membership (nullptr if free agent)
    Assembly* GetAssembly() const { return m_assembly; }
    void SetAssembly(Assembly* assembly) { m_assembly = assembly; }
    bool IsFree() const { return m_assembly == nullptr; }

    // Daemon/goal state (riding daemon, always non-null when daemons enabled)
    AgentState* GetDaemon() { return m_daemon.get(); }
    const AgentState* GetDaemon() const { return m_daemon.get(); }
    bool HasDaemon() const { return m_daemon != nullptr; }
    void SetDaemon(std::unique_ptr<AgentState> d) { m_daemon = std::move(d); }
    void ClearDaemon() { m_daemon.reset(); }

private:
    int m_id = -1;
    int m_speciesId = -1;
    Vec3 m_position;
    Vec3 m_velocity;
    double m_mass = 1.0;
    int m_maxValence = 0;
    int m_currentBondOrder = 0;
    std::vector<Bond*> m_bonds;
    Assembly* m_assembly = nullptr;
    std::unique_ptr<AgentState> m_daemon;
};

} // namespace DABM
