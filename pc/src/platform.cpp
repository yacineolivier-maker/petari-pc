// PC platform bring-up for Super Mario Galaxy: Aurora, the OS layer, the disc image, frames.

#include "pc/platform.h"

#include "pc/crash_handler.h"
#include "pc/main.h"
#include "pc/os.h"
#include "pc/user_files.h"

#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/event.h>

#include <revolution/os.h>
#include <revolution/vi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace pc {
bool IsShuttingDown = false;
}

namespace {

OSThread sMainThread;
bool sFrameStarted = false;

void AuroraLog(AuroraLogLevel level, const char* module, const char* message, unsigned int len) {
    static const char* names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
    FILE* out = level >= LOG_ERROR ? stderr : stdout;
    std::fprintf(out, "[%s: %s] %.*s\n", names[level], module, (int)len, message);
    if (level == LOG_FATAL) {
        std::fflush(out);
        std::abort();
    }
}

// The disc image: the first argument that isn't an option, else the user's RVZ next to the game
// folder (D:\SUPER_MARIO_GALAXY\Rom), else orig/RMGE01 (the extracted disc, for development).
const char* FindDisc(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            return argv[i];
        }
    }
    static std::string found;
    namespace fs = std::filesystem;
    std::error_code ec;
    const char* candidates[] = {
        "Rom",
        "../Rom",
        "../../Rom",
        "../../../Rom",
    };
    for (const char* dir : candidates) {
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            const auto ext = entry.path().extension().string();
            if (ext == ".rvz" || ext == ".iso" || ext == ".wbfs" || ext == ".ciso") {
                found = entry.path().string();
                return found.c_str();
            }
        }
    }
    return "orig/RMGE01/sys/main.dol";
}

}  // namespace

extern "C" void pc_platform_init(int argc, char* argv[]) {
    pc_log_file_init();
    pc_install_crash_handler();
    pc_watchdog_tick();  // SMGPC_WATCHDOG=N also covers the boot, before the first frame
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    bool startFullscreen = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--fullscreen") == 0) {
            startFullscreen = true;
        }
    }

    AuroraConfig config = {};
    config.appName = "Super Mario Galaxy";
    config.logCallback = &AuroraLog;
    config.logLevel = LOG_INFO;
    config.windowPosX = -1;
    config.windowPosY = -1;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.vsync = true;
    config.startFullscreen = startFullscreen;
    config.mem1Size = 256 * 1024 * 1024;  // Emulated MEM1 (the Wii has 24 MB).
    config.mem2Size = 16 * 1024 * 1024;   // Aurora's ARAM; the Wii's MEM2 is pc/src/os/wii_arena.cpp.
    aurora_initialize(argc, argv, &config);

    OSInit();
    OSPCAdoptCurrentThread(&sMainThread);

    const char* disc = FindDisc(argc, argv);
    if (!aurora_dvd_open(disc)) {
        std::fprintf(stderr, "[pc] could not open the disc image '%s'.\n"
                             "Usage: smgpc.exe [--fullscreen] [path/to/Super Mario Galaxy.rvz]\n",
                     disc);
        pc_platform_exit(1);
    }
    std::printf("[pc] disc: %s\n", disc);
}

extern "C" int pc_platform_begin_frame(void) {
    // aurora_begin_frame fails while the window can't be drawn to (e.g. minimized): keep pumping.
    while (true) {
        const AuroraEvent* event = aurora_update();
        while (event != nullptr && event->type != AURORA_NONE) {
            if (event->type == AURORA_EXIT) {
                pc_platform_exit(0);
            }
            event++;
        }
        if (pc::IsShuttingDown) {
            return 0;
        }
        if (aurora_begin_frame()) {
            break;
        }
    }
    sFrameStarted = true;
    pc_watchdog_tick();
    // SMGPC_LOG_FRAMES=1: print the frame number (first 10 frames, then every 60).
    static const bool sLogFrames = std::getenv("SMGPC_LOG_FRAMES") != nullptr;
    static u32 sFrameCount = 0;
    sFrameCount++;
    if (sLogFrames && (sFrameCount <= 10 || sFrameCount % 60 == 0)) {
        std::fprintf(stderr, "[pc] frame %u\n", sFrameCount);
    }
    // Drives the VI retrace callbacks once per frame, like the Wii's VI interrupt: JUTVideo's
    // post-retrace callback posts the message MainLoopFramework::waitForRetrace waits for.
    VIWaitForRetrace();
    return 1;
}

extern "C" void pc_platform_end_frame(void) {
    if (sFrameStarted) {
        aurora_end_frame();
        sFrameStarted = false;
    }
}

extern "C" void pc_platform_exit(int code) {
    pc::IsShuttingDown = true;
    aurora_shutdown();
    std::exit(code);
}
