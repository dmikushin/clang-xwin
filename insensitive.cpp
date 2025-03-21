// A shared library to intercept file access calls and make filenames case-insensitive
// clang++-20 -g -O0 -std=c++17 -fPIC insensitive.cpp -shared -o libinsensitive.so

#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <memory>
#include <vector>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <system_error>
#include <unordered_map>
#include <mutex>
#include <iomanip>

namespace fs = std::filesystem;

// Debug logging system
class DebugLogger {
private:
    bool enabled;
    int level;
    std::mutex log_mutex;
    FILE* log_file;

    // Helper method to get level as string
    static std::string getLevelString(int level) {
        switch (level) {
            case ERROR: return "ERROR";
            case WARNING: return "WARNING";
            case INFO: return "INFO";
            case DEBUG: return "DEBUG";
            case TRACE: return "TRACE";
            default: return "UNKNOWN";
        }
    }

    // Helper to write log prefix to stream
    void writeLogPrefix(std::ostringstream& oss, int msg_level) {
        // Print timestamp, pid, and level prefix
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        oss << "[" 
            << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") 
            << "][" << getpid() << "]["
            << getLevelString(msg_level) << "] ";
    }

    // Internal log writer that takes a stringstream
    void writeLogMessage(int msg_level, const std::string& message) {
        if (!enabled || msg_level > level) return;

        std::lock_guard<std::mutex> lock(log_mutex);
        
        std::ostringstream oss;
        writeLogPrefix(oss, msg_level);
        oss << message;

        // Ensure we have a newline
        std::string output = oss.str();
        if (!output.empty() && output.back() != '\n') {
            output += '\n';
        }
        
        fputs(output.c_str(), log_file);
        fflush(log_file);
    }

public:
    enum LogLevel {
        ERROR = 0,
        WARNING = 1,
        INFO = 2,
        DEBUG = 3,
        TRACE = 4
    };

    DebugLogger() : enabled(false), level(ERROR), log_file(stderr) {
        // Check environment variable to enable logging
        const char* env_debug = getenv("INSENSITIVE_DEBUG");
        if (env_debug && (strcmp(env_debug, "1") == 0 || 
                          strcmp(env_debug, "true") == 0 || 
                          strcmp(env_debug, "yes") == 0)) {
            enabled = true;
        }
        
        // Check environment variable for log level
        const char* env_level = getenv("INSENSITIVE_DEBUG_LEVEL");
        if (env_level) {
            level = atoi(env_level);
            if (level < ERROR) level = ERROR;
            if (level > TRACE) level = TRACE;
        }
        
        // Check for log file path
        const char* env_file = getenv("INSENSITIVE_DEBUG_FILE");
        if (env_file && strlen(env_file) > 0) {
            FILE* f = fopen(env_file, "a");
            if (f) {
                log_file = f;
            } else {
                fprintf(stderr, "Warning: Could not open debug log file '%s', using stderr\n", env_file);
            }
        }
    }

    ~DebugLogger() {
        if (log_file && log_file != stderr && log_file != stdout) {
            fclose(log_file);
        }
    }

    bool isEnabled() const {
        return enabled;
    }

    int getLevel() const {
        return level;
    }

    // Stream-based logging methods
    template<typename... Args>
    void error(const Args&... args) {
        if (!enabled || ERROR > level) return;
        std::ostringstream oss;
        (oss << ... << args);
        writeLogMessage(ERROR, oss.str());
    }
    
    template<typename... Args>
    void warning(const Args&... args) {
        if (!enabled || WARNING > level) return;
        std::ostringstream oss;
        (oss << ... << args);
        writeLogMessage(WARNING, oss.str());
    }
    
    template<typename... Args>
    void info(const Args&... args) {
        if (!enabled || INFO > level) return;
        std::ostringstream oss;
        (oss << ... << args);
        writeLogMessage(INFO, oss.str());
    }
    
    template<typename... Args>
    void debug(const Args&... args) {
        if (!enabled || DEBUG > level) return;
        std::ostringstream oss;
        (oss << ... << args);
        writeLogMessage(DEBUG, oss.str());
    }
    
    template<typename... Args>
    void trace(const Args&... args) {
        if (!enabled || TRACE > level) return;
        std::ostringstream oss;
        (oss << ... << args);
        writeLogMessage(TRACE, oss.str());
    }
};

