// PC display settings: <user dir>/display.ini (written with the defaults on first launch).
#include "pc/display.h"

#include "pc/user_files.h"

#include <dolphin/gx/GXAurora.h>
#include <dolphin/gx/GXExtra.h>
#include <dolphin/vi.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

PcSettings g_settings;
std::string g_path;

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
        } else if (key == "supersampling" && n >= 1 && n <= 4) {
            g_settings.supersampling = n;
        } else if (key == "distance_affichage") {
            const float f = float(std::atof(value.c_str()));
            if (f >= 1.0f && f <= 4.0f) {
                g_settings.drawDistance = f;
            }
        } else if (key == "particules") {
            const float f = float(std::atof(value.c_str()));
            if (f >= 1.0f && f <= 3.0f) {
                g_settings.particleDensity = f;
            }
        } else if (key == "vitesse_pointeur") {
            const float f = float(std::atof(value.c_str()));
            if (f >= 0.5f && f <= 2.0f) {
                g_settings.pointerSpeed = f;
            }
        } else if (key == "deux_joueurs") {
            g_settings.coStar = ParseBool(value);
        } else if (key == "rayons_soleil") {
            const float f = float(std::atof(value.c_str()));
            if (f >= 0.0f && f <= 2.0f) {
                g_settings.godRays = f;
            }
        } else if (key == "eclairage_par_pixel") {
            g_settings.pixelLighting = ParseBool(value);
        } else if (key == "fxaa") {
            g_settings.fxaa = ParseBool(value);
        } else if (key == "nettete") {
            const float f = float(std::atof(value.c_str()));
            if (f >= 0.0f && f <= 1.0f) {
                g_settings.sharpen = f;
            }
        } else if (key == "couleurs_vives") {
            const float f = float(std::atof(value.c_str()));
            if (f >= -1.0f && f <= 1.0f) {
                g_settings.vibrance = f;
            }
        } else if (key == "langue" && (value == "fr" || value == "en" || value == "es")) {
            g_settings.language = value;
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
                 "backend = %s\n"
                 "; Surechantillonnage : rendu interne a N fois la taille de la fenetre, puis reduit\n"
                 "; (1 = desactive, 2 = par defaut, image nettement plus fine mais demande plus de puissance)\n"
                 "supersampling = %d\n"
                 "; Langue du jeu : fr (francais), en (anglais) ou es (espagnol)\n"
                 "langue = %s\n"
                 "\n"
                 "; --- Ameliorations ---\n"
                 "; Distance d'affichage : multiplie les distances ou les objets passent en version\n"
                 "; simplifiee ou disparaissent (1 = Wii, jusqu'a 4)\n"
                 "distance_affichage = %.1f\n"
                 "; Densite des particules (poussieres, etincelles...) : 1 = Wii, jusqu'a 3\n"
                 "particules = %.1f\n"
                 "; Rayons de soleil la ou le jeu affiche un reflet du soleil : 0 = desactive,\n"
                 "; sinon intensite (jusqu'a 2)\n"
                 "rayons_soleil = %.1f\n"
                 "; Eclairage calcule pour chaque pixel au lieu de chaque sommet (reliefs plus lisses)\n"
                 "eclairage_par_pixel = %d\n"
                 "; Anticrenelage FXAA de l'image finale (utile sans surechantillonnage ni msaa)\n"
                 "fxaa = %d\n"
                 "; Nettete de l'image finale : 0 = desactivee, jusqu'a 1\n"
                 "nettete = %.2f\n"
                 "; Couleurs plus vives : 0 = Wii, jusqu'a 1 (valeurs negatives = plus ternes)\n"
                 "couleurs_vives = %.2f\n"
                 "\n"
                 "; Mode deux joueurs (co-star) : avec deux manettes, la deuxieme pilote le pointeur\n"
                 "; etoile du second joueur (1 = active)\n"
                 "deux_joueurs = %d\n"
                 "; Vitesse du pointeur etoile au stick droit : 0.5 a 2 (1 = normale)\n"
                 "vitesse_pointeur = %.2f\n",
                 g_settings.fullscreen ? 1 : 0, g_settings.windowWidth, g_settings.windowHeight, g_settings.msaa,
                 g_settings.anisotropy, g_settings.alphaToCoverage ? 1 : 0, g_settings.textureMipmaps ? 1 : 0,
                 g_settings.vsync ? 1 : 0, g_settings.backend.c_str(), g_settings.supersampling,
                 g_settings.language.c_str(), g_settings.drawDistance, g_settings.particleDensity,
                 g_settings.godRays, g_settings.pixelLighting ? 1 : 0, g_settings.fxaa ? 1 : 0, g_settings.sharpen,
                 g_settings.vibrance, g_settings.coStar ? 1 : 0, g_settings.pointerSpeed);
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
    g_path = path;
    // Test overrides (not saved, unless the settings menu saves them).
    if (const char* env = std::getenv("SMGPC_VSYNC")) {
        g_settings.vsync = ParseBool(env);
    }
    if (const char* env = std::getenv("SMGPC_SUPERSAMPLE")) {
        g_settings.supersampling = std::atoi(env);
    }
    if (const char* env = std::getenv("SMGPC_DRAW_DISTANCE")) {
        g_settings.drawDistance = float(std::atof(env));
    }
    if (const char* env = std::getenv("SMGPC_PARTICLES")) {
        g_settings.particleDensity = float(std::atof(env));
    }
    if (const char* env = std::getenv("SMGPC_GOD_RAYS")) {
        g_settings.godRays = float(std::atof(env));
    }
    if (const char* env = std::getenv("SMGPC_PIXEL_LIGHTING")) {
        g_settings.pixelLighting = ParseBool(env);
    }
    if (const char* env = std::getenv("SMGPC_FXAA")) {
        g_settings.fxaa = ParseBool(env);
    }
    if (const char* env = std::getenv("SMGPC_SHARPEN")) {
        g_settings.sharpen = float(std::atof(env));
    }
    if (const char* env = std::getenv("SMGPC_VIBRANCE")) {
        g_settings.vibrance = float(std::atof(env));
    }
    if (const char* env = std::getenv("SMGPC_CO_STAR")) {
        g_settings.coStar = ParseBool(env);
    }
    if (const char* env = std::getenv("SMGPC_LANGUAGE")) {
        g_settings.language = env;
    }
    // SMGPC_WINDOW_SIZE=<w>x<h> (e.g. 2100x900 to test an ultrawide window).
    if (const char* env = std::getenv("SMGPC_WINDOW_SIZE")) {
        int w = 0, h = 0;
        if (std::sscanf(env, "%dx%d", &w, &h) == 2 && w >= 320 && h >= 240) {
            g_settings.windowWidth = w;
            g_settings.windowHeight = h;
        }
    }
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
    pc_settings_apply_supersampling();
    pc_settings_apply_lighting();
    pc_settings_apply_post_process();
}

