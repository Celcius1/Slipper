#pragma once
#include "string"
#include "vector"
#include "optional"

namespace Slipper {
namespace Repository {

class RepoConfig {
public:
    RepoConfig() = default;
    explicit RepoConfig(const std::string& name) : name(name) {}

    // Core attributes mapped from repos.conf
    std::string name;
    std::optional< std::string > location;
    std::optional< std::string > sync_type;
    std::optional< std::string > sync_uri;
    bool auto_sync = true;
    int priority = 0;

    // Output formatted info for debugging (similar to Portage's info_string)
    std::string toString() const {
        std::string out = "[" + name + "]\n";
        if (location) out += "  location: " + location.value() + "\n";
        if (sync_type) out += "  sync-type: " + sync_type.value() + "\n";
        if (sync_uri) out += "  sync-uri: " + sync_uri.value() + "\n";
        out += "  priority: " + std::to_string(priority) + "\n";
        out += "  auto-sync: " + std::string(auto_sync ? "yes" : "no") + "\n";
        return out;
    }
};

} // namespace Repository
} // namespace Slipper