#pragma once
// PC platform bring-up for Super Mario Galaxy (see pc/src/platform.cpp).

#ifdef __cplusplus
extern "C" {
#endif

// Replaces the Wii IPL/apploader: brings up Aurora (window, GX, emulated MEM1), the OS layer and
// the disc image, then returns so the original main() can run. Exits the process on failure.
// Usage: smgpc.exe [--fullscreen] [disc image (.rvz/.iso)]
void pc_platform_init(int argc, char* argv[]);

// Called at the top of each game frame: pumps window events. Returns 0 when the game should quit.
int pc_platform_begin_frame(void);

// Called at the end of each game frame: presents it.
void pc_platform_end_frame(void);

// Shuts Aurora down and exits the process.
void pc_platform_exit(int code);

#ifdef __cplusplus
}
#endif
