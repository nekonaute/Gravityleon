#include "audio.h"
#include "modplayer.h"
#include "raylib.h"
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <functional>
#include <vector>

namespace {

bool gReady = false;
bool gKeep = false;                     // tests: keep the samples to write them out
std::vector<std::vector<short>> gKept;
Sound gSfx[Audio::kSfxCount];
float gGain[Audio::kSfxCount];        // each effect's own loudness: all kept soft
bool gSoundOn = true;
float gSoundVolume = 0.6f;
// Music: our own .mod player, fed to a raylib audio stream (raylib's loader hangs on looping modules).
ModPlayer gMod;
AudioStream gStream{};
bool gHaveMusic = false, gMusicOn = true, gMusicPlaying = false;
std::vector<short> gMixBuf;
const int kStreamFrames = 4096;
float gMusicVolume = 0.45f;
// A change of music under way: fading out, a silence, then the next one (gNext) fading in.
std::string gMusicPath;
ModPlayer gNext;
std::string gNextPath;
enum Phase { kCurrentTrack, kFadeOut, kGap } gPhase = kCurrentTrack;
float gFade = 1, gGapLeft = 0;
// An MP3 (the golden doors' music): raylib's own streaming decoder plays it, looping -- the modules'
// stream paused meanwhile. The same fades.
Music gMp3{}, gNextMp3{};
bool gMp3Cur = false, gNextIsMp3 = false;

bool isMp3(const std::string& p) {
    if (p.size() < 4) return false;
    std::string e = p.substr(p.size() - 4);
    for (char& c : e) c = (char)tolower((unsigned char)c);
    return e == ".mp3";
}

// An MP3 streamed by raylib: from its file -- raylib opens it itself -- or, in the single-file build
// (make dist), from the bytes baked into it. raylib's own opening would go past the baked-in files
// (src/embed.h), find nothing beside the game, and leave the Love and Solo music out. Those bytes
// are the embedded files' own, kept for the whole run (embed_data): they outlive any stream reading
// them -- the one playing, one waiting, one replaced or unloaded, at the music's toggles, at the end.
Music loadMp3(const std::string& path) {
#ifdef EMBED_ASSETS
    unsigned long n = 0;
    if (const unsigned char* d = embed_data(path.c_str(), &n))
        if (n > 0 && n <= (unsigned long)INT_MAX) return LoadMusicStreamFromMemory(".mp3", d, (int)n);
#endif
    return LoadMusicStream(path.c_str());
}

void setVolumes() {                                 // the one playing, at the fade's volume -- the other silent
    SetAudioStreamVolume(gStream, gMp3Cur ? 0 : gMusicVolume * gFade);
    if (gMp3Cur) SetMusicVolume(gMp3, gMusicVolume * gFade);
}

const int kRate = 22050;
const float kTau = 6.2831853f;

// Renders f(t) -- one sample per call, roughly -1..1 -- into a sound `dur` seconds long.
Sound render(float dur, const std::function<float(float)>& f) {
    int n = (int)(dur * kRate);
    std::vector<short> buf(n);
    for (int i = 0; i < n; i++) {
        float v = f(i / (float)kRate);
        v = v > 1 ? 1 : (v < -1 ? -1 : v);
        buf[i] = (short)(v * 30000);
    }
    // A few milliseconds of fade at both ends: no clicks.
    int edge = std::min(n / 4, (int)(0.003f * kRate));
    for (int i = 0; i < edge; i++) {
        float k = i / (float)edge;
        buf[i] = (short)(buf[i] * k);
        buf[n - 1 - i] = (short)(buf[n - 1 - i] * k);
    }
    if (gKeep) gKept.push_back(buf);
    Wave w;
    w.frameCount = (unsigned)n;
    w.sampleRate = kRate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = buf.data();
    return LoadSoundFromWave(w);       // copies the samples
}

// A tiny noise source with a one-pole low-pass: 'a' near 0 = dull, near 1 = bright.
struct Noise {
    unsigned s = 22222;
    float lp = 0;
    float next(float a) {
        s = s * 1664525u + 1013904223u;
        float white = ((s >> 9) & 0x7FFF) / 16383.5f - 1.0f;
        lp += (white - lp) * a;
        return lp;
    }
};

// A sine whose frequency glides from f0 to f1 over `glide` seconds (phase kept continuous).
struct Glide {
    float phase = 0, f0, f1, glide;
    Glide(float a, float b, float g) : f0(a), f1(b), glide(g) {}
    float next(float t) {
        float k = t / glide;
        k = k > 1 ? 1 : k;
        float f = f0 * powf(f1 / f0, k);
        phase += kTau * f / kRate;
        return sinf(phase);
    }
};

float decay(float t, float tau) { return expf(-t / tau); }
float attack(float t, float a) { return t < a ? t / a : 1.0f; }

void build() {
    using namespace Audio;
    {   // step: a soft thud -- dull noise and a low knock, very short
        Noise nz;
        gSfx[kStep] = render(0.07f, [nz](float t) mutable {
            return nz.next(0.12f) * 1.6f * decay(t, 0.012f) + sinf(kTau * 110 * t) * 0.45f * decay(t, 0.02f);
        });
        gGain[kStep] = 0.28f * 0.6f;                    // (0.6 of what it was)
    }
    {   // jump: a light rising chirp
        Glide g(260, 640, 0.12f);
        Noise nz;
        gSfx[kJump] = render(0.17f, [g, nz](float t) mutable {
            return (g.next(t) * 0.55f + nz.next(0.5f) * 0.12f) * attack(t, 0.005f) * decay(t, 0.06f);
        });
        gGain[kJump] = 0.32f;
    }
    {   // tongue out: "thwip" -- a fast downward sweep, a touch of grit
        Glide g(1150, 330, 0.09f);
        Noise nz;
        gSfx[kTongueOut] = render(0.12f, [g, nz](float t) mutable {
            float s = tanhf(2.2f * g.next(t)) * 0.45f + nz.next(0.7f) * 0.10f;
            return s * attack(t, 0.004f) * decay(t, 0.045f);
        });
        gGain[kTongueOut] = 0.30f;
    }
    {   // stick: "thock" -- a wet little splat and a low knock
        Noise nz;
        gSfx[kStick] = render(0.13f, [nz](float t) mutable {
            return nz.next(0.3f) * 1.3f * decay(t, 0.014f) + sinf(kTau * 170 * t) * 0.5f * decay(t, 0.045f);
        });
        gGain[kStick] = 0.34f;
    }
    {   // release: a little "pop"
        Glide g(480, 1600, 0.035f);
        gSfx[kRelease] = render(0.06f, [g](float t) mutable {
            return g.next(t) * 0.6f * attack(t, 0.002f) * decay(t, 0.02f);
        });
        gGain[kRelease] = 0.28f;
    }
    {   // swallow: "gulp" -- falling, with a wobble
        float ph = 0;
        gSfx[kSwallow] = render(0.19f, [ph](float t) mutable {
            float k = fminf(1, t / 0.14f);
            float f = 420 * powf(160.0f / 420.0f, k) * (1 + 0.12f * sinf(kTau * 28 * t));
            ph += kTau * f / kRate;
            return sinf(ph) * 0.6f * attack(t, 0.01f) * decay(t, 0.07f);
        });
        gGain[kSwallow] = 0.34f;
    }
    {   // fruit: a two-note chime
        gSfx[kFruit] = render(0.45f, [](float t) {
            float a = (sinf(kTau * 988 * t) + 0.25f * sinf(kTau * 1976 * t)) * decay(t, 0.09f);
            float tb = t - 0.11f;
            float b = tb > 0 ? (sinf(kTau * 1319 * tb) + 0.25f * sinf(kTau * 2638 * tb)) * attack(tb, 0.004f) * decay(tb, 0.14f) : 0;
            return (a + b) * 0.4f;
        });
        gGain[kFruit] = 0.40f;
    }
    {   // crash: a woody "bonk" on the head, then a short dizzy slide-whistle going down
        Glide g(620, 300, 0.06f);
        Noise nz;
        float ph = 0;
        gSfx[kCrash] = render(0.42f, [g, nz, ph](float t) mutable {
            float bonk = (g.next(t) + 0.35f * sinf(kTau * 1710 * t)) * decay(t, 0.05f) * 0.6f
                       + nz.next(0.6f) * decay(t, 0.006f) * 0.5f;
            float tw = t - 0.10f, whistle = 0;
            if (tw > 0) {
                float f = 1250 * powf(650.0f / 1250.0f, fminf(1, tw / 0.28f)) * (1 + 0.05f * sinf(kTau * 11 * tw));
                ph += kTau * f / kRate;
                whistle = sinf(ph) * 0.22f * attack(tw, 0.02f) * decay(tw, 0.12f);
            }
            return bonk + whistle;
        });
        gGain[kCrash] = 0.36f;
    }
    {   // thud: landing hard from a height -- a heavy, dull, low hit
        Glide g(95, 48, 0.12f);
        Noise nz;
        gSfx[kThud] = render(0.32f, [g, nz](float t) mutable {
            return g.next(t) * 0.8f * attack(t, 0.003f) * decay(t, 0.09f) + nz.next(0.08f) * 2.2f * decay(t, 0.035f);
        });
        gGain[kThud] = 0.42f;
    }
    {   // death: swallowed by dark matter -- a muffled burst, then a long falling tone
        Glide g(520, 70, 0.7f);
        Noise nz;
        gSfx[kDeath] = render(0.9f, [g, nz](float t) mutable {
            float burst = nz.next(0.25f) * 2.0f * decay(t, 0.05f);
            float tone = (g.next(t) + 0.3f * sinf(kTau * 3 * t) * g.next(t)) * 0.45f * attack(t, 0.02f) * decay(t, 0.35f);
            return burst + tone;
        });
        gGain[kDeath] = 0.40f;
    }
    {   // cough: two short breathy "kehs" -- a fly coming back out
        Noise nz;
        gSfx[kCough] = render(0.30f, [nz](float t) mutable {
            float s = 0;
            for (float t0 : {0.0f, 0.13f}) {
                float u = t - t0;
                if (u >= 0) s += (nz.next(0.35f) * 1.5f + sinf(kTau * 190 * u) * 0.25f) * attack(u, 0.004f) * decay(u, 0.04f);
            }
            return s;
        });
        gGain[kCough] = 0.26f;
    }
    {   // hat: an accordion squeezed -- a short reedy wheeze, two close notes gliding down
        Glide g1(330, 220, 0.2f), g2(337, 225, 0.2f);
        Noise nz;
        gSfx[kHat] = render(0.26f, [g1, g2, nz](float t) mutable {
            float reed = tanhf(2.5f * (g1.next(t) + g2.next(t))) * 0.4f;
            return (reed + nz.next(0.4f) * 0.15f) * attack(t, 0.01f) * decay(t, 0.09f);
        });
        gGain[kHat] = 0.28f;
    }
}

void applyMusic() {
    if (!gHaveMusic) return;
    bool want = gMusicOn;
    if (want && !gMusicPlaying) { if (gMp3Cur) ResumeMusicStream(gMp3); else PlayAudioStream(gStream); gMusicPlaying = true; }
    if (!want && gMusicPlaying) { if (gMp3Cur) PauseMusicStream(gMp3); else PauseAudioStream(gStream); gMusicPlaying = false; }   // resumes where it was
    setVolumes();
}

void switchToNext() {                               // the next music takes over, from its start
    gMusicPath = gNextPath;
    if (gNextPath.empty()) return;                  // (none: silence -- the old one goes on, at no volume)
    if (gMp3Cur) { StopMusicStream(gMp3); UnloadMusicStream(gMp3); gMp3Cur = false; }
    if (gNextIsMp3) {
        gMp3 = gNextMp3; gNextIsMp3 = false; gMp3Cur = true;
        PauseAudioStream(gStream);
        PlayMusicStream(gMp3);
        if (!gMusicPlaying) PauseMusicStream(gMp3);
    } else {
        gMod = gNext;
        if (gMusicPlaying) PlayAudioStream(gStream);
    }
    setVolumes();
    TraceLog(LOG_INFO, "AUDIO: music now %s", gMusicPath.c_str());
}

}  // namespace

