#pragma once
#include "stack"
#include "vector"
#include "string"
#include "PackageTracker.hpp"
#include "../dep/Atom.hpp"

namespace Slipper {
namespace Resolver {

// ---------------------------------------------------------
// ResolutionState
// Mutable state container for a single dependency resolution pass.
// Discarded and rebuilt if conflict backtracking is required.
// ---------------------------------------------------------
struct ResolutionState {
    // Tracks packages selected for the current graph to prevent slot collisions
    PackageTracker tracker;

    // Stack of packages that still need their dependencies evaluated
    std::stack< Slipper::Dep::Atom > dep_stack;

    // Stack of disjunctive/OR dependencies (e.g., || ( a b )) waiting for evaluation
    std::stack< std::vector< Slipper::Dep::Atom > > dep_disjunctive_stack;

    // Records conflict reasons if the graph hits a dead end (used for backtracking)
    std::vector< std::string > backtrack_infos;

    ResolutionState();

    // Pushes a new atom onto the evaluation stack
    void queueDependency(const Slipper::Dep::Atom& atom);
    
    // Checks if there are pending dependencies to evaluate
    bool hasPendingDependencies() const;
};

} // namespace Resolver
} // namespace Slipper