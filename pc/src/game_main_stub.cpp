// The game's entry point lives in src/Game/System/GameSystem.cpp (aurora_main, see aurora/main.h).
// This file only exists because CMake needs at least one source for the executable target;
// referencing the symbol pulls GameSystem.obj out of the System library.
extern "C" int aurora_main(int argc, char* argv[]);
extern "C" int (*const smg_force_link_main)(int, char*[]) = &aurora_main;
