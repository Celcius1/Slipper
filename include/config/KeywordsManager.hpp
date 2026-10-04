#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "../dep/Atom.hpp"

namespace Slipper {
namespace Config {

class KeywordsManager {
public:
    KeywordsManager() = default;

    // Registers a line from package.accept_keywords (e.g., atom_str="sys-kernel/gentoo-sources", keywords={"~amd64"})
    void addKeyword(const std::string& atom_str, const std::vector< std::string >& keywords);

    // Resolves the accepted keywords for a specific package
    std::vector< std::string > getKeywords(const Slipper::Dep::Atom& pkg) const;

private:
    // Maps a Category/Package to a list of specific atoms and their applied keywords
    std::unordered_map< std::string, std::vector< std::pair< Slipper::Dep::Atom, std::vector< std::string > > > > pkeywords_dict;
};

} // namespace Config
} // namespace Slipper