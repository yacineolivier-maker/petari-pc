// The player's user directory for the PC port (saves, settings, logs).
#include "pc/user_files.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>

namespace {
std::string sUserDir;
}

extern "C" const char* pc_user_dir(void) {
    if (sUserDir.empty()) {
        if (char* p = SDL_GetPrefPath(nullptr, "Super Mario Galaxy")) {
            sUserDir = p;
            SDL_free(p);
        } else {
            sUserDir = "./";
        }
    }
    return sUserDir.c_str();
}

extern "C" void pc_log_file_init(void) {
#ifdef _WIN32
    // Only when there is no console (the shortcut starts a windowed app).
    if (std::getenv("SMGPC_CONSOLE") != nullptr) {
        return;
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::u8path(pc_user_dir()) / "logs";
    fs::create_directories(dir, ec);
    char name[64];
    std::time_t t = std::time(nullptr);
    std::strftime(name, sizeof(name), "smgpc-%Y%m%d-%H%M%S.log", std::localtime(&t));
    std::string path = (dir / name).string();
    std::freopen(path.c_str(), "w", stdout);
    std::freopen(path.c_str(), "a", stderr);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
#endif
}
