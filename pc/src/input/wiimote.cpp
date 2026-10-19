// Wii Remote + Nunchuk emulation on top of a PC gamepad (Xbox layout), for the PC port.
//
// The game reads its controllers only through KPADRead (src/Game/System/WPadHolder.cpp), so this
// file implements the KPAD/WPAD API the game uses and fills KPADStatus from Aurora's PADRead
// (which reads the SDL gamepad). Default mapping:
//
//   Left stick ........ Nunchuk stick          A ........... A (jump)
//   Right stick ....... pointer (Star Pointer) B / X ....... shake (spin)
//   RB / RT ........... B (shoot Star Bits)    LT .......... Z (crouch)
//   LB ................ C (camera behind Mario) Start ...... +   Back ....... -
//   D-pad ............. D-pad                  Y ........... 1   R3 ......... recentre pointer
//
// Co-star (display.ini deux_joueurs = 1): the second Wii Remote only appears once a second gamepad
// has been used on its own (a connected but idle or turned-off pad, or a virtual copy of the first
// one, doesn't show a second Star Pointer). Until then every gamepad drives the first remote.
//
// Mouse (PC addition): moving it inside the window places the pointer there; left button = A,
// right button = B. SMGPC_NO_MOUSE=1 turns it off.
//
// The spin is detected by the game from accelerometer changes (WPadHVSwing); pressing the
// shake button produces a short synthetic shake of the remote's accelerometer.

#include <revolution/kpad.h>
#include <revolution/wpad.h>

#include <dolphin/pad.h>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

bool pc_settings_co_star();
float pc_settings_pointer_speed();
float pc_settings_speaker_volume();
float pc_settings_rumble();
bool pc_settings_menu_open();
void pc_settings_menu_toggle();

namespace {

constexpr int kNumChannels = 4;
constexpr int kShakeFrames = 80;  // length of a synthetic shake (see the accelerometer below)

// Mouse state for the first remote: pointer position (-1..1) when the mouse moved over the
// window this frame, and the A/B buttons from its left/right buttons.
struct MouseInput {
    bool moved = false;
    f32 x = 0.0f, y = 0.0f;
    u32 buttons = 0;
};

MouseInput ReadMouse() {
    static const bool sDisabled = std::getenv("SMGPC_NO_MOUSE") != nullptr;
    static f32 sLastX = -1.0f, sLastY = -1.0f;
    MouseInput m;
    if (sDisabled || pc_settings_menu_open()) {
        return m;
    }
    SDL_Window* window = SDL_GetMouseFocus();
    if (window == nullptr) {
        return m;
    }
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0) {
        return m;
    }
    float mx = 0.0f, my = 0.0f;
    const SDL_MouseButtonFlags b = SDL_GetMouseState(&mx, &my);
    if (mx != sLastX || my != sLastY) {
        if (sLastX >= 0.0f) {
            m.moved = true;
        }
        sLastX = mx;
        sLastY = my;
    }
    m.x = mx / w * 2.0f - 1.0f;
    m.y = my / h * 2.0f - 1.0f;
    if (b & SDL_BUTTON_LMASK) m.buttons |= WPAD_BUTTON_A;
    if (b & SDL_BUTTON_RMASK) m.buttons |= WPAD_BUTTON_B;
    return m;
}

struct ChannelState {
    bool connected = false;
    bool announced = false;
    u32 prevHold = 0;
    f32 pointerX = 0.0f;  // -1 (left) .. 1 (right)
    f32 pointerY = 0.0f;  // -1 (top) .. 1 (bottom)
    int shakeFrames = 0;
    bool prevShakeButton = false;
    WPADConnectCallback connectCallback = nullptr;
    WPADExtensionCallback extensionCallback = nullptr;
};

ChannelState sChannels[kNumChannels];
bool sPadInitialized = false;
PADStatus sPads[PAD_MAX_CONTROLLERS];
// Co-star mode: gamepad port of each remote (-1: every gamepad drives the first remote).
int sPadPort[PAD_MAX_CONTROLLERS] = {-1, -1, -1, -1};
u32 sLastPadReadFrame = 0xFFFFFFFF;
u32 sFrame = 0;

