#pragma once

#include "Vec3.h"

namespace Chemistry {

// Forward declaration
class Reactor;

// Base class for all particles in the simulation.
// Analogous to Region::Agent in the economic model.
class Particle {
public:
    // Static pointer to the owning Reactor (set once during initialization).
    // Analogous to Agent::s_region.
    static Reactor* s_reactor;
    static void SetReactor(Reactor* reactor);

    Particle();
    explicit Particle(int id);
    virtual ~Particle();

    // Core properties
    int GetID() const { return m_id; }
    double GetMass() const { return m_mass; }

    // Position
    const Vec3& GetPosition() const { return m_position; }
    void SetPosition(const Vec3& pos) { m_position = pos; }

    // Velocity
    const Vec3& GetVelocity() const { return m_velocity; }
    void SetVelocity(const Vec3& vel) { m_velocity = vel; }

    // Per-timestep activity (analogous to Agent::monthlyActivity)
    virtual void StepActivity(double dt) = 0;

protected:
    int m_id;
    Vec3 m_position;
    Vec3 m_velocity;
    double m_mass = 1.0;
};

} // namespace Chemistry
