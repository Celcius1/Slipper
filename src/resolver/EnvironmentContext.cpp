#include "../../include/resolver/EnvironmentContext.hpp"
#include "../../include/Logger.hpp"
#include "cstdlib" 

using namespace Slipper::Resolver;

EnvironmentContext::EnvironmentContext(
    std::shared_ptr< Dbapi::VarDbApi > v,
    std::shared_ptr< Dbapi::PortDbApi > p,
    std::shared_ptr< Dbapi::BinDbApi > b,
    std::shared_ptr< Config::UseManager > u,
    std::shared_ptr< Config::MaskManager > m,
    std::shared_ptr< Config::KeywordsManager > k,
    std::shared_ptr< Config::LicenseManager > l
) : vardb(v), portdb(p), bindb(b), use_manager(u), mask_manager(m), keywords_manager(k), license_manager(l) {
    
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("EnvironmentContext::Constructor", "Initialising immutable environment context.");
        Logger::logDebug("EnvironmentContext::Constructor", "Database and Config APIs successfully mapped to daemon context.");
    }
}