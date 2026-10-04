#include "../../include/execution/Executor.hpp"
#include "../../include/Logger.hpp"
#include "../../include/dep/Atom.hpp"
#include "iostream"
#include "filesystem"
#include "cstdlib"
#include "unistd.h"
#include "sys/wait.h"
#include "fcntl.h"
#include "fstream"
#include "sstream"
#include "regex"

using namespace Slipper::Execution;

Executor::Executor(const std::vector< std::string >& repo_paths) : repos(repo_paths) {}

std::string Executor::findEbuildPath(const std::string& cpv) const {
    Slipper::Dep::Atom atom(cpv);
    std::string cp = atom.getCp();
    std::string version = atom.getVersion().value_or("");
    std::string pkg_name = cp.substr(cp.find('/') + 1);
    
    std::string ebuild_name = pkg_name + "-" + version + ".ebuild";

    for (const auto& repo : repos) {
        std::string full_path = repo + "/" + cp + "/" + ebuild_name;
        if (std::filesystem::exists(full_path)) {
            return full_path;
        }
    }
    return "";
}

std::vector< std::pair< std::string, std::string > > Executor::extractSrcUri(const std::string& ebuild_path) const {
    std::vector< std::pair< std::string, std::string > > uris;
    
    std::filesystem::path p(ebuild_path);
    std::string ebuild_file = p.filename().string();
    std::string category = p.parent_path().parent_path().filename().string();
    std::string repo_base = p.parent_path().parent_path().parent_path().string();
    
    std::string cpv = category + "/" + ebuild_file.substr(0, ebuild_file.find(".ebuild"));
    std::string cache_path = repo_base + "/metadata/md5-cache/" + cpv;
    
    if (std::filesystem::exists(cache_path)) {
        std::ifstream file(cache_path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("SRC_URI=", 0) == 0) {
                std::string uri_str = line.substr(8);
                std::stringstream ss(uri_str);
                std::string token;
                
                while (ss >> token) {
                    if (token == "->") {
                        std::string target_name;
                        if (ss >> target_name && !uris.empty()) {
                            uris.back().second = target_name;
                        }
                    } else {
                        std::string filename = token.substr(token.find_last_of('/') + 1);
                        uris.push_back({token, filename});
                    }
                }
                return uris;
            }
        }
    }
    return uris;
}

bool Executor::fetchSources(const std::vector< std::pair< std::string, std::string > >& uris) const {
    std::string distdir = "/var/cache/distfiles";
    std::filesystem::create_directories(distdir);
    
    for (const auto& item : uris) {
        std::string url = item.first;
        std::string filename = item.second;
        std::string dest = distdir + "/" + filename;
        
        if (std::filesystem::exists(dest)) {
            if (std::getenv("DEBUG")) {
                Logger::logDebug("Executor", "Distfile already exists: " + dest);
            }
            continue;
        }
        
        std::cout << ">>> Downloading " << filename << "..." << std::endl;
        std::string cmd = "wget -q --show-progress -O " + dest + " " + url;
        int ret = system(cmd.c_str());
        if (ret != 0) {
            Logger::logError("Executor", "Failed to fetch: " + url);
            return false;
        }
    }
    return true;
}

