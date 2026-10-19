// In-game settings menu (Dear ImGui, drawn by Aurora over the game): F1, or View + Menu on the
// gamepad. Edits display.ini; most settings apply immediately, the others at the next launch.
#include "pc/settings_menu.h"

#include "pc/display.h"

#include <imgui.h>

#include <SDL3/SDL_video.h>

#include <cstdlib>
#include <filesystem>

// UI strings are UTF-8; this file's narrow literals are Shift-JIS (game strings), so use u8"".
#define T(s) reinterpret_cast< const char* >(u8##s)

namespace {

bool sOpen = false;
bool sDirty = false;
bool sFocusFirst = false;
bool sQuit = false;  // "Quitter le jeu" confirmed: exits at the end of the frame
PcSettings sAtLaunch;  // to tell which restart-only settings changed

SDL_Window* GameWindow() {
    int count = 0;
    SDL_Window* window = nullptr;
    if (SDL_Window** windows = SDL_GetWindows(&count)) {
        if (count > 0) {
            window = windows[0];
        }
        SDL_free(windows);
    }
    return window;
}

void Close() {
    sOpen = false;
    if (sDirty) {
        pc_settings_save();
        sDirty = false;
    }
}

// A slider for a float setting, in steps.
bool SliderStep(const char* label, float* value, float min, float max, float step, const char* format) {
    float v = *value;
    if (ImGui::SliderFloat(label, &v, min, max, format, ImGuiSliderFlags_AlwaysClamp)) {
        v = min + step * float(int((v - min) / step + 0.5f));
        if (v != *value) {
            *value = v;
            return true;
        }
    }
    return false;
}

void Hint(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
        ImGui::SetTooltip("%s", text);
    }
}

}  // namespace

