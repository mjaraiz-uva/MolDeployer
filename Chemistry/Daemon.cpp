#include "Daemon.h"
#include "Atom.h"
#include "Bond.h"
#include "Molecule.h"
#include "SpatialGrid.h"
#include "ChemistryConfig.h"
#include "Reactor.h"
#include "../DataManager/DataManager.h"

namespace Chemistry {

Daemon::Daemon(int id, const Recipe* recipe, int creationStep, int timeoutSteps)
	: Particle(id), m_recipe(recipe),
	  m_creationStep(creationStep), m_timeoutSteps(timeoutSteps),
	  m_lastSuccessStep(creationStep) {
	// Daemons are very light — they move fast to search for components
	m_mass = 0.1;
}

void Daemon::StepActivity(double dt) {
	// Brownian motion identical to atoms but with configurable speed multiplier
	// (mass is very low, so thermal velocity sigma = sqrt(kB*T/m) is already high)
	m_position = m_position + m_velocity * dt;
}

bool Daemon::ShouldDie(int currentStep) const {
	return (currentStep - m_lastSuccessStep) >= m_timeoutSteps;
}

void Daemon::RecordSuccess(int step) {
	m_lastSuccessStep = step;
	m_successCount++;
}

std::string Daemon::GetAtomFormula(const Atom* atom) {
	if (!atom) return "";
	if (atom->IsFree()) {
		return ElementSymbol(atom->GetElement());
	}
	Molecule* mol = atom->GetMolecule();
	if (mol) {
		return mol->GetMolecularFormula();
	}
	return ElementSymbol(atom->GetElement());
}

Atom* Daemon::FindBondableAtom(Atom* anchor) {
	if (!anchor) return nullptr;
	if (anchor->CanBond()) return anchor;

	Molecule* mol = anchor->GetMolecule();
	if (mol) {
		for (Atom* a : mol->GetAtoms()) {
			if (a->CanBond()) return a;
		}
	}
	return nullptr;
}

bool Daemon::TryAssemble(const SpatialGrid& grid,
                          const std::vector<std::unique_ptr<Atom>>& atoms,
                          double cutoff, int currentStep,
                          Atom*& outAtomA, Atom*& outAtomB) {
	outAtomA = nullptr;
	outAtomB = nullptr;

	if (m_state != State::SEARCHING) return false;

	const std::string& compA = m_recipe->componentA;
	const std::string& compB = m_recipe->componentB;

	// Find atoms near the daemon's position.
	// We check all atoms and filter by distance to the daemon (not to each other).
	// This is a simple O(N) scan — could be optimized with a spatial query on daemon position,
	// but for now we iterate atoms and check distance to daemon.
	double cutoffSq = cutoff * cutoff;

	Atom* foundA = nullptr;
	Atom* foundB = nullptr;

	for (const auto& atomPtr : atoms) {
		Atom* atom = atomPtr.get();
		if (!atom) continue;

		// Check distance from daemon to this atom
		Vec3 diff = atom->GetPosition() - m_position;
		if (diff.lengthSq() > cutoffSq) continue;

		// Determine this atom's formula identity
		std::string formula = GetAtomFormula(atom);

		// Try to match component A
		if (!foundA && formula == compA) {
			Atom* bondable = FindBondableAtom(atom);
			if (bondable) {
				foundA = atom;  // Keep reference to anchor for molecule identity
			}
		}
		// Try to match component B (must be a different entity than A)
		else if (!foundB && formula == compB) {
			// Ensure it's not the same molecule as foundA
			if (foundA) {
				bool sameEntity = false;
				if (!foundA->IsFree() && !atom->IsFree() &&
				    foundA->GetMolecule() == atom->GetMolecule()) {
					sameEntity = true;
				}
				if (foundA == atom) sameEntity = true;
				if (sameEntity) continue;
			}
			Atom* bondable = FindBondableAtom(atom);
			if (bondable) {
				foundB = atom;
			}
		}

		if (foundA && foundB) break;
	}

	if (!foundA || !foundB) return false;

	// Find the actual atoms to bond (with free valence) in each component
	outAtomA = FindBondableAtom(foundA);
	outAtomB = FindBondableAtom(foundB);

	if (!outAtomA || !outAtomB) return false;

	// Don't form a bond that already exists
	if (outAtomA->GetBondTo(outAtomB)) return false;

	return true;
}

std::unique_ptr<Daemon> Daemon::SpawnOffspring(int newId, int currentStep,
                                                const Vec3& boxSize, std::mt19937& rng) const {
	auto offspring = std::make_unique<Daemon>(newId, m_recipe, currentStep, m_timeoutSteps);

	// Place offspring near parent with a small random offset
	std::uniform_real_distribution<double> offset(-5.0, 5.0);
	Vec3 newPos = m_position + Vec3(offset(rng), offset(rng), offset(rng));

	// Clamp to box
	newPos.x = (newPos.x < 0) ? 0 : (newPos.x > boxSize.x ? boxSize.x : newPos.x);
	newPos.y = (newPos.y < 0) ? 0 : (newPos.y > boxSize.y ? boxSize.y : newPos.y);
	newPos.z = (newPos.z < 0) ? 0 : (newPos.z > boxSize.z ? boxSize.z : newPos.z);
	offspring->SetPosition(newPos);

	// Random velocity (thermal)
	double temperature = DataManager::GetConfigParameters().temperature;
	double sigma = std::sqrt(ChemistryConfig::kB * temperature / offspring->GetMass());
	std::normal_distribution<double> velDist(0.0, sigma);
	offspring->SetVelocity(Vec3(velDist(rng), velDist(rng), velDist(rng)));

	return offspring;
}

nlohmann::json Daemon::Save() const {
	nlohmann::json j;
	j["id"] = m_id;
	j["targetFormula"] = m_recipe->targetFormula;
	j["componentA"] = m_recipe->componentA;
	j["componentB"] = m_recipe->componentB;
	j["energyGain"] = m_recipe->energyGain;
	j["creationStep"] = m_creationStep;
	j["timeoutSteps"] = m_timeoutSteps;
	j["lastSuccessStep"] = m_lastSuccessStep;
	j["successCount"] = m_successCount;
	j["state"] = static_cast<int>(m_state);
	j["px"] = m_position.x;
	j["py"] = m_position.y;
	j["pz"] = m_position.z;
	j["vx"] = m_velocity.x;
	j["vy"] = m_velocity.y;
	j["vz"] = m_velocity.z;
	return j;
}

} // namespace Chemistry