void Audio::init(const std::string& musicPath) {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) { TraceLog(LOG_WARNING, "AUDIO: no audio device -- silent"); return; }
    gReady = true;
    build();
    gMusicPath = musicPath;
    if (gMod.load(musicPath)) {                     // it loops for ever by itself
        SetAudioStreamBufferSizeDefault(kStreamFrames);
        gStream = LoadAudioStream(gMod.rate(), 16, 2);
        gHaveMusic = IsAudioStreamValid(gStream);
        gMixBuf.resize(kStreamFrames * 2);
    }
    if (!gHaveMusic) TraceLog(LOG_WARNING, "AUDIO: could not load music %s", musicPath.c_str());
    applyMusic();
}

void Audio::shutdown() {
    if (!gReady) return;
    for (auto& s : gSfx) UnloadSound(s);
    if (gHaveMusic) UnloadAudioStream(gStream);
    if (gMp3Cur) UnloadMusicStream(gMp3);
    if (gNextIsMp3) UnloadMusicStream(gNextMp3);
    CloseAudioDevice();
    gReady = false;
}

void Audio::update(bool quietForPrompt, float fadeOut, float gap, float fadeIn) {
    if (!gReady || !gMusicPlaying) return;
    const float dt = fminf(GetFrameTime(), 0.1f);
    const float was = gFade;
    if (gPhase == kGap) {
        if ((gGapLeft -= dt) <= 0) { switchToNext(); gPhase = kCurrentTrack; }
    } else if (gPhase == kFadeOut || quietForPrompt || gMusicPath.empty()) {
        gFade = fmaxf(0, gFade - dt / fadeOut);
        if (gPhase == kFadeOut && gFade == 0) { gPhase = kGap; gGapLeft = gap; }
    } else {
        gFade = fminf(1, gFade + dt / fadeIn);
    }
    if (gFade != was) setVolumes();
    if (gMp3Cur) { UpdateMusicStream(gMp3); return; }
    while (IsAudioStreamProcessed(gStream)) {
        gMod.render(gMixBuf.data(), kStreamFrames);
        UpdateAudioStream(gStream, gMixBuf.data(), kStreamFrames);
    }
}

