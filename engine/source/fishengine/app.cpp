#include "app.h"
#include <fmt/format.h>
#include <assert.h>
#include "fs.hpp"

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