// Wii Remote + Nunchuk emulation on top of a PC gamepad (Xbox layout), for the PC port.
//
// The game reads its controllers only through KPADRead (src/Game/System/WPadHolder.cpp), so this
// file implements the KPAD/WPAD API the game uses and fills KPADStatus from Aurora's PADRead
// (which reads the SDL gamepad). Default mapping:
//
//   Left stick ........ Nunchuk stick          A ........... A (jump)
//   Right stick ....... pointer (Star Pointer) B / X ....... shake (spin)
//   RB / RT ........... B (shoot Star Bits)    LT .......... Z (crouch)
//   LB ................ C                      Start ....... +   Back ....... -
//   D-pad ............. D-pad                  Y ........... 1   R3 ......... recentre pointer
//
// The spin is detected by the game from accelerometer changes (WPadHVSwing); pressing the
// shake button produces a short synthetic shake of the remote's accelerometer.

#include <revolution/kpad.h>
#include <revolution/wpad.h>

#include <dolphin/pad.h>

#include <cmath>
#include <cstring>

namespace {

constexpr int kNumChannels = 4;

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
u32 sLastPadReadFrame = 0xFFFFFFFF;
u32 sFrame = 0;

f32 StickAxis(s8 v) {
    // Aurora reports sticks in the GameCube range (about -100..100 after clamping).
    f32 f = v / 100.0f;
    if (f > 1.0f) f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    const f32 dead = 0.15f;
    if (std::fabs(f) < dead) return 0.0f;
    return (f - std::copysign(dead, f)) / (1.0f - dead);
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
    PADRead(sPads);
    PADClamp(sPads);
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
    // Left trigger: Nunchuk Z. LB: Nunchuk C.
    if (pad.triggerLeft > 0x60) b |= WPAD_BUTTON_Z;
    if (btn & PAD_TRIGGER_L && pad.triggerLeft <= 0x60) b |= WPAD_BUTTON_C;
    shakeButton = (btn & PAD_BUTTON_B) || (btn & PAD_BUTTON_X);
    return b;
}

}  // namespace

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
    const u32 hold = MapButtons(pad, shakeButton);
    s.hold = hold;
    s.trig = hold & ~ch.prevHold;
    s.release = ch.prevHold & ~hold;
    ch.prevHold = hold;

    // Pointer: the right stick moves it (velocity control); R3 recentres it.
    const f32 rx = StickAxis(pad.substickX);
    const f32 ry = StickAxis(pad.substickY);
    const f32 speed = 0.035f;
    ch.pointerX += rx * std::fabs(rx) * speed * 1.6f + rx * speed * 0.4f;
    ch.pointerY -= ry * std::fabs(ry) * speed * 1.6f + ry * speed * 0.4f;
    if (pad.extButton & PAD_BUTTON_RIGHT_STICK) {
        ch.pointerX = ch.pointerY = 0.0f;
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
        ch.shakeFrames = 8;
    }
    ch.prevShakeButton = shakeButton;
    s.acc.x = 0.0f;
    s.acc.y = -1.0f;
    s.acc.z = 0.0f;
    if (ch.shakeFrames > 0) {
        const f32 amp = 2.5f;
        s.acc.x = (ch.shakeFrames & 1) ? amp : -amp;
        s.acc.z = (ch.shakeFrames & 1) ? -amp * 0.5f : amp * 0.5f;
        ch.shakeFrames--;
    }
    s.acc_value = std::sqrt(s.acc.x * s.acc.x + s.acc.y * s.acc.y + s.acc.z * s.acc.z);
    s.acc_vertical.x = 0.0f;
    s.acc_vertical.y = -1.0f;

    s.dev_type = WPAD_DEV_FREESTYLE;
    s.wpad_err = WPAD_ERR_NONE;
    s.data_format = 0;
    s.ex_status.fs.stick.x = StickAxis(pad.stickX);
    s.ex_status.fs.stick.y = StickAxis(pad.stickY);
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

BOOL WPADIsSpeakerEnabled(s32) { return FALSE; }
s32 WPADControlSpeaker(s32 chan, u32, WPADCallback callback) {
    if (callback != nullptr) {
        callback(chan, WPAD_ERR_NONE);
    }
    return WPAD_ERR_NONE;
}
u8 WPADGetSpeakerVolume(void) { return 0; }
s32 WPADSendStreamData(s32, void*, u16) { return WPAD_ERR_NONE; }
BOOL WPADCanSendStreamData(s32) { return FALSE; }

void WPADControlMotor(s32 chan, u32 command) {
    if (chan >= 0 && chan < PAD_MAX_CONTROLLERS) {
        PADControlMotor(chan, command != 0 ? PAD_MOTOR_RUMBLE : PAD_MOTOR_STOP);
    }
}

void WPADSetAutoSleepTime(u8) {}
u8 WPADGetSensorBarPosition(void) { return 0; }

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
