// A shared library to intercept Clang header file accesses and make filenames case-insensitive

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
    std::unique_ptr<char[]> replace_filename_case_insensitive(const char* path) {
        fs::path p(path);
        std::string filename = p.filename().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

        // Check cache first
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto it = cache.find(filename);
            if (it != cache.end()) {
                return clone(it->second.c_str());
            }
        }

        std::unique_ptr<char[]> path_new = clone(path);
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(p.parent_path(), ec)) {
            std::string direntry = entry.path().filename().string();
            std::transform(direntry.begin(), direntry.end(), direntry.begin(), ::tolower);
            if (filename == direntry) {
                path_new = clone(entry.path().c_str());

                // Update cache
                std::lock_guard<std::mutex> lock(cache_mutex);
                cache[filename] = entry.path().string();
                break;
            }
        }

        return path_new;
    }

    DEF(open);
    DEF(stat);

    Wrapper() {
        BIND(open);
        BIND(stat);
    }
};

static Wrapper wrapper;

#define WRAP(func) wrapper.func##_real
#define CASE(path) wrapper.replace_filename_case_insensitive(path).get()

int open(const char *path, int flags, ...) {
    va_list args;
    va_start(args, flags);
    int result = WRAP(open)(CASE(path), flags, args);
    va_end(args);
    return result;
}

int stat(const char *path, struct stat *buf) {
    return WRAP(stat)(CASE(path), buf);
}

