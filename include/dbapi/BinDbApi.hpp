#pragma once
#include "Dbapi.hpp"
#include "string"
#include "vector"

namespace Slipper {
namespace Dbapi {

class BinDbApi : public Dbapi {
public:
    // Initializes the binary package database with the PKGDIR path
    explicit BinDbApi(const std::string& pkgdir);

    std::vector< std::string > cp_all() const override;
    std::vector< std::string > cp_list(const std::string& cp) const override;
    std::vector< std::string > aux_get(const std::string& cpv, const std::vector< std::string >& wants) const override;

private:
    std::string pkgdir;
};

} // namespace Dbapi
} // namespace Slipper