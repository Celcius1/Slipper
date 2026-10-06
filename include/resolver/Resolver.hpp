#pragma once
#include "memory"
#include "string"
#include "vector"
#include "unordered_set"
#include "EnvironmentContext.hpp"
#include "ResolutionState.hpp"
#include "../dep/Atom.hpp"

namespace Slipper {
namespace Resolver {

struct ParsedDeps {
    std::vector< Slipper::Dep::Atom > mandatory;
    std::vector< std::vector< Slipper::Dep::Atom > > disjunctive;
    std::vector< Slipper::Dep::Atom > idepend; // EAPI 8 Install-time dependencies
};

class Resolver {
public:
    explicit Resolver(std::shared_ptr< EnvironmentContext > env);

    // Resolves a single target
    bool resolve(ResolutionState& state, const Slipper::Dep::Atom& root_atom);
    
    // OVERLOAD: Resolves a set of targets (e.g., @world)
    bool resolve(ResolutionState& state, const std::vector< Slipper::Dep::Atom >& root_atoms);

private:
    std::shared_ptr< EnvironmentContext > env_context;

    bool createGraph(ResolutionState& state);
    std::string getBestVisible(const Slipper::Dep::Atom& atom) const;
    ParsedDeps parseDependencies(const std::string& dep_string, const std::unordered_set< std::string >& active_use) const;
};

} // namespace Resolver
} // namespace Slipper