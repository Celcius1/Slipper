#pragma once
#include "memory"
#include "string"
#include "vector"
#include "functional"
#include "UseManager.hpp"
#include "MaskManager.hpp"
#include "KeywordsManager.hpp"
#include "LicenseManager.hpp"

namespace Slipper {
namespace Config {

class ConfigLoader {
public:
    // Parses /etc/portage/ make.conf and package.* files into the respective managers
    static void loadSystemConfig(
        std::shared_ptr< UseManager > use_mgr,
        std::shared_ptr< MaskManager > mask_mgr,
        std::shared_ptr< KeywordsManager > kw_mgr,
        std::shared_ptr< LicenseManager > lic_mgr,
        const std::string& config_root = "/etc/portage"
    );

private:
    // Helper to handle both flat files and directories, extracting valid configuration lines
    static void processPath(const std::string& path, std::function< void(const std::string&) > line_processor);
    
    // Helper to split a space-separated string into a vector of tokens
    static std::vector< std::string > tokenize(const std::string& str);
};

} // namespace Config
} // namespace Slipper