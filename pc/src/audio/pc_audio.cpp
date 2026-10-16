// PC audio output: replaces the Wii audio thread + DSP + AI DMA with an SDL3 audio stream whose
// callback drives JAudio2 directly. For each DSP subframe (80 samples at 48 kHz) it runs
// JASDriver::updateDSP() (sequencer, channel updates, streams) and then the HLE DSP mixer
// (pc_dsp.cpp), exactly where the real DSP would have rendered that subframe.
//
// Structure adapted from Dusklight, the Twilight Princess PC port (CC0 1.0):
//   src/dusk/audio/DuskAudioSystem.cpp and JASCriticalSection.cpp.
//
// Environment:
//   SMGPC_AUDIO_DUMP=out.wav  also write the mixed output to a 48 kHz stereo 16-bit WAV file.
//   SMGPC_NO_AUDIO=1          don't open an audio device (the mixer still runs if dumping).
//   SMGPC_NO_REVERB=1         disable the FX-bus reverb.

#include "pc/audio.h"

#include "pc_dsp.h"

#include "JSystem/JAudio2/JASAiCtrl.hpp"
#include "JSystem/JAudio2/JASAudioThread.hpp"
#include "JSystem/JAudio2/JASChannel.hpp"
#include "JSystem/JAudio2/JASCriticalSection.hpp"
#include "JSystem/JAudio2/JASDSPChannel.hpp"
#include "JSystem/JAudio2/JASDSPInterface.hpp"
#include "JSystem/JAudio2/JASDriverIF.hpp"
#include "JSystem/JAudio2/JASHeapCtrl.hpp"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

// --- JASCriticalSection ---------------------------------------------------------------------
// On the Wii it disables interrupts, which (single core, audio thread at top priority) keeps the
// audio thread/DSP callbacks out. Here the audio "thread" is the SDL callback, so it's one
// recursive mutex, held by the callback for each rendered frame.
// Never destroyed: the SDL audio thread may still run while the process exits.
static std::recursive_mutex& AudioMutex() {
    static std::recursive_mutex* sMutex = new std::recursive_mutex;
    return *sMutex;
}

JASCriticalSection::JASCriticalSection() {
    AudioMutex().lock();
}

JASCriticalSection::~JASCriticalSection() {
    AudioMutex().unlock();
}

namespace smgpc::audio {
namespace {

constexpr int kChannels = 2;

SDL_AudioStream* sStream = nullptr;
bool sInitialized = false;
std::atomic<bool> sExiting{false};  // set (under the lock) by the atexit handler

// --- WAV dump -------------------------------------------------------------------------------
FILE* sDumpFile = nullptr;
u32 sDumpBytes = 0;
u32 sDumpLastHeaderUpdate = 0;

void WriteWavHeader(FILE* f, u32 dataBytes) {
    auto put32 = [&](u32 v) { fwrite(&v, 4, 1, f); };
    auto put16 = [&](u16 v) { fwrite(&v, 2, 1, f); };
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f);
    put32(36 + dataBytes);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(16);
    put16(1);  // PCM
    put16(kChannels);
    put32(kSampleRate);
    put32(kSampleRate * kChannels * 2);
    put16(kChannels * 2);
    put16(16);
    fwrite("data", 1, 4, f);
    put32(dataBytes);
    fseek(f, 0, SEEK_END);
}

void DumpSamples(const f32* interleaved, int frames) {
    if (sDumpFile == nullptr) {
        return;
    }
    s16 pcm[kSubframeSize * kChannels];
    for (int i = 0; i < frames * kChannels; i++) {
        pcm[i] = (s16)std::clamp((int)lrintf(interleaved[i] * 32767.0f), -32768, 32767);
    }
    fwrite(pcm, sizeof(s16), frames * kChannels, sDumpFile);
    sDumpBytes += frames * kChannels * sizeof(s16);
    // Keep the header valid so the file is usable even if the game is killed.
    if (sDumpBytes - sDumpLastHeaderUpdate >= kSampleRate * kChannels * 2 / 2) {
        sDumpLastHeaderUpdate = sDumpBytes;
        WriteWavHeader(sDumpFile, sDumpBytes);
        fflush(sDumpFile);
    }
}

// --- External mix (THP movie audio, MIX_MODE_INTERLEAVE) -------------------------------------
// The callback produces 32 kHz stereo; resample 2 subframes of input to 3 at 48 kHz.
constexpr int kExtInFrames = kSubframeSize * 2;
constexpr int kExtOutFrames = kSubframeSize * 3;

struct LinearResampler32to48 {
    f32 prev = 0.0f;
    int phase = 0;

