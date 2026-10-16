// PC platform bring-up for Super Mario Galaxy: Aurora, the OS layer, the disc image, frames.

#include "pc/platform.h"

#include "pc/crash_handler.h"
#include "pc/display.h"
#include "pc/main.h"
#include "pc/os.h"
#include "pc/user_files.h"

#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/event.h>
#include <aurora/gfx.h>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <revolution/os.h>
#include <revolution/vi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <vector>

bool pc_load_window_icon(std::vector<uint8_t>& rgba, uint32_t& size);
extern "C" unsigned long long pc_private_bytes(void);

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
    pc_display_configure(&config, startFullscreen);  // display.ini: window, MSAA, anisotropy...
    static std::vector<uint8_t> sIcon;
    uint32_t iconSize = 0;
    if (pc_load_window_icon(sIcon, iconSize)) {
        config.iconRGBA8 = sIcon.data();
        config.iconWidth = iconSize;
        config.iconHeight = iconSize;
    }
    aurora_initialize(argc, argv, &config);
    pc_display_apply_after_init();

    OSInit();
    OSPCAdoptCurrentThread(&sMainThread);

    const char* disc = FindDisc(argc, argv);
    if (!aurora_dvd_open(disc)) {
        std::fprintf(stderr, "[pc] could not open the disc image '%s'.\n"
                             "Usage: smgpc.exe [--fullscreen] [path/to/Super Mario Galaxy.rvz]\n",
                     disc);
        char message[1024];
        std::snprintf(message, sizeof(message),
                      "Impossible d'ouvrir l'image du disque de Super Mario Galaxy :\n%s\n\n"
                      "Placez le fichier .rvz / .iso du jeu (version US RMGE01) dans le dossier Rom "
                      // UTF-8 written out: this file's execution charset is Shift-JIS (game strings).
                      "\xC3\xA0 c\xC3\xB4t\xC3\xA9 du dossier du jeu, ou passez son chemin en argument :\n"
                      "smgpc.exe \"chemin\\vers\\Super Mario Galaxy.rvz\"",
                      disc);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Super Mario Galaxy", message, nullptr);
        pc_platform_exit(1);
    }
    std::printf("[pc] disc: %s\n", disc);
}

namespace {

// The game advances one tick per frame, timed by the Wii's 60 Hz retrace. With vsync on a 60 Hz
// display, presenting already paces the frames; on a faster display or without vsync, wait here.
bool NeedsFrameLimiter() {
    static int sCheckFrames = 0;
    static bool sNeeded = false;
    if (sCheckFrames-- > 0) {
        return sNeeded;
    }
    sCheckFrames = 120;  // the window may move to another display or toggle fullscreen
    sNeeded = !pc_display_vsync();
    int count = 0;
    if (SDL_Window** windows = SDL_GetWindows(&count)) {
        if (count > 0) {
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(windows[0]));
            if (mode != nullptr && mode->refresh_rate > 61.0f) {
                sNeeded = true;
            }
        }
        SDL_free(windows);
    }
    return sNeeded;
}

void LimitFrameRate() {
    using Clock = std::chrono::steady_clock;
    constexpr auto kFrame = std::chrono::nanoseconds(1'000'000'000LL * 1001 / 60000);  // NTSC 59.94 Hz
    static Clock::time_point sNext = Clock::now();
    if (!NeedsFrameLimiter()) {
        sNext = Clock::now();
        return;
    }
    auto now = Clock::now();
    if (now < sNext) {
        // Sleep most of the wait, spin the last 2 ms (Sleep granularity).
        const auto sleep = std::chrono::duration_cast< std::chrono::milliseconds >(sNext - now) - std::chrono::milliseconds(2);
        if (sleep.count() > 0) {
            SDL_Delay(static_cast< Uint32 >(sleep.count()));
        }
        while (Clock::now() < sNext) {
        }
        sNext += kFrame;
    } else {
        // Late (loading, hitch): don't try to catch up.
        sNext = now + kFrame;
    }
}

}  // namespace

extern "C" int pc_platform_begin_frame(void) {
    LimitFrameRate();
    // aurora_begin_frame fails while the window can't be drawn to (e.g. minimized): keep pumping.
    while (true) {
        const AuroraEvent* event = aurora_update();
        while (event != nullptr && event->type != AURORA_NONE) {
            if (event->type == AURORA_EXIT) {
                pc_platform_exit(0);
            }
            // F11 or Alt+Enter: toggle fullscreen.
            if (event->type == AURORA_SDL_EVENT && event->sdl.type == SDL_EVENT_KEY_DOWN && !event->sdl.key.repeat &&
                (event->sdl.key.key == SDLK_F11 || (event->sdl.key.key == SDLK_RETURN && (event->sdl.key.mod & SDL_KMOD_ALT)))) {
                if (SDL_Window* window = SDL_GetWindowFromID(event->sdl.key.windowID)) {
                    const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                    SDL_SetWindowFullscreen(window, !fullscreen);
                }
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
    // SMGPC_LOG_MEMORY=1: committed private memory, whenever it changed by 64 MB.
    static const bool sLogMemory = std::getenv("SMGPC_LOG_MEMORY") != nullptr;
    static u32 sMemoryFrames = 0;
    if (sLogMemory && ++sMemoryFrames != 0) {
        static unsigned long long sLast = 0;
        const unsigned long long now = pc_private_bytes();
        if (now > sLast + (64ull << 20) || now + (64ull << 20) < sLast) {
            std::fprintf(stderr, "[pc] memory: %llu MB private at frame %u\n", now >> 20, sMemoryFrames);
            sLast = now;
        }
    }
    // SMGPC_WINDOW_TITLE=<title>: rename the window (test scripts find their own instance by it).
    static bool sTitleSet = false;
    if (!sTitleSet) {
        sTitleSet = true;
        if (const char* title = std::getenv("SMGPC_WINDOW_TITLE")) {
            int count = 0;
            if (SDL_Window** windows = SDL_GetWindows(&count)) {
                for (int i = 0; i < count; i++) {
                    SDL_SetWindowTitle(windows[i], title);
                }
                SDL_free(windows);
            }
        }
    }
    // SMGPC_LOG_FRAMES=1: print the frame number (first 10 frames, then every 60).
    static const bool sLogFrames = std::getenv("SMGPC_LOG_FRAMES") != nullptr;
    static u32 sFrameCount = 0;
    sFrameCount++;
    if (sLogFrames && (sFrameCount <= 10 || sFrameCount % 60 == 0)) {
        // Frames per second over the last 60 frames.
        static auto sLastTime = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - sLastTime).count();
        sLastTime = now;
        const AuroraStats* stats = aurora_get_stats();
        std::fprintf(stderr, "[pc] frame %u: %.1f fps, %u draws (%u merged), %u pipelines created, %u queued\n", sFrameCount,
                     sFrameCount % 60 == 0 && seconds > 0.0 ? 60.0 / seconds : 0.0, stats ? stats->drawCallCount : 0,
                     stats ? stats->mergedDrawCallCount : 0, stats ? stats->createdPipelines : 0,
                     stats ? stats->queuedPipelines : 0);
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