// Co-star: gamepad port of the first and second player (-1: not chosen yet), and for each port the
// number of consecutive frames it was used differently from the first player's gamepad.
int sCoStarPort[2] = {-1, -1};
int sCoStarDiffFrames[PAD_MAX_CONTROLLERS] = {};
constexpr int kCoStarDiffFrames = 10;

bool sRumbleOn[PAD_MAX_CONTROLLERS] = {};
constexpr u32 kRumbleMs = 150;
void Rumble(int port, bool on);

// Sticks: Aurora's GameCube clamp (PADClamp) would limit them to an octagon of radius 72 (and 57
// on the diagonals) and its dead zone is per axis, which snaps the stick to the axes. A Nunchuk
// stick reads about -1..1 in a circle: use the full range with a radial dead zone instead.
struct StickVec {
    f32 x = 0.0f, y = 0.0f;
};

StickVec StickVector(s8 rawX, s8 rawY) {
    const f32 x = rawX / 127.0f;
    const f32 y = rawY / 127.0f;
    const f32 m = std::sqrt(x * x + y * y);
    const f32 dead = 0.18f, full = 0.95f;
    if (m <= dead) {
        return {};
    }
    f32 scaled = (m - dead) / (full - dead);
    if (scaled > 1.0f) scaled = 1.0f;
    return {x / m * scaled, y / m * scaled};
}

bool PadUsed(const PADStatus& p) {
    return p.button != 0 || p.extButton != 0 || std::abs((int)p.stickX) > 50 || std::abs((int)p.stickY) > 50 ||
           std::abs((int)p.substickX) > 50 || std::abs((int)p.substickY) > 50 || p.triggerLeft > 0x60 ||
           p.triggerRight > 0x60;
}

bool PadsAlike(const PADStatus& a, const PADStatus& b) {
    auto within = [](int u, int v, int tol) { return std::abs(u - v) <= tol; };
    return a.button == b.button && a.extButton == b.extButton && within(a.stickX, b.stickX, 16) &&
           within(a.stickY, b.stickY, 16) && within(a.substickX, b.substickX, 16) && within(a.substickY, b.substickY, 16) &&
           within(a.triggerLeft, b.triggerLeft, 48) && within(a.triggerRight, b.triggerRight, 48);
}

