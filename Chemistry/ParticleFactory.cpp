#include "ParticleFactory.h"
#include "ChemistryConfig.h"

namespace Chemistry {

std::unique_ptr<Atom> ParticleFactory::CreateAtom(int id, Element element,
                                                   const Vec3& boxSize,
                                                   double temperature,
                                                   std::mt19937& rng) {
    auto atom = std::make_unique<Atom>(id, element);

    // Random position uniformly distributed within the box
    std::uniform_real_distribution<double> distX(0.0, boxSize.x);
    std::uniform_real_distribution<double> distY(0.0, boxSize.y);
    std::uniform_real_distribution<double> distZ(0.0, boxSize.z);
    atom->SetPosition({ distX(rng), distY(rng), distZ(rng) });

    // Maxwell-Boltzmann velocity at the given temperature
    double mass = atom->GetMass();
    Vec3 vel{
        RandomVelocityComponent(mass, temperature, rng),
        RandomVelocityComponent(mass, temperature, rng),
        RandomVelocityComponent(mass, temperature, rng)
    };
    atom->SetVelocity(vel);

    return atom;
}

double ParticleFactory::RandomVelocityComponent(double mass, double temperature, std::mt19937& rng) {
    if (temperature <= 0.0 || mass <= 0.0) return 0.0;

    // Standard deviation of velocity component from Maxwell-Boltzmann:
    // sigma = sqrt(kB * T / m)
    // Using kB in kJ/(mol*K) and mass in u (g/mol), we get velocity in
    // sqrt(kJ/g) = sqrt(1000 J / 0.001 kg) = sqrt(1e6 m^2/s^2) = 1000 m/s
    // But since we're in simplified units, we use the dimensionless form.
    double sigma = std::sqrt(ChemistryConfig::kB * temperature / mass);

    std::normal_distribution<double> dist(0.0, sigma);
    return dist(rng);
}

} // namespace Chemistry