#define DEF(func) decltype(&func) func##_real
#define STR(x) #x
#define BIND(func) func##_real = getFunctionPointer<decltype(&func)>(STR(func))

// Add a cache and a mutex for thread safety
class Wrapper {
    std::unordered_map<std::string, std::string> cache;
    std::mutex cache_mutex;

    template<typename T>
    T getFunctionPointer(const char* name) {
        logger.debug("Getting function pointer for ", name);
        T func = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
        if (!func) {
            logger.error("Could not find original function ", name, ": ", dlerror());
        }
        return func;
    }

    static std::unique_ptr<char[]> clone(const char* source) {
        if (!source) {
            return nullptr;
        }

        size_t length = std::strlen(source) + 1;
        std::unique_ptr<char[]> cloned(new char[length]);
        std::strcpy(cloned.get(), source);

        return cloned;
    }

    // Check if a path should be excluded from case-insensitive handling
    bool should_exclude_path(const char* path) {
        if (!path) return false;
        
        // List of paths to exclude from case-insensitive handling
        static const std::vector<std::string> excluded_prefixes = {
            "/dev/",
            "/proc/",
            "/sys/"
        };
        
        for (const auto& prefix : excluded_prefixes) {
            if (strncmp(path, prefix.c_str(), prefix.length()) == 0) {
                logger.trace("Path '", path, "' excluded from case-insensitive handling");
                return true;
            }
        }
        
        return false;
    }

    bool file_exists_real(const char* path) {
        logger.trace("Checking if file exists: ", path);
        struct stat st;
        int result = stat_real(path, &st);
        logger.trace("File ", path, " exists? ", result == 0 ? "yes" : "no");
        return result == 0;
    }

    std::unique_ptr<char[]> replace_filename_case_insensitive(const char* path) {
        if (!path) return nullptr;
        
        logger.debug("Processing path: ", path);
        
        // Skip case-insensitive handling for excluded paths
        if (should_exclude_path(path)) {
            logger.debug("Path excluded, returning unchanged: ", path);
            return clone(path);
        }
        
        fs::path p(path);
        
        // If path is empty or root directory, just return it
        if (p.empty() || p == p.root_path()) {
            logger.debug("Path is empty or root, returning unchanged: ", path);
            return clone(path);
        }
        
        // If file exists with exact case, no need to search
        if (file_exists_real(path)) {
            logger.debug("File exists with exact case, returning unchanged: ", path);
            return clone(path);
        }
        
        std::string filename = p.filename().string();
        std::string lower_filename = filename;
        std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);
        logger.debug("Looking for case-insensitive match for '", filename, "' (lowercase: '", lower_filename, "')");

        // Check cache first
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto it = cache.find(p.string());
            if (it != cache.end()) {
                logger.debug("Cache hit: ", p.string(), " -> ", it->second);
                return clone(it->second.c_str());
            }
            logger.trace("Cache miss for ", p.string());
        }

        std::unique_ptr<char[]> path_new = clone(path);
        
        // Check if parent path exists
        logger.trace("Checking parent path: ", p.parent_path().c_str());
        if (!file_exists_real(p.parent_path().c_str())) {
            logger.debug("Parent path doesn't exist, trying to find it recursively: ", 
                         p.parent_path().c_str());
            // Try to find the parent path recursively
            auto parent_path = replace_filename_case_insensitive(p.parent_path().c_str());
            if (parent_path) {
                fs::path new_parent(parent_path.get());
                if (file_exists_real(new_parent.c_str())) {
                    logger.debug("Found parent path: ", new_parent.c_str());
                    p = new_parent / p.filename();
                } else {
                    logger.debug("Could not find parent path: ", new_parent.c_str());
                }
            }
        }

        // We need to use a different approach to iterate through directories
        // to avoid calling our intercepted functions
        try {
            logger.debug("Opening directory: ", p.parent_path().c_str());
            DIR* dir = opendir_real(p.parent_path().c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string direntry = entry->d_name;
                    
                    // Skip . and .. entries
                    if (direntry == "." || direntry == "..") continue;
                    
                    std::string lower_direntry = direntry;
                    std::transform(lower_direntry.begin(), lower_direntry.end(), lower_direntry.begin(), ::tolower);
                    
                    logger.trace("Comparing '", filename, "' (lowercase: '", lower_filename, 
                                "') with '", direntry, "' (lowercase: '", lower_direntry, "')");
                    
                    if (lower_filename == lower_direntry) {
                        fs::path new_path = p.parent_path() / direntry;
                        logger.info("Found case-insensitive match: ", path, " -> ", new_path.c_str());
                        path_new = clone(new_path.c_str());

                        // Update cache
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        cache[p.string()] = new_path.string();
                        break;
                    }
                }
                closedir(dir);
                
                if (strcmp(path, path_new.get()) == 0) {
                    logger.debug("No case-insensitive match found, using original path: ", path);
                }
            } else {
                logger.warning("Could not open directory: ", p.parent_path().c_str());
            }
        } catch (const std::exception& e) {
            logger.error("Exception while searching for case-insensitive match: ", e.what());
            // Just return the original path if we encounter any errors
        }

        return path_new;
    }

    Wrapper() {
        // Bind all function pointers to the original functions
        BIND(open);
        BIND(open64);
        BIND(openat);
        BIND(stat);
        BIND(lstat);
        BIND(fstatat);
        BIND(access);
        BIND(faccessat);
        BIND(opendir);
        BIND(readlink);
        BIND(readdir);
        BIND(closedir);
        
        logger.info("Initialization complete, debug level: ", logger.getLevel());
    }

