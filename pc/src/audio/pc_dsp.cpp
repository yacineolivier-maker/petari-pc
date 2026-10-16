// HLE of the JAudio2 DSP ucode (the Wii runs the same DSP mixer as the GameCube): decodes the wave
// data (ADPCM4 / PCM16 / PCM8 from emulated ARAM), resamples it with the channel pitch, applies the
// biquad filter and the per-bus volumes, and mixes all 64 voices of JASDsp::CH_BUF into stereo.
// Writes back the feedback fields the CPU side reads (finished flag, ARAM read position, samples
// left), which JASDSPChannel and JASAramStream (streamed music) rely on.
//
// Adapted from Dusklight, the Twilight Princess PC port (CC0 1.0):
//   src/dusk/audio/DuskDsp.cpp and Adpcm.cpp (by the Dusklight contributors).
// SMG's JASDsp::TChannel uses offset names; the Dusklight names are noted next to each accessor.
// The reverb is a compact Freeverb (Jezar at Dreampoint, public domain) fed by the FX send buses.

#include "pc_dsp.h"

#include "JSystem/JAudio2/JASDSPInterface.hpp"
#include "JSystem/JAudio2/JASDriverIF.hpp"

// Aurora's ARAM emulation (dolphin/ar.h clashes with the revolution shims): ARAM addresses are
// offsets into the buffer returned by ARGetStorageAddress().
extern "C" void* ARGetStorageAddress(void);
extern "C" u32 ARGetSize(void);

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>

namespace smgpc::audio {

bool gEnableReverb = true;
int gFxVoices = 0;

namespace {

constexpr int kNumChannels = 64;
constexpr int kNumBuses = 6;

// Bus connect values written by TChannel::setBusConnect (connect_table): main L/R, FX send L/R.
constexpr u16 kBusMainL = 0x0D00;
constexpr u16 kBusMainR = 0x0D60;
constexpr u16 kBusFxL = 0x0DC0;
constexpr u16 kBusFxR = 0x0E20;

struct BiquadCoeffs {
    f32 b1, b2, a1, a2;
};

// Per-voice state the real DSP keeps in its own memory.
struct ChannelAux {
    s16 hist1;  // ADPCM history: last sample
    s16 hist2;  // ADPCM history: sample before

    f32 prevMain[2];  // last applied main L/R volume (NaN after reset: start at the init volume)
    f32 prevFx[2];

    static constexpr int kDecodeBufSize = 2048;
    s16 decodeBuf[kDecodeBufSize];
    int decodeBufCount;
    f32 resamplePos;
    s16 resamplePrev;

    u16 oscPhase;

    u16 curBiquad[4];
    BiquadCoeffs biquad;
    f32 biqIn1, biqIn2, biqOut1, biqOut2;

