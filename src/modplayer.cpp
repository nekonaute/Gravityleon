#include "modplayer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

const double kPalClock = 3546894.6;    // Amiga PAL: a period P plays at kPalClock / P Hz

const int kSine[32] = {0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
                       255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24};

int u16(const unsigned char* p) { return (p[0] << 8) | p[1]; }

}  // namespace

bool ModPlayer::load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<unsigned char> d;
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + n);
    fclose(f);
    if (d.size() < 1084) return false;
    songLen_ = d[950];
    if (songLen_ < 1 || songLen_ > 128) return false;
    int npat = 0;
    for (int i = 0; i < 128; i++) { order_[i] = d[952 + i]; if (order_[i] + 1 > npat) npat = order_[i] + 1; }
    size_t patBytes = (size_t)npat * 1024;
    if (d.size() < 1084 + patBytes) return false;
    patterns_.assign(d.begin() + 1084, d.begin() + 1084 + patBytes);
    size_t at = 1084 + patBytes;
    for (int i = 0; i < 31; i++) {
        const unsigned char* h = &d[20 + i * 30];
        int len = u16(h + 22) * 2;
        Sample& s = samples_[i];
        s.finetune = h[24] & 15;
        if (s.finetune > 7) s.finetune -= 16;
        s.volume = h[25] > 64 ? 64 : h[25];
        s.loopStart = u16(h + 26) * 2;
        s.loopLen = u16(h + 28) * 2;
        s.data.clear();
        for (int k = 0; k < len && at + k < d.size(); k++) s.data.push_back((signed char)d[at + k] / 128.0f);
        at += len;
        if (s.loopStart + s.loopLen > (int)s.data.size()) s.loopLen = (int)s.data.size() - s.loopStart;
        if (s.loopLen < 0) s.loopLen = 0;
    }
    tickFrames_ = kRate * 2.5 / bpm_;
    tickLeft_ = 0;
    tick_ = 0; row_ = 0; orderPos_ = 0;
    loaded_ = true;
    return true;
}

void ModPlayer::triggerNote(Channel& c, int note, int smp, int effect, int param) {
    if (smp > 0 && smp <= 31) { c.sample = smp - 1; c.volume = samples_[c.sample].volume; }
    if (note > 0 && c.sample >= 0) {
        int ft = samples_[c.sample].finetune;
        int per = (int)lround(note * pow(2.0, -ft / 96.0));   // finetune: 1/8 semitone steps
        if (effect == 3 || effect == 5) { c.target = per; return; }   // tone portamento: glide there
        c.period = c.basePeriod = per;
        c.pos = (effect == 9) ? ((param ? param : c.lastOffset) * 256.0) : 0;
        if (effect == 9 && param) c.lastOffset = param;
        c.vibPos = 0; c.tremPos = 0;
        c.active = true;
    }
}

void ModPlayer::startRow() {
    if (orderPos_ >= songLen_) orderPos_ = 0;
    const unsigned char* row = &patterns_[order_[orderPos_] * 1024 + row_ * 16];
    for (int i = 0; i < 4; i++) {
        Channel& c = ch_[i];
        const unsigned char* b = row + i * 4;
        int smp = (b[0] & 0xF0) | (b[2] >> 4);
        int note = ((b[0] & 0x0F) << 8) | b[1];
        c.effect = b[2] & 0x0F;
        c.param = b[3];
        int x = c.param >> 4, y = c.param & 15;
        c.cutTick = -1;
        if (c.effect == 0xE && x == 0xD && y > 0) {   // note delay: keep it for later
            c.delayTicks = y; c.pendingNote = note; c.pendingSample = smp;
        } else {
            c.delayTicks = -1;
            triggerNote(c, note, smp, c.effect, c.param);
        }
        switch (c.effect) {
            case 0x3: if (c.param) c.portaSpeed = c.param; break;
            case 0x4: if (x) c.vibSpeed = x; if (y) c.vibDepth = y; break;
            case 0x7: if (x) c.tremSpeed = x; if (y) c.tremDepth = y; break;
            case 0xB: jumpOrder_ = c.param; break;
            case 0xC: c.volume = c.param > 64 ? 64 : c.param; break;
            case 0xD: breakRow_ = x * 10 + y; if (breakRow_ > 63) breakRow_ = 0; break;
            case 0xF:
                if (c.param == 0) break;
                if (c.param < 32) speed_ = c.param;
                else { bpm_ = c.param; tickFrames_ = kRate * 2.5 / bpm_; }
                break;
            case 0xE:
                switch (x) {
                    case 0x1: c.period -= y; break;                                  // fine porta up
                    case 0x2: c.period += y; break;                                  // fine porta down
                    case 0x6:                                                        // pattern loop
                        if (y == 0) c.loopRow = row_;
                        else if (c.loopCount == 0) { c.loopCount = y; breakRow_ = c.loopRow; jumpOrder_ = orderPos_; }
                        else if (--c.loopCount > 0) { breakRow_ = c.loopRow; jumpOrder_ = orderPos_; }
                        break;
                    case 0xA: c.volume = std::min(64, c.volume + y); break;          // fine volume up
                    case 0xB: c.volume = std::max(0, c.volume - y); break;           // fine volume down
                    case 0xC: c.cutTick = y; break;                                  // note cut
                    case 0xE: patternDelay_ = y; break;                              // pattern delay
                }
                break;
        }
        if (c.period > 0 && c.period < 28) c.period = 28;
        c.arpOffset = 0;
        updateOutputs(c);
    }
}