// --- Native Multiline Bash Environment Filter ---
bool Executor::filterEnvironment(const std::string& build_dir) const {
    std::string vars_raw = build_dir + "/environment-vars.raw";
    std::string funcs_raw = build_dir + "/environment-funcs.raw";
    std::string sh_path = build_dir + "/environment.sh";

    if (!std::filesystem::exists(vars_raw)) return true;

    std::ifstream in_vars(vars_raw);
    std::ofstream out(sh_path);

    std::string line;
    char multi_line_quote = 0;
    
    std::regex internal_vars_re(R"(^(declare|typeset)\s+([^=]*\s+)?(BASH_.*|BASHOPTS|FUNCNAME|GROUPS|EUID|PPID|UID|SHELLOPTS)(=.*)?$)");
    std::regex var_assign_re(R"((^|^declare\s+-\S+\s+|^declare\s+|^export\s+)([^=\s]+)=("|\')?.*$)");
    std::regex readonly_re(R"(^declare\s+-(\S*)r(\S*)\s+(.*))");
    std::regex close_quote_re(R"((\\"|"|')\s*$)");
    std::smatch match;

    if (std::getenv("DEBUG")) {
        Logger::logDebug("Executor::filterEnvironment", "Starting environment filter pass for: " + build_dir);
    }

    // Phase 1: Safely filter the variable assignments
    while (std::getline(in_vars, line)) {
        if (multi_line_quote != 0) {
            out << line << "\n";
            if (std::regex_search(line, match, close_quote_re)) {
                if (match.str(1)[0] == multi_line_quote) {
                    if (std::getenv("DEBUG")) {
                        Logger::logDebug("Executor::filterEnvironment", "Closed multi-line quote block.");
                    }
                    multi_line_quote = 0;
                }
            }
            continue;
        }

        // Drop read-only internal Bash arrays and variables completely
        if (std::regex_match(line, match, internal_vars_re)) {
            if (std::getenv("DEBUG")) {
                Logger::logDebug("Executor::filterEnvironment", "Stripped protected internal bash variable.");
            }
            continue;
        }

        if (std::regex_match(line, match, var_assign_re)) {
            std::string pre_r = match[1];
            std::string post_r = match[2];
            std::string remainder = match[3];
            
            if (pre_r.empty() && post_r.empty()) {
                out << "declare " << remainder << "\n";
            } else {
                out << "declare -" << pre_r << post_r << " " << remainder << "\n";
            }
            continue;
        }

        out << line << "\n";
    }
    in_vars.close();

    // Phase 2: Blindly append the unmolested function bodies
    if (std::filesystem::exists(funcs_raw)) {
        std::ifstream in_funcs(funcs_raw);
        while (std::getline(in_funcs, line)) {
            out << line << "\n";
        }
        in_funcs.close();
    }

    std::filesystem::remove(vars_raw);
    std::filesystem::remove(funcs_raw);
    return true;
}
// -----------------------------------------------------

bool Executor::executeEbuildPhases(const std::string& ebuild_path, const std::string& cpv, bool stream_output) const {
    std::vector< std::string > phases = {"setup", "unpack", "prepare", "configure", "compile", "install"};
    std::string wrapper_path = "/usr/local/share/slipper/slipper-functions.sh";

    Slipper::Dep::Atom atom(cpv);
    std::string category = atom.getCp().substr(0, atom.getCp().find('/'));
    std::string pkg_name = atom.getCp().substr(atom.getCp().find('/') + 1);
    std::string full_version = atom.getVersion().value_or("");
    
    std::string pv = full_version;
    std::string pr = "r0";
    size_t r_pos = full_version.find("-r");
    if (r_pos != std::string::npos) {
        pv = full_version.substr(0, r_pos);
        pr = full_version.substr(r_pos + 1);
    }
    std::string pvr = pv;
    if (pr != "r0") pvr += "-" + pr;
    
    std::string p = pkg_name + "-" + pv;
    std::string pf = pkg_name + "-" + pvr;
    
    std::string build_dir = "/var/tmp/slipper/" + cpv;
    std::string workdir = build_dir + "/work";
    std::string image_dir = build_dir + "/image";
    std::string default_source_dir = workdir + "/" + p;
    std::string ebuild_dir = std::filesystem::path(ebuild_path).parent_path().string();
    std::string filesdir = ebuild_dir + "/files";

    std::filesystem::create_directories(workdir);
    std::filesystem::create_directories(image_dir);

    auto uris = extractSrcUri(ebuild_path);
    std::string a_var = "";
    for (const auto& item : uris) {
        a_var += item.second + " ";
    }
    if (!a_var.empty()) a_var.pop_back();

    for (const auto& phase : phases) {
        if (!stream_output) {
            std::cout << " * Executing phase: " << phase << "..." << std::endl;
        }

        // NEW: Trigger the C++ environment filter before spawning the next bash process
        filterEnvironment(build_dir);

        pid_t pid = fork();
        
        if (pid == 0) {
            setenv("CATEGORY", category.c_str(), 1);
            setenv("PN", pkg_name.c_str(), 1);
            setenv("PV", pv.c_str(), 1);
            setenv("PR", pr.c_str(), 1);
            setenv("PVR", pvr.c_str(), 1);
            setenv("P", p.c_str(), 1);
            setenv("PF", pf.c_str(), 1);
            
            setenv("EBUILD", ebuild_path.c_str(), 1);
            setenv("FILESDIR", filesdir.c_str(), 1);
            setenv("WORKDIR", workdir.c_str(), 1);
            setenv("S", default_source_dir.c_str(), 1);
            setenv("T", build_dir.c_str(), 1);
            setenv("A", a_var.c_str(), 1);
            
            setenv("D", (image_dir + "/").c_str(), 1);
            setenv("ED", (image_dir + "/").c_str(), 1);
            
            chdir(workdir.c_str());

            if (!stream_output) {
                std::string log_path = "/var/log/slipper/build-" + p + ".log";
                int fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0664);
                if (fd != -1) {
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }
            }
            
            execl("/bin/bash", "bash", wrapper_path.c_str(), ebuild_path.c_str(), phase.c_str(), (char*)NULL);
            exit(1);
        } else if (pid > 0) {
            int status = 0; // Initialize to prevent garbage memory evaluation
            waitpid(pid, &status, 0);
            
            if (std::getenv("DEBUG")) {
                Logger::logDebug("Executor::executeEbuildPhases", "Child process waitpid returned. Status: " + std::to_string(status));
            }
            
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                if (std::getenv("DEBUG")) {
                    Logger::logError("Executor::executeEbuildPhases", "Ebuild phase execution failed with non-zero status.");
                }
                return false; 
            }
        } else {
            return false;
        }
    }
    return true;
}