void ReadPads() {
    if (!sPadInitialized) {
        PADInit();
        sPadInitialized = true;
    }
    // KPADRead is called once per channel each frame; read the hardware once per frame.
    if (sLastPadReadFrame == sFrame) {
        return;
    }
    sLastPadReadFrame = sFrame;
    for (u32 port = 0; port < PAD_MAX_CONTROLLERS; port++) {
        if (PADDeadZones* dz = PADGetDeadZones(port)) {
            dz->useDeadzones = false;     // radial dead zone in StickVector instead
            dz->emulateTriggers = false;  // PAD_TRIGGER_L is LB below, the analog values stay
        }
    }
    PADStatus raw[PAD_MAX_CONTROLLERS];
    PADRead(raw);
    for (int port = 0; port < PAD_MAX_CONTROLLERS; port++) {
        if (sRumbleOn[port]) {
            Rumble(port, true);
        }
    }
    // Aurora's Xbox layout leaves LB unmapped: read it from SDL (the port is the player index).
    for (int port = 0; port < PAD_MAX_CONTROLLERS; port++) {
        if (raw[port].err != PAD_ERR_NONE) {
            continue;
        }
        if (SDL_Gamepad* g = SDL_GetGamepadFromPlayerIndex(port)) {
            if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
                raw[port].button |= PAD_TRIGGER_L;
            }
        }
    }
    // View + Menu on any gamepad opens/closes the settings menu; the game gets no input while
    // it is open (nor on the frame the buttons are released).
    static bool sComboHeld = false;
    bool combo = false;
    for (const PADStatus& p : raw) {
        if (p.err == PAD_ERR_NONE && (p.button & PAD_BUTTON_START) && (p.extButton & PAD_BUTTON_BACK)) {
            combo = true;
        }
    }
    if (combo && !sComboHeld) {
        pc_settings_menu_toggle();
    }
    const bool suppress = combo || sComboHeld || pc_settings_menu_open();
    sComboHeld = combo;
    if (suppress) {
        for (PADStatus& p : raw) {
            const s8 err = p.err;
            p = {};
            p.err = err;
        }
    }
    // Co-star (display.ini "deux_joueurs"): the first gamepad used is the first player; another one
    // becomes the second Wii Remote (the co-star's Star Pointer) once it has been used on its own for
    // a few frames. Gamepads that only mirror the first one never do.
    for (int& port : sCoStarPort) {
        if (port >= 0 && raw[port].err != PAD_ERR_NONE) {
            port = -1;
        }
    }
    if (sCoStarPort[0] < 0) {
        sCoStarPort[1] = -1;
    }
    if (!pc_settings_co_star()) {
        sCoStarPort[1] = -1;
    } else {
        if (sCoStarPort[0] < 0 && !suppress) {
            for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
                if (raw[i].err == PAD_ERR_NONE && PadUsed(raw[i])) {
                    sCoStarPort[0] = i;
                    break;
                }
            }
        }
        for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
            const bool candidate = sCoStarPort[0] >= 0 && sCoStarPort[1] < 0 && i != sCoStarPort[0] &&
                                   raw[i].err == PAD_ERR_NONE && !suppress && PadUsed(raw[i]) &&
                                   !PadsAlike(raw[i], raw[sCoStarPort[0]]);
            sCoStarDiffFrames[i] = candidate ? sCoStarDiffFrames[i] + 1 : 0;
            if (sCoStarDiffFrames[i] >= kCoStarDiffFrames) {
                sCoStarPort[1] = i;
                std::fprintf(stderr, "[pc] co-star: gamepad %d is the second player\n", i);
            }
        }
        if (sCoStarPort[1] >= 0) {
            for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
                sPadPort[i] = i < 2 ? sCoStarPort[i] : -1;
                if (i < 2) {
                    sPads[i] = raw[sCoStarPort[i]];
                } else {
                    sPads[i] = {};
                    sPads[i].err = PAD_ERR_NO_CONTROLLER;
                }
            }
            return;
        }
    }
    for (int& port : sPadPort) {
        port = -1;
    }
    // Single player: every connected gamepad (and the keyboard) drives the first Wii Remote, so it
    // doesn't matter which port SDL gives the Xbox controller. The other remotes stay disconnected
    // (otherwise the game shows a second player's "co-star" cursor).
    auto dominant = [](s8 a, s8 b) { return std::abs((int)b) > std::abs((int)a) ? b : a; };
    PADStatus merged = {};
    for (const PADStatus& p : raw) {
        if (p.err != PAD_ERR_NONE) {
            continue;
        }
        merged.button |= p.button;
        merged.extButton |= p.extButton;
        merged.stickX = dominant(merged.stickX, p.stickX);
        merged.stickY = dominant(merged.stickY, p.stickY);
        merged.substickX = dominant(merged.substickX, p.substickX);
        merged.substickY = dominant(merged.substickY, p.substickY);
        merged.triggerLeft = p.triggerLeft > merged.triggerLeft ? p.triggerLeft : merged.triggerLeft;
        merged.triggerRight = p.triggerRight > merged.triggerRight ? p.triggerRight : merged.triggerRight;
    }
    merged.err = PAD_ERR_NONE;
    sPads[0] = merged;
    for (int i = 1; i < PAD_MAX_CONTROLLERS; i++) {
        sPads[i] = {};
        sPads[i].err = PAD_ERR_NO_CONTROLLER;
    }
}

