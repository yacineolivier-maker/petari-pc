// Wii system services for the PC port: system settings (SC) and the NAND file system.
//
// SC: the game takes its language from the console settings; the PC port answers French
// (the disc's UsFrench data), 16:9 and stereo.
// NAND: save files live under <user dir>/nand/<Wii path>, e.g.
// %APPDATA%/Super Mario Galaxy/nand/title/00010000/524d4745/data/GameData.bin.

#include <revolution/nand.h>
#include <revolution/sc.h>

#include "pc/user_files.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr int kMaxFiles = 16;
std::mutex sNandMutex;
FILE* sFiles[kMaxFiles];

fs::path HostPath(const char* nandPath) {
    std::string rel = nandPath != nullptr ? nandPath : "";
    while (!rel.empty() && rel[0] == '/') {
        rel.erase(0, 1);
    }
    return fs::path(pc_user_dir()) / "nand" / fs::u8path(rel);
}

FILE* FileOf(const NANDFileInfo* info) {
    if (info == nullptr || info->fileDescriptor < 0 || info->fileDescriptor >= kMaxFiles) {
        return nullptr;
    }
    return sFiles[info->fileDescriptor];
}

}  // namespace

extern "C" {

u8 SCGetLanguage(void) { return SC_LANG_FRENCH; }
u8 SCGetSoundMode(void) { return SC_SOUND_MODE_STEREO; }
u8 SCGetAspectRatio(void) { return SC_ASPECT_RATIO_16x9; }
u8 SCGetProgressiveMode(void) { return 1; }

// Screen dimming (the Wii's screen saver) is not emulated: report "not dimming".
u32 VIGetDimmingCount(void) { return 1; }

s32 NANDInit(void) { return NAND_RESULT_OK; }

s32 NANDGetHomeDir(char path[NAND_MAX_PATH]) {
    // RMGE01's title data directory.
    std::snprintf(path, NAND_MAX_PATH, "/title/00010000/524d4745/data");
    return NAND_RESULT_OK;
}

s32 NANDCreate(const char* path, u8, u8) {
    std::lock_guard lock(sNandMutex);
    fs::path p = HostPath(path);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    if (fs::exists(p, ec)) {
        return NAND_RESULT_EXISTS;
    }
    FILE* f = std::fopen(p.string().c_str(), "wb");
    if (f == nullptr) {
        return NAND_RESULT_ACCESS;
    }
    std::fclose(f);
    return NAND_RESULT_OK;
}

s32 NANDOpen(const char* path, NANDFileInfo* info, u8 mode) {
    std::lock_guard lock(sNandMutex);
    fs::path p = HostPath(path);
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        return NAND_RESULT_NOEXISTS;
    }
    int fd = -1;
    for (int i = 0; i < kMaxFiles; i++) {
        if (sFiles[i] == nullptr) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        return NAND_RESULT_MAXFD;
    }
    const char* fmode = mode == NAND_ACCESS_READ ? "rb" : "r+b";
    FILE* f = std::fopen(p.string().c_str(), fmode);
    if (f == nullptr) {
        return NAND_RESULT_ACCESS;
    }
    sFiles[fd] = f;
    std::memset(info, 0, sizeof(*info));
    info->fileDescriptor = fd;
    info->accType = mode;
    std::snprintf(info->origPath, NAND_MAX_PATH, "%s", path);
    return NAND_RESULT_OK;
}

s32 NANDPrivateOpen(const char* path, NANDFileInfo* info, u8 mode) { return NANDOpen(path, info, mode); }

s32 NANDClose(NANDFileInfo* info) {
    std::lock_guard lock(sNandMutex);
    FILE* f = FileOf(info);
    if (f == nullptr) {
        return NAND_RESULT_INVALID;
    }
    std::fclose(f);
    sFiles[info->fileDescriptor] = nullptr;
    info->fileDescriptor = -1;
    return NAND_RESULT_OK;
}

s32 NANDRead(NANDFileInfo* info, void* buf, u32 length) {
    std::lock_guard lock(sNandMutex);
    FILE* f = FileOf(info);
    if (f == nullptr) {
        return NAND_RESULT_INVALID;
    }
    return (s32)std::fread(buf, 1, length, f);
}

s32 NANDWrite(NANDFileInfo* info, const void* buf, u32 length) {
    std::lock_guard lock(sNandMutex);
    FILE* f = FileOf(info);
    if (f == nullptr) {
        return NAND_RESULT_INVALID;
    }
    s32 n = (s32)std::fwrite(buf, 1, length, f);
    std::fflush(f);
    return n;
}

s32 NANDSeek(NANDFileInfo* info, s32 offset, s32 whence) {
    std::lock_guard lock(sNandMutex);
    FILE* f = FileOf(info);
    if (f == nullptr) {
        return NAND_RESULT_INVALID;
    }
    int origin = whence == 0 ? SEEK_SET : whence == 1 ? SEEK_CUR : SEEK_END;
    if (std::fseek(f, offset, origin) != 0) {
        return NAND_RESULT_INVALID;
    }
    return (s32)std::ftell(f);
}

s32 NANDGetLength(NANDFileInfo* info, u32* length) {
    std::lock_guard lock(sNandMutex);
    FILE* f = FileOf(info);
    if (f == nullptr) {
        return NAND_RESULT_INVALID;
    }
    long cur = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    *length = (u32)std::ftell(f);
    std::fseek(f, cur, SEEK_SET);
    return NAND_RESULT_OK;
}

s32 NANDDelete(const char* path) {
    std::lock_guard lock(sNandMutex);
    std::error_code ec;
    fs::path p = HostPath(path);
    if (!fs::exists(p, ec)) {
        return NAND_RESULT_NOEXISTS;
    }
    fs::remove_all(p, ec);
    return ec ? NAND_RESULT_ACCESS : NAND_RESULT_OK;
}

// Moves `path` into directory `destDir` (the game writes a temp file, then moves it over the save).
s32 NANDMove(const char* path, const char* destDir) {
    std::lock_guard lock(sNandMutex);
    std::error_code ec;
    fs::path src = HostPath(path);
    if (!fs::exists(src, ec)) {
        return NAND_RESULT_NOEXISTS;
    }
    fs::path dst = HostPath(destDir) / src.filename();
    fs::create_directories(dst.parent_path(), ec);
    fs::remove(dst, ec);
    fs::rename(src, dst, ec);
    return ec ? NAND_RESULT_ACCESS : NAND_RESULT_OK;
}

// Free space check: plenty of room on PC.
s32 NANDCheck(u32, u32, u32* answer) {
    if (answer != nullptr) {
        *answer = 0;
    }
    return NAND_RESULT_OK;
}

void NANDInitBanner(NANDBanner* banner, u32, const u16*, const u16*) {
    if (banner != nullptr) {
        std::memset(banner, 0, sizeof(*banner));
    }
}

}  // extern "C"