// --- NEW: Live Filesystem Merge Engine ---
bool Executor::mergeImage(const std::string& cpv, const std::string& ebuild_path) const {
    std::cout << ">>> Merging " << cpv << " to live filesystem..." << std::endl;

    Slipper::Dep::Atom atom(cpv);
    std::string category = atom.getCp().substr(0, atom.getCp().find('/'));
    std::string pkg_name = atom.getCp().substr(atom.getCp().find('/') + 1);
    
    std::string full_version = atom.getVersion().value_or("");
    std::string pv = full_version;
    std::string pr = "r0";
    size_t r_pos = full_version.find("-r");
    if (r_pos != std::string::npos) {
        pv = full_version.substr(0, r_pos);
        pr = full_version.substr(r_pos + 1);
    }
    std::string pvr = pv;
    if (pr != "r0") pvr += "-" + pr;
    std::string pf = pkg_name + "-" + pvr;
    
    std::string image_dir = "/var/tmp/slipper/" + cpv + "/image";
    std::string vdb_dir = "/var/db/pkg/" + cpv;

    // 1. Establish the VDB Entry
    std::filesystem::create_directories(vdb_dir);
    std::ofstream contents_file(vdb_dir + "/CONTENTS");

    // 2. Recursively traverse the Image Directory
    if (std::filesystem::exists(image_dir)) {
        for (auto const& dir_entry : std::filesystem::recursive_directory_iterator(image_dir)) {
            // Calculate absolute target path on the live system
            std::string relative_path = dir_entry.path().string().substr(image_dir.length());
            if (relative_path.empty()) continue;
            std::string target_path = relative_path;
            
            // Generate standard copy options (overwrite + preserve symlinks)
            auto copy_opts = std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::copy_symlinks;

            if (dir_entry.is_directory()) {
                std::filesystem::create_directories(target_path);
                contents_file << "dir " << target_path << "\n";
            } else {
                std::filesystem::copy(dir_entry.path(), target_path, copy_opts);
                contents_file << "obj " << target_path << "\n";
            }
        }
    }

    // 3. Stage the Metadata needed by Slipper and Portage
    std::string ebuild_name = std::filesystem::path(ebuild_path).filename().string();
    std::filesystem::copy(ebuild_path, vdb_dir + "/" + ebuild_name, std::filesystem::copy_options::overwrite_existing);

    std::ofstream(vdb_dir + "/PF") << pf << "\n";
    std::ofstream(vdb_dir + "/CATEGORY") << category << "\n";
    std::ofstream(vdb_dir + "/SLOT") << "0\n";
    std::ofstream(vdb_dir + "/EAPI") << "8\n";
    std::ofstream(vdb_dir + "/REPOSITORY") << "gentoo\n";
    
    return true;
}

bool Executor::clean(const std::string& cpv) const {
    std::string build_dir = "/var/tmp/slipper/" + cpv;
    if (std::filesystem::exists(build_dir)) {
        std::error_code ec;
        std::filesystem::remove_all(build_dir, ec);
        if (ec) {
            Logger::logError("Executor", "Failed to clean build directory: " + ec.message());
            return false;
        }
    }
    return true;
}

bool Executor::slipQueue(const std::vector< std::string >& merge_queue, bool stream_output) {
    if (merge_queue.empty()) return true;

    std::cout << "\n>>> Starting Slipper Execution Engine...\n" << std::endl;

    for (const auto& cpv : merge_queue) {
        std::cout << ">>> Slipping in " << cpv << "..." << std::endl;
        
        // NEW: Guarantee a clean slate by purging any stale state from previous crashes
        if (std::getenv("DEBUG")) {
            Logger::logDebug("Executor::slipQueue", "Purging stale build directory for " + cpv);
        }
        clean(cpv);
        
        std::string ebuild_path = findEbuildPath(cpv);
        if (ebuild_path.empty()) return false;

        auto uris = extractSrcUri(ebuild_path);
        if (!fetchSources(uris)) return false;

        if (!executeEbuildPhases(ebuild_path, cpv, stream_output)) {
            std::cout << "[!] Fatal: Build failed for " << cpv << std::endl;
            return false;
        }

        if (!mergeImage(cpv, ebuild_path)) {
            std::cout << "[!] Fatal: Live merge failed for " << cpv << std::endl;
            return false;
        }

        // NEW: Visual indicator for the linker cache update
        if (!stream_output) {
            std::cout << " * Updating dynamic linker cache (ldconfig)..." << std::endl;
        }

        if (std::getenv("DEBUG")) {
            Logger::logDebug("Executor::slipQueue", "Updating dynamic linker cache via ldconfig.");
        }
        system("ldconfig");

        if (!stream_output) {
            std::cout << " * Executing phase: clean..." << std::endl;
        }
        clean(cpv);

        std::cout << ">>> Successfully slipped in " << cpv << "\n" << std::endl;
    }

    std::cout << ">>> Slipper execution complete." << std::endl;
    return true;
}

