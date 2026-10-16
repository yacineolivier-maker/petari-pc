#pragma once

// HLE of the JAudio2 DSP ucode: mixes JASDsp::CH_BUF voices into a stereo float subframe.
// Adapted from Dusklight (Twilight Princess PC port, CC0): src/dusk/audio/DuskDsp.cpp, Adpcm.cpp.

#include <revolution/types.h>

namespace smgpc::audio {
    constexpr int kSampleRate = 48000;
    constexpr int kSubframeSize = 0x50;  // samples rendered by the DSP per subframe

    struct StereoSubframe {
        f32 l[kSubframeSize];
        f32 r[kSubframeSize];
    };

    void DspInit();

    // Renders one subframe of all active DSP channels (overwrites out). Caller holds the
    // JASCriticalSection.
    void DspRender(StereoSubframe& out);

    extern bool gEnableReverb;
    extern int gFxVoices;  // voices sending to the reverb in the last subframe (debug stats)
}  // namespace smgpc::audio