u32 MapButtons(const PADStatus& pad, bool& shakeButton) {
    u32 b = 0;
    const u32 btn = pad.button;
    const u32 ext = pad.extButton;
    if (btn & PAD_BUTTON_A) b |= WPAD_BUTTON_A;
    if (btn & PAD_BUTTON_Y) b |= WPAD_BUTTON_1;
    if (btn & PAD_BUTTON_START) b |= WPAD_BUTTON_PLUS;
    if (ext & PAD_BUTTON_BACK) b |= WPAD_BUTTON_MINUS;
    if (btn & PAD_BUTTON_UP) b |= WPAD_BUTTON_UP;
    if (btn & PAD_BUTTON_DOWN) b |= WPAD_BUTTON_DOWN;
    if (btn & PAD_BUTTON_LEFT) b |= WPAD_BUTTON_LEFT;
    if (btn & PAD_BUTTON_RIGHT) b |= WPAD_BUTTON_RIGHT;
    // RB (Aurora: Z) or the right trigger: B (Star Bits).
    if ((btn & PAD_TRIGGER_Z) || pad.triggerRight > 0x60) b |= WPAD_BUTTON_B;
    // Left trigger: Nunchuk Z. LB (PAD_TRIGGER_L, see ReadPads): Nunchuk C.
    if (pad.triggerLeft > 0x60) b |= WPAD_BUTTON_Z;
    if (btn & PAD_TRIGGER_L) b |= WPAD_BUTTON_C;
    shakeButton = (btn & PAD_BUTTON_B) || (btn & PAD_BUTTON_X);
    return b;
}

// SMGPC_INPUT: scripted input for automated tests, e.g. "A@300,A@420+5,SHAKE@900,STICKUP@1000+60".
// Each entry presses a button at a frame (KPADRead frames of channel 0) for a number of frames
// (default 3). Buttons: A B PLUS MINUS HOME 1 2 UP DOWN LEFT RIGHT Z C SHAKE, and STICKUP/DOWN/
// LEFT/RIGHT (Nunchuk stick), and PTR<x>:<y> to put the pointer at x, y (-1..1, e.g. PTR-0.5:0.3@900+30).
struct ScriptEntry {
    u32 buttons;
    bool shake;
    f32 stickX, stickY;
    u32 start, end;
    bool hasPointer;
    f32 pointerX, pointerY;
};
std::vector<ScriptEntry> sScript;
bool sScriptLoaded = false;

void LoadScript() {
    sScriptLoaded = true;
    const char* env = std::getenv("SMGPC_INPUT");
    if (env == nullptr) {
        return;
    }
    std::string text = env;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t comma = text.find(',', pos);
        std::string item = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        pos = comma == std::string::npos ? text.size() : comma + 1;
        size_t at = item.find('@');
        if (at == std::string::npos) {
            continue;
        }
        std::string name = item.substr(0, at);
        u32 start = (u32)std::strtoul(item.c_str() + at + 1, nullptr, 10);
        u32 dur = 3;
        size_t plus = item.find('+', at);
        if (plus != std::string::npos) {
            dur = (u32)std::strtoul(item.c_str() + plus + 1, nullptr, 10);
        }
        ScriptEntry e{0, false, 0.0f, 0.0f, start, start + dur, false, 0.0f, 0.0f};
        static const struct { const char* name; u32 bit; } kButtons[] = {
            {"A", WPAD_BUTTON_A}, {"B", WPAD_BUTTON_B}, {"PLUS", WPAD_BUTTON_PLUS}, {"MINUS", WPAD_BUTTON_MINUS},
            {"HOME", WPAD_BUTTON_HOME}, {"1", WPAD_BUTTON_1}, {"2", WPAD_BUTTON_2}, {"UP", WPAD_BUTTON_UP},
            {"DOWN", WPAD_BUTTON_DOWN}, {"LEFT", WPAD_BUTTON_LEFT}, {"RIGHT", WPAD_BUTTON_RIGHT},
            {"Z", WPAD_BUTTON_Z}, {"C", WPAD_BUTTON_C},
        };
        for (const auto& b : kButtons) {
            if (name == b.name) {
                e.buttons = b.bit;
            }
        }
        if (name == "SHAKE") e.shake = true;
        if (name == "STICKUP") e.stickY = 1.0f;
        if (name == "STICKDOWN") e.stickY = -1.0f;
        if (name == "STICKLEFT") e.stickX = -1.0f;
        if (name == "STICKRIGHT") e.stickX = 1.0f;
        if (name.rfind("PTR", 0) == 0) {
            char* end = nullptr;
            e.hasPointer = true;
            e.pointerX = std::strtof(name.c_str() + 3, &end);
            e.pointerY = (end != nullptr && *end == ':') ? std::strtof(end + 1, nullptr) : 0.0f;
        }
        sScript.push_back(e);
    }
    std::fprintf(stderr, "[pc] scripted input: %zu entries\n", sScript.size());
}

