#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "unordered_set"
#include "../dep/Atom.hpp"

namespace Slipper {
namespace Config {

class UseManager {
public:
    UseManager() = default;

    // Registers a line from package.use (e.g., atom_str=">=sys-apps/portage-3.0.4", flags={"ipc", "-native-extensions"})
    void addPackageUse(const std::string& atom_str, const std::vector< std::string >& flags);

    // Resolves the final stacked USE flags for a given package by simulating ordered_by_atom_specificity
    std::unordered_set< std::string > getPUSE(const Slipper::Dep::Atom& pkg) const;

private:
    // Maps a category/package (CP) to a list of specific atoms and their applied flags.
    // This directly mirrors the self._pusedict structure in Portage.
    std::unordered_map< std::string, std::vector< std::pair< Slipper::Dep::Atom, std::vector< std::string > > > > puse_dict;
};

} // namespace Config
} // namespace Slipper