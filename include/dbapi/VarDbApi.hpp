#pragma once
#include "Dbapi.hpp"
#include "string"
#include "vector"

namespace Slipper {
namespace Dbapi {

class VarDbApi : public Dbapi {
public:
    // Initializes the VDB path using the system's root (EPREFIX + /var/db/pkg)
    explicit VarDbApi(const std::string& eroot);

    std::vector< std::string > cp_all() const override;
    std::vector< std::string > cp_list(const std::string& cp) const override;
    std::vector< std::string > aux_get(const std::string& cpv, const std::vector< std::string >& wants) const override;

private:
    std::string dbroot;
};

} // namespace Dbapi
} // namespace Slipper