// Applies the script to the frame's state (on top of the real gamepad).
void ApplyScript(u32 frame, u32& hold, bool& shake, f32& stickX, f32& stickY, f32& pointerX, f32& pointerY) {
    if (!sScriptLoaded) {
        LoadScript();
    }
    for (const ScriptEntry& e : sScript) {
        if (frame >= e.start && frame < e.end) {
            hold |= e.buttons;
            shake = shake || e.shake;
            if (e.stickX != 0.0f) stickX = e.stickX;
            if (e.stickY != 0.0f) stickY = e.stickY;
            if (e.hasPointer) {
                pointerX = e.pointerX;
                pointerY = e.pointerY;
            }
        }
    }
}

}  // namespace

extern "C" u32 pc_input_frame(void) { return sFrame; }

extern "C" {

void KPADInit() {
    for (ChannelState& ch : sChannels) {
        ch = ChannelState{};
    }
}

void KPADReset(void) {}
void KPADSetBtnRepeat(s32, f32, f32) {}
void KPADSetSensorHeight(s32, f32) {}
void KPADSetAccParam(s32, f32, f32) {}
void KPADSetPosParam(s32, f32, f32) {}
void KPADSetHoriParam(s32, f32, f32) {}
void KPADSetDistParam(s32, f32, f32) {}

s32 KPADRead(s32 chan, KPADStatus status[], u32 count) {
    if (chan < 0 || chan >= kNumChannels || count == 0) {
        return 0;
    }
    if (chan == 0) {
        sFrame++;
    }
    ReadPads();
    ChannelState& ch = sChannels[chan];
    const PADStatus& pad = sPads[chan];
    const bool connected = pad.err == PAD_ERR_NONE;
    if (connected != ch.connected) {
        ch.connected = connected;
        ch.announced = false;
    }
    if (!ch.announced) {
        ch.announced = true;
        if (ch.connectCallback != nullptr) {
            ch.connectCallback(chan, connected ? WPAD_ERR_NONE : WPAD_ERR_NO_CONTROLLER);
        }
        if (connected && ch.extensionCallback != nullptr) {
            ch.extensionCallback(chan, WPAD_DEV_FREESTYLE);
        }
    }

    KPADStatus& s = status[0];
    std::memset(&s, 0, sizeof(s));
    if (!connected) {
        s.wpad_err = WPAD_ERR_NO_CONTROLLER;
        s.dev_type = WPAD_DEV_NOT_FOUND;
        return 0;
    }

    bool shakeButton = false;
    u32 hold = MapButtons(pad, shakeButton);
    const StickVec left = StickVector(pad.stickX, pad.stickY);
    f32 stickX = left.x;
    f32 stickY = left.y;
    MouseInput mouse;
    if (chan == 0) {
        ApplyScript(sFrame, hold, shakeButton, stickX, stickY, ch.pointerX, ch.pointerY);
        mouse = ReadMouse();
        hold |= mouse.buttons;
    }
    s.hold = hold;
    s.trig = hold & ~ch.prevHold;
    s.release = ch.prevHold & ~hold;
    ch.prevHold = hold;

    // Pointer: the right stick moves it (velocity control); R3 recentres it.
    const StickVec right = StickVector(pad.substickX, pad.substickY);
    f32 rx = right.x;
    f32 ry = right.y;
    if (chan != 0) {
        // The co-star has no Nunchuk: either stick moves the pointer.
        if (std::fabs(stickX) > std::fabs(rx)) rx = stickX;
        if (std::fabs(stickY) > std::fabs(ry)) ry = stickY;
        stickX = stickY = 0.0f;
    }
    const f32 speed = 0.035f * pc_settings_pointer_speed();
    ch.pointerX += rx * std::fabs(rx) * speed * 0.5f + rx * speed * 0.14f;
    ch.pointerY -= ry * std::fabs(ry) * speed * 0.5f + ry * speed * 0.14f;
    if (pad.extButton & PAD_BUTTON_RIGHT_STICK) {
        ch.pointerX = ch.pointerY = 0.0f;
    }
    if (mouse.moved) {
        ch.pointerX = mouse.x;
        ch.pointerY = mouse.y;
    }
    if (ch.pointerX > 1.0f) ch.pointerX = 1.0f;
    if (ch.pointerX < -1.0f) ch.pointerX = -1.0f;
    if (ch.pointerY > 1.0f) ch.pointerY = 1.0f;
    if (ch.pointerY < -1.0f) ch.pointerY = -1.0f;
    s.pos.x = ch.pointerX;
    s.pos.y = ch.pointerY;
    s.dpd_valid_fg = 2;
    s.horizon.x = 1.0f;
    s.horizon.y = 0.0f;
    s.dist = 2.0f;

    // Accelerometer: remote held still, pointing at the screen; a shake on the shake button.
    if (shakeButton && !ch.prevShakeButton) {
        ch.shakeFrames = kShakeFrames;
    }
    ch.prevShakeButton = shakeButton;
    s.acc.x = 0.0f;
    s.acc.y = -1.0f;
    s.acc.z = 0.0f;
    if (ch.shakeFrames > 0) {
        // One swing. WPadHVSwing compares the acceleration with the one 20 frames earlier
        // (threshold 1.0): a sharp jump followed by a slow return to rest reads as exactly one
        // swing (a short pulse would echo 20 frames later, an oscillation reads as several). The
        // vertical part feeds the underwater spin (WPadHVSwing::updateCentrifugal).
        const f32 e = f32(ch.shakeFrames) / kShakeFrames;
        s.acc.x = 2.5f * e;
        s.acc.y = -1.0f + 1.5f * e;
        s.acc.z = -1.25f * e;
        ch.shakeFrames--;
    }
    s.acc_value = std::sqrt(s.acc.x * s.acc.x + s.acc.y * s.acc.y + s.acc.z * s.acc.z);
    s.acc_vertical.x = 0.0f;
    s.acc_vertical.y = -1.0f;

    s.dev_type = WPAD_DEV_FREESTYLE;
    s.wpad_err = WPAD_ERR_NONE;
    s.data_format = 0;
    s.ex_status.fs.stick.x = stickX;
    s.ex_status.fs.stick.y = stickY;
    s.ex_status.fs.acc.x = 0.0f;
    s.ex_status.fs.acc.y = -1.0f;
    s.ex_status.fs.acc.z = 0.0f;
    s.ex_status.fs.acc_value = 1.0f;
    return 1;
}

// WPAD: connection, extension and info queries are answered from the gamepad state; rumble goes
// to the gamepad; the remote speaker is not emulated (its sounds play through the main mix later).
void WPADRegisterAllocator(WPADAlloc, WPADFree) {}
u32 WPADGetWorkMemorySize(void) { return 0; }
void WPADDisconnect(s32) {}

s32 WPADGetInfoAsync(s32 chan, WPADInfo* info, WPADCallback callback) {
    if (info != nullptr) {
        std::memset(info, 0, sizeof(*info));
        info->dpd = TRUE;
        info->attach = TRUE;
        info->battery = 4;
    }
    if (callback != nullptr) {
        callback(chan, WPAD_ERR_NONE);
    }
    return WPAD_ERR_NONE;
}

// The first remote's speaker is played on the PC's audio output (SpkSpeakerCtrl::renderForPc);
// with display.ini sons_wiimote = 0 it is off and the game plays its TV fallbacks instead.
BOOL WPADIsSpeakerEnabled(s32 chan) { return chan == 0 && pc_settings_speaker_volume() > 0.0f; }
s32 WPADControlSpeaker(s32 chan, u32, WPADCallback callback) {
    if (callback != nullptr) {
        callback(chan, WPAD_ERR_NONE);
    }
    return WPAD_ERR_NONE;
}
u8 WPADGetSpeakerVolume(void) { return pc_settings_speaker_volume() > 0.0f ? 127 : 0; }
s32 WPADSendStreamData(s32, void*, u16) { return WPAD_ERR_NONE; }
BOOL WPADCanSendStreamData(s32) { return FALSE; }  // the speaker stream doesn't go through WPAD on PC

void WPADControlMotor(s32 chan, u32 command) {
    if (chan >= 0 && chan < PAD_MAX_CONTROLLERS && sPadPort[chan] >= 0) {  // co-star mode
        Rumble(sPadPort[chan], command != 0);
        return;
    }
    // The first remote is all the gamepads (see ReadPads): rumble every port.
    if (chan == 0) {
        for (int port = 0; port < PAD_MAX_CONTROLLERS; port++) {
            Rumble(port, command != 0);
        }
    }
}

void WPADSetAutoSleepTime(u8) {}
u8 WPADGetSensorBarPosition(void) { return 0; }

}  // extern "C"