void Audio::play(Sfx s, float pitch, float volume) {
    if (!gReady || !gSoundOn) return;
    SetSoundPitch(gSfx[s], pitch);
    SetSoundVolume(gSfx[s], gGain[s] * gSoundVolume * volume);
    PlaySound(gSfx[s]);
}

void Audio::changeMusic(const std::string& path) {
    if (!gReady || !gHaveMusic) return;
    if (path == (gPhase == kCurrentTrack ? gMusicPath : gNextPath)) return;
    Music m{};
    bool mp3 = isMp3(path);
    if (mp3) {
        m = loadMp3(path);
        if (!IsMusicValid(m)) { TraceLog(LOG_WARNING, "AUDIO: could not load music %s", path.c_str()); return; }
        m.looping = true;
        TraceLog(LOG_INFO, "AUDIO: %s loaded (%.0f s, %u frames)", path.c_str(), GetMusicTimeLength(m), m.frameCount);
    } else if (!path.empty() && !gNext.load(path)) { TraceLog(LOG_WARNING, "AUDIO: could not load music %s", path.c_str()); return; }
    if (gNextIsMp3) UnloadMusicStream(gNextMp3);    // (one waiting: replaced)
    gNextMp3 = m; gNextIsMp3 = mp3;
    gNextPath = path;
    if (!gMusicPlaying) { switchToNext(); gFade = path.empty() ? 0 : 1; gPhase = kCurrentTrack; return; }
    if (gPhase != kGap) gPhase = kFadeOut;           // fade from the current volume, including a partially answered prompt
}

