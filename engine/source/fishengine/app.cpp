#include "app.h"


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
    if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path,
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

const char *ApplicationFilePath() {
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

#include <fmt/format.h>
#include <assert.h>

extern "C" {
bool ConvertToDDS(const char *path);
}

bool ConvertToDDS(const char *path) {
    std::filesystem::path p(path);
    p = p.lexically_normal();
    assert(std::filesystem::exists(p));
    const auto dir = p.parent_path();

    std::filesystem::path texconv = ApplicationFilePath();
    texconv = texconv / "texconv.exe";
    texconv = texconv.lexically_normal();
    assert(std::filesystem::exists(texconv));
    const auto cmd = fmt::format(R"({} -pow2 -f BC1_UNORM -y -o "{}" "{}")",
                                 texconv.string(), dir.string(), p.string());
    puts(cmd.c_str());
    int ret = system(cmd.c_str());
    return ret == 0;
}