void pc_settings_apply_lighting() {
    GXSetPerPixelLighting(g_settings.pixelLighting);
}

void pc_settings_apply_post_process() {
    aurora_set_post_process(g_settings.fxaa, g_settings.sharpen, g_settings.vibrance);
}

void pc_settings_apply_supersampling() {
    const int n = g_settings.supersampling;
    VISetFrameBufferSupersample(float(n > 1 ? n : 1));
    aurora_set_resampler(n > 1 ? SAMPLER_AREA : SAMPLER_BILINEAR);  // box-filtered downsampling
}

PcSettings& pc_settings() {
    return g_settings;
}

void pc_settings_save() {
    if (!g_path.empty()) {
        Save(g_path);
    }
}

// Enhancements read by the game code.
float pc_settings_draw_distance() {
    return g_settings.drawDistance;
}

float pc_settings_particle_density() {
    return g_settings.particleDensity;
}

float pc_settings_god_rays() {
    return g_settings.godRays;
}

// Game language as an SC_LANG_* value (RMGE01 has US English, US Spanish and US French).
extern "C" unsigned char pc_settings_language() {
    if (g_settings.language == "en") {
        return 1;  // SC_LANG_ENGLISH
    }
    if (g_settings.language == "es") {
        return 4;  // SC_LANG_SPANISH
    }
    return 3;  // SC_LANG_FRENCH
}

bool pc_display_vsync() {
    return g_settings.vsync;
}

bool pc_settings_co_star() {
    return g_settings.coStar;
}

float pc_settings_pointer_speed() {
    return g_settings.pointerSpeed;
}
