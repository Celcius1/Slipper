#pragma once
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <chrono>
#include <ctime>

class Logger {
private:
    // Evaluate the environment variable exactly once at binary load time
    static inline const bool IS_DEBUG = []() {
        if (const char* env_p = std::getenv("DEBUG")) {
            std::string debugFlag(env_p);
            return debugFlag == "1" || debugFlag == "true" || debugFlag == "TRUE";
        }
        return false;
    }();

    static inline std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
        return std::string(buf);
    }

    static inline void writeToFile(const std::string& level, const std::string& component, const std::string& message) {
        // Atomic append mode ensures forked child processes don't overwrite each other
        std::ofstream log_file("/var/log/slipper/slipper.log", std::ios_base::app);
        if (log_file.is_open()) {
            log_file << "[" << getCurrentTime() << "] [" << level << "] [" << component << "] " << message << "\n";
        }
    }

public:
    static inline bool isDebugEnabled() {
        return IS_DEBUG;
    }

    // Standard output for major daemon lifecycle events (always visible).
    static inline void logInfo(const std::string& routine, const std::string& message) {
        std::cout << "[INFO] [" << routine << "] " << message << std::endl;
        writeToFile("INFO", routine, message);
    }

    static inline void logError(const std::string& routine, const std::string& message) {
        std::cerr << "\033[1;31m[ERROR] [" << routine << "] " << message << "\033[0m" << std::endl;
        writeToFile("ERROR", routine, message);
    }

    // Use this for step-by-step routine tracking.
    // It evaluates against the static constant, entirely bypassing glibc environment locks.
    static inline void logDebug(const std::string& routine, const std::string& message) {
        if (IS_DEBUG) {
            std::cout << "[DEBUG] [" << routine << "] " << message << std::endl;
            writeToFile("DEBUG", routine, message);
        }
    }
};