// --- NEW: Slipper Unmerge Engine ---
bool Executor::slipOut(const std::string& cpv) {
    if (std::getenv("DEBUG")) {
        Logger::logDebug("Executor::slipOut", "Initialising uninstall routine for: " + cpv);
    }

    std::cout << ">>> Slipping out " << cpv << "..." << std::endl;
    std::filesystem::path vdb_path = std::filesystem::path("/var/db/pkg") / cpv;

    if (!std::filesystem::exists(vdb_path)) {
        std::cout << "[!] Package " << cpv << " is not installed (VDB entry missing)." << std::endl;
        return false;
    }

    std::filesystem::path contents_file = vdb_path / "CONTENTS";
    if (!std::filesystem::exists(contents_file)) {
        std::cout << "[!] CONTENTS file missing for " << cpv << ". Cannot safely uninstall." << std::endl;
        return false;
    }

    if (std::getenv("DEBUG")) {
        Logger::logDebug("Executor::slipOut", "Successfully located CONTENTS file: " + contents_file.string());
    }

    std::ifstream file(contents_file);
    std::string line;
    std::vector< std::filesystem::path > files_to_remove;
    std::vector< std::filesystem::path > dirs_to_remove;

    while (std::getline(file, line)) {
        size_t first_space = line.find(' ');
        if (first_space == std::string::npos) continue;

        std::string type = line.substr(0, first_space);
        size_t second_space = line.find(' ', first_space + 1);
        std::string path_str;

        if (type == "sym") {
            size_t arrow_pos = line.find(" -> ", first_space + 1);
            if (arrow_pos != std::string::npos) {
                path_str = line.substr(first_space + 1, arrow_pos - (first_space + 1));
            }
        } else if (type == "obj" || type == "dir") {
            if (second_space != std::string::npos) {
                path_str = line.substr(first_space + 1, second_space - (first_space + 1));
            } else {
                path_str = line.substr(first_space + 1);
            }
        }

        if (!path_str.empty()) {
            if (type == "dir") {
                dirs_to_remove.push_back(path_str);
            } else {
                files_to_remove.push_back(path_str);
            }
        }
    }

    if (std::getenv("DEBUG")) {
        Logger::logDebug("Executor::slipOut", "Commencing file and symlink removal...");
    }
    
    for (const auto& f : files_to_remove) {
        std::error_code ec;
        if (std::filesystem::symlink_status(f, ec).type() != std::filesystem::file_type::not_found) {
            std::filesystem::remove(f, ec);
            if (ec && std::getenv("DEBUG")) {
                Logger::logError("Executor::slipOut", "Failed to remove " + f.string() + ": " + ec.message());
            }
        }
    }

    if (std::getenv("DEBUG")) {
        Logger::logDebug("Executor::slipOut", "Sorting directories for safe bottom-up removal...");
    }
    std::sort(dirs_to_remove.begin(), dirs_to_remove.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
        return a.string().length() > b.string().length();
    });

    for (const auto& d : dirs_to_remove) {
        std::error_code ec;
        if (std::filesystem::exists(d, ec) && std::filesystem::is_empty(d, ec)) {
            std::filesystem::remove(d, ec);
        }
    }

    if (std::getenv("DEBUG")) {
        Logger::logDebug("Executor::slipOut", "Erasing package Virtual Database (VDB) entry: " + vdb_path.string());
    }
    std::error_code ec;
    std::filesystem::remove_all(vdb_path, ec);

    std::cout << " * Updating dynamic linker cache (ldconfig)..." << std::endl;
    if (std::getenv("DEBUG")) {
        Logger::logDebug("Executor::slipOut", "Updating dynamic linker cache via ldconfig.");
    }
    system("ldconfig");

    std::cout << ">>> Successfully slipped out " << cpv << "\n" << std::endl;
    return true;
}