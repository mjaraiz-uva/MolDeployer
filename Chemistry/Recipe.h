#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <random>

namespace Chemistry {

// A Recipe defines a binary decomposition of a target molecule into exactly 2 components.
// Analogous to IC coefficients in the DEPX economic model's Firm production system.
// Example: H2O = OH + H, or H2O = O + H2
struct Recipe {
	std::string targetFormula;   // e.g. "H2O" (Hill system)
	std::string componentA;      // e.g. "HO"  (Hill system)
	std::string componentB;      // e.g. "H"
	double energyGain;           // Bond energy released by forming the new bond (kJ/mol)
};

// Singleton registry of all known binary decomposition recipes.
// Analogous to the SAM (Social Accounting Matrix) in the DEPX economic model.
class RecipeBook {
public:
	static RecipeBook& GetInstance();

	// Get all recipes that produce a given target formula
	const std::vector<Recipe>& GetRecipesFor(const std::string& targetFormula) const;

	// Get all recipes that USE a given formula as an input component (A or B)
	const std::vector<const Recipe*>& GetRecipesUsing(const std::string& formula) const;

	// Get list of all target formulas that have recipes
	const std::vector<std::string>& GetAllTargets() const { return m_allTargets; }

	// Pick a random recipe from the entire book (uniform over all recipes)
	const Recipe& GetRandomRecipe(std::mt19937& rng) const;

	// Total number of recipes
	size_t GetTotalRecipeCount() const { return m_allRecipes.size(); }

private:
	RecipeBook();
	void InitDefaultRecipes();

	// Map: targetFormula -> list of recipes
	std::unordered_map<std::string, std::vector<Recipe>> m_recipesByTarget;

	// Flat list of all recipes (for random selection)
	std::vector<Recipe> m_allRecipes;

	// Sorted list of all target formulas
	std::vector<std::string> m_allTargets;

	// Reverse index: formula -> recipes that use it as componentA or componentB
	std::unordered_map<std::string, std::vector<const Recipe*>> m_recipesByComponent;

	// Empty vectors for lookups when key not found
	static const std::vector<Recipe> s_emptyRecipes;
	static const std::vector<const Recipe*> s_emptyRecipePtrs;
};

} // namespace Chemistry
