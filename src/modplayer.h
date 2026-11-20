#pragma once
#include <string>
#include <vector>

// A small ProTracker .mod player (4 channels, 31 samples, the standard effects), rendering
// 16-bit stereo. raylib's own loader hangs on modules that loop back on themselves -- it tries
// to measure the song's length first -- so the game plays them with this instead. It never ends:
// at the end of the order list, or on a position jump, it simply carries on.
class ModPlayer {
public:
    bool load(const std::string& path);
    void render(short* out, int frames);       // interleaved stereo
    int rate() const { return kRate; }

private:
    static const int kRate = 44100;
    struct Sample { std::vector<float> data; int loopStart = 0, loopLen = 0; int volume = 64; int finetune = 0; };
    struct Channel {
        int sample = -1;
        double pos = 0;
        int period = 0, basePeriod = 0, target = 0;
        int volume = 0;
        int effect = 0, param = 0;
        int portaSpeed = 0, vibSpeed = 0, vibDepth = 0, vibPos = 0;
        int tremSpeed = 0, tremDepth = 0, tremPos = 0;
        int arpOffset = 0, lastOffset = 0;
        int volOut = 0, periodOut = 0;
        bool active = false;
        int loopRow = 0, loopCount = 0;
        int delayTicks = -1, cutTick = -1;
        int pendingNote = 0, pendingSample = 0;
    };
    Sample samples_[31];
    std::vector<unsigned char> patterns_;
    int order_[128] = {};
    int songLen_ = 1;
    Channel ch_[4];
    int speed_ = 6, bpm_ = 125, tick_ = 0, row_ = 0, orderPos_ = 0;
    int jumpOrder_ = -1, breakRow_ = -1, patternDelay_ = 0;
    double tickFrames_ = 0, tickLeft_ = 0;
    bool loaded_ = false;

    void startRow();
    void doTick();
    void triggerNote(Channel& c, int note, int smp, int effect, int param);
    void updateOutputs(Channel& c);
};
