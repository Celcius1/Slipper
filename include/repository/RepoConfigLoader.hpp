#pragma once
#include "string"
#include "map"
#include "RepoConfig.hpp"

namespace Slipper {
namespace Repository {

class RepoConfigLoader {
public:
    // Parses an INI-formatted string or file stream
    void loadFromString(const std::string& config_content);
    
    // Retrieve the loaded repositories
    const std::map< std::string, RepoConfig >& getRepos() const { return repos; }

private:
    std::map< std::string, RepoConfig > repos;
    
    // Helper to trim whitespace from strings
    static std::string trim(const std::string& str);
};

} // namespace Repository
} // namespace Slipper