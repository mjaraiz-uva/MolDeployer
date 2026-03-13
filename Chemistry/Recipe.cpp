#include "Recipe.h"
#include "ChemistryConfig.h"
#include <algorithm>

namespace Chemistry {

const std::vector<Recipe> RecipeBook::s_emptyRecipes;

RecipeBook& RecipeBook::GetInstance() {
	static RecipeBook instance;
	return instance;
}

RecipeBook::RecipeBook() {
	InitDefaultRecipes();
}

void RecipeBook::InitDefaultRecipes() {
	// Helper to add a recipe
	auto add = [this](const std::string& target, const std::string& a, const std::string& b, double energy) {
		Recipe r{ target, a, b, energy };
		m_recipesByTarget[target].push_back(r);
		m_allRecipes.push_back(r);
	};

	// Bond energies from ChemistryConfig (kJ/mol):
	// C-H=411, O-H=459, C-O=358, C=O=799, H-H=436, C-C=346, O=O=498

	// --- Diatomic molecules ---
	add("H2", "H", "H", 436.0);        // H + H -> H2
	add("HO", "O", "H", 459.0);        // O + H -> OH
	add("CO", "C", "O", 358.0);        // C + O -> CO (single bond)

	// --- Triatomic molecules ---
	add("H2O", "HO", "H", 459.0);      // OH + H -> H2O
	add("H2O", "O", "H2", 459.0);      // O + H2 -> H2O (alternative)

	add("CO2", "CO", "O", 799.0);      // CO + O -> CO2 (forms C=O double bond)

	// --- Methane synthesis chain ---
	add("CH", "C", "H", 411.0);        // C + H -> CH
	add("CH2", "CH", "H", 411.0);      // CH + H -> CH2
	add("CH3", "CH2", "H", 411.0);     // CH2 + H -> CH3
	add("CH4", "CH3", "H", 411.0);     // CH3 + H -> CH4

	// --- Carbon-oxygen-hydrogen ---
	add("CHO", "CO", "H", 411.0);      // CO + H -> CHO (formyl)
	add("CH2O", "CHO", "H", 411.0);    // CHO + H -> CH2O (formaldehyde)
	add("CHO2", "CO", "HO", 358.0);    // CO + OH -> COOH (formic acid)

	// --- C-C bond formation ---
	add("C2", "C", "C", 346.0);        // C + C -> C2

	// Build sorted target list
	m_allTargets.clear();
	for (const auto& [target, _] : m_recipesByTarget) {
		m_allTargets.push_back(target);
	}
	std::sort(m_allTargets.begin(), m_allTargets.end());
}

const std::vector<Recipe>& RecipeBook::GetRecipesFor(const std::string& targetFormula) const {
	auto it = m_recipesByTarget.find(targetFormula);
	if (it != m_recipesByTarget.end()) {
		return it->second;
	}
	return s_emptyRecipes;
}

const Recipe& RecipeBook::GetRandomRecipe(std::mt19937& rng) const {
	std::uniform_int_distribution<size_t> dist(0, m_allRecipes.size() - 1);
	return m_allRecipes[dist(rng)];
}

} // namespace Chemistry
