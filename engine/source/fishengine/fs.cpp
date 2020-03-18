#include "fs.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::string ReadFileAsString(const std::string& path) {
    assert(fs::exists(path));
    std::ifstream f(path);
    if (f) {
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    return "";
}

bool ReadBinaryFile(const std::string& path, std::vector<char>& bin) {
    assert(fs::exists(path));
    std::ifstream input(path.c_str(), std::ios::binary);
    assert(input.is_open());
    input.seekg(0, std::ios::end);
    size_t size = input.tellg();
    input.seekg(0, std::ios::beg);
    bin.resize(size);
    input.read(bin.data(), size);
    input.close();
    return true;
}

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"

// from
// https://stackoverflow.com/questions/516200/relative-paths-not-working-in-xcode-c
// ----------------------------------------------------------------------------
// This makes relative paths work in C++ in Xcode by changing directory to the
// Resources folder inside the .app bundle
std::string ApplicationFilePath() {
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8*)path,
                                          PATH_MAX)) {
        // error!
        abort();
    }
    CFRelease(resourcesURL);

    chdir(path);
    //	std::cout << "Current Path: " << path << std::endl;
    return std::string(path);
}
#endif
// ----------------------------------------------------------------------------

#if _WIN32
#include <Shlwapi.h>

#include <filesystem>

char g_exe_path[MAX_PATH];

const char* ApplicationFilePath() {
    auto hModule = GetModuleHandleW(NULL);
    char path2[MAX_PATH];
    GetModuleFileNameA(hModule, path2, MAX_PATH);
    std::filesystem::path p(path2);
    std::string p2 = p.parent_path().string();
    memcpy(g_exe_path, p2.c_str(), p2.length());
    return g_exe_path;
}
#endif

#if __linux__
// #include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>  // memset

#include <filesystem>

// https://stackoverflow.com/questions/4025370/can-an-executable-discover-its-own-path-linux
std::string ApplicationFilePath() {
    char dest[PATH_MAX];
    memset(dest, 0, sizeof(dest));
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1) {
        puts("[fatal error] readlink failed!");
        abort();
    }
    std::filesystem::path p(dest);
    return p.parent_path().string();
}
#endif
