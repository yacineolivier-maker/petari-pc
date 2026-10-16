#pragma once

// PC audio: high-level emulation of the JAudio2 DSP mixer, output through SDL3.
// See pc/src/audio/pc_audio.cpp (driver glue) and pc/src/audio/pc_dsp.cpp (the mixer).

namespace smgpc::audio {
    // Replaces JASAudioThread::create(): sets up JASDriver/JASDsp state and starts the SDL3 output
    // stream, whose callback then drives JASDriver::updateDSP() once per DSP subframe.
    void Initialize();
    void Shutdown();
}  // namespace smgpc::audio
