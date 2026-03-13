#pragma once

#include <memory>
#include <random>
#include "Atom.h"
#include "Vec3.h"

namespace Chemistry {

// Factory for creating atoms with random initial positions and velocities.
// Analogous to Region::AgentFactory in the economic model.
class ParticleFactory {
public:
    // Create an atom with a random position within the box and
    // a Maxwell-Boltzmann distributed velocity at the given temperature.
    static std::unique_ptr<Atom> CreateAtom(int id, Element element,
                                            const Vec3& boxSize,
                                            double temperature,
                                            std::mt19937& rng);

private:
    // Generate a random velocity component from Maxwell-Boltzmann distribution
    static double RandomVelocityComponent(double mass, double temperature, std::mt19937& rng);
};

} // namespace Chemistry