    bool reportedBadFormat;
};

ChannelAux sAux[kNumChannels];

// --- TChannel field accessors (SMG names -> Dusklight names) --------------------------------
inline u16& ResetFlag(JASDsp::TChannel& c) { return c._8; }             // mResetFlag
inline u16* OutputBus(JASDsp::TChannel& c, int i) { return &c._10[0][0] + i * 4; }  // mOutputChannels[i]
inline u16 AutoPanDolby(const JASDsp::TChannel& c) { return c._50; }    // mAutoMixerPanDolby
inline u16 AutoFxMix(const JASDsp::TChannel& c) { return c._52; }       // mAutoMixerFxMix
inline u16 AutoInitVolume(const JASDsp::TChannel& c) { return c._54; }  // mAutoMixerInitVolume
inline u16 AutoVolume(const JASDsp::TChannel& c) { return c._56; }      // mAutoMixerVolume
inline u16 AutoBeenSet(const JASDsp::TChannel& c) { return c._58; }     // mAutoMixerBeenSet
inline u32 SamplesPerBlock(const JASDsp::TChannel& c) { return c._64; } // mSamplesPerBlock
inline u32& SamplePos(JASDsp::TChannel& c) { return *reinterpret_cast<u32*>(&c._68); }  // mSamplePosition
inline u32& AramStreamPos(JASDsp::TChannel& c) { return *reinterpret_cast<u32*>(&c._70); }  // mAramStreamPosition
inline u32& SamplesLeft(JASDsp::TChannel& c) { return *reinterpret_cast<u32*>(&c._74); }  // mSamplesLeft
inline u32 BytesPerBlock(const JASDsp::TChannel& c) { return c._100; }  // mBytesPerBlock
inline u16 LoopFlag(const JASDsp::TChannel& c) { return c._102; }       // mLoopFlag
inline s16 LoopLast(const JASDsp::TChannel& c) { return c._104; }       // mpLast
inline s16 LoopPenult(const JASDsp::TChannel& c) { return c._106; }     // mpPenult
inline u32 LoopStart(const JASDsp::TChannel& c) { return c._110; }      // mLoopStartSample
inline u32 EndSample(const JASDsp::TChannel& c) { return c._114; }      // mEndSample
inline u32 WaveAram(const JASDsp::TChannel& c) { return c._118; }       // mWaveAramAddress

f32 VolumeFromU16(u16 v) {
    return static_cast<f32>(v) / static_cast<f32>(JASDriver::getChannelLevel_dsp());
}

// --- Wave decoding --------------------------------------------------------------------------
// https://github.com/magcius/vgmtrans/blob/8e34ddc2/src/main/formats/JaiSeqScanner.cpp#L489-L531
constexpr s16 kAdpcmCoef0[16] = {0,      0x0800, 0,      0x0400, 0x1000, 0x0e00, 0x0c00, 0x1200,
                                 0x1068, 0x12c0, 0x1400, 0x0800, 0x0400, -0x400, -0x400, -0x800};
constexpr s16 kAdpcmCoef1[16] = {0,      0,      0x0800, 0x0400, -0x800, -0x600, -0x400, -0xa00,
                                 -0x8c8, -0x8fc, -0xc00, -0x800, -0x400, 0x0400, 0,      0};

s16 Clamp16(s32 v) {
    return (s16)std::clamp<s32>(v, -0x8000, 0x7FFF);
}

// 4-bit (9 bytes / 16 samples) and 2-bit (5 bytes / 16 samples) Nintendo/JAudio ADPCM.
void AdpcmToPcm16(const u8* src, u32 frameBytes, u32 frames, s16* pcm, u32 pcmLength, s16& hist1, s16& hist2) {
    s16* end = pcm + pcmLength;
    const int bits = frameBytes == 9 ? 4 : 2;
    for (u32 f = 0; f < frames; f++, src += frameBytes) {
        const u8 header = src[0];
        const s32 scale = 1 << (header >> 4);
        const s32 c0 = kAdpcmCoef0[header & 0xF];
        const s32 c1 = kAdpcmCoef1[header & 0xF];
        for (int i = 0; i < 16; i++) {
            s32 nibble;
            if (bits == 4) {
                const u8 b = src[1 + i / 2];
                nibble = (s8)((i & 1) ? (b << 4) : (b & 0xF0)) >> 4;
            } else {
                const u8 b = src[1 + i / 4];
                nibble = (s8)(b << ((i & 3) * 2)) >> 6;
                nibble <<= 2;  // 2-bit codes are scaled up to the 4-bit range
            }
            const s16 sample = Clamp16((((nibble * scale) << 11) + (c0 * hist1 + c1 * hist2)) >> 11);
            hist2 = hist1;
            hist1 = sample;
            *pcm++ = sample;
            if (pcm == end) {
                return;
            }
        }
    }
}

bool IsAdpcm(const JASDsp::TChannel& c) {
    return SamplesPerBlock(c) == 16;
}

// Bytes per block as the CPU side sees it (PCM formats store bits per sample in _100).
u32 BlockBytes(const JASDsp::TChannel& c) {
    if (SamplesPerBlock(c) == 1) {
        return BytesPerBlock(c) == 16 ? 2 : 1;
    }
    return BytesPerBlock(c);
}

bool ValidFormat(const JASDsp::TChannel& c) {
    if (SamplesPerBlock(c) == 16) {
        return BytesPerBlock(c) == 9 || BytesPerBlock(c) == 5;
    }
    if (SamplesPerBlock(c) == 1) {
        return BytesPerBlock(c) == 16 || BytesPerBlock(c) == 8;
    }
    return false;
}

u32 SamplesToBytes(const JASDsp::TChannel& c, u32 samples) {
    const u32 spb = SamplesPerBlock(c);
    return ((samples + spb - 1) / spb) * BlockBytes(c);
}

void ResetChannel(JASDsp::TChannel& c, ChannelAux& aux) {
    SamplesLeft(c) = EndSample(c) - SamplePos(c);
    aux.hist1 = 0;
    aux.hist2 = 0;
    aux.decodeBufCount = 0;
    aux.resamplePos = 0.0f;
    aux.resamplePrev = 0;
    aux.oscPhase = 0;
    aux.biqIn1 = aux.biqIn2 = aux.biqOut1 = aux.biqOut2 = 0.0f;
    aux.prevMain[0] = aux.prevMain[1] = NAN;
    aux.prevFx[0] = aux.prevFx[1] = NAN;
    ResetFlag(c) = 0;
}

// Reads one contiguous run of samples. Returns how many were written to out (may be fewer than
// asked), or -1 if the wave data lies outside ARAM.
int ReadSamplesChunk(JASDsp::TChannel& c, ChannelAux& aux, int desired, s16* out, int outSize) {
    const u8* aram = static_cast<const u8*>(ARGetStorageAddress());
    const u32 aramSize = ARGetSize();
    const u32 spb = SamplesPerBlock(c);

    // Loops can start mid-block: rewind to the block start and drop the extra samples.
    const u32 skip = SamplePos(c) % spb;
    if (skip != 0) {
        desired += (int)skip;
        SamplesLeft(c) += skip;
        SamplePos(c) -= skip;
    }
    desired = (int)(((u32)desired + spb - 1) / spb * spb);

    const u32 renderSamples = std::min<u32>(SamplesLeft(c), (u32)desired);
    const u32 dataPos = WaveAram(c) + SamplesToBytes(c, SamplePos(c));
    const u32 dataLen = SamplesToBytes(c, renderSamples);
    if (aram == nullptr || dataPos >= aramSize || dataLen > aramSize - dataPos) {
        return -1;
    }
    const u8* data = aram + dataPos;

    s16 tmp[ChannelAux::kDecodeBufSize + 32];
    const u32 n = std::min<u32>(renderSamples, (u32)(sizeof(tmp) / sizeof(tmp[0])));
    if (spb == 1) {
        if (BytesPerBlock(c) == 16) {
            for (u32 i = 0; i < n; i++) {
                tmp[i] = (s16)((data[i * 2] << 8) | data[i * 2 + 1]);  // big-endian PCM16
            }
        } else {
            for (u32 i = 0; i < n; i++) {
                tmp[i] = (s16)((s8)data[i] << 8);
            }
        }
    } else {
        AdpcmToPcm16(data, BytesPerBlock(c), (n + 15) / 16, tmp, n, aux.hist1, aux.hist2);
    }

    SamplesLeft(c) -= n;
    SamplePos(c) += n;

    int count = std::min((int)n - (int)skip, outSize);
    if (count > 0) {
        memcpy(out, tmp + skip, count * sizeof(s16));
    }
    return std::max(count, 0);
}

// Fills the decode buffer with at least `needed` samples (fewer if a one-shot wave ends).
// Returns false on a fatal data error.
bool FillDecodeBuf(JASDsp::TChannel& c, ChannelAux& aux, int needed) {
    while (aux.decodeBufCount < needed) {
        if (SamplesLeft(c) == 0) {
            if (!LoopFlag(c)) {
                break;
            }
            if (EndSample(c) <= LoopStart(c)) {
                break;  // degenerate loop: would spin forever
            }
            SamplesLeft(c) = EndSample(c) - LoopStart(c);
            SamplePos(c) = LoopStart(c);
            aux.hist1 = LoopLast(c);
            aux.hist2 = LoopPenult(c);
        }
        const int space = ChannelAux::kDecodeBufSize - aux.decodeBufCount;
        if (space <= 0) {
            break;
        }
        const int got = ReadSamplesChunk(c, aux, std::min(space, needed - aux.decodeBufCount),
                                         aux.decodeBuf + aux.decodeBufCount, space);
        if (got < 0) {
            return false;
        }
        aux.decodeBufCount += got;
    }
    AramStreamPos(c) = WaveAram(c) + SamplesToBytes(c, SamplePos(c));
    return true;
}

// Adapts IIR biquad coefficients designed for 32 kHz to 48 kHz, preserving the frequencies
// (pole/zero mapping z -> z^(32/48)). From Dusklight.
BiquadCoeffs RemapBiquad32to48(f32 b1, f32 b2, f32 a1, f32 a2) {
    using cplx = std::complex<f64>;
    constexpr f64 kPi = 3.14159265358979323846;
    constexpr f64 kOldRate = 32000.0;
    constexpr f64 kNewRate = (f64)kSampleRate;
    constexpr f64 kGamma = kOldRate / kNewRate;

    if (b1 == 0 && b2 == 0 && a1 == 0 && a2 == 0) {
        return {0, 0, 0, 0};
    }

    auto mapPoint = [](cplx z) {
        if (std::abs(z.imag()) < 1e-9 * std::max(std::abs(z.real()), 1.0)) {
            f64 r = z.real();
            f64 sign = (r >= 0.0) ? 1.0 : -1.0;
            return cplx(sign * std::pow(std::abs(r), kGamma), 0.0);
        }
        return std::polar(std::pow(std::abs(z), kGamma), std::arg(z) * kGamma);
    };

    f64 disc = (f64)a1 * a1 + 4.0 * a2;
    cplx p1, p2;
    if (disc >= 0.0) {
        f64 sq = std::sqrt(disc);
        p1 = {(a1 + sq) * 0.5, 0.0};
        p2 = {(a1 - sq) * 0.5, 0.0};
    } else {
        f64 sq = std::sqrt(-disc);
        p1 = {a1 * 0.5, sq * 0.5};
        p2 = {a1 * 0.5, -sq * 0.5};
    }
    cplx p1n = mapPoint(p1);
    cplx p2n = mapPoint(p2);
    f64 a1n = (p1n + p2n).real();
    f64 a2n = -(p1n * p2n).real();
    f64 z0n = (b1 == 0.0f) ? 0.0 : mapPoint(cplx(-(f64)b2 / b1, 0.0)).real();

    f64 denOldDc = 1.0 - a1 - a2;
    f64 fRef = (std::abs(denOldDc) > 1e-6) ? 0.0 : 1.0;
    auto H = [](cplx z, cplx q1, cplx q2, f64 bb1, f64 bb2) { return (bb1 * z + bb2) / (z * (z - q1) * (z - q2)); };
    cplx zOldRef = std::polar(1.0, 2.0 * kPi * fRef / kOldRate);
    cplx zNewRef = std::polar(1.0, 2.0 * kPi * fRef / kNewRate);
    cplx hOldRef = H(zOldRef, p1, p2, b1, b2) * zOldRef;
    cplx shapeNewRef = H(zNewRef, p1n, p2n, 1.0, -z0n) * zNewRef;
    f64 K = (hOldRef / shapeNewRef).real();
    return {(f32)K, (f32)(-K * z0n), (f32)a1n, (f32)a2n};
}

// --- Oscillators (wave-less channels, setOscInfo) ------------------------------------------
s16 sEvolvingHarmonic[64];

void GenerateEvolvingHarmonic() {
    static bool initialized = false;
    if (!initialized) {
        sEvolvingHarmonic[62] = 8191;
        sEvolvingHarmonic[63] = 16383;
        initialized = true;
    }
    u32 prev2 = (u32)sEvolvingHarmonic[62];
    u32 prev1 = (u32)sEvolvingHarmonic[63];
    for (int i = 0; i < 64; i += 2) {
        u32 cur = (u32)sEvolvingHarmonic[i];
        sEvolvingHarmonic[i] = (s16)((s32)(prev2 * prev1 - (cur << 16)) >> 16);
        prev2 = prev1;
        prev1 = cur;
        cur = (u32)sEvolvingHarmonic[i + 1];
        sEvolvingHarmonic[i + 1] = (s16)((s32)(2u * (prev2 * prev1 + (cur << 16))) >> 16);
        prev2 = prev1;
        prev1 = cur;
    }
}

void RenderOscChannel(JASDsp::TChannel& c, ChannelAux& aux, f32* buf) {
    if (ResetFlag(c)) {
        ResetChannel(c, aux);
    }
    const u32 step = c.mPitch >> 1;
    for (int i = 0; i < kSubframeSize; i++) {
        const u16 ph = aux.oscPhase;
        f32 s;
        switch (BytesPerBlock(c)) {
        case 0:  // square, 50% duty
            s = ph < 0x8000u ? 0.5f : -0.5f;
            break;
        case 3:  // square, 25% duty
            s = ph < 0x4000u ? 0.5f : -0.5f;
            break;
        case 1:   // saw
        case 12:  // evolving ramp
            s = (f32)(s16)ph / 32768.0f;
            break;
        case 4:  // triangle
            s = 0.5f - fabsf((f32)(s16)ph / 32768.0f);
            break;
        case 7:   // sine
        case 10:  // sine, variable step
            s = sinf((f32)ph * (2.0f * 3.14159265f / 65536.0f)) * 0.5f;
            break;
        case 11:  // evolving harmonic
            s = sEvolvingHarmonic[ph >> 10] / 32768.0f;
            break;
        default:
            s = 0.0f;
            break;
        }
        buf[i] = s;
        aux.oscPhase = (u16)(ph + step);
    }
}

// Renders one voice into a mono buffer. Returns false when the channel can't be played.
bool RenderWaveChannel(JASDsp::TChannel& c, ChannelAux& aux, f32* buf) {
    if (ResetFlag(c)) {
        ResetChannel(c, aux);
    }

    const f32 step = (f32)c.mPitch / 4096.0f;
    const int needed = (int)(aux.resamplePos + kSubframeSize * step) + 2;
    if (!FillDecodeBuf(c, aux, needed)) {
        return false;
    }
    if (aux.decodeBufCount < needed) {
        c.mIsFinished = 1;  // source ran dry
    }

    // Linear resampling.
    f32 pos = aux.resamplePos;
    s16 prev = aux.resamplePrev;
    s16 next = aux.decodeBufCount > 0 ? aux.decodeBuf[0] : prev;
    int srcIdx = 0;
    for (int i = 0; i < kSubframeSize; i++) {
        buf[i] = (prev + pos * (next - prev)) / 32768.0f;
        pos += step;
        while (pos >= 1.0f) {
            pos -= 1.0f;
            prev = next;
            srcIdx++;
            next = srcIdx < aux.decodeBufCount ? aux.decodeBuf[srcIdx] : prev;
        }
    }
    aux.resamplePos = pos;
    aux.resamplePrev = prev;

    // Biquad IIR: out[n] = (b1*in[n-1] + b2*in[n-2] + a1*out[n-1] + a2*out[n-2]) / 32768.
    if ((c.mFilterMode & 0x20) != 0) {
        const s16* p = c.iir_filter_params;
        if (memcmp(p, aux.curBiquad, sizeof(aux.curBiquad)) != 0) {
            aux.biquad = RemapBiquad32to48(p[0] / 32768.0f, p[1] / 32768.0f, p[2] / 32768.0f, p[3] / 32768.0f);
            memcpy(aux.curBiquad, p, sizeof(aux.curBiquad));
        }
        const BiquadCoeffs k = aux.biquad;
        for (int i = 0; i < kSubframeSize; i++) {
            f32 out = std::clamp(k.b1 * aux.biqIn1 + k.b2 * aux.biqIn2 + k.a1 * aux.biqOut1 + k.a2 * aux.biqOut2, -1.0f, 1.0f);
            aux.biqIn2 = aux.biqIn1;
            aux.biqIn1 = buf[i];
            aux.biqOut2 = aux.biqOut1;
            buf[i] = aux.biqOut1 = out;
        }
    }

    const int remaining = aux.decodeBufCount - srcIdx;
    if (remaining > 0) {
        memmove(aux.decodeBuf, aux.decodeBuf + srcIdx, remaining * sizeof(s16));
    }
    aux.decodeBufCount = std::max(0, remaining);
    return true;
}

// Mixes src * ramp(start -> end) into dst.
void MixRamp(f32* dst, const f32* src, f32& prev, f32 target, f32 init) {
    if (std::isnan(prev)) {
        prev = init;
    }
    const f32 start = prev;
    prev = target;
    if (start == 0.0f && target == 0.0f) {
        return;
    }
    const f32 inc = (target - start) / (f32)kSubframeSize;
    for (int i = 0; i < kSubframeSize; i++) {
        dst[i] += src[i] * (start + inc * i);
    }
}

// --- Reverb (Freeverb, Jezar at Dreampoint, public domain) ------------------------------------
class Freeverb {
public:
    void init() {
        static const int kComb[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int kAllpass[4] = {556, 441, 341, 225};
        constexpr int kSpread = 23;
        const f32 rateScale = (f32)kSampleRate / 44100.0f;
        for (int i = 0; i < 8; i++) {
            mCombL[i].init((int)(kComb[i] * rateScale));
            mCombR[i].init((int)((kComb[i] + kSpread) * rateScale));
        }
        for (int i = 0; i < 4; i++) {
            mApL[i].init((int)(kAllpass[i] * rateScale));
            mApR[i].init((int)((kAllpass[i] + kSpread) * rateScale));
        }
        setParams(0.5f, 0.7f);
    }

    void setParams(f32 roomSize, f32 damp) {
        const f32 feedback = roomSize * 0.28f + 0.7f;
        const f32 d = powf(damp * 0.4f, 32000.0f / 48000.0f);
        for (int i = 0; i < 8; i++) {
            mCombL[i].feedback = mCombR[i].feedback = feedback;
            mCombL[i].damp = mCombR[i].damp = d;
        }
    }

    // Adds the wet signal to outL/outR. Returns the wet energy (to detect the end of the tail).
    f32 process(const f32* inL, const f32* inR, f32* outL, f32* outR, int n) {
        constexpr f32 kFixedGain = 0.015f;
        constexpr f32 kWet = 3.0f;  // scalewet (3) * wet (1), width 1
        f32 energy = 0.0f;
        for (int s = 0; s < n; s++) {
            const f32 input = (inL[s] + inR[s]) * kFixedGain;
            f32 l = 0.0f, r = 0.0f;
            for (int i = 0; i < 8; i++) {
                l += mCombL[i].process(input);
                r += mCombR[i].process(input);
            }
            for (int i = 0; i < 4; i++) {
                l = mApL[i].process(l);
                r = mApR[i].process(r);
            }
            l *= kWet;
            r *= kWet;
            energy += l * l + r * r;
            outL[s] += l;
            outR[s] += r;
        }
        return energy;
    }

private:
    struct Comb {
        f32 buf[2048];
        int size = 1, idx = 0;
        f32 feedback = 0.0f, damp = 0.0f, store = 0.0f;
        void init(int n) {
            size = std::min(n, 2048);
            idx = 0;
            store = 0.0f;
            memset(buf, 0, sizeof(buf));
        }
        f32 process(f32 in) {
            f32 out = buf[idx];
            store = out * (1.0f - damp) + store * damp;
            if (fabsf(store) < 1e-20f) store = 0.0f;
            buf[idx] = in + store * feedback;
            if (++idx >= size) idx = 0;
            return out;
        }
    };
    struct Allpass {
        f32 buf[1024];
        int size = 1, idx = 0;
        void init(int n) {
            size = std::min(n, 1024);
            idx = 0;
            memset(buf, 0, sizeof(buf));
        }
        f32 process(f32 in) {
            f32 b = buf[idx];
            if (fabsf(b) < 1e-20f) b = 0.0f;
            buf[idx] = in + b * 0.5f;
            if (++idx >= size) idx = 0;
            return b - in;
        }
    };
    Comb mCombL[8], mCombR[8];
    Allpass mApL[4], mApR[4];
};

Freeverb sReverb;
bool sReverbHasTail = false;

}  // namespace

void DspInit() {
    for (ChannelAux& aux : sAux) {
        memset(&aux, 0, sizeof(aux));
        aux.prevMain[0] = aux.prevMain[1] = NAN;
        aux.prevFx[0] = aux.prevFx[1] = NAN;
    }
    sReverb.init();
}

void DspRender(StereoSubframe& out) {
    memset(&out, 0, sizeof(out));
    if (JASDsp::CH_BUF == nullptr) {
        return;
    }

    GenerateEvolvingHarmonic();

    StereoSubframe fx;
    memset(&fx, 0, sizeof(fx));
    bool anyFx = false;
    int fxVoices = 0;

    for (int i = 0; i < kNumChannels; i++) {
        JASDsp::TChannel& c = JASDsp::CH_BUF[i];
        ChannelAux& aux = sAux[i];

        if (!c.mIsActive || c.mPauseFlag) {
            continue;
        }
        if (c.mForcedStop) {
            c.mIsFinished = 1;
            continue;
        }

        f32 mono[kSubframeSize];
        if (WaveAram(c) == 0) {
            RenderOscChannel(c, aux, mono);
        } else {
            if (!ValidFormat(c) || !RenderWaveChannel(c, aux, mono)) {
                if (!aux.reportedBadFormat) {
                    aux.reportedBadFormat = true;
                    fprintf(stderr, "[pc audio] channel %d: can't play wave (format %u/%u, ARAM 0x%X), stopping it\n",
                            i, (unsigned)SamplesPerBlock(c), (unsigned)BytesPerBlock(c), (unsigned)WaveAram(c));
                }
                c.mIsFinished = 1;
                continue;
            }
        }

        bool voiceFx = false;
        if (AutoBeenSet(c)) {
            // Auto mixer ("Dolby" mix configs): volume, pan and FX send computed by the DSP.
            const f32 vol = VolumeFromU16(AutoVolume(c));
            const f32 initVol = VolumeFromU16(AutoInitVolume(c));
            const f32 right = (f32)(AutoPanDolby(c) >> 8) / 127.0f;
            const f32 left = 1.0f - right;
            MixRamp(out.l, mono, aux.prevMain[0], left * vol, left * initVol);
            MixRamp(out.r, mono, aux.prevMain[1], right * vol, right * initVol);
            const f32 fxGain = (f32)(AutoFxMix(c) >> 8) / 127.0f;
            if (gEnableReverb && fxGain > 0.0f) {
                anyFx = voiceFx = true;
                MixRamp(fx.l, mono, aux.prevFx[0], left * vol * fxGain, left * initVol * fxGain);
                MixRamp(fx.r, mono, aux.prevFx[1], right * vol * fxGain, right * initVol * fxGain);
            }
        } else {
            for (int b = 0; b < kNumBuses; b++) {
                u16* bus = OutputBus(c, b);  // {connect, target volume, current volume, progress}
                const f32 target = VolumeFromU16(bus[1]);
                const f32 init = VolumeFromU16(bus[2]);
                switch (bus[0]) {
                case kBusMainL:
                    MixRamp(out.l, mono, aux.prevMain[0], target, init);
                    break;
                case kBusMainR:
                    MixRamp(out.r, mono, aux.prevMain[1], target, init);
                    break;
                case kBusFxL:
                    if (gEnableReverb) {
                        anyFx = true;
                        voiceFx |= target > 0.0f;
                        MixRamp(fx.l, mono, aux.prevFx[0], target, init);
                    }
                    break;
                case kBusFxR:
                    if (gEnableReverb) {
                        anyFx = true;
                        voiceFx |= target > 0.0f;
                        MixRamp(fx.r, mono, aux.prevFx[1], target, init);
                    }
                    break;
                default:
                    break;
                }
                bus[2] = bus[1];  // the DSP has reached the target volume
            }
        }
        fxVoices += voiceFx;
    }
    gFxVoices = fxVoices;

    if (gEnableReverb && (anyFx || sReverbHasTail)) {
        // -80 dBFS threshold on the subframe's wet energy.
        constexpr f32 kEpsilon = 2.0f * kSubframeSize * 1e-8f;
        sReverbHasTail = sReverb.process(fx.l, fx.r, out.l, out.r, kSubframeSize) >= kEpsilon;
    }
}

}  // namespace smgpc::audio
