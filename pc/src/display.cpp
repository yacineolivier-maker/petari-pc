// PC display settings: <user dir>/display.ini (written with the defaults on first launch).
#include "pc/display.h"

#include "pc/user_files.h"

#include <dolphin/gx/GXAurora.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

struct Settings {
    bool fullscreen = false;
    int windowWidth = 1280;
    int windowHeight = 720;
    int msaa = 4;
    int anisotropy = 16;
    bool alphaToCoverage = true;
    bool textureMipmaps = true;
    bool vsync = true;
    std::string backend = "auto";
};

Settings g_settings;

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    const size_t e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

bool ParseBool(std::string v) {
    for (char& c : v) {
        c = char(std::tolower((unsigned char)c));
    }
    return v == "1" || v == "on" || v == "yes" || v == "true" || v == "oui";
}

void Load(FILE* f) {
    char line[256];
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        const std::string s = Trim(line);
        if (s.empty() || s[0] == ';' || s[0] == '#') {
            continue;
        }
        const size_t eq = s.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = Trim(s.substr(0, eq));
        const std::string value = Trim(s.substr(eq + 1));
        const int n = std::atoi(value.c_str());
        if (key == "fullscreen") {
            g_settings.fullscreen = ParseBool(value);
        } else if (key == "window_width" && n >= 320) {
            g_settings.windowWidth = n;
        } else if (key == "window_height" && n >= 240) {
            g_settings.windowHeight = n;
        } else if (key == "msaa" && (n == 1 || n == 2 || n == 4 || n == 8)) {
            g_settings.msaa = n;
        } else if (key == "anisotropy" && n >= 1 && n <= 16) {
            g_settings.anisotropy = n;
        } else if (key == "alpha_to_coverage") {
            g_settings.alphaToCoverage = ParseBool(value);
        } else if (key == "texture_mipmaps") {
            g_settings.textureMipmaps = ParseBool(value);
        } else if (key == "vsync") {
            g_settings.vsync = ParseBool(value);
        } else if (key == "backend") {
            g_settings.backend = value;
        }
    }
}

void Save(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return;
    }
    std::fprintf(f,
                 "; Super Mario Galaxy (portage PC) - reglages d'affichage. Modifiez puis relancez le jeu.\n"
                 "; F11 ou Alt+Entree basculent le plein ecran pendant le jeu.\n"
                 "\n"
                 "; Demarrer en plein ecran (1/0)\n"
                 "fullscreen = %d\n"
                 "; Taille de la fenetre\n"
                 "window_width = %d\n"
                 "window_height = %d\n"
                 "; Anticrenelage multi-echantillons : 1, 2, 4 ou 8\n"
                 "msaa = %d\n"
                 "; Filtrage anisotrope des textures : 1 a 16\n"
                 "anisotropy = %d\n"
                 "; Bords lisses pour le feuillage et les grilles (avec msaa > 1)\n"
                 "alpha_to_coverage = %d\n"
                 "; Mipmaps generees pour les textures qui n'en ont pas (moins de scintillement au loin)\n"
                 "texture_mipmaps = %d\n"
                 "; Synchronisation verticale\n"
                 "vsync = %d\n"
                 "; API graphique : auto, d3d12, vulkan ou d3d11\n"
                 "backend = %s\n",
                 g_settings.fullscreen ? 1 : 0, g_settings.windowWidth, g_settings.windowHeight, g_settings.msaa,
                 g_settings.anisotropy, g_settings.alphaToCoverage ? 1 : 0, g_settings.textureMipmaps ? 1 : 0,
                 g_settings.vsync ? 1 : 0, g_settings.backend.c_str());
    std::fclose(f);
}

}  // namespace

void pc_display_configure(AuroraConfig* config, bool forceFullscreen) {
    const std::string path = std::string(pc_user_dir()) + "/display.ini";
    if (FILE* f = std::fopen(path.c_str(), "r")) {
        Load(f);
        std::fclose(f);
    }
    Save(path);  // rewrites it with any new keys and the current values
    config->startFullscreen = g_settings.fullscreen || forceFullscreen;
    config->windowWidth = uint32_t(g_settings.windowWidth);
    config->windowHeight = uint32_t(g_settings.windowHeight);
    config->msaa = uint32_t(g_settings.msaa);
    config->maxTextureAnisotropy = uint16_t(g_settings.anisotropy);
    config->vsync = g_settings.vsync;
    // SMGPC_BACKEND overrides the file (tests).
    std::string backend = g_settings.backend;
    if (const char* env = std::getenv("SMGPC_BACKEND")) {
        backend = env;
    }
    for (char& c : backend) {
        c = char(std::tolower((unsigned char)c));
    }
    if (backend == "d3d12") {
        config->desiredBackend = BACKEND_D3D12;
    } else if (backend == "vulkan") {
        config->desiredBackend = BACKEND_VULKAN;
    } else if (backend == "d3d11") {
        config->desiredBackend = BACKEND_D3D11;
    }
}

void pc_display_apply_after_init() {
    AuroraSetAlphaToCoverage(g_settings.alphaToCoverage && g_settings.msaa > 1 ? 1 : 0);
    AuroraSetAutoMipmaps(g_settings.textureMipmaps ? 1 : 0);
}
