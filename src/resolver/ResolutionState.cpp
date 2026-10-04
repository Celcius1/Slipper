#include "../../include/resolver/ResolutionState.hpp"
#include "../../include/Logger.hpp"
#include "cstdlib" // For std::getenv

using namespace Slipper::Resolver;

// ---------------------------------------------------------
// ResolutionState Constructor
// ---------------------------------------------------------
ResolutionState::ResolutionState() {
    if (std::getenv("DEBUG")) {
        Logger::logDebug("ResolutionState::Constructor", "Spawning fresh resolution state for new graph calculation.");
    }
}

// ---------------------------------------------------------
// queueDependency
// Adds an atom to the primary processing stack.
// ---------------------------------------------------------
void ResolutionState::queueDependency(const Slipper::Dep::Atom& atom) {
    if (std::getenv("DEBUG")) {
        Logger::logDebug("ResolutionState::queueDependency", "Pushing atom to dependency stack: " + atom.getRawString());
    }
    dep_stack.push(atom);
}

// ---------------------------------------------------------
// hasPendingDependencies
// Returns true if either the primary or disjunctive stack has work.
// ---------------------------------------------------------
bool ResolutionState::hasPendingDependencies() const {
    bool pending = !dep_stack.empty() || !dep_disjunctive_stack.empty();
    
    if (std::getenv("DEBUG")) {
        Logger::logDebug("ResolutionState::hasPendingDependencies", 
            "Pending check: Primary Stack=" + std::to_string(dep_stack.size()) + 
            ", Disjunctive Stack=" + std::to_string(dep_disjunctive_stack.size()));
    }
    
    return pending;
}