    void resample(const s16* in, f32* out, int ch) {
        int idx = -1;
        int phi = phase;
        for (int n = 0; n < kExtOutFrames; n++) {
            const f32 s0 = idx < 0 ? prev : in[idx * kChannels + ch] / 32768.0f;
            const f32 s1 = in[(idx + 1) * kChannels + ch] / 32768.0f;
            out[n * kChannels + ch] = s0 + (phi * (1.0f / 3.0f)) * (s1 - s0);
            phi += 2;
            if (phi >= 3) {
                phi -= 3;
                idx++;
            }
        }
        prev = in[(kExtInFrames - 1) * kChannels + ch] / 32768.0f;
        phase = phi;
    }
};

struct {
    f32 buf[kExtOutFrames * kChannels];
    int available = 0;  // frames left in buf
    LinearResampler32to48 left, right;
} sExt;

void MixExternal(f32* interleaved) {
    if (JASDriver::extMixCallback == nullptr || JASDriver::sMixMode != MIX_MODE_INTERLEAVE) {
        sExt.available = 0;
        return;
    }
    if (sExt.available <= 0) {
        const s16* data = JASDriver::extMixCallback(kExtInFrames);
        if (data == nullptr) {
            return;
        }
        sExt.left.resample(data, sExt.buf, 0);
        sExt.right.resample(data, sExt.buf, 1);
        sExt.available = kExtOutFrames;
    }
    const f32* src = sExt.buf + (kExtOutFrames - sExt.available) * kChannels;
    for (int i = 0; i < kSubframeSize * kChannels; i++) {
        interleaved[i] += src[i];
    }
    sExt.available -= kSubframeSize;
}

// --- Rendering ------------------------------------------------------------------------------
StereoSubframe sSubframe;

// Renders one DSP subframe into interleaved stereo floats (kSubframeSize frames).
void RenderSubframe(f32* interleaved) {
    JASDriver::updateDSP();
    DspRender(sSubframe);

    // JASDsp::setDSPMixerLevel: AudParams::outputGain (0.9), faded out by JASAudioReseter.
    const f32 master = JASDsp::getDSPMixerLevel();
    for (int i = 0; i < kSubframeSize; i++) {
        interleaved[i * 2 + 0] = sSubframe.l[i] * master;
        interleaved[i * 2 + 1] = sSubframe.r[i] * master;
    }
    MixExternal(interleaved);
}

// Renders a whole audio frame (getSubFrames() subframes, like one DSP sync). Returns bytes written.
int RenderFrame() {
    static f32 sFrameBuf[16 * kSubframeSize * kChannels];
    static const bool sDebug = std::getenv("SMGPC_DEBUG_AUDIO") != nullptr;
    static f64 sTotalUs = 0.0, sMaxUs = 0.0;
    static u32 sFrames = 0;
    const auto start = std::chrono::steady_clock::now();
    JASCriticalSection section;
    const u32 subframes = std::min<u32>(JASDriver::getSubFrames(), 16);
    const int bytes = (int)(subframes * kSubframeSize * kChannels * sizeof(f32));
    if (sExiting) {
        // The game's statics are being torn down: output silence, don't touch JAudio.
        memset(sFrameBuf, 0, bytes);
        if (sStream != nullptr) {
            SDL_PutAudioStreamData(sStream, sFrameBuf, bytes);
        }
        return bytes;
    }

    JASAudioThread::setDSPSyncCount(subframes);
    for (u32 i = 0; i < subframes; i++) {
        f32* dst = sFrameBuf + i * kSubframeSize * kChannels;
        RenderSubframe(dst);
        DumpSamples(dst, kSubframeSize);
        JASAudioThread::snIntCount -= 1;
    }

    if (sDebug) {
        const f64 us = std::chrono::duration<f64, std::micro>(std::chrono::steady_clock::now() - start).count();
        sTotalUs += us;
        sMaxUs = std::max(sMaxUs, us);
        static int sMaxActive = 0, sMaxFx = 0;
        int nowActive = 0;
        for (int i = 0; i < 64; i++) {
            nowActive += JASDsp::CH_BUF[i].mIsActive != 0;
        }
        sMaxActive = std::max(sMaxActive, nowActive);
        sMaxFx = std::max(sMaxFx, gFxVoices);
        if (++sFrames % 300 == 0) {
            const int active = sMaxActive, fxVoices = sMaxFx;
            sMaxActive = sMaxFx = 0;
            fprintf(stderr, "[pc audio] %u frames, render avg %.0f us max %.0f us, %d active voices max (%d with reverb send)\n", sFrames,
                    sTotalUs / 300, sMaxUs, active, fxVoices);
            sTotalUs = sMaxUs = 0.0;
        }
    }

    if (sStream != nullptr) {
        SDL_PutAudioStreamData(sStream, sFrameBuf, bytes);
    }
    return bytes;
}

void SDLCALL GetNewAudio(void*, SDL_AudioStream*, int additionalAmount, int) {
    while (additionalAmount > 0) {
        additionalAmount -= RenderFrame();
    }
}

// Without an audio device (or with SMGPC_NO_AUDIO) but dumping: render in real time on a thread.

void OnExit() {
    JASCriticalSection section;
    sExiting = true;
    if (sDumpFile != nullptr) {
        WriteWavHeader(sDumpFile, sDumpBytes);
        fclose(sDumpFile);
        sDumpFile = nullptr;
    }
}

void TimerThreadMain() {
    using clock = std::chrono::steady_clock;
    const auto frameDuration = std::chrono::microseconds(1000000LL * 10 * kSubframeSize / kSampleRate);
    auto next = clock::now();
    while (!sExiting) {
        RenderFrame();
        next += frameDuration * JASDriver::getSubFrames() / 10;
        std::this_thread::sleep_until(next);
    }
}

}  // namespace