public:

    // Implementation of wrap_func to handle function calls with logging
    template<typename Func, typename ReturnType, typename... Args>
    ReturnType wrap_func(const char* func_name, Func func, Args... args) {
        auto result = func(args...);
        logger.debug("EXIT: ", func_name, "() -> ", result);
        return result;
    }

    // Implementation of case_adjusted_path to handle path adjustment with logging
    template<typename... Args>
    std::unique_ptr<char[]> case_adjusted_path(const char* func_name, const char* path) {
        logger.debug("ENTER: ", func_name, "(", path ? path : "(null)", ")");
        
        std::unique_ptr<char[]> adjusted_path = replace_filename_case_insensitive(path);
        
        if (path && adjusted_path.get() && strcmp(path, adjusted_path.get()) != 0) {
            logger.debug("ADJUST: ", func_name, " path ", path, " -> ", adjusted_path.get());
        }
        
        return adjusted_path;
    }

    // Define all function pointers for the functions we'll intercept
    DEF(open);
    DEF(open64);
    DEF(openat);
    DEF(stat);
    DEF(lstat);
    DEF(fstatat);
    DEF(access);
    DEF(faccessat);
    DEF(opendir);
    DEF(readlink);
    DEF(readdir);
    DEF(closedir);

    DebugLogger logger;

    static Wrapper& get()
    {
        // Do not destroy, since the order of destruction is undetermined.
        static Wrapper* wrapper = nullptr;
        if (!wrapper) wrapper = new Wrapper();
        return *wrapper;
    }
};

// Helper macros to make function calls cleaner
#define WRAP(func, ...) \
    Wrapper::get().wrap_func<decltype(Wrapper::get().func##_real), decltype(Wrapper::get().func##_real(__VA_ARGS__))>(#func, Wrapper::get().func##_real, __VA_ARGS__)

#define CASE(func, path) \
    Wrapper::get().case_adjusted_path(#func, path).get()

int open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    return WRAP(open, CASE(open, path), flags, mode);
}

int open64(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    return WRAP(open64, CASE(open64, path), flags, mode);
}

int openat(int dirfd, const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    return WRAP(openat, dirfd, CASE(openat, path), flags, mode);
}

int stat(const char *path, struct stat *buf) {
    return WRAP(stat, CASE(stat, path), buf);
}

int lstat(const char *path, struct stat *buf) {
    return WRAP(lstat, CASE(lstat, path), buf);
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
    return WRAP(fstatat, dirfd, CASE(fstatat, path), buf, flags);
}

int access(const char *path, int mode) {
    return WRAP(access, CASE(access, path), mode);
}

int faccessat(int dirfd, const char *path, int mode, int flags) {
    return WRAP(faccessat, dirfd, CASE(faccessat, path), mode, flags);
}

DIR *opendir(const char *path) {
    return WRAP(opendir, CASE(opendir, path));
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    return WRAP(readlink, CASE(readlink, path), buf, bufsiz);
}
