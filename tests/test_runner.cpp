#include "iostream"
#include "string"
#include "vector"
#include "memory"
#include "filesystem"
#include "fstream"

#include "../include/Logger.hpp"
#include "../include/dep/Atom.hpp"
#include "../include/dep/soname/Parser.hpp"
#include "../include/Versions.hpp"
#include "../include/repository/RepoConfigLoader.hpp"
#include "../include/config/UseManager.hpp"
#include "../include/config/MaskManager.hpp"
#include "../include/config/KeywordsManager.hpp"
#include "../include/config/LicenseManager.hpp"
#include "../include/dbapi/VarDbApi.hpp"
#include "../include/dbapi/PortDbApi.hpp"
#include "../include/dbapi/BinDbApi.hpp"
#include "../include/resolver/PackageTracker.hpp"
#include "../include/resolver/ConflictHandlers.hpp"

void testAtomParser() {
    Logger::logDebug("testAtomParser", "Entering routine: testAtomParser.");
    std::string test_string = ">=sys-apps/portage-3.0.4:0/3=[ipc,native-extensions]::gentoo";
    Logger::logInfo("testAtomParser", "Testing Atom Parser with string: " + test_string);
    
    try {
        Slipper::Dep::Atom test_atom(test_string);
        std::cout << "Category: " << test_atom.getCategory() << std::endl;
        std::cout << "Package: " << test_atom.getPackageName() << std::endl;
        std::cout << "Version: " << test_atom.getVersion().value_or("None") << std::endl;
        std::cout << "Slot: " << test_atom.getSlot().value_or("None") << std::endl;
        std::cout << "Sub-Slot: " << test_atom.getSubSlot().value_or("None") << std::endl;
        std::cout << "Repo: " << test_atom.getRepo().value_or("None") << std::endl;
        std::cout << "USE Deps:" << std::endl;
        for (const auto& flag : test_atom.getUseDeps()) {
            std::cout << " - " << flag << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testAtomParser", std::string("Parser threw an exception: ") + e.what());
    }
    Logger::logDebug("testAtomParser", "Exiting routine: testAtomParser.");
}

void testSonameParser() {
    Logger::logDebug("testSonameParser", "Entering routine: testSonameParser.");
    std::string test_string = "x86_64: libc.so.6 libm.so.6 x86_32: libc.so.6";
    Logger::logInfo("testSonameParser", "Testing Soname Parser with string: " + test_string);
    
    try {
        auto sonames = Slipper::Dep::Soname::parseSonameDeps(test_string);
        std::cout << "Successfully parsed Sonames:" << std::endl;
        for (const auto& sa : sonames) {
            std::cout << " - " << sa.toString() << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testSonameParser", std::string("Parser threw an exception: ") + e.what());
    }
    Logger::logDebug("testSonameParser", "Exiting routine: testSonameParser.");
}

void testVersionParser() {
    Logger::logDebug("testVersionParser", "Entering routine: testVersionParser.");
    std::vector< std::pair< std::string, std::string > > tests = {
        {"1.0-r1", "1.2-r3"}, 
        {"1.3", "1.2-r3"}, 
        {"1.0_p3", "1.0_p3"}, 
        {"1.0_pre2", "1.0_p1"}, 
        {"1.02", "1.1"}
    };

    Logger::logInfo("testVersionParser", "Running Portage version comparison test battery:");
    for (const auto& test : tests) {
        auto result = Slipper::Versions::vercmp(test.first, test.second);
        if (result.has_value()) {
            std::cout << " - Comparing " << test.first << " vs " << test.second 
                      << " -> Result: " << result.value() << std::endl;
        } else {
            std::cout << " - Comparing " << test.first << " vs " << test.second 
                      << " -> Result: INVALID SYNTAX" << std::endl;
        }
    }
    Logger::logDebug("testVersionParser", "Exiting routine: testVersionParser.");
}

void testRepoConfigParser() {
    Logger::logDebug("testRepoConfigParser", "Entering routine: testRepoConfigParser.");
    std::string mock_repos_conf = R"(
[DEFAULT]
main-repo = gentoo

[gentoo]
location = /var/db/repos/gentoo
sync-type = rsync
sync-uri = rsync://rsync.au.gentoo.org/gentoo-portage
auto-sync = yes
priority = -1000

[slipper-overlay]
location = /var/db/repos/slipper-overlay
sync-type = git
sync-uri = https://github.com/cel-tech-serv/slipper-overlay.git
priority = 50
    )";

    Logger::logInfo("testRepoConfigParser", "Testing RepoConfigLoader with mock repos.conf data");
    try {
        Slipper::Repository::RepoConfigLoader loader;
        loader.loadFromString(mock_repos_conf);
        auto repos = loader.getRepos();
        std::cout << "Successfully parsed " << repos.size() << " repository sections:" << std::endl;
        for (const auto& pair : repos) {
            std::cout << pair.second.toString() << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testRepoConfigParser", std::string("Parser threw an exception: ") + e.what());
    }
    Logger::logDebug("testRepoConfigParser", "Exiting routine: testRepoConfigParser.");
}

void testUseManager() {
    Logger::logDebug("testUseManager", "Entering routine: testUseManager.");
    Slipper::Config::UseManager use_manager;
    use_manager.addPackageUse("sys-apps/portage", {"ipc", "-bluetooth"});
    use_manager.addPackageUse(">=sys-apps/portage-3.0.4", {"native-extensions", "bluetooth"});

    Logger::logInfo("testUseManager", "Testing USE flag resolution for: sys-apps/portage-3.0.4");
    try {
        Slipper::Dep::Atom target_pkg("sys-apps/portage-3.0.4");
        auto active_flags = use_manager.getPUSE(target_pkg);
        std::cout << "Final Active USE Flags:" << std::endl;
        for (const auto& flag : active_flags) {
            std::cout << " + " << flag << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testUseManager", std::string("UseManager threw an exception: ") + e.what());
    }
    Logger::logDebug("testUseManager", "Exiting routine: testUseManager.");
}

void testMaskManager() {
    Logger::logDebug("testMaskManager", "Entering routine: testMaskManager.");
    Slipper::Config::MaskManager mask_manager;
    mask_manager.addMask(">=sys-kernel/gentoo-sources-6.1.0");
    mask_manager.addUnmask("=sys-kernel/gentoo-sources-6.1.12");

    Logger::logInfo("testMaskManager", "Testing Mask resolution for: sys-kernel/gentoo-sources-6.1.12");
    try {
        Slipper::Dep::Atom target_pkg("sys-kernel/gentoo-sources-6.1.12");
        auto mask_result = mask_manager.getMaskAtom(target_pkg);
        if (mask_result.has_value()) {
            std::cout << "Verdict: MASKED by " << mask_result.value().getRawString() << std::endl;
        } else {
            std::cout << "Verdict: PERMITTED (Unmasked)" << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testMaskManager", std::string("MaskManager threw an exception: ") + e.what());
    }
    Logger::logDebug("testMaskManager", "Exiting routine: testMaskManager.");
}

void testKeywordsManager() {
    Logger::logDebug("testKeywordsManager", "Entering routine: testKeywordsManager.");
    Slipper::Config::KeywordsManager keywords_manager;
    keywords_manager.addKeyword("sys-kernel/gentoo-sources", {"~amd64"});
    keywords_manager.addKeyword("=sys-kernel/gentoo-sources-9999", {"**"});

    Logger::logInfo("testKeywordsManager", "Testing Keyword resolution for: sys-kernel/gentoo-sources-9999");
    try {
        Slipper::Dep::Atom target_pkg("sys-kernel/gentoo-sources-9999");
        auto active_keywords = keywords_manager.getKeywords(target_pkg);
        std::cout << "Accepted Keywords:" << std::endl;
        for (const auto& kw : active_keywords) {
            std::cout << " + " << kw << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testKeywordsManager", std::string("KeywordsManager threw an exception: ") + e.what());
    }
    Logger::logDebug("testKeywordsManager", "Exiting routine: testKeywordsManager.");
}

void testLicenseManager() {
    Logger::logDebug("testLicenseManager", "Entering routine: testLicenseManager.");
    Slipper::Config::LicenseManager license_manager;
    license_manager.addLicenseGroup("@GPL-COMPATIBLE", {"GPL-2", "GPL-3", "LGPL-2.1"});
    license_manager.addPackageLicense("sys-kernel/gentoo-sources", {"@GPL-COMPATIBLE", "linux-fw"});
    license_manager.addPackageLicense("=sys-kernel/gentoo-sources-6.1.12", {"-linux-fw"});

    Logger::logInfo("testLicenseManager", "Testing License resolution for: sys-kernel/gentoo-sources-6.1.12");
    try {
        Slipper::Dep::Atom target_pkg("sys-kernel/gentoo-sources-6.1.12");
        auto active_licenses = license_manager.getAcceptedLicenses(target_pkg);
        std::cout << "Accepted Licenses:" << std::endl;
        for (const auto& lic : active_licenses) {
            std::cout << " + " << lic << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testLicenseManager", std::string("LicenseManager threw an exception: ") + e.what());
    }
    Logger::logDebug("testLicenseManager", "Exiting routine: testLicenseManager.");
}

void testVarDbApi() {
    Logger::logDebug("testVarDbApi", "Entering routine: testVarDbApi.");
    std::string mock_eroot = "./mock_root";
    std::string mock_vdb = mock_eroot + "/var/db/pkg";
    std::string mock_pkg = mock_vdb + "/sys-apps/portage-3.0.4";
    
    try {
        std::filesystem::create_directories(mock_pkg);
        std::ofstream eapi_file(mock_pkg + "/EAPI");
        eapi_file << "8\n";
        eapi_file.close();

        std::ofstream use_file(mock_pkg + "/USE");
        use_file << "ipc native-extensions xattr\n";
        use_file.close();
        
        Logger::logInfo("testVarDbApi", "Mock VDB created at " + mock_vdb);
        Slipper::Dbapi::VarDbApi vardb(mock_eroot);
        
        std::cout << "--- Testing VarDbApi cp_all() ---" << std::endl;
        for (const auto& cp : vardb.cp_all()) {
            std::cout << "Found CP: " << cp << std::endl;
        }

        std::cout << "--- Testing VarDbApi aux_get() ---" << std::endl;
        std::vector< std::string > wants = {"EAPI", "USE", "MISSING_KEY"};
        auto metadata = vardb.aux_get("sys-apps/portage-3.0.4", wants);
        
        for (size_t i = 0; i < wants.size(); ++i) {
            std::cout << "Metadata [" << wants[i] << "]: " << (metadata[i].empty() ? "" : metadata[i]) << std::endl;
        }
        std::filesystem::remove_all(mock_eroot);
    } catch (const std::exception& e) {
        Logger::logInfo("testVarDbApi", std::string("VarDbApi threw an exception: ") + e.what());
    }
    Logger::logDebug("testVarDbApi", "Exiting routine: testVarDbApi.");
}

void testPortDbApi() {
    Logger::logDebug("testPortDbApi", "Entering routine: testPortDbApi.");
    std::string mock_repo = "./mock_repo";
    std::string mock_pkg_dir = mock_repo + "/sys-apps/portage";
    std::string mock_cache_dir = mock_repo + "/metadata/md5-cache/sys-apps";
    
    try {
        std::filesystem::create_directories(mock_pkg_dir);
        std::filesystem::create_directories(mock_cache_dir);
        
        std::ofstream ebuild_file(mock_pkg_dir + "/portage-3.0.4.ebuild");
        ebuild_file << "# Mock ebuild\n";
        ebuild_file.close();

        std::ofstream cache_file(mock_cache_dir + "/portage-3.0.4");
        cache_file << "EAPI=8\n";
        cache_file << "DEPEND=sys-libs/glibc\n";
        cache_file << "RDEPEND=sys-libs/glibc\n";
        cache_file << "IUSE=ipc native-extensions\n";
        cache_file.close();
        
        Logger::logInfo("testPortDbApi", "Mock Repository created at " + mock_repo);
        Slipper::Dbapi::PortDbApi portdb({mock_repo});
        
        std::cout << "--- Testing PortDbApi cp_list() ---" << std::endl;
        for (const auto& cpv : portdb.cp_list("sys-apps/portage")) {
            std::cout << "Found Ebuild CPV: " << cpv << std::endl;
        }

        std::cout << "--- Testing PortDbApi aux_get() ---" << std::endl;
        std::vector< std::string > wants = {"EAPI", "DEPEND", "IUSE", "MISSING_KEY"};
        auto metadata = portdb.aux_get("sys-apps/portage-3.0.4", wants);
        
        for (size_t i = 0; i < wants.size(); ++i) {
            std::cout << "Cache [" << wants[i] << "]: " << (metadata[i].empty() ? "" : metadata[i]) << std::endl;
        }
        std::filesystem::remove_all(mock_repo);
    } catch (const std::exception& e) {
        Logger::logInfo("testPortDbApi", std::string("PortDbApi threw an exception: ") + e.what());
    }
    Logger::logDebug("testPortDbApi", "Exiting routine: testPortDbApi.");
}

void testBinDbApi() {
    Logger::logDebug("testBinDbApi", "Entering routine: testBinDbApi.");
    std::string mock_pkgdir = "./mock_pkgdir";
    std::string mock_cat_dir = mock_pkgdir + "/sys-apps";
    
    try {
        std::filesystem::create_directories(mock_cat_dir);
        std::ofstream binpkg_file(mock_cat_dir + "/portage-3.0.4-1.gpkg.tar");
        binpkg_file << "Mock Binary Data\n";
        binpkg_file.close();
        
        Logger::logInfo("testBinDbApi", "Mock PKGDIR created at " + mock_pkgdir);
        Slipper::Dbapi::BinDbApi bindb(mock_pkgdir);
        
        std::cout << "--- Testing BinDbApi cp_all() ---" << std::endl;
        for (const auto& cp : bindb.cp_all()) {
            std::cout << "Found Binary CP: " << cp << std::endl;
        }
        std::filesystem::remove_all(mock_pkgdir);
    } catch (const std::exception& e) {
        Logger::logInfo("testBinDbApi", std::string("BinDbApi threw an exception: ") + e.what());
    }
    Logger::logDebug("testBinDbApi", "Exiting routine: testBinDbApi.");
}

void testPackageTracker() {
    Logger::logDebug("testPackageTracker", "Entering routine: testPackageTracker.");
    try {
        Slipper::Resolver::PackageTracker tracker;
        tracker.addPkg("sys-libs/glibc-2.33");
        tracker.addPkg("sys-apps/portage-3.0.4");
        
        Logger::logInfo("testPackageTracker", "Querying tracker for sys-apps/portage...");
        Slipper::Dep::Atom query_atom("sys-apps/portage");
        auto matches = tracker.match(query_atom);
        
        std::cout << "--- Tracker Matches ---" << std::endl;
        for (const auto& match : matches) {
            std::cout << "Matched CPV: " << match << std::endl;
        }

        Logger::logInfo("testPackageTracker", "Testing tracker removal...");
        tracker.removePkg("sys-apps/portage-3.0.4");
        
        if (!tracker.contains("sys-apps/portage-3.0.4")) {
            std::cout << "Successfully removed sys-apps/portage-3.0.4 from tracker." << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testPackageTracker", std::string("PackageTracker threw an exception: ") + e.what());
    }
    Logger::logDebug("testPackageTracker", "Exiting routine: testPackageTracker.");
}

void testConflictHandlers() {
    Logger::logDebug("testConflictHandlers", "Entering routine: testConflictHandlers.");
    try {
        Slipper::Resolver::PackageTracker tracker;
        tracker.addPkg("sys-apps/portage-3.0.4");
        tracker.addPkg("sys-apps/portage-3.0.5");
        
        Logger::logInfo("testConflictHandlers", "Evaluating Slot Conflicts...");
        Slipper::Resolver::SlotConflictHandler slot_handler(tracker);
        slot_handler.evaluateConflicts();
        
        for (const auto& conflict : slot_handler.getConflicts()) {
            std::cout << "--- " << conflict.type << " Detected ---" << std::endl;
            std::cout << "Atom: " << conflict.atom << std::endl;
            std::cout << "Involved Packages:" << std::endl;
            for (const auto& pkg : conflict.pkgs) {
                std::cout << "  - " << pkg << std::endl;
            }
        }
        
        slot_handler.generateSolutions();

        Logger::logInfo("testConflictHandlers", "Evaluating Circular Dependencies...");
        Slipper::Resolver::CircularDependencyHandler cycle_handler;
        
        std::unordered_map< std::string, std::vector< std::string > > mock_graph;
        mock_graph["sys-libs/glibc-2.33"] = {"sys-devel/gcc-11.2.0"};
        mock_graph["sys-devel/gcc-11.2.0"] = {"sys-libs/glibc-2.33"};
        
        cycle_handler.detectCycles(mock_graph);
        
        auto cycle = cycle_handler.getShortestCycle();
        if (!cycle.empty()) {
            std::cout << "--- Circular Dependency Detected ---" << std::endl;
            for (size_t i = 0; i < cycle.size(); ++i) {
                std::cout << cycle[i] << (i < cycle.size() - 1 ? " -> " : "");
            }
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        Logger::logInfo("testConflictHandlers", std::string("ConflictHandlers threw an exception: ") + e.what());
    }
    Logger::logDebug("testConflictHandlers", "Exiting routine: testConflictHandlers.");
}

int main() {
    std::cout << "Running Slipper Test Battery...\n" << std::endl;
    
    testAtomParser();
    testSonameParser();
    testVersionParser();
    testRepoConfigParser();
    testUseManager();
    testMaskManager();
    testKeywordsManager();
    testLicenseManager();
    testVarDbApi();
    testPortDbApi();
    testBinDbApi();
    testPackageTracker();
    testConflictHandlers();

    std::cout << "\nTest Battery Complete." << std::endl;
    return 0;
}