void Initialize() {
    if (sInitialized) {
        return;
    }
    sInitialized = true;

    // 48 kHz mode: 10 subframes per frame; voice pitch and track tempo scale accordingly.
    JASDriver::setOutputRate((JASOutputRate)1);

    DspInit();
    gEnableReverb = std::getenv("SMGPC_NO_REVERB") == nullptr;

    // What JASAudioThread::run() and JASDriver::initAI() set up before the audio loop.
    JASChannel::initBankDisposeMsgQueue();
    JASDsp::initBuffer();
    JASDSPChannel::initAll();
    JASPoolAllocObject_MultiThreaded< JASChannel >::newMemPool(0x48);

    if (const char* dump = std::getenv("SMGPC_AUDIO_DUMP")) {
        sDumpFile = fopen(dump, "wb");
        if (sDumpFile != nullptr) {
            WriteWavHeader(sDumpFile, 0);
            fprintf(stderr, "[pc audio] dumping the output to %s\n", dump);
        } else {
            fprintf(stderr, "[pc audio] can't open the dump file %s\n", dump);
        }
    }

    if (std::getenv("SMGPC_NO_AUDIO") == nullptr) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            fprintf(stderr, "[pc audio] SDL audio init failed: %s\n", SDL_GetError());
        } else {
            const SDL_AudioSpec spec = {SDL_AUDIO_F32, kChannels, kSampleRate};
            sStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &GetNewAudio, nullptr);
            if (sStream == nullptr) {
                fprintf(stderr, "[pc audio] can't open the audio device: %s\n", SDL_GetError());
            }
        }
    }

    atexit(OnExit);

    if (sStream != nullptr) {
        SDL_ResumeAudioStreamDevice(sStream);
        fprintf(stderr, "[pc audio] output started (48000 Hz stereo, HLE DSP mixer)\n");
    } else if (sDumpFile != nullptr) {
        std::thread(TimerThreadMain).detach();
        fprintf(stderr, "[pc audio] no audio device: rendering on a timer thread for the dump\n");
    }
}

void Shutdown() {
    OnExit();
    if (sStream != nullptr) {
        SDL_DestroyAudioStream(sStream);
        sStream = nullptr;
    }
}

}  // namespace smgpc::audio
