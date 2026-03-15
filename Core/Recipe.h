#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <random>

namespace DABM {

// A Recipe defines a binary production rule: componentA + componentB -> targetFormula.
// Chemistry: H + H -> H2, or OH + H -> H2O.
// Economics: wood + workers -> furniture (ConsumedPerUnit in Deployers).
// Robotics: assemble_leftframe + get_right -> set_right_on_leftframe.
struct Recipe {
    std::string targetFormula;   // output product name
    std::string componentA;      // input 1
    std::string componentB;      // input 2
    double energyGain = 0.0;     // benefit of this transformation (bond energy, profit, etc.)
};

// Registry of all known production recipes.
// Analogous to the SAM (Social Accounting Matrix) in DEPLOYERS.
class RecipeBook {
public:
    void AddRecipe(const Recipe& recipe);
    void BuildIndices();

    // Get all recipes that produce a given target
    const std::vector<Recipe>& GetRecipesFor(const std::string& targetFormula) const;

    // Get all recipes that USE a given formula as an input component (A or B)
    const std::vector<const Recipe*>& GetRecipesUsing(const std::string& formula) const;

    // Get list of all target formulas that have recipes
    const std::vector<std::string>& GetAllTargets() const { return m_allTargets; }

    // Pick a random recipe from the entire book
    const Recipe& GetRandomRecipe(std::mt19937& rng) const;

    // Total number of recipes
    size_t GetTotalRecipeCount() const { return m_allRecipes.size(); }

    bool IsEmpty() const { return m_allRecipes.empty(); }

private:
    std::unordered_map<std::string, std::vector<Recipe>> m_recipesByTarget;
    std::vector<Recipe> m_allRecipes;
    std::vector<std::string> m_allTargets;
    std::unordered_map<std::string, std::vector<const Recipe*>> m_recipesByComponent;

    static const std::vector<Recipe> s_emptyRecipes;
    static const std::vector<const Recipe*> s_emptyRecipePtrs;
};

} // namespace DABM