void pc_settings_menu_init() {
    sAtLaunch = pc_settings();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;  // no imgui.ini next to the saves
    // Segoe UI (accents, readable); ImGui's built-in font otherwise.
    const char* windir = std::getenv("WINDIR");
    const std::filesystem::path font = std::filesystem::path(windir != nullptr ? windir : "C:\\Windows") / "Fonts" / "segoeui.ttf";
    std::error_code ec;
    if (std::filesystem::exists(font, ec)) {
        io.Fonts->AddFontFromFileTTF(font.string().c_str(), 32.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
    }
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.WindowPadding = ImVec2(20.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(12.0f, 10.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.07f, 0.16f, 0.94f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.22f, 0.55f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.98f, 0.80f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.90f, 0.40f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.98f, 0.80f, 0.20f, 1.0f);
}

bool pc_settings_menu_open() {
    return sOpen;
}

bool pc_settings_menu_quit_requested() {
    return sQuit;
}

void pc_settings_menu_toggle() {
    if (sOpen) {
        Close();
    } else {
        sOpen = true;
        sFocusFirst = true;
    }
}

void pc_settings_menu_draw() {
    if (!sOpen) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    // The font is 32 px for a 1080p window.
    io.FontGlobalScale = io.DisplaySize.y > 0.0f ? io.DisplaySize.y / 1080.0f : 1.0f;
    if (io.FontGlobalScale < 0.55f) {
        io.FontGlobalScale = 0.55f;
    }
    const float scale = io.FontGlobalScale;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(900.0f * scale, 0.0f), ImGuiCond_Always);
    // Fits its content, but scrolls instead of going past the bottom of a small window.
    ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f * scale, 0.0f), ImVec2(900.0f * scale, io.DisplaySize.y * 0.94f));
    bool open = true;
    if (!ImGui::Begin(T("Réglages  (F1 ou Affichage + Menu pour fermer)"), &open,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    PcSettings& s = pc_settings();
    ImGui::PushItemWidth(380.0f * scale);

    ImGui::SeparatorText(T("Affichage"));
    if (sFocusFirst) {
        ImGui::SetKeyboardFocusHere();
        sFocusFirst = false;
    }
    if (SDL_Window* window = GameWindow()) {  // F11 / Alt+Enter may have changed it
        s.fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    }
    if (ImGui::Checkbox(T("Plein écran"), &s.fullscreen)) {
        if (SDL_Window* window = GameWindow()) {
            SDL_SetWindowFullscreen(window, s.fullscreen);
        }
        sDirty = true;
    }
    int ss = s.supersampling;
    if (ImGui::SliderInt(T("Suréchantillonnage"), &ss, 1, 4, ss == 1 ? T("désactivé") : "%dx", ImGuiSliderFlags_AlwaysClamp)) {
        s.supersampling = ss;
        pc_settings_apply_supersampling();
        sDirty = true;
    }
    Hint(T("Rendu interne à N fois la taille de la fenêtre, puis réduit : image plus fine, demande plus de puissance."));
    if (ImGui::Checkbox(T("Pause quand la fenêtre est inactive"), &s.pauseUnfocused)) {
        aurora_set_pause_on_focus_lost(s.pauseUnfocused);
        sDirty = true;
    }
    if (ImGui::Checkbox(T("Synchronisation verticale"), &s.vsync)) {
        sDirty = true;
    }
    if (s.vsync != sAtLaunch.vsync) {
        ImGui::SameLine();
        ImGui::TextDisabled(T("(au prochain lancement)"));
    }
    static const int kMsaa[] = {1, 2, 4, 8};
    static const char* const kMsaaNames[] = {T("désactivé"), "x2", "x4", "x8"};
    int msaaIndex = s.msaa == 1 ? 0 : s.msaa == 2 ? 1 : s.msaa == 8 ? 3 : 2;
    if (ImGui::SliderInt(T("Anticrénelage (MSAA)"), &msaaIndex, 0, 3, kMsaaNames[msaaIndex], ImGuiSliderFlags_AlwaysClamp)) {
        s.msaa = kMsaa[msaaIndex];
        sDirty = true;
    }
    if (s.msaa != sAtLaunch.msaa) {
        ImGui::SameLine();
        ImGui::TextDisabled(T("(au prochain lancement)"));
    }

    ImGui::SeparatorText(T("Améliorations"));
    if (ImGui::Checkbox(T("Éclairage par pixel"), &s.pixelLighting)) {
        pc_settings_apply_lighting();
        sDirty = true;
    }
    Hint(T("Lumière calculée pour chaque pixel au lieu de chaque sommet : reliefs et reflets plus lisses."));
    if (SliderStep(T("Distance d'affichage"), &s.drawDistance, 1.0f, 4.0f, 0.5f, "x%.1f")) {
        sDirty = true;
    }
    Hint(T("Distance à laquelle les objets passent en version simplifiée ou disparaissent (x1 = Wii)."));
    if (SliderStep(T("Particules"), &s.particleDensity, 1.0f, 3.0f, 0.25f, "x%.2f")) {
        sDirty = true;
    }
    Hint(T("Densité des poussières, étincelles... (x1 = Wii). Effet complet au prochain niveau."));
    if (SliderStep(T("Rayons de soleil"), &s.godRays, 0.0f, 2.0f, 0.1f, s.godRays == 0.0f ? T("désactivés") : "%.1f")) {
        sDirty = true;
    }
    Hint(T("Rayons autour du soleil dans les galaxies où le jeu affiche un reflet du soleil."));
    if (SliderStep(T("Netteté"), &s.sharpen, 0.0f, 1.0f, 0.05f, s.sharpen == 0.0f ? T("désactivée") : "%.2f")) {
        pc_settings_apply_post_process();
        sDirty = true;
    }
    Hint(T("Accentue les détails de l'image finale sans créer de halos."));
    if (SliderStep(T("Couleurs vives"), &s.vibrance, -0.5f, 1.0f, 0.05f, s.vibrance == 0.0f ? T("comme sur Wii") : "%.2f")) {
        pc_settings_apply_post_process();
        sDirty = true;
    }
    Hint(T("Ravive les couleurs ternes sans saturer celles qui sont déjà vives."));
    if (ImGui::Checkbox(T("Anticrénelage FXAA"), &s.fxaa)) {
        pc_settings_apply_post_process();
        sDirty = true;
    }
    Hint(T("Lisse les bords de l'image finale. Utile surtout sans suréchantillonnage ni MSAA."));

    ImGui::SeparatorText(T("Jeu"));
    if (ImGui::Checkbox(T("Mode deux joueurs (co-star)"), &s.coStar)) {
        sDirty = true;
    }
    Hint(T("Une deuxième manette pilote le pointeur étoile du second joueur (il apparaît quand on l'utilise) : il ramasse et tire des fragments, immobilise les ennemis, fait sauter Mario avec A."));
    if (SliderStep(T("Vitesse du pointeur"), &s.pointerSpeed, 0.5f, 2.0f, 0.1f, "x%.1f")) {
        sDirty = true;
    }
    Hint(T("Vitesse du pointeur étoile déplacé au stick droit."));
    if (SliderStep(T("Volume général"), &s.masterVolume, 0.0f, 1.0f, 0.05f, s.masterVolume == 0.0f ? T("muet") : "%.2f")) {
        sDirty = true;
    }
    if (SliderStep(T("Sons de la Wiimote"), &s.speakerVolume, 0.0f, 1.0f, 0.05f,
                   s.speakerVolume == 0.0f ? T("remplacés par ceux du jeu") : "%.2f")) {
        sDirty = true;
    }
    Hint(T("Volume des sons que la Wiimote jouait sur son haut-parleur (fragments d'étoile, toupie, menus...)."));
    static const char* const kLanguages[] = {"fr", "en", "es"};
    static const char* const kLanguageNames[] = {T("Français"), T("English"), T("Español")};
    int lang = s.language == "en" ? 1 : s.language == "es" ? 2 : 0;
    if (ImGui::Combo(T("Langue"), &lang, kLanguageNames, 3)) {
        s.language = kLanguages[lang];
        sDirty = true;
    }
    if (s.language != sAtLaunch.language) {
        ImGui::SameLine();
        ImGui::TextDisabled(T("(au prochain lancement)"));
    }

    ImGui::PopItemWidth();

    // Gamepad layout (pc/src/input/wiimote.cpp).
    ImGui::SeparatorText(T("Commandes (manette Xbox)"));
    static const char* const kControls[][2] = {
        {T("Stick gauche"), T("Déplacer Mario")},
        {T("A"), T("Sauter")},
        {T("X ou B"), T("Toupie (secouer la Wiimote)")},
        {T("Stick droit"), T("Déplacer le pointeur étoile")},
        {T("Clic stick droit"), T("Recentrer le pointeur")},
        {T("RB ou RT"), T("Tirer des fragments d'étoile (bouton B)")},
        {T("LT"), T("S'accroupir, attaque au sol (bouton Z)")},
        {T("LB"), T("Caméra derrière Mario (bouton C)")},
        {T("Croix"), T("Caméra / vue subjective")},
        {T("Menu"), T("Pause (bouton +)")},
        {T("Affichage"), T("Bouton -")},
        {T("Y"), T("Bouton 1")},
        {T("Affichage + Menu"), T("Ce menu de réglages")},
    };
    if (ImGui::BeginTable("controls", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        for (const auto& row : kControls) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row[0]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row[1]);
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled(T("Souris : le pointeur suit la souris, clic gauche = A, clic droit = B."));

    ImGui::Spacing();
    if (ImGui::Button(T("Fermer"), ImVec2(200.0f * scale, 0.0f))) {
        open = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(T("Quitter le jeu"), ImVec2(240.0f * scale, 0.0f))) {
        ImGui::OpenPopup("quit");
    }
    ImGui::TextDisabled(T("Les réglages sont enregistrés dans display.ini."));
    bool popupOpen = false;
    if (ImGui::BeginPopupModal("quit", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        popupOpen = true;
        ImGui::TextUnformatted(T("Quitter Super Mario Galaxy ?"));
        ImGui::TextDisabled(T("La progression non sauvegardée sera perdue."));
        ImGui::Spacing();
        if (ImGui::Button(T("Non"), ImVec2(160.0f * scale, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button(T("Oui, quitter"), ImVec2(200.0f * scale, 0.0f))) {
            sQuit = true;
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    // Gamepad B / Escape close the menu (or only the confirmation above).
    if (!popupOpen && (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))) {
        open = false;
    }
    ImGui::End();
    if (!open) {
        Close();
    }
}
