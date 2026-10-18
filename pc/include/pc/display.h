#pragma once

#include <aurora/aurora.h>

#include <string>

// display.ini contents (pc/src/display.cpp). The in-game settings menu (pc/src/ui/settings_menu.cpp)
// edits them live; the game reads the enhancement values every frame.
struct PcSettings {
    bool fullscreen = false;
    int windowWidth = 1280;
    int windowHeight = 720;
    int msaa = 4;
    int anisotropy = 16;
    bool alphaToCoverage = true;
    bool textureMipmaps = true;
    bool vsync = true;
    std::string backend = "auto";
    int supersampling = 2;
    float drawDistance = 2.0f;
    float particleDensity = 1.5f;
    float godRays = 0.6f;
    bool pixelLighting = true;
    bool fxaa = false;        // final image anti-aliasing (useful without supersampling/MSAA)
    float sharpen = 0.3f;     // contrast-adaptive sharpening, 0-1
    float vibrance = 0.1f;    // saturation boost of dull colors, -1-1
    bool coStar = false;
    float pointerSpeed = 1.0f;  // right stick Star Pointer speed, 0.5-2
    float masterVolume = 1.0f;   // whole game audio, 0-1
    float speakerVolume = 0.8f;  // Wii Remote speaker sounds on the PC output, 0 = TV fallbacks
    std::string language = "fr";
};

PcSettings& pc_settings();

// Writes display.ini with the current settings.
void pc_settings_save();

// Applies the internal resolution (supersampling) now.
void pc_settings_apply_supersampling();

// Applies the lighting mode (per pixel or per vertex) now.
void pc_settings_apply_lighting();

// Applies the final post-process pass (FXAA, sharpening, vibrance) now.
void pc_settings_apply_post_process();

// Reads <user dir>/display.ini (creating it with the defaults) into the Aurora configuration.
void pc_display_configure(AuroraConfig* config, bool forceFullscreen);

// Settings that are applied once Aurora is initialized (alpha to coverage, texture mipmaps).
void pc_display_apply_after_init();

// Whether vsync is on (display.ini "vsync").
bool pc_display_vsync();
