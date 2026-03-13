#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include "Particle.h"
#include "Recipe.h"

namespace Chemistry {

// Forward declarations
class Atom;
class SpatialGrid;

// A Daemon is an independent spatial agent that specializes in one reaction (Recipe).
// It moves through the simulation box searching for its two input components (atoms or
// molecules), and when both are found nearby, it catalyzes the bond formation.
//
// Analogous to a Worker/Producer/Firm in the DEPX economic model:
// - Has its own position and velocity (moves independently of atoms)
// - Specializes in one "product" (target molecule)
// - Searches for "inputs" (two component atoms/molecules) in its spatial neighborhood
// - On success: reproduces (spawns a copy nearby) — "assisted production"
// - On timeout without success: dies — Darwinian selection pressure
//
// Atoms and molecules are GOODS. Daemons are the PRODUCERS.
class Daemon : public Particle {
public:
	enum class State { SEARCHING, DEAD };

	Daemon(int id, const Recipe* recipe, int creationStep, int timeoutSteps);

	// Per-timestep: Brownian motion (lighter than atoms, moves faster)
	void StepActivity(double dt) override;

	// Search neighborhood and attempt assembly.
	// Returns true if successful. outAtomA/outAtomB = atoms to bond.
	bool TryAssemble(const SpatialGrid& grid,
	                 const std::vector<std::unique_ptr<Atom>>& atoms,
	                 double cutoff, int currentStep,
	                 Atom*& outAtomA, Atom*& outAtomB);

	// Check if daemon should die (timeout since last success or creation)
	bool ShouldDie(int currentStep) const;

	// Record a successful assembly
	void RecordSuccess(int step);

	// Factory: create a copy nearby (reproduction on success)
	std::unique_ptr<Daemon> SpawnOffspring(int newId, int currentStep,
	                                        const Vec3& boxSize, std::mt19937& rng) const;

	// Accessors
	const Recipe* GetRecipe() const { return m_recipe; }
	int GetSuccessCount() const { return m_successCount; }
	State GetState() const { return m_state; }
	void SetState(State s) { m_state = s; }
	int GetCreationStep() const { return m_creationStep; }
	int GetLastSuccessStep() const { return m_lastSuccessStep; }
	int GetTimeoutSteps() const { return m_timeoutSteps; }
	const std::string& GetTargetFormula() const { return m_recipe->targetFormula; }

	// Snapshot persistence
	nlohmann::json Save() const;

private:
	const Recipe* m_recipe;       // Specialization (non-owning, points into RecipeBook)
	int m_creationStep;
	int m_timeoutSteps;
	int m_lastSuccessStep;        // Step of last successful assembly (or creation step)
	int m_successCount = 0;
	State m_state = State::SEARCHING;

	// Get the molecular formula for an atom (element symbol if free, molecule formula if bonded)
	static std::string GetAtomFormula(const Atom* atom);

	// Find an atom with free valence in the molecule containing 'anchor'
	static Atom* FindBondableAtom(Atom* anchor);
};

} // namespace Chemistry