void ModPlayer::updateOutputs(Channel& c) {
    c.periodOut = c.period;
    c.volOut = c.volume;
}

void ModPlayer::doTick() {
    for (int i = 0; i < 4; i++) {
        Channel& c = ch_[i];
        int x = c.param >> 4, y = c.param & 15;
        if (c.delayTicks == tick_) { triggerNote(c, c.pendingNote, c.pendingSample, 0, 0); c.delayTicks = -1; }
        if (c.cutTick == tick_) c.volume = 0;
        if (tick_ == 0) { updateOutputs(c); continue; }
        auto volSlide = [&]() { c.volume = std::max(0, std::min(64, c.volume + (x ? x : -y))); };
        auto tonePorta = [&]() {
            if (!c.target) return;
            if (c.period < c.target) { c.period += c.portaSpeed; if (c.period > c.target) c.period = c.target; }
            else if (c.period > c.target) { c.period -= c.portaSpeed; if (c.period < c.target) c.period = c.target; }
        };
        auto vibrato = [&]() {
            int delta = kSine[c.vibPos & 31] * c.vibDepth / 128;
            c.periodOut = c.period + ((c.vibPos & 32) ? -delta : delta);
            c.vibPos = (c.vibPos + c.vibSpeed) & 63;
        };
        c.periodOut = c.period;
        c.volOut = c.volume;
        switch (c.effect) {
            case 0x0:
                if (c.param) {                                                       // arpeggio
                    int k = tick_ % 3, semis = k == 0 ? 0 : (k == 1 ? x : y);
                    c.periodOut = (int)lround(c.period * pow(2.0, -semis / 12.0));
                }
                break;
            case 0x1: c.period = std::max(113, c.period - c.param); c.periodOut = c.period; break;
            case 0x2: c.period = std::min(856 * 2, c.period + c.param); c.periodOut = c.period; break;
            case 0x3: tonePorta(); c.periodOut = c.period; break;
            case 0x4: vibrato(); break;
            case 0x5: tonePorta(); volSlide(); c.periodOut = c.period; c.volOut = c.volume; break;
            case 0x6: vibrato(); volSlide(); c.volOut = c.volume; break;
            case 0x7: {
                int delta = kSine[c.tremPos & 31] * c.tremDepth / 64;
                c.volOut = std::max(0, std::min(64, c.volume + ((c.tremPos & 32) ? -delta : delta)));
                c.tremPos = (c.tremPos + c.tremSpeed) & 63;
                break;
            }
            case 0xA: volSlide(); c.volOut = c.volume; break;
            case 0xE:
                if (x == 0x9 && y && tick_ % y == 0) c.pos = 0;                      // retrigger
                break;
        }
    }
}

void ModPlayer::render(short* out, int frames) {
    if (!loaded_) { for (int i = 0; i < frames * 2; i++) out[i] = 0; return; }
    for (int f = 0; f < frames; f++) {
        if (tickLeft_ <= 0) {
            if (tick_ == 0) startRow();
            doTick();
            tickLeft_ += tickFrames_;
            if (++tick_ >= speed_) {
                tick_ = 0;
                if (patternDelay_ > 0) { patternDelay_--; }
                else if (jumpOrder_ >= 0 || breakRow_ >= 0) {
                    orderPos_ = jumpOrder_ >= 0 ? jumpOrder_ : orderPos_ + 1;
                    row_ = breakRow_ >= 0 ? breakRow_ : 0;
                    jumpOrder_ = breakRow_ = -1;
                } else if (++row_ >= 64) { row_ = 0; orderPos_++; }
                if (orderPos_ >= songLen_) orderPos_ = 0;        // the end: round again
            }
        }
        tickLeft_ -= 1;
        float l = 0, r = 0;
        for (int i = 0; i < 4; i++) {
            Channel& c = ch_[i];
            if (!c.active || c.sample < 0 || c.periodOut <= 0) continue;
            const Sample& s = samples_[c.sample];
            int len = (int)s.data.size();
            if (len == 0) continue;
            bool loops = s.loopLen > 2;
            if (c.pos >= len) {
                if (!loops) { c.active = false; continue; }
                c.pos = s.loopStart + fmod(c.pos - s.loopStart, (double)s.loopLen);
            }
            int i0 = (int)c.pos;
            double fr = c.pos - i0;
            int i1 = i0 + 1;
            if (i1 >= len) i1 = loops ? s.loopStart : i0;
            float v = (float)(s.data[i0] * (1 - fr) + s.data[i1] * fr) * c.volOut / 64.0f;
            c.pos += kPalClock / c.periodOut / kRate;
            if (loops && c.pos >= s.loopStart + s.loopLen) c.pos -= s.loopLen;
            // Amiga layout -- channels 1 and 4 left, 2 and 3 right -- softened for headphones.
            bool left = (i == 0 || i == 3);
            l += v * (left ? 0.75f : 0.25f);
            r += v * (left ? 0.25f : 0.75f);
        }
        out[f * 2] = (short)std::max(-32767.0f, std::min(32767.0f, l * 0.5f * 32767));
        out[f * 2 + 1] = (short)std::max(-32767.0f, std::min(32767.0f, r * 0.5f * 32767));
    }
}