// Rumble goes straight to SDL (the port is the player index) with a short duration, renewed every
// frame by ReadPads while it is on: if the game stops running (paused in the background, menu,
// crash) the gamepad stops vibrating by itself.
namespace {
void Rumble(int port, bool on) {
    sRumbleOn[port] = on;
    if (SDL_Gamepad* g = SDL_GetGamepadFromPlayerIndex(port)) {
        // Strength from display.ini "vibrations" (0.5 = Aurora's default intensity).
        const u16 v = on ? u16(pc_settings_rumble() * 65535.0f) : 0;
        SDL_RumbleGamepad(g, v, v, on ? kRumbleMs : 0);
    }
}
}  // namespace

// Whether the game was already told this channel is connected (for SpkSpeakerCtrl::setup).
bool pc_wpad_announced_connected(s32 chan) {
    return chan >= 0 && chan < kNumChannels && sChannels[chan].announced && sChannels[chan].connected;
}

extern "C" {

s32 WPADProbe(s32 chan, u32* type) {
    ReadPads();
    const bool connected = chan >= 0 && chan < PAD_MAX_CONTROLLERS && sPads[chan].err == PAD_ERR_NONE;
    if (type != nullptr) {
        *type = connected ? WPAD_DEV_FREESTYLE : WPAD_DEV_NOT_FOUND;
    }
    return connected ? WPAD_ERR_NONE : WPAD_ERR_NO_CONTROLLER;
}

WPADExtensionCallback WPADSetExtensionCallback(s32 chan, WPADExtensionCallback callback) {
    if (chan < 0 || chan >= kNumChannels) {
        return nullptr;
    }
    WPADExtensionCallback old = sChannels[chan].extensionCallback;
    sChannels[chan].extensionCallback = callback;
    if (callback != nullptr && sChannels[chan].connected) {
        callback(chan, WPAD_DEV_FREESTYLE);
    }
    return old;
}

WPADConnectCallback WPADSetConnectCallback(s32 chan, WPADConnectCallback callback) {
    if (chan < 0 || chan >= kNumChannels) {
        return nullptr;
    }
    WPADConnectCallback old = sChannels[chan].connectCallback;
    sChannels[chan].connectCallback = callback;
    sChannels[chan].announced = false;
    return old;
}

}  // extern "C"