float Audio::musicTime(const std::string& path) {
    if (!gReady || !gMusicPlaying || !gMp3Cur || path != gMusicPath) return -1;
    return GetMusicTimePlayed(gMp3);
}

void Audio::setSoundOn(bool on) { gSoundOn = on; }
void Audio::setSoundVolume(float v) { gSoundVolume = v < 0 ? 0 : (v > 1 ? 1 : v); }
void Audio::setMusicOn(bool on) { gMusicOn = on; if (gReady) applyMusic(); }
void Audio::setMusicVolume(float v) { gMusicVolume = v < 0 ? 0 : (v > 1 ? 1 : v); if (gReady) applyMusic(); }

void Audio::check(const std::vector<std::string>& musicPaths, const std::string& dir) {
    InitAudioDevice();
    gKeep = true;
    build();
    static const char* names[kSfxCount] = {"step", "jump", "tongue_out", "stick", "release", "swallow", "fruit", "crash", "thud", "death", "cough", "hat"};
    for (int i = 0; i < kSfxCount; i++) {
        const auto& b = gKept[i];
        int peak = 0;
        double rms = 0;
        for (short v : b) { peak = std::max(peak, abs((int)v)); rms += (double)v * v; }
        rms = sqrt(rms / b.size()) / 32768.0;
        Wave w;
        w.frameCount = (unsigned)b.size(); w.sampleRate = kRate; w.sampleSize = 16; w.channels = 1;
        w.data = (void*)b.data();
        std::string path = dir + "/sfx_" + names[i] + ".wav";
        ExportWave(w, path.c_str());
        printf("AUDIOCHECK %-10s %.3f s  peak %.2f  rms %.3f  gain %.2f  -> %s\n", names[i], b.size() / (float)kRate,
               peak / 32768.0, rms, gGain[i], path.c_str());
    }
    // Each music: 20 s rendered by our player, written out, with its loudness second by second.
    for (size_t m = 0; m < musicPaths.size(); m++) {
        const std::string& musicPath = musicPaths[m];
        ModPlayer mp;
        if (!mp.load(musicPath)) { printf("AUDIOCHECK music %s: DOES NOT LOAD\n", musicPath.c_str()); continue; }
        const int secs = 20;
        std::vector<short> pcm((size_t)mp.rate() * secs * 2);
        mp.render(pcm.data(), mp.rate() * secs);
        printf("AUDIOCHECK music %s loads; loudness (rms) per second:", musicPath.c_str());
        for (int k = 0; k < secs; k++) {
            double e = 0;
            for (int j = 0; j < mp.rate() * 2; j++) { double v = pcm[(size_t)k * mp.rate() * 2 + j] / 32768.0; e += v * v; }
            printf(" %.2f", sqrt(e / (mp.rate() * 2)));
        }
        printf("\n");
        Wave w;
        w.frameCount = (unsigned)(mp.rate() * secs); w.sampleRate = mp.rate(); w.sampleSize = 16; w.channels = 2;
        w.data = pcm.data();
        std::string path = dir + "/music" + std::to_string(m) + "_first20s.wav";
        ExportWave(w, path.c_str());
        printf("AUDIOCHECK music -> %s\n", path.c_str());
        // ...then 3 more minutes, past the end of the song: it must loop round, never fall silent.
        std::vector<short> more((size_t)mp.rate() * 10 * 2);
        printf("AUDIOCHECK music 20 s - 200 s, rms per 10 s:");
        for (int k = 0; k < 18; k++) {
            mp.render(more.data(), mp.rate() * 10);
            double e = 0;
            for (short v : more) { double x = v / 32768.0; e += x * x; }
            printf(" %.2f", sqrt(e / more.size()));
        }
        printf("\n");
    }
    CloseAudioDevice();
}
