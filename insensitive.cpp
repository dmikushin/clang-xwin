// A shared library to intercept Clang header file accesses and make filenames case-insensitive
// clang++-20 -g -O0 -std=c++17 -fPIC insensitive.cpp -shared -o libinsensitive.so

#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
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

#define DEF(func) decltype(&func) func##_real
#define STR(x) #x
#define BIND(func) func##_real = getFunctionPointer<decltype(&func)>(STR(func))

namespace fs = std::filesystem;

// Add a cache and a mutex for thread safety
class Wrapper {
    std::unordered_map<std::string, std::string> cache;
    std::mutex cache_mutex;

    template<typename T>
    T getFunctionPointer(const char* name) {
        T func = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
        if (!func) {
            std::cerr << "Error: Could not find original function " << name << std::endl;
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

public:
    bool file_exists_real(const char* path) {
        struct stat st;
        return stat_real(path, &st) == 0;
    }

    std::unique_ptr<char[]> replace_filename_case_insensitive(const char* path) {
        if (!path) return nullptr;
        fs::path p(path);
        
        // If path is empty or root directory, just return it
        if (p.empty() || p == p.root_path()) {
            return clone(path);
        }
        
        // If file exists with exact case, no need to search
        if (file_exists_real(path)) return clone(path);
        
        std::string filename = p.filename().string();
        std::string lower_filename = filename;
        std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);

        // Check cache first
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto it = cache.find(p.string());
            if (it != cache.end()) {
                return clone(it->second.c_str());
            }
        }

        std::unique_ptr<char[]> path_new = clone(path);
        
        // Check if parent path exists
        if (!file_exists_real(p.parent_path().c_str())) {
            // Try to find the parent path recursively
            auto parent_path = replace_filename_case_insensitive(p.parent_path().c_str());
            if (parent_path) {
                fs::path new_parent(parent_path.get());
                if (file_exists_real(new_parent.c_str())) {
                    p = new_parent / p.filename();
                }
            }
        }

        // We need to use a different approach to iterate through directories
        // to avoid calling our intercepted functions
        try {
            DIR* dir = opendir_real(p.parent_path().c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string direntry = entry->d_name;
                    
                    // Skip . and .. entries
                    if (direntry == "." || direntry == "..") continue;
                    
                    std::string lower_direntry = direntry;
                    std::transform(lower_direntry.begin(), lower_direntry.end(), lower_direntry.begin(), ::tolower);
                    
                    if (lower_filename == lower_direntry) {
                        fs::path new_path = p.parent_path() / direntry;
                        path_new = clone(new_path.c_str());

                        // Update cache
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        cache[p.string()] = new_path.string();
                        break;
                    }
                }
                closedir(dir);
            }
        } catch (const std::exception& e) {
            // Just return the original path if we encounter any errors
        }

        return path_new;
    }

    }

    // Define all function pointers for the functions we'll intercept
    DEF(open);
    DEF(open64);
    DEF(openat);
    DEF(stat);
    DEF(lstat);
    DEF(fstatat);
    DEF(__xstat);
    DEF(__lxstat);
    DEF(__fxstatat);
    DEF(access);
    DEF(faccessat);
    DEF(opendir);
    DEF(readlink);
    DEF(readdir);
    DEF(closedir);

    Wrapper() {
        // Bind all function pointers to the original functions
        BIND(open);
        BIND(open64);
        BIND(openat);
        BIND(stat);
        BIND(lstat);
        BIND(fstatat);
        BIND(__xstat);
        BIND(__lxstat);
        BIND(__fxstatat);
        BIND(access);
        BIND(faccessat);
        BIND(opendir);
        BIND(readlink);
        BIND(readdir);
        BIND(closedir);
    }
};

static Wrapper wrapper;

#define WRAP(func) wrapper.func##_real
#define CASE(path) wrapper.replace_filename_case_insensitive(path).get()

// Intercept open() and its variants
int open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    return WRAP(open)(CASE(path), flags, mode);
}

int open64(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    return WRAP(open64)(CASE(path), flags, mode);
}

int openat(int dirfd, const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    return WRAP(openat)(dirfd, CASE(path), flags, mode);
}

// Intercept stat() and its variants
int stat(const char *path, struct stat *buf) {
    return WRAP(stat)(CASE(path), buf);
}

int lstat(const char *path, struct stat *buf) {
    return WRAP(lstat)(CASE(path), buf);
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
    return WRAP(fstatat)(dirfd, CASE(path), buf, flags);
}

// Intercept __xstat and its variants (used by glibc internally)
int __xstat(int ver, const char *path, struct stat *buf) {
    return WRAP(__xstat)(ver, CASE(path), buf);
}

int __lxstat(int ver, const char *path, struct stat *buf) {
    return WRAP(__lxstat)(ver, CASE(path), buf);
}

int __fxstatat(int ver, int dirfd, const char *path, struct stat *buf, int flags) {
    return WRAP(__fxstatat)(ver, dirfd, CASE(path), buf, flags);
}

// Intercept access() and its variants
int access(const char *path, int mode) {
    return WRAP(access)(CASE(path), mode);
}

int faccessat(int dirfd, const char *path, int mode, int flags) {
    return WRAP(faccessat)(dirfd, CASE(path), mode, flags);
}

// Intercept opendir()
DIR *opendir(const char *path) {
    return WRAP(opendir)(CASE(path));
}

// Intercept readlink()
ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    return WRAP(readlink)(CASE(path), buf, bufsiz);
}
