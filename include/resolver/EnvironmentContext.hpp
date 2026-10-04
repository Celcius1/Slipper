#pragma once
#include "memory"
#include "string"
#include "vector"
#include "../dbapi/VarDbApi.hpp"
#include "../dbapi/PortDbApi.hpp"
#include "../dbapi/BinDbApi.hpp"
#include "../config/UseManager.hpp"
#include "../config/MaskManager.hpp"
#include "../config/KeywordsManager.hpp"
#include "../config/LicenseManager.hpp"

namespace Slipper {
namespace Resolver {

struct EnvironmentContext {
    std::shared_ptr< Dbapi::VarDbApi > vardb;
    std::shared_ptr< Dbapi::PortDbApi > portdb;
    std::shared_ptr< Dbapi::BinDbApi > bindb;
    std::shared_ptr< Config::UseManager > use_manager;
    std::shared_ptr< Config::MaskManager > mask_manager;
    std::shared_ptr< Config::KeywordsManager > keywords_manager;
    std::shared_ptr< Config::LicenseManager > license_manager;

    EnvironmentContext(
        std::shared_ptr< Dbapi::VarDbApi > v,
        std::shared_ptr< Dbapi::PortDbApi > p,
        std::shared_ptr< Dbapi::BinDbApi > b,
        std::shared_ptr< Config::UseManager > u,
        std::shared_ptr< Config::MaskManager > m,
        std::shared_ptr< Config::KeywordsManager > k,
        std::shared_ptr< Config::LicenseManager > l
    );
};

} // namespace Resolver
} // namespace Slipper