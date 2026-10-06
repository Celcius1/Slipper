#include "../../include/execution/Executor.hpp"
#include "../../include/Logger.hpp"
#include "../../include/dep/Atom.hpp"
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

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
                        // EAPI 8: Strip fetch+ and mirror+ prefixes to resolve the actual URI
                        if (token.find("fetch+") == 0) {
                            token = token.substr(6);
                            if (Logger::isDebugEnabled()) Logger::logDebug("Executor::extractSrcUri", "Stripped EAPI 8 fetch+ prefix.");
                        } else if (token.find("mirror+") == 0) {
                            token = token.substr(7);
                            if (Logger::isDebugEnabled()) Logger::logDebug("Executor::extractSrcUri", "Stripped EAPI 8 mirror+ prefix.");
                        }

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
    std::error_code ec;
    
    std::filesystem::create_directories(distdir, ec);
    if (ec) {
        Logger::logError("Executor::fetchSources", "Failed to create distdir: " + ec.message());
        return false;
    }
    
    for (const auto& item : uris) {
        std::string url = item.first;
        std::string filename = item.second;
        std::string dest = distdir + "/" + filename;
        
        if (std::filesystem::exists(dest)) {
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Executor::fetchSources", "Distfile already exists: " + dest);
            }
            continue;
        }
        
        std::cout << ">>> Downloading " << filename << "..." << std::endl;
        
        pid_t pid = fork();
        if (pid == -1) {
            Logger::logError("Executor::fetchSources", std::string("Fork failed: ") + strerror(errno));
            return false;
        } else if (pid == 0) {
            // Child process: Execute wget directly, bypassing the shell interpreter
            execlp("wget", "wget", "-q", "--show-progress", "-O", dest.c_str(), url.c_str(), static_cast< char* >(NULL));
            
            // SECURITY: Code following an exec() is exclusively a fatal error block
            Logger::logError("Executor::fetchSources", std::string("execlp failed: ") + strerror(errno));
            _exit(EXIT_FAILURE);
        } else {
            // Parent process: Wait securely for the child to finish
            int status = 0;
            waitpid(pid, &status, 0);
            
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                Logger::logError("Executor::fetchSources", "Failed to fetch: " + url + " (Exit status: " + std::to_string(WEXITSTATUS(status)) + ")");
                return false;
            }
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

    if (Logger::isDebugEnabled()) {
        Logger::logDebug("Executor::filterEnvironment", "Starting environment filter pass for: " + build_dir);
    }

    // Phase 1: Safely filter the variable assignments
    while (std::getline(in_vars, line)) {
        if (multi_line_quote != 0) {
            out << line << "\n";
            if (std::regex_search(line, match, close_quote_re)) {
                if (match.str(1)[0] == multi_line_quote) {
                    if (Logger::isDebugEnabled()) {
                        Logger::logDebug("Executor::filterEnvironment", "Closed multi-line quote block.");
                    }
                    multi_line_quote = 0;
                }
            }
            continue;
        }

        // Drop read-only internal Bash arrays and variables completely
        if (std::regex_match(line, match, internal_vars_re)) {
            if (Logger::isDebugEnabled()) {
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
    std::string wrapper_path = "/usr/share/slipper/slipper-functions.sh";

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
    std::string empty_dir = build_dir + "/empty"; // EAPI 8 required empty directory
    std::string default_source_dir = workdir + "/" + p;
    std::string ebuild_dir = std::filesystem::path(ebuild_path).parent_path().string();
    std::string filesdir = ebuild_dir + "/files";

    std::filesystem::create_directories(workdir);
    std::filesystem::create_directories(image_dir);
    std::filesystem::create_directories(empty_dir);

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
            setenv("PORTAGE_EMPTY_DIR", empty_dir.c_str(), 1); // Pass to bash wrapper
            
            chdir(workdir.c_str());

            if (!stream_output) {
                std::string log_path = "/var/log/slipper/build-" + p + ".log";
                // SECURITY: Enforce O_CLOEXEC and explicit error checking on open()
                int fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0664);
                if (fd == -1) {
                    Logger::logError("Executor::executeEbuildPhases", std::string("Failed to open log file: ") + strerror(errno));
                    _exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            
            execl("/bin/bash", "bash", wrapper_path.c_str(), ebuild_path.c_str(), phase.c_str(), (char*)NULL);
            
            // SECURITY: Fatal error-handling block
            Logger::logError("Executor::executeEbuildPhases", std::string("execl failed: ") + strerror(errno));
            _exit(EXIT_FAILURE);
        } else if (pid > 0) {
            int status = 0; 
            waitpid(pid, &status, 0);
            
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Executor::executeEbuildPhases", "Child process waitpid returned. Status: " + std::to_string(status));
            }
            
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                if (Logger::isDebugEnabled()) {
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
    std::error_code ec;

    // 1. Establish the VDB Entry securely
    std::filesystem::create_directories(vdb_dir, ec);
    if (ec) {
        Logger::logError("Executor::mergeImage", "Failed to create VDB directory: " + ec.message());
        return false;
    }
    std::ofstream contents_file(vdb_dir + "/CONTENTS");

    // 2. Recursively traverse the Image Directory with symlink defenses
    if (std::filesystem::exists(image_dir)) {
        for (auto const& dir_entry : std::filesystem::recursive_directory_iterator(image_dir)) {
            std::string relative_path = dir_entry.path().string().substr(image_dir.length());
            if (relative_path.empty()) continue;
            
            // The absolute target path on the live system
            std::string target_path = relative_path; 

            // Eliminate Check-Then-Act: Directly remove the target path to break existing symlinks/files
            std::filesystem::remove(target_path, ec);

            if (dir_entry.is_symlink(ec)) {
                std::string link_target = std::filesystem::read_symlink(dir_entry.path(), ec).string();
                if (symlink(link_target.c_str(), target_path.c_str()) == -1) {
                    Logger::logError("Executor::mergeImage", std::string("Failed to securely create symlink: ") + strerror(errno));
                    return false;
                }
                contents_file << "sym " << target_path << " -> " << link_target << "\n";
            } else if (dir_entry.is_directory(ec)) {
                std::filesystem::create_directories(target_path, ec);
                contents_file << "dir " << target_path << "\n";
            } else {
                int src_fd = open(dir_entry.path().string().c_str(), O_RDONLY | O_CLOEXEC);
                if (src_fd == -1) {
                    Logger::logError("Executor::mergeImage", std::string("Failed to open source file: ") + strerror(errno));
                    return false;
                }

                // Direct OS Enforcement: Atomic file creation preventing symlink traversal (CWE-61) and TOCTOU (CWE-367)
                int dst_fd = open(target_path.c_str(), O_CREAT | O_WRONLY | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
                if (dst_fd == -1) {
                    Logger::logError("Executor::mergeImage", std::string("Atomic file creation failed: ") + strerror(errno));
                    close(src_fd);
                    return false;
                }

                char buf[8192];
                ssize_t bytes_read;
                while ((bytes_read = read(src_fd, buf, sizeof(buf))) > 0) {
                    ssize_t bytes_written = 0;
                    while (bytes_written < bytes_read) {
                        ssize_t res = write(dst_fd, buf + bytes_written, bytes_read - bytes_written);
                        if (res == -1) {
                            Logger::logError("Executor::mergeImage", std::string("Failed to write to target file: ") + strerror(errno));
                            close(src_fd);
                            close(dst_fd);
                            return false;
                        }
                        bytes_written += res;
                    }
                }
                
                struct stat st;
                if (fstat(src_fd, &st) == 0) {
                    fchmod(dst_fd, st.st_mode);
                }

                close(src_fd);
                close(dst_fd);

                contents_file << "obj " << target_path << "\n";
            }
        }
    }

    // 3. Stage the Metadata needed by Slipper and Portage
    std::string ebuild_name = std::filesystem::path(ebuild_path).filename().string();
    std::filesystem::copy(ebuild_path, vdb_dir + "/" + ebuild_name, std::filesystem::copy_options::overwrite_existing, ec);

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
        if (Logger::isDebugEnabled()) {
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

        if (Logger::isDebugEnabled()) {
            Logger::logDebug("Executor::slipQueue", "Updating dynamic linker cache via ldconfig.");
        }
        pid_t ld_pid = fork();
        if (ld_pid == -1) {
            Logger::logError("Executor", std::string("Fork failed for ldconfig: ") + strerror(errno));
        } else if (ld_pid == 0) {
            execl("/sbin/ldconfig", "ldconfig", (char*)NULL);
            
            Logger::logError("Executor", std::string("execl failed for ldconfig: ") + strerror(errno));
            _exit(EXIT_FAILURE);
        } else {
            int status = 0;
            waitpid(ld_pid, &status, 0);
            
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) == 127) {
                    Logger::logError("Executor", "Execution failure: ldconfig not found (status 127).");
                } else if (WEXITSTATUS(status) != 0) {
                    Logger::logError("Executor", "ldconfig failed with exit status: " + std::to_string(WEXITSTATUS(status)));
                }
            }
        }

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
    if (Logger::isDebugEnabled()) {
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

    if (Logger::isDebugEnabled()) {
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

    if (Logger::isDebugEnabled()) {
        Logger::logDebug("Executor::slipOut", "Commencing file and symlink removal...");
    }
    
    for (const auto& f : files_to_remove) {
        std::error_code ec;
        if (std::filesystem::symlink_status(f, ec).type() != std::filesystem::file_type::not_found) {
            std::filesystem::remove(f, ec);
            if (ec && Logger::isDebugEnabled()) {
                Logger::logError("Executor::slipOut", "Failed to remove " + f.string() + ": " + ec.message());
            }
        }
    }

    if (Logger::isDebugEnabled()) {
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

    if (Logger::isDebugEnabled()) {
        Logger::logDebug("Executor::slipOut", "Erasing package Virtual Database (VDB) entry: " + vdb_path.string());
    }
    std::error_code ec;
    std::filesystem::remove_all(vdb_path, ec);

    std::cout << " * Updating dynamic linker cache (ldconfig)..." << std::endl;
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("Executor::slipOut", "Updating dynamic linker cache via ldconfig.");
    }
    pid_t ld_pid = fork();
        if (ld_pid == -1) {
            Logger::logError("Executor", std::string("Fork failed for ldconfig: ") + strerror(errno));
        } else if (ld_pid == 0) {
            execl("/sbin/ldconfig", "ldconfig", (char*)NULL);
            
            Logger::logError("Executor", std::string("execl failed for ldconfig: ") + strerror(errno));
            _exit(EXIT_FAILURE);
        } else {
            int status = 0;
            waitpid(ld_pid, &status, 0);
            
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) == 127) {
                    Logger::logError("Executor", "Execution failure: ldconfig not found (status 127).");
                } else if (WEXITSTATUS(status) != 0) {
                    Logger::logError("Executor", "ldconfig failed with exit status: " + std::to_string(WEXITSTATUS(status)));
                }
            }
        }

    std::cout << ">>> Successfully slipped out " << cpv << "\n" << std::endl;
    return true;
}