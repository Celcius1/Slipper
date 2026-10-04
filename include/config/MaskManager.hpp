#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "optional"
#include "../dep/Atom.hpp"

namespace Slipper {
namespace Config {

class MaskManager {
public:
    MaskManager() = default;

    // Registers a masking atom from package.mask
    void addMask(const std::string& atom_str);

    // Registers an unmasking atom from package.unmask
    void addUnmask(const std::string& atom_str);

    // Evaluates if a package is masked.
    // Returns the masking Atom if blocked, or std::nullopt if permitted.
    std::optional< Slipper::Dep::Atom > getMaskAtom(const Slipper::Dep::Atom& pkg) const;

private:
    // Maps a Category/Package to a list of masking atoms
    std::unordered_map< std::string, std::vector< Slipper::Dep::Atom > > pmask_dict;
    
    // Maps a Category/Package to a list of unmasking atoms
    std::unordered_map< std::string, std::vector< Slipper::Dep::Atom > > punmask_dict;
};

} // namespace Config
} // namespace Slipper