#pragma once
#include "Dbapi.hpp"
#include "string"
#include "vector"

namespace Slipper {
namespace Dbapi {

class PortDbApi : public Dbapi {
public:
    // Initializes the repository database with a list of active repository paths
    explicit PortDbApi(const std::vector< std::string >& porttrees);

    std::vector< std::string > cp_all() const override;
    std::vector< std::string > cp_list(const std::string& cp) const override;
    std::vector< std::string > aux_get(const std::string& cpv, const std::vector< std::string >& wants) const override;

private:
    std::vector< std::string > porttrees;
};

} // namespace Dbapi
} // namespace Slipper