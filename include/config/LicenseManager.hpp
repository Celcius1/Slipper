#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "unordered_set"
#include "../dep/Atom.hpp"

namespace Slipper {
namespace Config {

class LicenseManager {
public:
    LicenseManager() = default;

    // Registers a global license group (e.g., group="@FREE", licenses={"GPL-2", "MIT"})
    void addLicenseGroup(const std::string& group_name, const std::vector< std::string >& licenses);

    // Registers a line from package.license (e.g., atom_str="www-client/google-chrome", licenses={"google-chrome"})
    void addPackageLicense(const std::string& atom_str, const std::vector< std::string >& licenses);

    // Resolves the accepted licenses for a specific package, expanding groups as necessary
    std::unordered_set< std::string > getAcceptedLicenses(const Slipper::Dep::Atom& pkg) const;

private:
    // Expands a single token, mapping e.g., "@FREE" to its constituent licenses
    std::vector< std::string > expandLicenseToken(const std::string& token) const;

    // Maps a license group (e.g., "@FREE") to its licenses
    std::unordered_map< std::string, std::vector< std::string > > license_groups;
    
    // Maps a Category/Package to specific atoms and their accepted licenses
    std::unordered_map< std::string, std::vector< std::pair< Slipper::Dep::Atom, std::vector< std::string > > > > plicense_dict;
};

} // namespace Config
} // namespace Slipper