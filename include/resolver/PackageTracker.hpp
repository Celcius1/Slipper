#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "optional"
#include "../dep/Atom.hpp"

namespace Slipper {
namespace Resolver {

class PackageTracker {
public:
    PackageTracker() = default;

    void addPkg(const std::string& cpv);
    void removePkg(const std::string& cpv);
    bool contains(const std::string& cpv) const;
    std::vector< std::string > match(const Slipper::Dep::Atom& atom) const;
    
    // NEW: Extracts the entire flattened list of tracked packages
    std::vector< std::string > getAllPackages() const;

private:
    std::unordered_map< std::string, std::vector< std::string > > cp_map;
};

} // namespace Resolver
} // namespace Slipper