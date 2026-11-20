#pragma once
#include <string>
#include <vector>

// Sound effects are synthesised at startup (no sound files to ship); the music is a tracker
// module streamed from assets/music/. Nothing plays until init() -- the scripted test runs stay silent.
namespace Audio {

enum Sfx { kStep, kJump, kTongueOut, kStick, kRelease, kSwallow, kFruit, kCrash, kThud, kDeath, kCough, kHat, kSfxCount };

void init(const std::string& musicPath);
void shutdown();
// Once per frame. The in-game exit prompt fades the current track to silence while it keeps streaming.
void update(bool quietForPrompt, float fadeOut, float gap, float fadeIn);
// Queue a track change for the next fade. An empty path requests silence.
void changeMusic(const std::string& path);
// Seconds into `path`, if it is the music playing now (an MP3, looping: from its start again each
// time round) -- else -1 (another, none, music off, no audio).
float musicTime(const std::string& path);

void play(Sfx s, float pitch = 1.0f, float volume = 1.0f);
void setSoundOn(bool on);
void setSoundVolume(float v);
void setMusicOn(bool on);
void setMusicVolume(float v);

// Tests: writes every effect to `dir` as a WAV, prints its length and peak, and checks that each
// music loads and loops -- without playing anything.
void check(const std::vector<std::string>& musicPaths, const std::string& dir);

}  // namespace Audio
