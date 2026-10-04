#pragma once
#include "string"
#include "vector"

namespace Slipper {
namespace Dbapi {

class Dbapi {
public:
    virtual ~Dbapi() = default;

    // Returns a list of all Category/Package strings in the database
    virtual std::vector< std::string > cp_all() const = 0;
    
    // Returns a list of all installed versions (CPVs) for a given Category/Package
    virtual std::vector< std::string > cp_list(const std::string& cp) const = 0;
    
    // Retrieves specific metadata values (e.g. "EAPI", "USE") for a specific CPV
    virtual std::vector< std::string > aux_get(const std::string& cpv, const std::vector< std::string >& wants) const = 0;

    // Returns all CPVs in the database by combining cp_all and cp_list
    virtual std::vector< std::string > cpv_all() const {
        std::vector< std::string > results;
        for (const auto& cp : cp_all()) {
            auto versions = cp_list(cp);
            results.insert(results.end(), versions.begin(), versions.end());
        }
        return results;
    }
};

} // namespace Dbapi
} // namespace Slipper