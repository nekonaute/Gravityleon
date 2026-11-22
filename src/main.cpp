// GRAVITY LEON -- a cube with legs in a platform world.
//
// The spec (workshop/specs 1.jpg, specs 2.jpg):
//   * Leon runs with Left/Right or the left stick. Acceleration is non-linear, and the top
//     speed is v_max = G(v_cmd) * v_cmd with a gain G that is low for slow commands and high
//     for fast ones: a small push walks him precisely, a full push sends him flying.
//   * He jumps with Up (or Space) or pad A, and steers his fall with Left/Right.
//   * He sits at the centre of the screen. When he moves, the world scrolls to bring him back,
//     WITH A DELAY -- yet he never leaves a central zone covering half of the screen's area.
//   * A long fall ends in a violent landing; then the camera recentres.

#ifdef __EMSCRIPTEN__
#include "../web/platform.h"
#endif
#include "audio.h"
#include "input.h"
#include "padcheck.h"
#include "params.h"
#include "perf.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <map>
#include <sstream>
#include <new>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------------------------
// Palette: ink on paper, like the drawings in workshop/.
// ---------------------------------------------------------------------------------------------
static const Color kPaper     = {244, 240, 230, 255};
static const Color kGrid      = {170, 196, 222, 70};
static const Color kGridMajor = {150, 180, 212, 110};
static const Color kInk       = {38, 38, 48, 255};
static const Color kSlab      = {214, 208, 194, 255};
static const Color kTitleGround = {234, 231, 224, 255};   // the ground under the title: the slab twice as light, plain
static const Color kBody      = {253, 252, 248, 255};
static const Color kStar      = {244, 184, 52, 255};
static const Color kGravity   = {22, 22, 28, 255};      // gravity platforms: mostly black...
static const Color kArrow     = {236, 231, 218, 235};   // ...paved with small light arrows
static const Color kIceGravity = {233, 233, 227, 255};  // ice gravity platforms: the same inverted, white...
static const Color kIceArrow  = {19, 24, 37, 235};      // ...paved with dark arrows
static const Color kTongue    = {232, 96, 128, 255};
static const Color kMouth     = {92, 30, 44, 255};
static const Color kBlood     = {186, 22, 34, 255};     // the Monstermunch's munches: blood cubes
static const Color kRetina    = {246, 202, 40, 255};    // ...its eyes' yellow retinas...
static const Color kHeart     = {214, 44, 58, 255};     // ...the red heart (or diamond) they sit in (workshop/monstermunch-love.jpg)...
static const Color kSuitBlack = {30, 28, 36, 255};      // ...or the black spade (or club)
static const Color kSpit      = {176, 214, 236, 230};
static const Color kWing      = {120, 150, 190, 120};
static const Color kIce       = {204, 230, 242, 255};   // slippery platforms
static const Color kIceShine  = {250, 253, 255, 220};
static const Color kCrate     = {233, 222, 196, 255};
static const Color kBubble    = {214, 236, 248, 140};
static const Color kPine      = {236, 172, 58, 255};    // pineapple (checkpoint)
static const Color kLeaf      = {98, 166, 78, 255};
static const Color kBlack     = {8, 6, 12, 255};
static const Color kGold      = {240, 188, 40, 255};    // a golden fly...
static const Color kGoldWing  = {255, 244, 190, 200};
static const Color kGoldGlow  = {255, 226, 110, 120};   // ...and its glow
static const Color kGuide     = {255, 204, 64, 255};    // the guide arrow
static const Color kFlagRed   = {214, 44, 44, 255};     // the red flag (the keys and their doors: key_color_*)...
static const Color kFlagGreen = {40, 156, 64, 255};     // ...the green ones
static const Color kKeyBlue   = {52, 100, 214, 255};    // ...the blue ones
static const Color kFlagViolet = {140, 66, 200, 255};    // the violet flag: the start of cameleon mode
static const Color kFlagYellow = {232, 186, 30, 255};    // the yellow flag: the end of easy mode
static const Color kGoldKey  = {244, 166, 28, 255};     // the golden key and its doors (yellow, going orange): key_color_golden's default
// The flags, in the order of the modes they end -- yellow easy, red hard, green dark, blue harder --
// and violet, which starts cameleon mode: their letters in a level, their colours.
enum FlagId { flYellow, flRed, flGreen, flBlue, flViolet, flCount };
static const char kFlagLetters[flCount + 1] = "YRGJV";
static const Color kFlagCols[flCount] = {kFlagYellow, kFlagRed, kFlagGreen, kKeyBlue, kFlagViolet};
// The music: the level's, then from the red flag up, then from the blue flag on -- then none, from
// the violet flag up (it fades out: the way outside in silence) -- and the golden doors', the left
// one's (the Love door: the love room) and the right one's (the Solo door: the solo room), once open.
static const char* const kMusic[6] = {"assets/music/03 - tno - Tartopom.mod", "assets/music/08 - tno - World Of Pistou II.mod",
                                      "assets/music/18 - tno - Dying Respect.mod", "",
                                      "assets/music/niko - palindrome au piano.MP3", "assets/music/nocomp - Power Plant (Ultimate Fan Mix).mp3"};
static const float kKeyLen = 36.0f, kKeyH = 16.0f;      // the key: its length, its height (the bow's)
// The engraving (look = engraving): ink on old paper; the world in half-tones, which the
// engraving shader cuts into lines.
static const Color kEngPaper  = {244, 240, 231, 255};
static const Color kEngInk    = {19, 17, 16, 255};
static const Color kEngPlate  = {168, 162, 150, 255};   // iron plates: a half-tone, hatched...
static const Color kEngPlate2 = {184, 178, 166, 255};   // ...every other plate a little lighter
static const Color kEngLit    = {228, 223, 212, 255};   // faces in the light
static const Color kEngShade  = {96, 91, 84, 255};      // faces in the shade: cross-hatched
static const Color kEngShade2 = {128, 122, 114, 255};

static const float kTile = 40.0f;
static const float kStep = 1.0f / 120.0f;          // fixed physics step

// The key by the space bar that jumps too: Cmd on a Mac, Ctrl elsewhere.
#if defined(__EMSCRIPTEN__)
static int kJumpKey1 = KEY_LEFT_CONTROL, kJumpKey2 = KEY_RIGHT_CONTROL;
static const char* kJumpKeyName = "Ctrl";
#elif defined(__APPLE__)
static const int kJumpKey1 = KEY_LEFT_SUPER, kJumpKey2 = KEY_RIGHT_SUPER;
static const char* kJumpKeyName = "Cmd";
#else
static const int kJumpKey1 = KEY_LEFT_CONTROL, kJumpKey2 = KEY_RIGHT_CONTROL;
static const char* kJumpKeyName = "Ctrl";
#endif

// Leon's hitbox, origin at the middle of his feet.
static const float kLeonW = 30.0f, kLeonH = 38.0f;
static const float kAnnaRX = 20.5f, kAnnaMid = 26.0f, kAnnaBrim = 40.5f;   // Anna as first drawn (the ending's sketch): her body's half width, its middle over her feet, her hat's brim over them
static const float kLeonDrawH = 40.0f;   // drawn height, feet to the top of the body
static const float kBodyW = 36.0f, kBodyH = 27.0f;   // the cube itself
static const float kSlabH = 12.0f;      // thickness of a '=' platform
static const float kBodyRound = 0.28f;  // DrawRectangleRounded roundness of the cube
static const float kOutline = 3.0f;     // ink line around the cube
static const float kRollR = 17.0f;      // rolling radius of the knocked-out cube
// The hat: its crown's height, whole; where its brim sits, above his hitbox's top (the drawn body
// is 2 px taller, then the outline); how high the brim's curled ends rise.
static const float kHatCrown = 22.0f, kHatSeat = 5.75f, kBrimRise = 9.8f;
// Anna playing (Game::anna): her round body in the cube's place (kBodyW x kBodyH, his hitbox), her
// face and hat as on the tower's top at kAnnaPlayS of their size -- her cone kAnnaCone tall, its brim
// kAnnaSeat above the hitbox's top, its curls rising kAnnaCurlRise; her brim reaching kAnnaBrimReach
// from her middle, curls and all (anna_brim_size 1: its 29 px, its curls' 11 round, scaled).
static const float kAnnaPlayS = kBodyW / (2 * kAnnaRX + 3);
static const float kAnnaCone = 52 * kAnnaPlayS;
static const float kAnnaSeat = (kAnnaBrim - kAnnaMid) * kAnnaPlayS - (kBodyH / 2 - 2);   // (her body's middle: kBodyH / 2 - 2 under the hitbox's top)
static const float kAnnaCurlRise = 13 * kAnnaPlayS, kAnnaBrimReach = 45 * kAnnaPlayS;
// A loose hat caught on his tongue: put back on his head when the tongue, coming back, is this
// close (pixels) -- curling up over it in this long (seconds).
static const float kHatPutFrom = 70.0f, kHatPutT = 0.35f;
// A loose hat, as a ball this wide round its middle: 11 px down to its brim, the brim's half thickness
// (1.25), and the ink (1.5) drawn over the ground's edge -- so it lies on the ground's line, not in it.
static const float kLooseHatR = 11 + 1.25f + 1.5f;
// His tongue as drawn (out of his mouth): half its width, ink and all.
static const float kTongueHalfW = 3.7f;
// Stuck when a Monstermunch caught him, his tongue holds against its first munches -- this many --
// before it gives way.
static const int kTongueHolds = 2;

// printf into a std::string.
static void appendf(std::string& out, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    out += buf;
}

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float smooth01(float u) { u = clampf(u, 0, 1); return u * u * (3 - 2 * u); }   // (0 to 1, easing in and out)
static float signf(float v) { return (v > 0) - (v < 0); }

// The game's area in the window: the largest rectangle of screen_ratio that fits in it, centred,
// black bands round it -- never stretched. Set each frame (fitGame); everything is laid out in it as
// if it were the screen: gameW() x gameH(), its top left (0, 0) (gameViewport).
static int gGameX = 0, gGameY = 0, gGameW = 1280, gGameH = 720;
static int gameW() { return gGameW; }
static int gameH() { return gGameH; }
static void fitGame(float ratio) {                       // (ratio <= 0: the whole window, whatever its shape)
    int ww = std::max(1, GetScreenWidth()), wh = std::max(1, GetScreenHeight());
    if (ratio <= 0) { gGameX = gGameY = 0; gGameW = ww; gGameH = wh; return; }
    if ((float)ww / wh > ratio) { gGameH = wh; gGameW = std::max(1, (int)lroundf(wh * ratio)); }
    else { gGameW = ww; gGameH = std::max(1, (int)lroundf(ww / ratio)); }
    gGameX = (ww - gGameW) / 2; gGameY = (wh - gGameH) / 2;
}
// Drawing into the game's area: the viewport there, (0, 0) - (gameW, gameH) its corners. After
// BeginDrawing -- and after each EndTextureMode, which puts the whole window back.
static void gameViewport() {
    rlDrawRenderBatchActive();
    float sx = GetRenderWidth() / (float)std::max(1, GetScreenWidth()), sy = GetRenderHeight() / (float)std::max(1, GetScreenHeight());
    rlViewport((int)lroundf(gGameX * sx), (int)lroundf(GetRenderHeight() - (gGameY + gGameH) * sy), (int)lroundf(gGameW * sx), (int)lroundf(gGameH * sy));
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, gGameW, gGameH, 0, 0.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}
// The sketch look's far-layer patterns, by name (background_pattern_*; drawBgPattern).
static const char* kBgNames[] = {"none", "karakusa", "yagasuri", "asanoha", "seigaiha", "sayagata", "ichimatsu"};
static const int kBgCount = (int)(sizeof kBgNames / sizeof kBgNames[0]);
static int bgPatternNamed(std::string v) {                   // (unknown: none)
    for (char& c : v) c = (char)tolower((unsigned char)c);
    if (v == "karukasa") v = "karakusa";
    for (int k = 0; k < kBgCount; k++) if (v == kBgNames[k]) return k;
    return 0;
}

// A picture of the screen, cut to the game's area (no bands).
static Image gameShot() {
    Image shot = LoadImageFromScreen();
    float sx = shot.width / (float)std::max(1, GetScreenWidth()), sy = shot.height / (float)std::max(1, GetScreenHeight());
    if (gGameW < GetScreenWidth() || gGameH < GetScreenHeight())
        ImageCrop(&shot, {gGameX * sx, gGameY * sy, gGameW * sx, gGameH * sy});
    return shot;
}
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
// 1D Perlin noise: smooth, never repeating, about -1..1 -- a random slope at each whole x (from a
// hash), blended with the quintic fade.
static float perlin1(float x) {
    auto slope = [](int i) {
        unsigned h = (unsigned)i * 2654435761u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return (h & 0xffff) / 32767.5f - 1.0f;
    };
    int i = (int)floorf(x);
    float f = x - i, u = f * f * f * (f * (f * 6 - 15) + 10);
    return 2 * lerpf(slope(i) * f, slope(i + 1) * (f - 1), u);
}
// Quarter turns, clockwise on screen (y points down): n = 1 maps (1,0) to (0,1).
static Vector2 rotQ(int n, Vector2 v) {
    switch (((n % 4) + 4) % 4) {
        case 1: return {-v.y, v.x};
        case 2: return {-v.x, -v.y};
        case 3: return {v.y, -v.x};
        default: return v;
    }
}

// The turn's speed is a bell curve: it builds up, peaks halfway, and dies away into the
// target. Its integral, normalised to run 0..1: an error function. sigma is a fraction of the turn.
static float bellEase(float u, float sigma) {
    u = u < 0 ? 0 : (u > 1 ? 1 : u);
    float k = 1.0f / (sigma * 1.41421356f);
    float b = erff(0.5f * k);
    return (erff((u - 0.5f) * k) + b) / (2 * b);
}

// 0 -> 1 as u goes 0 -> 1, speeding up steadily over its first `e`, at a steady speed, then slowing
// down as steadily over its last `e` (a trapezoid of speed).
static float easeInOutLinear(float u, float e) {
    u = u < 0 ? 0 : (u > 1 ? 1 : u);
    e = e < 0 ? 0 : (e > 0.5f ? 0.5f : e);
    if (e <= 0) return u;
    float v = 1 / (1 - e);
    if (u < e) return 0.5f * v * u * u / e;
    if (u > 1 - e) return 1 - 0.5f * v * (1 - u) * (1 - u) / e;
    return 0.5f * v * e + v * (u - e);
}

static Color fade(Color c, float a) { c.a = (unsigned char)(c.a * clampf(a, 0, 1)); return c; }

// A mode by its name, as params.txt gives it ("DARK MODE", "dark", ...): 0 easy, 1 hard, 2 dark,
// 3 harder, 4 cameleon -- -1 none of them.
static int modeNamed(std::string s) {
    for (auto& ch : s) ch = (char)toupper((unsigned char)ch);
    if (s.size() > 5 && s.compare(s.size() - 5, 5, " MODE") == 0) s.resize(s.size() - 5);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    static const char* names[5] = {"EASY", "HARD", "DARK", "HARDER", "CAMELEON"};
    for (int k = 0; k < 5; k++) if (s == names[k]) return k;
    return -1;
}

// ---------------------------------------------------------------------------------------------
// Tuning, re-read from params.txt whenever it changes.
// ---------------------------------------------------------------------------------------------
struct Tune {
    float runSpeed, gainMin, gainExp, accelFloor, accelK, decelFloor, decelK, turnAccel;
    float keyStart, keyRamp, airControl, airDrag;
    float gravity, fallMult, jumpSpeed, jumpCutMult, maxFall, coyote, jumpBuffer, dropTime;
    bool doubleJump, dropThrough;
    float doubleJumpSpeed, doubleJumpSpin;
    float camArea, camDelayX, camDelayY;
    int zoneShow;                             // camera_zone_show: the camera zone drawn -- 0 never, 1 always, 2 in debug mode
    std::string cheatCode;                    // cheat_code: typed, cheat mode (lower case, letters and digits only)
    float hardLand, shakePx, shakeRot, shakeDecay, squashMax, stunTime, stunControl;
    float crashMin, knockBounces, knockBounce, knockBounceMin, knockBounceFriction, knockRollFriction;
    float knockDown, knockRise, knockControl, knockStars, wakeBlinkT, wakeBlinks;
    float turnTime, turnSigma, playerTurnDelay, lookPos;
    float leanRun, leanInertia, leanBrake, leanStiffness, leanDamping, edgeSlip, iceWobble, iceWobbleRate;
    float pedalAfter, pedalRate;
    float slipGrip, slipDrag, slipBubbles;
    float crateSize, crateFriction, crateSlipFriction, pullSpeed, pushSpeed, pullLean, crateNudge, crateRollMin;
    float deathWorldFade, deathPiecesWait, deathPiecesFade, deathFadeIn, checkpointShow;
    float lessonShow, lessonIn, lessonOut;
    float piecesSpread, piecesSpeed;
    float gravArrowSpeed, gravArrowBorder;    // gravity_arrow_*: its arrows' stream -- speed, fading margin (pixels)
    float gravRound, knockFall, lookFarMargin, goalTextDist, homeDist, guidePass;
    float goalWave, goalWavePeriod;           // "... COMPLETED !": its letters' wave, in font sizes up and down, and its period
    float keyFreq, keyAmp, doorGold, doorFade, darkDepth, darkPieces, lampFollow, lampBelly, lampLight, lampFlicker, lampFlickerFreq, lampPostLight,
          lampBellyLight, lampBellyFlicker, lampBellyFlickerFreq;
    Color colorOf[3];                         // color_1 .. color_3: the colour poles' ('1', '2', '3')
    Color leonSolo, leonLove;                 // leon_color_solo, leon_color_love: Leon's own colour through the Solo door, the Love door
    float leonColorTime;                      // leon_color_change_time: ...changing to it over this many seconds
    Color keyCol[5];                          // key_color_red, _blue, _green, _violet, _golden: the keys' colours, their doors' too
    // The ending (Game::endT): anna_color, ending_*.
    Color annaCol, endHeartCol;
    float annaBrim;                           // anna_brim_size: her hat's brim, curls and all (1: 29 px each side, curls 11 px round)
    float endWallH, endSmileWait, endSmileTime, endHeartRate, endHeartSize, endHeartSpeed, endHeartTurn, endHeartRise,
          endHeartFadeFrom, endHeartFadeTo, endHappyWait, endSwayAmp, endSwayFreq, endBrimUncurl, endBrimCurl, endBrimPause, endBrimEase,
          endPanWait, endPanTime, endPanLeft, endCreditsSpeed, endCreditsFast, endCreditsFastRamp, endCreditsWait,
          endScrollSpeed, endScrollRamp, endFadeTime, endGlowDim, endGlowPeriod;
    bool endBrimSync;                         // ending_brim_music_sync: the brims' moves timed by the music, from ending_brim_music_beat
    float endBrimBeat;
    float balloonGrow, balloonSize, balloonPale, gasReach, gasSpread, gasTime, gasPale, gasGravity, bellyTime, colorHide, colorHideHat, colorHidePlain, colorHidePole, colorHideBalloon;
    float balloonSwingAmp, balloonSwingFreq, balloonDashTime, balloonDashW, balloonRegrow, gasOpacity, gasFade, gasBoom, gasBrownian, colorFollow;
    bool balloonSync;                         // a room's balloons in turn: each bursting as the last one's cloud starts to clear
    std::map<int, int> balloonRespawnFirst;   // color_balloon_respawn_first: back at this checkpoint (map order), this colour's balloons first (0 cyan, 1 magenta, 2 gold)
    float gasHideTime;                        // a cloud clearing: it hides him from the Monstermunchs this many seconds more (-1: till gone)
    int balloonDashes;
    bool gasContour;
    float suicideTime, suicideTremble, suicideForce;
    float lampBlackMin, lampBlackMax, lampBlackTime;   // a lamp post's dim light: blackouts a second (from, to), each this long...
    float lampBlackLevel;                              // ...down to this fraction of it
    float darkLightRadius, darkLightDepth;             // a dark light ('@'): how far its darkness reaches (tiles), how black at its heart...
    float darkLightFade;                               // ...and Leon, reaching it, fading out in this many seconds -- gone
    float goldenFadeOut, goldenFadeIn, goldenLeonIn, goldenLightIn, goldenLightOut, goldenLightUp;   // the golden run's way in (golden_*)
    float goldenWait, goldenClocksOut, goldenClockIn;  // ...landed, the wait before 3, 2, 1; the clocks out, its own in
    float goldDoorName;                       // golden_door_name_distance: so many tiles from a golden door, its name starts to show
    float goldDoorFade;                       // golden_door_name_fade: ...fading in over so many tiles nearer
    float doorZoom, doorWaveTime, doorWavePause, doorWaveWidth;
    float doorNameOut;                        // golden_door_name_fade_out: a golden door open, both names gone in so many seconds   // ...its letters' wave: the peak size, the crossing, the rest, the bump's width (letters)
    float arrowWait, arrowFadeIn, arrowFadeOut, arrowSize, arrowPulse, arrowPeriod;
    float goldDelay, goldMaxSpeed;
    float hatSquashT, hatHoldT, hatUnfoldT, hatLow, hatShutT, hatBlinkT, hatBlinks;
    float brimStrainT, brimStrain, brimRingT, brimRing, brimRingFreq;
    float brimFlapFreq, brimFlap;             // pedalling in a long fall: its brim's ends flapping in the air, so many times a second, by so much of their curl
    bool hatRespawn;                          // knocked off as he was eaten: on his head again as he comes back (else it lies where it fell)
    float clockLimit, clockWarnT, clockWarnBlinks, clockAlarmT, clockAlarmBlinks;   // a clock's hour: then he explodes; before it, red and blinking
    float titleStartFade;
    // Anna playing (anna_*): her hat folded this low (of its height), its bands popping back one by one,
    // so many seconds apart, each in so many; her dash (LB / RB, Q / E) so many tiles in so many seconds,
    // blurring in and out over so many, its blur so long (pixels), her eyes so many times as big; again
    // after so many seconds, and so many in the air before she stands again.
    float annaHatLow, annaHatPopDelay, annaHatPopT;
    float annaMouthScale, annaMouthHeight;    // her mouth open: her body this much of its size; its line this high up it (from the bottom, of its height)
    float dashDist, dashTime, dashBlurIn, dashBlurOut, dashBlurLen, dashEyes, dashCool;
    float dashWindup, dashRecover;            // ...wound up so long before it; stopping so long after it (workshop/Anna dash.jpg)
    int dashAir;
    // The title with Anna in it: each character's highscores shown so long, scrolling away in so long;
    // the debug switch between the two, out and in again over so long; ANNA's rainbow, so fast.
    float titleScoresShow, titleScoresScroll, titleSwitchFade, titleAnnaRainbow;
    float titleNameShow, titleNameMorph;      // the two finished: LEON and ANNA in turn, each shown so long, morphing into the other in so long
    float titleIntro, titleIntroRest;         // the title from black: it and what lies behind it, then the rest of it
    float titleScoresEarly;                   // ...the highscores this many seconds before the rest (not before the title)
    float goldKeyGlow;                        // the golden key's halo: seconds a breath (swelling and shrinking back)
    float escFade;                            // ESC's question: the screen fades to black (and back) over this many seconds
    bool titleHelp;                           // the help lines under the title (and a few seconds after)
    float fullscreenWait;                     // Shift+F: no new toggle before the last one has settled
    bool startKey, startBlueKey, startGreenKey, startHat, startTongue, startDouble;   // Leon starts with the red, blue, green key, the hat, the tongue, the double jump (debug)
    int startPart;                            // start_level: the mode the run starts in, at its key (0 easy: at the start)
    // The Monstermunch: how near (screen widths) it wakes, and throws its tongue -- within this many
    // degrees of straight up; the tongue's speed, and the drag into its mouth; the rest after a miss;
    // the munches, in so many seconds, the blood cubes at each; the chewing after, and the fade out.
    float mmWake, mmReach, mmAngle, mmTongueSpeed, mmPullSpeed, mmRest, mmStandT;
    float mmMunches, mmMunchT, mmBlood, mmChewT, mmFadeOut;
    bool mmFreeFlies;                         // eaten: the flies in his belly go down with him, out after the last munch...
    float mmFreeDelay;                        // ...this many seconds after it...
    float mmFreedSpeed, mmFreedNoise;         // ...and home as an active Brownian particle: its speed (of a golden fly's top), its heading's diffusion
    float mmColorFade;                        // a colour fly out: the Monstermunch's white back in this many seconds
    float mmSwallow;                          // ...a missed throw's numb tongue, swallowed back this fast (pixels a second)
    float mmChewRate, mmChewJitter, mmNumbChewT;   // ...in chews: so many a second, each wait give or take this (s), each taking its share in this long
    float mmDroolDist, mmDroolOpen, mmDroolRate;   // Leon this near (screen widths), not to be had: its mouth this open, so many drops a second
    // ...its heart, and its eyes in it: the heart's size and the eyes' (1: as drawn); the yellow
    // retinas' radius (tiles), the black pupils in them (of that radius); the twitches of its left eye
    // and its right eye (a Poisson process each: so many a second on average, so far -- in open-eye
    // heights); its blinks (so many a second on average, each wait give or take this fraction of the
    // average, each this long).
    float mmHeartScale, mmEyeScale, mmRetinaR, mmPupil, mmSpasmF[2], mmSpasmA[2];
    float mmBlinkF, mmBlinkNoise, mmBlinkT;
    std::vector<int> mmSuits;                 // each Monstermunch's motif, in map order (round again): 0 heart, 1 spade, 2 club, 3 diamond
    bool barDecor;
    bool musicOn, soundOn;
    bool engrave;                             // look = engraving: the engraving (else the sketch)
    float engraveSpacing, engraveSkySpacing, engraveWind;
    bool engraveDrawings;                     // its clouds: the workshop's drawings (assets/clouds) -- else the former engraved ones
    float pressWaveAmp, pressWavePeriod, pressBlinkPeriod, pressSize, pressAlphaMin, pressAlphaMax;
    float scoresRainbow;                      // the title's highscore tables: a rainbow rolling down them, this many times a second
    float musicVolume, soundVolume;
    float musicFadeOut, musicGap, musicFadeIn;  // a change of music: the old one out, a silence, the new one in
    float tongueLen, tongueSend, tongueBack, tongueQuick, tonguePump, tongueDamp, tongueMaxSwing, tongueSag;
    float tongueStretch, chewTime, tongueAimGrace, dripEvery;
    float tongueGive, tongueGiveSpring, tongueFastStretch;
    bool spit;
    float flyCount, flySpeed, flyJitter;
    float playerFrac, zoom, deadzone;
    float screenRatio;                        // screen_ratio: the game's shape (width / height; 0: the window's own)
    int bgPattern[5];                         // background_pattern_<mode>: the sketch's far layer, each mode's (kBgNames)
    int bgGolden;                             // background_pattern_golden: from a golden key's first taking on, whatever the mode
    float bgScale, bgParallax, bgOpacity, bgFade;   // ...its size, its share of the world's moves, its ink, the crossfade (seconds)

    void read(const Params& p) {
        runSpeed    = p.f("run_speed", 340);
        gainMin     = p.f("run_gain_min", 0.3f);
        gainExp     = p.f("run_gain_exp", 2.0f);
        accelFloor  = p.f("run_accel_floor", 500);
        accelK      = p.f("run_accel_k", 6);
        decelFloor  = p.f("run_decel_floor", 900);
        decelK      = p.f("run_decel_k", 10);
        turnAccel   = p.f("run_turn_accel", 3200);
        keyStart    = p.f("key_command_start", 0.3f);
        keyRamp     = p.f("key_command_ramp", 0.35f);
        airControl  = p.f("air_control", 0.6f);
        airDrag     = p.f("air_drag", 0.3f);
        gravity     = p.f("gravity", 2200);
        fallMult    = p.f("fall_gravity_mult", 1.5f);
        jumpSpeed   = p.f("jump_speed", 820);
        jumpCutMult = p.f("jump_release_gravity_mult", 2.6f);
        maxFall     = p.f("max_fall_speed", 1500);
        coyote      = p.f("coyote_time", 0.08f);
        jumpBuffer  = p.f("jump_buffer", 0.12f);
        dropTime    = p.f("drop_through_time", 0.22f);
        dropThrough     = p.f("drop_through", 0) != 0;
        doubleJump      = p.f("double_jump", 1) != 0;
        doubleJumpSpeed = p.f("double_jump_speed", 720);
        doubleJumpSpin  = fmaxf(0, p.f("double_jump_spin_time", 0.35f));
        camArea     = clampf(p.f("camera_zone_area", 0.5f), 0.01f, 16.0f);   // (above 1: the zone past the screen -- tools/make_icon.py)
        camDelayX   = fmaxf(0.001f, p.f("camera_delay_x", 0.45f));
        camDelayY   = fmaxf(0.001f, p.f("camera_delay_y", 0.55f));
        {   // camera_zone_show: on, off, or debug (shown in debug mode only)
            std::string v = p.s("camera_zone_show", "debug");
            for (char& c : v) c = (char)tolower((unsigned char)c);
            zoneShow = (v == "on" || v == "true" || v == "yes" || v == "1") ? 1 : (v == "off" || v == "false" || v == "no" || v == "0") ? 0 : 2;
        }
        {   // cheat_code: its letters and digits, either case (anything else left out) -- none: SUPERBO
            std::string v = p.s("cheat_code", "superbo");
            cheatCode.clear();
            for (char c : v) if (isalnum((unsigned char)c)) cheatCode += (char)tolower((unsigned char)c);
            if (cheatCode.empty()) cheatCode = "superbo";
        }
        hardLand    = p.f("hard_landing_speed", 950);
        shakePx     = p.f("shake_pixels", 22);
        shakeRot    = p.f("shake_degrees", 1.6f);
        shakeDecay  = p.f("shake_decay", 1.8f);
        squashMax   = p.f("squash_max", 0.45f);
        stunTime    = p.f("hard_landing_stun", 0.22f);
        stunControl = p.f("hard_landing_stun_control", 0.25f);
        crashMin            = p.f("crash_min_speed", 0);
        knockBounces        = p.f("knock_bounces", 3);
        knockBounce         = p.f("knock_bounce", 0.45f);
        knockBounceMin      = p.f("knock_bounce_min_speed", 140);
        knockBounceFriction = p.f("knock_bounce_friction", 0.7f);
        knockRollFriction   = p.f("knock_roll_friction", 3.0f);
        knockDown           = fmaxf(0, p.f("knock_down_time", 0));
        knockRise           = fmaxf(0.01f, p.f("knock_rise_time", 0.5f));
        knockControl        = clampf(p.f("knock_control_after", 0.2f), 0.01f, knockRise);
        knockStars          = fmaxf(0.01f, p.f("knock_stars_time", 0.5f));
        wakeBlinkT          = fmaxf(0.1f, p.f("wake_blink_time", 1.0f));
        wakeBlinks          = fmaxf(0, floorf(p.f("wake_blinks", 2)));
        turnTime        = fmaxf(0.01f, p.f("gravity_turn_time", 0.5f));
        turnSigma       = clampf(p.f("gravity_turn_sigma", 0.15f), 0.03f, 1.0f);
        playerTurnDelay = fmaxf(0, p.f("player_turn_delay", 0.1f));
        tongueLen       = clampf(p.f("tongue_length", 0.625f), 0.05f, 2.0f);
        tongueSend      = fmaxf(0.01f, p.f("tongue_send_time", 0.1f));
        tongueBack      = fmaxf(0.01f, p.f("tongue_back_time", 0.2f));
        tongueQuick     = fmaxf(0.005f, p.f("tongue_quick_back_time", 0.05f));
        tonguePump      = p.f("tongue_swing_pump", 700);
        tongueDamp      = fmaxf(0, p.f("tongue_swing_damping", 0.12f));
        tongueMaxSwing  = p.f("tongue_swing_max_speed", 900);
        tongueSag       = fmaxf(0, p.f("tongue_sag", 1.0f));
        tongueStretch   = fmaxf(0, p.f("tongue_stretch_speed", 30));
        chewTime        = fmaxf(0.01f, p.f("tongue_chew_time", 0.08f));
        tongueAimGrace  = fmaxf(0, p.f("tongue_aim_grace", 0.06f));
        dripEvery       = fmaxf(0.2f, p.f("tongue_drip_interval", 3.0f));
        tongueGive      = fmaxf(0, p.f("tongue_give", 16));
        tongueGiveSpring = fmaxf(0, p.f("tongue_give_spring", 250));
        tongueFastStretch = fmaxf(1, p.f("tongue_fast_stretch", 2));
        spit            = p.b("tongue_spit_effect", false);
        flyCount        = fmaxf(0, p.f("fly_count", 8));
        flySpeed        = p.f("fly_speed", 70);
        flyJitter       = p.f("fly_jitter", 900);
        leanRun         = p.f("lean_running", 6.0f);
        leanInertia     = p.f("lean_inertia", 3.5f);
        leanBrake       = p.f("lean_brake_inertia", 0.6f);
        edgeSlip        = fmaxf(0, p.f("edge_step_off_speed", 120));
        iceWobble       = p.f("slippy_wobble", 6);
        iceWobbleRate   = fmaxf(0.05f, p.f("slippy_wobble_rate", 0.7f));
        pedalAfter      = fmaxf(0, p.f("fall_pedal_after", 3.0f));
        pedalRate       = fmaxf(0.1f, p.f("fall_pedal_rate", 2.0f));
        slipGrip        = clampf(p.f("slippy_grip", 0.06f), 0, 1);
        slipDrag        = clampf(p.f("slippy_drag", 0.03f), 0, 1);
        slipBubbles     = fmaxf(0, p.f("slippy_bubbles", 1.0f));
        crateSize       = clampf(p.f("block_size", 2.0f), 0.5f, 4.0f);
        crateFriction   = fmaxf(0, p.f("block_friction", 10));
        crateSlipFriction = fmaxf(0, p.f("block_slippy_friction", 0.3f));
        pullSpeed       = clampf(p.f("block_pull_speed", 0.5f), 0.05f, 1.0f);
        pushSpeed       = clampf(p.f("block_push_speed", 0.25f), 0.05f, 1.0f);
        crateNudge      = fmaxf(0, p.f("block_knock_shove", 6));
        crateRollMin    = fmaxf(10, p.f("block_roll_min_speed", 220));
        pullLean        = p.f("block_pull_lean", 12);
        deathWorldFade  = fmaxf(0.05f, p.f("death_world_fade", 1.0f));
        deathPiecesWait = fmaxf(0, p.f("death_pieces_wait", 1.0f));
        deathPiecesFade = fmaxf(0.05f, p.f("death_pieces_fade", 1.0f));
        deathFadeIn     = fmaxf(0.01f, p.f("death_fade_in", 0.5f));
        piecesSpread    = fmaxf(0.1f, p.f("death_pieces_spread", 3.0f));
        piecesSpeed     = clampf(p.f("death_pieces_speed", 0.5f), 0.05f, 2.0f);
        checkpointShow  = fmaxf(0.1f, p.f("checkpoint_message_time", 1.6f));
        lessonShow      = fmaxf(0, p.f("lesson_show_time", 4.0f));
        lessonIn        = fmaxf(0.01f, p.f("lesson_fade_in", 0.25f));
        lessonOut       = fmaxf(0.01f, p.f("lesson_fade_out", 0.5f));
        gravRound       = clampf(p.f("gravity_platform_round", 0.05f), 0, 0.5f);
        gravArrowSpeed  = fmaxf(0, p.f("gravity_arrow_speed", 8));
        gravArrowBorder = fmaxf(0, p.f("gravity_arrow_border", 10));
        knockFall       = fmaxf(0.1f, p.f("knock_fall_height", 2.0f));
        lookFarMargin   = clampf(p.f("look_far_margin", 0.5f), 0, 10);
        goalTextDist    = fmaxf(0.1f, p.f("goal_text_distance", 6.0f));
        goalWave        = fmaxf(0, p.f("goal_text_wave_amplitude", 2.0f));
        goalWavePeriod  = fmaxf(0.05f, p.f("goal_text_wave_period", 1.5f));
        homeDist        = fmaxf(0.2f, p.f("hard_mode_home_distance", 1.5f));
        keyFreq         = fmaxf(0, p.f("key_float_frequency", 0.5f));
        keyAmp          = fmaxf(0, p.f("key_float_amplitude", 0.5f));
        doorGold        = fmaxf(0.01f, p.f("door_gold_time", 0.5f));
        doorFade        = fmaxf(0.01f, p.f("door_fade_time", 0.5f));
        darkDepth       = fmaxf(1, p.f("dark_depth", 300));
        darkPieces      = clampf(p.f("dark_pieces_light", 0.25f), 0, 1);
        lampFollow      = fmaxf(0, p.f("lamp_follow_time", 10));
        lampBelly       = fmaxf(0, p.f("lamp_belly_time", 0));
        lampBellyLight  = clampf(p.f("lamp_belly_light", 0.5f), 0, 1);
        lampBellyFlicker     = clampf(p.f("lamp_belly_flicker_amplitude", 0), 0, 1);
        lampBellyFlickerFreq = fmaxf(0, p.f("lamp_belly_flicker_frequency", 5));
        {   // color_1 .. color_3, leon_color_*, key_color_*: "red green blue [alpha]", 0 to 255 each
            auto rgb = [&](const char* name, Color def) {
                int r, gr, b, al = 255;
                std::string v = p.s(name, "");
                return sscanf(v.c_str(), "%d %d %d %d", &r, &gr, &b, &al) >= 3
                    ? Color{(unsigned char)clampf((float)r, 0, 255), (unsigned char)clampf((float)gr, 0, 255), (unsigned char)clampf((float)b, 0, 255),
                            (unsigned char)clampf((float)al, 0, 255)}
                    : def;
            };
            const Color def[3] = {{100, 143, 255, 255}, {220, 38, 127, 255}, {255, 176, 0, 255}};
            for (int k = 0; k < 3; k++) colorOf[k] = rgb(TextFormat("color_%d", k + 1), def[k]);
            leonSolo = rgb("leon_color_solo", {214, 172, 32, 255});
            leonLove = rgb("leon_color_love", {255, 196, 216, 255});
            static const char* keyName[5] = {"red", "blue", "green", "violet", "golden"};
            const Color keyDef[5] = {{176, 68, 52, 255}, {102, 124, 158, 255}, {120, 134, 107, 255}, kFlagViolet, kGoldKey};   // (brick, grey blue, camouflage)
            for (int k = 0; k < 5; k++) keyCol[k] = rgb(TextFormat("key_color_%s", keyName[k]), keyDef[k]);
            annaCol     = rgb("anna_color", {116, 190, 246, 255});
            endHeartCol = rgb("ending_heart_color", {214, 52, 40, 255});
        }
        leonColorTime   = fmaxf(0, p.f("leon_color_change_time", 3));
        annaBrim        = clampf(p.f("anna_brim_size", 0.8f), 0.1f, 3);
        endWallH        = fmaxf(1, p.f("ending_wall_height", 40));
        endGlowDim      = clampf(p.f("ending_glow_dim", 0.2f), 0, 1);
        endGlowPeriod   = fmaxf(0.1f, p.f("ending_glow_period", 2));
        endSmileWait    = fmaxf(0, p.f("ending_smile_wait", 1));
        endSmileTime    = fmaxf(0.01f, p.f("ending_smile_time", 2));
        endHeartRate    = fmaxf(0, p.f("ending_heart_rate", 6));
        endHeartSize    = fmaxf(1, p.f("ending_heart_size", 9));
        endHeartSpeed   = fmaxf(0, p.f("ending_heart_speed", 20));
        endHeartTurn    = fmaxf(0, p.f("ending_heart_turn", 1.5f));
        endHeartRise    = p.f("ending_heart_rise", 25);
        endHeartFadeFrom = fmaxf(0, p.f("ending_heart_fade_from", 2));
        endHeartFadeTo  = fmaxf(endHeartFadeFrom + 0.01f, p.f("ending_heart_fade_to", 3));
        endHappyWait    = fmaxf(0, p.f("ending_happy_wait", 2));
        endSwayAmp      = p.f("ending_sway_amplitude", 12);
        endSwayFreq     = fmaxf(0, p.f("ending_sway_frequency", 0.375f));
        endBrimUncurl   = fmaxf(0.01f, p.f("ending_brim_uncurl_time", 1 / 6.0f));
        endBrimCurl     = fmaxf(0.01f, p.f("ending_brim_curl_time", 0.5f));
        endBrimPause    = fmaxf(0, p.f("ending_brim_pause", 1 / 3.0f));
        endBrimEase     = clampf(p.f("ending_brim_ease", 0.2f), 0, 0.5f);
        endBrimSync     = p.b("ending_brim_music_sync", true);
        endBrimBeat     = p.f("ending_brim_music_beat", 0.975f);
        endPanWait      = fmaxf(0, p.f("ending_pan_wait", 5));
        endPanTime      = fmaxf(0.01f, p.f("ending_pan_time", 2));
        endPanLeft      = p.f("ending_pan_leon_from_left", 3);
        endCreditsSpeed = fmaxf(1, p.f("ending_credits_speed", 40));
        endCreditsFast  = fmaxf(0.1f, p.f("ending_credits_fast", 2));
        endCreditsFastRamp = fmaxf(0, p.f("ending_credits_fast_ramp", 0.5f));
        endCreditsWait  = fmaxf(0, p.f("ending_credits_wait", 10));
        endScrollSpeed  = fmaxf(0.5f, p.f("ending_scroll_speed", 8));
        endScrollRamp   = fmaxf(0, p.f("ending_scroll_ramp", 0.5f));
        endFadeTime     = fmaxf(0.01f, p.f("ending_fade_time", 2));
        balloonGrow     = fmaxf(0.2f, p.f("color_balloon_grow_time", 5));
        balloonSize     = clampf(p.f("color_balloon_size", 0.7f), 0.2f, 2);
        balloonRegrow   = fmaxf(0, p.f("color_balloon_regrow_delay", 0));
        balloonSync     = p.b("color_balloon_sync", false);
        {   // color_balloon_respawn_first: "2:magenta 4:gold" -- checkpoint (its number in map order, as a bug
            // report's checkpoint.pine gives it) : the colour whose balloons grow first when he is back there
            balloonRespawnFirst.clear();
            std::string v = p.s("color_balloon_respawn_first", "");
            for (char& ch : v) if (ch == ',' || ch == ';') ch = ' ';
            std::istringstream in(v);
            std::string item;
            while (in >> item) {
                size_t c = item.find(':');
                if (c == std::string::npos) continue;
                int cp = atoi(item.substr(0, c).c_str());
                std::string col = item.substr(c + 1);
                for (char& ch : col) ch = (char)tolower((unsigned char)ch);
                int k = col == "magenta" || col == "1" ? 1 : col == "gold" || col == "yellow" || col == "2" ? 2 : 0;
                balloonRespawnFirst[cp] = k;
            }
        }
        balloonPale     = clampf(p.f("color_balloon_pale", 0.55f), 0, 1);
        gasReach        = fmaxf(0.5f, p.f("color_gas_reach", 2.4f));
        gasSpread       = fmaxf(0.05f, p.f("color_gas_spread_time", 1.2f));
        gasBoom         = clampf(p.f("color_gas_boom", 1), 1, 20);
        gasBrownian     = fmaxf(0, p.f("color_gas_brownian", 0));
        gasTime         = fmaxf(gasSpread + 0.1f, p.f("color_gas_time", 7));
        gasPale         = clampf(p.f("color_gas_pale", 0.4f), 0, 1);
        gasGravity      = fmaxf(0, p.f("color_gas_gravity", 0.05f));
        gasOpacity      = clampf(p.f("color_gas_opacity", 1), 0, 1);
        gasFade         = clampf(p.f("color_gas_fade", 0.5f), 0.01f, 1);
        gasHideTime     = p.f("color_gas_hide_time", -1);
        gasContour      = p.b("color_gas_contour", true);
        balloonSwingAmp = fmaxf(0, p.f("color_balloon_swing_amplitude", 120));
        balloonSwingFreq = fmaxf(0, p.f("color_balloon_swing_frequency", 6));
        balloonDashTime = fmaxf(0, p.f("color_balloon_dash_time", 0.5f));
        balloonDashes   = std::max(1, (int)p.f("color_balloon_dashes", 12));
        balloonDashW    = clampf(p.f("color_balloon_dash_thickness", 1.6f), 0.5f, 10);
        bellyTime       = fmaxf(0, p.f("color_belly_time", 7));
        colorFollow     = fmaxf(0, p.f("color_follow_time", 0));
        colorHide       = clampf(p.f("color_hide", 0.1f), 0, 1);
        colorHideHat    = clampf(p.f("color_hide_hat", 0.1f), 0, 1);
        colorHidePole   = clampf(p.f("color_hide_pole", 1), 0, 1);
        colorHideBalloon = clampf(p.f("color_hide_balloon", 1), 0, 1);
        colorHidePlain  = clampf(p.f("color_hide_plain", 0.6f), 0, 1);
        lampLight       = fmaxf(0, p.f("lamp_light_radius", 5));
        lampFlicker     = clampf(p.f("lamp_flicker_amplitude", 0.03f), 0, 0.9f);
        lampFlickerFreq = fmaxf(0, p.f("lamp_flicker_frequency", 1.5f));
        lampPostLight   = clampf(p.f("lamp_post_light", 1.0f / 3), 0, 1);
        lampBlackMin    = fmaxf(0.01f, p.f("lamp_post_blackouts_min", 1));
        lampBlackMax    = fmaxf(lampBlackMin, p.f("lamp_post_blackouts_max", 5));
        lampBlackTime   = fmaxf(0, p.f("lamp_post_blackout_time", 0.06f));
        lampBlackLevel  = clampf(p.f("lamp_post_blackout_level", 0.5f), 0, 1);
        darkLightRadius = fmaxf(0, p.f("dark_light_radius", 4));
        darkLightDepth  = clampf(p.f("dark_light_depth", 0.95f), 0, 1);
        darkLightFade   = fmaxf(0.01f, p.f("dark_light_fade", 0.2f));
        goldenFadeOut   = fmaxf(0.01f, p.f("golden_fade_out", 1));
        goldenFadeIn    = fmaxf(0.01f, p.f("golden_fade_in", 1));
        goldenLeonIn    = fmaxf(0.01f, p.f("golden_leon_fade_in", 3));
        goldenLightIn   = fmaxf(0.01f, p.f("golden_light_fade_in", 2));
        goldenLightOut  = fmaxf(0.01f, p.f("golden_light_fade_out", 2));
        goldenLightUp   = fmaxf(0, p.f("golden_light_height", 3));
        goldenWait      = fmaxf(0, p.f("golden_countdown_wait", 1));
        goldenClocksOut = fmaxf(0.01f, p.f("golden_clocks_fade_out", 2));
        goldenClockIn   = fmaxf(0.01f, p.f("golden_clock_fade_in", 1));
        goldDoorName    = fmaxf(0, p.f("golden_door_name_distance", 5));
        goldDoorFade    = fmaxf(0, p.f("golden_door_name_fade", 3));
        doorZoom        = fmaxf(1, p.f("golden_door_name_zoom", 1.5f));
        doorWaveTime    = fmaxf(0.05f, p.f("golden_door_name_wave_time", 1));
        doorWavePause   = fmaxf(0, p.f("golden_door_name_wave_pause", 1));
        doorWaveWidth   = fmaxf(0.1f, p.f("golden_door_name_wave_width", 1));
        doorNameOut     = fmaxf(0, p.f("golden_door_name_fade_out", 2));
        suicideTime     = fmaxf(0.1f, p.f("suicide_time", 1.5f));
        suicideTremble  = fmaxf(0, p.f("suicide_tremble", 5));
        suicideForce    = fmaxf(0, p.f("suicide_force", 2));
        guidePass       = fmaxf(0.2f, p.f("guide_pass_distance", 2.0f));
        arrowWait       = fmaxf(0, p.f("guide_arrow_wait", 3.0f));
        arrowFadeIn     = fmaxf(0.01f, p.f("guide_arrow_fade_in", 1.0f));
        arrowFadeOut    = fmaxf(0.01f, p.f("guide_arrow_fade_out", 0.25f));
        arrowSize       = fmaxf(0.1f, p.f("guide_arrow_size", 1.0f));
        arrowPulse      = clampf(p.f("guide_arrow_pulse", 0.1f), 0, 0.9f);
        arrowPeriod     = fmaxf(0.05f, p.f("guide_arrow_period", 2.0f));
        goldDelay       = fmaxf(0.01f, p.f("golden_fly_delay", 0.3f));
        goldMaxSpeed    = fmaxf(0.01f, p.f("golden_fly_max_speed", 0.5f));
        hatSquashT      = fmaxf(0.01f, p.f("hat_squash_time", 0.1f));
        hatHoldT        = fmaxf(0, p.f("hat_squashed_time", 0.3f));
        hatUnfoldT      = fmaxf(0.01f, p.f("hat_unfold_time", 0.6f));
        hatLow          = clampf(p.f("hat_squashed_height", 0.3f), 0.05f, 1.0f);
        hatShutT        = fmaxf(0, p.f("hat_bump_shut_time", 0.5f));
        hatBlinkT       = fmaxf(0.1f, p.f("hat_bump_blink_time", 0.5f));
        hatBlinks       = fmaxf(0, floorf(p.f("hat_bump_blinks", 2)));
        brimStrainT     = fmaxf(0.01f, p.f("hat_brim_strain_time", 0.5f));
        brimStrain      = clampf(p.f("hat_brim_strain", 0.25f), 0, 1);
        brimRingT       = fmaxf(0.01f, p.f("hat_brim_ring_time", 0.5f));
        brimRing        = fmaxf(0, p.f("hat_brim_ring", 0.12f));
        brimRingFreq    = fmaxf(0.1f, p.f("hat_brim_ring_freq", 8.0f));
        brimFlapFreq    = fmaxf(0, p.f("hat_brim_flap_frequency", 10));
        brimFlap        = fmaxf(0, p.f("hat_brim_flap", 0.4f));
        hatRespawn      = p.b("hat_back_on_respawn", true);
        {   // clock_limit: hours:minutes:seconds (or minutes:seconds, or seconds)
            std::string v = p.s("clock_limit", "1:00:00");
            float sec = 0;
            size_t at = 0;
            while (at <= v.size()) {
                size_t colon = v.find(':', at);
                sec = sec * 60 + (float)atof(v.substr(at, colon == std::string::npos ? std::string::npos : colon - at).c_str());
                if (colon == std::string::npos) break;
                at = colon + 1;
            }
            clockLimit = fmaxf(1, sec);
        }
        clockWarnT      = fmaxf(0, p.f("clock_warning_time", 10));
        clockWarnBlinks = fmaxf(0, p.f("clock_warning_blinks", 2));
        clockAlarmT     = fmaxf(0, p.f("clock_alarm_time", 2));
        clockAlarmBlinks = fmaxf(0, p.f("clock_alarm_blinks", 5));
        titleStartFade  = fmaxf(0.02f, p.f("title_start_fade_time", 1.0f));
        annaHatLow      = clampf(p.f("anna_hat_squashed_height", 0.15f), 0, 1);
        annaHatPopDelay = fmaxf(0, p.f("anna_hat_pop_delay", 0.25f));
        annaHatPopT     = fmaxf(0.01f, p.f("anna_hat_pop_time", 0.1f));
        annaMouthScale  = clampf(p.f("anna_mouth_body_scale", 0.9f), 0.5f, 1.5f);
        annaMouthHeight = clampf(p.f("anna_mouth_height", 0.4f), 0.1f, 0.9f);
        dashDist        = fmaxf(0.1f, p.f("anna_dash_distance", 4));
        dashTime        = fmaxf(0.02f, p.f("anna_dash_time", 0.16f));
        dashBlurIn      = fmaxf(0.001f, p.f("anna_dash_blur_in", 0.05f));
        dashBlurOut     = fmaxf(0.001f, p.f("anna_dash_blur_out", 0.08f));
        dashBlurLen     = fmaxf(0, p.f("anna_dash_blur_length", 30));
        dashEyes        = clampf(p.f("anna_dash_eyes", 0.6f), 0.1f, 1.5f);   // (her eyes, all through a dash: this much of their height, as wide)
        dashCool        = fmaxf(0, p.f("anna_dash_cooldown", 0.25f));
        dashAir         = std::max(0, (int)p.f("anna_dash_air", 1));
        dashWindup      = fmaxf(0, p.f("anna_dash_windup", 0.2f));
        dashRecover     = fmaxf(0.01f, p.f("anna_dash_recover", 0.35f));
        titleScoresShow   = fmaxf(0.5f, p.f("title_scores_switch_time", 6));
        titleScoresScroll = fmaxf(0.05f, p.f("title_scores_scroll_time", 0.5f));
        titleSwitchFade   = fmaxf(0.02f, p.f("title_switch_fade_time", 0.6f));
        titleAnnaRainbow  = p.f("title_anna_rainbow_speed", 0.5f);
        titleNameShow     = fmaxf(0.1f, p.f("title_name_switch_time", 4));
        titleNameMorph    = fmaxf(0.01f, p.f("title_name_morph_time", 0.5f));
        titleIntro      = fmaxf(0, p.f("title_fade_in_time", 0));
        goldKeyGlow     = fmaxf(0.1f, p.f("gold_key_glow_period", 1.6f));
        titleIntroRest  = fmaxf(0, p.f("title_fade_in_rest_time", 0));
        titleScoresEarly = fmaxf(0, p.f("title_scores_fade_in_early", 1));
        escFade         = fmaxf(0.01f, p.f("esc_fade_time", 0.25f));
        titleHelp       = p.b("title_help", false);
        fullscreenWait  = fmaxf(0, p.f("fullscreen_toggle_wait", 0.5f));
        startKey        = p.b("start_with_key", false);
        startBlueKey    = p.b("start_with_blue_key", false);
        startGreenKey   = p.b("start_with_green_key", false);
        startHat        = p.b("start_with_hat", false);
        startTongue     = p.b("start_with_tongue", false);
        startDouble     = p.b("start_with_double_jump", false);
        startPart       = std::max(0, modeNamed(p.s("start_level", "EASY MODE")));
        mmWake          = fmaxf(0, p.f("monstermunch_wake_distance", 1.0f / 3));
        mmReach         = fmaxf(0, p.f("monstermunch_reach", 0.25f));
        mmAngle         = clampf(p.f("monstermunch_throw_angle", 45), 0, 90);
        mmTongueSpeed   = fmaxf(10, p.f("monstermunch_tongue_speed", 2000));
        mmPullSpeed     = fmaxf(10, p.f("monstermunch_pull_speed", 700));
        mmRest          = fmaxf(0, p.f("monstermunch_rest_time", 0.5f));
        mmStandT        = fmaxf(0, p.f("monstermunch_stand_time", 0.5f));
        mmMunches       = fmaxf(1, floorf(p.f("monstermunch_munches", 3)));
        mmFreeFlies     = p.b("monstermunch_frees_flies", false);
        mmFreeDelay     = fmaxf(0, p.f("monstermunch_frees_flies_delay", 0));
        mmFreedSpeed    = clampf(p.f("monstermunch_freed_fly_speed", 1), 0.01f, 1);
        mmColorFade     = fmaxf(0, p.f("monstermunch_color_fade_time", 0));
        mmFreedNoise    = fmaxf(0, p.f("monstermunch_freed_fly_noise", 0));
        mmMunchT        = fmaxf(0.05f, p.f("monstermunch_munch_time", 1));
        mmBlood         = fmaxf(0, floorf(p.f("monstermunch_blood_cubes", 8)));
        mmChewT         = fmaxf(0, p.f("monstermunch_chew_time", 2));
        mmFadeOut       = fmaxf(0.01f, p.f("monstermunch_fade_out", 0.5f));
        mmSwallow       = fmaxf(1, p.f("monstermunch_numb_swallow_speed", 150));
        mmChewRate      = fmaxf(0.1f, p.f("monstermunch_numb_chews_per_second", 3));
        mmChewJitter    = fmaxf(0, p.f("monstermunch_numb_chew_jitter", 0.1f));
        mmNumbChewT     = fmaxf(0.01f, p.f("monstermunch_numb_chew_time", 0.08f));
        mmDroolDist     = fmaxf(0, p.f("monstermunch_drool_distance", 0.2f));
        mmDroolOpen     = clampf(p.f("monstermunch_drool_open", 0.2f), 0, 1);
        mmDroolRate     = fmaxf(0, p.f("monstermunch_drool_rate", 2));
        mmRetinaR       = fmaxf(0, p.f("monstermunch_retina_radius", 0.085f));
        mmPupil         = clampf(p.f("monstermunch_pupil_size", 0.666f), 0, 1);
        mmSpasmF[0]     = fmaxf(0, p.f("monstermunch_left_eye_spasm_frequency", 0.3f));
        mmSpasmA[0]     = fmaxf(0, p.f("monstermunch_left_eye_spasm_amplitude", 0.2f));
        mmSpasmF[1]     = fmaxf(0, p.f("monstermunch_right_eye_spasm_frequency", 0.2f));
        mmSpasmA[1]     = fmaxf(0, p.f("monstermunch_right_eye_spasm_amplitude", 0.2f));
        mmHeartScale    = fmaxf(0.1f, p.f("monstermunch_heart_scale", 1));
        mmEyeScale      = fmaxf(0.1f, p.f("monstermunch_eye_scale", 1));
        mmBlinkF        = fmaxf(0, p.f("monstermunch_blink_frequency", 0.3f));
        mmBlinkNoise    = clampf(p.f("monstermunch_blink_noise", 0.5f), 0, 1);
        mmBlinkT        = fmaxf(0.02f, p.f("monstermunch_blink_time", 0.15f));
        {   // monstermunch_suits: heart, spade, club (or clover), diamond -- one for each, in map order
            std::string list = p.s("monstermunch_suits", "diamond club spade heart"), word;
            mmSuits.clear();
            for (size_t c = 0; c <= list.size(); c++) {
                char ch = c < list.size() ? (char)tolower((unsigned char)list[c]) : ' ';
                if (isalpha((unsigned char)ch)) { word += ch; continue; }
                if (word.rfind("heart", 0) == 0) mmSuits.push_back(0);
                else if (word.rfind("spade", 0) == 0) mmSuits.push_back(1);
                else if (word.rfind("club", 0) == 0 || word.rfind("clover", 0) == 0) mmSuits.push_back(2);
                else if (word.rfind("diamond", 0) == 0) mmSuits.push_back(3);
                word.clear();
            }
            if (mmSuits.empty()) mmSuits.push_back(0);
        }
        barDecor        = p.b("display_Leon_bar_decorator", true);
        musicOn         = p.b("music", true);
        {   // look: sketch, or engraving (steampunk, after Doré) -- an older params' decor = engraving too
            std::string look = p.s("look", p.s("decor", "sketch"));
            for (auto& ch : look) ch = (char)tolower((unsigned char)ch);
            engrave = look == "engraving" || look == "engraved" || look == "steampunk" || look == "dore";
        }
        engraveSpacing  = fmaxf(1.5f, p.f("engraving_line_spacing", 3.5f));
        engraveSkySpacing = fmaxf(2.0f, p.f("engraving_sky_line_spacing", 5.0f));
        engraveWind     = fmaxf(0, p.f("engraving_cloud_wind", 1.0f));
        engraveDrawings = p.b("engraving_cloud_drawings", true);
        pressWaveAmp    = p.f("title_press_wave_amplitude", 2.0f);
        pressWavePeriod = fmaxf(0.05f, p.f("title_press_wave_period", 1.5f));
        pressBlinkPeriod = fmaxf(0.05f, p.f("title_press_blink_period", 2.0f));
        pressSize       = fmaxf(4, p.f("title_press_font_size", 24));
        scoresRainbow   = p.f("title_scores_rainbow_speed", 0.25f);
        pressAlphaMin   = clampf(p.f("title_press_alpha_min", 0.1f), 0, 1);
        pressAlphaMax   = clampf(p.f("title_press_alpha_max", 1.0f), 0, 1);
        musicVolume     = clampf(p.f("music_volume", 0.45f), 0, 1);
        musicFadeOut    = fmaxf(0.01f, p.f("music_fade_out", 3));
        musicGap        = fmaxf(0, p.f("music_gap", 0));
        musicFadeIn     = fmaxf(0.01f, p.f("music_fade_in", 3));
        soundOn         = p.b("sound", true);
        soundVolume     = clampf(p.f("sound_volume", 0.6f), 0, 1);
        leanStiffness   = fmaxf(1, p.f("lean_stiffness", 320));
        leanDamping     = fmaxf(0, p.f("lean_damping", 14));
        lookPos         = clampf(p.f("look_screen_position", 1.0f / 3.0f), 0.05f, 0.5f);
        playerFrac  = clampf(p.f("player_screen_height", 0.08f), 0.005f, 0.9f);
        zoom        = fmaxf(0.1f, p.f("view_zoom", 1.0f));
        {   // screen_ratio: "16:9" (or 16/9, 1.778); off -- the window's own shape
            std::string v = p.s("screen_ratio", "16:9");
            float a = 0, b = 0;
            if (sscanf(v.c_str(), "%f%*[:/]%f", &a, &b) == 2 && b > 0) screenRatio = a / b;
            else screenRatio = (float)atof(v.c_str());       // (a number -- or off: 0)
            if (screenRatio < 0.2f || screenRatio > 5) screenRatio = 0;
        }
        {   // background_pattern_<mode> -- for a mode with none, background_pattern -- else its own
            static const char* mode[5] = {"easy", "hard", "dark", "harder", "cameleon"};
            static const char* own[5] = {"karakusa", "yagasuri", "asanoha", "seigaiha", "sayagata"};
            std::string all = p.s("background_pattern", "");
            for (int k = 0; k < 5; k++)
                bgPattern[k] = bgPatternNamed(p.s((std::string("background_pattern_") + mode[k]).c_str(), all.empty() ? own[k] : all));
            bgGolden = bgPatternNamed(p.s("background_pattern_golden", "karakusa"));
        }
        bgOpacity   = clampf(p.f("background_pattern_opacity", 0.11f), 0.0f, 1.0f);
        bgFade      = fmaxf(0, p.f("background_pattern_fade", 3.0f));
        bgScale     = clampf(p.f("background_pattern_scale", 1.0f), 0.1f, 10.0f);
        bgParallax  = clampf(p.f("background_pattern_parallax", 0.35f), 0.0f, 1.0f);
        deadzone    = p.f("gamepad_deadzone", 0.2f);
    }
};

// ---------------------------------------------------------------------------------------------
// Level: a text map. '#' solid, '=' thin platform (solid too), 'P' start, '*' star,
// '>' '<' '^' 'v' gravity platforms (solid blocks, one arrow direction each), '~' slippery,
// 'X' dark matter (deadly), 'B' a movable block (its top-left tile; it is 2x2 tiles),
// 'A' a pineapple: a checkpoint ('a': back there, he faces right), 'D' / 'T' the double-jump / tongue fruit (a level that has one
// starts without that ability), 'F' a fly, 'E' the exit: a red key floating at the middle of its
// tiles, 'H' the hat, 'h' a spare-hat spot (a lost hat waits there), 'C' a double cherry (eaten, it tells how the tongue is swallowed back or paid
// out), 'G' the green flag: the end of the way back, after the exit, 'L' a locked door (a column of
// them makes one door): a wall until Leon touches it holding the key, '.' dark air: open, the
// light fading as Leon goes down it, 'I' a lamp post: a pole two tiles tall on the ground under
// this tile (or hanging from the ceiling over it), a golden fly circling its top that lights the
// dark, 'i' a lamp post with no fly yet, 'U' a banana (eaten, it says to swallow the fly), 'Z' a
// kaki (eaten, it says to look down -- the right stick, or Alt+Down), 'K' a
// blue key, 'l' a blue door (only the blue key opens it, as only the red one opens 'L'), 'J' the
// blue flag: the end of dark mode (its clock stops, the music changes) and the start of harder
// mode, 'V' the violet flag: the end of harder mode and the start of cameleon mode, 'k' a green
// key, 'g' a green door (only the green key opens it), 'p' a purple key, 'q' a purple door, 'o' the
// golden key (glowing; it starts no mode, no clock), 'O' a golden door (the golden key opens one of
// them, whichever it meets first: then it is gone), 'b' the black flag -- a rainbow flag (the end of the golden run; no
// checkpoint), 'M' a
// Monstermunch: its top-left tile (it is 2x2 tiles, solid, 'm' once loaded -- dug in the ground,
// a floor, a ceiling or a wall: its face towards the open side), 'u' '(' 'n' ')' ice gravity
// platforms (down, left, up, right: the round side shows the way) -- gravity platforms, white with
// dark arrows, that no tongue sticks to: it bounces off, as off ice --, '@' a dark light: a light
// the other way round, darkness round it.
// ---------------------------------------------------------------------------------------------
// The gravity frame a gravity platform asks for (0 down, 1 left, 2 up, 3 right) -- -1 not one.
static int gravityFrame(char c) {
    switch (c) {
        case 'v': case 'u': return 0;
        case '<': case '(': return 1;
        case '^': case 'n': return 2;
        case '>': case ')': return 3;
    }
    return -1;
}
static bool iceGravity(char c) { return c == 'u' || c == '(' || c == 'n' || c == ')'; }
static bool tongueBounces(char c) { return c == '~' || iceGravity(c); }   // (ice, and ice gravity)
static constexpr char kHatDropIce = ']';      // rightward ice at the first bend of the Solo pit
struct Level {
    int w = 0, h = 0;
    int generation = 0;                       // one more each load: what is worked out from the map (the background's cover) is stale
    std::vector<std::string> rows;
    Vector2 spawn = {100, 100};
    std::vector<Vector2> stars;
    std::vector<Vector2> crates;              // blocks: top-left corners as placed
    // The checkpoints: the pineapples ('A'), then each flag and each key -- a checkpoint too, of its
    // own (pineKind: 'A', or the flag's or key's letter; only a pineapple is drawn, eaten, taken away).
    std::vector<Vector2> pines;
    std::vector<char> pineKind;
    bool pineapple(size_t i) const { return pineKind[i] == 'A'; }
    std::vector<Vector2> fruits;              // ability fruits...
    std::vector<char> fruitKind;              // ...'D' double jump, 'T' tongue, 'U' banana, 'Z' kaki (those two: advice only)
    std::vector<Vector2> flySpots;            // 'F': where flies start (else, in a level without lamps: anywhere)
    Rectangle exitRect{0, 0, 0, 0};           // 'E' tiles: the exit, the red key -- floating at their middle
    bool hasExit = false;
    int exitKeyPine = -1;                     // (its checkpoint)
    int tongueFruitPine = -1;                 // the pineapple nearest the tongue fruit: hard mode's, kept for it -- in easy mode, not there (-1: none)
    // The flags (flagLetters), each the end of a mode: its tiles, its checkpoint -- and its ground for
    // each gravity (0 below, 1 left, 2 above, 3 right -- as the gravity frames), where its pole
    // stands on it, and how tall the pole is (what room allows).
    struct Flag {
        Rectangle rect{0, 0, 0, 0};
        bool has = false;
        int pine = -1;
        int side[4] = {};
        Vector2 foot[4] = {};
        float pole[4] = {};
    };
    Flag flags[flCount];
    Flag blackFlag;                           // 'b': the golden run's end (no checkpoint) -- a rainbow flag now, black at first
    std::vector<Rectangle> doors;            // 'L': locked doors, a column of tiles each
    std::vector<unsigned char> darkAir;       // '.': open tiles where the light fades going down (w*h)...
    bool hasDark = false;
    float darkTop = 0;                        // ...from the top of them (world y)
    std::vector<bool> pineBeyond;             // checkpoints only a door opens the way to
    std::vector<Vector2> lamps;               // 'I' / 'i': lamp posts, each the foot of its pole...
    std::vector<Vector2> lampDir;             // ...its pole's way from its foot: up off the ground, down from a ceiling, or out of a wall...
    std::vector<bool> lampLit;                // ...with its fly from the start ('I') or not ('i')
    std::vector<Vector2> poles;               // colour poles (workshop/colorpole.jpg), each the foot of its pole...
    std::vector<Vector2> poleDir;             // ...its way from its foot, as a lamp post's...
    std::vector<int> poleColour;              // ...its colour (0, 1, 2: color_1 .. color_3)...
    std::vector<bool> poleFly;                // ...and its kind: '1' '2' '3' a balloon's (no fly), '4' '5' '6' a colour fly's (no balloon),
                                              // '7' '8' '9' a colour fly's sticking out of the wall beside it
    std::vector<int> poleTurn;                // a balloon pole's turn in the level's round (color_balloon_sync): its colour (-1: a fly's pole)
    std::vector<char> doorChar;               // each door's letter: 'L' red, 'l' blue
    std::vector<Vector2> blueKeys;            // 'K': blue keys, floating at their tile's middle
    std::vector<Vector2> greenKeys;           // 'k': green keys, the same
    std::vector<Vector2> purpleKeys;          // 'p': purple keys, the same
    std::vector<Vector2> goldKeys;            // 'o': golden keys, the same
    std::vector<int> blueKeyPine, greenKeyPine, purpleKeyPine, goldKeyPine;   // (their checkpoints)
    std::vector<Vector2> darkLights;          // '@': dark lights, at their tile's middle
    Vector2 hatSpot{};                        // 'H': where the hat lies
    bool hasHatSpot = false;
    std::vector<Vector2> spareHats;           // 'h': where a lost hat waits -- still lost as he comes near one (spareHatStep)
    // The ending's place (workshop/ending (draft).png): the level's highest tiles -- the tower's top.
    // Leon's end on its first tile, Anna waiting on its last, and over its second an invisible ice
    // wall endWallH tiles high (ending_wall_height: the game sets it) -- never drawn.
    int endX0 = -1, endX1 = -1, endY = 0;     // its first and last tiles, its row (-1: none)
    int endWallH = 0;
    bool endWall(int x, int y) const { return endX0 >= 0 && x == endX0 + 1 && y < endY && y >= endY - endWallH; }
    Vector2 annaFeet() const { return {(endX1 + 0.5f) * kTile, endY * kTile}; }
    std::vector<bool> pineRight;              // 'a' (a pineapple as 'A'): back there, he faces right -- his right, as the screen shows it
    std::vector<Vector2> cherries;            // 'C': double cherries
    std::vector<Vector2> monsters;            // 'M': the Monstermunchs, each its top-left corner...
    std::vector<int> monsterFace;             // ...and its open side, as quarter turns from up (0 up, 1 right, 2 down, 3 left)
    std::vector<Rectangle> solidRects;      // merged blocks, for drawing the hatching
    std::vector<int> pathOrder;               // the checkpoints in the order the level's way meets them
    std::vector<int> pineStage;               // each checkpoint's stage: 0 easy, 1 hard, 2 dark, 3 harder, 4 cameleon
    std::vector<int> stageWay[5];             // each stage's way: its key's checkpoint, its pineapples, its flag's, the next key's

    char at(int x, int y) const {
        if (endWall(x, y)) return '~';            // (the ending's wall: ice, unseen)
        if (y < 0 || y >= h || x < 0 || x >= (int)rows[y].size()) return ' ';
        char c = rows[y][x];
        return c == kHatDropIce ? ')' : c;
    }
    bool solid(int x, int y) const { return at(x, y) == '#'; }
    bool oneway(int x, int y) const { return at(x, y) == '='; }
    bool gravity(int x, int y) const { return gravityFrame(at(x, y)) >= 0; }   // (an ice one too)
    bool slippy(int x, int y) const { return at(x, y) == '~'; }
    bool deadly(int x, int y) const { return at(x, y) == 'X'; }
    bool door(int x, int y) const { char c = at(x, y); return c == 'L' || c == 'l' || c == 'g' || c == 'q' || c == 'O'; }   // (an open door's tiles are blanked)
    bool monster(int x, int y) const { return at(x, y) == 'm'; }   // a Monstermunch's tiles (it draws itself)
    bool dark(int x, int y) const { return hasDark && x >= 0 && y >= 0 && x < w && y < h && darkAir[(size_t)y * w + x]; }
    // A whole-tile obstacle: solid, gravity, slippery, dark matter, a locked door or a Monstermunch.
    bool block(int x, int y) const { return solid(x, y) || gravity(x, y) || slippy(x, y) || deadly(x, y) || door(x, y) || monster(x, y); }

    bool load(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) { TraceLog(LOG_ERROR, "LEVEL: cannot open %s", path); return false; }
        rows.clear(); stars.clear(); solidRects.clear(); crates.clear(); pines.clear(); pineKind.clear(); pineRight.clear(); spareHats.clear();
        generation++;
        endX0 = endX1 = -1;
        fruits.clear(); fruitKind.clear(); flySpots.clear(); hasExit = false; hasHatSpot = false; cherries.clear();
        doors.clear(); doorChar.clear(); lamps.clear(); lampDir.clear(); lampLit.clear(); blueKeys.clear(); greenKeys.clear(); purpleKeys.clear(); goldKeys.clear();
        darkLights.clear();
        poles.clear(); poleDir.clear(); poleColour.clear(); poleFly.clear(); poleTurn.clear();
        monsters.clear();
        for (Flag& fl : flags) fl = Flag{};
        blackFlag = Flag{};
        std::vector<std::pair<int, int>> lampTiles, poleTiles;
        std::vector<bool> poleOnWall;                    // ('7' '8' '9': a fly's pole sticking out of the wall beside it, whatever else)
        auto addTile = [](Rectangle& rect, bool& has, int x, int y) {   // a flag's tiles: the box round them all
            Rectangle r = {x * kTile, y * kTile, kTile, kTile};
            if (has) {
                float x0 = fminf(rect.x, r.x), y0 = fminf(rect.y, r.y);
                float x1 = fmaxf(rect.x + rect.width, r.x + r.width), y1 = fmaxf(rect.y + rect.height, r.y + r.height);
                r = {x0, y0, x1 - x0, y1 - y0};
            }
            rect = r;
            has = true;
        };
        char buf[4096];
        while (fgets(buf, sizeof(buf), f)) {
            std::string l = buf;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            rows.push_back(l);
        }
        fclose(f);
        h = (int)rows.size();
        w = 0;
        for (auto& r : rows) w = std::max(w, (int)r.size());
        darkAir.assign((size_t)w * h, 0); hasDark = false; darkTop = 0;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < (int)rows[y].size(); x++) {
                char& c = rows[y][x];
                if (c == 'P') { spawn = {(x + 0.5f) * kTile, (y + 1) * kTile}; c = ' '; }
                else if (c == '.') {                     // dark air: open, the light fading down it
                    darkAir[(size_t)y * w + x] = 1;
                    if (!hasDark) darkTop = y * kTile;   // (rows go down: the first is the top)
                    hasDark = true; c = ' ';
                }
                else if (c == 'B') { crates.push_back({x * kTile, y * kTile}); c = ' '; }   // a block's top-left
                else if (c == 'A' || c == 'a') {         // pineapple ('a': back there, facing right)
                    pines.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); pineKind.push_back('A');
                    pineRight.resize(pines.size()); pineRight.back() = c == 'a'; c = ' ';
                }
                else if (c == 'h') { spareHats.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == 'D' || c == 'T' || c == 'U' || c == 'Z') { fruits.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); fruitKind.push_back(c); c = ' '; }
                else if (c == 'K') { blueKeys.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == 'k') { greenKeys.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == 'p') { purpleKeys.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == 'o') { goldKeys.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == '@') { darkLights.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == 'F') { flySpots.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == 'H') { hatSpot = {(x + 0.5f) * kTile, (y + 0.5f) * kTile}; hasHatSpot = true; c = ' '; }
                else if (c == 'I' || c == 'i') { lampTiles.push_back({x, y}); lampLit.push_back(c == 'I'); c = ' '; }
                else if (c >= '1' && c <= '9') { poleTiles.push_back({x, y}); poleColour.push_back((c - '1') % 3); poleFly.push_back(c >= '4'); poleOnWall.push_back(c >= '7'); c = ' '; }
                else if (c == 'C') { cherries.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
                else if (c == 'M') {                     // a Monstermunch: its 2x2 tiles, solid
                    monsters.push_back({x * kTile, y * kTile});
                    for (int j = 0; j < 2 && y + j < h; j++)
                        for (int i = 0; i < 2 && x + i < (int)rows[y + j].size(); i++) rows[y + j][x + i] = 'm';
                }
                else if (c == 'E') addTile(exitRect, hasExit, x, y);  // (stays in the map, passable)
                else if (const char* fl = c ? strchr(kFlagLetters, c) : nullptr) {
                    Flag& f = flags[fl - kFlagLetters];
                    addTile(f.rect, f.has, x, y); c = ' ';
                }
                else if (c == 'b') { addTile(blackFlag.rect, blackFlag.has, x, y); c = ' '; }
                else if (c == '*') { stars.push_back({(x + 0.5f) * kTile, (y + 0.5f) * kTile}); c = ' '; }
            }
        auto checkpointAt = [&](Vector2 p, char kind) { pines.push_back(p); pineKind.push_back(kind); return (int)pines.size() - 1; };
        for (int k = 0; k < flCount; k++)                // the flags' and keys' checkpoints: after the pineapples
            if (flags[k].has) flags[k].pine = checkpointAt({flags[k].rect.x + flags[k].rect.width / 2, flags[k].rect.y + flags[k].rect.height / 2}, kFlagLetters[k]);
        exitKeyPine = hasExit ? checkpointAt({exitRect.x + exitRect.width / 2, exitRect.y + exitRect.height / 2}, 'E') : -1;
        blueKeyPine.clear(); greenKeyPine.clear(); purpleKeyPine.clear(); goldKeyPine.clear();
        for (Vector2 k : blueKeys) blueKeyPine.push_back(checkpointAt(k, 'K'));
        for (Vector2 k : greenKeys) greenKeyPine.push_back(checkpointAt(k, 'k'));
        for (Vector2 k : purpleKeys) purpleKeyPine.push_back(checkpointAt(k, 'p'));
        for (Vector2 k : goldKeys) goldKeyPine.push_back(checkpointAt(k, 'o'));   // (last: the others' numbers kept)
        tongueFruitPine = -1;
        for (size_t i = 0; i < fruits.size() && tongueFruitPine < 0; i++) {
            if (fruitKind[i] != 'T') continue;
            float bd = 0;
            for (size_t j = 0; j < pines.size(); j++) {
                if (pineKind[j] != 'A') continue;
                float d = Vector2DistanceSqr(fruits[i], pines[j]);
                if (tongueFruitPine < 0 || d < bd) { tongueFruitPine = (int)j; bd = d; }
            }
        }
        if (tongueFruitPine == exitKeyPine) tongueFruitPine = -1;
        auto standOn = [&](int x, int y, Vector2& foot, Vector2& dir) {   // a post stands on the ground -- or, with none under it but
            if (block(x, y + 1) || !(block(x, y - 1) || block(x - 1, y) || block(x + 1, y)))   // a ceiling over it, hangs from that;
                { foot = {(x + 0.5f) * kTile, (y + 1) * kTile}; dir = {0, -1}; }               // with neither, but a wall beside it,
            else if (block(x, y - 1)) { foot = {(x + 0.5f) * kTile, y * kTile}; dir = {0, 1}; } // sticks out of that wall (left first)
            else if (block(x - 1, y)) { foot = {x * kTile, (y + 0.5f) * kTile}; dir = {1, 0}; }
            else { foot = {(x + 1) * kTile, (y + 0.5f) * kTile}; dir = {-1, 0}; }
        };
        for (size_t k = 0; k < poleTiles.size(); k++) {  // (a colour pole: the same -- '7' '8' '9': out of a wall beside it first)
            auto [x, y] = poleTiles[k];
            Vector2 foot, dir;
            if (poleOnWall[k] && block(x - 1, y)) { foot = {x * kTile, (y + 0.5f) * kTile}; dir = {1, 0}; }
            else if (poleOnWall[k] && block(x + 1, y)) { foot = {(x + 1) * kTile, (y + 0.5f) * kTile}; dir = {-1, 0}; }
            else standOn(x, y, foot, dir);
            poles.push_back(foot);
            poleDir.push_back(dir);
        }
        // color_balloon_sync: a balloon's turn is its colour's -- cyan, magenta, yellow, all of a colour at once.
        poleTurn.assign(poles.size(), -1);
        for (size_t k = 0; k < poles.size(); k++) if (!poleFly[k]) poleTurn[k] = poleColour[k];
        for (auto [x, y] : lampTiles) {
            Vector2 foot, dir;
            standOn(x, y, foot, dir);
            lamps.push_back(foot);
            lampDir.push_back(dir);
            if (dark(x - 1, y) || dark(x + 1, y) || dark(x, y - 1) || dark(x, y + 1))   // in dark air: in the dark too
                darkAir[(size_t)y * w + x] = 1;
        }
        monsterFace.clear();
        for (Vector2 mp : monsters) {                    // which way each Monstermunch faces: its open side
            int x = (int)lroundf(mp.x / kTile), y = (int)lroundf(mp.y / kTile);
            auto open = [&](int ax, int ay) {            // (off the map: not open -- else one at its edge faced out of it)
                return ay >= 0 && ay < h && ax >= 0 && ax < (int)rows[ay].size() && !block(ax, ay) && !oneway(ax, ay);
            };
            int face = 0;                                // (up -- in a floor --, else down, left, right)
            if (open(x, y - 1) && open(x + 1, y - 1)) face = 0;
            else if (open(x, y + 2) && open(x + 1, y + 2)) face = 2;
            else if (open(x - 1, y) && open(x - 1, y + 1)) face = 3;
            else if (open(x + 2, y) && open(x + 2, y + 1)) face = 1;
            monsterFace.push_back(face);
        }
        // Row runs of solid tiles, then stacked identical runs fused into one block.
        std::vector<Rectangle> open;
        for (int y = 0; y < h; y++) {
            std::vector<Rectangle> next;
            for (int x = 0; x < w;) {
                if (!solid(x, y)) { x++; continue; }
                int x0 = x;
                while (x < w && solid(x, y)) x++;
                Rectangle r = {x0 * kTile, y * kTile, (x - x0) * kTile, kTile};
                bool merged = false;
                for (auto& o : open)
                    if (o.x == r.x && o.width == r.width && o.y + o.height == r.y) {
                        o.height += kTile; next.push_back(o); o.width = -1; merged = true; break;
                    }
                if (!merged) next.push_back(r);
            }
            for (auto& o : open) if (o.width > 0) solidRects.push_back(o);
            open = next;
        }
        for (auto& o : open) solidRects.push_back(o);
        for (int x = 0; x < w; x++)                      // doors: each column run of 'L' (or of 'l')
            for (int y = 0; y < h;) {
                if (!door(x, y)) { y++; continue; }
                int y0 = y;
                char dc = at(x, y);
                while (y < h && at(x, y) == dc) y++;
                doors.push_back({x * kTile, y0 * kTile, kTile, (y - y0) * kTile});
                doorChar.push_back(dc);
            }
        for (int k = 0; k < flCount; k++) {
            Flag& f = flags[k];
            if (!f.has) continue;
            anchorFlag(f.rect, f.side, f.foot, f.pole);
            TraceLog(LOG_INFO, "LEVEL: flag %c's ground for gravity down/left/up/right (0 below, 1 left, 2 above, 3 right): %d %d %d %d, pole %.0f %.0f %.0f %.0f px",
                     kFlagLetters[k], f.side[0], f.side[1], f.side[2], f.side[3], f.pole[0], f.pole[1], f.pole[2], f.pole[3]);
        }
        if (blackFlag.has) anchorFlag(blackFlag.rect, blackFlag.side, blackFlag.foot, blackFlag.pole);
        if (!doors.empty()) TraceLog(LOG_INFO, "LEVEL: %d door(s)", (int)doors.size());
        orderPines();
        for (int y = 0; y < h && endX0 < 0; y++)     // the ending's place: the first run of solid tiles from the top -- not the map's side walls
            for (int x = 0; x < (int)rows[y].size(); x++) {
                if (!solid(x, y)) continue;
                int x1 = x;
                while (solid(x1 + 1, y)) x1++;
                if (x > 0 && x1 < w - 1) { endX0 = x; endX1 = x1; endY = y; break; }
                x = x1;
            }
        if (endX0 >= 0) TraceLog(LOG_INFO, "LEVEL: the ending's place: tiles %d..%d of row %d (Anna on the last, the wall over the second)", endX0, endX1, endY);
        TraceLog(LOG_INFO, "LEVEL: %dx%d tiles, %d stars, %d slabs", w, h, (int)stars.size(), (int)solidRects.size());
        return true;
    }

    // A flag is planted as the gravity of the moment would have it: on the ground that way, if
    // there is some within two tiles of its tiles; else on the ground nearest them -- below if as
    // near as any, then left, right, above. Its pole stands straight out of that ground. Measured
    // from the tiles across the flag's middle; with no ground within reach, it stands on its
    // tiles' bottom. For each gravity g: side[g], foot[g] -- and pole[g], 2.2 tiles, or less where
    // the room out of that ground is lower (a corridor).
    void anchorFlag(Rectangle r, int side[4], Vector2 foot[4], float pole[4]) const {
        int x0 = (int)lroundf(r.x / kTile), x1 = (int)lroundf((r.x + r.width) / kTile) - 1;
        int y0 = (int)lroundf(r.y / kTile), y1 = (int)lroundf((r.y + r.height) / kTile) - 1;
        float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
        int mx0 = (int)floorf((cx - 0.5f) / kTile), mx1 = (int)floorf((cx + 0.5f) / kTile);
        int my0 = (int)floorf((cy - 0.5f) / kTile), my1 = (int)floorf((cy + 0.5f) / kTile);
        auto ground = [&](int x, int y) { return block(x, y) || oneway(x, y); };
        const int reach = 30, near = 2;
        int dist[4];                                     // free tiles to the ground on each side (-1: none in reach)
        for (int s = 0; s < 4; s++) {
            dist[s] = -1;
            for (int d = 0; d < reach && dist[s] < 0; d++)
                if (s == 0 ? ground(mx0, y1 + 1 + d) || ground(mx1, y1 + 1 + d)
                    : s == 2 ? ground(mx0, y0 - 1 - d) || ground(mx1, y0 - 1 - d)
                    : s == 1 ? ground(x0 - 1 - d, my0) || ground(x0 - 1 - d, my1)
                             : ground(x1 + 1 + d, my0) || ground(x1 + 1 + d, my1)) dist[s] = d;
        }
        int nearest = -1;
        for (int s : {0, 1, 3, 2})
            if (dist[s] >= 0 && (nearest < 0 || dist[s] < dist[nearest])) nearest = s;
        for (int g = 0; g < 4; g++) {
            int s = dist[g] >= 0 && dist[g] <= near ? g : nearest < 0 ? 0 : nearest;
            int d = std::max(0, dist[s]);
            side[g] = s;
            foot[g] = s == 0 ? Vector2{cx, (y1 + 1 + d) * kTile} : s == 2 ? Vector2{cx, (float)(y0 - d) * kTile}
                    : s == 1 ? Vector2{(float)(x0 - d) * kTile, cy} : Vector2{(x1 + 1 + d) * kTile, cy};
            // The room: free tiles straight out of the ground, across the flag's middle.
            int n = 0;
            for (; n < 3; n++) {
                bool hit = s == 0 ? ground(mx0, y1 + d - n) || ground(mx1, y1 + d - n)
                         : s == 2 ? ground(mx0, y0 - d + n) || ground(mx1, y0 - d + n)
                         : s == 1 ? ground(x0 - d + n, my0) || ground(x0 - d + n, my1)
                                  : ground(x1 + d - n, my0) || ground(x1 + d - n, my1);
                if (hit) break;
            }
            pole[g] = fminf(2.2f * kTile, fmaxf(0.5f * kTile, n * kTile - 6));   // (its knob clear of a ceiling)
        }
    }

    // The checkpoints along the level's way -- a guess from distances through open tiles: from the
    // start, minus those to the exit (from the start alone if no exit is in reach). Those beyond a
    // locked door come after, in order from the start (doors open); one cut off from the start
    // goes last.
    void orderPines() {
        pathOrder.clear();
        pineBeyond.assign(pines.size(), false);
        pineStage.assign(pines.size(), 0);
        for (std::vector<int>& way : stageWay) way.clear();
        if (pines.empty() || w <= 0 || h <= 0) return;
        auto clear = [&](int x, int y, bool doorsOpen) {
            return x >= 0 && y >= 0 && x < w && y < h && (!block(x, y) || (doorsOpen && door(x, y))) && !oneway(x, y);
        };
        auto spread = [&](const std::vector<int>& from, bool doorsOpen = false) {   // breadth-first, in tiles
            std::vector<int> d(w * h, -1);
            std::deque<int> q;
            for (int c : from) if (d[c] < 0 && clear(c % w, c / w, doorsOpen)) { d[c] = 0; q.push_back(c); }
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            while (!q.empty()) {
                int c = q.front(), x = c % w, y = c / w;
                q.pop_front();
                for (int k = 0; k < 4; k++) {
                    int n = c + dy[k] * w + dx[k];
                    if (clear(x + dx[k], y + dy[k], doorsOpen) && d[n] < 0) { d[n] = d[c] + 1; q.push_back(n); }
                }
            }
            return d;
        };
        auto cell = [&](Vector2 p) {
            return std::clamp((int)floorf(p.y / kTile), 0, h - 1) * w + std::clamp((int)floorf(p.x / kTile), 0, w - 1);
        };
        std::vector<int> exitCells;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < (int)rows[y].size(); x++) if (rows[y][x] == 'E') exitCells.push_back(y * w + x);
        int start = cell({spawn.x, spawn.y - kTile / 2});
        std::vector<int> ds = spread({start}), de = spread(exitCells);
        std::vector<int> dOpen = doors.empty() ? ds : spread({start}, true);
        bool viaExit = false;
        for (int c : exitCells) if (ds[c] >= 0) viaExit = true;
        std::vector<float> key(pines.size());
        for (size_t i = 0; i < pines.size(); i++) {
            int c = cell(pines[i]);
            pineBeyond[i] = ds[c] < 0 && dOpen[c] >= 0;
            key[i] = pineBeyond[i] ? 1e6f + dOpen[c]
                   : ds[c] < 0 ? 2e6f + Vector2Distance(pines[i], spawn) : (float)(viaExit && de[c] >= 0 ? ds[c] - de[c] : ds[c]);
        }
        for (size_t i = 0; i < pines.size(); i++) pathOrder.push_back((int)i);
        std::stable_sort(pathOrder.begin(), pathOrder.end(), [&](int a, int b) { return key[a] < key[b]; });
        std::string s;
        for (int i : pathOrder) s += std::to_string(i) + (pineBeyond[i] ? "(beyond a door) " : " ");
        TraceLog(LOG_INFO, "LEVEL: checkpoints along the way (their numbers in map order): %s", s.c_str());

        // The stages -- 0 easy, 1 hard, 2 dark, 3 harder, 4 cameleon: each a stretch of the way its key
        // starts (the red, green, blue, purple key) and its flag ends (yellow, red, green, blue, violet).
        // A key's checkpoint is the stage it starts, a flag's the one it ends. A pineapple the start
        // reaches, every door shut: easy; beyond a door: the first stage whose way -- its key to its
        // flag, through open tiles, doors open -- passes it (4 tiles aside at most), else the stage of
        // the key nearest it. Each stage's way, for the guide arrow: its key, its pineapples nearest
        // the key first, its flag, the next stage's key.
        int keyPine[5] = {-1, exitKeyPine, greenKeyPine.empty() ? -1 : greenKeyPine[0],
                          blueKeyPine.empty() ? -1 : blueKeyPine[0], purpleKeyPine.empty() ? -1 : purpleKeyPine[0]};
        std::vector<int> dKey[5], dFlag[5];
        for (int st = 1; st < 5; st++) {
            if (keyPine[st] >= 0) dKey[st] = spread({cell(pines[keyPine[st]])}, true);
            if (flags[st].has) dFlag[st] = spread({cell(pines[flags[st].pine])}, true);
        }
        for (int st = 0; st < 5; st++) {
            if (keyPine[st] >= 0) pineStage[keyPine[st]] = st;
            if (flags[st].has) pineStage[flags[st].pine] = st;
        }
        for (size_t i = 0; i < pines.size(); i++) {
            if (pineKind[i] != 'A' || !pineBeyond[i]) continue;
            int c = cell(pines[i]), on = -1, near = -1;
            for (int st = 1; st < 5 && on < 0; st++) {
                if (dKey[st].empty() || dKey[st][c] < 0) continue;
                if (near < 0 || dKey[st][c] < dKey[near][c]) near = st;
                int f = dFlag[st].empty() ? -1 : cell(pines[flags[st].pine]);
                if (f >= 0 && dKey[st][f] >= 0 && dFlag[st][c] >= 0 && dKey[st][c] + dFlag[st][c] - dKey[st][f] <= 4) on = st;
            }
            pineStage[i] = on >= 0 ? on : near >= 0 ? near : 0;
        }
        for (int st = 0; st < 5; st++) {
            std::vector<int>& way = stageWay[st];
            if (st < 2 || keyPine[st] < 0) continue;       // (easy and hard: their own guides)
            way.push_back(keyPine[st]);
            std::vector<int> pa;
            for (size_t i = 0; i < pines.size(); i++) if (pineKind[i] == 'A' && pineStage[i] == st) pa.push_back((int)i);
            std::stable_sort(pa.begin(), pa.end(), [&](int a, int b) { return dKey[st][cell(pines[a])] < dKey[st][cell(pines[b])]; });
            way.insert(way.end(), pa.begin(), pa.end());
            if (flags[st].has) way.push_back(flags[st].pine);
            if (st + 1 < 5 && keyPine[st + 1] >= 0) way.push_back(keyPine[st + 1]);
        }
        s.clear();
        for (size_t i = 0; i < pines.size(); i++) s += std::to_string(pineStage[i]);
        TraceLog(LOG_INFO, "LEVEL: checkpoints' stages (in map order; 0 easy .. 4 cameleon): %s", s.c_str());
        for (int st = 2; st < 5; st++) {
            s.clear();
            for (int i : stageWay[st]) s += std::to_string(i) + " ";
            TraceLog(LOG_INFO, "LEVEL: stage %d's way: %s", st, s.empty() ? "-" : s.c_str());
        }
    }
};

// ---------------------------------------------------------------------------------------------
// Particles: landing dust, skid puffs, star sparks.
// ---------------------------------------------------------------------------------------------
struct Particle {
    Vector2 p{}, v{};
    float life = 0, maxLife = 0, r = 0;
    bool spark = false;
    bool drop = false;        // saliva: falls with gravity, gone when it hits something
    bool bubble = false;      // soap: grows a little, drifts up, pops
    bool mm = false;          // a Monstermunch's drool: falls through its tiles, into its place
};

// A movable block: pulled with the tongue, falls with gravity, slides on slippery ground.
struct Crate {
    Vector2 c{}, v{};         // centre and velocity (world)
    Vector2 home{};
    // Rolling. Collisions stay those of an upright square; only the drawing turns -- and it
    // always comes to rest flat on a face.
    float ang = 0, om = 0;    // degrees, degrees per second (clockwise on screen)
    int mode = 0;             // 0 resting / flying, 1 tipping over a corner, 2 rolling flat
    Vector2 pivot{};          // the corner it tips over
    Vector2 tipDir{1, 0};     // ...towards this side
    Vector2 c0{}, r0{};       // where it was, and its middle from the corner, when it began to tip
    float tipAng = 0;         // how far it has tipped (degrees)
    bool touched = false;     // never moves before Leon has touched it (body or tongue)...
    bool awake = false;       // ...then asleep -- frozen, even in mid-air -- until it has been on screen
    Vector2 visOff{};         // drawn this much off its collision box (the true arc)
    float tipSign = 1;
    float settleTo = 0;
};

// One of the 16 little cubes Leon bursts into.
struct Piece {
    Vector2 p{}, v{};
    float a = 0, av = 0;
    bool tongue = false;      // a bit of the torn tongue (pink)
    bool blood = false;       // a blood cube, from a Monstermunch's munch (red)
    float size = 0;           // its side (0: a quarter of Leon's)
};

// A fly: buzzes left or right (relative to gravity) with a restless random jitter, and
// turns round when it bumps into something. A golden one circles Leon instead.
struct Fly {
    Vector2 p{}, v{};           // (both start at zero -- v was once left to chance: flies shot off)
    float dir = 1;
    float phase = 0;
    bool caught = false;
    bool inMouth = false;       // swallowed: being chewed...
    bool golden = false;        // ...then coughed back out, golden: it follows Leon for ever
    float orbit = 0;            // where it is on its round about him (radians)
    int lamp = -1;              // a lamp's fly (golden, lighting the dark): its post, round which it flies...
    float lampT = -1;           // ...but swallowed and coughed back, seconds left round Leon (-1: at its post)
    int home = -1;              // ...and the post it started on: back there when Leon dies, whatever the death
    int pole = -1;              // a colour pole's fly: round its balloon, of its colour -- swallowed, kept in his belly a while
    float followT = -1;         // ...then spat out: seconds left circling Leon (color_follow_time) before it flies home (-1: at its balloon)
    bool freed = false;         // out of a Monstermunch: on its way home, wandering (Game::freedStep)...
    float head = 0;             // ...its heading (radians)
};

// A Monstermunch ('M', after workshop/monstermunch.jpg): a face dug in the ground, 2x2 tiles, its
// open face the ground's -- in a floor, its top; in a ceiling, its bottom (upright on the screen
// turned round with Leon's gravity there), a red heart on it with its eyes in it (after
// workshop/monstermunch-love.jpg). Asleep, its eyes shut; Leon near, it wakes and watches him; within reach out of its face, it throws its tongue at him, drags him into its mouth,
// munches him, and chews on. Its mouth is a V cut into its face from the open side, pointing the
// tongue's way.
struct Monster {
    enum State { kSleep, kWatch, kThrow, kNumb, kPull, kMunch, kChew };
    Rectangle r{};                  // its tiles (world)
    int face = 0;                   // its open side, quarter turns from up: its own frame, turned by rotQ(face)
    Vector2 centre() const { return {r.x + r.width / 2, r.y + r.height / 2}; }
    Vector2 out() const { return rotQ(face, {0, -1}); }                     // out of its face (world)
    Vector2 toWorld(Vector2 l) const { return Vector2Add(centre(), rotQ(face, l)); }
    State st = kSleep;
    float t = 0;                    // seconds in this state
    float open = 0;                 // the mouth: 0 shut .. 1 wide
    Vector2 u{0, -1};               // where the mouth points, and the tongue goes (world, unit)
    float ext = 0;                  // the tongue's tip: this far from its middle, along u...
    std::vector<Vector2> bnc;       // ...and off ice, as a ball off a wall: where it bounced (world)...
    Vector2 tipU{};                 // ...and its way since the last bounce
    // The thrown tongue, e along its way from its middle: the tip -- `from`, `before`: where its last
    // stretch starts, and the length before it.
    Vector2 tongueAt(float e, Vector2* from = nullptr, float* before = nullptr) const {
        Vector2 p = centre();
        float b = 0;
        for (Vector2 q : bnc) {
            float L = Vector2Distance(p, q);
            if (e < b + L && L > 0.001f) return Vector2Lerp(p, q, (e - b) / L);    // (short of that bounce)
            b += L; p = q;
        }
        if (from) *from = p;
        if (before) *before = b;
        return Vector2Add(p, Vector2Scale(bnc.empty() ? u : tipU, e - b));
    }
    // A miss: its tongue numb (kNumb, t seconds of it) -- a limp chain out of its lips (world, [0] at
    // them), this long, swallowed back before it throws again.
    std::vector<Vector2> rope, ropePrev;
    float ropeLen = 0;
    float chewIn = 0, chewLeft = 0; // ...in chews: seconds till the next, tongue still to take in from the last
    // Twitches -- the left eye, the right eye: seconds into one (-1: none), and till the next (-1: to
    // be drawn); a blink, the same.
    float spasmT[2] = {-1, -1}, spasmIn[2] = {-1, -1};
    float blinkT = -1, blinkIn = -1;
    float rest = 0;                 // a miss: seconds before it throws again
    float numbT = 0;                // its tongue numb: seconds since (its colour turning)
    float standT = 0;               // Leon standing on its face: seconds of it
    Vector2 look{};                 // its pupils, towards Leon
    int munched = 0;                // the munches so far (then, chewing, the little ones)
    int tintCol = -1;               // a colour fly out of its mouth: its colour, and when (its white fading back)
    float tintAt = 0;
    unsigned seed = 1;              // its own dice: the game's draws stay what they were without it
    bool calmed = false;            // out of the zone round Leon: put to sleep, left so
    void calm() {                   // to sleep at once, mouth shut, tongue in -- nothing to see, no effects
        st = kSleep; t = 0; open = 0; ext = 0; u = out(); rest = 0; munched = 0; numbT = 0; standT = 0; bnc.clear();
        rope.clear(); ropePrev.clear(); ropeLen = 0; chewLeft = 0; look = {0, 0};
        spasmT[0] = spasmT[1] = -1; blinkT = -1;
    }
    float rnd(float a, float b) {   // (xorshift)
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        return a + (b - a) * (seed & 0xffffff) / (float)0xffffff;
    }
};
// Its face, in its own frame: a square of half-side hs round 0, its corners rounded (radius rc) --
// clockwise from the top straight edge's right end, so that the last edge is that top edge.
static std::vector<Vector2> roundedSquare(float hs, float rc) {
    std::vector<Vector2> p;
    const Vector2 c[4] = {{hs - rc, -(hs - rc)}, {hs - rc, hs - rc}, {-(hs - rc), hs - rc}, {-(hs - rc), -(hs - rc)}};
    const int seg = 5;
    for (int k = 0; k < 4; k++)
        for (int j = 0; j <= seg; j++) {
            float a = (-90 + 90 * k + 90.0f * j / seg) * DEG2RAD;
            p.push_back({c[k].x + rc * cosf(a), c[k].y + rc * sinf(a)});
        }
    return p;
}
// A path cut into n points evenly along it (its length: len).
static std::vector<Vector2> evenPoints(const std::vector<Vector2>& path, int n, float& len) {
    len = 0;
    for (size_t k = 0; k + 1 < path.size(); k++) len += Vector2Distance(path[k], path[k + 1]);
    std::vector<Vector2> out(n, path[0]);
    size_t k = 0;
    float at = 0;
    for (int i = 0; i < n; i++) {
        float want = len * i / (n - 1);
        while (k + 1 < path.size() && at + Vector2Distance(path[k], path[k + 1]) < want) { at += Vector2Distance(path[k], path[k + 1]); k++; }
        float seg = k + 1 < path.size() ? Vector2Distance(path[k], path[k + 1]) : 0;
        out[i] = k + 1 < path.size() && seg > 0.001f ? Vector2Lerp(path[k], path[k + 1], (want - at) / seg) : path.back();
    }
    return out;
}
// From 0, inside a convex shape, along d: where it leaves it.
static Vector2 rayExit(const std::vector<Vector2>& poly, Vector2 d) {
    float best = 1e9f;
    for (size_t i = 0; i < poly.size(); i++) {
        Vector2 a = poly[i], e = Vector2Subtract(poly[(i + 1) % poly.size()], a);
        float den = d.x * e.y - d.y * e.x;
        if (fabsf(den) < 1e-6f) continue;
        float t = (a.x * e.y - a.y * e.x) / den, s = (a.x * d.y - a.y * d.x) / den;
        if (t > 0 && s >= -1e-4f && s <= 1 + 1e-4f) best = fminf(best, t);
    }
    return Vector2Scale(d, best < 1e8f ? best : 0);
}
// Its mouth as drawn, in its own frame (its middle at 0, its open face up, at y = -half its side;
// its corners rounded, a tenth of its side): f, where the tongue's way (u) comes out of the open
// face; the V's two ends on that face -- wide open, the ends of its straight part; shut, closed on
// f -- and the V's point, the back of the mouth: always on the face's border opposite where it
// opens (along -u from the middle).
struct MouthV { Vector2 f, e1, e2, a; };
static MouthV mouthV(const Monster& m) {
    float hs = m.r.width / 2, rc = m.r.width / 10;
    Vector2 u = rotQ(-m.face, m.u);
    float kf = u.y < -1e-4f ? hs / -u.y : 1e9f;
    MouthV v;
    v.f = {clampf(u.x * kf, -hs + rc, hs - rc), -hs};
    v.e1 = Vector2Lerp(v.f, {-hs + rc, -hs}, m.open);
    v.e2 = Vector2Lerp(v.f, {hs - rc, -hs}, m.open);
    v.a = rayExit(roundedSquare(hs, rc), Vector2Negate(u));
    return v;
}
// A twitch, `s` seconds into it: a jerk shaking itself out (-1..1), over in 0.35 s.
static const float kSpasmTime = 0.35f;
static float spasmWave(float s) { return s < 0 || s > kSpasmTime ? 0 : sinf(2 * PI * 14 * s) * expf(-s / 0.08f); }

struct Game {
    Params params;
    Tune t;
    Level level;

    // Leon
    Vector2 pos{}, prevPos{}, vel{};
    bool onGround = false;
    float coyote = 0, jumpBuf = 0, dropTimer = 0, stun = 0;
    bool jumping = false;
    bool airJump = false;                     // the double jump is still available
    float spin = 0;                           // seconds left of the double-jump flip
    float facing = 1, walkPhase = 0;
    float groundSpeed = 0;                    // how fast he really moves along the ground
    float gait = 0, gaitAmt = 0;              // step cycle 0..1, and how much he is walking
    float gaitStride = 10, gaitStance = 0.7f; // this step: length, share of it a foot is down
    float fallT = 0;                          // seconds falling, nothing holding him (past fall_pedal_after: pedalling)
    float moveDir = 1;                        // which way he is really going
    float squash = 0, squashVel = 0;          // + squashed flat, - stretched tall
    float tilt = 0;                           // degrees
    float tiltVel = 0;                        // the lean is a spring on the ground
    float stepVelX = 0;                       // vel.x at the start of the step, for his acceleration
    bool traceLean = false;
    float blink = 2.0f;
    float landFlash = 0;                      // 0..1, legs splayed after a hard landing

    // Knocked out after bumping into an obstacle from below: no control until back up.
    //   Air   falling or bouncing, spinning as a square
    //   Roll  on the ground, rolling to a stop -- always on his bottom face, feet down
    //   Down  lying there, seeing stars
    //   Rise  legs unfolding, body bobbing up (no control yet); the rest of the get-up
    //         plays on in kNone, while he can already move
    enum Knock { kNone, kAir, kRoll, kDown, kRise };
    Knock knock = kNone;
    float knockAngle = 0, knockOmega = 0;     // degrees, degrees per second
    float knockTimer = 0;
    bool settling = false;                    // end of the roll: tumbling onto his bottom face
    float settleTo = 0;                       // ...this angle, a whole number of turns
    float getUp = 0;                          // seconds left of the whole get-up animation
    float dark = 0;                           // the dark air's black over the world, 0 to 1 (his eyes stay)
    // Suicide (pad: RT + LT + both sticks pushed in, held): seconds of trembling so far -- he
    // bursts at suicide_time; let go, it dies down. Then the pupils bouncing about in his eyes.
    bool suicideHeld = false, suicideLatch = false;   // (latched after a burst: let go first)
    float suicideT = 0;
    Vector2 pupil{}, pupilV{};
    float starsT = -1;                        // seconds since he started getting up, while the dizzy stars circle (-1 gone)
    float wakeT = -1;                         // seconds since the get-up ended, while the eyes blink (-1 done)
    bool wasRising = false;                   // getting up at the last step
    int bounces = 0;
    int rollFace = 0;                         // knocked out: which face of him is on the ground (2: his head)
    float fallTop = 0;                        // his highest point (his frame) since he last stood, hung or turned
    bool trace = false;                       // --trace: print knock-state changes
    Knock tracedKnock = kNone;

    // Command
    float keyCmd = 0, cmd = 0;
    bool jumpHeld = false, downHeld = false;
    float look = 0;                           // -1 looking up, +1 looking down (his frame)
    Vector2 farLook{};                        // the right stick: looking far, as far as it is pushed
                                              // (his frame; up/down or left/right, one at a time)
    Vector2 farReach{};                       // how far the camera may stray from its zone for it

    // GRAVITY. Leon's physics runs in his own frame, where gravity always pulls towards +y;
    // `grav` quarter turns (clockwise) take that frame to the world: 0 = gravity down,
    // 1 = left, 2 = up, 3 = right. pos, vel and cam are all in this frame. Touching a gravity
    // platform switches the frame at once (converting the state, so nothing moves in the
    // world), then the pull and the screen swing round to it over gravity_turn_time.
    int grav = 0;
    float turnFrom = 0, turnDelta = 0;        // screen angle (deg, unwrapped) and its change
    float turnStart = -1;                     // game time the turn began, -1 = none
    // A quarter turn begun on the ground: his box, turned at once, reaches less far towards that
    // floor (kLeonW / 2, not kLeonH / 2) and the swinging pull presses it there, while his drawing
    // turns later -- drawn as it was, his feet would sink in. Where his centre was and which way that
    // floor lies (world; {0, 0}: none): his drawing is lifted back by what the box has crept (turnLift).
    Vector2 turnC0{}, turnFloor{};

    // THE TONGUE -- all of it in WORLD coordinates, so a gravity turn leaves it in place.
    //   Out    flying straight from the mouth; sticks to the first obstacle on its line
    //   Stuck  a rope of fixed length from piv[0] (the sticking point) through the corners it
    //          has wrapped round (piv[1..]) to the mouth. Only a TAUT rope holds Leon.
    //   Back   being swallowed, as a whip
    // (tgDead: it bounced off ice and held nothing -- limp, hanging out of his mouth, till swallowed)
    enum Tongue { tgNone, tgOut, tgStuck, tgBack, tgDead };
    Tongue tg = tgNone;
    Vector2 tgDir{1, 0};
    std::vector<Vector2> tgBnc;               // a tongue in flight: where it bounced off ice (world); tgDir is its last stretch's
    float tgExt = 0, tgMax = 0, tgBackSpeed = 0;
    bool tgResend = false;
    Vector2 tgResendDir{1, 0};
    std::vector<Vector2> piv;
    std::vector<int> pivSide;                 // which way the rope bends round each corner
    float tgLen = 0;
    bool tgTaut = false;
    std::vector<Vector2> rope, ropePrev;      // the free part, last corner -> mouth (verlet)
    Vector2 prevCentreW{};
    Vector2 pullW{0, 1};                      // gravity's direction in the world, this step
    float mouth = 0;                          // 0 shut .. 1 wide open
    float whip = 0;                           // seconds into the swallow, for the whip wave
    bool tonguePress = false;
    bool chewPress = false;                   // RT (keyboard V) pulled: one chew, waiting for the step
    bool payHeld = false;                     // LT (keyboard C) held: pay the tongue out fast
    float chewLeft = 0;                       // tongue still to chew back
    float eyeBig = 0;                         // eyes popping while he chews the tongue back
    bool paying = false;                      // paying out fast right now
    bool pourOnSwallow = false;
    float payDrip = 0;
    float spitClock = 0;
    std::vector<Fly> flies;
    int caughtFly = -1;
    float flyChew = -1;                       // seconds into chewing a fly, -1 = not
    int chews = 0;
    int mouthFly = -1;                        // the fly being chewed: coughed back out, golden, when done...
    float mouthT = 0;                         // ...a lamp's fly not before this many seconds more in his belly, chewed or not (lamp_belly_time)
    Vector2 tongueAim{1, 0};                  // where he looks when B is pressed (his frame)

    // Camera: the point drawn at the centre of the screen, in Leon's frame
    Vector2 cam{}, prevCam{};
    float trauma = 0, time = 0;

    std::vector<Particle> parts;
    // Sounds asked for during a step -- played by the main loop (the game never touches audio).
    struct SoundEvent { Audio::Sfx id; float pitch, volume; };
    std::vector<SoundEvent> sounds;
    void sound(Audio::Sfx id, float pitch = 1.0f, float volume = 1.0f) {
        sounds.push_back({id, pitch * GetRandomValue(94, 106) / 100.0f, volume});
        if (trace) fprintf(stderr, "t=%.2f sound %d\n", time, (int)id);
    }
    std::vector<Crate> crates;
    int tgCrate = -1;                         // the tongue is stuck to this block...
    Vector2 tgCrateOff{};                     // ...at this point of it
    bool pulling = false;                     // dragging a block with the tongue
    bool pushing = false;                     // walking a block along in front of him
    bool dead = false;                        // burst into pieces
    float deadT = 0;
    std::vector<Piece> pieces;
    // Into a dark light ('@'): his middle at its own, he fades out in dark_light_fade seconds, and is
    // gone -- the world goes on without him (a restart brings him back).
    float goneT = -1;                         // seconds since he reached it (-1: not)
    bool gone() const { return goneT >= t.darkLightFade; }
    float lostT = -1;                         // out of the map: seconds since (the world fading to black, death_world_fade -- then his checkpoint; -1: not)
    // THE ENDING (workshop/ending (draft).png). On the tower's top -- the level's highest tiles --
    // Anna waits, his love and an almighty sorceress, on its last tile (no mouth yet); over its second,
    // an invisible ice wall (Level::endWall). Touching it, he is no longer his to move: he falls where
    // he is, onto the tower's top. From his landing: ending_smile_wait, and the two smile, each smile
    // shown from its middle out over ending_smile_time; hearts then, between them, for good
    // (heartsStep); ending_happy_wait on, the two happy -- eyes ^^, swaying, their brims a party horn's;
    // ending_pan_wait on, the scene slides left over ending_pan_time (as the right stick looking right),
    // Leon ending ending_pan_leon_from_left tiles from the screen's left; then the credits roll up the
    // right two thirds of the screen (assets/endcredits.txt) -- down held, ending_credits_fast times as
    // fast; ending_credits_wait after they are gone, the scene scrolls away left, on and on (as the
    // right stick looking right, with no bounds: ending_scroll_speed, reached over ending_scroll_ramp);
    // the tower, Anna and the hearts off the screen, it fades to black (ending_fade_time), scrolling
    // still -- the music on -- and a message: back to the title with A or Space (main).
    float endT = -1;                          // seconds since he touched the wall (-1: not yet)...
    float endLandT = -1;                      // ...since he landed back on the tower's top (-1: not yet)
    float creditsH = 0;                       // the credits' height, on a 720-line screen (main)
    bool creditsFast = false;                 // down held (main: the ending's own -- Leon looks nowhere)...
    float creditsK = 1;                       // ...the credits' speed now, times ending_credits_speed (easing to and from ending_credits_fast)...
    float creditsGain = 0;                    // ...and the seconds of rolling it has gained them
    float endOffT = -1;                       // endLandT as the scrolled scene went off the screen: the fade's start (-1: not yet)
    float musicT = -1;                        // seconds into the music playing (main, each frame; -1: not known)
    struct Heart { Vector2 p; float a, age; };   // where (world), its heading (radians), seconds old
    std::vector<Heart> hearts;
    float heartDue = 0;                       // hearts owed (ending_heart_rate)
    float endHeartsAt() const { return t.endSmileWait + t.endSmileTime; }   // (seconds from his landing)
    float endHappyAt() const { return endHeartsAt() + t.endHappyWait; }
    float endPanAt() const { return endHappyAt() + t.endPanWait; }
    float endCreditsAt() const { return endPanAt() + t.endPanTime; }
    float endCreditsLen() const { return (720 + creditsH) / t.endCreditsSpeed; }   // (seconds they roll, at their own speed)
    float endScrollAt() const { return endCreditsAt() + endCreditsLen() - creditsGain + t.endCreditsWait; }
    float endSmile() const { return endLandT < 0 ? 0 : clampf((endLandT - t.endSmileWait) / t.endSmileTime, 0, 1); }   // how much of the smiles shows
    // Landed, face to face, their bodies glow: from their own colours to the colours they would have
    // were they see-through, ending_glow_dim of the paper showing -- opaque all the same, nothing behind
    // showing -- and back, once every ending_glow_period seconds. How much of the paper's colour, now.
    float endGlow() const { return endLandT < 0 ? 0 : t.endGlowDim * 0.5f * (1 - cosf(2 * PI * endLandT / t.endGlowPeriod)); }
    bool endHappy() const { return endLandT >= 0 && endLandT >= endHappyAt(); }
    // Happy, the two sway left and right, together: ending_sway_amplitude each way (degrees),
    // ending_sway_frequency times a second -- their feet planted, their knees giving, outwards.
    float endSway() const { return endHappy() ? t.endSwayAmp * sinf(2 * PI * t.endSwayFreq * (endLandT - endHappyAt())) : 0; }
    // Happy, their brims -- his, hers -- as a party horn: uncurling (ending_brim_uncurl_time), curling
    // back (ending_brim_curl_time), at rest (ending_brim_pause), again; each move speeding up and
    // slowing down over ending_brim_ease of its time. 1: curled as drawn, 0: flat. With
    // ending_brim_music_sync, timed by the music (musicT) -- each uncurling starting on a bass note
    // of the piano, ending_brim_music_beat seconds into it and every cycle on -- from the first
    // after they are happy (none cut in on); no music known (music off, silent tests): by the clock.
    float endBrimCurl() const {
        if (!endHappy()) return 1;
        const float a = t.endBrimUncurl, b = t.endBrimCurl, cyc = a + b + t.endBrimPause;
        float ph = fmodf(endLandT - endHappyAt(), cyc);
        if (t.endBrimSync && musicT >= 0) {
            ph = fmodf(fmodf(musicT - t.endBrimBeat, cyc) + cyc, cyc);
            if (endLandT - endHappyAt() < ph) return 1;           // (this one began before: at rest till the next)
        }
        if (ph < a) return 1 - easeInOutLinear(ph / a, t.endBrimEase);
        if (ph < a + b) return easeInOutLinear((ph - a) / b, t.endBrimEase);
        return 1;
    }
    float endPan() const {                    // the slide left, eased: 0 -> 1
        float u = endLandT < 0 ? 0 : clampf((endLandT - endPanAt()) / t.endPanTime, 0, 1);
        return u * u * (3 - 2 * u);
    }
    float endCreditsT() const { return endLandT < 0 || endLandT < endCreditsAt() ? -1 : endLandT - endCreditsAt() + creditsGain; }   // seconds they have rolled, at their own speed (-1: not yet)
    // How far the scene has scrolled away (world pixels): speeding up evenly over ending_scroll_ramp, then on.
    float endScroll() const {
        float u = endLandT < 0 ? 0 : endLandT - endScrollAt(), v = t.endScrollSpeed * kTile, a = t.endScrollRamp;
        if (u <= 0) return 0;
        return u < a ? v * u * u / (2 * a) : v * (u - a / 2);
    }
    float endBlack() const { return endOffT < 0 ? 0 : clampf((endLandT - endOffT) / t.endFadeTime, 0, 1); }
    Vector2 annaC() const { Vector2 f = level.annaFeet(); return {f.x, f.y - (kLeonH + 2 - kBodyH / 2)}; }   // (the one waiting's body's middle: hers, his)
    // The hearts' middle -- between his middle and hers -- and the distance between the two.
    void heartFrame(Vector2& M, float& D) const {
        Vector2 L = centreW(), A = annaC();
        M = Vector2Lerp(L, A, 0.5f);
        D = fmaxf(kTile, Vector2Distance(L, A));
    }
    // The golden run -- after a dark light: the world as at the very start (every key back, every door
    // shut but a golden one open, the flags furled; Leon bare: no power, no hat anywhere), the start
    // its one checkpoint, and one clock, SOLO RUN (the golden run's name on the screen), from GO to the black
    // flag (then its highscores). Its way in (goldenSeq): black in golden_fade_out; the world put back (goldenStart);
    // the screen back in golden_fade_in; Leon, held in a dark light golden_light_height tiles over the
    // start, showing in golden_leon_fade_in -- the light in golden_light_fade_in -- then dropping;
    // landed, the light fades out (golden_light_fade_out) and, golden_countdown_wait later, main counts
    // 3, 2, 1, GO -- his controls his own again only then. A death in it: all of it again, from the black.
    bool goldenRun = false, goldenDone = false;
    float goldenTime = 0;
    float goldenSeq = -1;                     // seconds into its way in (-1: none)
    float goldenOutT = -1;                    // the start's light fading out: seconds since he landed (-1: not)
    float goldenLandAt = -1;                  // ...when he landed (goldenSeq's; -1: not yet)
    float goldenClockAt = -1;                 // when he was let go (game time): its clock fades in from then (golden_clock_fade_in)
    float goldKeyAt = -1;                     // the golden key first taken (game time): the modes' clocks fade out (golden_clocks_fade_out)
    float goldenShown() const { return t.goldenFadeOut + t.goldenFadeIn; }   // (the screen back: Leon starts showing)
    bool goldenHeld() const { return goldenSeq >= t.goldenFadeOut && goldenSeq < goldenShown() + t.goldenLeonIn; }
    float goldenBlack() const {
        if (goldenSeq < 0) return 0;
        if (goldenSeq < t.goldenFadeOut) return goldenSeq / t.goldenFadeOut;
        return 1 - clampf((goldenSeq - t.goldenFadeOut) / t.goldenFadeIn, 0, 1);
    }
    Vector2 goldenLightAt() const { return {level.spawn.x, level.spawn.y - kLeonH / 2 - t.goldenLightUp * kTile}; }
    float goldenLightA() const {
        if (goldenOutT >= 0) return 1 - clampf(goldenOutT / t.goldenLightOut, 0, 1);
        if (!goldenRun || goldenSeq < goldenShown()) return 0;
        return clampf((goldenSeq - goldenShown()) / t.goldenLightIn, 0, 1);
    }
    // How much of him shows: fading into a dark light, fading in out of the golden run's.
    float leonFade() const {
        if (goneT >= 0) return 1 - clampf(goneT / t.darkLightFade, 0, 1);
        if (goldenSeq >= 0) return clampf((goldenSeq - goldenShown()) / t.goldenLeonIn, 0, 1);
        return 1;
    }
    float fadeIn = 0;                         // after coming back
    // The Monstermunchs ('M'), and the one that has caught him (-1: none): dragged in, munched,
    // chewed -- then back at the checkpoint. Swallowed, he is not drawn.
    std::vector<Monster> monsters;
    int eatenBy = -1;
    bool swallowed() const { return eatenBy >= 0 && monsters[eatenBy].st >= Monster::kMunch; }
    // His tongue, out as it caught him -- numb, or stuck -- goes down with him: a limp chain from his
    // mouth (from its lips, once he is in), [0] there, swallowed a share at each munch, all of it by
    // the last. Stuck, its far end first holds against kTongueHolds munches, then gives way. (In
    // flight, it is gone at once.)
    std::vector<Vector2> eatRope, eatRopePrev;
    float eatRopeLen = 0, eatChewLeft = 0, eatChewRate = 0;
    int eatHolds = 0;                         // munches it still holds against, its far end pinned at eatAnchor
    Vector2 eatAnchor{};
    int startPart = 0;                        // start_level: the run started at this mode's key (0: at the start)
    bool onTitle = false;                     // the title up: the level as at its start, whatever start_level (main)
    // Who plays: Anna (the title's choice, --anna; main) -- Leon waiting for her on the tower's top --,
    // else Leon, as ever. She is him but for her drawing, her hat and her dash.
    bool anna = false;
    // The end's words (drawEnd; main, as the game starts): 0 "Game Over" -- 1 Anna's game, the first
    // time: "GAME 100% OVER" --, 2 the two finished before: "GAME OVER (AGAIN)"; one heart a side more each.
    int endMsg = 0;
    bool eyesHeld = false;                    // 3, 2, 1: his eyes shut till GO -- in a golden run, wide open, no blink (main)
    bool fromSuicide = false;                 // this death his own doing: back at the start of the mode he was in...
    bool wantCountdown = false;               // ...then 3, 2, 1, GO (main)
    bool timeUp = false, timeUpOver = false;  // a clock's hour run out: he explodes, then the title (main)
    // The hat, knocked off by a Monstermunch or the first bend of the Solo pit: falling with his gravity, bouncing,
    // rolling, then lying where it ends up, upright -- his again when he touches it, or when his
    // tongue catches it: on its tip, brought back, then curled up over his head to put it on.
    bool hatLoose = false;
    Vector2 hatP{}, hatV{};
    float hatA = 0, hatW = 0;                 // its turn (degrees) and spin (degrees a second)
    bool hatOnTongue = false;                 // caught on his tongue's tip
    float hatPutT = -1;                       // ...being put back on his head: seconds into it (-1: not yet)
    Vector2 hatPut0{};                        // ...from where the tip was then (from his middle)
    // Knocked off as he was caught, a colour fly in his belly: the hat keeps its colour -- tint and
    // clouds -- as long as the fly had left in there (hatKeepT; -1 for ever, color_belly_time 0),
    // or till it is on his head again (his colour then).
    int hatKeep = -1;
    float hatKeepT = 0;
    int hatColour() const { return !hasHat && hatKeep >= 0 ? hatKeep : bellyColour(); }
    // What he had as each mode started (0 easy: the level's start; the others: their keys) -- his
    // powers (double jump, tongue, hat), the fruits eaten and the pineapples lit: a suicide's restart
    // puts them back so -- the pineapples lit since grey again, to be eaten again.
    struct Powers { bool saved = false, canDouble = false, canTongue = false, hasHat = false, hatTaken = false;
                    std::vector<bool> fruitEaten, pineEaten; std::vector<int> pineGrav; };
    Powers modeStart[6];                      // (each mode's -- and the golden run's)
    void notePowers(int k) {
        Powers& p = modeStart[k];
        p.saved = true; p.canDouble = canDouble; p.canTongue = canTongue; p.hasHat = hasHat; p.hatTaken = hatTaken; p.fruitEaten = fruitEaten;
        p.pineEaten = pineEaten; p.pineGrav = pineGrav;
    }
    void restorePowers(int k) {
        const Powers& p = modeStart[k];
        if (!p.saved) return;
        canDouble = p.canDouble; canTongue = p.canTongue;
        fruitEaten = p.fruitEaten;
        int back = 0;                                                          // (the flags' and keys' checkpoints: as they are)
        for (size_t i = 0; i < pineEaten.size() && i < p.pineEaten.size(); i++)
            if (level.pineapple(i) && pineEaten[i] && !p.pineEaten[i]) { pineEaten[i] = false; pineLitT[i] = -1; pineGrav[i] = p.pineGrav[i]; back++; }
        if (p.hasHat) { hasHat = hatTaken = true; hatLoose = false; }             // (worn then: on his head again)
        else if (!p.hatTaken) { hasHat = hatTaken = false; hatLoose = false; }    // (not taken then: back at its spot)
        else hasHat = false;                                                   // (lost then: lying where it is)
        hatOnTongue = false; hatPutT = -1;
        hatT = hatBlinkT = -1;
        if (trace) fprintf(stderr, "t=%.2f powers back as mode %d started: double jump %d, tongue %d, hat %d, %d pineapple(s) grey again\n", time, k,
                           canDouble ? 1 : 0, canTongue ? 1 : 0, hasHat ? 1 : 0, back);
    }
    // Things are kept going only within two screens (the wider way) of Leon: farther, they are left
    // as they are -- a Monstermunch put back to sleep, its tongue in.
    bool inZone(Vector2 p) const {
        Vector2 vs = viewSize();
        float R = 2 * fmaxf(vs.x, vs.y);
        return Vector2DistanceSqr(p, centreW()) < R * R;
    }
    // The clocks: 0 easy mode's (the run's), 1 hard, 2 dark, 3 harder, 4 cameleon, 5 the golden run's
    // -- and which run. (In a golden run, its own only: from GO, once he has landed, to the golden key.)
    float clockOf(int k) const { return k == 0 ? runTime : k == 1 ? hardTime : k == 2 ? darkTime : k == 3 ? harderTime : k == 4 ? camTime : goldenTime; }
    bool clockRuns(int k) const {
        if (goldenRun) return k == 5 && !goldenDone && goldenSeq < 0;
        return k == 0 ? !finished : k == 1 ? hardMode && !hardDone : k == 2 ? darkMode && !darkDone : k == 3 ? harderMode && !harderDone
             : k == 4 ? camMode && !camDone : false;
    }
    // Checkpoints: where Leon comes back after dark matter -- the level's start, or the last
    // pineapple eaten (its very spot, even in mid-air, and the gravity he had then).
    std::vector<bool> pineEaten;
    std::vector<float> pineLitT;              // ...when each was lit (game time; -1: not lit here)
    std::vector<bool> pineGone;               // hard mode: taken away
    // His stage: the latest of the keys he has taken, in their order -- red 1 (hard mode), green 2
    // (dark), blue 3 (harder), purple 4 (cameleon); none 0 (easy). (Debug mode hands them out in
    // any order: the latest counts.)
    int stage() const {
        auto any = [](const std::vector<bool>& v) { return std::find(v.begin(), v.end(), true) != v.end(); };
        return any(purpleKeyTaken) ? 4 : any(blueKeyTaken) ? 3 : any(greenKeyTaken) ? 2 : completed ? 1 : 0;
    }
    // A checkpoint gone: taken away by hard mode -- or, from the green key on, a pineapple of an
    // earlier stage never lit; or any pineapple, in a golden run (the start its one checkpoint); or, in
    // easy mode, the one by the tongue fruit (hard mode's: there only from the red key on): never
    // shown, never pointed to, not to be eaten.
    bool pineAway(size_t i) const {
        int s = stage();
        return (goldenRun && level.pineapple(i)) || pineGone[i] || (s >= 2 && level.pineapple(i) && !pineEaten[i] && level.pineStage[i] < s) ||
               (s == 0 && (int)i == level.tongueFruitPine);
    }
    // The apple and its halo fade over the two seconds after pickup. The respawn point stays active.
    float pineShow(size_t i) const {
        if (!pineEaten[i] || pineLitT[i] < 0) return 1;
        return 1 - clampf((time - pineLitT[i]) / 2, 0, 1);
    }
    std::vector<int> pineGrav;                // the gravity of each one's first touch (-1: never touched)
    int cpPine = -1;                          // the one he comes back to (-1: the start)
    int caughtPine = -1;
    Vector2 cpFeet{};
    int cpGrav = 0;
    float cpMsg = 0;                          // 'CHECKPOINT' on screen
    // Abilities: a level with a double-jump or tongue fruit starts without that ability.
    bool canDouble = true, canTongue = true;
    std::vector<bool> fruitEaten;
    std::vector<bool> cherryEaten;            // the double cherries: a lesson each, nothing more
    // The hat: taken from its spot, worn for good. A bump on the head folds it up instead of
    // knocking him out; folded or unfolding (hatT seconds after the bump), it protects no more.
    // (hatT runs on while the brim springs back, the crown whole again.)
    bool hatTaken = false, hasHat = false;
    float hatT = -1;
    float hatBlinkT = -1;                     // seconds since the hat took a bump, while the eyes blink (-1 done)
    float hatRoom = 1e9f;                     // the room above his head (a ceiling crushes the hat)
    float hatFrom = 1;                        // how tall the crown was as it took the bump
    // Anna's dash (LB or RB, either Shift): the way she faces, along the ground's line -- wound up first
    // (anna_dash_windup: still, knees bent, leaning over), then anna_dash_distance tiles in
    // anna_dash_time, gravity held off, nothing able to hurt her meanwhile; a wall stops it -- she
    // blinks, no more; then braking (anna_dash_recover).
    float dashPrepT = -1;                     // seconds into the wind-up (-1: none)
    float dashT = -1;                         // seconds into the dash itself (-1: none)
    float dashDir = 0;                        // its way, in her frame: -1 left, 1 right
    float dashGone = 0;                       // how far it has gone
    float dashOutT = -1;                      // seconds since it ended, while the blur goes and she brakes (-1: none)
    float dashWait = 0;                       // seconds before the next one (anna_dash_cooldown)
    int dashAirUsed = 0;                      // dashes since she last stood
    bool dashPress = false;                   // a bumper press or a Shift tap, waiting for the step
    bool shiftDashPending = false;            // Shift pressed in this life, waiting for release
    float hatSideL = 1e9f, hatSideR = 1e9f;   // her brim's room aside, to a wall (her frame, from her middle)
    float hatLeanDir = -1;                    // which way her cone bends under a ceiling: back from where she goes (-1 left, 1 right; swinging over)
    // Anna's grape: the dash's lesson, 4 tiles left of where she starts (her game only; she dashes from the start).
    bool grapeEaten = false;
    bool grapeAvailable() const { return anna && !goldenRun && !grapeEaten; }
    Vector2 grapeAt() const { return {level.spawn.x - 4 * kTile, level.spawn.y - kTile / 2}; }
    float abilityMsg = 0;
    const char* abilityText = "";
    const char* lessonText = nullptr;         // an ability's message ('Double-jump !', 'Hat !')...
    const char* lessonWord = nullptr;         // ...a word of it in colour ('Red key !')...
    Color lessonCol = kInk;
    // ...under it, smaller, how -- in colours, marked in the text: {i} ink, {r} red, {o} orange, {n} brown, {v} violet, {b} blue,
    // and {A} / {B} a pad's button in its circle (A green, B red)...
    const char* lessonSub = nullptr;
    float lessonT = -1;                       // ...and how long it has been up (-1: none)
    void lesson(const char* text, const char* word = nullptr, Color col = kInk) {
        lessonText = text; lessonWord = word; lessonCol = col; lessonT = 0;
        lessonSub = nullptr;
    }
    void lessonHow(const char* text, const char* sub) {
        lesson(text);
        lessonSub = sub;
    }
    bool completed = false;                   // the exit's red key taken
    bool hasKey = false;                      // holding a key -- that one, or start_with_key's -- for a door
    std::vector<bool> blueKeyTaken;           // the blue keys ('K')...
    bool hasBlueKey = false;                  // ...one held, for a blue door ('l')
    std::vector<bool> greenKeyTaken;          // the green keys ('k')...
    bool hasGreenKey = false;                 // ...one held, for a green door ('g')
    std::vector<bool> purpleKeyTaken;         // the purple keys ('p')...
    bool hasPurpleKey = false;                // ...one held, for a purple door ('q')
    std::vector<bool> goldKeyTaken;           // the golden keys ('o')...
    bool hasGoldKey = false;                  // ...one held, for a golden door ('O')
    int lastKey = -1;                         // the last key taken (or given, cheat mode): 0 red, 1 blue, 2 green, 3 violet, 4 golden (-1 none) -- the hat's bands its colour
    bool helped = false;                      // a debug key used (toggle()): no highscore from then on, this run
    bool cheats = false;                      // the cheat code typed (main): cheat mode, till the game quits -- a restart too
    // Debug mode: cheat mode, or a key or a power given -- a debug key, a start_with_*, start_level
    // past easy mode: the run's times are not fair (no highscore), and DEBUG MODE shows (drawHud).
    bool debugMode() const {
        return cheats || helped || startPart > 0 || t.startKey || t.startBlueKey || t.startGreenKey || t.startHat || t.startTongue || t.startDouble;
    }
    // GOD (debug mode, Shift+G): golden, glowing in the dark three golden flies' belly light, his pupils
    // twice as big; Monstermunchs' tongues bounce off him as off ice (none eats him, standing on its face
    // either); dark matter is ground to him -- to walk on, to stick his tongue to. Off again: where he is,
    // his checkpoint.
    bool god = false;
    void toggleGod() {
        god = !god;
        if (!god) { cpFeet = toWorld(pos); cpGrav = grav; cpMsg = t.checkpointShow; }
        if (trace) fprintf(stderr, "t=%.2f GOD %s\n", time, god ? "on" : "off: his checkpoint here");
    }
    float flagT[flCount] = {-1, -1, -1, -1, -1};   // each flag: seconds since it went up (-1: furled)
    bool flagTouched[flCount] = {};           // ...and touched yet, up or not (its mode never run: furled still)
    float blackFlagT = -1;                    // the black flag: seconds since it went up -- the golden run done by it (-1: furled)...
    bool blackFlagTouched = false;            // ...and touched yet (its highscores show by it)
    // His own colour, with no colour fly in him (one in him tints him as ever, from white): white at
    // first; through a golden door, open, the door's -- the Solo door, gold (leon_color_solo), the Love
    // door, pastel pink (leon_color_love) --, changing to it over leon_color_change_time. Nothing else
    // changes it (a golden run's start, a death: kept), till the game starts over. No glow.
    int own = 0;                              // 0 white, 1 gold (the Solo door), 2 pastel pink (the Love door)...
    Color ownFrom = kBody;                    // ...changing from this colour...
    float ownPinkFrom = 0;                    // ...his hat's pink from this much (hatPink)...
    float ownT = -1;                          // ...for this many seconds (-1: no change going on)
    Color ownColour(int k) const { return k == 1 ? t.leonSolo : k == 2 ? t.leonLove : kBody; }   // (Anna playing: the same)
    Color bodyColour() const {
        Color to = ownColour(own);
        return ownT >= 0 && ownT < t.leonColorTime ? ColorLerp(ownFrom, to, ownT / t.leonColorTime) : to;
    }
    // His hat, as his own colour goes: through the Love door, pink all over -- its white and its bands
    // alike, no stripes; the Solo door's gold, not (as ever). How pink, 0 to 1, changing with him.
    float hatPink() const {
        float to = own == 2 ? 1.0f : 0.0f;
        return ownT >= 0 && ownT < t.leonColorTime ? lerpf(ownPinkFrom, to, ownT / t.leonColorTime) : to;
    }
    Color hatWhite() const { return ColorLerp(kBody, t.leonLove, hatPink()); }
    // Each door: seconds since the key went into it (-1: locked). It turns gold, still shut, over
    // door_gold_time; then it is open -- its tiles blanked -- and fades out over door_fade_time.
    std::vector<float> doorT;
    // Easy mode: from the start, on the run's clock, till the yellow flag (with none, the red key).
    // Hard mode: from the red key, the way back to the red flag (with none, to the start) -- past
    // where the checkpoints were.
    bool hardMode = false, hardDone = false;
    float hardTime = 0;                       // its own clock
    // Dark mode: from the green key, down into the dark, on its own clock, till the green flag.
    bool darkMode = false, darkDone = false;
    float darkTime = 0;
    // Harder mode: from the blue key, on its own clock, till the blue flag.
    bool harderMode = false, harderDone = false;
    float harderTime = 0;
    // Cameleon mode: from the purple key, on its own clock, till the violet flag.
    bool camMode = false, camDone = false;
    float camTime = 0;
    // Parts done, their times waiting to be weighed against the highscores (0 easy, 1 hard, 2 dark,
    // 3 harder).
    std::vector<std::pair<int, float>> partsDone;
    int musicPart = 0;                        // the music wanted: 0 the level's, 1 from the red flag up, 2 from the blue flag, 3 none (the violet flag up)
    // After a golden key, the music is the latest of these: a golden key taken, none (3: it fades out); a
    // golden door open, its own -- the left one's (4), the right one's (5). -1: none of it yet.
    int goldMusic = -1;
    int goldDoorRank(size_t i) const {        // a golden door's rank in map order: 0 the left one (Love door), 1 the right one (Solo door)
        int r = 0;
        for (size_t j = 0; j < i; j++) if (level.doorChar[j] == 'O') r++;
        return r;
    }
    int goldDoorMusic(size_t i) const { return goldDoorRank(i) == 0 ? 4 : 5; }
    bool goldTaken() const {                  // a golden key taken -- or a golden run: the game's end, from then on
        return goldenRun || std::find(goldKeyTaken.begin(), goldKeyTaken.end(), true) != goldKeyTaken.end();
    }
    // The music playing: musicPart's -- till a golden key (goldMusic); a golden run, none of its own.
    int music() const { return goldMusic >= 0 ? goldMusic : goldenRun ? 3 : musicPart; }
    // Each lamp post's dim light, a faulty bulb: dark until lampOffTill, the next blackout at lampNextOff.
    std::vector<float> lampOffTill, lampNextOff;
    bool lampPostOn(size_t k) const { return k >= lampOffTill.size() || time >= lampOffTill[k]; }
    void lampFlicker() {
        if (lampNextOff.size() != level.lamps.size()) {          // (a fresh level: each its own rhythm)
            lampOffTill.assign(level.lamps.size(), -1);
            lampNextOff.assign(level.lamps.size(), 0);
            for (float& n : lampNextOff) n = time + GetRandomValue(0, 1000) / 1000.0f / t.lampBlackMin;
        }
        for (size_t k = 0; k < lampNextOff.size(); k++)
            if (time >= lampNextOff[k] && inZone(lampTop(k))) {  // out, a moment -- then the next one, 1/max to 1/min s on
                lampOffTill[k] = time + t.lampBlackTime;
                float u = GetRandomValue(0, 1000) / 1000.0f;
                lampNextOff[k] = time + lerpf(1 / t.lampBlackMax, 1 / t.lampBlackMin, u);
            }
    }
    // The colour poles (workshop/colorpole.jpg): each one grows a balloon of its colour -- a real
    // balloon's shape, longer than wide, upright against gravity, paler as it swells
    // (color_balloon_*), pushed over by a wall in its way -- that bursts: its gas thrown out of its
    // middle in puffs, disks of the colour paler still, spreading, piling up on what they meet, then
    // evaporating (color_gas_*). And it grows another, color_balloon_regrow_delay later.
    struct Gas {
        int colour = 0;
        float t = 0;                          // seconds since the burst
        std::vector<Vector2> p, v;            // each puff: its middle, its speed...
        std::vector<float> r, life;           // ...how big it gets, and how much of color_gas_time it lasts
    };
    std::vector<float> balloonT;              // each pole's balloon: seconds it has been growing (below 0: burst, none yet -- color_balloon_regrow_delay)...
    std::vector<float> balloonTilt;           // ...how far a wall has pushed it over, round its knot (radians)...
    std::vector<float> balloonSwing;          // ...and its swing on the knot: the phase of the oscillation a fresh balloon has (radians)
    std::vector<Gas> gases;
    unsigned gasSeed = 7;                     // the gas's own dice: the game's draws stay what they were without it
    float gasRnd(float a, float b) {          // (xorshift)
        gasSeed ^= gasSeed << 13; gasSeed ^= gasSeed >> 17; gasSeed ^= gasSeed << 5;
        return a + (b - a) * (gasSeed & 0xffffff) / (float)0xffffff;
    }
    int bellyFly = -1;                        // a colour fly swallowed: in his belly (he glows its colour)...
    float bellyT = 0;                         // ...this many seconds more (color_belly_time; 0 there: for ever)
    float leonShow = 1;                       // how much of him shows, all told, in a cloud (color_hide*): his glow fades so
    bool hidden = false;                      // a colour fly in him, in a cloud of its colour: the Monstermunchs cannot see him
                                              // (a cloud clearing: only color_gas_hide_time more -- then not hidden enough)
    static constexpr float kBalloonLong = 1.25f;   // a balloon's half length: its half width (balloonR) times this
    Vector2 poleTop(size_t k) const { return Vector2Add(level.poles[k], Vector2Scale(level.poleDir[k], 2 * kTile)); }
    // color_balloon_sync: the balloons burst by colours -- all the cyan ones at once, then all the magenta,
    // then all the yellow -- each colour as the last one's clouds start to evaporate: their first puffs,
    // this long after they burst (half the shortest puff's life, kGasLifeMin of color_gas_time).
    static constexpr float kGasLifeMin = 0.7f;
    float balloonTurn() const { return 0.5f * kGasLifeMin * t.gasTime; }
    // The balloons from the start, no gas: each at its own stage (they never burst all at once) -- or,
    // color_balloon_sync, in turn, the cyan ones growing, the others waiting (`first`: that colour's
    // growing first instead -- color_balloon_respawn_first, back at one of its checkpoints). At the
    // game's start, and again at the cameleon part's: the purple key taken, or Leon back at one of its
    // checkpoints.
    void startBalloons(int first = 0) {
        balloonT.assign(level.poles.size(), 0);
        for (size_t k = 0; k < level.poles.size(); k++)
            balloonT[k] = t.balloonSync && level.poleTurn[k] >= 0 ? -((level.poleTurn[k] - first + 3) % 3) * balloonTurn()   // (in turn: the first growing, the others waiting)
                                                                  : t.balloonGrow * fmodf(0.37f * k, 1.0f);
        gases.clear();
        balloonTilt.assign(level.poles.size(), 0);
        balloonSwing.assign(level.poles.size(), 0);
        if (trace) fprintf(stderr, "t=%.2f the balloons from the start\n", time);
    }
    float balloonR(size_t k) const {          // its half width: from a bud to color_balloon_size (its width, tiles)
        float u = k < balloonT.size() ? clampf(balloonT[k] / t.balloonGrow, 0, 1) : 0;
        return lerpf(4, t.balloonSize * kTile / 2, u);
    }
    // A fresh balloon, still small, swings on its knot: a quick jiggle that slows as it swells
    // (color_balloon_swing_frequency, slower as the square of its width) and an amplitude
    // (color_balloon_swing_amplitude) dying away as it fills out.
    float balloonSwingAngle(size_t k) const {
        if (k >= balloonSwing.size()) return 0;
        float amp = t.balloonSwingAmp * DEG2RAD * expf(-fmaxf(0, balloonT[k]) / (0.35f * t.balloonGrow));
        return amp * cosf(balloonSwing[k]);
    }
    // Its axis, from its knot (the pole's top): up against gravity -- along its pole, if that hangs
    // the other way -- turned by a wall's push and by its swing.
    Vector2 balloonAxis(size_t k) const {
        Vector2 up = Vector2Negate(pullW), d = level.poleDir[k];
        if (Vector2DotProduct(up, d) < 0) up = d;
        return Vector2Rotate(up, (k < balloonTilt.size() ? balloonTilt[k] : 0) + balloonSwingAngle(k));
    }
    // Its middle: its foot on the knot -- the middle of its ink line (1.6 px, outside its edge) through
    // the knot's centre.
    Vector2 balloonC(size_t k) const { return Vector2Add(poleTop(k), Vector2Scale(balloonAxis(k), balloonR(k) * kBalloonLong + 0.8f)); }
    // A colour fly's pole: its fly's home -- just past the dot on its top, as a lamp's fly's.
    Vector2 flyHome(size_t k) const { return Vector2Add(poleTop(k), Vector2Scale(level.poleDir[k], 4)); }
    int bellyColour() const { return bellyFly >= 0 ? level.poleColour[flies[bellyFly].pole] : -1; }
    // Swinging on his stuck tongue (taut, in the air): how far his legs turn out from his body,
    // degrees -- drawn only. Out at each end of the swing, back at its bottom (sinusoidal: the share
    // of the swing's energy that is height), as far as 45 degrees for a swing a quarter turn wide,
    // less as it is less.
    float swingLegs() const {
        if (tg != tgStuck || !tgTaut || onGround || piv.empty()) return 0;
        Vector2 n = toLocal(Vector2Subtract(centreW(), piv.back()));
        float L = Vector2Length(n);
        if (L < 1) return 0;
        float th = atan2f(n.x, n.y);                     // (0: hanging straight down)
        float pe = t.gravity * L * (1 - cosf(th)), e = 0.5f * Vector2LengthSqr(vel) + pe;
        if (e < 1) return 0;
        float wide = acosf(clampf(1 - e / (t.gravity * L), -1, 1));   // (the swing's half width)
        return 45 * clampf(wide / (PI / 2), 0, 1) * pe / e;
    }
    // How far, and which way, the walls round a disk push it out of them ({0, 0}: none touches it).
    Vector2 wallPush(Vector2 c, float r) const {
        Vector2 push = {0, 0};
        int x0 = (int)floorf((c.x - r) / kTile), x1 = (int)floorf((c.x + r) / kTile);
        int y0 = (int)floorf((c.y - r) / kTile), y1 = (int)floorf((c.y + r) / kTile);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                Rectangle b;
                if (!tileBox(x, y, b)) continue;
                Vector2 d = Vector2Subtract(c, {clampf(c.x, b.x, b.x + b.width), clampf(c.y, b.y, b.y + b.height)});
                float L = Vector2Length(d);
                if (L < r && L > 0.001f) push = Vector2Add(push, Vector2Scale(d, (r - L) / L));
            }
        return push;
    }
    // A puff's radius now: growing as it spreads (easing out, color_gas_spread_time -- the sharper
    // for color_gas_boom: most of it in the first instants); over the last half of its life it
    // shrinks away. Where it is: puffAt, swirling slowly -- false once gone.
    float puffR(const Gas& gs, size_t i) const {
        float s = clampf(gs.t / t.gasSpread, 0, 1), e = 1 - powf(1 - s, 3 * t.gasBoom);
        float end = t.gasTime * gs.life[i], k = clampf((end - gs.t) / (0.5f * end), 0, 1);
        return gs.r[i] * lerpf(0.3f, 1, e) * sqrtf(k);
    }
    bool puffAt(const Gas& gs, size_t i, Vector2& p, float& r) const {
        r = puffR(gs, i);
        p = gs.p[i];
        p.x += 3 * sinf(gs.t * 0.9f + i * 1.7f);
        p.y += 3 * cosf(gs.t * 0.7f + i * 2.3f);
        return r > 0.5f;
    }
    // A cloud's opacity now (color_gas_opacity): full from the burst, then -- over the last
    // color_gas_fade of its life -- fading to nothing, so it is clear before it is gone.
    float gasOpacity(const Gas& gs) const {
        float out = clampf((t.gasTime - gs.t) / (t.gasFade * t.gasTime), 0, 1);
        return t.gasOpacity * out;
    }
    void polesStep(float dt) {
        if (balloonT.size() != level.poles.size()) balloonT.assign(level.poles.size(), 0);
        if (balloonTilt.size() != level.poles.size()) balloonTilt.assign(level.poles.size(), 0);
        if (balloonSwing.size() != level.poles.size()) balloonSwing.assign(level.poles.size(), 0);
        // (A puff thrown at d * k, slowed down by k, ends up about d away: k is drag at first --
        // color_gas_boom times it: as fast again, out in as little time -- then drag, as it settles.)
        float drag = 3 / t.gasSpread, boom = drag * t.gasBoom;
        for (size_t k = 0; k < level.poles.size(); k++) {
            if (level.poleFly[k]) continue;              // (a colour fly's pole: no balloon)
            // Its swing on the knot: the phase runs on, its frequency falling as the balloon swells
            // (a bigger balloon swings slower: as the square of its width).
            float f = t.balloonSwingFreq * powf(4.0f / fmaxf(4.0f, balloonR(k)), 2.0f);
            if (balloonT[k] >= 0) balloonSwing[k] += 2 * PI * f * dt;   // (none yet: the next starts fully deflected)
            // A wall in its way pushes the balloon over, round its knot -- its middle and its round
            // top kept out of it; free, it rights itself.
            balloonTilt[k] *= expf(-dt * 1.5f);
            for (int it = 0; it < 30; it++) {
                Vector2 a = balloonAxis(k), c = balloonC(k);
                float R = balloonR(k);
                Vector2 push = Vector2Add(wallPush(c, R), wallPush(Vector2Add(c, Vector2Scale(a, R * (kBalloonLong - 1))), R));
                if (Vector2Length(push) < 0.01f) break;
                balloonTilt[k] += Vector2DotProduct(push, {-a.y, a.x}) > 0 ? 0.02f : -0.02f;
            }
            if ((balloonT[k] += dt) < t.balloonGrow) continue;
            // Grown: it bursts. Its puffs fly out of its middle all round, slowing down.
            Gas gs;
            gs.colour = level.poleColour[k];
            Vector2 c = balloonC(k);
            const int n = 14;
            float reach = t.gasReach * kTile;
            for (int i = 0; i < n; i++) {
                float a = (i + gasRnd(-0.4f, 0.4f)) * 2 * PI / n;
                float r = reach * gasRnd(0.3f, 0.45f), d = (reach - r) * gasRnd(0.5f, 1.0f);
                gs.p.push_back(c);
                gs.v.push_back(Vector2Scale({cosf(a), sinf(a)}, d * boom));
                gs.r.push_back(r);
                gs.life.push_back(gasRnd(kGasLifeMin, 1.0f));
            }
            gases.push_back(gs);
            balloonT[k] = -(t.balloonSync ? fmaxf(0, 3 * balloonTurn() - t.balloonGrow)   // (its colour's turn again: the three colours' round over)
                                          : t.balloonRegrow);   // (the next one after color_balloon_regrow_delay)
            balloonSwing[k] = 0;                         // (the new balloon starts fully deflected, and swings)
            if (Vector2Distance(c, centreW()) < viewSize().x) sound(Audio::kRelease, 1.8f, 0.5f);   // (pop)
            if (trace) fprintf(stderr, "t=%.2f colour pole %d's balloon bursts\n", time, (int)k);
        }
        // A puff meeting a wall: slowed down by it, the deeper in the harder -- never pushed back
        // out -- and its middle cannot go in at all: there it stops. Gas piles up on surfaces --
        // fading out, it falls with gravity, settling on the ground.
        const float kGasPush = 10;                     // (its slowing: per second, per pixel it is in)
        for (Gas& gs : gases) {
            gs.t += dt;
            for (size_t i = 0; i < gs.p.size(); i++) {
                Vector2& p = gs.p[i];
                Vector2& v = gs.v[i];
                v = Vector2Scale(v, expf(-(gs.t < t.gasSpread ? boom : drag) * dt));
                if (gs.t > 0.5f * t.gasTime * gs.life[i]) {      // fading out: it falls (color_gas_gravity), jiggling (color_gas_brownian)
                    v = Vector2Add(v, Vector2Scale(pullW, t.gasGravity * t.gravity * dt));
                    if (t.gasBrownian > 0) {                     // (kicks at random: about that speed, held down by the drag)
                        float k = t.gasBrownian * sqrtf(2 * drag * dt) * 1.7320508f;
                        v.x += k * gasRnd(-1, 1); v.y += k * gasRnd(-1, 1);
                    }
                }
                Vector2 push = wallPush(p, puffR(gs, i));
                if (float L = Vector2Length(push); L > 0.01f) {
                    Vector2 n = Vector2Scale(push, 1 / L);
                    float vn = Vector2DotProduct(v, n);
                    if (vn < 0) v = Vector2Add(v, Vector2Scale(n, fminf(-vn, kGasPush * L * dt)));
                }
                Vector2 q = {p.x + v.x * dt, p.y};
                if (obstacleAt(q)) v.x = 0; else p.x = q.x;
                q = {p.x, p.y + v.y * dt};
                if (obstacleAt(q)) v.y = 0; else p.y = q.y;
            }
        }
        gases.erase(std::remove_if(gases.begin(), gases.end(), [&](const Gas& gs) { return gs.t >= t.gasTime; }), gases.end());
        // How much of Leon shows, all told -- for his glow (he himself fades where a cloud covers
        // him, when drawn): in a puff of the colour he glows, color_hide; with no colour fly in him,
        // in a puff of any colour, color_hide_plain; else all of him. (In a puff: when it covers the
        // middle of his cube -- legs aside -- give or take a quarter of it.)
        int bc = bellyColour();
        Vector2 cube = Vector2Subtract(centreW(), Vector2Scale(pullW, kLeonH / 2 + 2 - kBodyH / 2)), p;
        float r, leonTo = 1;
        hidden = false;
        for (const Gas& gs : gases)
            for (size_t i = 0; !dead && (bc < 0 || gs.colour == bc) && i < gs.p.size(); i++)
                if (puffAt(gs, i, p, r) && Vector2Distance(p, cube) < r + kBodyH / 4) {
                    leonTo = bc < 0 ? t.colorHidePlain : t.colorHide;
                    if (bc >= 0 && (t.gasHideTime < 0 || gs.t < t.gasTime * (1 - t.gasFade) + t.gasHideTime)) hidden = true;   // (clearing: color_gas_hide_time more)
                }
        leonShow += (leonTo - leonShow) * (1 - expf(-dt * 6));
    }
    void poleFlyStep(Fly& f, size_t i, float dt) {
        if (f.followT >= 0) {                            // spat out: circling Leon for color_follow_time, then home
            goldenStep(f, i, dt);
            if ((f.followT -= dt) < 0 && trace) fprintf(stderr, "t=%.2f colour fly %d flies home to pole %d\n", time, (int)i, f.pole);
            return;
        }
        f.orbit += dt * 2.4f;                            // home: round the dot on its pole's top, as a moth round a lamp
        Vector2 c = flyHome(f.pole);
        flyChase(f, {c.x + cosf(f.orbit) * 30, c.y + sinf(f.orbit) * 9 + 5 * sinf(time * 1.1f + i)}, dt);
    }
    // Its time in his belly over -- or another one swallowed: he spits the colour fly out; it circles
    // Leon for color_follow_time (0: not at all), then flies home to its pole.
    void spitBellyFly() {
        Fly& f = flies[bellyFly];
        bellyFly = -1;
        f.inMouth = false; f.caught = false; f.followT = t.colorFollow > 0 ? t.colorFollow : -1;
        Vector2 d = toWorld({facing, 0});
        f.p = Vector2Add(centreW(), Vector2Scale(d, 16));
        f.v = Vector2Add(Vector2Scale(d, 240), Vector2Scale(pullW, -90));   // shot out, a little up
        f.orbit = GetRandomValue(0, 628) / 100.0f;
        chomp(0.9f);
        squash = 0.15f; squashVel = 0;
        sound(Audio::kCough);
        if (trace) fprintf(stderr, "t=%.2f spat the colour fly out: round Leon %.1fs, then pole %d\n", time, t.colorFollow, f.pole);
    }
    int hardNext = 0;                         // the next of hardWaypoints() to go by
    int exitPine = -1, tonguePine = -1;       // the two checkpoints it keeps
    bool engrave = false;                     // the engraving, after Doré (params' look = engraving, Shift+S)
    std::vector<bool> taken;
    int got = 0;
    float runTime = 0;
    bool finished = false;
    float hintTimer = 9.0f;
    float idle = 0;                           // seconds since a control was last touched...
    float arrowA = 0;                         // ...and the guide arrow it brings (0..1)

    void reset() {
        pos = prevPos = level.spawn;
        vel = {0, 0};
        onGround = false; jumping = false; airJump = false; spin = 0;
        coyote = jumpBuf = dropTimer = stun = 0;
        squash = squashVel = tilt = 0;
        knock = kNone; knockAngle = knockOmega = knockTimer = getUp = 0; bounces = 0;
        starsT = wakeT = -1; wasRising = false; dark = 0;
        suicideT = 0; pupil = pupilV = {0, 0};
        grav = 0; turnFrom = turnDelta = 0; turnStart = -1;
        tg = tgNone; tgResend = false; piv.clear(); pivSide.clear(); rope.clear(); mouth = 0;
        chewLeft = 0; eyeBig = 0; caughtFly = -1; flyChew = -1;
        flies.clear();
        // Placed -- or, in a level that places none and has no lamps (whose flies it places too), anywhere.
        bool anywhere = level.flySpots.empty() && level.lamps.empty() && level.poles.empty();
        int nf = anywhere ? (int)t.flyCount : (int)level.flySpots.size();
        for (int i = 0; i < nf; i++) {
            Fly f; f.p = anywhere ? flySpot(false) : level.flySpots[i];
            f.dir = GetRandomValue(0, 1) ? 1 : -1;
            f.phase = GetRandomValue(0, 628) / 100.0f; flies.push_back(f);
        }
        for (size_t k = 0; k < level.lamps.size(); k++) {   // each lit lamp's golden fly, at its post
            if (!level.lampLit[k]) continue;
            Fly f; f.golden = true; f.lamp = f.home = (int)k; f.p = lampTop(k);
            f.phase = GetRandomValue(0, 628) / 100.0f; f.orbit = GetRandomValue(0, 628) / 100.0f;
            flies.push_back(f);
        }
        // The colour poles: their balloons from the start (startBalloons), each colour fly round its
        // pole's top. (No draws from the game's dice.)
        startBalloons();
        bellyFly = -1; bellyT = 0; leonShow = 1; hidden = false;
        for (size_t k = 0; k < level.poles.size(); k++) {
            if (!level.poleFly[k]) continue;             // (a balloon's pole: no fly)
            Fly f; f.pole = (int)k; f.p = flyHome(k);
            f.phase = 1.3f * k; f.orbit = 2.1f * k;
            flies.push_back(f);
        }
        cam = prevCam = {pos.x, pos.y - kLeonH / 2};
        trauma = 0;
        parts.clear();
        taken.assign(level.stars.size(), false);
        got = 0; runTime = 0; finished = false;
        crates.clear();
        float cs = kLeonDrawH * t.crateSize;
        for (auto& h : level.crates) { Crate cr; cr.c = cr.home = {h.x + cs / 2, h.y + cs / 2}; cr.v = {0, 0}; crates.push_back(cr); }
        tgCrate = -1; pulling = false;
        dead = false; deadT = 0; pieces.clear(); goneT = lostT = -1;
        goldenRun = goldenDone = false; goldenTime = 0; goldenSeq = goldenOutT = goldenLandAt = goldenClockAt = goldKeyAt = -1;
        own = 0; ownFrom = ownColour(0); ownPinkFrom = 0; ownT = -1;
        endT = endLandT = -1; hearts.clear(); heartDue = 0;      // (the ending: not yet)
        creditsK = 1; creditsGain = 0; endOffT = -1;
        lastKey = -1;
        pineEaten.assign(level.pines.size(), false);
        pineLitT.assign(level.pines.size(), -1);
        pineGone.assign(level.pines.size(), false);
        pineGrav.assign(level.pines.size(), -1);
        cpPine = -1;
        fruitEaten.assign(level.fruits.size(), false);
        cherryEaten.assign(level.cherries.size(), false);
        canDouble = canTongue = true;
        for (char k : level.fruitKind) { if (k == 'D') canDouble = t.startDouble; if (k == 'T') canTongue = t.startTongue; }
        abilityMsg = 0; completed = false;
        lessonText = lessonWord = lessonSub = nullptr; lessonT = -1;
        for (float& f : flagT) f = -1;
        blackFlagT = -1; blackFlagTouched = false;   // (a golden run's start too: furled again till its end)
        goldMusic = -1;                               // (a golden run's start: its golden door kept open gives it its music again)
        doorT.assign(level.doors.size(), -1);
        hasKey = t.startKey;
        blueKeyTaken.assign(level.blueKeys.size(), false); hasBlueKey = t.startBlueKey;
        greenKeyTaken.assign(level.greenKeys.size(), false); hasGreenKey = t.startGreenKey;
        purpleKeyTaken.assign(level.purpleKeys.size(), false); hasPurpleKey = false;
        goldKeyTaken.assign(level.goldKeys.size(), false); hasGoldKey = false;
        helped = false; fallT = 0;
        for (size_t i = 0; i < level.doors.size(); i++)        // shut again (if opened without a reload)
            setDoorTiles(level.doors[i], level.doorChar[i]);
        hardMode = hardDone = false; hardTime = 0; hardNext = 0; exitPine = tonguePine = -1;
        darkMode = darkDone = false; darkTime = 0; partsDone.clear(); musicPart = 0;
        harderMode = harderDone = false; harderTime = 0;
        camMode = camDone = false; camTime = 0;
        for (bool& f : flagTouched) f = false;
        lampOffTill.clear(); lampNextOff.clear();
        mouthFly = -1; fallTop = pos.y; farReach = {0, 0};
        hatTaken = hasHat = t.startHat; hatT = hatBlinkT = -1; hatRoom = 1e9f;   // (start_with_hat: the one from its spot)
        resetDash();
        hatSideL = hatSideR = 1e9f; hatLeanDir = -1;
        caughtPine = -1; cpFeet = level.spawn; cpGrav = 0; cpMsg = 0;
        monsters.clear();
        for (size_t i = 0; i < level.monsters.size(); i++) {
            Monster m;
            m.r = {level.monsters[i].x, level.monsters[i].y, 2 * kTile, 2 * kTile};
            m.face = level.monsterFace[i];
            m.u = m.out();
            m.seed = 2463534242u + 7919u * (unsigned)i;
            monsters.push_back(m);
        }
        eatenBy = -1;
        hatLoose = hatOnTongue = false; hatPutT = -1; fromSuicide = wantCountdown = timeUp = timeUpOver = false;
        // Anna: her hat, her tongue, her double jump from the start -- their fruits, and the lessons' (the
        // banana, the kaki, the cherries), not in her game; her grape instead (the dash's lesson).
        grapeEaten = false;
        if (anna) {
            hatTaken = hasHat = true;
            canDouble = canTongue = true;
            fruitEaten.assign(level.fruits.size(), true);
            cherryEaten.assign(level.cherries.size(), true);
        }
        startAtPart(onTitle ? 0 : t.startPart);
        for (Powers& p : modeStart) p = Powers{};
        notePowers(0);
        faceNextCheckpoint();
    }

    // start_level: the run starts at that mode's first checkpoint -- its key (easy mode: the start)
    // -- as a run that got there would have it: the double jump, the tongue, the hat; the doors of
    // the modes before open, their keys gone into them; the music of the place. The gravity: down,
    // unless there is no ground under the key within 12 tiles (the purple key, under a ceiling):
    // then towards the nearest ground. He takes the key at once: its mode starts, on its own clock.
    // (Like start_with_*: no highscore that run.) Part 5, the end of the game (cheat key P): at the
    // golden key, every mode's door open, in silence -- the flags furled, no mode run.
    void startAtPart(int part) {
        startPart = 0;
        Vector2 key{};
        int pine = -1;
        if (part == 1 && level.hasExit) { key = middle(level.exitRect); pine = level.exitKeyPine; }
        if (part == 2 && !level.greenKeys.empty()) { key = level.greenKeys[0]; pine = level.greenKeyPine[0]; }
        if (part == 3 && !level.blueKeys.empty()) { key = level.blueKeys[0]; pine = level.blueKeyPine[0]; }
        if (part == 4 && !level.purpleKeys.empty()) { key = level.purpleKeys[0]; pine = level.purpleKeyPine[0]; }
        if (part == 5 && !level.goldKeys.empty()) { key = level.goldKeys[0]; pine = level.goldKeyPine[0]; }
        if (pine < 0) return;                            // (easy mode -- or no such key here: the start)
        startPart = part;
        canDouble = canTongue = true;
        for (size_t i = 0; i < level.fruits.size(); i++) if (level.fruitKind[i] != 'U' && level.fruitKind[i] != 'Z') fruitEaten[i] = true;
        if (level.hasHatSpot) hatTaken = hasHat = true;
        finished = true;                                 // (easy mode: not run, no clock)
        completed = part > 1;                            // the red key: taken, if before
        static const char doorOf[5] = {0, 'L', 'g', 'l', 'q'};   // each mode's door, opened in it
        for (size_t i = 0; i < level.doors.size(); i++)
            for (int k = 1; k < part; k++)
                if (level.doorChar[i] == doorOf[k] && doorT[i] < 0) { doorT[i] = t.doorGold + t.doorFade; openDoor(i); }
        if (part > 2) greenKeyTaken.assign(greenKeyTaken.size(), true);
        if (part > 3) blueKeyTaken.assign(blueKeyTaken.size(), true);
        if (part > 4) purpleKeyTaken.assign(purpleKeyTaken.size(), true);
        musicPart = part >= 5 ? 3 : part >= 4 ? 2 : part >= 2 ? 1 : 0;   // (past the red flag, then past the blue one, then the violet)
        int x = (int)floorf(key.x / kTile), y = (int)floorf(key.y / kTile);
        auto ground = [&](int s) {                       // free tiles to the ground: below, left, above, right
            const int dx[4] = {0, -1, 0, 1}, dy[4] = {1, 0, -1, 0};
            for (int d = 0; d < 12; d++)
                if (level.block(x + dx[s] * (d + 1), y + dy[s] * (d + 1)) || level.oneway(x + dx[s] * (d + 1), y + dy[s] * (d + 1))) return d;
            return -1;
        };
        int gv = 0;
        if (ground(0) < 0)
            for (int s : {1, 3, 2})
                if (ground(s) >= 0 && (gv == 0 || ground(s) < ground(gv))) gv = s;
        pineGrav[pine] = gv;
        checkpoint(pine);
        respawn();
        fadeIn = 0;
        if (trace) fprintf(stderr, "t=%.2f start_level: mode %d, at its key (%.0f,%.0f), gravity frame %d\n", time, part, key.x, key.y, gv);
    }

    // The golden run's start (its way in, the screen black): the world as at the very start -- reset(),
    // start_level aside -- but for a golden door open, which stays open (the one the golden key went
    // into: the next golden key can only open the other); Leon bare, no power and no hat anywhere,
    // held in the dark light over the start, the screen on the start.
    void goldenStart() {
        std::vector<bool> keep(doorT.size(), false);
        for (size_t i = 0; i < doorT.size(); i++) keep[i] = level.doorChar[i] == 'O' && doorT[i] >= 0;
        float seq = goldenSeq;
        int part = t.startPart, wasOwn = own;
        Color wasFrom = ownFrom;
        float wasT = ownT, wasPinkFrom = ownPinkFrom;
        t.startPart = 0;
        reset();
        t.startPart = part;
        own = wasOwn; ownFrom = wasFrom; ownPinkFrom = wasPinkFrom; ownT = wasT;   // (his colour as it was: only a golden door changes it)
        for (size_t i = 0; i < doorT.size(); i++)
            if (keep[i]) { doorT[i] = t.doorGold + t.doorFade; openDoor(i); }
        hatTaken = true; hasHat = false; hatLoose = false;   // (not on him, not at its spot)
        // No golden fly at the lamps (the dark is dark), no word of advice (the banana, the kaki, the
        // double cherries gone; the double jump's and the tongue's fruits say nothing): he knows.
        flies.erase(std::remove_if(flies.begin(), flies.end(), [](const Fly& f) { return f.lamp >= 0 || f.home >= 0; }), flies.end());
        for (size_t i = 0; i < level.fruits.size(); i++) if (level.fruitKind[i] == 'U' || level.fruitKind[i] == 'Z') fruitEaten[i] = true;
        cherryEaten.assign(cherryEaten.size(), true);
        facing = 1;                                          // (facing right, whatever he faced)
        goldenRun = true; goldenSeq = seq;
        notePowers(5);
        Vector2 at = goldenLightAt();
        pos = prevPos = toLocal({at.x, at.y + kLeonH / 2});
        fallTop = pos.y;
        cam = prevCam = startCentre();
        if (trace) fprintf(stderr, "t=%.2f golden run: the world put back, Leon in the dark light over the start (%.0f,%.0f)\n", time, at.x, at.y);
    }

    // Back from dark matter: at the checkpoint, still, tongue in, the gravity he had there.
    // Turn to face where the guide arrow points (start of the level, and on coming back) -- at the
    // golden key, right: the way on, to its shaft (no arrow from there); at an 'a' pineapple, right.
    void faceNextCheckpoint() {
        if (std::find(level.goldKeyPine.begin(), level.goldKeyPine.end(), cpPine) != level.goldKeyPine.end()) { facing = 1; return; }
        if (cpPine >= 0 && cpPine < (int)level.pineRight.size() && level.pineRight[cpPine]) { facing = 1; return; }
        Vector2 to;
        if (!guideTarget(to)) return;
        float dx = toLocal(Vector2Subtract(to, centreW())).x;
        if (fabsf(dx) > 1) facing = signf(dx);
    }

    // The exit's key, floating at the middle of the 'E' tiles: up and down along gravity, by
    // key_float_amplitude of its height, key_float_frequency times a second.
    Vector2 keyPos() const { return keyBob(middle(level.exitRect)); }
    Vector2 keyBob(Vector2 at) const {                 // (a blue key floats the same way)
        float bob = sinf(2 * PI * t.keyFreq * time) * t.keyAmp * kKeyH;
        return Vector2Add(at, Vector2Scale(pullW, -bob));
    }
    // In dark air ('.'): how dark it is where he is -- black from dark_depth tiles below its top.
    float darkHere() const {
        Vector2 c = centreW();
        int x = (int)floorf(c.x / kTile), y = (int)floorf(c.y / kTile);
        // (a tile an item stood on -- a fruit, a key, a lamp -- is open, not dark air: next to some, it is dark too)
        if (!level.dark(x, y) && !level.dark(x - 1, y) && !level.dark(x + 1, y) && !level.dark(x, y - 1) && !level.dark(x, y + 1))
            return 0;
        return clampf((c.y - level.darkTop) / (t.darkDepth * kTile), 0, 1);
    }
    // The end of a mode -- easy at the yellow flag (with none, the red key), dark at the green flag
    // (with none, the green door), harder at the blue flag: its clock stops, if it ran, and its time
    // is weighed against the highscores.
    void endEasy() {
        if (finished) return;
        finished = true;
        partsDone.push_back({0, runTime});
        if (trace) fprintf(stderr, "t=%.2f easy mode completed in %.1f s\n", time, runTime);
    }
    void endDark() {
        if (!darkMode || darkDone) return;
        darkDone = true;
        partsDone.push_back({2, darkTime});
        if (trace) fprintf(stderr, "t=%.2f dark mode completed in %.1f s\n", time, darkTime);
    }
    void endHarder() {
        if (!harderMode || harderDone) return;
        harderDone = true;
        partsDone.push_back({3, harderTime});
        if (trace) fprintf(stderr, "t=%.2f harder mode completed in %.1f s\n", time, harderTime);
    }
    // A key taken: "<Its colour> key !" -- if the level has a door it opens -- and the last key taken.
    static int keyOfDoor(char door) { return door == 'l' ? 1 : door == 'g' ? 2 : door == 'q' ? 3 : door == 'O' ? 4 : 0; }
    void goToDoor(char door, const char* text, const char* word) {
        lastKey = keyOfDoor(door);
        if (std::find(level.doorChar.begin(), level.doorChar.end(), door) != level.doorChar.end()) lesson(text, word, t.keyCol[lastKey]);
    }
    // Taken (or given at the start: start_with_key), and not yet in a door's lock (a level without
    // doors: no key to hold).
    bool keyHeld() const { return hasKey && !level.doors.empty(); }
    // Cheat mode's keys (1, 2, 3, 4, 5, 8, 9, 0): the red key, the blue key, the green key, the violet
    // key, the golden key, the double jump, the tongue, the hat, each switched on or off at once -- and from then on
    // this run records no highscore (the flags still show their tables, no name asked).
    void toggle(int what) {
        const char* name = "";
        bool on = false;
        switch (what) {
            case 1: on = hasKey = !hasKey; name = "the red key"; break;
            case 2: on = hasBlueKey = !hasBlueKey; name = "the blue key"; break;
            case 3: on = hasGreenKey = !hasGreenKey; name = "the green key"; break;
            case 4: on = hasPurpleKey = !hasPurpleKey; name = "the violet key"; break;
            case 5: on = hasGoldKey = !hasGoldKey; name = "the golden key"; break;
            case 8:
                on = canDouble = !canDouble; name = "the double jump";
                lesson(on ? "Double-jump !" : "No double-jump");
                break;
            case 9:
                on = canTongue = !canTongue; name = "the tongue";
                if (!on && tg != tgNone && tg != tgBack) tongueRetract(t.tongueBack);   // (out: back in)
                lesson(on ? "Tongue !" : "No tongue");
                break;
            case 0:
                on = hasHat = !hasHat; name = "the hat";
                if (on) hatTaken = true;
                hatT = hatBlinkT = -1;
                lesson(on ? "Hat !" : "No hat");
                break;
            default: return;
        }
        if (on && what <= 5) lastKey = what - 1;          // (a key given: the last one taken)
        helped = true;
        sound(Audio::kFruit, on ? 1.1f : 0.8f);
        if (trace) fprintf(stderr, "t=%.2f debug: %s %s\n", time, name, on ? "on" : "off");
    }
    void setDoorTiles(const Rectangle& d, char c) {
        int x = (int)lroundf(d.x / kTile);
        for (int y = (int)lroundf(d.y / kTile); y < (int)lroundf((d.y + d.height) / kTile); y++)
            if (y >= 0 && y < level.h && x >= 0 && x < (int)level.rows[y].size()) level.rows[y][x] = c;
    }
    // Gold, the door opens: its tiles go, and a tongue stuck to it lets go.
    void openDoor(size_t i) {
        Rectangle d = level.doors[i];
        setDoorTiles(d, ' ');
        if (level.doorChar[i] == 'O') goldMusic = goldDoorMusic(i);   // (a golden door: its music)
        if (tg == tgStuck && tgCrate < 0 && !piv.empty() &&
            CheckCollisionPointRec(piv[0], {d.x - 2, d.y - 2, d.width + 4, d.height + 4})) tongueRetract(t.tongueBack);
        if (trace) fprintf(stderr, "t=%.2f door %d open\n", time, (int)i);
    }

    // Where the guide arrow points: the checkpoint after his own along the level's way (lit or
    // not -- level.pathOrder: the yellow flag's, then the red key's among them), then the exit. In
    // hard mode, the way back: the pineapples before the red key it keeps, in reverse (those taken
    // away -- easy mode's, done with -- never), then the red flag (or the start).
    Vector2 startCentre() const { return {level.spawn.x, level.spawn.y - kLeonH / 2}; }
    static Vector2 middle(Rectangle r) { return {r.x + r.width / 2, r.y + r.height / 2}; }
    Vector2 homeCentre() const { return level.flags[flRed].has ? middle(level.flags[flRed].rect) : startCentre(); }
    // Where part k's highscores show -- by its flag (easy mode without a yellow flag: the red key;
    // hard mode without a red flag: the start) -- and how near Leon is to it: 1 within two thirds of
    // goal_text_distance, fading to 0 at it (1 for a part with no such place). (The golden run's: by
    // the black flag -- a level without one: by the golden key.)
    bool partPlace(int k, Vector2& at) const {
        if (k == 5 && level.blackFlag.has) { at = middle(level.blackFlag.rect); return true; }
        if (k == 5) { if (level.goldKeys.empty()) return false; at = level.goldKeys[0]; return true; }
        static const int flagOf[5] = {flYellow, flRed, flGreen, flBlue, flViolet};
        const Level::Flag& f = level.flags[flagOf[k]];
        if (f.has) { at = middle(f.rect); return true; }
        if (k == 0 && level.hasExit) { at = middle(level.exitRect); return true; }
        if (k == 1) { at = startCentre(); return true; }
        return false;
    }
    float partNear(int k) const {
        Vector2 at;
        if (!partPlace(k, at)) return 1;
        float reach = t.goalTextDist, d = Vector2Distance(centreW(), at) / kTile;
        return clampf((reach - d) / (reach / 3.0f), 0, 1);
    }
    std::vector<Vector2> hardWaypoints() const {
        const std::vector<int>& o = level.pathOrder;
        int k = (int)o.size();
        for (int j = 0; j < (int)o.size(); j++) if (o[j] == exitPine) k = j;
        std::vector<Vector2> w;
        for (int j = k - 1; j >= 0; j--) if (level.pineapple(o[j]) && !pineGone[o[j]]) w.push_back(level.pines[o[j]]);
        w.push_back(homeCentre());
        return w;
    }
    bool guideTarget(Vector2& out) const {
        if (goldenRun || std::find(goldKeyTaken.begin(), goldKeyTaken.end(), true) != goldKeyTaken.end()) return false;   // (the golden key taken, a golden run: nowhere more)
        if (int s = stage(); s >= 2) {                // from the green key on: his stage's way only (Level::stageWay) --
            const std::vector<int>& way = level.stageWay[s];   // the checkpoint after his own on it, nothing past its end
            int k = 0;
            for (int j = 0; j < (int)way.size(); j++) if (way[j] == cpPine) k = j;
            if (k + 1 >= (int)way.size()) return false;
            out = level.pines[way[k + 1]];
            return true;
        }
        if (hardMode) {
            if (hardDone) return false;
            std::vector<Vector2> w = hardWaypoints();
            out = w[std::min(hardNext, (int)w.size() - 1)];
            return true;
        }
        const std::vector<int>& o = level.pathOrder;
        int k = -1;                                   // his checkpoint's place along the way (-1: the start)
        for (int j = 0; j < (int)o.size(); j++) if (o[j] == cpPine) k = j;
        // (Not on to those beyond a door before the exit -- unless he is past one already. One taken away:
        // on to the next.)
        bool past = k >= 0 && level.pineBeyond[o[k]];
        int n = k + 1;
        while (n < (int)o.size() && pineAway(o[n])) n++;
        if (n < (int)o.size() && (past || !level.pineBeyond[o[n]])) { out = level.pines[o[n]]; return true; }
        if (!level.hasExit || completed) return false;
        out = middle(level.exitRect);
        return true;
    }

    // The red key taken: hard mode. Only two checkpoints stay: the key's own, lit (he comes back
    // there), and the pineapple nearest the tongue fruit, grey again (to be eaten again) -- and, as
    // they were, those beyond a locked door (the way on, once back), and the flags' and keys'.
    int nearestPine(Vector2 p, bool pineapples = false) const {
        int best = -1;
        float bd = 0;
        for (size_t i = 0; i < level.pines.size(); i++) {
            if (pineapples && !level.pineapple(i)) continue;
            float d = Vector2DistanceSqr(p, level.pines[i]);
            if (best < 0 || d < bd) { best = (int)i; bd = d; }
        }
        return best;
    }
    void hardModePines() {
        exitPine = level.exitKeyPine;
        tonguePine = -1;
        for (size_t i = 0; i < level.fruits.size(); i++)
            if (level.fruitKind[i] == 'T') { tonguePine = nearestPine(level.fruits[i], true); break; }
        if (tonguePine == exitPine) tonguePine = -1;
    }
    void startHardMode() {
        hardMode = true; hardDone = false; hardTime = 0; hardNext = 0;
        hardModePines();
        for (size_t i = 0; i < level.pines.size(); i++) {
            pineGone[i] = level.pineapple(i) && (int)i != tonguePine && !level.pineBeyond[i];
            if (pineGone[i]) { pineEaten[i] = false; pineLitT[i] = -1; }
        }
        if (tonguePine >= 0) { pineEaten[tonguePine] = false; pineLitT[tonguePine] = -1; }   // (to be reached again: whole again)
        if (exitPine >= 0) {
            pineEaten[exitPine] = true;
            cpPine = exitPine;
            if (pineGrav[exitPine] < 0) pineGrav[exitPine] = grav;   // (never touched: as he is now)
            cpGrav = pineGrav[exitPine];
            cpFeet = Vector2Add(level.pines[exitPine], rotQ(cpGrav, {0, kLeonH / 2}));
        }
        if (trace) fprintf(stderr, "t=%.2f hard mode: back to the %s (kept: checkpoint %d lit, %d grey)\n", time,
                           level.flags[flRed].has ? "red flag" : "start", exitPine, tonguePine);
    }

    void respawn() {
        if (goldenRun && !goldenDone) { cpPine = -1; cpFeet = level.spawn; cpGrav = 0; }   // (a golden run: the start, whatever was touched since -- done: the black flag)
        grav = cpGrav; turnFrom = cpGrav * 90.0f; turnDelta = 0; turnStart = -1;
        pos = prevPos = toLocal(cpFeet);
        fallTop = pos.y; farReach = {0, 0};
        hatT = hatBlinkT = -1;                        // the hat comes back whole
        if (hatLoose && t.hatRespawn) {               // ...and, lost (knocked off as he was eaten), on his head again (hat_back_on_respawn)
            hatLoose = false; hasHat = true;
            if (trace) fprintf(stderr, "t=%.2f his lost hat back on his head\n", time);
        }
        else if (hatLoose && trace) fprintf(stderr, "t=%.2f his lost hat stays where it fell (%.0f,%.0f)\n", time, hatP.x, hatP.y);   // (spareHatStep)
        vel = {0, 0};
        onGround = false; jumping = false; airJump = false; spin = 0;
        coyote = jumpBuf = stun = 0;
        squash = squashVel = tilt = tiltVel = 0;
        knock = kNone; getUp = 0;
        starsT = wakeT = -1; wasRising = false;
        suicideT = 0; pupil = pupilV = {0, 0};
        tg = tgNone; tgCrate = -1; pulling = false; pushing = false; piv.clear(); pivSide.clear(); rope.clear(); mouth = 0;
        chewLeft = 0; caughtFly = -1; caughtPine = -1; flyChew = -1; hatOnTongue = false; hatPutT = -1;
        eatRope.clear(); eatRopePrev.clear(); eatRopeLen = eatChewLeft = 0; eatHolds = 0;
        for (Fly& f : flies) {                        // the lamps' flies, sent home at his death (die()): those not there yet, put there
            f.freed = false;                          // (out of a Monstermunch, wandering home: the black over, straight home too)
            if (f.home >= 0 && Vector2Distance(f.p, lampTop(f.lamp)) > 2 * kTile) { f.p = lampTop(f.lamp); f.v = {0, 0}; }   // (its own -- or, out of a Monstermunch, the nearest)
            if (f.pole >= 0 && !f.inMouth && f.followT < 0 && Vector2Distance(f.p, flyHome(f.pole)) > 3 * kTile) { f.p = flyHome(f.pole); f.v = {0, 0}; }   // (out of one, on its way home)
        }
        cam = prevCam = {pos.x, pos.y - kLeonH / 2};
        dead = false; deadT = 0; pieces.clear(); goneT = lostT = -1;
        parts.clear();                                // (saliva, dust, sparks: none from before)
        eatenBy = -1;
        for (Monster& m : monsters) {                 // the Monstermunchs: asleep again, mouths shut
            m.calm();
        }
        fadeIn = t.deathFadeIn;
        // Input and movement from the previous life end at the checkpoint.
        resetDash();
        tonguePress = false; tgResend = false; chewPress = false; jumpBuf = 0; paying = false;
        unwedgeCrates();
        updateHatClearance();                         // the countdown can draw the hat before the next physics step
        faceNextCheckpoint();
        if (cpPine >= 0 && level.pineStage[cpPine] == 4) {   // (back at a checkpoint of cameleon mode -- its key's, its pineapples', its flag's: the balloons from the start)
            auto it = t.balloonRespawnFirst.find(cpPine);     // (that checkpoint's own colour first, if it has one: color_balloon_respawn_first)
            startBalloons(it == t.balloonRespawnFirst.end() ? 0 : it->second);
        }
        if (trace) fprintf(stderr, "t=%.2f back at the checkpoint (%.0f,%.0f), gravity frame %d\n", time, cpFeet.x, cpFeet.y, grav);
    }

    // His checkpoint from now on: a pineapple eaten -- touched, or swallowed off the tongue: chewed
    // like a fly, and CHECKPOINT on screen -- or a flag up, a key taken (they say so themselves).
    void checkpoint(int i) {
        if (i < 0) return;
        if (!pineEaten[i]) pineLitT[i] = time;
        pineEaten[i] = true;
        cpPine = i;
        if (pineGrav[i] < 0) pineGrav[i] = grav;          // he comes back with the gravity of the first touch
        cpGrav = pineGrav[i];
        cpFeet = Vector2Add(level.pines[i], rotQ(cpGrav, {0, kLeonH / 2}));
    }
    void eatPine(int i) {
        checkpoint(i);
        cpMsg = t.checkpointShow;
        flyChew = 0; chews = 0;
        sound(Audio::kFruit);
        sparks(level.pines[i], false);
        if (trace) fprintf(stderr, "t=%.2f pineapple eaten: checkpoint\n", time);
    }
    Vector2 pinePos(size_t i) const {
        if ((int)i == caughtPine && (tg == tgBack || tg == tgOut || tg == tgDead)) return tongueTip();
        return Vector2Add(level.pines[i], Vector2Scale(pullW, -sinf(time * 2.0f + i) * 3.0f));
    }

    // ---- collision ------------------------------------------------------------------------
    // Every obstacle is a box: a whole tile for '#', the slab at the top of the tile for '='.
    // Both are solid from every side -- landed on from above, bumped into from below.
    bool tileBox(int tx, int ty, Rectangle& r) const {
        if (level.block(tx, ty)) { r = {tx * kTile, ty * kTile, kTile, kTile}; return true; }
        if (level.oneway(tx, ty) && dropTimer <= 0) { r = {tx * kTile, ty * kTile, kTile, kSlabH}; return true; }
        return false;
    }

    Vector2 toWorld(Vector2 v) const { return rotQ(grav, v); }
    Vector2 toLocal(Vector2 v) const { return rotQ(-grav, v); }
    // A box turned by quarter turns is still an axis-aligned box: map two corners.
    static Rectangle turnBox(int n, float x0, float y0, float x1, float y1) {
        Vector2 a = rotQ(n, {x0, y0}), b = rotQ(n, {x1, y1});
        return {fminf(a.x, b.x), fminf(a.y, b.y), fabsf(a.x - b.x), fabsf(a.y - b.y)};
    }

    // Calls f(box) for every obstacle overlapping the open box (x0,x1) x (y0,y1), all in
    // Leon's frame: the box is turned into the world, the tiles found there turned back.
    template <class F> void boxes(float x0, float y0, float x1, float y1, F f) const {
        Rectangle w = turnBox(grav, x0, y0, x1, y1);
        float wx0 = w.x, wy0 = w.y, wx1 = w.x + w.width, wy1 = w.y + w.height;
        int tx0 = (int)floorf(wx0 / kTile), tx1 = (int)floorf(wx1 / kTile);
        int ty0 = (int)floorf(wy0 / kTile), ty1 = (int)floorf(wy1 / kTile);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                Rectangle r;
                if (tileBox(tx, ty, r) && r.x < wx1 && r.x + r.width > wx0 && r.y < wy1 && r.y + r.height > wy0)
                    f(turnBox(-grav, r.x, r.y, r.x + r.width, r.y + r.height));
            }
        for (size_t i = 0; i < crates.size(); i++) {      // and the blocks
            Rectangle r = crateRect(i);
            if (r.x < wx1 && r.x + r.width > wx0 && r.y < wy1 && r.y + r.height > wy0)
                f(turnBox(-grav, r.x, r.y, r.x + r.width, r.y + r.height));
        }
    }

    // ---- blocks ----------------------------------------------------------------------------
    float crateSide() const { return kLeonDrawH * t.crateSize; }
    Rectangle crateRect(size_t i) const {
        float h = crateSide() / 2;
        return {crates[i].c.x - h, crates[i].c.y - h, 2 * h, 2 * h};
    }
    int crateAt(Vector2 w) const {
        for (size_t i = 0; i < crates.size(); i++) if (CheckCollisionPointRec(w, crateRect(i))) return (int)i;
        return -1;
    }
    Rectangle leonWorldBox() const {
        return turnBox(grav, pos.x - kLeonW / 2, pos.y - kLeonH, pos.x + kLeonW / 2, pos.y);
    }
    // Would block i fit at centre c? Tiles, the other blocks and Leon all stop it.
    bool crateFits(size_t i, Vector2 c, bool ignoreLeon = false) const {
        float h = crateSide() / 2 - 0.01f;
        Rectangle r = {c.x - h, c.y - h, 2 * h, 2 * h};
        int tx0 = (int)floorf(r.x / kTile), tx1 = (int)floorf((r.x + r.width) / kTile);
        int ty0 = (int)floorf(r.y / kTile), ty1 = (int)floorf((r.y + r.height) / kTile);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                Rectangle b;
                if (tileBox(tx, ty, b) && CheckCollisionRecs(r, b)) return false;
            }
        for (size_t j = 0; j < crates.size(); j++)
            if (j != i && CheckCollisionRecs(r, crateRect(j))) return false;
        // Leon against its whole box: a block must never rest the least bit inside him (his own
        // moves would all be blocked -- held in the air).
        float H = crateSide() / 2;
        if (!dead && !ignoreLeon && CheckCollisionRecs({c.x - H, c.y - H, 2 * H, 2 * H}, leonWorldBox())) return false;
        return true;
    }
    // Moves block i by d, one axis at a time, as far as it fits. Returns how far it went.
    Vector2 crateMove(size_t i, Vector2 d, bool ignoreLeon = false) {
        Vector2 start = crates[i].c;
        for (int axis = 0; axis < 2; axis++) {
            float step = axis == 0 ? d.x : d.y;
            if (step == 0) continue;
            Vector2 c = crates[i].c;
            Vector2 full = c;
            (axis == 0 ? full.x : full.y) += step;
            if (crateFits(i, full, ignoreLeon)) { crates[i].c = full; continue; }
            float lo = 0, hi = 1;
            for (int k = 0; k < 12; k++) {
                float mid = (lo + hi) / 2;
                Vector2 q = c;
                (axis == 0 ? q.x : q.y) += step * mid;
                if (crateFits(i, q, ignoreLeon)) lo = mid; else hi = mid;
            }
            (axis == 0 ? crates[i].c.x : crates[i].c.y) += step * lo;
            (axis == 0 ? crates[i].v.x : crates[i].v.y) = 0;
        }
        return Vector2Subtract(crates[i].c, start);
    }
    // What is right under block i, along gravity: 0 nothing, 1 ground, 2 slippery ground.
    int crateGround(size_t i, float along) const {
        Vector2 g = pullW;
        Vector2 gd = fabsf(g.x) > fabsf(g.y) ? Vector2{signf(g.x), 0} : Vector2{0, signf(g.y)};
        Vector2 side = {-gd.y, gd.x};
        float h = crateSide() / 2;
        Vector2 probe = Vector2Add(Vector2Add(crates[i].c, Vector2Scale(gd, h + 1.5f)), Vector2Scale(side, along * (h - 2)));
        int tx = (int)floorf(probe.x / kTile), ty = (int)floorf(probe.y / kTile);
        Rectangle b;
        if (tileBox(tx, ty, b) && CheckCollisionPointRec(probe, b)) return level.slippy(tx, ty) ? 2 : 1;
        for (size_t j = 0; j < crates.size(); j++) if (j != i && CheckCollisionPointRec(probe, crateRect(j))) return 1;
        if (!dead && CheckCollisionPointRec(probe, leonWorldBox())) return 3;   // Leon: no stable ground
        return 0;
    }
    // What holds block i up, scanned along its whole bottom face (along gravity).
    struct Support { bool any = false, mid = false, leon = false, slip = false; float heldSide = 0; };
    Support crateSupport(size_t i) const {
        Vector2 gd = fabsf(pullW.x) > fabsf(pullW.y) ? Vector2{signf(pullW.x), 0} : Vector2{0, signf(pullW.y)};
        Vector2 side = {-gd.y, gd.x};
        float h = crateSide() / 2;
        Vector2 bottom = Vector2Add(crates[i].c, Vector2Scale(gd, h + 1.5f));
        Support sp;
        float sum = 0; int n = 0;
        for (float o = -(h - 1); o <= h - 1; o += 4.0f) {
            char k = obstacleAtExcept(Vector2Add(bottom, Vector2Scale(side, o)), (int)i);
            if (!k) continue;
            sp.any = true; sum += o; n++;
            if (fabsf(o) <= 4.0f && k != 'L') sp.mid = true;
            if (k == 'L') sp.leon = true;
            if (k == '~') sp.slip = true;
        }
        if (n) sp.heldSide = sum / n;
        return sp;
    }

    void crateKeepSpeed(int i, Vector2 moved, float dt) {
        Crate& cr = crates[i];
        if (cr.mode != 0) return;         // tipping or rolling: it goes its own way
        Vector2 gs = {-pullW.y, pullW.x};
        float sv = Vector2DotProduct(moved, gs) / dt;
        cr.v = Vector2Add(Vector2Scale(gs, sv), Vector2Scale(pullW, Vector2DotProduct(cr.v, pullW)));
    }

    // Safety: knocked out and colliding, Leon shoves any block touching him a little away -- so
    // he can never be wedged between a block and a wall (or under one) in an endless knock-out.
    // Never upwards against gravity: a block on top of him goes sideways.
    void nudgeCrates() {
        Rectangle me = leonWorldBox();
        Rectangle grown = {me.x - 1.5f, me.y - 1.5f, me.width + 3, me.height + 3};
        Vector2 gd = fabsf(pullW.x) > fabsf(pullW.y) ? Vector2{signf(pullW.x), 0} : Vector2{0, signf(pullW.y)};
        Vector2 side = {-gd.y, gd.x};
        Vector2 c = centreW();
        for (size_t i = 0; i < crates.size(); i++) {
            if (!CheckCollisionRecs(grown, crateRect(i))) continue;
            Vector2 away = Vector2Subtract(crates[i].c, c);
            Vector2 push = fabsf(away.x) > fabsf(away.y) ? Vector2{signf(away.x), 0} : Vector2{0, signf(away.y)};
            if (Vector2DotProduct(push, gd) < -0.5f) {        // it is above him: sideways instead
                float sd = Vector2DotProduct(away, side);
                push = Vector2Scale(side, fabsf(sd) > 1 ? signf(sd) : Vector2DotProduct(toWorld({facing, 0}), side));
            }
            Vector2 moved = crateMove(i, Vector2Scale(push, t.crateNudge), true);   // away from him: he is no obstacle
            if (trace) fprintf(stderr, "t=%.2f knocked into a block: shoved it (%.1f,%.1f)\n", time, moved.x, moved.y);
        }
    }

    // Back at his checkpoint (respawn), a block where he stands -- dragged or pushed there: lifted two
    // tiles up, his way, to fall back; or, no room there, put two tiles to his right, else to his left;
    // none of the three free, back home, as the level put it.
    void unwedgeCrates() {
        Rectangle me = leonWorldBox();
        for (size_t i = 0; i < crates.size(); i++) {
            if (!CheckCollisionRecs(me, crateRect(i))) continue;
            Crate& c = crates[i];
            const char* how = nullptr;
            for (int k = 0; k < 3 && !how; k++) {
                Vector2 to = Vector2Add(c.c, toWorld(k == 0 ? Vector2{0, -2 * kTile} : Vector2{k == 1 ? 2 * kTile : -2 * kTile, 0}));
                if (crateFits(i, to)) { c.c = to; how = k == 0 ? "up" : k == 1 ? "right" : "left"; }
            }
            if (how) c.touched = true;                       // (lifted: it falls)
            else { c.c = c.home; c.touched = c.awake = false; how = "home"; }
            c.v = {0, 0}; c.om = 0; c.mode = 0; c.visOff = {0, 0}; c.ang = roundf(c.ang / 90.0f) * 90.0f;
            if (trace) fprintf(stderr, "t=%.2f block %d where he comes back: moved %s, to (%.0f,%.0f)\n", time, (int)i, how, c.c.x, c.c.y);
        }
    }

    // Leon touching a block -- his body, or the tongue stuck to it -- lets it move from then on;
    // before that it stays where the level put it, even in mid-air.
    void touchCrate(int i, const char* how) {
        if (crates[i].touched) return;
        crates[i].touched = true;
        if (trace) fprintf(stderr, "t=%.2f block %d touched (%s)\n", time, i, how);
    }
    void touchCrates() {
        if (dead) return;
        Rectangle me = leonWorldBox();
        Rectangle grown = {me.x - 1.5f, me.y - 1.5f, me.width + 3, me.height + 3};
        for (size_t i = 0; i < crates.size(); i++)
            if (CheckCollisionRecs(grown, crateRect(i))) touchCrate((int)i, "body");
    }

    // Walking into a block on the ground pushes it, if it can go -- at block_push_speed.
    void pushCrates(float dt) {
        pushing = false;
        if (knock != kNone || !onGround || fabsf(vel.x) < 1) return;
        float dx = vel.x * dt, dir = signf(dx);
        for (size_t i = 0; i < crates.size(); i++) {
            if (crates[i].mode != 0) continue;    // tipping or rolling: not to be pushed
            Rectangle w = crateRect(i);
            Rectangle l = turnBox(-grav, w.x, w.y, w.x + w.width, w.y + w.height);   // in his frame
            bool level = l.y < pos.y - 2 && l.y + l.height > pos.y - kLeonH + 2;
            float face = dir > 0 ? l.x : l.x + l.width;
            float gap = (face - (pos.x + dir * kLeonW / 2)) * dir;
            if (!level || gap < -0.5f || gap > fabsf(dx) + 0.5f) continue;
            Vector2 moved = crateMove(i, toWorld({dir * (fabsf(dx) - gap + 0.01f), 0}));
            if (Vector2Length(moved) > 0.001f) { pushing = true; crateKeepSpeed((int)i, moved, dt); }
        }
    }

    // Start block i tipping over the edge of what holds it -- towards where nothing does (or,
    // on Leon, towards the side it overhangs him most).
    void startTip(size_t i, Vector2 gd, Vector2 side, const Support& sp) {
        Crate& c = crates[i];
        float h = crateSide() / 2;
        Vector2 bottom = Vector2Add(c.c, Vector2Scale(gd, h));
        float s;
        float edge;                                    // the corner, along `side`, from the middle
        if (sp.leon && !sp.mid) {
            Rectangle lb = leonWorldBox();
            Vector2 lc = {lb.x + lb.width / 2, lb.y + lb.height / 2};
            float off = Vector2DotProduct(Vector2Subtract(c.c, lc), side);
            s = fabsf(off) > 1 ? signf(off) : signf(Vector2DotProduct(toWorld({facing, 0}), side));
            float halfW = fabsf(Vector2DotProduct({lb.width / 2, lb.height / 2}, side));
            edge = Vector2DotProduct(lc, side) + s * halfW - Vector2DotProduct(c.c, side);
        } else {
            s = sp.heldSide > 0 ? -1.0f : 1.0f;        // away from where it is held
            edge = -s * (h - 1);                       // walk from the held side: the last held point
            for (float o = -s * (h - 1); o * s <= h - 1; o += s * 2.0f) {
                if (obstacleAtExcept(Vector2Add(Vector2Add(bottom, Vector2Scale(gd, 1.5f)), Vector2Scale(side, o)), (int)i)) edge = o;
                else break;
            }
        }
        edge = clampf(edge, -h, h);
        c.pivot = Vector2Add(bottom, Vector2Scale(side, edge));
        c.tipDir = Vector2Scale(side, s);
        c.c0 = c.c;
        c.r0 = Vector2Subtract(c.c, c.pivot);
        c.tipSign = (Vector2DotProduct(Vector2{-c.r0.y, c.r0.x}, c.tipDir) >= 0) ? 1.0f : -1.0f;
        c.tipAng = 0; c.om = 0;
        c.mode = 1;
        c.v = {0, 0};
        if (trace) fprintf(stderr, "t=%.2f block %d tips over%s\n", time, (int)i, sp.leon && !sp.mid ? " (off Leon)" : "");
    }

    char obstacleAtExcept(Vector2 w, int ci) const {
        for (size_t j = 0; j < crates.size(); j++) if ((int)j != ci && CheckCollisionPointRec(w, crateRect(j))) return 'B';
        if (!dead && CheckCollisionPointRec(w, leonWorldBox())) return 'L';
        int tx = (int)floorf(w.x / kTile), ty = (int)floorf(w.y / kTile);
        Rectangle b;
        if (!(tileBox(tx, ty, b) && CheckCollisionPointRec(w, b))) return 0;
        return level.slippy(tx, ty) ? '~' : '#';
    }

    void crateStep(float dt) {
        Vector2 g = pullW;
        Vector2 gd = fabsf(g.x) > fabsf(g.y) ? Vector2{signf(g.x), 0} : Vector2{0, signf(g.y)};
        Vector2 side = {-gd.y, gd.x};
        const float L = crateSide();
        Vector2 camW = toWorld(cam);
        Vector2 vs = viewSize();
        float seeR = sqrtf(vs.x * vs.x + vs.y * vs.y) / 2;
        for (size_t i = 0; i < crates.size(); i++) {
            Crate& c = crates[i];
            // Blocks never move before Leon has touched them. Then they keep still until they
            // are seen; once moving, they finish their move even off screen; at rest and out of
            // sight, they go back to sleep.
            if (!c.touched || !inZone(c.c)) continue;   // (far from him: frozen as it is)
            bool seen = Vector2Distance(c.c, camW) < seeR + L;
            if (!c.awake) { if (!seen) continue; c.awake = true; }
            float angBefore = c.ang;
            if (c.mode == 1) {
                // Tipping over a corner: drawn turning about it (faster as its middle passes the
                // corner -- gravity's torque -- never slower than block_roll_min_speed), while its
                // collision box slides along with the arc. When nothing holds the box any more,
                // it falls, still spinning.
                Vector2 arc = Vector2Add(c.pivot, Vector2Rotate(c.r0, c.tipAng * c.tipSign * DEG2RAD));
                Vector2 r = Vector2Subtract(arc, c.pivot);
                float phi = atan2f(Vector2DotProduct(r, c.tipDir), Vector2DotProduct(r, Vector2Negate(gd)));
                float alpha = 1.5f * t.gravity / (L * 0.7071f) * sinf(fmaxf(phi, 0.05f)) * RAD2DEG;
                c.om = fmaxf(fabsf(c.om) + alpha * dt, t.crateRollMin) * c.tipSign;
                float dth = fabsf(c.om) * dt;
                c.tipAng += dth;
                c.ang += dth * c.tipSign;
                arc = Vector2Add(c.pivot, Vector2Rotate(c.r0, c.tipAng * c.tipSign * DEG2RAD));
                float along = Vector2DotProduct(Vector2Subtract(arc, c.c0), c.tipDir);
                Vector2 want = Vector2Add(c.c0, Vector2Scale(c.tipDir, along));
                Vector2 moved = crateMove(i, Vector2Subtract(want, c.c), true);
                c.visOff = Vector2Subtract(arc, c.c);
                bool stuck = Vector2Length(Vector2Subtract(want, c.c)) > 0.5f && Vector2Length(moved) < 0.01f;
                if (!crateSupport(i).any || c.tipAng > 85.0f || stuck) {
                    Vector2 rr = Vector2Subtract(arc, c.pivot);
                    c.v = Vector2Scale(Vector2{-rr.y, rr.x}, c.om * DEG2RAD);
                    c.mode = 0;
                }
            } else if (c.mode == 2) {
                // Rolling flat: on round to the next face, the way it was turning -- never back --
                // one block width per quarter turn, never slower than block_roll_min_speed.
                float rem = c.settleTo - c.ang;
                float dth = signf(rem) * fminf(fabsf(rem), fmaxf(fabsf(c.om), t.crateRollMin) * dt);
                c.ang += dth;
                crateMove(i, Vector2Scale(side, -(L / (PI / 2)) * dth * DEG2RAD), true);
                float m = fabsf(remainderf(c.ang, 90.0f));          // a rolling square rises on its corner
                c.visOff = Vector2Scale(gd, -(L / 2) * (1.41421356f * cosf((45.0f - m) * DEG2RAD) - 1));
                if (fabsf(c.settleTo - c.ang) < 0.01f) { c.ang = c.settleTo; c.om = 0; c.mode = 0; c.v = {0, 0}; c.visOff = {0, 0}; }
            } else {
                c.v = Vector2Add(c.v, Vector2Scale(pullW, t.gravity * dt));
                float along = Vector2DotProduct(c.v, gd);
                if (along > t.maxFall) c.v = Vector2Subtract(c.v, Vector2Scale(gd, along - t.maxFall));
                Support sp = crateSupport(i);
                bool under = sp.any;
                bool flat = fabsf(remainderf(c.ang, 90.0f)) < 0.5f;
                c.visOff = Vector2Scale(c.visOff, expf(-dt * 14));   // drawing catches up with the box
                if (under && !flat) {
                    // Landed tilted: roll on to the next face, the way it was spinning.
                    c.settleTo = (c.om >= 0) ? ceilf(c.ang / 90.0f) * 90.0f : floorf(c.ang / 90.0f) * 90.0f;
                    if (fabsf(c.settleTo - c.ang) < 0.01f) c.settleTo += (c.om >= 0 ? 90.0f : -90.0f);
                    c.mode = 2;
                    c.v = {0, 0};
                } else if (under) {   // on the ground: sliding along it dies away -- slowly on ice
                    float sv = Vector2DotProduct(c.v, side);
                    float k = sp.slip ? t.crateSlipFriction : t.crateFriction;
                    c.v = Vector2Subtract(c.v, Vector2Scale(side, sv * fminf(1.0f, k * dt)));
                    // Middle past an edge, or resting on Leon: it tips over, to the obvious side.
                    if (!sp.mid) startTip(i, gd, side, sp);
                } else {
                    c.ang += c.om * dt;               // flying: still spinning
                }
                if (c.mode == 0) crateMove(i, Vector2Scale(c.v, dt));
            }
            if (!seen && c.mode == 0 && Vector2Length(c.v) < 1 && crateSupport(i).any) c.awake = false;
            if (tgCrate == (int)i && c.ang != angBefore)       // the tongue's sticking point turns with it
                tgCrateOff = Vector2Rotate(tgCrateOff, (c.ang - angBefore) * DEG2RAD);
            if (c.c.x < -12 * kTile || c.c.y < -12 * kTile || c.c.x > (level.w + 12) * kTile || c.c.y > (level.h + 12) * kTile) {
                c.c = c.home; c.v = {0, 0}; c.ang = c.om = 0; c.mode = 0; c.visOff = {0, 0};
                if (tgCrate == (int)i) tongueRetract(t.tongueBack);
            }
        }
    }

    // The gravity platform Leon is touching (his box grown by a pixel -- and the hat on his head
    // touches for him), as the frame it asks for -- or -1. Unless `any`, one already matching the
    // current gravity is skipped.
    struct GravityContact { int frame; bool knocksHatOff; };
    GravityContact gravityContact(bool any = false) const {
        float up = 1;
        if (hasHat) { float hl, hr; hatSides(hl, hr); up += kHatSeat + kHatCrown * fmaxf(hl, hr); }
        Rectangle w = turnBox(grav, pos.x - kLeonW / 2 - 1, pos.y - kLeonH - up, pos.x + kLeonW / 2 + 1, pos.y + 1);
        int tx0 = (int)floorf(w.x / kTile), tx1 = (int)floorf((w.x + w.width) / kTile);
        int ty0 = (int)floorf(w.y / kTile), ty1 = (int)floorf((w.y + w.height) / kTile);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                int f = gravityFrame(level.at(tx, ty));
                if (f >= 0 && (any || f != grav)) return {f, level.rows[ty][tx] == kHatDropIce};
            }
        return {-1, false};
    }

    // Screen angle of the world at game time tt (degrees, clockwise): follows the bell curve.
    float frameAngle(float tt) const {
        if (turnStart < 0) return turnFrom;
        return turnFrom + turnDelta * bellEase((tt - turnStart) / t.turnTime, t.turnSigma);
    }

    // Tests: start in frame f at once, no swing.
    void forceGravity(int f) {
        int r = grav - f;
        auto turnFeet = [&](Vector2 feet) {
            Vector2 c = rotQ(r, {feet.x, feet.y - kLeonH / 2});
            return Vector2{c.x, c.y + kLeonH / 2};
        };
        pos = prevPos = turnFeet(pos);
        vel = rotQ(r, vel);
        cam = prevCam = rotQ(r, cam);
        grav = f; turnFrom = f * 90.0f; turnDelta = 0; turnStart = -1;
        fallTop = pos.y;
    }

    // Switch gravity to frame f: Leon's centre, speed and the camera keep their place in the
    // world; only the frame they are written in turns. Then the swing begins.
    void setGravity(int f) {
        if (turnStart >= 0) { turnFrom += turnDelta; turnStart = -1; }
        int dq = ((f - grav) % 4 + 4) % 4;
        float delta = (dq == 1) ? 90.0f : (dq == 3) ? -90.0f : (vel.x >= 0 ? 180.0f : -180.0f);
        bool quarter = dq == 1 || dq == 3;
        turnC0 = centreW(); turnFloor = quarter && (onGround || standing()) ? toWorld({0, 1}) : Vector2{0, 0};   // (turnLift: on that floor -- or landing on it)
        int r = grav - f;                                // old frame -> new frame
        auto turnFeet = [&](Vector2 feet) {
            Vector2 c = rotQ(r, {feet.x, feet.y - kLeonH / 2});
            return Vector2{c.x, c.y + kLeonH / 2};
        };
        pos = turnFeet(pos); prevPos = turnFeet(prevPos);
        vel = rotQ(r, vel);
        cam = rotQ(r, cam); prevCam = rotQ(r, prevCam);
        grav = f;
        turnDelta = delta;
        turnStart = time;
        onGround = false; jumping = false; coyote = 0; spin = 0;
        fallTop = pos.y;                                 // a fall is measured in one gravity
        // His box is now turned: if that pushes it into a wall, slide it out.
        if (blocked(pos.x, pos.y)) {
            for (int d = 1; d <= 20; d++) {
                const Vector2 dirs[8] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
                bool done = false;
                for (auto& v : dirs)
                    if (!blocked(pos.x + v.x * d, pos.y + v.y * d)) {
                        pos.x += v.x * d; pos.y += v.y * d; prevPos = pos; done = true; break;
                    }
                if (done) break;
            }
        }
        if (trace) fprintf(stderr, "t=%.2f gravity -> frame %d (turn %+.0f deg)\n", time, f, delta);
    }

    // How much of his drawing still stands on the floor a quarter turn began on, at game time tt (his
    // drawing's own, lagging the turn): 1 as it begins, 0 once his drawing has turned (0: no such turn).
    float turnStood(float tt) const {
        if (turnStart < 0 || (turnFloor.x == 0 && turnFloor.y == 0)) return 0;
        return 1 - bellEase((tt - turnStart) / t.turnTime, t.turnSigma);
    }
    // His drawing's lift (world) then, his drawn centre at c: what his box has crept towards that
    // floor (at most the difference of his two half sizes), as much as his drawing still stands on it.
    Vector2 turnLift(Vector2 c, float tt) const {
        float stood = turnStood(tt);
        if (stood <= 0) return {0, 0};
        float crept = clampf(Vector2DotProduct(Vector2Subtract(c, turnC0), turnFloor), 0, (kLeonH - kLeonW) / 2);
        return Vector2Scale(turnFloor, -crept * stood);
    }

    // Overlapping anything (a sliver under 0.05 px aside: touching is not being stuck).
    bool blocked(float x, float y) const {
        const float e = 0.05f;
        bool any = false;
        boxes(x - kLeonW / 2 + e, y - kLeonH + e, x + kLeonW / 2 - e, y - e, [&](const Rectangle&) { any = true; });
        return any;
    }

    // Returns true when a wall stopped the move.
    bool moveX(float dx) {
        if (dx == 0) return false;
        float old = pos.x, nx = pos.x + dx;
        bool hit = false;
        float lim = nx;
        boxes(nx - kLeonW / 2, pos.y - kLeonH, nx + kLeonW / 2, pos.y, [&](const Rectangle& r) {
            hit = true;
            if (dx > 0) lim = fminf(lim, r.x - kLeonW / 2 - 0.001f);
            else lim = fmaxf(lim, r.x + r.width + kLeonW / 2 + 0.001f);
        });
        if (!hit) { pos.x = nx; return false; }
        pos.x = (dx > 0) ? fmaxf(old, lim) : fminf(old, lim);   // never pulled backwards
        if (blocked(pos.x, pos.y)) pos.x = old;
        return true;
    }

    // Swept, so no speed can tunnel through a thin slab. +1 = stopped by a floor,
    // -1 = stopped by a ceiling (his head hit an obstacle from below), 0 = free.
    int moveY(float dy) {
        if (dy == 0) return 0;
        float ny = pos.y + dy;
        float x0 = pos.x - kLeonW / 2, x1 = pos.x + kLeonW / 2;
        if (dy > 0) {
            float top = 1e9f;
            boxes(x0, pos.y - kLeonH, x1, ny + 0.01f, [&](const Rectangle& r) {
                if (r.y >= pos.y - 0.5f) top = fminf(top, r.y);   // below his feet only
            });
            if (top <= ny) { pos.y = top; return 1; }
            float old = pos.y;
            pos.y = ny;
            if (blocked(pos.x, pos.y)) { pos.y = old; return 1; }
            return 0;
        }
        float head = pos.y - kLeonH, nhead = ny - kLeonH;
        float bottom = -1e9f;
        boxes(x0, nhead - 0.01f, x1, pos.y, [&](const Rectangle& r) {
            float b = r.y + r.height;
            if (b <= head + 0.5f) bottom = fmaxf(bottom, b);   // above his head only
        });
        if (bottom >= nhead) { pos.y = bottom + kLeonH + 0.001f; return -1; }
        float old = pos.y;
        pos.y = ny;
        if (blocked(pos.x, pos.y)) { pos.y = old; return -1; }
        return 0;
    }

    // Is there floor right under his feet between local x0 and x1?
    bool supportedAt(float x0, float x1) const {
        bool any = false;
        boxes(x0, pos.y - 0.25f, x1, pos.y + 0.5f, [&](const Rectangle& r) {
            if (fabsf(r.y - pos.y) <= 0.5f) any = true;
        });
        return any;
    }

    // Standing on slippery ground only (any grip under his feet wins).
    bool onSlippy() const { return onGround && slippyUnder(); }

    // Ice under his feet, and nothing that grips (standing or not).
    bool slippyUnder() const {
        Rectangle w = turnBox(grav, pos.x - kLeonW / 2, pos.y + 0.5f, pos.x + kLeonW / 2, pos.y + 1.5f);
        bool ice = false, grip = false;
        int tx0 = (int)floorf(w.x / kTile), tx1 = (int)floorf((w.x + w.width - 0.01f) / kTile);
        int ty0 = (int)floorf(w.y / kTile), ty1 = (int)floorf((w.y + w.height - 0.01f) / kTile);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                Rectangle r;
                if (!tileBox(tx, ty, r) || !CheckCollisionRecs(r, w)) continue;
                if (level.slippy(tx, ty)) ice = true; else grip = true;
            }
        for (size_t i = 0; i < crates.size(); i++) if (CheckCollisionRecs(crateRect(i), w)) grip = true;
        return ice && !grip;
    }

    bool standing() const {
        bool any = false;
        boxes(pos.x - kLeonW / 2, pos.y - 0.25f, pos.x + kLeonW / 2, pos.y + 0.5f, [&](const Rectangle& r) {
            if (fabsf(r.y - pos.y) <= 0.5f) any = true;
        });
        return any;
    }

    // ---- effects --------------------------------------------------------------------------
    // Dust at his feet -- on ice, soap bubbles instead, gentler (they drift on far longer).
    void dust(Vector2 at, int n, float spread, float up) {
        if (slippyUnder()) { bubbles(at, n, spread * 0.4f, up * 0.4f); return; }
        for (int i = 0; i < n; i++) {
            float a = GetRandomValue(-1000, 1000) / 1000.0f;
            Particle q;
            q.p = {at.x + a * 10, at.y - 2};
            q.v = {a * spread, -up * GetRandomValue(30, 100) / 100.0f};
            q.maxLife = q.life = GetRandomValue(30, 60) / 100.0f;
            q.r = GetRandomValue(3, 7);
            q.spark = false;
            emit(q);
        }
    }

    // Soap bubbles at his feet (ice), flung sideways, drifting up and popping.
    void bubbles(Vector2 at, int n, float spread, float up) {
        for (int i = 0; i < n; i++) {
            float a = GetRandomValue(-1000, 1000) / 1000.0f;
            Particle q;
            q.p = {at.x + a * 14, at.y - GetRandomValue(1, 4)};
            q.v = {a * spread, -up * GetRandomValue(30, 100) / 100.0f};
            q.maxLife = q.life = GetRandomValue(40, 90) / 100.0f;
            q.r = GetRandomValue(18, 36) / 10.0f;
            q.spark = false; q.bubble = true;
            emit(q);
        }
    }

    // Particles live in the world: born in Leon's frame, turned out of it.
    void emit(Particle q, bool local = true) {
        if (local) { q.p = toWorld(q.p); q.v = toWorld(q.v); }
        parts.push_back(q);
    }

    void sparks(Vector2 at, bool local = true) {
        for (int i = 0; i < 14; i++) {
            float a = i * (2 * PI / 14) + GetRandomValue(0, 100) / 300.0f;
            float s = GetRandomValue(160, 320);
            Particle q;
            q.p = at;
            q.v = {cosf(a) * s, sinf(a) * s};
            q.maxLife = q.life = GetRandomValue(35, 60) / 100.0f;
            q.r = 3;
            q.spark = true;
            emit(q, local);
        }
    }

    // A flat ring of puffs under his feet: the air he just kicked off.
    void puffRing(Vector2 at) {
        for (int i = 0; i < 12; i++) {
            float a = i * (2 * PI / 12);
            Particle q;
            q.p = {at.x, at.y};
            q.v = {cosf(a) * 220, sinf(a) * 45 + 30};
            q.maxLife = q.life = 0.35f;
            q.r = 4;
            q.spark = false;
            emit(q);
        }
    }

    // Wearing the hat, whole: it takes the bump -- folding up like an accordion -- and he keeps
    // his feet. Folded, and until it has unfolded again, it protects no more.
    void hatBump() {
        hatT = 0;
        hatBlinkT = 0;                                // the jolt: his eyes shut, then blink (hat_bump_*)
        hatFrom = clampf((hatRoom - hatSeat()) / hatCrown(), hatLowest(), 1);   // crushed by the ceiling already
        vel.y = 60;                                   // stopped dead, then down again
        jumping = false; spin = 0; jumpBuf = 0;
        trauma = fminf(1.0f, trauma + 0.15f);
        sound(Audio::kHat);
        if (trace) fprintf(stderr, "t=%.2f the hat takes the bump\n", time);
    }
    // Seconds after a bump: the crown starts unfolding, then is whole (and protects) again.
    float hatUnfoldAt() const { return t.hatSquashT + t.hatHoldT; }
    float hatWholeAt() const { return anna ? annaPopAt(3) + t.annaHatPopT : hatUnfoldAt() + t.hatUnfoldT; }
    // The hat's measures, his or hers: its brim over the hitbox's top, its crown's height, how low it folds.
    float hatSeat() const { return anna ? kAnnaSeat : kHatSeat; }
    float hatCrown() const { return anna ? kAnnaCone : kHatCrown; }
    float hatLowest() const { return anna ? t.annaHatLow : t.hatLow; }
    // Anna's hat: four bands, from the brim up (1 whole, 0 flat), folding from the top down -- the top
    // one all the way before the next. Folded to `f` of its height: band k's height.
    static float annaBand(float f, int k) { return clampf(f * 4 - k, 0, 1); }
    // After a bump: folded fast (hat_squash_time), held (hat_squashed_time), then popping back one by
    // one -- the lowest folded first -- anna_hat_pop_delay apart, each over anna_hat_pop_time,
    // overshooting a little.
    float annaPopAt(int k) const {
        int first = std::min(3, (int)floorf(t.annaHatLow * 4));   // (the lowest band folded at all)
        return hatUnfoldAt() + std::max(0, k - first) * t.annaHatPopDelay;
    }
    void annaBands(float h[4]) const {
        for (int k = 0; k < 4; k++) h[k] = 1;
        if (hatT < 0) return;
        const float lo = t.annaHatLow;
        if (hatT < t.hatSquashT) {
            float u = hatT / t.hatSquashT, f = lerpf(hatFrom, lo, 1 - (1 - u) * (1 - u));
            for (int k = 0; k < 4; k++) h[k] = annaBand(f, k);
            return;
        }
        for (int k = 0; k < 4; k++) {
            float folded = annaBand(lo, k), u = (hatT - annaPopAt(k)) / t.annaHatPopT;
            if (folded >= 1 || u <= 0) { h[k] = folded; continue; }
            if (u >= 1) continue;
            float v = u - 1, back = 1 + 2.7f * v * v * v + 1.7f * v * v;   // (an ease out, past 1 and back)
            h[k] = folded + (1 - folded) * back;
        }
    }
    bool hatGuards() const { return hasHat && (hatT < 0 || hatT >= hatWholeAt()); }
    // The room above his head, up to `upTo` pixels, in his own frame.
    float roomAbove(float upTo) const {
        float head = pos.y - kLeonH, room = upTo;
        boxes(pos.x - kLeonW / 2, head - upTo, pos.x + kLeonW / 2, head, [&](const Rectangle& r) {
            float b = r.y + r.height;
            if (b <= head + 0.5f) room = fminf(room, fmaxf(0, head - b));
        });
        return room;
    }
    void updateHatClearance() {
        hatRoom = hasHat ? roomAbove(hatSeat() + hatCrown() + 4) : 1e9f;
        // Anna's brim reaches past her hitbox. Measure each wall from her middle in her gravity frame.
        hatSideL = hatSideR = 1e9f;
        if (!anna || !hasHat || endT >= 0) return;      // (the ending: free -- not bent on its unseen wall)
        float brim = pos.y - kLeonH - kAnnaSeat;
        boxes(pos.x - kAnnaBrimReach, brim - kAnnaCurlRise, pos.x + kAnnaBrimReach, brim + 1, [&](const Rectangle& r) {
            if (r.y + r.height < brim - 0.5f) return;       // (a ceiling: not down to the brim -- its curls are pressed flat by it, hatRoom)
            if (r.x >= pos.x) hatSideR = fminf(hatSideR, r.x - pos.x);
            else if (r.x + r.width <= pos.x) hatSideL = fminf(hatSideL, pos.x - (r.x + r.width));
        });
    }
    // The hat's crown, the height of its left and right sides (1 = whole): folding fast, staying
    // folded, then pulled open like an accordion, one side at a time -- left, right, three times.
    void hatSides(float& hl, float& hr) const {
        hl = hr = 1;
        if (hatT < 0) return;
        float lo = t.hatLow;
        if (hatT < t.hatSquashT) {
            float u = hatT / t.hatSquashT;
            hl = hr = lerpf(hatFrom, lo, 1 - (1 - u) * (1 - u));
            return;
        }
        float u = (hatT - t.hatSquashT - t.hatHoldT) / t.hatUnfoldT;
        if (u <= 0) { hl = hr = lo; return; }
        float s = fminf(u, 1) * 6;                       // six pulls: left, right, left, ...
        int k = std::min(5, (int)s);
        float f = fminf(1, s - k), e = f * f * (3 - 2 * f);
        float pl = (k + 1) / 2 + (k % 2 == 0 ? e : 0), pr = k / 2 + (k % 2 == 1 ? e : 0);   // pulls so far, each side
        hl = lerpf(lo, 1, pl / 3);
        hr = lerpf(lo, 1, pr / 3);
    }
    // The brim's curl (1 = as drawn): flat from the bump on, while the hat folds and stays folded;
    // once the crown starts unfolding, it strains back a little, slowly, then snaps to its curl
    // and rings a while, faintly, dying away -- like a spring.
    float hatBrimDoneAt() const { return hatUnfoldAt() + t.brimStrainT + t.brimRingT; }
    float hatCurl() const {
        if (hatT < 0) {                                   // whole -- pedalling in a long fall, flapping in the air:
            float pedal = onGround ? 0 : clampf((fallT - t.pedalAfter) / 0.3f, 0, 1);   // its ends curling up and
            return endBrimCurl() * (1 + pedal * t.brimFlap * (0.5f - 0.5f * cosf(2 * PI * t.brimFlapFreq * time)));   // back, hat_brim_flap_frequency a second (the ending: a party horn)
        }
        float u = hatT - hatUnfoldAt();
        if (u < 0) return 0;
        if (u < t.brimStrainT) { float s = u / t.brimStrainT; return t.brimStrain * s * s * (3 - 2 * s); }
        float v = u - t.brimStrainT;
        if (v >= t.brimRingT) return 1;
        float fade = 1 - v / t.brimRingT;
        return 1 + t.brimRing * sinf(2 * PI * t.brimRingFreq * v) * fade * fade;
    }
    // Knocked out and landing (or rolling) onto his head: the hat folds up all the same -- and,
    // folded, protects no more for a while.
    int groundFace() const { return ((int)lroundf(knockAngle / 90.0f) % 4 + 4) % 4; }   // 0: feet down, 2: head
    void hatGround() {
        if (!hasHat || (hatT >= 0 && hatT < t.hatSquashT + t.hatHoldT)) return;      // (already folded)
        hatT = 0; hatFrom = 1;
        sound(Audio::kHat, 0.9f, 0.7f);
        if (trace) fprintf(stderr, "t=%.2f the hat hits the ground\n", time);
    }

    void startKnock(float headY) {
        sound(Audio::kCrash);
        if (tg != tgNone) tongueRetract(t.tongueBack);
        flyChew = -1;
        knock = kAir;
        bounces = 0;
        knockAngle = 0;
        rollFace = 0;
        getUp = 0;
        starsT = wakeT = hatBlinkT = -1;
        settling = false;
        float dir = (fabsf(vel.x) > 20) ? signf(vel.x) : facing;
        knockOmega = dir * fmaxf(240.0f, fabsf(vel.x) / kRollR * RAD2DEG * 0.6f);
        vel.y = 60;                                   // it stops him dead, then he drops
        jumping = false; spin = 0; jumpBuf = 0;
        trauma = fminf(1.0f, trauma + 0.3f);
        sparks({pos.x, headY});
    }

    // Knocked out and hitting the ground: bounce as a square, or settle into a roll.
    void knockLand(float impact) {
        rollFace = groundFace();
        if (trace) fprintf(stderr, "t=%.2f knocked out, landing at %.0f deg (face %d down)\n", time, knockAngle, rollFace);
        if (rollFace == 2) hatGround();                  // head first
        float k = clampf(impact / t.maxFall, 0, 1);
        dust({pos.x, pos.y}, 3 + (int)(k * 12), 60 + 200 * k, 40 + 120 * k);
        sound(Audio::kThud, 1.15f, clampf(0.25f + k, 0.25f, 0.8f));   // bouncing, out cold: softer
        if (impact > t.hardLand) trauma = fminf(1.0f, trauma + 0.35f);
        bounces++;
        float up = impact * t.knockBounce;
        vel.x *= t.knockBounceFriction;
        if (bounces <= (int)t.knockBounces && up > t.knockBounceMin) {
            vel.y = -up;
            knockOmega = vel.x / kRollR * RAD2DEG;
        } else {
            vel.y = 0;
            knock = kRoll;
            settling = false;
        }
    }

    // Landing from higher than knock_fall_height screens: out cold, as from a bump on the head.
    void fallKnock(float impact) {
        if (trace) fprintf(stderr, "t=%.2f fell %.0f px: knocked out\n", time, pos.y - fallTop);
        startKnock(pos.y - kLeonH);
        sound(Audio::kThud, 0.85f, 1.0f);
        trauma = fminf(1.0f, trauma + 0.4f);
        knockLand(impact);
    }

    // The knocked-out part of a step: gravity, a rolling stop, then back on his feet.
    void knockStep(float dt) {
        jumpBuf = 0;
        if (knock == kRoll) {                            // rolling on, a face at a time: the head's too?
            int f = groundFace();
            if (f != rollFace) {
                rollFace = f;
                if (trace) fprintf(stderr, "t=%.2f rolling onto face %d\n", time, f);
                if (f == 2) hatGround();
            }
        }
        if (knock == kAir) knockAngle += knockOmega * dt;
        if (knock == kRoll && !settling) {
            vel.x -= vel.x * t.knockRollFriction * dt;
            knockAngle += vel.x / kRollR * RAD2DEG * dt;
            if (fabsf(vel.x) < 60) {
                // Slow enough to stop: carry on tumbling, face by face, until the feet are
                // underneath. Just past upright, he tips back instead.
                float dir = (vel.x != 0) ? signf(vel.x) : (knockOmega != 0 ? signf(knockOmega) : facing);
                float a = knockAngle;
                settleTo = (dir > 0) ? ceilf(a / 360.0f) * 360.0f : floorf(a / 360.0f) * 360.0f;
                if (fabsf(settleTo - a) > 270) settleTo -= dir * 360;
                settling = true;
                vel.x = 0;
            }
        } else if (knock == kRoll) {
            float rem = settleTo - knockAngle;
            float d = fminf(fabsf(rem), fmaxf(400.0f, fabsf(rem) * 7.0f) * dt) * signf(rem);
            knockAngle += d;
            moveX(d * DEG2RAD * kRollR * 0.7f);   // each tumble carries him along a little
            if (fabsf(settleTo - knockAngle) < 0.01f) {
                knockAngle = 0;
                knock = kDown;
                knockTimer = t.knockDown;
            }
        } else if (knock == kDown) {
            if ((knockTimer -= dt) <= 0) {
                knock = kRise;
                knockTimer = t.knockControl;
                getUp = t.knockRise;
                starsT = 0;
            }
        } else if (knock == kRise) {
            if ((knockTimer -= dt) <= 0) {   // on his feet: control is back, the get-up plays on
                knock = kNone;
                knockAngle = 0; tilt = 0;
            }
        }
    }

    // Seconds into the get-up animation, or -1 when none is playing.
    float getUpTime() const {
        if (getUp <= 0 || !(knock == kRise || knock == kNone)) return -1;
        return t.knockRise - getUp;
    }

    // The dizzy stars over him, from the start of the get-up: fading out over knock_stars_time
    // (of the clock, not of the get-up, which moving speeds up).
    float starsAlpha() const { return starsT < 0 ? 0 : 1 - starsT / t.knockStars; }

    // Just up from a knock-out, the eyes blink wake_blinks times, evenly over wake_blink_time; the
    // hat taking a bump, they shut for hat_bump_shut_time, then blink hat_bump_blinks times over
    // hat_bump_blink_time.
    static bool blinkShut(float at, float time, float blinks) {
        if (at < 0 || blinks < 1) return false;
        float span = time / blinks, u = fmodf(at, span);   // one blink's share
        return fabsf(u - span / 2) < fminf(0.06f, span * 0.3f);
    }
    bool wakeShut() const {
        if (hatBlinkT >= 0 && hatBlinkT < t.hatShutT) return true;
        return blinkShut(wakeT, t.wakeBlinkT, t.wakeBlinks) || blinkShut(hatBlinkT - t.hatShutT, t.hatBlinkT, t.hatBlinks);
    }

    // ---- tongue ---------------------------------------------------------------------------
    Vector2 centreW() const { return toWorld({pos.x, pos.y - kLeonH / 2}); }
    float tongueReach() const { return viewSize().y * t.tongueLen; }

    // What occupies a world point: '#', a gravity block, '=' (its slab only) -- or 0.
    char obstacleAt(Vector2 w) const {
        if (crateAt(w) >= 0) return 'B';
        int tx = (int)floorf(w.x / kTile), ty = (int)floorf(w.y / kTile);
        char c = level.at(tx, ty);
        if (level.block(tx, ty)) return c;
        if (c == '=' && w.y - ty * kTile < kSlabH) return c;
        return 0;
    }

    void requestGravity(char c) {
        int f = gravityFrame(c);
        bool turning = turnStart >= 0 && time - turnStart < t.turnTime;
        if (f >= 0 && f != grav && !turning) setGravity(f);
    }

    // Saliva: a few drops flung from a world point, roughly along `dir`.
    void spitDrops(Vector2 at, Vector2 dir, int n, float speed) {
        if (!t.spit) return;
        for (int i = 0; i < n; i++) {
            float a = GetRandomValue(-70, 70) / 100.0f;
            Vector2 d = Vector2Rotate(dir, a);
            Particle q;
            q.p = at;
            q.v = Vector2Scale(d, speed * GetRandomValue(50, 110) / 100.0f);
            q.maxLife = q.life = 1.4f;
            q.r = GetRandomValue(15, 30) / 10.0f;
            q.spark = false;
            q.drop = true;
            emit(q, false);
        }
    }

    // A dust puff at a world point: the tongue rubbing on a corner as it stretches.
    void rubDust(Vector2 at) {
        Particle q;
        q.p = at;
        q.v = Vector2Scale(Vector2Rotate(toWorld({0, -1}), GetRandomValue(-100, 100) / 100.0f), 30);
        q.maxLife = q.life = 0.35f;
        q.r = 2.5f;
        q.spark = false;
        emit(q, false);
    }

    // An empty spot for a fly. `hidden`: not inside the area the player can currently see.
    Vector2 flySpot(bool hidden) const {
        Vector2 camW = toWorld(cam);
        Vector2 v = viewSize();
        float r = sqrtf(v.x * v.x + v.y * v.y) / 2 + kTile;
        for (int tries = 0; tries < 400; tries++) {
            int tx = GetRandomValue(1, std::max(1, level.w - 2)), ty = GetRandomValue(1, std::max(1, level.h - 2));
            if (level.at(tx, ty) != ' ') continue;
            Vector2 p = {(tx + 0.5f) * kTile, (ty + 0.5f) * kTile};
            if (hidden && Vector2Distance(p, camW) < r) continue;
            return p;
        }
        return {(level.w / 2 + 0.5f) * kTile, kTile * 2.5f};
    }

    // A golden fly circles Leon for ever, a little behind him: round an ellipse about him (turned
    // with gravity, a bit above his middle), never faster than golden_fly_max_speed of his top
    // falling speed -- through walls, if it must.
    void goldenStep(Fly& f, size_t i, float dt) {
        f.orbit += dt * (1.3f + 0.3f * sinf(time * 0.7f + i * 1.9f));
        float R = 32 + 8 * sinf(time * 0.9f + i * 2.3f);
        flyChase(f, Vector2Add(centreW(), toWorld({cosf(f.orbit) * R * 1.25f, sinf(f.orbit) * R * 0.7f - 8})), dt);
    }

    // A lamp's fly: round the disk at the end of its post, like a moth round a lamp -- or,
    // swallowed and coughed back, round Leon for lamp_follow_time; then to the nearest lamp (with a
    // fly already or not), which it lights from then on: the light can be carried, lamp to lamp.
    Vector2 lampTop(size_t k) const {
        return Vector2Add(level.lamps[k], Vector2Scale(level.lampDir[k], 2 * kTile));
    }
    // Off to the lamp nearest it -- its time round Leon over -- and that lamp's from now on.
    void lampFlySettle(Fly& f) {
        f.lampT = -1;
        for (size_t k = 0; k < level.lamps.size(); k++)
            if (Vector2Distance(f.p, lampTop(k)) < Vector2Distance(f.p, lampTop(f.lamp))) f.lamp = (int)k;
        if (trace) fprintf(stderr, "t=%.2f the lamp's fly goes to lamp %d\n", time, f.lamp);
    }
    void lampStep(Fly& f, size_t i, float dt) {
        if (f.lampT >= 0) {
            goldenStep(f, i, dt);
            if ((f.lampT -= dt) < 0) lampFlySettle(f);
            return;
        }
        f.orbit += dt * 2.4f;
        Vector2 top = lampTop(f.lamp);
        Vector2 out = Vector2Scale(level.lampDir[f.lamp], 4);   // (just past the disk, away from the pole)
        flyChase(f, {top.x + out.x + cosf(f.orbit) * 30, top.y + out.y + sinf(f.orbit) * 9 + 5 * sinf(time * 1.1f + i)}, dt);
    }

    // Out of a Monstermunch, a fly finds its way home as an active Brownian particle: it swims at
    // monstermunch_freed_fly_speed of a golden fly's top speed, its heading wandering at random
    // (monstermunch_freed_fly_noise: rotational diffusion, radians squared a second) as it turns,
    // gently, towards home -- its pole's balloon, its lamp; there, it circles as ever. (Through walls.)
    void freedStep(Fly& f, float dt) {
        Vector2 d = Vector2Subtract(f.pole >= 0 ? flyHome(f.pole) : lampTop(f.lamp), f.p);
        if (Vector2Length(d) < 40) { f.freed = false; return; }
        float u1 = fmaxf(1e-6f, GetRandomValue(1, 10000) / 10000.0f), u2 = GetRandomValue(0, 10000) / 10000.0f;
        float g = sqrtf(-2 * logf(u1)) * cosf(2 * PI * u2);           // (a normal draw)
        f.head += 1.5f * sinf(atan2f(d.y, d.x) - f.head) * dt + sqrtf(2 * t.mmFreedNoise * dt) * g;
        float v0 = t.mmFreedSpeed * t.goldMaxSpeed * t.maxFall;
        f.v = {cosf(f.head) * v0, sinf(f.head) * v0};
        f.p = Vector2Add(f.p, Vector2Scale(f.v, dt));
    }
    // Towards a point that moves, as a golden fly does: never faster than golden_fly_max_speed of
    // his top falling speed -- through walls, if it must.
    void flyChase(Fly& f, Vector2 target, float dt) {
        Vector2 want = Vector2Scale(Vector2Subtract(target, f.p), 1.0f / t.goldDelay);
        float top = t.goldMaxSpeed * t.maxFall, L = Vector2Length(want);
        if (L > top) want = Vector2Scale(want, top / L);
        Vector2 kick = {GetRandomValue(-100, 100) / 100.0f, GetRandomValue(-100, 100) / 100.0f};
        f.v = Vector2Add(f.v, Vector2Add(Vector2Scale(Vector2Subtract(want, f.v), 1 - expf(-dt * 8)),
                                         Vector2Scale(kick, t.flyJitter * 0.25f * dt)));
        L = Vector2Length(f.v);
        if (L > top) f.v = Vector2Scale(f.v, top / L);
        f.p = Vector2Add(f.p, Vector2Scale(f.v, dt));
    }

    void flyStep(float dt) {
        Vector2 axis = toWorld({1, 0});                 // left/right is always across gravity
        for (size_t i = 0; i < flies.size(); i++) {
            Fly& f = flies[i];
            if (!inZone(f.p)) continue;                 // (far from him: left where it is)
            f.phase += dt * 60;
            if (f.inMouth) continue;                    // being chewed
            if (f.caught) {
                f.freed = false;
                if ((int)i == caughtFly && (tg == tgBack || tg == tgOut || tg == tgDead))
                    f.p = tongueTip();
                continue;
            }
            if (f.freed) { freedStep(f, dt); continue; }
            if (f.pole >= 0) { poleFlyStep(f, i, dt); continue; }
            if (f.lamp >= 0) { lampStep(f, i, dt); continue; }
            if (f.golden) { goldenStep(f, i, dt); continue; }
            if (GetRandomValue(0, 1000) < 1000 * 0.15f * dt) f.dir = -f.dir;   // now and then, a change of mind
            Vector2 want = Vector2Scale(axis, f.dir * t.flySpeed);
            Vector2 kick = {GetRandomValue(-100, 100) / 100.0f, GetRandomValue(-100, 100) / 100.0f};
            f.v = Vector2Add(f.v, Vector2Add(Vector2Scale(Vector2Subtract(want, f.v), 2.5f * dt),
                                             Vector2Scale(kick, t.flyJitter * dt)));
            Vector2 np = Vector2Add(f.p, Vector2Scale(f.v, dt));
            bool out = np.x < kTile || np.y < kTile || np.x > (level.w - 1) * kTile || np.y > (level.h - 1) * kTile;
            if (out || obstacleAt(np)) {               // bump: the other way
                f.dir = -f.dir;
                f.v = Vector2Scale(f.v, -0.6f);
            } else {
                f.p = np;
            }
        }
    }

    void chomp(float open) {
        mouth = fmaxf(mouth, open);
        squash = fmaxf(squash, 0.09f); squashVel = 0;
    }

    void tongueThrow(Vector2 dirW) {
        tg = tgOut;
        tgDir = Vector2Normalize(dirW);
        tgExt = 0;
        tgBnc.clear();
        tgMax = tongueReach();
        tgResend = false;
        chewLeft = 0;
        sound(Audio::kTongueOut);
        spitDrops(centreW(), tgDir, GetRandomValue(1, 3), 160);
        if (trace) fprintf(stderr, "t=%.2f tongue out (%.2f,%.2f)\n", time, tgDir.x, tgDir.y);
    }

    void tongueRetract(float duration) {
        if (tg == tgStuck) {               // swallowed along the straight line to the nearest bend
            // Letting go of a stuck tongue: saliva pours out of the mouth -- and again when it is swallowed.
            spitDrops(centreW(), Vector2Normalize(Vector2Add(mouthDir(), pullW)), GetRandomValue(5, 8), 70);
            pourOnSwallow = true;
            sound(Audio::kRelease);
            Vector2 d = Vector2Subtract(piv.back(), centreW());
            tgExt = Vector2Length(d);
            tgDir = (tgExt > 0.01f) ? Vector2Scale(d, 1 / tgExt) : tgDir;
        }
        if (tg == tgDead && rope.size() >= 2) {   // a dead one: reeled in along the way it lies
            size_t n = rope.size();
            tgBnc.assign(rope.begin() + 1, rope.end() - 1);
            Vector2 d = Vector2Subtract(rope[n - 1], rope[n - 2]);
            if (Vector2Length(d) > 0.01f) tgDir = Vector2Normalize(d);
            tgExt = 0;
            for (size_t i = 1; i < n; i++) tgExt += Vector2Distance(rope[i - 1], rope[i]);
            rope.clear();
        }
        tg = tgBack;
        tgCrate = -1; pulling = false;
        tgBackSpeed = tongueReach() / duration;
        whip = 0;
        if (trace) fprintf(stderr, "t=%.2f tongue back, Leon speed (%.0f,%.0f)\n", time, vel.x, vel.y);
    }

    void pressTongue(Vector2 dirW) {
        if (!canTongue) return;             // not yet: the tongue fruit gives it
        if (knock != kNone) return;
        flyChew = -1;                       // any action interrupts chewing
        if (tg == tgNone) tongueThrow(dirW);
        else if (tg == tgStuck || tg == tgDead) tongueRetract(t.tongueBack);   // let go -- or, dead, reeled in
        else {                              // in flight: snap it back and throw again
            tongueRetract(t.tongueQuick);
            tgResend = true;
            tgResendDir = dirW;
        }
    }

    void stick(Vector2 at, char c) {
        tg = tgStuck;
        tgBnc.clear();                     // (after bounces too: straight from him to where it holds)
        piv.assign(1, at);
        pivSide.assign(1, 0);
        tgLen = fmaxf(1.0f, Vector2Distance(at, centreW()));
        resetRope();
        sound(Audio::kStick);
        spitClock = t.dripEvery * GetRandomValue(30, 100) / 100.0f;   // the first drop comes soon-ish
        if (trace) fprintf(stderr, "t=%.2f tongue stuck at (%.0f,%.0f) on '%c', length %.0f\n", time, at.x, at.y, c, tgLen);
        requestGravity(c);
    }

    void swallow() {
        blink = -0.001f;                   // blink as it goes down
        if (hatOnTongue) mouth = 0;        // (the tongue putting his hat on: no gulp -- the hat stays as it was drawn on it)
        else {
            squash = 0.16f; squashVel = 0; // ...and gulp
            mouth = 1.0f;
        }
        sound(Audio::kSwallow);
        spitDrops(centreW(), tgDir, GetRandomValue(1, 2), 90);
        if (pourOnSwallow) {                // it had been stuck: the swallow pours saliva out
            pourOnSwallow = false;
            spitDrops(centreW(), Vector2Normalize(Vector2Add(mouthDir(), pullW)), GetRandomValue(5, 8), 70);
        }
        if (caughtPine >= 0) { int i = caughtPine; caughtPine = -1; eatPine(i); }
        if (hatOnTongue) hatBack();         // the hat on it: on his head
        if (caughtFly >= 0 && flies[caughtFly].pole >= 0) {   // a colour fly: swallowed whole, kept in his belly, glowing
            if (bellyFly >= 0) spitBellyFly();                // (one already in there goes first)
            Fly& f = flies[caughtFly];
            f.caught = false; f.inMouth = true; f.v = {0, 0};
            bellyFly = caughtFly; bellyT = t.bellyTime;
            caughtFly = -1;
            if (trace) fprintf(stderr, "t=%.2f colour fly swallowed (pole %d): in his belly\n", time, f.pole);
        }
        if (caughtFly >= 0) {               // a fly: chew it -- one, two ... three -- then cough it out
            if (mouthFly >= 0) coughFly();  // (one still in there goes first)
            Fly& f = flies[caughtFly];
            f.caught = false;
            f.inMouth = true;
            f.v = {0, 0};
            mouthFly = caughtFly;
            mouthT = f.lamp >= 0 ? t.lampBelly : 0;
            caughtFly = -1;
            flyChew = 0; chews = 0;
            if (trace) fprintf(stderr, "t=%.2f fly eaten\n", time);
        }
        if (trace) fprintf(stderr, "t=%.2f tongue swallowed\n", time);
    }

    // Chewed (or cut short): he coughs the fly back out -- golden now, and his for ever.
    void coughFly() {
        Fly& f = flies[mouthFly];
        mouthFly = -1;
        f.inMouth = false; f.caught = false; f.golden = true;
        if (f.lamp >= 0) f.lampT = t.lampFollow;           // a lamp's fly: his for a while only
        Vector2 d = toWorld({facing, 0});
        f.p = Vector2Add(centreW(), Vector2Scale(d, 16));
        f.v = Vector2Add(Vector2Scale(d, 240), Vector2Scale(pullW, -90));   // shot out, a little up
        f.orbit = GetRandomValue(0, 628) / 100.0f;
        chomp(0.9f);                                   // the cough: mouth wide, a jolt...
        squash = 0.15f; squashVel = 0;
        for (int k = 0; k < 6; k++) {                  // ...and a little puff
            Particle q;
            q.p = {pos.x + facing * 14, pos.y - kLeonH / 2 + 2};
            q.v = {facing * GetRandomValue(50, 150), -(float)GetRandomValue(0, 50)};
            q.maxLife = q.life = GetRandomValue(25, 45) / 100.0f;
            q.r = GetRandomValue(20, 40) / 10.0f;
            q.spark = false;
            emit(q);
        }
        sound(Audio::kCough);
        if (trace) fprintf(stderr, "t=%.2f coughed the fly back out: golden\n", time);
    }

    float ropeRemaining() const {
        float used = 0;
        for (size_t i = 1; i < piv.size(); i++) used += Vector2Distance(piv[i - 1], piv[i]);
        return fmaxf(1.0f, tgLen - used);
    }

    void resetRope() {
        const int n = 24;
        Vector2 a = piv.back(), b = centreW();
        rope.resize(n); ropePrev.resize(n);
        for (int i = 0; i < n; i++) rope[i] = ropePrev[i] = Vector2Lerp(a, b, i / (float)(n - 1));
    }

    static float cross2(Vector2 a, Vector2 b) { return a.x * b.y - a.y * b.x; }
    static int sgn(float v) { return (v > 1e-4f) - (v < -1e-4f); }

    // Corner candidates for wrapping: convex corners of whole-tile blocks, and slab ends.
    // Each is nudged a hair outwards so the rope leaving it does not touch its block.
    template <class F> void corners(Rectangle area, F f) const {
        int gx0 = (int)floorf(area.x / kTile), gx1 = (int)ceilf((area.x + area.width) / kTile);
        int gy0 = (int)floorf(area.y / kTile), gy1 = (int)ceilf((area.y + area.height) / kTile);
        const float e = 0.75f;
        for (int gy = gy0; gy <= gy1; gy++)
            for (int gx = gx0; gx <= gx1; gx++) {
                int n = 0, sx = 0, sy = 0;
                for (int dy = -1; dy <= 0; dy++)
                    for (int dx = -1; dx <= 0; dx++)
                        if (level.block(gx + dx, gy + dy)) { n++; sx = gx + dx; sy = gy + dy; }
                if (n == 1)
                    f(Vector2{gx * kTile + (sx < gx ? e : -e), gy * kTile + (sy < gy ? e : -e)}, level.at(sx, sy));
                if (level.oneway(gx, gy)) {
                    float X = gx * kTile, Y = gy * kTile;
                    if (!level.oneway(gx - 1, gy) && !level.block(gx - 1, gy)) {
                        f(Vector2{X - e, Y - e}, '='); f(Vector2{X - e, Y + kSlabH + e}, '=');
                    }
                    if (!level.oneway(gx + 1, gy) && !level.block(gx + 1, gy)) {
                        f(Vector2{X + kTile + e, Y - e}, '='); f(Vector2{X + kTile + e, Y + kSlabH + e}, '=');
                    }
                }
            }
    }

    static bool inTriangle(Vector2 p, Vector2 a, Vector2 b, Vector2 c) {
        float d1 = cross2(Vector2Subtract(b, a), Vector2Subtract(p, a));
        float d2 = cross2(Vector2Subtract(c, b), Vector2Subtract(p, b));
        float d3 = cross2(Vector2Subtract(a, c), Vector2Subtract(p, c));
        return (d1 > 0 && d2 > 0 && d3 > 0) || (d1 < 0 && d2 < 0 && d3 < 0);
    }

    // The rope's straight end swept the triangle (last bend, previous Leon, current Leon) this
    // step: the first corner it met becomes a new bend.
    bool findWrap(Vector2 last, Vector2 from, Vector2 to, float reach, Vector2& out, char& tile) const {
        float x0 = fminf(last.x, fminf(from.x, to.x)), y0 = fminf(last.y, fminf(from.y, to.y));
        float x1 = fmaxf(last.x, fmaxf(from.x, to.x)), y1 = fmaxf(last.y, fmaxf(from.y, to.y));
        Vector2 ref = Vector2Normalize(Vector2Subtract(from, last));
        float best = 1e9f;
        bool found = false;
        corners({x0, y0, x1 - x0, y1 - y0}, [&](Vector2 c, char ch) {
            if (Vector2Distance(c, last) < 1.5f || Vector2Distance(c, last) > reach) return;
            if (!clearLine(last, c)) return;            // behind something: not this one
            if (!inTriangle(c, last, from, to)) return;
            float ang = acosf(clampf(Vector2DotProduct(ref, Vector2Normalize(Vector2Subtract(c, last))), -1, 1));
            if (ang < best) { best = ang; out = c; tile = ch; found = true; }
        });
        return found;
    }

    // Stuck: bend round corners, unbend, then -- only if taut -- hold Leon at the rope's length.
    void ropeStep(float dt) {
        if (tgCrate >= 0) piv[0] = Vector2Add(crates[tgCrate].c, tgCrateOff);   // the anchor rides on its block
        Vector2 P = centreW();
        for (int k = 0; k < 4; k++) {
            Vector2 c; char ch = 0;
            if (!findWrap(piv.back(), prevCentreW, P, ropeRemaining(), c, ch)) break;
            int side = sgn(cross2(Vector2Subtract(c, piv.back()), Vector2Subtract(P, c)));
            if (side == 0) side = sgn(cross2(Vector2Subtract(c, piv.back()), Vector2Subtract(prevCentreW, c)));
            piv.push_back(c);
            pivSide.push_back(side);
            resetRope();
            if (trace) fprintf(stderr, "t=%.2f tongue bends round (%.0f,%.0f)\n", time, c.x, c.y);
            if (ch && ch != '#' && ch != '=') requestGravity(ch);
        }
        while (piv.size() > 1) {
            size_t n = piv.size();
            int s = sgn(cross2(Vector2Subtract(piv[n - 1], piv[n - 2]), Vector2Subtract(P, piv[n - 1])));
            bool swungBack = pivSide[n - 1] == 0 || (s != 0 && s != pivSide[n - 1]);
            if (!swungBack || !clearLine(piv[n - 2], P)) break;   // only ever to a clear straight line
            piv.pop_back(); pivSide.pop_back();
            resetRope();
            if (trace) fprintf(stderr, "t=%.2f tongue unbends\n", time);
        }
        // RT (keyboard V): each pull swallows one chew -- a body height of tongue -- eyes
        // popping. LT (keyboard C) held: it pays out fast. Otherwise it slowly stretches out
        // to its full reach.
        bool stretching = false;
        {
            if (payHeld && !paying && tgLen < tongueReach()) { eyeBig = 1; payDrip = 0; }   // eyes pop as he lets it run
            paying = payHeld && tgLen < tongueReach();
            float used = tgLen - ropeRemaining();
            if (chewPress) {
                chewPress = false;
                chewLeft += kLeonDrawH;
                eyeBig = 1;
                chomp(0.9f);
                // ...and a little fountain of saliva, up against gravity.
                spitDrops(centreW(), Vector2Negate(pullW), GetRandomValue(2, 3), 260);
                if (trace) fprintf(stderr, "t=%.2f tongue chewed back\n", time);
            }
            if (chewLeft > 0) {
                float k = fminf(chewLeft, kLeonDrawH / t.chewTime * dt);
                chewLeft -= k;
                tgLen = fmaxf(used + 6.0f, tgLen - k);
            } else if (tgLen < tongueReach()) {
                float rate = t.tongueStretch * (paying ? t.tongueFastStretch : 1.0f);
                tgLen = fminf(tongueReach(), tgLen + rate * dt);
                stretching = true;
                if (paying && (payDrip -= dt) <= 0) {   // ...and drools a little more
                    payDrip = 0.35f;
                    spitDrops(centreW(), Vector2Normalize(Vector2Add(mouthDir(), pullW)), 1, 50);
                }
            }
        }
        // Safety net: if the free end still runs through something, bend it round the corner
        // that gives the shortest clear path.
        for (int k = 0; k < 2 && !clearLine(piv.back(), P); k++) {
            Vector2 last = piv.back(), best{};
            float bestLen = 1e9f;
            char bestCh = 0;
            Rectangle area = {fminf(last.x, P.x) - kTile, fminf(last.y, P.y) - kTile,
                              fabsf(last.x - P.x) + 2 * kTile, fabsf(last.y - P.y) + 2 * kTile};
            corners(area, [&](Vector2 c, char ch) {
                float len = Vector2Distance(last, c) + Vector2Distance(c, P);
                if (len < bestLen && Vector2Distance(last, c) > 1.5f && clearLine(last, c) && clearLine(c, P)) {
                    bestLen = len; best = c; bestCh = ch;
                }
            });
            if (bestLen > 1e8f) break;
            int side = sgn(cross2(Vector2Subtract(best, last), Vector2Subtract(P, best)));
            piv.push_back(best);
            pivSide.push_back(side);
            resetRope();
            if (trace) fprintf(stderr, "t=%.2f tongue caught on (%.0f,%.0f) (safety net)\n", time, best.x, best.y);
            if (bestCh && bestCh != '#' && bestCh != '=') requestGravity(bestCh);
        }
        // Stuck to a block and pulling: the block is dragged along the tongue (never lifted --
        // only across gravity, or with it), and Leon goes at block_pull_speed.
        pulling = false;
        if (tgCrate >= 0) {
            Vector2 next = piv.size() > 1 ? piv[1] : P;
            float excess = Vector2Distance(P, piv.back()) - ropeRemaining();
            if (excess > 0) {
                Vector2 dir = Vector2Normalize(Vector2Subtract(next, piv[0]));
                Vector2 mv = Vector2Scale(dir, excess);
                float up = Vector2DotProduct(mv, pullW);
                if (up < 0) mv = Vector2Subtract(mv, Vector2Scale(pullW, up));
                Vector2 moved = crateMove(tgCrate, mv);
                piv[0] = Vector2Add(crates[tgCrate].c, tgCrateOff);
                if (Vector2Length(moved) > 0.05f) {
                    pulling = onGround;
                    crateKeepSpeed(tgCrate, moved, dt);   // it keeps that speed (and so slides on, on ice)
                }
            }
        }
        float rem = ropeRemaining();
        Vector2 last = piv.back();
        Vector2 d = Vector2Subtract(P, last);
        float dist = Vector2Length(d);
        tgTaut = dist >= rem - 0.5f;
        if (stretching && tgTaut)          // rubbing round its corners as it pays out
            for (size_t i = 1; i < piv.size(); i++)
                if (GetRandomValue(0, 1000) < 1000 * 7.0f * dt) rubDust(piv[i]);
        // Fully paid out, walking away on the ground: no wall -- the tongue gives a little, and
        // the further it gives the harder it pulls back, so he slows to a stop (and is tugged
        // back a touch when he lets go). Beyond the give, it holds hard.
        bool full = tgLen >= tongueReach() - 0.01f && chewLeft <= 0;
        if (full && onGround && dist > rem && dist > 0.001f) {
            Vector2 n = Vector2Scale(d, 1 / dist);
            float over = dist - rem;
            Vector2 vw = toWorld(vel);
            // Away from the sticking point he may go no faster than the tongue paid out a moment
            // ago, less and less as it gives -- a smooth brake to a stop -- and it tugs back.
            float vr = Vector2DotProduct(vw, n);
            float vmax = t.tongueStretch * fmaxf(0, 1 - over / fmaxf(1, t.tongueGive));
            if (vr > vmax) vw = Vector2Subtract(vw, Vector2Scale(n, vr - vmax));
            vw = Vector2Subtract(vw, Vector2Scale(n, t.tongueGiveSpring * over * dt));
            vel = toLocal(vw);
            rem += t.tongueGive;                    // the hard limit sits at the end of the give
        }
        if (dist > rem && dist > 0.001f) {
            Vector2 n = Vector2Scale(d, 1 / dist);
            Vector2 dl = toLocal(Vector2Subtract(Vector2Add(last, Vector2Scale(n, rem)), P));
            moveX(dl.x);
            moveY(dl.y);
            Vector2 vw = toWorld(vel);
            float vr = Vector2DotProduct(vw, n);
            if (vr > 0) vw = Vector2Subtract(vw, Vector2Scale(n, vr));   // the rope takes the outward speed
            vel = toLocal(vw);
        }
        int rubbing = ropeVisual(dt, rem);
        if (stretching && rubbing >= 0 && GetRandomValue(0, 1000) < 1000 * 5.0f * dt) rubDust(rope[rubbing]);
        // Saliva: one drop at a time, about every tongue_drip_interval, from the tongue's lowest
        // point -- lowest along the current gravity.
        if (t.spit) {
            spitClock -= dt;
            if (spitClock <= 0) {
                spitClock = t.dripEvery * GetRandomValue(80, 120) / 100.0f;
                Vector2 low = piv[0];
                auto consider = [&](Vector2 q) { if (Vector2DotProduct(q, pullW) > Vector2DotProduct(low, pullW)) low = q; };
                for (auto& q : piv) consider(q);
                for (size_t i = 1; i + 1 < rope.size(); i++) consider(rope[i]);
                spitDrops(low, pullW, 1, 15);
                if (trace) fprintf(stderr, "t=%.2f saliva drop at (%.0f,%.0f)\n", time, low.x, low.y);
            }
        }
    }

    // The free part of the rope as a verlet chain pinned at both ends: it sags when slack,
    // drapes over what it touches, and pulls itself straight when taut.
    // Returns the index of a chain point that touched an obstacle (-1 if none).
    int ropeVisual(float dt, float rem) {
        int n = (int)rope.size();
        if (n < 2) return -1;
        int touched = -1;
        Vector2 a = piv.back(), b = centreW();
        Vector2 g = Vector2Scale(pullW, t.gravity * t.tongueSag * dt * dt);
        for (int i = 1; i < n - 1; i++) {
            Vector2 v = Vector2Scale(Vector2Subtract(rope[i], ropePrev[i]), 0.97f);
            ropePrev[i] = rope[i];
            rope[i] = Vector2Add(Vector2Add(rope[i], v), g);
        }
        float rest = rem / (n - 1);
        for (int it = 0; it < 16; it++) {
            rope[0] = a; rope[n - 1] = b;
            for (int i = 0; i < n - 1; i++) {
                Vector2 d = Vector2Subtract(rope[i + 1], rope[i]);
                float L = Vector2Length(d);
                if (L < 1e-4f) continue;
                Vector2 corr = Vector2Scale(d, (L - rest) / L * 0.5f);
                if (i > 0) rope[i] = Vector2Add(rope[i], corr);
                if (i + 1 < n - 1) rope[i + 1] = Vector2Subtract(rope[i + 1], corr);
            }
            // Out the way each point came in (so it cannot pop through a thin slab), then any
            // segment cutting a corner is pushed off it.
            for (int i = 1; i < n - 1; i++) if (pushOut(rope[i], ropePrev[i])) touched = i;
            for (int i = 0; i < n - 1; i++) {
                Vector2 mid = Vector2Lerp(rope[i], rope[i + 1], 0.5f), out = mid;
                if (!pushOut(out, out)) continue;
                Vector2 dlt = Vector2Subtract(out, mid);
                bool pinA = (i == 0), pinB = (i + 1 == n - 1);
                if (!pinA) rope[i] = Vector2Add(rope[i], pinB ? Vector2Scale(dlt, 2) : dlt);
                if (!pinB) rope[i + 1] = Vector2Add(rope[i + 1], pinA ? Vector2Scale(dlt, 2) : dlt);
                touched = std::max(touched, i);
            }
        }
        rope[0] = a; rope[n - 1] = b;
        // Taut (pulling, swinging): it is a straight line -- the chain alone cannot quite manage that.
        if (tgTaut)
            for (int i = 1; i < n - 1; i++) {
                Vector2 straight = Vector2Lerp(a, b, i / (float)(n - 1));
                Vector2 q = Vector2Lerp(rope[i], straight, 0.5f);
                if (!obstacleAt(q)) rope[i] = q;
            }
        return touched;
    }

    // Moves a world point out of any obstacle: through the face it came in by (`from`, its
    // position before), or -- if that tells nothing -- by the shortest way.
    bool pushOut(Vector2& p, Vector2 from) const {
        int tx = (int)floorf(p.x / kTile), ty = (int)floorf(p.y / kTile);
        Rectangle r;
        int ci = crateAt(p);
        if (ci >= 0) r = crateRect(ci);
        else if (!tileBox(tx, ty, r)) return false;
        if (!CheckCollisionPointRec(p, r)) return false;
        if (from.y <= r.y) { p.y = r.y - 0.1f; return true; }
        if (from.y >= r.y + r.height) { p.y = r.y + r.height + 0.1f; return true; }
        if (from.x <= r.x) { p.x = r.x - 0.1f; return true; }
        if (from.x >= r.x + r.width) { p.x = r.x + r.width + 0.1f; return true; }
        float l = p.x - r.x, rr = r.x + r.width - p.x, u = p.y - r.y, dn = r.y + r.height - p.y;
        float m = fminf(fminf(l, rr), fminf(u, dn));
        if (m == l) p.x = r.x - 0.1f;
        else if (m == rr) p.x = r.x + r.width + 0.1f;
        else if (m == u) p.y = r.y - 0.1f;
        else p.y = r.y + r.height + 0.1f;
        return true;
    }

    // Where the flying tongue meets the surface it hit at distance `d` (first sample inside; `back`:
    // how far before it the last one outside was -- its stride): the boundary found by bisection,
    // then set 1 px out from the face it came in by, along its normal (`normal`, if asked) -- never a
    // hair inside the obstacle, whatever the angle it came in at, nor out by a face with more of the
    // obstacle just across it (ice tiles side by side: their seam is no face to bounce off).
    Vector2 surfacePoint(Vector2 m, float d, Vector2* normal = nullptr) const { return surfacePoint(m, d, tgDir, 2.0f, normal); }
    Vector2 surfacePoint(Vector2 m, float d, Vector2 dir, float back = 2.0f, Vector2* normal = nullptr) const {   // (any tongue: from m along dir)
        float lo = fmaxf(0, d - back), hi = d;
        for (int i = 0; i < 12; i++) {
            float mid = (lo + hi) / 2;
            if (obstacleAt(Vector2Add(m, Vector2Scale(dir, mid)))) hi = mid; else lo = mid;
        }
        Vector2 out = Vector2Add(m, Vector2Scale(dir, lo)), in = Vector2Add(m, Vector2Scale(dir, hi));
        Rectangle r;
        int ci = crateAt(in);
        if (ci >= 0) r = crateRect(ci);
        else if (!tileBox((int)floorf(in.x / kTile), (int)floorf(in.y / kTile), r)) return out;
        // How far inside each face it is: the face it came in by is the one it is still out of (below
        // 0) -- else, buried, the nearest; either way one with nothing just across it, if any.
        const Vector2 to[4] = {{r.x - 1.0f, out.y}, {r.x + r.width + 1.0f, out.y}, {out.x, r.y - 1.0f}, {out.x, r.y + r.height + 1.0f}};
        const Vector2 nrm[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        const float depth[4] = {out.x - r.x, r.x + r.width - out.x, out.y - r.y, r.y + r.height - out.y};
        int k = -1;
        for (int j = 0; j < 4; j++)
            if (!obstacleAt(to[j]) && (k < 0 || depth[j] < depth[k])) k = j;
        if (k < 0) for (int j = 0; j < 4; j++) if (k < 0 || depth[j] < depth[k]) k = j;
        if (normal) *normal = nrm[k];
        return to[k];
    }

    void tongueStep(float dt) {
        Vector2 m = centreW();
        if (tg == tgOut) {
            tgExt = fminf(tgMax, tgExt + tgMax / t.tongueSend * dt);
            // A straight line from the mouth -- or from where it last bounced: it sticks to the
            // first thing on it. Ice it cannot stick to: it bounces off, as a ball off a wall, and
            // flies on (tgBnc), to stick farther on -- or come back, at the end of its reach.
            for (bool again = true; again && tg == tgOut;) {
                again = false;
                Vector2 o = m;                                // this stretch's start, and the length before it
                float base = 0;
                for (const Vector2& b : tgBnc) { base += Vector2Distance(o, b); o = b; }
                for (float d = 0; d <= tgExt - base; d += 2.0f) {
                    Vector2 q = Vector2Add(o, Vector2Scale(tgDir, d));
                    char c = obstacleAt(q);
                    if (c == 'X' && !god) {  // spikes: the tongue is torn -- and Leon with it (GOD: it sticks, as to rock)
                        tgExt = base + d;
                        if (trace) fprintf(stderr, "t=%.2f tongue hit the spikes\n", time);
                        die();
                        return;
                    }
                    if (tongueBounces(c)) {  // slippery (ice, or ice gravity): off it, the way a ball bounces
                        Vector2 n, at = surfacePoint(o, d, &n);
                        Rectangle r;
                        int tx = (int)floorf(q.x / kTile), ty = (int)floorf(q.y / kTile);
                        if ((int)tgBnc.size() >= 8 || !tileBox(tx, ty, r)) { tgExt = base + d; goDead(); break; }
                        if (n.x != 0) tgDir.x = -tgDir.x;   // off a side...
                        if (n.y != 0) tgDir.y = -tgDir.y;   // ...or the top or bottom
                        tgBnc.push_back(at);
                        sound(Audio::kStick, 1.8f, 0.45f);
                        if (trace) fprintf(stderr, "t=%.2f tongue bounces off ice at (%.0f,%.0f)\n", time, at.x, at.y);
                        again = true;
                        break;
                    }
                    if (c) {
                        Vector2 at = surfacePoint(o, d);
                        tgCrate = (c == 'B') ? crateAt(q) : -1;
                        if (tgCrate >= 0) { tgCrateOff = Vector2Subtract(at, crates[tgCrate].c); touchCrate(tgCrate, "tongue"); }
                        bool bounced = !tgBnc.empty();
                        float pathLen = base + Vector2Distance(o, at);
                        stick(at, c);
                        if (bounced) {                // after ice: it holds with all it ran out -- slack, to be swallowed taut
                            tgLen = fmaxf(tgLen, pathLen);
                            if (trace) fprintf(stderr, "t=%.2f   (after bouncing: slack, length %.0f)\n", time, tgLen);
                        }
                        break;
                    }
                }
            }
            // ...or snaps up a fly on the way, and brings it back.
            if (tg == tgOut && caughtFly < 0) {      // a pineapple, too
                for (size_t i = 0; i < level.pines.size(); i++) {
                    if (pineEaten[i] || pineAway(i) || !level.pineapple(i)) continue;
                    if (tongueTouches(level.pines[i], 16.0f)) {   // it stays where it hangs: the tongue lights it
                        if (trace) fprintf(stderr, "t=%.2f pineapple caught\n", time);
                        eatPine((int)i);
                        if (tgBnc.empty()) tongueRetract(t.tongueBack); else goDead();   // (past ice: dead)
                        break;
                    }
                }
            }
            if (tg == tgOut && caughtFly < 0) {
                for (size_t i = 0; i < flies.size(); i++) {
                    const Fly& fi = flies[i];
                    bool atPost = fi.lamp >= 0 && fi.lampT < 0;  // (a lamp's fly at its post can be caught)
                    if (fi.caught || fi.inMouth || (fi.golden && !atPost)) continue;   // golden ones are his
                    if (tongueTouches(flies[i].p, 10.0f)) {
                        flies[i].caught = true;
                        caughtFly = (int)i;
                        Vector2 tip = tongueTip();
                        if (trace) fprintf(stderr, "t=%.2f fly caught at (%.0f,%.0f)\n", time, tip.x, tip.y);
                        if (tgBnc.empty()) tongueRetract(t.tongueBack); else goDead();   // (past ice: dead, the fly on it)
                        break;
                    }
                }
            }
            if (tg == tgOut && caughtFly < 0 && hatLoose && !hatOnTongue && tongueTouches(hatP, 14.0f)) {   // his hat, lost: brought back
                hatOnTongue = true;
                if (trace) fprintf(stderr, "t=%.2f hat caught on the tongue at (%.0f,%.0f)\n", time, hatP.x, hatP.y);
                tongueRetract(t.tongueBack);                 // (past ice too, back the way it went: never dead -- nothing he does counts till it is on)
            }
            if (tg == tgOut && tgExt >= tgMax) {             // its whole reach out, nothing held
                if (tgBnc.empty()) tongueRetract(t.tongueBack); else goDead();
            }
        } else if (tg == tgBack) {
            whip += dt;
            if (hatOnTongue && hatPutT < 0 && tgExt <= kHatPutFrom) {   // the hat near: the tongue curls up to put it on
                hatPut0 = Vector2Subtract(tongueTip(), m);
                hatPutT = 0; tgBnc.clear(); tgDir = hatPutDir(); tgExt = 30;
                if (trace) fprintf(stderr, "t=%.2f the tongue puts the hat back on\n", time);
            }
            bool in;
            if (hatPutT >= 0) in = (hatPutT += dt) >= kHatPutT;
            else in = (tgExt -= tgBackSpeed * dt) <= 0;
            if (in) {
                tgExt = 0;
                tg = tgNone;
                swallow();
                if (tgResend) { tgResend = false; tongueThrow(tgResendDir); }
            }
        } else if (tg == tgStuck) {
            ropeStep(dt);
        } else if (tg == tgDead) {
            deadStep(dt);
        }
        if (tg != tgStuck) { chewPress = false; paying = false; }
        prevCentreW = centreW();
    }

    // The first point of a stuck tongue OUTSIDE the body, counting from the mouth: the chain
    // index, or -1 for the nearest bend. From the body centre to it the tongue is drawn
    // straight, so the part in the mouth always joins the first segment outside.
    // Only a point the mouth can see in a straight line counts: the tongue never cuts a corner
    // (say the edge of the platform he stands on) to get there.
    int tongueExit() const {
        Vector2 c = centreW();
        int seen = -1;
        for (int i = (int)rope.size() - 2; i >= 1; i--) {
            if (!clearLine(c, rope[i])) break;
            seen = i;
            if (Vector2Distance(rope[i], c) >= 24.0f) return i;
        }
        return seen >= 0 ? seen : -1;
    }

    bool clearLine(Vector2 a, Vector2 b) const {
        float L = Vector2Distance(a, b);
        for (float d = 1.5f; d < L - 1.0f; d += 2.0f)
            if (obstacleAt(Vector2Lerp(a, b, d / L))) return false;
        return true;
    }

    // Where the mouth points -- along that first outward segment -- and the eyes follow it.
    Vector2 mouthDir() const {
        if (tg == tgStuck) {
            int i = tongueExit();
            Vector2 d = Vector2Subtract(i >= 0 ? rope[i] : piv.back(), centreW());
            if (Vector2Length(d) > 0.5f) return Vector2Normalize(d);
        }
        if (tg == tgNone) return toWorld({facing, 0});   // chewing: straight ahead
        if (tg == tgDead && rope.size() >= 2) {          // dead: the way it hangs out of his mouth
            Vector2 d = Vector2Subtract(rope[std::min<size_t>(3, rope.size() - 1)], centreW());
            if (Vector2Length(d) > 0.5f) return Vector2Normalize(d);
        }
        if (!tgBnc.empty()) {                            // bounced: along the first stretch
            Vector2 d = Vector2Subtract(tgBnc[0], centreW());
            if (Vector2Length(d) > 0.5f) return Vector2Normalize(d);
        }
        return tgDir;
    }

    // A tongue in flight (or coming back): its path from `from` (his centre) through the points
    // where it bounced off ice, to its tip, tgExt along it.
    std::vector<Vector2> tonguePath(Vector2 from) const {
        std::vector<Vector2> pts{from};
        float left = tgExt;
        Vector2 prev = from;
        for (const Vector2& b : tgBnc) {
            float seg = Vector2Distance(prev, b);
            if (left <= seg) { pts.push_back(Vector2Lerp(prev, b, seg > 0.001f ? left / seg : 0)); return pts; }
            left -= seg;
            pts.push_back(b);
            prev = b;
        }
        pts.push_back(Vector2Add(prev, Vector2Scale(tgDir, left)));
        return pts;
    }
    Vector2 tongueTip() const { return tg == tgDead && !rope.empty() ? rope.back() : tonguePath(centreW()).back(); }
    // The hat on its tip, put back: the tongue leaves his mouth up and to the side the hat came from
    // (hatPutDir), its tip swinging out that way and over onto his head -- where the hat sits
    // (its middle 11 px above its brim).
    Vector2 hatPutDir() const {
        float side = Vector2DotProduct(hatPut0, toWorld({1, 0})) >= 0 ? 1.0f : -1.0f;
        return Vector2Normalize(toWorld({side * 0.75f, -1}));
    }
    Vector2 hatPutTip() const {
        Vector2 seat = toWorld({0, -(kLeonH / 2 + kHatSeat + 11)}), q = Vector2Scale(hatPutDir(), 62);
        float s = clampf(hatPutT / kHatPutT, 0, 1);
        s = s * s * (3 - 2 * s);
        return Vector2Add(centreW(), Vector2Lerp(Vector2Lerp(hatPut0, q, s), Vector2Lerp(q, seat, s), s));
    }
    // The hat on his head again -- touched, or put there by his tongue.
    void hatBack() {
        hatLoose = hatOnTongue = false; hatPutT = -1;
        hasHat = true; hatT = hatBlinkT = -1;
        lesson("Hat !");
        sound(Audio::kFruit, 1.1f);
        if (trace) fprintf(stderr, "t=%.2f hat taken back\n", time);
    }

    void knockHatOff(Vector2 carrierVelocity) {
        if (!hasHat) return;
        if (bellyFly >= 0) { hatKeep = bellyColour(); hatKeepT = t.bellyTime > 0 ? bellyT : -1; }
        hasHat = false; hatLoose = true;
        hatP = Vector2Add(centreW(), toWorld({0, -kLeonH / 2 - 10}));
        hatV = Vector2Add(toWorld({(float)GetRandomValue(-120, 120), -(float)GetRandomValue(250, 380)}), Vector2Scale(toWorld(carrierVelocity), 0.5f));
        hatW = (float)GetRandomValue(-500, 500);
        hatA = grav * 90.0f;
    }

    // The tongue bounced off ice and then held nothing -- at the end of its reach, or a fly
    // snapped up: it goes dead, limp, hanging out of his mouth (deadStep). It is never swallowed
    // by itself: RT (V) swallows it back a chew at a time, the tongue key reels it all in.
    void goDead() {
        float len;
        rope = evenPoints(tonguePath(centreW()), 24, len);   // (the path, cut into 24 points evenly)
        tgLen = fmaxf(tgExt, 1.0f);
        ropePrev = rope;
        tg = tgDead;
        tgBnc.clear();
        chewLeft = 0;
        sound(Audio::kRelease, 0.7f, 0.6f);
        if (trace) fprintf(stderr, "t=%.2f tongue goes dead (length %.0f)\n", time, tgLen);
    }
    void deadStep(float dt) {
        if (chewPress) {                                  // RT: a chew swallowed, as for a stuck tongue
            chewPress = false;
            chewLeft += kLeonDrawH;
            eyeBig = 1;
            chomp(0.9f);
            spitDrops(centreW(), Vector2Negate(pullW), GetRandomValue(2, 3), 260);
            if (trace) fprintf(stderr, "t=%.2f dead tongue chewed back\n", time);
        }
        if (chewLeft > 0) {
            float k = fminf(chewLeft, kLeonDrawH / t.chewTime * dt);
            chewLeft -= k;
            tgLen -= k;
        } else if (payHeld && tgLen < tongueReach()) {    // LT: paid out
            tgLen = fminf(tongueReach(), tgLen + t.tongueStretch * t.tongueFastStretch * dt);
        }
        if (tgLen <= 12.0f) {                             // all in: swallowed (with whatever it holds)
            tg = tgNone; tgLen = 0; rope.clear(); chewLeft = 0;
            swallow();
            return;
        }
        // A chain from his mouth, its end free: dead weight, falling, dragging on the ground.
        int n = (int)rope.size();
        Vector2 a = centreW();
        Vector2 g = Vector2Scale(pullW, t.gravity * dt * dt);
        for (int i = 1; i < n; i++) {
            Vector2 v = Vector2Scale(Vector2Subtract(rope[i], ropePrev[i]), 0.97f);
            ropePrev[i] = rope[i];
            rope[i] = Vector2Add(Vector2Add(rope[i], v), g);
        }
        float rest = tgLen / (n - 1);
        for (int it = 0; it < 16; it++) {
            rope[0] = a;
            for (int i = 0; i < n - 1; i++) {
                Vector2 d = Vector2Subtract(rope[i + 1], rope[i]);
                float L = Vector2Length(d);
                if (L < 1e-4f) continue;
                Vector2 corr = Vector2Scale(d, (L - rest) / L * (i == 0 ? 1.0f : 0.5f));
                if (i > 0) rope[i] = Vector2Add(rope[i], corr);
                rope[i + 1] = Vector2Subtract(rope[i + 1], corr);   // (the first one alone: the mouth holds)
            }
            for (int i = 1; i < n; i++) pushOut(rope[i], ropePrev[i]);
        }
        rope[0] = a;
        if (trace && fmodf(time, 0.25f) < dt)
            fprintf(stderr, "t=%.2f dead tongue: from (%.0f,%.0f) to (%.0f,%.0f), length %.0f\n", time, rope[0].x, rope[0].y,
                    rope[n - 1].x, rope[n - 1].y, tgLen);
    }
    // A point within `r` of the tongue in flight? Then the tongue ends there -- cut back to it.
    bool tongueTouches(Vector2 p, float r) {
        std::vector<Vector2> pts = tonguePath(centreW());
        float len = 0;
        for (size_t k = 0; k + 1 < pts.size(); k++) {
            Vector2 a = pts[k], b = pts[k + 1];
            float L = Vector2Distance(a, b);
            Vector2 u = L > 0.001f ? Vector2Scale(Vector2Subtract(b, a), 1 / L) : tgDir;
            float along = clampf(Vector2DotProduct(Vector2Subtract(p, a), u), 0, L);
            if (Vector2Distance(p, Vector2Add(a, Vector2Scale(u, along))) < r) {
                tgBnc.resize(k);                         // (the bounces before this stretch stay)
                tgDir = u;
                tgExt = fmaxf(len + along, 1.0f);
                return true;
            }
            len += L;
        }
        return false;
    }
    Vector2 eyeTarget() const { return Vector2Add(centreW(), Vector2Scale(mouthDir(), 60.0f)); }

    void land(float impact) {
        if (impact < 100) return;                        // (put down there -- the start, a checkpoint: no drop, no dust)
        float k = clampf(impact / t.maxFall, 0, 1);
        squash = fmaxf(squash, t.squashMax * k * k + 0.08f);
        squashVel = 0;
        dust({pos.x, pos.y}, 3 + (int)(k * 16), 60 + 260 * k, 40 + 160 * k);
        if (impact > t.hardLand) {
            float hard = clampf((impact - t.hardLand) / fmaxf(1, t.maxFall - t.hardLand), 0, 1);
            trauma = fminf(1.0f, trauma + 0.45f + 0.55f * hard);
            stun = t.stunTime * (0.5f + 0.5f * hard);
            landFlash = 1;
            sound(Audio::kThud, 1.0f - 0.15f * hard, 0.6f + 0.4f * hard);   // from a height: the harder, the deeper
        }
    }

    // ---- one physics step -----------------------------------------------------------------
    // What squeezes his hat (above his head, as far as hatRoom, and a pixel past it) is dark matter.
    bool hatInDarkMatter() const {
        if (!hasHat || hatRoom >= hatSeat() + hatCrown() + 4) return false;
        float head = pos.y - kLeonH;
        Rectangle w = turnBox(grav, pos.x - kLeonW / 2, head - hatRoom - 1, pos.x + kLeonW / 2, head);
        int tx0 = (int)floorf(w.x / kTile), tx1 = (int)floorf((w.x + w.width) / kTile);
        int ty0 = (int)floorf(w.y / kTile), ty1 = (int)floorf((w.y + w.height) / kTile);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) if (level.deadly(tx, ty)) return true;
        return false;
    }
    // Dark matter: Leon bursts into 16 little cubes that bounce and settle; then fade, restart.
    bool touchingDeadly() const {
        Rectangle w = turnBox(grav, pos.x - kLeonW / 2 - 1, pos.y - kLeonH - 1, pos.x + kLeonW / 2 + 1, pos.y + 1);
        int tx0 = (int)floorf(w.x / kTile), tx1 = (int)floorf((w.x + w.width) / kTile);
        int ty0 = (int)floorf(w.y / kTile), ty1 = (int)floorf((w.y + w.height) / kTile);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) if (level.deadly(tx, ty)) return true;
        return false;
    }

    // The tongue as a world polyline from the mouth (for bursting it).
    std::vector<Vector2> tonguePoints() const {
        std::vector<Vector2> pts;
        Vector2 m = centreW();
        if (tg == tgOut || tg == tgBack) pts = tonguePath(m);
        else if (tg == tgDead) pts = rope;
        else if (tg == tgStuck) {
            pts.push_back(m);
            for (int i = (int)rope.size() - 2; i >= 1; i--) pts.push_back(rope[i]);
            for (int i = (int)piv.size() - 1; i >= 0; i--) pts.push_back(piv[i]);
        }
        return pts;
    }

    // Suicide, held: he trembles harder and harder (tremble(): suicide_tremble pixels at the end),
    // eyes going wide, the pupils bouncing about in them faster and faster, and at suicide_time he
    // bursts -- suicide_force times as hard as dark matter does it. Let go before: it dies down.
    // True if he burst.
    bool suicideStep(float dt) {
        if (!suicideHeld) suicideLatch = false;
        if (suicideHeld && !suicideLatch) suicideT += dt;
        else suicideT = fmaxf(0, suicideT - 2 * dt);
        if (suicideT <= 0) { pupil = pupilV = {0, 0}; return false; }
        float u = clampf(suicideT / t.suicideTime, 0, 1);
        eyeBig = fmaxf(eyeBig, fminf(1, 1.5f * u));
        const float R = 1.8f;                            // how far a pupil can go in its eye
        float sp = 20 + 90 * u;
        if (Vector2Length(pupilV) < 0.01f) { float a = GetRandomValue(0, 628) / 100.0f; pupilV = {cosf(a), sinf(a)}; }
        pupilV = Vector2Scale(Vector2Normalize(pupilV), sp);
        pupil = Vector2Add(pupil, Vector2Scale(pupilV, dt));
        float L = Vector2Length(pupil);
        if (L > R) {                                     // off the rim, a little askew
            Vector2 n = Vector2Scale(pupil, 1 / L);
            pupil = Vector2Scale(n, R);
            pupilV = Vector2Rotate(Vector2Subtract(pupilV, Vector2Scale(n, 2 * Vector2DotProduct(pupilV, n))),
                                   GetRandomValue(-40, 40) / 100.0f);
        }
        if (suicideT < t.suicideTime) return false;
        suicideLatch = true;
        die(t.suicideForce, true);
        return true;
    }
    float tremble() const { float u = clampf(suicideT / t.suicideTime, 0, 1); return t.suicideTremble * u * u; }

    // Dark matter -- or, `suicide`, his own doing: the burst `force` times as hard.
    void die(float force = 1, bool suicide = false) {
        fromSuicide = suicide;
        // The tongue, if out, bursts too: pink cubes the size of Leon's pieces, all along it.
        std::vector<Vector2> tpts = tonguePoints();
        float step = kBodyW / 4 + 1;
        std::vector<Piece> tonguePieces;
        for (size_t k = 0; k + 1 < tpts.size(); k++) {
            float L = Vector2Distance(tpts[k], tpts[k + 1]);
            for (float d = (k == 0 ? 24.0f : 0.0f); d < L; d += step) {
                Piece pc;
                pc.p = Vector2Lerp(tpts[k], tpts[k + 1], d / fmaxf(L, 0.001f));
                float a = GetRandomValue(0, 628) / 100.0f, sp = GetRandomValue(30, 110);
                pc.v = Vector2Add(Vector2Scale({cosf(a), sinf(a)}, sp), Vector2Scale(pullW, -(float)GetRandomValue(40, 120)));
                pc.v = Vector2Scale(pc.v, force);
                pc.a = GetRandomValue(0, 90); pc.av = GetRandomValue(-400, 400);
                pc.tongue = true;
                tonguePieces.push_back(pc);
            }
        }
        dead = true; deadT = 0;
        deathCleanup();
        pieces.clear();
        Vector2 c = centreW();
        for (int j = 0; j < 4; j++)
            for (int i = 0; i < 4; i++) {
                Vector2 l = {(i - 1.5f) * kBodyW / 4, (j - 1.5f) * kBodyH / 4 - 7.5f};   // across the cube
                Piece pc;
                pc.p = Vector2Add(c, toWorld(l));
                Vector2 out = Vector2Normalize(Vector2Add(l, {GetRandomValue(-30, 30) / 10.0f, GetRandomValue(-30, 30) / 10.0f}));
                float sp = GetRandomValue(160, 380);
                pc.v = Vector2Add(toWorld(Vector2Scale(out, sp)), Vector2Scale(pullW, -(float)GetRandomValue(150, 320)));
                pc.v = Vector2Scale(pc.v, sqrtf(t.piecesSpread) * force);   // reach grows with the square of the speed
                pc.v = Vector2Add(pc.v, Vector2Scale(toWorld(vel), 0.5f));
                pc.a = 0; pc.av = GetRandomValue(-900, 900);
                pieces.push_back(pc);
            }
        trauma = fminf(1, trauma + 0.6f);
        sparks(c, false);
        sound(Audio::kDeath);
        for (auto& tp : tonguePieces) pieces.push_back(tp);
        suicideT = 0; pupil = pupilV = {0, 0};
        if (trace) fprintf(stderr, "t=%.2f %s (%d tongue pieces)\n", time,
                           suicide ? "Leon bursts: suicide" : timeUp ? "Leon explodes: time is up" : "Leon touched dark matter", (int)tonguePieces.size());
    }
    // Whatever the death: the tongue gone, the checkpoint caught on it too, the fly he was chewing let go.
    // (Eaten, monstermunch_frees_flies: the colour fly and a lamp's fly in his belly stay in there --
    // freeBellyFlies, after the last munch.)
    void deathCleanup(bool eaten = false) {
        const bool keep = eaten && t.mmFreeFlies;
        caughtPine = -1; cpMsg = 0;
        if (hatOnTongue) { hatOnTongue = false; hatPutT = -1; hatV = {0, 0}; hatW = 0; }   // (the hat on it: dropped there)
        tg = tgNone; tgCrate = -1; pulling = false; piv.clear(); pivSide.clear(); rope.clear();
        knock = kNone; getUp = 0; flyChew = -1;
        if (mouthFly >= 0 && !(keep && flies[mouthFly].lamp >= 0)) {   // the fly he was chewing gets away -- golden all the same
            Fly& f = flies[mouthFly];
            f.inMouth = false; f.golden = true; f.p = centreW(); f.v = {0, 0};
            mouthFly = -1;
        }
        if (bellyFly >= 0 && !keep) {       // the colour fly in his belly: back round its pole
            Fly& f = flies[bellyFly];
            f.inMouth = false; f.p = flyHome(f.pole); f.v = {0, 0};
            bellyFly = -1;
        }
        // The lamps' flies -- on the tongue, round him, settled at another lamp -- all go back to
        // the post each started on, whatever the death. (A colour fly on the tongue: let go.)
        for (size_t i = 0; i < flies.size(); i++) {
            Fly& f = flies[i];
            if ((int)i == bellyFly || (int)i == mouthFly) continue;   // (kept in his belly, eaten)
            f.freed = false;                             // (one out of a Monstermunch: as any other)
            if (f.pole >= 0) {                           // a colour fly: let go of the tongue, stop circling, back to its pole
                if (f.caught && (int)i == caughtFly) caughtFly = -1;
                f.caught = false; f.followT = -1; f.p = flyHome(f.pole); f.v = {0, 0};
            }
            if (f.home < 0) continue;
            if (f.caught) { f.caught = false; f.golden = true; f.v = {0, 0}; if ((int)i == caughtFly) caughtFly = -1; }
            f.lamp = f.home; f.lampT = -1;
        }
    }

    void deathStep(float realDt) {
        // Dead again before the fade in of his coming back is over: it plays out first -- his pieces
        // flying meanwhile -- then this death's own fade out, and the rest.
        if (fadeIn > 0) fadeIn = fmaxf(0, fadeIn - realDt);
        else deadT += realDt;
        piecesStep(realDt * t.piecesSpeed);   // the pieces fly in slow motion (their gravity too)
        if (deadT < t.deathPiecesWait + t.deathPiecesFade) return;
        if (timeUp) { timeUpOver = true; return; }       // (main: back to the title)
        bool own = fromSuicide;
        if (own) restartMode();
        respawn();
        if (goldenRun && !goldenDone) { fromSuicide = false; goldenAgain(); return; }
        if (own) { wantCountdown = true; fromSuicide = false; }
    }
    // A death in a golden run (any: dark matter, eaten, a suicide): the run from scratch -- its way in
    // all over again, from the black (the screen black already), the world put back, its clock from nothing.
    void goldenAgain() {
        goneT = t.darkLightFade;                         // (gone: not drawn, nothing of his moving)
        goldenSeq = t.goldenFadeOut - 1e-4f;             // (the black: the world put back at the next step)
        fadeIn = 0;                                      // (the golden run's black takes over from the death's)
        if (trace) fprintf(stderr, "t=%.2f golden run: dead -- its way in again\n", time);
    }
    // Suicide: back at the first checkpoint of the latest mode still running -- its key (easy mode:
    // the start) -- its clock back to nothing, its pineapples lit since grey again (restorePowers);
    // main then counts 3, 2, 1, GO. None running: the last
    // checkpoint, as any death. (A golden run: its start, its clock from nothing, his powers as then.)
    void restartMode() {
        if (goldenRun && !goldenDone) {
            goldenTime = 0;
            restorePowers(5);
            if (trace) fprintf(stderr, "t=%.2f suicide: back to the start of the golden run, its clock from nothing\n", time);
            return;
        }
        int mode = -1;
        for (int k = 0; k < 5; k++) if (clockRuns(k)) mode = k;
        int pine = mode == 1 ? level.exitKeyPine : mode == 2 && !level.greenKeyPine.empty() ? level.greenKeyPine[0]
                 : mode == 3 && !level.blueKeyPine.empty() ? level.blueKeyPine[0]
                 : mode == 4 && !level.purpleKeyPine.empty() ? level.purpleKeyPine[0] : -1;
        if (mode == 0) { cpPine = -1; cpFeet = level.spawn; cpGrav = 0; runTime = 0; }
        else if (pine >= 0) {
            checkpoint(pine);
            if (mode == 1) { hardTime = 0; hardNext = 0; } else if (mode == 2) darkTime = 0; else if (mode == 3) harderTime = 0; else camTime = 0;
        }
        if (trace) fprintf(stderr, "t=%.2f suicide: back to the start of mode %d, its clock from nothing\n", time, mode);
        if (mode >= 0) restorePowers(mode);
    }
    // The ending (endT): the wall touched -- his box and a pixel round it --, then the landing, then
    // the clock of it all.
    void endingStep(float dt) {
        if (level.endX0 < 0) return;
        if (endT < 0) {
            Rectangle b = turnBox(grav, pos.x - kLeonW / 2 - 1, pos.y - kLeonH - 1, pos.x + kLeonW / 2 + 1, pos.y + 1);
            Rectangle w = {(level.endX0 + 1) * kTile, (float)(level.endY - level.endWallH) * kTile, kTile, level.endWallH * kTile};
            if (!CheckCollisionRecs(b, w)) return;
            endT = 0; vel.x = 0;
            facing = centreW().x < level.annaFeet().x ? 1.0f : -1.0f;   // (towards her)
            if (trace) fprintf(stderr, "t=%.2f the ending: the wall touched -- no more control\n", time);
            return;
        }
        endT += dt;
        // His tongue not left out: whatever it does -- stuck, flying, limp -- it is swallowed, reeled in
        // as B does it (and not thrown again). In, and he on the tower's top: he faces her, standing as
        // ever (the open mouth gone with the tongue, his lean springing back).
        if (tg == tgStuck || tg == tgOut || tg == tgDead) {
            tongueRetract(t.tongueBack);
            if (trace) fprintf(stderr, "t=%.2f the ending: his tongue swallowed\n", time);
        }
        tgResend = false;
        if (tg == tgNone && onGround) facing = centreW().x < level.annaFeet().x ? 1.0f : -1.0f;   // (towards her)
        if (endLandT < 0) {
            if (!onGround) return;
            endLandT = 0;
            if (trace) fprintf(stderr, "t=%.2f the ending: landed on the tower's top\n", time);
            return;
        }
        float was = endLandT;
        endLandT += dt;
        auto mark = [&](float at, const char* what) { if (trace && was < at && endLandT >= at) fprintf(stderr, "t=%.2f the ending: %s\n", time, what); };
        mark(t.endSmileWait, "the smiles");
        mark(endHeartsAt(), "the hearts");
        mark(endHappyAt(), "happy: eyes ^^, stepping on the spot");
        mark(endPanAt(), "the scene slides left");
        mark(endCreditsAt(), "the credits");
        // The credits rolling, down held: easing to ending_credits_fast times as fast -- and back -- over
        // ending_credits_fast_ramp, the seconds of rolling it gains them bringing all after them sooner.
        if (float ct = endCreditsT(); ct >= 0 && ct < endCreditsLen()) {
            float want = creditsFast ? t.endCreditsFast : 1, rate = fabsf(t.endCreditsFast - 1) / fmaxf(t.endCreditsFastRamp, 1e-3f);
            creditsK += clampf(want - creditsK, -rate * dt, rate * dt);
            creditsGain += (creditsK - 1) * dt;
        } else creditsK = 1;
        mark(endCreditsAt() + endCreditsLen() - creditsGain, "the credits gone");
        mark(endScrollAt(), "the scene scrolls away");
        // Scrolled away, the tower, Anna and the hearts all past the screen's left edge: the fade to black.
        if (endOffT < 0 && endScroll() > 0 && toWorld(cam).x - viewSize().x / 2 > endSceneRight()) {
            endOffT = endLandT;
            if (trace) fprintf(stderr, "t=%.2f the ending: the scene off the screen -- the fade to black\n", time);
        }
        heartsStep(dt);
    }
    // The ending's scene, its right edge (world x): the tower's top; Anna, her brim flat as a party horn
    // unrolled (29 pixels, and some 51 of curl, times anna_brim_size) and swaying; any heart within the
    // screen's height (above it, a heart only rises further).
    float endSceneRight() const {
        Vector2 F = level.annaFeet(), camW = toWorld(cam), v = viewSize();
        float r = fmaxf((level.endX1 + 1) * kTile, F.x + 80 * t.annaBrim * kAnnaPlayS + 12);
        for (const Heart& h : hearts)
            if (fabsf(h.p.y - camW.y) < v.y / 2 + t.endHeartSize) r = fmaxf(r, h.p.x + t.endHeartSize / 2);
        return r;
    }
    // The hearts: born between the two, each going its own way -- straight on, its heading wandering
    // (ending_heart_turn), at ending_heart_speed -- and drifting up (ending_heart_rise): an active
    // Brownian particle. From their middle, ending_heart_fade_from times the distance between them,
    // it fades out; at ending_heart_fade_to times, it is gone (drawHearts).
    void heartsStep(float dt) {
        if (endLandT < endHeartsAt()) return;
        Vector2 M;
        float D;
        heartFrame(M, D);
        Vector2 L = centreW(), A = annaC();
        auto uni = [] { return GetRandomValue(0, 1000000) / 1000000.0f; };
        auto gauss = [&] { float u = fmaxf(1e-6f, uni()), v = uni(); return sqrtf(-2 * logf(u)) * cosf(2 * PI * v); };
        heartDue += dt * t.endHeartRate;
        for (; heartDue >= 1; heartDue -= 1) {
            float x0 = fminf(L.x, A.x) + kBodyW / 2, x1 = fmaxf(L.x, A.x) - kBodyW / 2;   // (between their bodies, by their faces and over)
            if (x1 < x0) x0 = x1 = M.x;
            hearts.push_back({{lerpf(x0, x1, uni()), M.y + lerpf(-0.6f, 0.2f, uni()) * kTile}, uni() * 2 * PI, 0});
        }
        for (Heart& h : hearts) {
            h.a += sqrtf(2 * t.endHeartTurn * dt) * gauss();
            h.p.x += t.endHeartSpeed * cosf(h.a) * dt;
            h.p.y += (t.endHeartSpeed * sinf(h.a) - t.endHeartRise) * dt;
            h.age += dt;
        }
        hearts.erase(std::remove_if(hearts.begin(), hearts.end(), [&](const Heart& h) { return Vector2Distance(h.p, M) >= t.endHeartFadeTo * D; }),
                     hearts.end());
    }

    // His hat lost, lying where it fell: still lost as he comes near a spare-hat spot ('h'), it waits
    // there -- put there as the spot is about to show on the screen, unless where it lies might show
    // (in sight, it is his to fetch).
    void spareHatStep() {
        if (!hatLoose || hatOnTongue) return;
        Vector2 camW = toWorld(cam), vs = viewSize();
        float seeR = sqrtf(vs.x * vs.x + vs.y * vs.y) / 2 + kTile;
        if (Vector2Distance(hatP, camW) < seeR) return;
        for (Vector2 s : level.spareHats) {
            if (Vector2Distance(s, camW) >= seeR) continue;
            hatP = s; hatV = {0, 0}; hatW = 0; hatA = grav * 90.0f;
            if (trace) fprintf(stderr, "t=%.2f his lost hat waits at the spare-hat spot (%.0f,%.0f)\n", time, s.x, s.y);
            return;
        }
    }
    // The hat upright for his gravity (degrees): the nearest such turn to its own.
    float hatUpright() const { return grav * 90.0f + 360.0f * roundf((hatA - grav * 90.0f) / 360.0f); }
    // The loose hat: falling with his gravity, bouncing off whatever it meets, rolling on the ground
    // -- its spin following its speed along it -- slowing, and at rest turning upright, crown up.
    // On his tongue: at its tip, righting itself.
    void looseHatStep(float dt) {
        if (!hatLoose || !inZone(hatP)) return;
        if (hatOnTongue) {
            hatP = hatPutT >= 0 ? hatPutTip() : tongueTip();
            hatV = {0, 0}; hatW = 0;
            hatA += (hatUpright() - hatA) * fminf(1, (hatPutT >= 0 ? 14 : 5) * dt);
            return;
        }
        const float r = kLooseHatR;
        auto hit = [&](Vector2 p) { return obstacleAt(p) != 0; };
        hatV = Vector2Add(hatV, Vector2Scale(pullW, t.gravity * dt));
        Vector2 np = Vector2Add(hatP, Vector2Scale(hatV, dt));
        if (hit({np.x + signf(hatV.x) * r, hatP.y})) { hatV.x *= -0.45f; hatW *= -0.6f; np.x = hatP.x; }
        if (hit({np.x, np.y + signf(hatV.y) * r})) { hatV.y *= -0.45f; np.y = hatP.y; }
        hatP = np;
        if (hit(Vector2Add(hatP, Vector2Scale(pullW, r + 1.5f)))) {   // on the ground: rolling
            Vector2 along = {pullW.y, -pullW.x};
            float v = Vector2DotProduct(hatV, along);
            hatW = v / r * RAD2DEG;
            hatV = Vector2Subtract(hatV, Vector2Scale(along, v * fminf(1, 1.5f * dt)));
            if (fabsf(v) < 10 && fabsf(Vector2DotProduct(hatV, pullW)) < 40) {
                hatV = {0, 0}; hatW = 0;
                // At rest, settled right onto the ground -- the gap its last little bounce left closed,
                // its brim a hair into the ground's ink: no hairline of paper between them.
                for (int k = 0; k < 40 && !hit(Vector2Add(hatP, Vector2Scale(pullW, r - 0.4f))); k++)
                    hatP = Vector2Add(hatP, Vector2Scale(pullW, 0.1f));
                hatA += (hatUpright() - hatA) * fminf(1, 8 * dt);
                return;
            }
        }
        hatA += hatW * dt;
    }
    // A Monstermunch munching -- the lost hat lying on its face: it hops (`up`: how hard, pixels a
    // second), and off to the side of the middle it lies on, the farther out the harder.
    void hatJolt(Monster& m, float up) {
        if (!hatLoose || hatOnTongue || !inZone(hatP)) return;
        if (!CheckCollisionPointRec(Vector2Add(hatP, Vector2Scale(pullW, kLooseHatR + 2)), m.r)) return;
        Vector2 across = {-pullW.y, pullW.x};
        float k = clampf(Vector2DotProduct(Vector2Subtract(hatP, m.centre()), across) / (m.r.width / 2), -1, 1);
        hatV = Vector2Add(Vector2Scale(pullW, -up * m.rnd(0.8f, 1.2f)), Vector2Scale(across, up * (0.8f * k + m.rnd(-0.15f, 0.15f))));
        hatW = m.rnd(-2, 2) * up;
        if (trace) fprintf(stderr, "t=%.2f the hat hops off monstermunch %d's face (%.0f,%.0f)\n", time, (int)(&m - monsters.data()), hatV.x, hatV.y);
    }
    // His pieces (and blood cubes): falling with his gravity, bouncing, coming to rest -- on their
    // sides, not their middles: each as thick as it is drawn.
    // (A munch's cubes -- those with a size -- go through the Monstermunchs' tiles: they pop out of
    // one's middle, and fall back into its place -- seen only through its mouth.)
    void piecesStep(float dt) {
        for (auto& pc : pieces) {
            bool munch = pc.size > 0;
            float r = (munch ? pc.size : kBodyW / 4) / 2;     // (half its side, as drawn)
            auto in = [&](Vector2 p) { char c = obstacleAt(p); return c != 0 && !(munch && c == 'm'); };
            // Its side facing d (a unit vector), centred at c, in something: its middle or either end.
            auto hit = [&](Vector2 c, Vector2 d) {
                Vector2 e = Vector2Add(c, Vector2Scale(d, r)), s = {-d.y * 0.8f * r, d.x * 0.8f * r};
                return in(e) || in(Vector2Add(e, s)) || in(Vector2Subtract(e, s));
            };
            pc.v = Vector2Add(pc.v, Vector2Scale(pullW, t.gravity * dt));
            Vector2 np = Vector2Add(pc.p, Vector2Scale(pc.v, dt));
            if (pc.v.x != 0 && hit({np.x, pc.p.y}, {signf(pc.v.x), 0})) { pc.v.x *= -0.45f; pc.v.y *= 0.8f; pc.av *= 0.6f; np.x = pc.p.x; }
            if (pc.v.y != 0 && hit(np, {0, signf(pc.v.y)})) { pc.v.y *= -0.45f; pc.v.x *= 0.8f; pc.av *= 0.6f; np.y = pc.p.y; }
            if (in(np)) np = pc.p;
            pc.p = np;
            pc.a += pc.av * dt;
            if (Vector2Length(pc.v) < 25 && hit(Vector2Add(pc.p, Vector2Scale(pullW, 1.5f)), pullW)) { pc.v = {0, 0}; pc.av = 0; }
        }
    }

    // ---- the Monstermunchs ------------------------------------------------------------------
    // Asleep, its eyes shut; awake while Leon is within monstermunch_wake_distance (of the
    // screen's width), its eyes on him; within monstermunch_reach, no more than
    // monstermunch_throw_angle off straight out of its face from its middle -- whatever lies between -- it throws its
    // tongue at him -- straight, at monstermunch_tongue_speed. On him: caught (true). Missed --
    // something in the way, its reach run out, or him another's already -- the tongue goes numb and
    // must be swallowed back before it throws again (numbStep). (`seen`: he is there to be seen --
    // not dead; `catchable`: and to be caught -- no Monstermunch has him yet. The one that has him is
    // eatenStep's; the others go on trying.)
    bool monstersStep(float dt, bool seen, bool catchable) {
        Vector2 lc = centreW();
        float vw = viewSize().x;
        for (size_t i = 0; i < monsters.size(); i++) {
            if ((int)i == eatenBy) continue;
            Monster& m = monsters[i];
            if (!inZone(m.centre())) {                       // far from him: put to sleep, tongue in, left so
                if (!m.calmed) { m.calm(); m.calmed = true; }
                continue;
            }
            m.calmed = false;
            m.t += dt;
            spasmStep(m, dt);
            m.rest = fmaxf(0, m.rest - dt);
            // Leon standing on its face (his feet's middle on it): it has him at once -- asleep, resting,
            // whatever -- unless it is swallowing its numb tongue: then after monstermunch_stand_time of
            // it, the tongue swallowed on as it eats him.
            if (catchable && !god && onGround && CheckCollisionPointRec(Vector2Add(lc, Vector2Scale(pullW, kLeonH / 2 + 2)), m.r)) {
                if ((m.standT += dt) >= t.mmStandT || m.st != Monster::kNumb) {
                    m.standT = 0;
                    if (trace) fprintf(stderr, "t=%.2f Leon standing on monstermunch %d: eaten %s\n", time, (int)i,
                                       m.st == Monster::kNumb ? "(its tongue numb: after its stand time)" : "at once");
                    caught(i);
                    return true;
                }
            } else {
                m.standT = 0;
            }
            Vector2 mc = m.centre(), d = Vector2Subtract(lc, mc);
            float dist = Vector2Length(d);
            bool near = seen && !hidden && dist < t.mmWake * vw;   // (hidden in a cloud of his colour: not seen at all)
            Vector2 want = near && dist > 1 ? Vector2Scale(d, 1 / dist) : Vector2{0, 0};
            m.look = Vector2Lerp(m.look, want, 1 - expf(-dt * 12));
            switch (m.st) {
                case Monster::kSleep:
                case Monster::kWatch: {
                    // Awake, Leon near (monstermunch_drool_distance) but not to be had -- out of its
                    // throw, or in its rest: its mouth a little open, straight out,
                    // drooling (monstermunch_drool_open, monstermunch_drool_rate). Else shut.
                    bool drools = m.st == Monster::kWatch && dist < t.mmDroolDist * vw;
                    float to = drools ? t.mmDroolOpen : 0;
                    m.open = m.open > to ? fmaxf(to, m.open - dt / 0.12f) : fminf(to, m.open + dt / 0.3f);
                    if (drools) {
                        m.u = Vector2Normalize(Vector2Lerp(m.u, m.out(), 1 - expf(-dt * 6)));
                        if (m.rnd(0, 1) < t.mmDroolRate * dt) drool(m, 110, 190, 3);   // (out a little, back in)
                    }
                    if (m.st == Monster::kSleep) {
                        if (near) {
                            m.st = Monster::kWatch; m.t = 0;
                            if (trace) fprintf(stderr, "t=%.2f monstermunch %d wakes\n", time, (int)i);
                        }
                    } else if (!near) {
                        m.st = Monster::kSleep; m.t = 0;
                        if (trace) fprintf(stderr, "t=%.2f monstermunch %d falls asleep\n", time, (int)i);
                    } else if (m.rest <= 0 && dist < t.mmReach * vw && Vector2DotProduct(d, m.out()) >= dist * cosf(t.mmAngle * DEG2RAD)) {
                        // (Whatever lies in the way: it never cares -- its tongue meets it, and goes numb.)
                        m.st = Monster::kThrow; m.t = 0; m.u = Vector2Scale(d, 1 / dist); m.ext = 0; m.bnc.clear(); m.tipU = m.u;
                        spray(m, m.toWorld(mouthV(m).f), m.u, 4, 0.5f, 150, 320);   // (a little saliva, thrown with it)
                        sound(Audio::kTongueOut, 0.6f);
                        if (trace) fprintf(stderr, "t=%.2f monstermunch %d throws its tongue\n", time, (int)i);
                    }
                    break;
                }
                case Monster::kThrow: {
                    m.open = fminf(1, m.open + dt / 0.06f);
                    float from = m.ext;
                    m.ext += t.mmTongueSpeed * dt;
                    // Off ice it bounces, as a ball off a wall -- as Leon's does (up to 8 times) -- and flies on;
                    // bounced, it catches nothing: it goes numb where it meets anything else, or him, or at the
                    // end of its reach.
                    for (float e = from, last = from;; last = e, e = fminf(m.ext, e + 6)) {   // its way this step, in short strides
                        Vector2 o = m.centre();
                        float base = 0;
                        Vector2 tip = m.tongueAt(e, &o, &base), dir = m.bnc.empty() ? m.u : m.tipU;
                        auto splash = [&] { spray(m, Vector2Subtract(tip, Vector2Scale(dir, 6)), Vector2Negate(dir), 10, 1.3f, 80, 260); };
                        // Something in its way first: him only if it got that far -- else, within reach
                        // of him (8 px) from inside a thin platform, it caught him through it.
                        bool out = !m.bnc.empty() ||                     // (past its face -- or bounced: then all of it counts, its own face too)
                                   Vector2DotProduct(Vector2Subtract(tip, m.toWorld({0, -m.r.width / 2})), m.out()) > 0;
                        char c = out ? obstacleAt(tip) : 0;
                        Rectangle tb;
                        if (tongueBounces(c) && m.bnc.size() < 8 && tileBox((int)floorf(tip.x / kTile), (int)floorf(tip.y / kTile), tb)) {
                            Vector2 n, at = surfacePoint(o, e - base, dir, e - last, &n), nd = dir;
                            if (n.x != 0) nd.x = -nd.x;   // off a side...
                            if (n.y != 0) nd.y = -nd.y;   // ...or the top or bottom
                            m.bnc.push_back(at); m.tipU = nd;
                            e = base + Vector2Distance(o, at);              // (on from there, the new way --
                            m.ext = fmaxf(m.ext, e);                         // never short of that bounce)
                            sound(Audio::kStick, 1.6f, 0.4f);
                            if (trace) fprintf(stderr, "t=%.2f monstermunch %d's tongue bounces off ice at (%.0f,%.0f)\n", time, (int)i, at.x, at.y);
                            continue;
                        }
                        if (c) { splash(); m.ext = e; goNumb(m, (int)i); break; }
                        if (seen && CheckCollisionCircleRec(tip, 8, leonWorldBox())) {
                            if (god) {                                    // GOD: off him as off ice -- coming at him; going away, on past him
                                Rectangle lb = leonWorldBox();
                                Vector2 d = Vector2Subtract(tip, {lb.x + lb.width / 2, lb.y + lb.height / 2}), nd = dir;
                                if (Vector2DotProduct(dir, d) < 0 && m.bnc.size() < 8) {
                                    if (fabsf(d.x) * lb.height > fabsf(d.y) * lb.width) nd.x = -nd.x; else nd.y = -nd.y;
                                    m.bnc.push_back(tip); m.tipU = nd;
                                    e = base + Vector2Distance(o, tip);
                                    m.ext = fmaxf(m.ext, e);
                                    sound(Audio::kStick, 1.6f, 0.4f);
                                    if (trace) fprintf(stderr, "t=%.2f monstermunch %d's tongue bounces off GOD Leon\n", time, (int)i);
                                    continue;
                                }
                                if (e >= m.ext) break;
                                continue;
                            }
                            splash();                                     // (a burst of saliva off it, on impact)
                            if (catchable && m.bnc.empty()) { caught(i); return true; }
                            m.ext = e; goNumb(m, (int)i); break;          // (another has him -- or it bounced)
                        }
                        if (e >= m.ext) break;
                    }
                    if (m.st == Monster::kThrow && m.ext >= t.mmReach * vw + kLeonH) goNumb(m, (int)i);
                    break;
                }
                case Monster::kNumb:
                    numbStep(m, dt, near);
                    break;
                default: break;
            }
        }
        return false;
    }
    // A miss: its tongue goes numb -- a limp chain from its lips, where u leaves its face, along its way
    // (round the bounces, if any) to the tip.
    void goNumb(Monster& m, int i) {
        std::vector<Vector2> path{m.toWorld(mouthV(m).f)};
        path.insert(path.end(), m.bnc.begin(), m.bnc.end());
        path.push_back(m.tongueAt(m.ext));
        m.rope = m.ropePrev = evenPoints(path, 24, m.ropeLen);
        m.bnc.clear();
        m.ext = 0;
        m.st = Monster::kNumb; m.t = 0; m.numbT = 0;
        sound(Audio::kRelease, 0.5f, 0.6f);
        if (trace) fprintf(stderr, "t=%.2f monstermunch %d misses: its tongue goes numb (%.0f px)\n", time, i, m.ropeLen);
    }
    // The numb tongue: dead weight hanging out of its lips -- falling with Leon's gravity, lying on the
    // ground (its own face too) -- swallowed back in chews, as Leon does his (RT): about
    // monstermunch_numb_chews_per_second, each wait give or take monstermunch_numb_chew_jitter, each
    // chew its mouth snapping wide, drooling, and taking in its share -- monstermunch_numb_swallow_speed
    // on average -- in monstermunch_numb_chew_time; the mouth held half open between. All in: it
    // shuts its mouth, rests (monstermunch_rest_time), and may throw again.
    void numbStep(Monster& m, float dt, bool near) {
        m.open += (0.7f - m.open) * (1 - expf(-dt * 10));
        if (!numbTongueStep(m, dt, true)) return;
        m.st = near ? Monster::kWatch : Monster::kSleep; m.t = 0; m.rest = t.mmRest;
    }
    // ...the tongue itself: its chews (`mouth`: each snapping the mouth wide -- not while it munches
    // him: it goes on swallowing its tongue all the same), what they take in, the chain lying out of
    // its lips. True once all of it is in.
    bool numbTongueStep(Monster& m, float dt, bool mouth) {
        m.numbT += dt;
        float share = t.mmSwallow / t.mmChewRate;
        if (m.numbT <= dt) m.chewIn = 1 / t.mmChewRate + m.rnd(-t.mmChewJitter, t.mmChewJitter);   // (just numb: the first one)
        if ((m.chewIn -= dt) <= 0) {
            m.chewLeft += share;
            m.chewIn = fmaxf(t.mmNumbChewT, 1 / t.mmChewRate + m.rnd(-t.mmChewJitter, t.mmChewJitter));
            if (mouth) m.open = 1;
            for (int j = 0; j < 2; j++) drool(m, 40, 160);
            hatJolt(m, 150);
            sound(Audio::kSwallow, 0.7f, 0.5f);
            if (trace) fprintf(stderr, "t=%.2f monstermunch %d chews its numb tongue (%.0f px left)\n", time, (int)(&m - monsters.data()), m.ropeLen);
        }
        if (m.chewLeft > 0) {
            float q = fminf(m.chewLeft, share / t.mmNumbChewT * dt);
            m.chewLeft -= q;
            m.ropeLen -= q;
        }
        if (m.ropeLen <= 4) {
            m.rope.clear(); m.ropePrev.clear(); m.ropeLen = 0; m.chewLeft = 0;
            sound(Audio::kSwallow, 0.6f, 0.7f);
            if (trace) fprintf(stderr, "t=%.2f monstermunch %d has swallowed its numb tongue\n", time, (int)(&m - monsters.data()));
            return true;
        }
        float k = m.r.width / 80.0f;                          // (its width as drawn: 18 at its lips, 12 at its tip)
        chainStep(m.rope, m.ropePrev, m.ropeLen, m.toWorld(mouthV(m).f), nullptr, dt, 9 * k, 6 * k);
        return false;
    }
    // A limp chain (world): [0] held at `root` -- its far end too, at `end`, if given -- the rest
    // dead weight, falling with his gravity, lying on the ground with all its thickness above it:
    // each point kept clear of the ground under it by the chain's half width there (hw0 at its root,
    // hw1 at its end).
    void chainStep(std::vector<Vector2>& c, std::vector<Vector2>& prev, float len, Vector2 root, const Vector2* end,
                   float dt, float hw0, float hw1) {
        int n = (int)c.size();
        int last = end ? n - 2 : n - 1;                       // (the last free point)
        Vector2 g = Vector2Scale(pullW, t.gravity * dt * dt);
        for (int i = 1; i <= last; i++) {
            Vector2 v = Vector2Scale(Vector2Subtract(c[i], prev[i]), 0.97f);
            prev[i] = c[i];
            c[i] = Vector2Add(Vector2Add(c[i], v), g);
        }
        float rest = len / (n - 1);
        auto clear = [&](int i) {
            Vector2 off = Vector2Scale(pullW, lerpf(hw0, hw1, (float)i / (n - 1)));
            Vector2 q = Vector2Add(c[i], off);
            if (pushOut(q, Vector2Add(prev[i], off))) c[i] = Vector2Subtract(q, off);
            pushOut(c[i], prev[i]);
        };
        for (int it = 0; it < 16; it++) {
            c[0] = root;
            if (end) c[n - 1] = *end;
            for (int i = 0; i < n - 1; i++) {
                Vector2 d = Vector2Subtract(c[i + 1], c[i]);
                float L = Vector2Length(d);
                bool a = i > 0, b = i + 1 <= last;                // (which of its two ends may move: not a held one)
                if (L < 1e-4f || (!a && !b)) continue;
                Vector2 corr = Vector2Scale(d, (L - rest) / L * (a && b ? 0.5f : 1.0f));
                if (a) c[i] = Vector2Add(c[i], corr);
                if (b) c[i + 1] = Vector2Subtract(c[i + 1], corr);
            }
            for (int i = 1; i <= last; i++) clear(i);
        }
        c[0] = root;
        if (end) c[n - 1] = *end;
    }
    // Its eyes' twitches (left, right): each a Poisson process, monstermunch_*_spasm_frequency a second
    // on average -- the waits between them drawn from the exponential law. And its blinks:
    // monstermunch_blink_frequency a second on average, each wait give or take
    // monstermunch_blink_noise of the average (0: like a clock), each monstermunch_blink_time long.
    void spasmStep(Monster& m, float dt) {
        if (m.blinkT >= 0 && (m.blinkT += dt) > t.mmBlinkT) m.blinkT = -1;
        if (t.mmBlinkF <= 0) m.blinkIn = -1;
        else {
            auto wait = [&]() { return (1 + t.mmBlinkNoise * m.rnd(-1, 1)) / t.mmBlinkF; };
            if (m.blinkIn < 0) m.blinkIn = wait();
            if ((m.blinkIn -= dt) <= 0) { m.blinkT = 0; m.blinkIn = fmaxf(t.mmBlinkT, wait()); }
        }
        for (int j = 0; j < 2; j++) {
            if (m.spasmT[j] >= 0 && (m.spasmT[j] += dt) > kSpasmTime) m.spasmT[j] = -1;
            float f = t.mmSpasmF[j];
            if (f <= 0) { m.spasmIn[j] = -1; continue; }
            if (m.spasmIn[j] < 0) m.spasmIn[j] = -logf(fmaxf(1e-6f, m.rnd(0, 1))) / f;
            if ((m.spasmIn[j] -= dt) <= 0) { m.spasmT[j] = 0; m.spasmIn[j] = -logf(fmaxf(1e-6f, m.rnd(0, 1))) / f; }
        }
    }
    // Its tongue on him: nothing he can do now. It sticks to his middle.
    void caught(size_t i) {
        Monster& m = monsters[i];
        eatenBy = (int)i;
        m.st = Monster::kPull; m.t = 0;
        Vector2 d = Vector2Subtract(centreW(), m.centre());
        float L = Vector2Length(d);
        if (L > 1) { m.u = Vector2Scale(d, 1 / L); m.ext = L; }
        // His tongue: in flight, gone at once; numb or stuck, it goes down with him (eatenTongueStep)
        // -- the way it lies, from his mouth.
        std::vector<Vector2> path;
        eatHolds = 0; eatChewLeft = 0;
        if (tg == tgDead && rope.size() >= 2) {
            path = rope;
            path[0] = centreW();
        } else if (tg == tgStuck && !piv.empty()) {
            path = piv;                                       // (from where it holds, round its bends, to him)
            for (size_t k = 1; k + 1 < rope.size(); k++) path.push_back(rope[k]);
            path.push_back(centreW());
            std::reverse(path.begin(), path.end());
            eatAnchor = path.back();
            eatHolds = std::max(0, std::min(kTongueHolds, (int)t.mmMunches - 1));   // (a munch left to swallow it)
        }
        eatRope.clear(); eatRopePrev.clear(); eatRopeLen = 0;
        if (path.size() >= 2) {
            eatRope = eatRopePrev = evenPoints(path, 24, eatRopeLen);
            if (trace) fprintf(stderr, "t=%.2f his tongue, %s, goes down with him (%.0f px)\n", time, eatHolds > 0 ? "stuck" : "numb", eatRopeLen);
        }
        knockHatOff(vel);                                     // keep the fly's colour before the catch releases it
        deathCleanup(true);
        suicideT = 0; pupil = pupilV = {0, 0};
        vel = {0, 0}; spin = 0; squash = squashVel = 0;
        sound(Audio::kStick, 0.7f);
        if (trace) fprintf(stderr, "t=%.2f monstermunch %d catches Leon\n", time, (int)i);
    }
    // Caught: dragged into its mouth along the tongue (monstermunch_pull_speed), wriggling, eyes
    // popping; swallowed; munched monstermunch_munches times in monstermunch_munch_time -- blood
    // cubes and saliva out of its mouth at each; dead, chewed on a little, drooling, for
    // monstermunch_chew_time; the screen fades out (monstermunch_fade_out); back at the checkpoint.
    void eatenStep(float dt) {
        Monster& m = monsters[eatenBy];
        prevCam = cam;
        time += dt;
        fadeIn = fmaxf(0, fadeIn - dt);                      // (caught just back: the fade in plays on)
        m.t += dt;
        spasmStep(m, dt);
        monstersStep(dt, true, false);                       // (the others go on trying for him)
        MouthV v = mouthV(m);
        if (m.st != Monster::kPull) m.u = Vector2Normalize(Vector2Lerp(m.u, m.out(), 1 - expf(-dt * 6)));   // its mouth straightens
        if (m.st == Monster::kPull) {
            m.open = fminf(1, m.open + dt / 0.06f);
            m.ext = fmaxf(0, m.ext - t.mmPullSpeed * dt);
            tilt = 14 * sinf(time * 28);
            eyeBig = 1;
            Vector2 c = Vector2Add(m.centre(), Vector2Scale(m.u, m.ext));
            pos = Vector2Add(toLocal(c), {0, kLeonH / 2});
            if (m.ext <= Vector2Length(v.f) + 4) {           // at its lips: swallowed
                m.st = Monster::kMunch; m.t = 0; m.munched = 0; m.ext = 0; m.open = 0;
                sound(Audio::kSwallow, 0.55f);
                if (trace) fprintf(stderr, "t=%.2f monstermunch %d swallows Leon\n", time, eatenBy);
            }
        } else if (m.st == Monster::kMunch) {
            int n = (int)t.mmMunches;
            float per = t.mmMunchT / n;
            int k = std::min(n, (int)(m.t / per));
            while (m.munched < k) { m.munched++; munchBurst(m, m.munched == n); eatenTongueMunch(m, n - m.munched); }
            float f = fmodf(m.t, per) / per;                    // opens, then snaps shut
            m.open = 0.85f * (f < 0.65f ? sinf(f / 0.65f * PI / 2) : 1 - (f - 0.65f) / 0.35f);
            if (m.munched >= n) { m.st = Monster::kChew; m.t = 0; m.munched = 0; m.open = 0; }
        } else {                                              // dead: little munches, drooling
            if (fadeIn > 0) m.t = fminf(m.t, t.mmChewT);       // (the fade in not over: the fade out waits for it)
            if (m.t >= t.mmFreeDelay) freeBellyFlies(m);      // (the flies in his belly: out, monstermunch_frees_flies_delay on)
            float ph = m.t * 2.5f;
            m.open = 0.22f * (0.5f - 0.5f * cosf(2 * PI * ph));
            if ((int)ph > m.munched) {
                m.munched = (int)ph;
                for (int j = 0; j < 2; j++) drool(m, 30, 150);
                hatJolt(m, 150);
                sound(Audio::kThud, 1.4f, 0.35f);
            }
            if (m.t >= t.mmChewT + t.mmFadeOut) {            // (a fly still in: out now)
                freeBellyFlies(m); respawn();
                if (goldenRun && !goldenDone) goldenAgain();
                return;
            }
        }
        if (!m.rope.empty()) numbTongueStep(m, dt, false);  // (its own numb tongue, if out: swallowed on, all the same)
        eatenTongueStep(m, dt);
        piecesStep(dt);
        looseHatStep(dt);
        flyStep(dt);
        partsStep(dt);
        updateCamera(dt);
        trauma = fmaxf(0, trauma - t.shakeDecay * dt);
    }
    // His tongue, going down with him: a chain from his mouth -- from its lips, once he is in it --
    // held at its far end while stuck (stretching, as he is dragged off), swallowed in its chews.
    void eatenTongueStep(Monster& m, float dt) {
        if (eatRope.size() < 2) return;
        if (eatChewLeft > 0) {
            float q = fminf(eatChewLeft, eatChewRate * dt);
            eatChewLeft -= q;
            eatRopeLen -= q;
        }
        if (eatHolds == 0 && eatRopeLen <= 2) {               // all in
            eatRope.clear(); eatRopePrev.clear(); eatRopeLen = eatChewLeft = 0;
            return;
        }
        Vector2 root = m.st == Monster::kPull ? centreW() : m.toWorld(mouthV(m).f);
        if (eatHolds > 0) eatRopeLen = fmaxf(eatRopeLen, Vector2Distance(root, eatAnchor));
        chainStep(eatRope, eatRopePrev, fmaxf(1, eatRopeLen), root, eatHolds > 0 ? &eatAnchor : nullptr, dt, kTongueHalfW, kTongueHalfW);
    }
    // A munch, on his tongue too (`left`: the munches after this one). Stuck, it holds: pulled in
    // -- some of its slack taken -- but no further; at its last hold, it gives way, numb. Numb, a
    // share of it goes down, in monstermunch_numb_chew_time, as the Monstermunch takes its own --
    // all that is left by the last munch.
    void eatenTongueMunch(Monster& m, int left) {
        if (eatRope.size() < 2) return;
        int me = (int)(&m - monsters.data());
        if (eatHolds > 0) {
            eatRopeLen = fmaxf(Vector2Distance(m.toWorld(mouthV(m).f), eatAnchor), 0.8f * eatRopeLen);
            if (--eatHolds == 0) {                            // given way: it whips aside as it falls (taut and straight, it would stand)
                int n = (int)eatRope.size();
                Vector2 d = Vector2Subtract(eatRope.back(), eatRope[0]), across = Vector2Normalize({-d.y, d.x});
                if (m.rnd(0, 1) < 0.5f) across = Vector2Negate(across);
                for (int j = 1; j < n; j++) eatRopePrev[j] = Vector2Subtract(eatRope[j], Vector2Scale(across, 3.0f * j / (n - 1)));
                sound(Audio::kRelease, 0.8f, 0.7f);
            } else sound(Audio::kStick, 0.8f, 0.5f);
            if (trace) fprintf(stderr, "t=%.2f monstermunch %d pulls at his stuck tongue: %s\n", time, me, eatHolds ? "it holds" : "it gives way");
            return;
        }
        float share = (eatRopeLen - eatChewLeft) / (left + 1);
        eatChewLeft += share;
        eatChewRate = eatChewLeft / t.mmNumbChewT;
        if (trace) fprintf(stderr, "t=%.2f monstermunch %d swallows %.0f px of his tongue (%.0f left)\n", time, me, share, eatRopeLen - eatChewLeft);
    }
    // How black the screen is, his end fading out (0 till then).
    float eatenFade() const {
        if (eatenBy < 0 || monsters[eatenBy].st != Monster::kChew) return 0;
        return clampf((monsters[eatenBy].t - t.mmChewT) / t.mmFadeOut, 0, 1);
    }
    // The last munch done -- monstermunch_frees_flies_delay after it --, the flies kept in his belly
    // (deathCleanup, monstermunch_frees_flies) fly out of its mouth: a colour fly home to its pole, a
    // lamp's fly to the lamp nearest.
    void freeBellyFlies(Monster& m) {
        Vector2 at = m.toWorld(mouthV(m).f);
        for (int* k : {&bellyFly, &mouthFly}) {
            if (*k < 0) continue;
            Fly& f = flies[*k];
            *k = -1;
            f.inMouth = f.caught = false;
            f.p = at; f.v = rotQ(m.face, {m.rnd(-60, 60), -240});
            f.freed = true; f.head = atan2f(f.v.y, f.v.x);   // (home as a Brownian swimmer: freedStep)
            if (f.pole >= 0) {
                f.followT = -1;
                m.tintCol = level.poleColour[f.pole]; m.tintAt = time;   // (its white back, from the fly's colour)
                if (trace) fprintf(stderr, "t=%.2f the colour fly gets out of monstermunch %d: home to pole %d\n", time, eatenBy, f.pole);
            } else {
                f.golden = true;
                if (trace) fprintf(stderr, "t=%.2f the lamp's fly gets out of monstermunch %d\n", time, eatenBy);
                if (f.lamp >= 0) lampFlySettle(f);
            }
        }
    }
    // A munch: blood cubes -- some of them bits of him, white -- popping out of its middle (behind
    // its face and teeth: seen through the mouth, then out of it), and saliva out of its mouth.
    // (Laid out in its own frame, its open face up; fast enough to get out against gravity.)
    void munchBurst(Monster& m, bool last) {
        for (int k = 0; k < (int)t.mmBlood; k++) {
            Piece pc;
            pc.p = m.toWorld({m.rnd(-8, 8), m.rnd(-8, 8)});
            float a = m.rnd(-0.55f, 0.55f), sp = m.rnd(520, 780);
            pc.v = rotQ(m.face, {sinf(a) * sp, -cosf(a) * sp});
            pc.a = m.rnd(0, 90); pc.av = m.rnd(-700, 700);
            pc.blood = k % 3 != 2;
            pc.size = pc.blood ? m.rnd(4, 7) : m.rnd(6, 9);
            pieces.push_back(pc);
        }
        for (int k = 0; k < 7; k++) drool(m, 80, 260);
        hatJolt(m, last ? 300 : 240);
        trauma = fminf(1, trauma + (last ? 0.5f : 0.3f));
        sound(Audio::kCrash, 0.75f);
        if (last) sound(Audio::kDeath, 0.8f);
        if (trace) fprintf(stderr, "t=%.2f monstermunch munch %d%s\n", time, m.munched, last ? ": Leon is dead" : "");
    }
    // Saliva flying off a point: n drops, along dir give or take `spread` radians, slow..fast.
    void spray(Monster& m, Vector2 at, Vector2 dir, int n, float spread, float slow, float fast) {
        for (int k = 0; k < n; k++) {
            Particle q;
            q.p = at;
            q.v = Vector2Scale(Vector2Rotate(dir, m.rnd(-spread, spread)), m.rnd(slow, fast));
            q.maxLife = q.life = 1.4f;
            q.r = m.rnd(2.0f, 3.6f);
            q.drop = true; q.mm = true;
            parts.push_back(q);
        }
    }
    void drool(Monster& m, float slow, float fast, float spread = 20) {   // a drop of saliva out of its mouth
        Particle q;
        q.p = m.toWorld({mouthV(m).f.x + m.rnd(-spread, spread), -m.r.width / 2 - 2});
        float a = m.rnd(-1.0f, 1.0f), sp = m.rnd(slow, fast);
        q.v = rotQ(m.face, {sinf(a) * sp, -cosf(a) * sp});
        q.maxLife = q.life = 1.4f;
        q.r = m.rnd(2.0f, 3.6f);
        q.drop = true; q.mm = true;
        parts.push_back(q);
    }

    void step(float dt) {
        prevPos = pos;
        level.endWallH = (int)t.endWallH;          // (the ending's wall: ending_wall_height, read live)
        if (lessonT >= 0) lessonT += dt;
        for (float& f : flagT) if (f >= 0) f += dt;
        if (blackFlagT >= 0) blackFlagT += dt;
        if (ownT >= 0) ownT += dt;
        lampFlicker();
        polesStep(dt);
        if (hatKeep >= 0 && (!hatLoose || (hatKeepT >= 0 && (hatKeepT -= dt) <= 0))) hatKeep = -1;   // the hat's colour: the fly's time out -- or his again
        for (size_t i = 0; i < doorT.size(); i++)           // a door turning gold, then open and fading
            if (doorT[i] >= 0) {
                float before = doorT[i];
                doorT[i] += dt;
                if (before < t.doorGold && doorT[i] >= t.doorGold) openDoor(i);
            }
        if (eatenBy >= 0) {                // a Monstermunch has him: nothing he does counts
            tonguePress = false; chewPress = false; jumpBuf = 0;
            eatenStep(dt);
            return;
        }
        if (dead) {                        // only the pieces, the world and the camera go on
            tonguePress = false; chewPress = false; jumpBuf = 0;    // presses while dead are dropped
            prevCam = cam;
            time += dt;
            deathStep(dt);
            if (!dead) return;
            crateStep(dt);
            looseHatStep(dt);
            monstersStep(dt, false, false);
            flyStep(dt);
            for (auto& q : parts) { q.life -= dt; q.p.x += q.v.x * dt; q.p.y += q.v.y * dt; }
            parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& q) { return q.life <= 0; }), parts.end());
            trauma = fmaxf(0, trauma - t.shakeDecay * dt);
            return;
        }
        if (goneT >= 0) {
            bool was = gone();
            goneT += dt;
            if (!was && gone()) {
                if (trace) fprintf(stderr, "t=%.2f faded out: gone -- the golden run's way in\n", time);
                goldenSeq = 0;
            }
        }
        if (goldenSeq >= 0) {              // the golden run's way in: black, the world put back, the screen back, Leon showing
            float before = goldenSeq;
            goldenSeq += dt;
            if (before < t.goldenFadeOut && goldenSeq >= t.goldenFadeOut) goldenStart();
            float letGo = goldenShown() + t.goldenLeonIn;
            if (before < letGo && goldenSeq >= letGo) {
                goldenClockAt = time;
                if (trace) fprintf(stderr, "t=%.2f golden run: Leon let go\n", time);
            }
        }
        if (lostT >= 0 && (lostT += dt) >= t.deathWorldFade) {   // out of the map, the screen black: back at his checkpoint
            respawn();
            if (trace) fprintf(stderr, "t=%.2f out of the map: back at the checkpoint\n", time);
        }
        if (gone() || goldenHeld() || lostT >= 0) {   // into a dark light, and faded out -- or held in the golden run's, or out of the map: the world goes on without him
            tonguePress = false; chewPress = false; jumpBuf = 0;
            prevCam = cam;
            time += dt;
            crateStep(dt);
            looseHatStep(dt);
            monstersStep(dt, false, false);
            flyStep(dt);
            for (auto& q : parts) { q.life -= dt; q.p.x += q.v.x * dt; q.p.y += q.v.y * dt; }
            parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& q) { return q.life <= 0; }), parts.end());
            trauma = fmaxf(0, trauma - t.shakeDecay * dt);
            return;
        }
        fadeIn = fmaxf(0, fadeIn - dt);
        stepVelX = vel.x;
        prevCam = cam;
        time += dt;
        if (!finished) runTime += dt;
        if (goldenOutT >= 0) goldenOutT += dt;
        if (goldenSeq >= 0 && goldenLandAt < 0 && onGround) {   // the golden run: let go, landed -- the light fades out...
            goldenLandAt = goldenSeq; goldenOutT = 0;
            if (trace) fprintf(stderr, "t=%.2f golden run: landed\n", time);
        }
        if (goldenSeq >= 0 && goldenLandAt >= 0 && goldenSeq >= goldenLandAt + t.goldenWait) {   // ...his shape settled: main counts 3, 2, 1, GO
            goldenSeq = goldenLandAt = -1;
            wantCountdown = true;
            if (trace) fprintf(stderr, "t=%.2f golden run: 3, 2, 1, GO\n", time);
        }
        // How far he falls: from his highest point since he last stood, hung by the tongue, was
        // knocked out or turned with gravity.
        if (onGround || tg == tgStuck || knock != kNone) fallTop = pos.y;
        else fallTop = fminf(fallTop, pos.y);

        if (knock != kNone) knockStep(dt);
        else if (goldenSeq >= 0) { tonguePress = false; chewPress = false; jumpBuf = 0; }   // (the golden run's way in: no controls of his yet)
        else if (dashing()) { tonguePress = false; chewPress = false; }   // (Anna dashing: nothing else till it is over)
        else control(dt);
        if (anna) dashStep(dt);
        // Standing on an edge with his middle already past it: he can't stay there -- he steps
        // off, keeping any speed he has that way, until he drops and falls as usual.
        if (knock == kNone && onGround && !supportedAt(pos.x - 1, pos.x + 1)) {
            float dir = supportedAt(pos.x - kLeonW / 2, pos.x) ? 1.0f : -1.0f;   // away from the platform
            if (vel.x * dir < t.edgeSlip) vel.x = dir * t.edgeSlip;
        }

        // The pull, in Leon's frame. Mid-turn it swings from the old direction to +y, always
        // pointing at the bottom of the turning screen.
        Vector2 pull = {0, 1};
        if (turnStart >= 0) {
            float u = bellEase((time - turnStart) / t.turnTime, t.turnSigma);
            float phi = (90.0f - turnDelta * (1 - u)) * DEG2RAD;
            pull = {cosf(phi), sinf(phi)};
        }
        pullW = toWorld(pull);
        float g = t.gravity;
        if (vel.y > 0) g *= t.fallMult;
        else if (jumping && !jumpHeld) g *= t.jumpCutMult;   // let go early: shorter hop
        vel.y = fminf(vel.y + g * pull.y * dt, t.maxFall);
        vel.x += t.gravity * pull.x * dt;
        if (vel.y >= 0) jumping = false;
        if (dashT >= 0) vel = {dashDir * dashSpeed(), 0};   // (Anna dashing: straight on, gravity held off)
        else if (dashPrepT >= 0) vel = {0, 0};             // (...winding up: still, where she is)

        pushCrates(dt);
        bool hitX = false;
        if ((hitX = moveX(vel.x * dt))) {
            if (knock != kNone) { vel.x *= -0.35f; knockOmega *= -0.35f; }   // knocked off a wall
            else vel.x = 0;
            if (dashT >= 0) dashEnd(true);
        }
        if (dashT >= 0 && (dashT += dt) >= t.dashTime) dashEnd(false);
        float impact = vel.y;
        bool wasGround = onGround;
        int hit = moveY(vel.y * dt);
        if (knock != kNone && (hitX || hit != 0)) nudgeCrates();
        if (tonguePress) {
            tonguePress = false;
            if (!hatOnTongue) {                          // (bringing his hat back: the tongue heeds nothing till it is on his head)
                pressTongue(toWorld(tongueAim));
                if (fabsf(tongueAim.x) > 0.2f && knock == kNone) facing = signf(tongueAim.x);
            }
        }
        tongueStep(dt);
        touchCrates();
        crateStep(dt);
        if (!god && !dashing() && touchingDeadly()) { die(); return; }   // (GOD: dark matter is ground -- Anna dashing: nothing hurts her)
        if (suicideStep(dt)) return;
        if (monstersStep(dt, true, !dashing())) return;   // a Monstermunch's tongue on him
        for (size_t i = 0; i < level.pines.size(); i++)
            if (!pineEaten[i] && !pineAway(i) && level.pineapple(i) && (int)i != caughtPine && Vector2Distance(centreW(), level.pines[i]) < 28.0f)
                eatPine((int)i);
        cpMsg = fmaxf(0, cpMsg - dt);
        for (size_t i = 0; i < level.fruits.size(); i++)
            if (!fruitEaten[i] && Vector2Distance(centreW(), level.fruits[i]) < 28.0f) {
                fruitEaten[i] = true;
                char kind = level.fruitKind[i];
                bool dj = kind == 'D';
                if (kind == 'U') {                       // the banana: no ability, a word of advice
                    abilityText = "BANANA";
                    lesson("Swallow the fly !");
                } else if (kind == 'Z') {                // the kaki: the same -- looking down
                    abilityText = "KAKI";
                    lessonHow("Look down !", "{v}Right Stick{i} or keyboard {b}<Alt>+<Down>");
                } else {
                    if (dj) canDouble = true; else canTongue = true;
                    abilityText = dj ? "DOUBLE JUMP" : "TONGUE";
                    abilityMsg = 1.8f;
                    if (goldenRun) {}                        // (a golden run: no lesson -- he knows)
                    else if (dj) lessonHow("Double-jump !", "{i}Joypad {A}{i} - keyboard {b}<spacebar>");
                    else lessonHow("Tongue !", "{i}Joypad {B}{i} - keyboard {b}<X>");
                }
                flyChew = 0; chews = 0;
                sparks(level.fruits[i], false);
                sound(Audio::kFruit, dj ? 1.0f : 0.85f);
                if (trace) fprintf(stderr, "t=%.2f fruit eaten: %s\n", time, abilityText);
            }
        abilityMsg = fmaxf(0, abilityMsg - dt);
        if (level.hasHatSpot && !hatTaken && Vector2Distance(centreW(), level.hatSpot) < 28.0f) {
            hatTaken = hasHat = true;                        // on his head at once
            lesson("Hat !");
            sparks(level.hatSpot, false);
            sound(Audio::kFruit, 1.1f);
            if (trace) fprintf(stderr, "t=%.2f hat taken\n", time);
        }
        for (size_t i = 0; i < level.cherries.size(); i++)
            if (!cherryEaten[i] && Vector2Distance(centreW(), level.cherries[i]) < 28.0f) {
                cherryEaten[i] = true;
                lessonHow("Extend or Swallow Tongue !", "{i}Joypad {n}LT{i} or {n}RT{i} - keyboard {b}<C> and <V>");
                flyChew = 0; chews = 0;
                sparks(level.cherries[i], false);
                sound(Audio::kFruit, 1.15f);
                if (trace) fprintf(stderr, "t=%.2f cherry eaten\n", time);
            }
        if (grapeAvailable() && Vector2Distance(centreW(), grapeAt()) < 28.0f) {   // her grape: the dash's lesson
            grapeEaten = true;
            abilityText = "GRAPE";
            lessonHow("Dash !", "{i}Joypad {o}RB{i} or {o}LB{i} - Keyboard {b}<Shift>");
            flyChew = 0; chews = 0;
            sparks(grapeAt(), false);
            sound(Audio::kFruit, 1.05f);
            if (trace) fprintf(stderr, "t=%.2f grape eaten: the dash's lesson\n", time);
        }
        // The keys, each a checkpoint, and the start of a mode, on its own clock: the red one (the
        // exit's), hard mode -- in a level without a yellow flag, it ends easy mode too -- the green
        // one, dark mode, the blue one, harder mode. Each says which door it opens.
        if (level.hasExit && !completed && CheckCollisionCircleRec(keyPos(), kKeyLen * 0.4f, leonWorldBox())) {
            completed = true;
            hasKey = true;
            if (!level.flags[flYellow].has) endEasy();
            sound(Audio::kFruit, 1.25f);
            sparks(keyPos(), false);
            if (trace) fprintf(stderr, "t=%.2f red key taken\n", time);
            checkpoint(level.exitKeyPine);
            goToDoor('L', "Red key !", "Red");
            startHardMode();
            notePowers(1);
        }
        for (size_t i = 0; i < level.blueKeys.size(); i++)      // a blue key, for a blue door
            if (!blueKeyTaken[i] && CheckCollisionCircleRec(keyBob(level.blueKeys[i]), kKeyLen * 0.4f, leonWorldBox())) {
                blueKeyTaken[i] = true; hasBlueKey = true;
                sound(Audio::kFruit, 1.15f);
                sparks(keyBob(level.blueKeys[i]), false);
                checkpoint(level.blueKeyPine[i]);
                goToDoor('l', "Blue key !", "Blue");
                if (!harderMode) { harderMode = true; harderDone = false; harderTime = 0; notePowers(3); }
                if (trace) fprintf(stderr, "t=%.2f blue key taken: harder mode\n", time);
            }
        for (size_t i = 0; i < level.greenKeys.size(); i++)     // a green key, for a green door
            if (!greenKeyTaken[i] && CheckCollisionCircleRec(keyBob(level.greenKeys[i]), kKeyLen * 0.4f, leonWorldBox())) {
                greenKeyTaken[i] = true; hasGreenKey = true;
                sound(Audio::kFruit, 1.15f);
                sparks(keyBob(level.greenKeys[i]), false);
                checkpoint(level.greenKeyPine[i]);
                goToDoor('g', "Green key !", "Green");
                if (!darkMode) { darkMode = true; darkDone = false; darkTime = 0; notePowers(2); }
                if (trace) fprintf(stderr, "t=%.2f green key taken: dark mode\n", time);
            }
        for (size_t i = 0; i < level.purpleKeys.size(); i++)    // a purple key, for a purple door
            if (!purpleKeyTaken[i] && CheckCollisionCircleRec(keyBob(level.purpleKeys[i]), kKeyLen * 0.4f, leonWorldBox())) {
                purpleKeyTaken[i] = true; hasPurpleKey = true;
                sound(Audio::kFruit, 1.15f);
                sparks(keyBob(level.purpleKeys[i]), false);
                checkpoint(level.purpleKeyPine[i]);
                goToDoor('q', "Purple key !", "Purple");
                if (!camMode) { camMode = true; camDone = false; camTime = 0; notePowers(4); }
                startBalloons();                                 // (the cameleon part starts: its balloons from the start)
                if (trace) fprintf(stderr, "t=%.2f purple key taken: cameleon mode\n", time);
            }
        for (size_t i = 0; i < level.goldKeys.size(); i++)      // the golden key, for a golden door: no mode, no clock
            if (!goldKeyTaken[i] && CheckCollisionCircleRec(keyBob(level.goldKeys[i]), kKeyLen * 0.4f, leonWorldBox())) {
                goldKeyTaken[i] = true; hasGoldKey = true;
                goldMusic = 3;                                   // (the music fades out)
                sound(Audio::kFruit, 1.15f);
                sparks(keyBob(level.goldKeys[i]), false);
                checkpoint(level.goldKeyPine[i]);
                goToDoor('O', "Golden key !", "Golden");
                if (trace) fprintf(stderr, "t=%.2f golden key taken\n", time);
                if (!goldenRun && goldKeyAt < 0) goldKeyAt = time;   // (the first: the modes' clocks fade out)
                if (goldenRun && !goldenDone && !level.blackFlag.has) {   // taken again: the golden run's end (a level without a black flag) -- its clock stops, its highscores
                    goldenDone = true;
                    partsDone.push_back({5, goldenTime});
                    if (trace) fprintf(stderr, "t=%.2f golden run completed in %.1f s\n", time, goldenTime);
                }
            }
        for (const Vector2& d : level.darkLights)                // a dark light: his middle at its own -- he fades out (goneT)
            if (goneT < 0 && Vector2Distance(centreW(), d) < kTile / 2) {
                goneT = 0;
                if (trace) fprintf(stderr, "t=%.2f into the dark light: fading out\n", time);
            }
        // A locked door, touched holding its key (red for 'L', blue for 'l', green for 'g', purple for
        // 'q', golden for 'O' -- the first golden door it meets takes it: the others stay shut): the key
        // goes in, the door turns gold.
        for (size_t i = 0; i < level.doors.size(); i++) {
            char dc = level.doorChar[i];
            bool blue = dc == 'l', green = dc == 'g', purple = dc == 'q', golden = dc == 'O';
            if (doorT[i] >= 0 || !(golden ? hasGoldKey : purple ? hasPurpleKey : green ? hasGreenKey : blue ? hasBlueKey : keyHeld())) continue;
            Rectangle d = level.doors[i];
            if (!CheckCollisionRecs(leonWorldBox(), {d.x - 2, d.y - 2, d.width + 4, d.height + 4})) continue;
            doorT[i] = 0;
            if (golden) hasGoldKey = false; else if (purple) hasPurpleKey = false; else if (green) hasGreenKey = false; else if (blue) hasBlueKey = false; else hasKey = false;
            if (green && !level.flags[flGreen].has) endDark();   // (no green flag: the green door ends the dark)
            sound(Audio::kFruit, 0.8f);
            if (trace) fprintf(stderr, "t=%.2f the %s key goes into door %d\n", time, golden ? "golden" : purple ? "purple" : green ? "green" : blue ? "blue" : "red", (int)i);
            break;
        }
        if (hardMode && !hardDone) {                         // the way back, on its own clock
            hardTime += dt;
            std::vector<Vector2> w = hardWaypoints();
            Vector2 c = centreW();
            for (int k = hardNext; k + 1 < (int)w.size(); k++)    // by an old checkpoint (even out of turn): on to the next
                if (Vector2Distance(c, w[k]) < t.guidePass * kTile) {
                    hardNext = k + 1;
                    if (trace) fprintf(stderr, "t=%.2f hard mode: by waypoint %d, on to the next\n", time, k);
                }
            // Home, in a level without a red flag: back near the start. (With one: the flag, below.)
            if (!level.flags[flRed].has && Vector2Distance(c, startCentre()) < t.homeDist * kTile) {
                hardDone = true;
                hardNext = (int)w.size() - 1;
                sound(Audio::kFruit, 1.25f);
                partsDone.push_back({1, hardTime});
                if (trace) fprintf(stderr, "t=%.2f hard mode completed in %.1f s\n", time, hardTime);
            }
        }
        // A flag touched: a checkpoint, and the music of the place (the red one: World Of Pistou II,
        // the blue one: Dying Respect). It goes up if its mode ran -- its key taken in the level --
        // and ends it (its clock stops): the yellow one, easy mode; the red one, hard mode; the green
        // one, dark mode; the blue one, harder mode; the violet one, cameleon mode. Its mode never
        // run (a debug key, or none), it stays furled -- to go up later, the key taken.
        // (A golden run: no flags -- only their music still changes where they stand.)
        for (int f = 0; f < flCount; f++) {
            const Level::Flag& fl = level.flags[f];
            if (!fl.has || flagT[f] >= 0 || !CheckCollisionRecs(leonWorldBox(), fl.rect)) continue;
            if (goldenRun) {
                int was = musicPart;
                if (f == flRed) musicPart = std::max(musicPart, 1);
                if (f == flBlue) musicPart = std::max(musicPart, 2);
                if (f == flViolet && camMode) musicPart = 3;
                if (trace && musicPart != was) fprintf(stderr, "t=%.2f golden run: past the %c flag's place, its music\n", time, kFlagLetters[f]);
                continue;
            }
            if (!flagTouched[f]) {
                flagTouched[f] = true;
                checkpoint(fl.pine);
                if (f == flRed) musicPart = std::max(musicPart, 1);
                if (f == flBlue) musicPart = 2;
                if (trace) fprintf(stderr, "t=%.2f flag %c touched\n", time, kFlagLetters[f]);
            }
            bool ran = (f == flYellow && startPart == 0) || (f == flRed && hardMode) || (f == flGreen && darkMode) ||
                       (f == flBlue && harderMode) || (f == flViolet && camMode);
            if (!ran) continue;
            flagT[f] = 0;
            sound(Audio::kFruit, 1.25f);
            checkpoint(fl.pine);
            if (trace) fprintf(stderr, "t=%.2f flag %c up\n", time, kFlagLetters[f]);
            if (f == flYellow) endEasy();
            if (f == flRed && !hardDone) {
                hardDone = true;
                hardNext = (int)hardWaypoints().size() - 1;
                partsDone.push_back({1, hardTime});
                if (trace) fprintf(stderr, "t=%.2f hard mode completed in %.1f s\n", time, hardTime);
            }
            if (f == flGreen) endDark();
            if (f == flBlue) endHarder();
            if (f == flViolet && !camDone) {
                camDone = true;
                musicPart = 3;                               // (the music fades out: the way outside in silence)
                partsDone.push_back({4, camTime});
                if (trace) fprintf(stderr, "t=%.2f cameleon mode completed in %.1f s\n", time, camTime);
            }
        }
        // The black flag touched: the golden run's highscores show by it, under its name. In a golden
        // run, it ends it: up, its clock stops, its highscores (the name typed there) -- and, up, it is
        // his checkpoint from then on (its foot, the right way up). Not one otherwise.
        if (level.blackFlag.has && CheckCollisionRecs(leonWorldBox(), level.blackFlag.rect)) {
            if (!blackFlagTouched) {
                blackFlagTouched = true;
                if (trace) fprintf(stderr, "t=%.2f black flag touched\n", time);
            }
            if (goldenRun && !goldenDone) {
                goldenDone = true; blackFlagT = 0;
                const Rectangle& r = level.blackFlag.rect;
                cpPine = -1; cpGrav = 0; cpFeet = {r.x + r.width / 2, r.y + r.height};
                sound(Audio::kFruit, 1.25f);
                partsDone.push_back({5, goldenTime});
                if (trace) fprintf(stderr, "t=%.2f black flag up: golden run completed in %.1f s\n", time, goldenTime);
            }
        }
        // Through a golden door, open -- his centre in its doorway --, its colour his: the Love door (the
        // left one), pastel pink; the Solo door, gold.
        for (size_t i = 0; i < doorT.size(); i++)
            if (level.doorChar[i] == 'O' && doorT[i] >= t.doorGold && CheckCollisionPointRec(centreW(), level.doors[i])) {
                int k = goldDoorRank(i) == 0 ? 2 : 1;
                if (k == own) continue;
                ownFrom = bodyColour(); ownPinkFrom = hatPink(); own = k; ownT = 0;
                if (trace) fprintf(stderr, "t=%.2f through the %s door: %s\n", time, k == 2 ? "Love" : "Solo", k == 2 ? "pastel pink" : "gold");
            }
        if (darkMode && !darkDone) darkTime += dt;
        if (harderMode && !harderDone) harderTime += dt;
        if (camMode && !camDone) camTime += dt;
        if (clockRuns(5)) goldenTime += dt;
        for (int k = 0; k < 6; k++)                      // a clock run up to clock_limit (an hour): he explodes -- then the title
            if (clockRuns(k) && clockOf(k) >= t.clockLimit) {
                timeUp = true;
                if (trace) fprintf(stderr, "t=%.2f clock %d at its limit: Leon explodes\n", time, k);
                die();
                return;
            }
        spareHatStep();
        looseHatStep(dt);
        if (hatLoose && !hatOnTongue && Vector2Distance(centreW(), hatP) < 28) {   // the loose hat touched: his again
            sparks(hatP, false);
            hatBack();
        }
        GravityContact gravHit = gravityContact();
        bool turning = turnStart >= 0 && time - turnStart < t.turnTime;
        // Bumping into something from below knocks him out, and so does landing from higher than
        // knock_fall_height screens. A gravity platform never does -- it turns the world instead --
        // and nothing does while the world is turning, or while his tongue is stuck.
        bool spared = turning || gravityContact(true).frame >= 0 || tg == tgStuck;   // or hanging by the tongue
        // The hat meets a ceiling before his head does: crushed (drawn no taller than the room
        // left), it takes the bump as soon as it is flat to its folded height -- his head never
        // gets any closer. (Too fast for that, the head's own bump still is the hat's.)
        // Dark matter is no ceiling to the hat: it takes no bump off it, and stops nothing -- he goes on,
        // and dies if his body gets there. Only his hat in it, he lives, the hat squashed to the room left.
        updateHatClearance();
        if (anna) {
            float want = vel.x > 30 ? -1.0f : vel.x < -30 ? 1.0f : hatLeanDir >= 0 ? 1.0f : -1.0f;   // (still: as it was)
            hatLeanDir += clampf(want - hatLeanDir, -6 * dt, 6 * dt);
        }
        if (hit == 0 && hatGuards() && knock == kNone && -impact > t.crashMin && !spared &&
            hatRoom <= hatSeat() + hatCrown() * hatLowest() && !hatInDarkMatter()) {
            if (trace) fprintf(stderr, "t=%.2f the hat meets the ceiling (%.1f px above his head)\n", time, hatRoom);
            hatBump();
        }
        if (hit == -1) {
            if (knock == kNone && -impact > t.crashMin && !spared) {
                if (hatGuards()) hatBump();                     // the hat takes it
                else startKnock(pos.y - kLeonH);
            } else vel.y = 0;
        } else if (hit == 1) {
            if (knock == kNone && !spared && pos.y - fallTop > t.knockFall * viewSize().y) fallKnock(impact);
            else if (knock == kAir) knockLand(impact);
            else vel.y = 0;
        }
        if (gravHit.frame >= 0 && !turning) {
            if (gravHit.knocksHatOff && grav == 0 && impact > 0)
                knockHatOff({vel.x, -impact});               // rebound up the shaft with the speed of the fall
            setGravity(gravHit.frame);
        }
        if (turnStart >= 0 && time - turnStart >= t.turnTime + t.playerTurnDelay) {
            turnFrom += turnDelta; turnStart = -1;      // the turn (and Leon's lag behind it) is over
        }
        onGround = (vel.y >= 0) && standing();
        endingStep(dt);
        if (knock == kNone) {
            if (onGround && !wasGround) land(impact);
            if (onGround) { airJump = true; spin = 0; }
        } else if (!onGround && knock != kAir) {
            knock = kAir;                 // rolled off an edge: keeps falling, still out cold
            bounces = 0;
            settling = false;
            knockOmega = vel.x / kRollR * RAD2DEG;
            if (fabsf(knockOmega) < 120) knockOmega = facing * 120;
        }

        // Out of the map, whichever way (12 tiles past its edge): he stops there, the world fades to
        // black (death_world_fade), and he is back at his checkpoint, as after a death -- no pieces.
        Vector2 wc = toWorld(pos);
        const float m = 12 * kTile;
        if (wc.x < -m || wc.y < -m || wc.x > level.w * kTile + m || wc.y > level.h * kTile + m) {
            lostT = 0; vel = {0, 0};
            if (trace) fprintf(stderr, "t=%.2f out of the map: fading to black\n", time);
        }
        afterMove(dt);
        if (trace && fmodf(time, 1.0f) < dt) { Vector2 w = toWorld(pos); fprintf(stderr, "t=%.2f at tile (%.1f,%.1f) gravity %d\n", time, w.x / kTile, w.y / kTile, grav); }
        if (trace && knock != tracedKnock) {
            static const char* names[] = {"none", "air", "roll", "down", "rise"};
            fprintf(stderr, "t=%.2f knock %s  pos=(%.0f,%.0f) vel=(%.0f,%.0f) bounces=%d angle=%.0f\n", time,
                    names[knock], pos.x, pos.y, vel.x, vel.y, bounces, knockAngle);
            tracedKnock = knock;
        }
    }

    // Anna's dash: LB or RB, either Shift (main: dashPress), the way she faces -- standing, or in the air
    // (anna_dash_air of them before she stands again); not knocked out, her hat not on her tongue, the
    // ending not begun, the last one anna_dash_cooldown ago. Wound up first, still, where she is -- her
    // tongue, out, swallowed meanwhile --; then off, a little cloud left where she was.
    float dashSpeed() const { return t.dashDist * kTile / t.dashTime; }
    bool dashing() const { return dashT >= 0 || dashPrepT >= 0; }
    void resetDash() {
        dashPress = shiftDashPending = false;
        dashPrepT = dashT = dashOutT = dashBonkT = -1;
        dashGone = dashWait = 0;
        dashAirUsed = 0;
    }
    void dashStep(float dt) {
        bool press = dashPress;
        dashPress = false;
        if (dashOutT >= 0 && (dashOutT += dt) >= fmaxf(t.dashBlurOut, t.dashRecover)) dashOutT = -1;
        if (dashBonkT >= 0 && (dashBonkT += dt) >= 0.8f) dashBonkT = -1;
        dashWait = fmaxf(0, dashWait - dt);
        if (onGround) dashAirUsed = 0;
        if (dashPrepT >= 0 && (dashPrepT += dt) >= t.dashWindup) {   // wound up: off
            if (tg == tgBack) { tgExt = 0; tg = tgNone; swallow(); }   // (the tongue in, whatever was left of it)
            dashPrepT = -1; dashT = 0; dashGone = 0;
            for (int i = 0; i < 9 && onGround; i++) {     // (the cloud, on the ground: puffs where she stood, drifting back, fading)
                Particle q;
                q.p = {pos.x - dashDir * GetRandomValue(0, 10), pos.y - GetRandomValue(2, 26)};
                q.v = {-dashDir * GetRandomValue(10, 50), (float)GetRandomValue(-25, 5)};
                q.maxLife = q.life = GetRandomValue(30, 50) / 100.0f;
                q.r = GetRandomValue(30, 55) / 10.0f;
                q.spark = false;
                emit(q);
            }
            sound(Audio::kJump, 1.3f, 0.6f);
            if (trace) fprintf(stderr, "t=%.2f Anna dashes %s\n", time, dashDir < 0 ? "left" : "right");
        }
        if (!press || dashing() || knock != kNone || endT >= 0 || goldenSeq >= 0 || dashWait > 0 || hatOnTongue) return;
        if (!onGround && dashAirUsed >= t.dashAir) return;
        if (!onGround) dashAirUsed++;
        if (tg != tgNone) {                               // the tongue out: swallowed quickly, as she winds up
            if (tg != tgBack) tongueRetract(fmaxf(0.01f, t.dashWindup * 0.9f));
            else tgBackSpeed = fmaxf(tgBackSpeed, tongueReach() / fmaxf(0.01f, t.dashWindup * 0.9f));
            tgResend = false;
        }
        dashPrepT = 0; dashDir = facing < 0 ? -1.0f : 1.0f; dashOutT = -1;
        jumping = false; spin = 0;
        if (trace) fprintf(stderr, "t=%.2f Anna winds up a dash\n", time);
        if (t.dashWindup <= 0) dashStep(0);
    }
    // Over: run out of it at running speed -- or stopped by a wall: her eyes knocked shut a while, no more
    // (not knocked out).
    void dashEnd(bool wall) {
        if (trace) fprintf(stderr, "t=%.2f Anna's dash %s\n", time, wall ? "stopped by a wall: her eyes knocked shut a while" : "over");
        dashT = -1; dashOutT = 0; dashWait = t.dashCool;
        vel = {wall ? 0 : dashDir * fminf(t.runSpeed, dashSpeed()), 0};
        if (wall) dashBonkT = 0;                            // (her eyes shut, open, shut, open)
        for (int i = 0; i < 6 && onGround; i++) {         // (stopping on the ground: a little dust behind her feet)
            Particle q;
            q.p = {pos.x - dashDir * GetRandomValue(4, 16), pos.y - GetRandomValue(1, 8)};
            q.v = {-dashDir * GetRandomValue(5, 30), (float)GetRandomValue(-20, 0)};
            q.maxLife = q.life = GetRandomValue(25, 40) / 100.0f;
            q.r = GetRandomValue(25, 45) / 10.0f;
            q.spark = false;
            emit(q);
        }
    }
    // Her eyes through a dash -- from the wind-up's start to the stop's end --: narrowed (anna_dash_eyes of
    // their height, as wide), in and out in 0.04 s. How much, 0 to 1.
    float dashSquint() const {
        const float e = 0.04f;
        if (dashPrepT >= 0) return smooth01(dashPrepT / fminf(e, fmaxf(t.dashWindup, 1e-3f)));
        if (dashT >= 0) return 1;
        if (dashOutT >= 0 && dashOutT < t.dashRecover) return smooth01((t.dashRecover - dashOutT) / e);
        return 0;
    }
    // Into a wall: not knocked out, but her eyes shut 0.5 s, open 0.2 s, shut 0.1 s, and open again.
    float dashBonkT = -1;                     // seconds since a dash met a wall (-1: none)
    bool dashBonkShut() const { return dashBonkT >= 0 && (dashBonkT < 0.5f || (dashBonkT >= 0.7f && dashBonkT < 0.8f)); }
    // How blurred she is (0 to 1): blurring in at the start (anna_dash_blur_in), out after the end.
    float dashBlur() const {
        if (dashT >= 0) return clampf(dashT / t.dashBlurIn, 0, 1);
        if (dashOutT >= 0) return 1 - clampf(dashOutT / t.dashBlurOut, 0, 1);
        return 0;
    }
    // Her pose through a dash (drawAnnaDash), after workshop/Anna dash.jpg -- the way she goes +: her body
    // stretched (sx, sy) about its middle, that middle so high over her feet (lift); her cone bent (hat,
    // degrees, + forwards); her brims' curls behind and in front (1 as ever, 0 flat, more: tighter); her
    // legs' angle from upright (+ backwards) and their bend (0 straight, 1 a knee well out); her face
    // slid forwards (of her body's half width); her body tilted (degrees, + nose down); her eyes looking
    // down. Wound up (anna_dash_windup) through three poses, held through the dash; then, stopping
    // (anna_dash_recover), three more and back as ever. False: none of it now.
    struct DashPose { float sx, sy, lift, hat, brimBack, brimFront, legA, legK, face, tilt, lookDown; };
    bool dashPose(DashPose& o) const {
        static const DashPose N = {1, 1, kLeonH + 2 - kBodyH / 2, 0, 1, 1, 0, 0, 0, 0, 0};   // as ever
        static const DashPose S[3] = {
            {1.08f, 0.92f, 23.5f, 12, 0.35f, 1.35f, -8, 0.85f, 0.15f, -15, 0},   // 1: knees bent, her body tilted back (15 degrees, nose up), the cone tipped forwards, the brim behind flattening
            {1.32f, 0.76f, 20.0f, -48, 0.8f, 1.7f, 35, 0.35f, 0.32f, 0, 0},      // 2: the cone whipped back, stretched, legs going back
            {1.95f, 0.52f, 15.0f, -84, 0.0f, 2.1f, 78, 0.05f, 0.62f, 0, 0}};     // 3, the dash: long and flat, the cone behind, legs trailing
        static const DashPose T[3] = {
            {1.5f, 0.66f, 17.0f, 42, 1.7f, 0.1f, -42, 0.25f, 0.4f, 6, 0},        // 1: the cone thrown forwards, feet ahead, skidding
            {1.18f, 0.84f, 21.0f, 72, 1.5f, 0.5f, -12, 0.95f, 0.25f, 8, 1},      // 2: the cone bent flat forwards, knees bent, looking down
            {1.06f, 0.92f, 23.0f, -8, 1.25f, 1.25f, 0, 0.7f, 0.1f, 0, 0}};       // 3: the cone springing back up, a squat
        auto mix = [](const DashPose& a, const DashPose& b, float w) {
            const float* p = &a.sx; const float* q = &b.sx;
            DashPose r;
            float* z = &r.sx;
            for (size_t i = 0; i < sizeof(DashPose) / sizeof(float); i++) z[i] = lerpf(p[i], q[i], smooth01(w));
            return r;
        };
        auto through = [&](const DashPose* k, int n, const float* at, float u) {   // (keyframes k[0..n-1] at times at[0..n-1])
            for (int i = 0; i + 1 < n; i++)
                if (u <= at[i + 1]) return mix(k[i], k[i + 1], (u - at[i]) / fmaxf(at[i + 1] - at[i], 1e-4f));
            return k[n - 1];
        };
        if (dashPrepT >= 0) {
            const DashPose k[4] = {N, S[0], S[1], S[2]};
            const float at[4] = {0, 1.0f / 3, 2.0f / 3, 1};
            o = through(k, 4, at, dashPrepT / fmaxf(t.dashWindup, 1e-3f));
        } else if (dashT >= 0) {
            o = S[2];
        } else if (dashOutT >= 0 && dashOutT < t.dashRecover) {
            const DashPose k[5] = {S[2], T[0], T[1], T[2], N};
            const float at[5] = {0, 0.15f, 0.45f, 0.75f, 1};
            o = through(k, 5, at, dashOutT / t.dashRecover);
        } else return false;
        return true;
    }

    // Running and jumping: everything the player controls.
    void control(float dt) {
        // v_max = G(v_cmd) * v_cmd, with G growing from gainMin (slow) to 1 (full push).
        float c = cmd * (stun > 0 ? t.stunControl : 1.0f);
        float u = fabsf(c);
        float gain = t.gainMin + (1.0f - t.gainMin) * powf(u, t.gainExp);
        float target = t.runSpeed * c * gain * (pushing ? t.pushSpeed : pulling ? t.pullSpeed : 1.0f);   // blocks are heavy

        // Hanging from a taut tongue in the air: the stick pumps the swing, like on a
        // playground swing -- push with the motion and it grows, up to a point.
        if (tg == tgStuck && tgTaut && !onGround) {
            Vector2 n = Vector2Normalize(Vector2Subtract(centreW(), piv.back()));
            Vector2 tang = {-n.y, n.x};
            if (Vector2DotProduct(tang, toWorld({1, 0})) < 0) tang = Vector2Negate(tang);
            Vector2 vw = toWorld(vel);
            bool growing = Vector2DotProduct(vw, tang) * c > 0;
            if (!growing || Vector2Length(vw) < t.tongueMaxSwing)
                vw = Vector2Add(vw, Vector2Scale(tang, c * t.tonguePump * dt));
            vw = Vector2Scale(vw, 1 - t.tongueDamp * dt);
            vel = toLocal(vw);
            if (u > 0.05f) facing = signf(c);
            jumpLogic(dt);
            return;
        }

        // Non-linear approach: the further from the target, the harder he pushes.
        float diff = target - vel.x;
        bool turning = vel.x * target < 0;
        bool speeding = fabsf(target) > fabsf(vel.x) && !turning;
        float a;
        if (turning) a = t.turnAccel;
        else if (speeding) a = t.accelFloor + t.accelK * fabsf(diff);
        else a = t.decelFloor + t.decelK * fabsf(diff);
        if (!onGround) a *= (u > 0.01f) ? t.airControl : t.airDrag;
        else if (onSlippy()) a *= (u > 0.01f) ? t.slipGrip : t.slipDrag;   // ice: he keeps sliding
        float dv = a * dt;
        vel.x = (fabsf(diff) <= dv) ? target : vel.x + signf(diff) * dv;
        if (u > 0.05f) facing = signf(c);

        if (onGround && turning && fabsf(vel.x) > t.runSpeed * 0.4f && GetRandomValue(0, 3) == 0)
            dust({pos.x - signf(vel.x) * 10, pos.y}, 1, 40, 60);
        jumpLogic(dt);
    }

    // Jump: buffered press + a little grace after leaving an edge.
    void jumpLogic(float dt) {
        // Stuck: jump lets go of the tongue and jumps from right there, on top of his momentum.
        if (jumpBuf > 0 && tg == tgStuck && stun <= 0) {
            jumpBuf = 0;
            tongueRetract(t.tongueBack);
            vel.y -= t.jumpSpeed;
            jumping = true; onGround = false; coyote = 0; flyChew = -1;
            sound(Audio::kJump, 0.9f);
            squash = -0.28f; squashVel = 0;
            if (trace) fprintf(stderr, "t=%.2f tongue jump, speed (%.0f,%.0f)\n", time, vel.x, vel.y);
            return;
        }
        coyote = onGround ? t.coyote : coyote - dt;
        jumpBuf -= dt;
        dropTimer -= dt;
        if (jumpBuf > 0 && onGround && downHeld && t.dropThrough && standingOnOneway()) {
            dropTimer = t.dropTime;
            jumpBuf = 0;
            onGround = false;
            pos.y += 1;
        } else if (jumpBuf > 0 && coyote > 0 && stun <= 0) {
            vel.y = -t.jumpSpeed;
            jumpBuf = 0; coyote = 0; getUp = 0; flyChew = -1;
            sound(Audio::kJump);
            jumping = true; onGround = false;
            squash = -0.28f; squashVel = 0;
            dust({pos.x, pos.y}, 5, 90, 40);
        } else if (jumpBuf > 0 && !onGround && airJump && t.doubleJump && canDouble && stun <= 0) {
            // Double jump: a fresh kick from mid-air, whatever the fall speed was.
            vel.y = -t.doubleJumpSpeed;
            jumpBuf = 0; airJump = false; flyChew = -1;
            sound(Audio::kJump, 1.35f);
            jumping = true;
            squash = -0.22f; squashVel = 0;
            spin = t.doubleJumpSpin;
            puffRing({pos.x, pos.y});
        }
    }

    void afterMove(float dt) {
        stun = fmaxf(0, stun - dt);
        spin = fmaxf(0, spin - dt);
        // Moving cuts the get-up short, but smoothly: the rest plays three times faster.
        bool moving = knock == kNone && fabsf(cmd) > 0.05f;
        getUp = fmaxf(0, getUp - dt * (moving ? 3.0f : 1.0f));
        if (knock == kNone && !onGround) getUp = 0;
        if (starsT >= 0 && (starsT += dt) >= t.knockStars) starsT = -1;
        if (wakeT >= 0 && (wakeT += dt) >= t.wakeBlinkT) wakeT = -1;
        if (hatBlinkT >= 0 && (hatBlinkT += dt) >= t.hatShutT + t.hatBlinkT) hatBlinkT = -1;
        if (wasRising && getUp <= 0 && knock == kNone) wakeT = 0;   // up (or jumping out of it): the blinks
        wasRising = getUp > 0;
        dark += (darkHere() - dark) * fminf(1, dt * 4);   // (eased: no jump at a respawn or a ledge)
        landFlash = fmaxf(0, landFlash - dt * 2.5f);
        eyeBig = fmaxf(0, eyeBig - dt * 3.5f);
        // Sliding on ice: soap bubbles at his feet, more the faster he goes.
        if (onSlippy() && groundSpeed > 15 && GetRandomValue(0, 1000) < 1000 * dt * t.slipBubbles * groundSpeed / 25.0f) {
            Particle q;
            q.p = toWorld({pos.x + GetRandomValue(-14, 14), pos.y - GetRandomValue(1, 4)});
            q.v = toWorld({-vel.x * 0.15f + GetRandomValue(-10, 10), -(float)GetRandomValue(8, 22)});
            q.maxLife = q.life = GetRandomValue(40, 90) / 100.0f;
            q.r = GetRandomValue(18, 36) / 10.0f;
            q.spark = false; q.bubble = true;
            parts.push_back(q);
        }
        flyStep(dt);
        if (flyChew >= 0) {                // one, two ... three
            static const float at[3] = {0.12f, 0.30f, 0.72f};
            flyChew += dt;
            if (chews < 3 && flyChew >= at[chews]) { chomp(0.45f); chews++; }
            if (chews >= 3 && flyChew > at[2] + 0.25f) flyChew = -1;
        }
        if (mouthFly >= 0) mouthT -= dt;
        if (mouthFly >= 0 && flyChew < 0 && mouthT <= 0) coughFly();   // chewed -- or cut short -- and a lamp's fly's time in his belly over: out it comes
        if (bellyFly >= 0 && t.bellyTime > 0 && (bellyT -= dt) <= 0) spitBellyFly();   // the colour fly's time in his belly over
        if (hatT >= 0) {                                     // the hat: folded, then pulled open
            float unfoldAt = hatUnfoldAt();
            auto pull = [&](float h) { return h < unfoldAt ? -1 : (int)((h - unfoldAt) / t.hatUnfoldT * 6); };
            int before = pull(hatT);
            hatT += dt;
            int now = pull(hatT);
            if (now != before && now < 6) sound(Audio::kHat, now % 2 ? 1.6f : 1.35f, 0.4f);   // each pull wheezes
            if (hatT >= fmaxf(hatWholeAt(), hatBrimDoneAt())) hatT = -1;   // (it protects once whole: hatGuards)
        }
        float mouthTo = (tg == tgOut || tg == tgBack) ? 1.0f : (tg == tgStuck || tg == tgDead) ? 0.55f : 0.0f;
        if (hatPutT >= 0) {                              // putting his hat on: the mouth shut by the time it is on (no tilt, no jump)
            float u = clampf(hatPutT / kHatPutT, 0, 1);
            mouthTo = 1 - u * u * (3 - 2 * u);
        }
        mouth += (mouthTo - mouth) * (1 - expf(-dt * (mouthTo > mouth ? 40.0f : 22.0f)));

        // Squash spring.
        squashVel += (-260.0f * squash - 16.0f * squashVel) * dt;
        squash += squashVel * dt;

        // Lean. On the ground it is a springy body on top of the legs: it leans forward with
        // speed, but the legs' acceleration throws it the other way -- so starting to run, it
        // first rocks back as the legs carry it off, then tenses forward; braking pitches it
        // further forward, and it springs back upright with a little wobble.
        // In the air: nose up when leaping, settling while falling.
        if (endHappy() && onGround && knock == kNone) {   // the ending, happy: swaying (endSway), his feet planted
            float was = tilt;
            tilt = endSway();
            tiltVel = (tilt - was) / dt;
        } else if (onGround && knock == kNone) {
            float accel = (vel.x - stepVelX) / dt;
            float target = vel.x / t.runSpeed * t.leanRun;
            if (pulling || pushing) target += moveDir * t.pullLean;   // leaning into the drag (or the push)
            // On ice he is never quite steady: a slow sway, around upright when still, around his
            // lean when he slides or runs.
            if (onSlippy()) target += t.iceWobble * sinf(time * 2 * PI * t.iceWobbleRate);
            bool braking = accel * vel.x < 0 || (vel.x == 0 && accel * stepVelX < 0);   // slowing down
            float push = clampf(-accel * (braking ? t.leanBrake : t.leanInertia),
                                -t.leanStiffness * 14.0f, t.leanStiffness * 14.0f);
            tiltVel += (t.leanStiffness * (target - tilt) - t.leanDamping * tiltVel + push) * dt;
            tilt += tiltVel * dt;
        } else {
            float tiltTarget = -vel.x / t.runSpeed * 16.0f * clampf(-vel.y / t.jumpSpeed + 0.3f, 0, 1);
            tilt = lerpf(tilt, tiltTarget, 1 - expf(-dt * 12));
            tiltVel = 0;
        }
        // Falling -- down along gravity, nothing holding him -- for fall_pedal_after: he pedals.
        fallT = (!onGround && vel.y > 0 && knock == kNone && tg != tgStuck) ? fallT + dt : 0;
        if (traceLean) fprintf(stderr, "lean t=%.3f vel=%.0f tilt=%.2f real=%.0f tongue=%.0f\n", time, vel.x, tilt,
                               groundSpeed, tg == tgStuck ? tgLen : 0.0f);
        if (traceLean && !flies.empty()) {
            float top = 0;
            for (auto& f : flies) top = fmaxf(top, Vector2Length(f.v));
            fprintf(stderr, "flies t=%.3f fastest=%.0f\n", time, top);
        }
        if (traceLean && !crates.empty()) fprintf(stderr, "crate t=%.3f x=%.1f pulling=%d leon=%.1f y=%.1f ang=%.1f mode=%d\n", time, crates[0].c.x, pulling ? 1 : 0, toWorld(pos).x, crates[0].c.y, crates[0].ang, crates[0].mode);
        if (traceLean) fprintf(stderr, "gait t=%.3f px=%.2f gait=%.4f S=%.2f b=%.3f dir=%.0f amt=%.3f\n", time, pos.x, gait,
                               gaitStride, gaitStance, moveDir, gaitAmt);

        // The legs follow how far he REALLY went -- held back by a taut tongue, they slow down
        // with him instead of freezing (the rope keeps cancelling his speed).
        float moved = fabsf(pos.x - prevPos.x);
        groundSpeed += (moved / dt - groundSpeed) * (1 - expf(-dt * 20));
        if (onGround && knock == kNone) walkPhase += moved / 9.0f;
        // Step cycle. A foot on the ground travels back exactly as fast as the body goes
        // forward, so it stays planted: during the `stance` share of a cycle the body covers one
        // stride. Slow, the strides are short and each foot stays down longer; fast, the cadence
        // is capped and the feet may slip, as a running cartoon's do.
        if (onGround && knock == kNone) {
            float sp = moved / dt;
            float k = clampf(sp / t.runSpeed, 0, 1);
            gaitStride = lerpf(9.0f, 14.0f, k);
            gaitStance = lerpf(0.78f, 0.55f, k);
            float cycle = fmaxf(gaitStride / gaitStance, sp / 6.0f);   // distance per cycle; <= 6 steps/s
            bool coasting = onSlippy() && fabsf(cmd) < 0.05f;   // sliding on ice: the feet stay still
            float before = gait;
            if (!coasting) gait = fmodf(gait + moved / cycle, 1.0f);
            bool footDown = (gait < before) || (before < 0.5f && gait >= 0.5f);
            if (footDown && gaitAmt > 0.4f && !coasting)
                sound(Audio::kStep, 1.0f, clampf(0.35f + sp / t.runSpeed, 0.35f, 1.0f));
            if (moved / dt > 2.0f) moveDir = signf(pos.x - prevPos.x);
            gaitAmt += ((sp > 3.0f && !coasting ? 1.0f : 0.0f) - gaitAmt) * (1 - expf(-dt * 10));
        } else {
            gaitAmt += (0 - gaitAmt) * (1 - expf(-dt * 10));
        }
        if (trace && tg == tgStuck && fmodf(time, 0.5f) < dt)
            fprintf(stderr, "t=%.2f held by tongue: speed %.0f, wants %.0f, ground speed %.0f, walk phase %.1f\n",
                    time, vel.x, cmd * t.runSpeed, groundSpeed, walkPhase);
        blink -= dt;
        if (blink < -0.12f) blink = GetRandomValue(150, 500) / 100.0f;

        // Stars.
        Vector2 centre = toWorld({pos.x, pos.y - kLeonH / 2});   // stars are in the world
        for (size_t i = 0; i < level.stars.size(); i++) {
            if (taken[i]) continue;
            Vector2 s = starPos(i);
            float dx = s.x - centre.x, dy = s.y - centre.y;
            if (dx * dx + dy * dy < 30 * 30) {
                taken[i] = true; got++;
                sparks(s, false);
                if (got == (int)level.stars.size() && !level.hasExit) finished = true;
            }
        }

        partsStep(dt);
        updateCamera(dt);
        trauma = fmaxf(0, trauma - t.shakeDecay * dt);
    }

    // Particles: dust and sparks slowing, soap drifting, saliva falling.
    void partsStep(float dt) {
        for (auto& q : parts) {
            q.life -= dt;
            q.p.x += q.v.x * dt; q.p.y += q.v.y * dt;
            if (q.bubble) {                // soap drifts gently, slowing
                q.v = Vector2Scale(q.v, 1 - 1.5f * dt);
                continue;
            }
            if (q.drop) {                  // saliva falls with gravity, and is gone on impact (a Monstermunch's goes into it)
                q.v = Vector2Add(q.v, Vector2Scale(pullW, t.gravity * 0.5f * dt));
                char c = obstacleAt(q.p);
                if (c && !(q.mm && c == 'm')) q.life = 0;
                continue;
            }
            float drag = q.spark ? 4.0f : 6.0f;
            q.v.x -= q.v.x * drag * dt; q.v.y -= q.v.y * drag * dt;
            if (!q.spark) q.v.y -= 20 * dt;
        }
        parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& q) { return q.life <= 0; }),
                    parts.end());
    }

    bool standingOnOneway() const {
        if (grav != 0) return false;   // only defined the right way up
        int x0 = (int)floorf((pos.x - kLeonW / 2) / kTile), x1 = (int)floorf((pos.x + kLeonW / 2 - 0.01f) / kTile);
        int row = (int)floorf((pos.y + 0.5f) / kTile);
        bool any = false;
        for (int tx = x0; tx <= x1; tx++) {
            if (level.block(tx, row)) return false;
            any |= level.oneway(tx, row);
        }
        return any;
    }

    Vector2 starPos(size_t i) const {
        Vector2 s = level.stars[i];
        s.y += sinf(time * 2.2f + i * 1.3f) * 4.0f;
        return s;
    }

    // The camera chases Leon with a delay (exponential, separate per axis), but a hard
    // rule overrides the delay: Leon stays inside the central zone of camera_zone_area of the
    // screen -- a rectangle of the screen's proportions, scaled by sqrt(area) on each side.
    Vector2 viewSize() const {
        float vh = kLeonDrawH / t.playerFrac / t.zoom;   // world height on screen
        float vw = vh * (float)gameW() / (float)std::max(1, gameH());
        return {vw, vh};
    }

    void updateCamera(float dt) {
        Vector2 c = {pos.x, pos.y - kLeonH / 2};
        Vector2 v = viewSize();
        // Looking down aims the camera below him, so that he settles look_screen_position from
        // the top of the screen; looking up mirrors it. The right stick looks much further, as far
        // as it is pushed: all the way, he ends up look_far_margin of his size from the edge.
        // Reached with the usual camera delay.
        bool canLook = knock == kNone;
        float nearY = look * (0.5f - t.lookPos) * v.y;
        float farY = farLook.y * (v.y * 0.5f - kLeonDrawH * (0.5f + t.lookFarMargin));
        float lookAhead = canLook ? (fabsf(farY) > fabsf(nearY) ? farY : nearY) : 0;
        float lookAside = canLook ? farLook.x * (v.x * 0.5f - kLeonW * (0.5f + t.lookFarMargin)) : 0;
        if (float u = endPan(); u > 0) lookAside = lerpf(lookAside, v.x * 0.5f - t.endPanLeft * kTile, u);   // the ending: the scene slides left, Leon ending ending_pan_leon_from_left tiles from the screen's left
        lookAside += endScroll();                                                                            // ...and at last, away, on and on
        cam.x += (c.x + lookAside - cam.x) * (1 - expf(-dt / t.camDelayX));
        cam.y += (c.y + lookAhead - cam.y) * (1 - expf(-dt / t.camDelayY));
        // The zone gives way to a far look -- and comes back as smoothly as the camera does.
        farReach.x = fmaxf(fabsf(lookAside), farReach.x * expf(-dt / t.camDelayX));
        farReach.y = fmaxf(fabsf(lookAhead), farReach.y * expf(-dt / t.camDelayY));
        float k = sqrtf(t.camArea);
        float hw = fmaxf(v.x * 0.5f * k - kLeonW / 2, farReach.x), hh = fmaxf(v.y * 0.5f * k - kLeonH / 2, farReach.y);
        cam.x = clampf(cam.x, c.x - hw, c.x + hw);
        cam.y = clampf(cam.y, c.y - hh, c.y + hh);
    }

    // ---- bug reports ---------------------------------------------------------------------------
    // The state of the game as text ("key: value" lines, comments in brackets after the values),
    // enough to know where Leon is and to put the game back in that state (--from-report).
    std::string reportState(const std::string& levelFile) const {
        std::string o;
        auto bits = [](const std::vector<bool>& v) { std::string b; for (bool x : v) b += x ? '1' : '0'; return b.empty() ? std::string("-") : b; };
        static const char* knockNames[] = {"none", "air", "roll", "down", "rise"};
        static const char* tgNames[] = {"none", "out", "stuck", "back", "dead"};
        Vector2 c = centreW();
        appendf(o, "level.file: %s\n", levelFile.c_str());
        appendf(o, "level.size: %d %d   (tiles of %.0f px)\n", level.w, level.h, kTile);
        appendf(o, "time.game: %.3f\n", time);
        appendf(o, "time.run: %.3f   (the clock on screen)\n", runTime);
        appendf(o, "completed: %d   (the red key taken)\n", completed ? 1 : 0);
        appendf(o, "easy.done: %d   (its clock stopped: the yellow flag up)\n", finished ? 1 : 0);
        appendf(o, "flags.up: %d %d %d %d %d   (yellow, red, green, blue, violet)\n", flagT[flYellow] >= 0 ? 1 : 0, flagT[flRed] >= 0 ? 1 : 0,
                flagT[flGreen] >= 0 ? 1 : 0, flagT[flBlue] >= 0 ? 1 : 0, flagT[flViolet] >= 0 ? 1 : 0);
        appendf(o, "flags.touched: %d %d %d %d %d   (up or not: its mode never run)\n", flagTouched[0] ? 1 : 0, flagTouched[1] ? 1 : 0,
                flagTouched[2] ? 1 : 0, flagTouched[3] ? 1 : 0, flagTouched[4] ? 1 : 0);
        appendf(o, "leon.tile: %.2f %.2f   (world tiles, his centre; x right, y down)\n", c.x / kTile, c.y / kTile);
        appendf(o, "leon.centre: %.2f %.2f   (world pixels)\n", c.x, c.y);
        appendf(o, "leon.gravity: %d   (0 down, 1 left, 2 up, 3 right)%s\n", grav,
                turnStart >= 0 ? "  -- in the middle of a gravity turn" : "");
        appendf(o, "leon.velocity: %.2f %.2f   (his frame: x across gravity, y along it)\n", vel.x, vel.y);
        appendf(o, "leon.facing: %d\n", (int)facing);
        appendf(o, "leon.onGround: %d\n", onGround ? 1 : 0);
        appendf(o, "leon.knock: %s\n", knockNames[knock]);
        appendf(o, "leon.dead: %d\n", dead ? 1 : 0);
        appendf(o, "leon.abilities: %d %d   (double jump, tongue)\n", canDouble ? 1 : 0, canTongue ? 1 : 0);
        appendf(o, "leon.god: %d   (GOD: debug mode, Shift+G)\n", god ? 1 : 0);
        appendf(o, "player: %s   (who plays: leon, or anna -- Leon then waiting on the tower's top)\n", anna ? "anna" : "leon");
        appendf(o, "leon.colour: %d %.3f   (his own: 0 white, 1 gold -- the Solo door --, 2 pastel pink -- the Love door --; seconds into changing to it, -1 done)\n", own, ownT >= 0 && ownT < t.leonColorTime ? ownT : -1.0f);
        appendf(o, "leon.hat: %d %.3f   (wearing it; seconds since it took a bump, -1 whole)\n", hasHat ? 1 : 0, hatT);
        appendf(o, "hat.taken: %d   (from its spot in the level)\n", hatTaken ? 1 : 0);
        appendf(o, "hat.loose: %d %.2f %.2f   (knocked off his head: lying there, world pixels)\n", hatLoose ? 1 : 0, hatP.x, hatP.y);
        appendf(o, "ending: %.3f %.3f %.3f %.3f   (seconds since he touched the invisible wall on the tower's top, since he landed back there -- -1 not yet --; seconds of rolling the credits gained, down held; when the scene went off the screen, as the second, -1 not yet)\n",
                endT, endLandT, creditsGain, endOffT);
        appendf(o, "tongue.state: %s\n", tgNames[tg]);
        if (tg == tgOut || tg == tgBack)
            appendf(o, "tongue.direction: %.4f %.4f   (world)\ntongue.extent: %.2f\n", tgDir.x, tgDir.y, tgExt);
        if (tg == tgStuck) {
            appendf(o, "tongue.length: %.2f\n", tgLen);
            appendf(o, "tongue.block: %d %.2f %.2f   (block it is stuck to, -1 none; offset)\n", tgCrate, tgCrateOff.x, tgCrateOff.y);
            appendf(o, "tongue.bends: %d   (the sticking point, then the corners it bends round)\n", (int)piv.size());
            for (size_t i = 0; i < piv.size(); i++)
                appendf(o, "tongue.point.%d: %.2f %.2f %d\n", (int)i, piv[i].x, piv[i].y, pivSide[i]);
        }
        appendf(o, "checkpoint.feet: %.2f %.2f   (where he comes back, world pixels)\n", cpFeet.x, cpFeet.y);
        appendf(o, "checkpoint.gravity: %d\n", cpGrav);
        appendf(o, "checkpoints.lit: %s   (in map order -- the pineapples, then the flags' and the keys')\n", bits(pineEaten).c_str());
        appendf(o, "checkpoints.gone: %s   (hard mode: taken away)\n", bits(pineGone).c_str());
        std::string pg;
        for (int gv : pineGrav) pg += gv < 0 ? "- " : std::to_string(gv) + " ";
        appendf(o, "checkpoints.gravity: %s  (each one's first touch, in map order; - never)\n", pg.empty() ? "-" : pg.c_str());
        appendf(o, "checkpoint.pine: %d   (the one he comes back to, its number in map order; -1 the start)\n", cpPine);
        appendf(o, "hard.mode: %d %d %.3f %d   (on -- the red key --, back home -- the red flag up --, its clock, next waypoint)\n", hardMode ? 1 : 0,
                hardDone ? 1 : 0, hardTime, hardNext);
        appendf(o, "dark.mode: %d %d %.3f   (on -- the green key --, done -- the green flag --, its clock)\n", darkMode ? 1 : 0,
                darkDone ? 1 : 0, darkTime);
        appendf(o, "harder.mode: %d %d %.3f   (on -- the blue key --, done -- the blue flag --, its clock)\n",
                harderMode ? 1 : 0, harderDone ? 1 : 0, harderTime);
        appendf(o, "cameleon.mode: %d %d %.3f   (on -- the purple key --, done -- the violet flag --, its clock)\n", camMode ? 1 : 0,
                camDone ? 1 : 0, camTime);
        appendf(o, "golden.run: %d %d %.3f   (on -- after a dark light --, done -- the black flag reached --, its clock)\n", goldenRun ? 1 : 0,
                goldenDone ? 1 : 0, goldenTime);
        appendf(o, "flag.black: %d %d   (touched -- its highscores showing --, up -- the golden run done by it)\n", blackFlagTouched ? 1 : 0,
                blackFlagT >= 0 ? 1 : 0);
        appendf(o, "key.held: %d %d %d %d %d   (red -- the exit's, taken, or start_with_key's --, blue, green, purple and golden, for doors)\n",
                hasKey ? 1 : 0, hasBlueKey ? 1 : 0, hasGreenKey ? 1 : 0, hasPurpleKey ? 1 : 0, hasGoldKey ? 1 : 0);
        if (!blueKeyTaken.empty()) appendf(o, "keys.blue.taken: %s\n", bits(blueKeyTaken).c_str());
        if (!greenKeyTaken.empty()) appendf(o, "keys.green.taken: %s\n", bits(greenKeyTaken).c_str());
        if (!purpleKeyTaken.empty()) appendf(o, "keys.purple.taken: %s\n", bits(purpleKeyTaken).c_str());
        if (!goldKeyTaken.empty()) appendf(o, "keys.gold.taken: %s\n", bits(goldKeyTaken).c_str());
        appendf(o, "key.last: %d   (the last key taken: 0 red, 1 blue, 2 green, 3 violet, 4 golden, -1 none -- the hat's bands its colour)\n", lastKey);
        if (!doorT.empty()) {
            std::string ds;
            for (float d : doorT) ds += TextFormat("%.3f ", d);
            appendf(o, "doors: %s  (each door, in map order: seconds since the key went in, -1 locked)\n", ds.c_str());
        }
        appendf(o, "fruits.eaten: %s\n", bits(fruitEaten).c_str());
        appendf(o, "cherries.eaten: %s\n", bits(cherryEaten).c_str());
        appendf(o, "stars.taken: %s\n", bits(taken).c_str());
        appendf(o, "blocks: %d\n", (int)crates.size());
        for (size_t i = 0; i < crates.size(); i++)
            appendf(o, "block.%d: %.2f %.2f %.2f %.2f %.2f %d %d %d   (centre x y, velocity x y, angle, awake, mode, touched)\n", (int)i,
                    crates[i].c.x, crates[i].c.y, crates[i].v.x, crates[i].v.y, crates[i].ang, crates[i].awake ? 1 : 0, crates[i].mode,
                    crates[i].touched ? 1 : 0);
        int golden = 0;
        std::string lampS;
        for (const Fly& f : flies) {
            golden += f.golden && f.lamp < 0 ? 1 : 0;
            if (f.lamp >= 0) lampS += TextFormat("%.2f@%d ", f.lampT, f.lamp);
        }
        appendf(o, "flies.golden: %d   (following Leon)\n", golden);
        if (!lampS.empty())
            appendf(o, "lamps: %s  (each lamp's fly: seconds left round Leon, -1 at its post; @ its lamp, in map order)\n", lampS.c_str());
        if (!level.poles.empty())
            appendf(o, "belly: %d %.2f   (a colour fly in his belly: its pole, in map order -- -1 none --, seconds left in there)\n",
                    bellyFly >= 0 ? flies[bellyFly].pole : -1, bellyT);
        Vector2 cw = toWorld(cam);
        appendf(o, "camera: %.2f %.2f   (world pixels, the screen's centre)\n", cw.x, cw.y);
        return o;
    }

    // Put the game back in a reported state (what reportState wrote). Leon comes back alive and
    // not knocked out; the flies are left where they are.
    void applyReport(const std::map<std::string, std::string>& m) {
        auto get = [&](const std::string& k) -> const char* { auto it = m.find(k); return it == m.end() ? nullptr : it->second.c_str(); };
        auto bits = [&](const char* k, std::vector<bool>& out) {
            const char* v = get(k);
            if (!v) return;
            for (size_t i = 0; v[i] == '0' || v[i] == '1'; i++) if (i < out.size()) out[i] = v[i] == '1';
        };
        int a = 0, b = 0;
        float x = 0, y = 0;
        if (const char* v = get("leon.abilities")) if (sscanf(v, "%d %d", &a, &b) == 2) { canDouble = a; canTongue = b; }
        if (const char* v = get("leon.god")) god = atoi(v) != 0;
        { const char* v = get("player"); anna = v && !strncmp(v, "anna", 4); }   // (an older report: none -- Leon)
        if (const char* v = get("leon.love")) if (atoi(v) != 0) own = 2;   // (an older report's: pink or not)
        if (const char* v = get("leon.colour")) { float ct = -1; if (sscanf(v, "%d %f", &own, &ct) == 2) ownT = ct; }   // (changing: from white)
        if (const char* v = get("leon.hat")) {
            float ht = -1;
            if (sscanf(v, "%d %f", &a, &ht) >= 1) { hasHat = a != 0; hatT = ht; }
        }
        if (const char* v = get("hat.taken")) hatTaken = atoi(v) != 0;
        if (const char* v = get("ending")) {
            hearts.clear(); heartDue = 0; creditsK = 1; creditsGain = 0; endOffT = -1;   // (an older report: no gain, not off yet)
            sscanf(v, "%f %f %f %f", &endT, &endLandT, &creditsGain, &endOffT);
        }
        if (const char* v = get("hat.loose")) {
            int on = 0;
            if (sscanf(v, "%d %f %f", &on, &x, &y) == 3 && on) { hatLoose = true; hatP = {x, y}; hatV = {0, 0}; hatW = 0; hatA = 0; }
        }
        bits("checkpoints.lit", pineEaten);
        bits("checkpoints.gone", pineGone);
        if (const char* v = get("checkpoints.gravity")) {    // "0 3 - 1 ..." (older reports: none)
            for (size_t i = 0; i < pineGrav.size() && *v && *v != '('; i++) {
                while (*v == ' ') v++;
                if (*v == '-') { pineGrav[i] = -1; v++; }
                else if (*v >= '0' && *v <= '3') { pineGrav[i] = *v - '0'; v++; }
                else break;
            }
        }
        if (const char* v = get("hard.mode")) {
            int on = 0, home = 0, nx = 0;
            float ht = 0;
            if (sscanf(v, "%d %d %f %d", &on, &home, &ht, &nx) == 4) {
                hardMode = on != 0; hardDone = home != 0; hardTime = ht; hardNext = std::max(0, nx);
                if (hardMode) hardModePines();
            }
        }
        bits("fruits.eaten", fruitEaten);
        bits("cherries.eaten", cherryEaten);
        bits("stars.taken", taken);
        got = 0;
        for (bool t2 : taken) got += t2 ? 1 : 0;
        if (const char* v = get("checkpoint.feet")) if (sscanf(v, "%f %f", &x, &y) == 2) cpFeet = {x, y};
        if (const char* v = get("checkpoint.gravity")) cpGrav = atoi(v);
        cpPine = -1;
        if (const char* v = get("checkpoint.pine")) cpPine = std::clamp(atoi(v), -1, (int)level.pines.size() - 1);
        else {                                        // (an older report: the one under his checkpoint)
            int n = nearestPine(cpFeet);
            if (n >= 0 && Vector2Distance(level.pines[n], cpFeet) < 1.5f * kTile) cpPine = n;
        }
        if (const char* v = get("time.run")) runTime = (float)atof(v);
        if (const char* v = get("time.game")) time = (float)atof(v);
        if (const char* v = get("completed")) completed = finished = atoi(v) != 0;
        if (const char* v = get("easy.done")) finished = atoi(v) != 0;   // (an older report: done at the red key)
        if (const char* v = get("dark.mode")) {
            int on = 0, dn = 0;
            float tm = 0;
            if (sscanf(v, "%d %d %f", &on, &dn, &tm) == 3) { darkMode = on != 0; darkDone = dn != 0; darkTime = tm; }
        }
        if (const char* v = get("harder.mode")) {
            int on = 0, dn = 0;
            float tm = 0;
            if (sscanf(v, "%d %d %f", &on, &dn, &tm) == 3) { harderMode = on != 0; harderDone = dn != 0; harderTime = tm; }
            if (const char* cv = get("cameleon.mode")) {         // (on done time -- older: on time)
                if (sscanf(cv, "%d %d %f", &on, &dn, &tm) == 3) { camMode = on != 0; camDone = dn != 0; camTime = tm; }
                else if (sscanf(cv, "%d %f", &on, &tm) == 2) { camMode = on != 0; camTime = tm; }
            }
        } else if (const char* v = get("cameleon.mode")) {     // (an older report: "cameleon" was the blue flag's, now harder mode)
            int on = 0;
            float tm = 0;
            if (sscanf(v, "%d %f", &on, &tm) == 2) { harderMode = on != 0; harderTime = tm; }
        }
        if (const char* v = get("golden.run")) {
            int on = 0, dn = 0;
            float tm = 0;
            if (sscanf(v, "%d %d %f", &on, &dn, &tm) == 3) { goldenRun = on != 0; goldenDone = dn != 0; goldenTime = tm; }
        }
        blackFlagTouched = false; blackFlagT = goldenDone && level.blackFlag.has ? 10.0f : -1;   // (an older report: up if the run was done)
        if (const char* v = get("flag.black")) {
            int tc = 0, up = 0;
            if (sscanf(v, "%d %d", &tc, &up) == 2 && level.blackFlag.has) { blackFlagTouched = tc != 0; blackFlagT = up ? 10.0f : -1; }
        }
        if (const char* v = get("flags.up")) {               // the flags up, flying
            int up[flCount] = {};
            sscanf(v, "%d %d %d %d %d", &up[0], &up[1], &up[2], &up[3], &up[4]);
            for (int f = 0; f < flCount; f++) flagT[f] = up[f] && level.flags[f].has ? 10.0f : -1;
        } else {                                             // (an older report: each mode's end, its flag)
            bool up[flCount] = {finished && completed, hardDone || darkMode, darkDone, harderDone, camMode};
            for (int f = 0; f < flCount; f++) flagT[f] = up[f] && level.flags[f].has ? 10.0f : -1;
        }
        for (int f = 0; f < flCount; f++) flagTouched[f] = flagT[f] >= 0;
        if (const char* v = get("flags.touched")) {          // (older reports: those up)
            int tc[flCount] = {};
            if (sscanf(v, "%d %d %d %d %d", &tc[0], &tc[1], &tc[2], &tc[3], &tc[4]) == flCount)
                for (int f = 0; f < flCount; f++) flagTouched[f] = tc[f] && level.flags[f].has;
        }
        musicPart = camDone ? 3 : flagTouched[flBlue] ? 2 : darkMode || flagTouched[flRed] ? 1 : 0;
        if (const char* v = get("doors")) {                   // the key in a door: gold, or open
            char* e = (char*)v;
            for (size_t i = 0; i < doorT.size(); i++) {
                char* s = e;
                float d = strtof(s, &e);
                if (e == s) break;
                doorT[i] = d < 0 ? -1 : d;
                if (doorT[i] >= t.doorGold) openDoor(i);
            }
        }
        bits("keys.blue.taken", blueKeyTaken);
        bits("keys.green.taken", greenKeyTaken);
        bits("keys.purple.taken", purpleKeyTaken);
        bits("keys.gold.taken", goldKeyTaken);
        {   // the last key taken -- an older report's: the latest of them along the level (red, green, blue, purple, golden)
            auto any = [](const std::vector<bool>& v) { return std::find(v.begin(), v.end(), true) != v.end(); };
            lastKey = any(goldKeyTaken) ? 4 : any(purpleKeyTaken) ? 3 : any(blueKeyTaken) ? 1 : any(greenKeyTaken) ? 2 : completed ? 0 : -1;
            if (const char* v = get("key.last")) lastKey = std::clamp(atoi(v), -1, 4);
        }
        goldMusic = goldTaken() ? 3 : -1;                         // (the music after a golden key: the golden door opened last -- else none)
        for (size_t i = 0, last = 0; i < doorT.size(); i++)
            if (level.doorChar[i] == 'O' && doorT[i] >= t.doorGold && (goldMusic < 4 || doorT[i] < doorT[last])) { goldMusic = goldDoorMusic(i); last = i; }
        if (const char* v = get("key.held")) {
            int r = 0, b = 0, gk = 0, pk = 0, ok = 0;
            int n = sscanf(v, "%d %d %d %d %d", &r, &b, &gk, &pk, &ok);
            hasKey = r != 0;
            hasBlueKey = n >= 2 && b != 0;
            hasGreenKey = n >= 3 && gk != 0;
            hasPurpleKey = n >= 4 && pk != 0;
            hasGoldKey = n >= 5 && ok != 0;
        } else {                                                // (an older report: the exit's key, not yet in a door)
            hasKey = completed;
            for (float d : doorT) if (d >= 0) hasKey = false;
        }
        for (size_t i = 0; i < crates.size(); i++) {
            const char* v = get("block." + std::to_string(i));
            float cx, cy, vx, vy, an;
            int aw, md, tc = 0;
            int n = v ? sscanf(v, "%f %f %f %f %f %d %d %d", &cx, &cy, &vx, &vy, &an, &aw, &md, &tc) : 0;
            if (n >= 7) {
                crates[i].c = {cx, cy}; crates[i].v = {vx, vy}; crates[i].ang = an;
                crates[i].awake = aw != 0; crates[i].mode = 0; crates[i].visOff = {0, 0};
                // (a report older than `touched`: a block away from where it was placed had been)
                crates[i].touched = n >= 8 ? tc != 0 : Vector2Distance(crates[i].c, crates[i].home) > 0.5f;
            }
        }
        // Leon: his centre stays where it was; the frame is that of his gravity then.
        if (const char* v = get("leon.centre")) if (sscanf(v, "%f %f", &x, &y) == 2) {
            int gr = 0;
            if (const char* gv = get("leon.gravity")) gr = atoi(gv);
            grav = 0; turnFrom = turnDelta = 0; turnStart = -1;
            pos = prevPos = {x, y + kLeonH / 2};
            cam = prevCam = {x, y};
            if (gr > 0) forceGravity(gr);
            fallTop = pos.y;
        }
        if (const char* v = get("flies.golden")) {          // his golden flies, round him again
            int n = atoi(v);
            for (Fly& f : flies)
                if (n > 0 && !f.golden && f.pole < 0) { f.golden = true; f.caught = f.inMouth = false; f.p = centreW(); f.v = {0, 0}; n--; }
        }
        if (const char* v = get("belly")) {                  // the colour fly in his belly: in there again
            int k = -1;
            float bt = 0;
            if (sscanf(v, "%d %f", &k, &bt) == 2)
                for (size_t i = 0; i < flies.size(); i++)
                    if (k >= 0 && flies[i].pole == k) { flies[i].inMouth = true; flies[i].caught = false; bellyFly = (int)i; bellyT = bt; }
        }
        if (const char* v = get("lamps")) {                   // each lamp's fly: round him still, or at its post
            char* e = (char*)v;
            for (Fly& f : flies) {
                if (f.lamp < 0) continue;
                char* s = e;
                float lt = strtof(s, &e);
                if (e == s) break;
                if (*e == '@') {                              // (its lamp; an older report has none)
                    int k = (int)strtol(e + 1, &e, 10);
                    if (k >= 0 && k < (int)level.lamps.size()) f.lamp = k;
                }
                f.lampT = lt < 0 ? -1 : lt;
                f.p = f.lampT >= 0 ? centreW() : lampTop(f.lamp);
            }
        }
        if (const char* v = get("leon.velocity")) if (sscanf(v, "%f %f", &x, &y) == 2) vel = {x, y};
        if (const char* v = get("camera")) if (sscanf(v, "%f %f", &x, &y) == 2) cam = prevCam = toLocal({x, y});   // same framing
        if (const char* v = get("leon.facing")) facing = atoi(v) < 0 ? -1.0f : 1.0f;
        // A stuck tongue comes back stuck, bends and all.
        const char* ts = get("tongue.state");
        if (ts && !strncmp(ts, "stuck", 5)) {
            int nb = 0;
            if (const char* v = get("tongue.bends")) nb = atoi(v);
            piv.clear(); pivSide.clear();
            for (int i = 0; i < nb; i++) {
                const char* v = get("tongue.point." + std::to_string(i));
                int sd = 0;
                if (v && sscanf(v, "%f %f %d", &x, &y, &sd) >= 2) { piv.push_back({x, y}); pivSide.push_back(sd); }
            }
            if (!piv.empty()) {
                tg = tgStuck;
                if (const char* v = get("tongue.length")) tgLen = (float)atof(v);
                tgCrate = -1;
                if (const char* v = get("tongue.block")) {
                    int ci = -1;
                    if (sscanf(v, "%d %f %f", &ci, &x, &y) == 3 && ci >= 0 && ci < (int)crates.size()) { tgCrate = ci; tgCrateOff = {x, y}; }
                }
                resetRope();
                prevCentreW = centreW();
                // Let the slack tongue drape before play starts (it was stored as points, not as
                // a hanging rope): a second of its own physics, Leon and everything else still.
                pullW = toWorld({0, 1});                                // his gravity, not the default
                for (int k = 0; k < 120; k++) ropeVisual(1.0f / 120.0f, ropeRemaining());
            }
        }
        dark = darkHere();                                    // where he is put back: as dark at once
    }
};

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------
// raylib's DrawLineEx, to the last bit (raylib 6.0, as compiled in its library): the corners of its
// band -- its squared length one fused multiply-add, its half width thick / (len + len) -- p0, p1
// round a, p2, p3 round b; DrawTriangleStrip makes them the triangles (p2 p0 p1) and (p3 p2 p1).
// False: nothing to draw (no thickness, no length), as DrawLineEx.
static bool lineCorners(Vector2 a, Vector2 b, float thick, Vector2 p[4]) {
    if (!(thick > 0)) return false;
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(fmaf(dx, dx, dy * dy));
    if (!(len > 0)) return false;
    float scale = thick / (len + len), rx = -(scale * dy), ry = dx * scale;
    p[0] = {a.x - rx, a.y - ry}; p[1] = {a.x + rx, a.y + ry};
    p[2] = {b.x - rx, b.y - ry}; p[3] = {b.x + rx, b.y + ry};
    return true;
}
// A thick ink line: DrawLineEx's very triangles (lineCorners), put to raylib's batch as one of the
// quads its circles, rectangles and fans are drawn with -- the quad (p2 p0 p1 p3), which it cuts into
// (p2 p0 p1) and (p2 p1 p3): DrawLineEx's own two. DrawLineEx's triangles cut the batch each time
// they meet those (one draw call more: hundreds a frame round a Monstermunch's ink, line, cap, line).
// Its texture is the shapes' -- a white texel in the default font's atlas --, read at that texel's
// middle at every corner: white, as DrawLineEx's own texture is. (Spread over the texel from corner
// to corner, as the shapes do it, a line thinner than a pixel had its edge pixels read the font's
// texels beside it: a shade off, zoomed far out.)
static void inkLine(Vector2 a, Vector2 b, float thick, Color col) {
    Vector2 p[4];
    if (!lineCorners(a, b, thick, p)) return;
    const Texture2D tex = GetShapesTexture();
    const Rectangle sr = GetShapesTextureRectangle();
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlTexCoord2f((sr.x + sr.width / 2) / tex.width, (sr.y + sr.height / 2) / tex.height);
    rlVertex2f(p[2].x, p[2].y);
    rlVertex2f(p[0].x, p[0].y);
    rlVertex2f(p[1].x, p[1].y);
    rlVertex2f(p[3].x, p[3].y);
    rlEnd();
    rlSetTexture(0);
}

static unsigned hash2(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// The sketch's far layer: a pattern, repeated (background_pattern). Karakusa -- the Japanese
// scrolling vine: in a tile (its width 1, y down), a stem waving from edge to edge, and off it
// tendrils curling tighter and tighter (Euler spirals): a big scroll above, one below, and smaller
// curls the other side. Its rows 0.6 of a tile apart, every other one half a tile along: the scrolls
// of one row in among the next's.
static const std::vector<std::vector<Vector2>>& karakusaMotif() {
    static std::vector<std::vector<Vector2>> lines;
    if (!lines.empty()) return lines;
    const float A = 0.09f;                               // (the stem's wave)
    auto stemY = [&](float x) { return 0.5f + A * sinf(2 * PI * x); };
    auto tendril = [&](float x0, float side, float len, float k0, float k1, int n) {   // (side: -1 up, +1 down)
        Vector2 p = {x0, stemY(x0)};
        float th = atan2f(A * 2 * PI * cosf(2 * PI * x0), 1.0f), ds = len / n;   // (leaving the stem along it)
        std::vector<Vector2> pts{p};
        for (int i = 0; i < n; i++) {
            th += side * (k0 + k1 * i * ds) * ds;        // (its bend tightening as it goes)
            p.x += cosf(th) * ds; p.y += sinf(th) * ds;
            pts.push_back(p);
        }
        return pts;
    };
    std::vector<Vector2> stem;
    for (int i = 0; i <= 24; i++) stem.push_back({i / 24.0f, stemY(i / 24.0f)});
    lines.push_back(stem);
    lines.push_back(tendril(0.02f, -1, 1.00f, 3.4f, 9, 44));     // the big scrolls
    lines.push_back(tendril(0.52f, +1, 1.00f, 3.4f, 9, 44));
    lines.push_back(tendril(0.30f, +1, 0.40f, 6.0f, 40, 22));    // the small curls
    lines.push_back(tendril(0.80f, -1, 0.40f, 6.0f, 40, 22));
    return lines;
}

// The far layer's patterns, by name (background_pattern_*): each a tile -- `base` world pixels wide at
// background_pattern_scale 1, `H` of that tall, every other row `stagger` of a tile along -- its
// lines (and fills) in tile units (x 0..1, y 0..H). Drawn tile by tile: what a tile's motif puts
// past its edges is its own, the next tile's motif not drawing it again.
struct BgPattern {
    float base = 160, H = 1, stagger = 0;
    std::vector<std::vector<Vector2>> lines, fills;
};
static const BgPattern& bgPattern(int id) {
    static BgPattern pats[kBgCount];
    static bool built = false;
    if (!built) {
        built = true;
        {   // karakusa: the scrolling vine (karakusaMotif), its rows 0.6 apart, every other one half along
            BgPattern& p = pats[1];
            p.base = 160; p.H = 0.6f; p.stagger = 0.5f; p.lines = karakusaMotif();
        }
        {   // yagasuri: arrow feathers -- two columns, the second half a feather lower; in each, its sides,
            // its shaft, and the feathers stacked: a chevron and two barbs under it, pointing down
            BgPattern& p = pats[2];
            p.base = 80;
            for (int c = 0; c < 2; c++) {
                float x0 = c * 0.5f, off = 0.25f * c;
                p.lines.push_back({{x0, 0}, {x0, 1}});
                p.lines.push_back({{x0 + 0.25f, 0}, {x0 + 0.25f, 1}});
                for (int j = 0; j < 2; j++)
                    for (int b = 0; b < 3; b++) {
                        float y = j * 0.5f + off + b * 0.08f, in = b * 0.06f;
                        p.lines.push_back({{x0 + in, y + in * 0.7f}, {x0 + 0.25f, y + 0.18f}, {x0 + 0.5f - in, y + in * 0.7f}});
                    }
            }
        }
        {   // asanoha: the hemp leaf -- the triangle lattice (side 1), each triangle's sides and the
            // three lines from its middle to its corners
            BgPattern& p = pats[3];
            const float h = sqrtf(3.0f) / 2;
            p.base = 60; p.H = 2 * h;
            for (int j = -1; j <= 2; j++)
                for (int i = -2; i <= 2; i++) {
                    Vector2 a = {i + ((j & 1) ? 0.5f : 0.0f), j * h}, b = {a.x + 1, a.y}, c = {a.x + 0.5f, a.y + h}, d = {a.x + 1.5f, a.y + h};
                    const Vector2 up[3] = {a, b, c}, down[3] = {b, d, c};
                    for (const Vector2* t : {up, down}) {
                        Vector2 m = {(t[0].x + t[1].x + t[2].x) / 3, (t[0].y + t[1].y + t[2].y) / 3};
                        if (m.x < 0 || m.x >= 1 || m.y < 0 || m.y >= p.H) continue;
                        for (int k = 0; k < 3; k++) {
                            p.lines.push_back({t[k], t[(k + 1) % 3]});
                            p.lines.push_back({m, t[k]});
                        }
                    }
                }
        }
        {   // seigaiha: the blue sea's waves -- fans of four concentric arcs (radius 1/2), rows a quarter
            // apart, every other one half along: each fan above its middle, hidden where the next row's
            // two fans lie over it
            BgPattern& p = pats[4];
            p.base = 80; p.H = 0.5f;
            const Vector2 fan[2] = {{0, 0}, {0.5f, 0.25f}};
            for (Vector2 c : fan)
                for (float r : {0.5f, 0.375f, 0.25f, 0.125f}) {
                    std::vector<Vector2> run;
                    for (int k = 0; k <= 64; k++) {
                        float th = PI + PI * k / 64;
                        Vector2 q = {c.x + r * cosf(th), c.y + r * sinf(th)};
                        bool hid = false;
                        for (float dx : {-0.5f, 0.5f})
                            if (Vector2Distance(q, {c.x + dx, c.y + 0.25f}) < 0.5f - 1e-4f && q.y <= c.y + 0.25f) hid = true;
                        if (hid) { if (run.size() > 1) p.lines.push_back(run); run.clear(); }
                        else run.push_back(q);
                    }
                    if (run.size() > 1) p.lines.push_back(run);
                }
        }
        {   // sayagata: interlocked manji -- on the square lattice (3, 1), (-1, 3) of a tenth of the tile,
            // each manji's arms 2 out then 1 turned a quarter, into its neighbours' arms
            BgPattern& p = pats[5];
            p.base = 150;
            const int a = 3, b = 1, n = a * a + b * b;
            const float L1 = 2, L2 = 1;
            for (int i = -n; i < 2 * n; i++)
                for (int j = -n; j < 2 * n; j++) {
                    int cx = i * a - j * b, cy = i * b + j * a;
                    if (cx < 0 || cx >= n || cy < 0 || cy >= n) continue;
                    for (int d = 0; d < 4; d++) {
                        const Vector2 dir[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
                        Vector2 u = dir[d], tu = {-u.y, u.x}, p1 = {cx + L1 * u.x, cy + L1 * u.y};
                        p.lines.push_back({{(float)cx / n, (float)cy / n}, {p1.x / n, p1.y / n}, {(p1.x + L2 * tu.x) / n, (p1.y + L2 * tu.y) / n}});
                    }
                }
        }
        {   // ichimatsu: the checkerboard -- two squares of four filled
            BgPattern& p = pats[6];
            p.base = 80;
            p.fills = {{{0, 0}, {0.5f, 0}, {0.5f, 0.5f}, {0, 0.5f}}, {{0.5f, 0.5f}, {1, 0.5f}, {1, 1}, {0.5f, 1}}};
        }
    }
    return pats[std::clamp(id, 0, kBgCount - 1)];
}
// The far layer's change of pattern (main): from gBgFrom to gBgTo, gBgFade of the way (1: done).
static int gBgFrom = 0, gBgTo = 0;
static float gBgFade = 1;
static Rectangle gSeen{};                     // the world that can show on the screen this frame: the box round its corners (main)
// The far layer's pattern, put out in one go. Stroke by stroke through raylib's batch it cost most
// of a frame (seigaiha: ~200 000 strokes over the culling square, each six vertices passed one by
// one). Here their triangles -- the very ones DrawLineEx and DrawTriangleFan make, worked out the
// same way to the last bit (lineCorners) -- go into a vertex buffer of their own, in the order the
// batch would have had them, and are drawn with raylib's own shader, under the matrix its batch has
// just set, a run per colour. Only the tiles that can show: on the screen (gSeen), and not under the
// ground -- its slabs, solid, are drawn over them anyway (drawLevel). Built again only when what they
// were built from changes, a piece at a time: the buffer, on the GPU, the only one to hold them all.
// The buffer keeps to a budget, kBgBudget -- 16 MiB, some four times what hard mode's seigaiha takes
// at the default size, a crossfade's two patterns and a turned screen in it. Past it (a pattern made
// tiny, the view zoomed far out: background_pattern_scale, view_zoom), the buffer let go, the pattern
// is drawn stroke by stroke as before it (drawBgPattern); the same when no memory is to be had for it. Counts and sizes in 64 bits, checked before they go to rlgl's
// ints. (An OpenGL without vertex arrays: stroke by stroke, as ever.)
struct BgRun { int first, count; Color col; };
struct BgPass { const BgPattern* p = nullptr; float T = 0, lw = 0; Color col{}, fill{}; std::vector<Vector2> bases; };   // a pattern's tiles to put out
static const int64_t kBgVertexBytes = 3 * (int64_t)sizeof(float);   // (x, y, z)
static const int64_t kBgBudget = (int64_t)16 << 20;                 // (bytes)
static const int64_t kBgBudgetVertices = kBgBudget / kBgVertexBytes;
static const int64_t kBgSlack = 65536;                              // (vertices: a buffer over twice its need by more than this, made smaller)
static bool gBgStrokesOnly = false;                                 // --bg-fallback: stroke by stroke, always (tests)
static bool gBgProbe = false;                                       // --bg-probe: each build's full size told, past the budget too (tests)
static struct {
    unsigned vao = 0, vbo = 0;
    int64_t cap = 0;                          // (vertices the buffer holds)
    int64_t peak = 0;                         // (the most bytes it has held)
    bool failed = false;                      // (no vertex arrays: stroke by stroke)
    bool strokes = false;                     // (the pattern as built last: past the budget, or no buffer to be had)
    bool overflow = false;                    // (more sent than counted: never -- but then, not drawn from)
    float piece[3 * 8190];                    // (x, y, z a vertex: a piece of them on its way to the buffer, whole triangles)
    int64_t inPiece = 0, sent = 0;            // (vertices in the piece; sent before it)
    std::vector<BgRun> runs;                  // the triangles' runs, each its colour, in drawing order
    std::vector<float> key, built;            // what this frame's would be built from; what the buffer's were
    std::vector<Rectangle> slabs;             // the ground drawn over them this frame (drawLevel's, cut to the view as there)
    BgPass pass[2];
} gBgMesh;
static void bgSend() {                        // (the piece to the buffer, after what was sent)
    auto& M = gBgMesh;
    if (M.inPiece > 0) {
        if (M.sent + M.inPiece > M.cap) M.overflow = true;
        else rlUpdateVertexBuffer(M.vbo, M.piece, (int)(M.inPiece * kBgVertexBytes), (int)(M.sent * kBgVertexBytes));   // (within the budget: ints)
    }
    M.sent += M.inPiece; M.inPiece = 0;
}
static void bgTriangle(Vector2 a, Vector2 b, Vector2 c, Color col) {
    auto& M = gBgMesh;
    if (M.inPiece + 3 > (int64_t)(sizeof M.piece / sizeof(float)) / 3) bgSend();
    const int64_t at = M.sent + M.inPiece;
    if (M.runs.empty() || memcmp(&M.runs.back().col, &col, sizeof col) != 0) M.runs.push_back({(int)at, 0, col});
    for (Vector2 v : {a, b, c}) {                            // (z: halfway in the depth range)
        float* q = M.piece + 3 * M.inPiece++;
        q[0] = v.x; q[1] = v.y; q[2] = -0.5f;
    }
    M.runs.back().count += 3;
}
// A pattern's motif: its extent in a tile, its points' (x0 y0 x1 y1, in tile widths), and the
// vertices a tile of it makes (a fill's triangles, a stroke's two).
static int64_t bgMotif(const BgPattern& p, float b[4]) {
    b[0] = b[1] = 1e9f; b[2] = b[3] = -1e9f;
    int64_t per = 0;
    for (const auto* set : {&p.fills, &p.lines})
        for (const auto& ln : *set) {
            const int64_t n = (int64_t)ln.size();
            per += set == &p.fills ? 3 * std::max<int64_t>(0, n - 2) : 6 * std::max<int64_t>(0, n - 1);
            for (Vector2 v : ln) { b[0] = fminf(b[0], v.x); b[1] = fminf(b[1], v.y); b[2] = fmaxf(b[2], v.x); b[3] = fmaxf(b[3], v.y); }
        }
    return per;
}
// One pattern, `amount` of its full ink: its tiles that can show, as drawBgPattern goes through them
// -- those whose motif (with its strokes' width, and some) reaches gSeen and is not wholly under a
// slab -- kept while their vertices stay within `room`. Returns how many vertices they make, at most:
// past `room`, more than it, the tiles let go (probing, all of them counted).
static int64_t bgPatternTiles(const Game& g, int id, Rectangle view, float amount, BgPass& P, int64_t room) {
    P.bases.clear();
    if (id <= 0 || amount <= 0) return 0;
    const BgPattern& p = bgPattern(id);
    const Color ink = {38, 38, 48, 255};
    P.p = &p;
    P.col = ColorLerp(kPaper, ink, amount); P.fill = ColorLerp(kPaper, ink, 0.5f * amount);
    const float T = p.base * g.t.bgScale, row = p.H * T;
    Vector2 c = {view.x + view.width / 2, view.y + view.height / 2};
    Vector2 o = Vector2Scale(c, 1 - g.t.bgParallax);        // (its origin, drifting with the camera)
    const float lw = fmaxf(1.0f, 2.0f * fminf(1.0f, g.t.bgScale));
    P.T = T; P.lw = lw;
    float bb[4];
    const int64_t per = bgMotif(p, bb);
    const float m = lw + 2;                                  // (a stroke's half width, and some)
    const Rectangle& S = gSeen;
    int64_t n = 0;
    int r0 = (int)floorf((view.y - o.y) / row) - 2, r1 = (int)ceilf((view.y + view.height - o.y) / row) + 1;
    for (int r = r0; r <= r1; r++) {
        float sx = (r & 1) ? p.stagger : 0.0f;
        int k0 = (int)floorf((view.x - o.x) / T - sx) - 1, k1 = (int)ceilf((view.x + view.width - o.x) / T - sx) + 1;
        for (int k = k0; k <= k1; k++) {
            Vector2 base = {o.x + (k + sx) * T, o.y + r * row};
            const float x0 = base.x + bb[0] * T - m, x1 = base.x + bb[2] * T + m, y0 = base.y + bb[1] * T - m, y1 = base.y + bb[3] * T + m;
            if (x1 < S.x || x0 > S.x + S.width || y1 < S.y || y0 > S.y + S.height) continue;   // (off the screen)
            bool under = false;                                                              // (under the ground)
            for (const Rectangle& sl : gBgMesh.slabs)
                if (sl.x <= x0 && x1 <= sl.x + sl.width && sl.y <= y0 && y1 <= sl.y + sl.height) { under = true; break; }
            if (under) continue;
            n += per;
            if (n <= room) P.bases.push_back(base);
            else {                                                                           // (past the budget)
                P.bases.clear();
                if (!gBgProbe) return n;
            }
        }
    }
    return n;
}
// ...and their triangles, as drawBgPattern draws them, tile for tile: the fills (DrawTriangleFan:
// q0 qi qi+1), then the strokes (DrawLineEx: p2 p0 p1, p3 p2 p1).
static void bgPatternAdd(const BgPass& P) {
    const float T = P.T;
    for (Vector2 base : P.bases) {
        for (const auto& f : P.p->fills) {
            Vector2 q0 = {base.x + f[0].x * T, base.y + f[0].y * T};
            for (size_t i = 1; i + 1 < f.size(); i++)
                bgTriangle(q0, {base.x + f[i].x * T, base.y + f[i].y * T}, {base.x + f[i + 1].x * T, base.y + f[i + 1].y * T}, P.fill);
        }
        for (const auto& ln : P.p->lines)
            for (size_t i = 0; i + 1 < ln.size(); i++) {
                Vector2 q[4];
                if (!lineCorners({base.x + ln[i].x * T, base.y + ln[i].y * T}, {base.x + ln[i + 1].x * T, base.y + ln[i + 1].y * T}, P.lw, q)) continue;
                bgTriangle(q[2], q[0], q[1], P.col);
                bgTriangle(q[3], q[2], q[1], P.col);
            }
    }
}
// What a pattern's triangles and colours are built from: the same as last time, nothing to build again.
static void bgPatternKey(const Game& g, int id, Rectangle view, float amount) {
    if (id <= 0 || amount <= 0) return;
    for (float v : {(float)id, amount, g.t.bgScale, g.t.bgParallax, view.x, view.y, view.width, view.height, gSeen.x, gSeen.y, gSeen.width, gSeen.height})
        gBgMesh.key.push_back(v);
}
// Both patterns (a crossfade's) built into the buffer: their tiles found, room made, their triangles
// sent. 1: built; 0: past the budget -- stroke by stroke, till what they are built from changes; -1:
// no buffer to be had -- stroke by stroke, and tried again next frame.
static int bgMeshBuild(const Game& g, Rectangle view, int from, float aFrom, int to, float aTo) {
    auto& M = gBgMesh;
    if (!M.vao && !(M.vao = rlLoadVertexArray())) { M.failed = true; return -1; }
    M.runs.clear();
    int64_t n = 0;
    bool over = false;
    try {
        M.slabs.clear();
        for (const Rectangle& r : g.level.solidRects)        // (drawLevel's slabs, as it cuts them, that can show)
            if (CheckCollisionRecs(r, view) && CheckCollisionRecs(r, gSeen)) M.slabs.push_back(GetCollisionRec(r, view));
        n = bgPatternTiles(g, from, view, aFrom, M.pass[0], kBgBudgetVertices);
        if (n <= kBgBudgetVertices || gBgProbe) n += bgPatternTiles(g, to, view, aTo, M.pass[1], std::max<int64_t>(0, kBgBudgetVertices - n));
        over = n > kBgBudgetVertices;
    } catch (const std::bad_alloc&) {                        // (no memory for its tiles: as past the budget)
        M.pass[0].bases.clear(); M.pass[1].bases.clear();
        over = true;
    }
    if (gBgProbe)
        TraceLog(LOG_INFO, "BACKGROUND: probe: %lld vertices -- %lld bytes as sized before the budget (with a quarter more), %s",
                 (long long)n, (long long)((n + n / 4) * kBgVertexBytes), over ? "past the budget: stroke by stroke" : "in the buffer");
    if (over) {
        if (M.vbo) { rlUnloadVertexBuffer(M.vbo); M.vbo = 0; M.cap = 0; }   // (not used meanwhile: let go)
        if (!M.strokes) TraceLog(LOG_INFO, "BACKGROUND: the pattern past its %lld MiB budget -- drawn stroke by stroke", (long long)(kBgBudget >> 20));
        return 0;
    }
    const int64_t want = std::min(n + n / 4, kBgBudgetVertices);   // (a quarter more: room to move)
    if (n > M.cap || M.cap > 2 * want + kBgSlack) {          // (too small -- or far too big, back from a far zoom)
        if (M.vbo) { rlUnloadVertexBuffer(M.vbo); M.vbo = 0; M.cap = 0; }
        if (want > 0) {
            const int64_t bytes = want * kBgVertexBytes;     // (within the budget: an int)
            rlEnableVertexArray(M.vao);
            M.vbo = rlLoadVertexBuffer(nullptr, (int)bytes, true);
            if (M.vbo) {
                rlSetVertexAttribute(rlGetShaderLocsDefault()[RL_SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, false, 0, 0);
                rlEnableVertexAttribute(rlGetShaderLocsDefault()[RL_SHADER_LOC_VERTEX_POSITION]);
            }
            rlDisableVertexArray();
            if (!M.vbo) return -1;
            M.cap = want; M.peak = std::max(M.peak, bytes);
            TraceLog(LOG_INFO, "BACKGROUND: pattern buffer %.2f MiB (%lld vertices; %lld wanted)", bytes / 1048576.0, (long long)want, (long long)n);
        }
    }
    if (M.strokes) TraceLog(LOG_INFO, "BACKGROUND: the pattern back within its budget -- in its buffer");
    rlEnableVertexArray(M.vao);
    M.inPiece = M.sent = 0; M.overflow = false;
    for (const BgPass& P : M.pass) bgPatternAdd(P);
    bgSend();
    rlDisableVertexArray();
    if (M.overflow) { M.runs.clear(); return -1; }
    return 1;
}
// The buffer drawn: raylib's batch first draws what it holds -- a triangle of no size queued, so that
// it does, setting its shader's matrix (modelview times projection, its own arithmetic) and its
// colDiffuse (white) -- then its shader, as it left them, draws the runs, each its colour.
static void bgMeshDraw() {
    auto& M = gBgMesh;
    if (M.runs.empty()) return;
    const int* locs = rlGetShaderLocsDefault();
    rlBegin(RL_TRIANGLES);
    rlColor4ub(0, 0, 0, 0);
    for (int k = 0; k < 3; k++) rlVertex2f(0, 0);
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableShader(rlGetShaderIdDefault());
    rlActiveTextureSlot(0);
    rlEnableTexture(rlGetTextureIdDefault());
    rlEnableVertexArray(M.vao);
    for (const BgRun& r : M.runs) {
        const float col[4] = {r.col.r / 255.0f, r.col.g / 255.0f, r.col.b / 255.0f, r.col.a / 255.0f};
        rlSetVertexAttributeDefault(locs[RL_SHADER_LOC_VERTEX_COLOR], col, RL_SHADER_ATTRIB_VEC4, 4);   // (the colour attribute left off: this, for all)
        rlDrawVertexArray(r.first, r.count);
    }
    rlDisableVertexArray();
    rlDisableTexture();
    rlDisableShader();
}
// One pattern, `amount` of its full ink (background_pattern_opacity, and its share of a crossfade):
// the ink as it shows on the paper, solid (see-through, its lines' joins would show as darker dots).
// (Stroke by stroke through raylib's batch, as before the buffer -- when the vertex buffer cannot be
// had, or the pattern would pass its budget. Kept as it was, every tile of the view: leaving out
// those off the screen, nothing else changed, moved a sample of the picture by a shade, once.)
static void drawBgPattern(const Game& g, int id, Rectangle view, float amount) {
    if (id <= 0 || amount <= 0) return;
    const BgPattern& p = bgPattern(id);
    const Color ink = {38, 38, 48, 255};
    Color col = ColorLerp(kPaper, ink, amount), fill = ColorLerp(kPaper, ink, 0.5f * amount);
    const float T = p.base * g.t.bgScale, row = p.H * T;
    Vector2 c = {view.x + view.width / 2, view.y + view.height / 2};
    Vector2 o = Vector2Scale(c, 1 - g.t.bgParallax);        // (its origin, drifting with the camera)
    const float lw = fmaxf(1.0f, 2.0f * fminf(1.0f, g.t.bgScale));
    int r0 = (int)floorf((view.y - o.y) / row) - 2, r1 = (int)ceilf((view.y + view.height - o.y) / row) + 1;
    for (int r = r0; r <= r1; r++) {
        float sx = (r & 1) ? p.stagger : 0.0f;
        int k0 = (int)floorf((view.x - o.x) / T - sx) - 1, k1 = (int)ceilf((view.x + view.width - o.x) / T - sx) + 1;
        for (int k = k0; k <= k1; k++) {
            Vector2 base = {o.x + (k + sx) * T, o.y + r * row};
            for (const auto& f : p.fills) {
                std::vector<Vector2> q;
                for (Vector2 v : f) q.push_back({base.x + v.x * T, base.y + v.y * T});
                DrawTriangleFan(q.data(), (int)q.size(), fill);
            }
            for (const auto& ln : p.lines)
                for (size_t i = 0; i + 1 < ln.size(); i++)
                    DrawLineEx({base.x + ln[i].x * T, base.y + ln[i].y * T}, {base.x + ln[i + 1].x * T, base.y + ln[i + 1].y * T}, lw, col);
        }
    }
}

static void drawBackground(const Game& g, Rectangle view) {
    if (g.engrave) return;   // (the engraving's sky: drawn separately)
    // The far layer, under the graph paper: the pattern of the mode he is in (background_pattern_*),
    // sliding with background_pattern_parallax of the world's moves -- a change of mode crossfading
    // the old one out and the new one in, over background_pattern_fade seconds.
    const float aFrom = g.t.bgOpacity * (1 - gBgFade), aTo = g.t.bgOpacity * gBgFade;
    auto& M = gBgMesh;
    if (!M.failed && !gBgStrokesOnly) {
        M.key.clear();
        bgPatternKey(g, gBgFrom, view, aFrom);
        bgPatternKey(g, gBgTo, view, aTo);
        M.key.push_back((float)g.level.generation);          // (a map loaded since -- startup, a switch: its ground may lie elsewhere)
        if (M.key != M.built) {                              // (built again: something moved, or changed colour)
            const int built = bgMeshBuild(g, view, gBgFrom, aFrom, gBgTo, aTo);
            M.strokes = built <= 0;
            if (built < 0) M.built.clear(); else M.built = M.key;
        }
    }
    if (!M.failed && !gBgStrokesOnly && !M.strokes) bgMeshDraw();
    else {
        drawBgPattern(g, gBgFrom, view, aFrom);
        drawBgPattern(g, gBgTo, view, aTo);
    }
    // Graph paper, locked to the world.
    float s = kTile;
    int x0 = (int)floorf(view.x / s), x1 = (int)ceilf((view.x + view.width) / s);
    int y0 = (int)floorf(view.y / s), y1 = (int)ceilf((view.y + view.height) / s);
    for (int x = x0; x <= x1; x++)
        inkLine({x * s, view.y}, {x * s, view.y + view.height}, (x % 5 == 0) ? 1.6f : 1.0f,
                (x % 5 == 0) ? kGridMajor : kGrid);
    for (int y = y0; y <= y1; y++)
        inkLine({view.x, y * s}, {view.x + view.width, y * s}, (y % 5 == 0) ? 1.6f : 1.0f,
                (y % 5 == 0) ? kGridMajor : kGrid);

}

// ---- the engraving (look = engraving): steampunk, after Gustave Doré ---------------------------
// The sky and its clouds are drawn as finished line art -- pure ink on paper, which the engraving
// shader leaves as it is -- so their strokes drift with the clouds; everything else is drawn in
// half-tones, which the shader cuts into lines fixed on the world.

// The sky: fine horizontal strokes, closer and heavier towards the top, the paper bare low down
// -- on the screen as it is (W x H), whatever the gravity.
static void engraveSky(int W, int H) {
    ClearBackground(kEngPaper);
    float ui = H / 720.0f;
    for (float y = 1; y < H * 0.8f;) {
        float k = y / (H * 0.8f);
        inkLine({0, y}, {(float)W, y}, fmaxf(0.6f, (1.25f - 1.1f * k) * ui), kEngInk);
        y += lerpf(3.0f, 8.0f, k * k) * ui;
    }
}

// An engraver's stroke: a polyline whose line swells in its middle and tapers away at both ends,
// as a burin cuts it -- and the same along an elliptic arc (angles in radians: 0 right, pi/2 down).
static void burin(const Vector2* p, int n, float thick, Color col) {
    for (int i = 0; i + 1 < n; i++) {
        float t = (i + 0.5f) / (n - 1);
        inkLine(p[i], p[i + 1], thick * (0.2f + 0.8f * sinf(PI * t)), col);
    }
}
static void burinArc(Vector2 c, float rx, float ry, float a0, float a1, float thick, Color col) {
    const int n = 14;
    Vector2 p[n + 1];
    for (int i = 0; i <= n; i++) {
        float a = a0 + (a1 - a0) * i / n;
        p[i] = {c.x + rx * cosf(a), c.y + ry * sinf(a)};
    }
    burin(p, n + 1, thick, col);
}

// A cloud after the Dutch engravers of the 16th century: a bank of billows on a flat base, left
// white where the light falls (from above left) -- the tops under a fine contour, each billow
// modelled by a few curved strokes following its round on the shadowed side, the underside dark
// with long level strokes. `o`: the middle of its base, on the screen; `ink`: the weight of its
// lines (lighter far away).
static void dutchCloud(Vector2 o, float W, unsigned h, float ink) {
    struct Lobe { float x, y, rx, ry; };
    int n = 4 + h % 3;
    Lobe lobes[6];
    for (int k = 0; k < n; k++) {
        unsigned q = hash2((int)(h % 100003) + k * 31, 7717);
        float u = (k + 0.5f) / n;
        float rx = W / n * (0.62f + (q % 30) / 100.0f);
        float lift = powf(sinf(PI * u), 0.8f) * W * 0.16f;               // the middle billows the highest
        lobes[k] = {o.x - W / 2 + u * W + ((int)((q >> 8) % 11) - 5) * W / 200, o.y - lift - rx * 0.35f,
                    rx, rx * (0.72f + (q >> 16) % 20 / 100.0f)};
    }
    std::sort(lobes, lobes + n, [](const Lobe& a, const Lobe& b) { return a.y < b.y; });   // the high ones behind
    DrawEllipse((int)o.x, (int)(o.y - W * 0.05f), W * 0.56f, W * 0.07f, kEngPaper);       // the base: a long flat lens
    for (const Lobe& L : lobes) {
        DrawEllipse((int)L.x, (int)L.y, L.rx, L.ry, kEngPaper);
        burinArc({L.x, L.y}, L.rx, L.ry, PI * 1.08f, PI * 1.92f, 1.2f * ink, kEngInk);    // the lit top, finely
        for (int s = 1; s <= 3; s++) {                                    // its round, in the shade
            float f = 1 - s * 0.17f;
            burinArc({L.x + L.rx * (1 - f) * 0.4f, L.y + L.ry * (1 - f) * 0.5f}, L.rx * f, L.ry * f,
                     -0.15f + s * 0.1f, PI * 0.85f, (0.8f + 0.25f * s) * ink, kEngInk);
        }
    }
    for (int i = 0; i < 6; i++) {                                         // the underside, level and dark
        float y = o.y - i * 3.0f * ink, half = W * (0.5f - i * 0.055f);
        const int m = 10;
        Vector2 p[m + 1];
        for (int k = 0; k <= m; k++) {
            float u = (float)k / m;
            p[k] = {o.x + W * 0.03f - half + 2 * half * u, y - sinf(PI * u) * W * 0.012f};
        }
        burin(p, m + 1, (1.7f - 0.2f * i) * ink, kEngInk);
    }
}

// Three layers of clouds, far to near, like the flats of a diorama: each slides with its own share
// of the camera's motion on the screen (parallax) and its own wind -- `scroll`, kept by the main
// loop in screen pixels -- so whatever the gravity, they never jump. Drawn on the screen (W x H).
static const float kCloudPar[3] = {0.03f, 0.07f, 0.13f};
static const float kCloudWind[3] = {3.0f, 6.0f, 10.0f};   // screen pixels a second, on a 720-line screen
static void engraveClouds(int W, int H, const Vector2 scroll[3]) {
    float ui = H / 720.0f;
    const float rowY[3] = {0.15f, 0.28f, 0.44f}, cellW[3] = {300, 470, 650}, size[3] = {200, 330, 480};
    const float ink[3] = {0.7f, 0.9f, 1.15f};
    for (int k = 0; k < 3; k++) {
        float cell = cellW[k] * ui, ox = -scroll[k].x, oy = -scroll[k].y;
        int i0 = (int)floorf(-ox / cell) - 2, i1 = (int)ceilf((W - ox) / cell) + 1;
        for (int i = i0; i <= i1; i++) {
            unsigned h = hash2(i, 9100 + k * 37);
            if (h % 4 == 0) continue;                                     // gaps of open sky
            float w = size[k] * ui * (0.7f + (h >> 8) % 60 / 100.0f);
            Vector2 o = {ox + i * cell + (h >> 4) % (int)(cell * 0.4f), H * rowY[k] + oy + ((int)((h >> 14) % 40) - 20) * ui};
            dutchCloud(o, w, h, ink[k] * ui);
        }
    }
}

// The cloud drawn by hand (workshop/nuages-2.jpg: the cumulus), redrawn as an engraving (assets/clouds,
// made by tools/make_clouds.py), in the three layers of engraveClouds -- where a cloud of one of the two
// other drawings once was (the billowing one, the streaked one: taken out), open sky -- mirrored or
// not, set on its base. Written as it is: the alpha under it cleared, as inColour does (the engraving
// leaves it).
static Texture2D gCloudTex;
static bool gCloudTried = false, gCloudOk = false;
static bool loadCloudDrawings() {
    if (gCloudTried) return gCloudOk;
    gCloudTried = true;
    const char* name = "assets/clouds/cloud-cumulus.png";
    std::string data;
    char buf[65536];
    if (FILE* fi = fopen(name, "rb")) {                   // (read as the game reads its files: baked in too, by make dist)
        for (size_t n; (n = fread(buf, 1, sizeof buf, fi)) > 0;) data.append(buf, n);
        fclose(fi);
    }
    Image im = data.empty() ? Image{} : LoadImageFromMemory(".png", (const unsigned char*)data.data(), (int)data.size());
    if (!im.data) { TraceLog(LOG_WARNING, "ENGRAVING: cannot load %s -- the former clouds instead", name); return false; }
    gCloudTex = LoadTextureFromImage(im);
    UnloadImage(im);
    GenTextureMipmaps(&gCloudTex);                        // (drawn small: its fine lines kept as a tone, not a shimmer)
    SetTextureFilter(gCloudTex, TEXTURE_FILTER_TRILINEAR);
    return gCloudOk = true;
}
static void drawCloudDrawings(int W, int H, const Vector2 scroll[3]) {
    float ui = H / 720.0f;
    const float rowY[3] = {0.18f, 0.32f, 0.5f}, cellW[3] = {340, 520, 720}, size[3] = {210, 340, 500};
    const Texture2D& t = gCloudTex;
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (int k = 0; k < 3; k++) {
        float cell = cellW[k] * ui, ox = -scroll[k].x, oy = -scroll[k].y;
        int i0 = (int)floorf(-ox / cell) - 2, i1 = (int)ceilf((W - ox) / cell) + 1;
        for (int i = i0; i <= i1; i++) {
            unsigned h = hash2(i, 9100 + k * 37);
            if (h % 4 == 0 || (h >> 20) % 3 != 2) continue;  // gaps of open sky -- and where the other drawings were
            float w = size[k] * ui * (0.7f + (h >> 8) % 60 / 100.0f), hh = w * t.height / t.width;
            Vector2 o = {ox + i * cell + (h >> 4) % (int)(cell * 0.4f), H * rowY[k] + oy + ((int)((h >> 14) % 40) - 20) * ui};
            if (o.x + w < 0 || o.x - w > W) continue;
            float sw = (h >> 23) & 1 ? -(float)t.width : (float)t.width;   // (mirrored, half of them)
            DrawTexturePro(t, {0, 0, sw, (float)t.height}, {o.x - w / 2, o.y - hh, w, hh}, {0, 0}, 0, WHITE);
        }
    }
    EndBlendMode();
}

// The ground: riveted iron plates two tiles square, lit from above left -- top and left faces bright
// where they meet the air, under and right faces in shadow -- seams between the plates and a row of
// rivets along each.
static void engraveSolid(const Level& L, int x, int y, float X, float Y) {
    bool lighter = ((x >> 1) + (y >> 1)) & 1;
    DrawRectangleRec({X, Y, kTile, kTile}, lighter ? kEngPlate2 : kEngPlate);
    if (!L.solid(x, y + 1)) DrawRectangleRec({X, Y + kTile - 8, kTile, 8}, kEngShade);
    if (!L.solid(x + 1, y)) DrawRectangleRec({X + kTile - 6, Y, 6, kTile}, kEngShade2);
    if (!L.solid(x, y - 1)) DrawRectangleRec({X, Y, kTile, 6}, kEngLit);
    if (!L.solid(x - 1, y)) DrawRectangleRec({X, Y, 5, kTile}, kEngLit);
    if ((x & 1) == 0 && L.solid(x - 1, y)) inkLine({X, Y}, {X, Y + kTile}, 1.6f, kEngInk);
    if ((y & 1) == 0 && L.solid(x, y - 1)) inkLine({X, Y}, {X + kTile, Y}, 1.6f, kEngInk);
    float ex = (x & 1) ? X + kTile - 6 : X + 6, ey = (y & 1) ? Y + kTile - 6 : Y + 6;   // along the plate's edges
    for (int k = 0; k < 3; k++) {
        float t = 7 + k * 13.0f;
        const Vector2 at[2] = {{ex, Y + t}, {X + t, ey}};
        for (const Vector2& r : at) {                   // (octagons: at this size, as round as circles, far cheaper)
            DrawPoly(r, 8, 2.3f, 0, kEngInk);
            DrawPoly({r.x - 0.8f, r.y - 0.8f}, 6, 0.9f, 0, kEngPaper);
        }
    }
}

// A thin platform: a riveted iron girder -- a dark beam, a light zigzag truss inside.
static void engraveGirder(const Level& L, int x, int y, float X, float Y) {
    const float sh = 12.0f;
    DrawRectangleRec({X, Y, kTile, sh}, kEngShade);
    for (int k = 0; k < 4; k++) {
        float x0 = X + k * 10;
        inkLine({x0, Y + sh - 2.5f}, {x0 + 5, Y + 2.5f}, 1.8f, kEngLit);
        inkLine({x0 + 5, Y + 2.5f}, {x0 + 10, Y + sh - 2.5f}, 1.8f, kEngLit);
    }
    inkLine({X, Y + 1}, {X + kTile, Y + 1}, 2.2f, kEngInk);              // the flanges
    inkLine({X, Y + sh - 1}, {X + kTile, Y + sh - 1}, 2.2f, kEngInk);
    for (int side = 0; side < 2; side++) {
        if (L.oneway(x + (side ? 1 : -1), y)) continue;                     // its ends, riveted
        float ex = side ? X + kTile - 1.1f : X + 1.1f;
        inkLine({ex, Y}, {ex, Y + sh}, 2.2f, kEngInk);
        DrawCircleV({side ? ex - 4 : ex + 4, Y + sh / 2}, 1.8f, kEngInk);
    }
}

// The engraving's sky, after the 19th-century wood engravings (the "Cloud" plate): the paper left
// bare but round each cloud, where the open sky is a few thin level lines, broken here and there,
// each stopping at its own point; the billows' heads white in the light under a fine contour,
// broken, heavier on their shadowed side; the body in the shade, cut in slanting lines twice as
// close, swelling with the shade, its billows told apart by white cuts, its strokes thinning and
// stopping one by one towards its edge. Lit from the upper left, as the iron plates. Cut by a
// shader of its own from shapes laid out on the screen (layoutClouds), each layer's lines sliding
// with it. Its pixels leave with alpha 0: the engraving shader keeps them as they are.
static const int kSkyClouds = 24, kSkyShapes = 160;
#ifdef __EMSCRIPTEN__
#define GRAVITY_FRAGMENT_INPUTS "#version 300 es\nprecision highp float;\nprecision highp int;\nin mediump vec2 fragTexCoord;\nin mediump vec4 fragColor;\n"
#else
#define GRAVITY_FRAGMENT_INPUTS "#version 330\nin vec2 fragTexCoord;\nin vec4 fragColor;\n"
#endif
static const char* kSkyFS = GRAVITY_FRAGMENT_INPUTS R"(
uniform vec2 screen;
uniform float spacing;            // between the lines in the shade; the open sky's are twice as far apart
uniform vec2 light;               // towards the light, on the screen
uniform vec2 frames[3];           // how far each layer has slid: its lines go with it
uniform int count;                // clouds
)"
#ifdef __EMSCRIPTEN__
R"(layout(std140) uniform SkyLayoutData {
    vec4 clouds[96];
    vec4 shapes[320];
};
)"
#else
R"(uniform vec4 clouds[96];          // per cloud: its box; (first shape, shapes, layer, strength); its halo of
                                  // open sky (x, y, rx, ry); its body's lines (slant, about x, 0, 0)
uniform vec4 shapes[320];         // per shape: (x, y, rx, ry), then (kind: 0 billow / 1 body, shade, phase, scallops)
)"
#endif
R"(out vec4 finalColor;

float hash(vec2 q) {              // on whole numbers: as steady however far the clouds have slid
    uvec2 u = uvec2(ivec2(floor(q))) * uvec2(1597334673u, 3812015801u);
    return float(((u.x ^ u.y) * 1597334673u) >> 8) / 16777216.0;
}

void main() {
    vec2 p = fragTexCoord * screen;
    float s = spacing;
    vec2 l2 = normalize(light);
    vec3 L = normalize(vec3(light, 0.75));
    float halo = 0.0;                     // the open sky round the clouds: the tone of its lines...
    int haloLayer = 0;                    // ...and whose they are
    float tone = 0.0, cover = 0.0;        // the clouds' own tone (0 paper, 1 ink), and how much of the sky they hide
    int lay = 0;                          // the lines in front: whose, and their slant (about x = about)
    float slant = 0.0, about = 0.0;
    float black = 0.0, white = 0.0;       // the billows' contours: an ink line in the light, a white cut in the shade
    for (int c = 0; c < count; c++) {
        vec4 box = clouds[4 * c];
        if (p.x < box.x || p.y < box.y || p.x > box.z || p.y > box.w) continue;
        vec4 info = clouds[4 * c + 1], hb = clouds[4 * c + 2], bl = clouds[4 * c + 3];
        int layer = int(info.z);
        float str = info.w;
        vec2 fr = frames[layer];
        float m = floor((p.y - fr.y) / (2.0 * s) + 0.25);           // its open sky: every other line,
        float eh = length((p - hb.xy) / hb.zw)                        // each stopping at its own point
                 + 0.2 * (hash(vec2(m, 17.0)) - 0.5) + 0.06 * sin((p.x - fr.x) * 0.02 + m * 2.1);
        float th = str * 0.15 * (1.0 - 0.4 * smoothstep(0.35, 0.95, eh)) * step(eh, 0.97);
        if (th > halo) { halo = th; haloLayer = layer; }
        int first = int(info.x), n = int(info.y);
        for (int i = first; i < first + n; i++) {
            vec4 g = shapes[2 * i], k = shapes[2 * i + 1];
            vec2 d = (p - g.xy) / g.zw;
            float e = length(d);
            if (k.x > 0.5) {                      // the body: the shade, darkest low and away from the light,
                if (e > 1.12) continue;           // its strokes thinning towards its edge, each stopping where it will
                float j = floor((p.y - fr.y - bl.x * (p.x - bl.y)) / s + 0.5);
                float er = e + 0.2 * (hash(vec2(j, 29.0)) - 0.5) + 0.05 * sin((p.x - fr.x) * 0.06 + j * 2.3);
                float w = 1.0 - smoothstep(0.86, 0.9, er);
                if (w <= 0.0) continue;
                float tb = clamp(0.6 + 0.12 * d.x - 0.2 * d.y, 0.25, 0.72) * (1.0 - 0.75 * smoothstep(0.5, 0.9, er));
                tone = mix(tone, tb * str, w);
                cover += (1.0 - cover) * w;
                black *= 1.0 - w;
                white *= 1.0 - w;
                if (w > 0.5) { lay = layer; slant = bl.x; about = bl.y; }
            } else {                              // a billow: a cauliflower head
                if (e > 1.16) continue;
                float a = atan(d.y, d.x);
                float rim = 1.0 + 0.08 * (abs(sin(0.5 * k.w * a + k.z)) - 0.64) + 0.05 * sin(2.0 * a + k.z)
                          + 0.04 * sin(3.0 * a + 2.0 * k.z);
                float sd = (e - rim) * g.z;       // pixels from its edge (inside: below 0)
                float cv = 1.0 - smoothstep(-0.6, 0.6, sd);
                if (cv <= 0.0) continue;
                float e2 = min(e / rim, 1.0);
                float lit = dot(vec3(d / rim, sqrt(1.0 - e2 * e2)), L);
                bool shade = k.y > 0.5;           // in the body's shade (or a head in the light: white)
                float tb = shade ? str * k.y * mix(0.42, 0.7, smoothstep(0.55, -0.1, lit)) : 0.0;
                float facing = dot(d / max(e, 1e-4), l2);              // its edge: 1 towards the light
                float lw = s * (shade ? 0.32 : 0.2 + 0.24 * clamp(-facing, 0.0, 1.0)) * (0.55 + 0.45 * str);
                float arc = a * g.z / (3.0 * s) + k.z * 5.0;            // broken here and there, most in the light
                float gap = step(fract(arc), 0.3) * step(hash(vec2(floor(arc), k.z * 97.0)), 0.18 + 0.4 * clamp(facing, 0.0, 1.0));
                float band = (smoothstep(-lw - 0.6, -lw + 0.6, sd) - smoothstep(-0.6, 0.6, sd)) * (1.0 - gap);
                // A head's contour shows against the sky and the shade; against another white head, only
                // a stroke here and there on its dark side. One in the shade is told by a white cut on its lit side.
                float onWhite = cover * (1.0 - smoothstep(0.04, 0.15, tone));
                float stroke = step(hash(vec2(floor(arc), k.z * 53.0)), 0.45) * step(0.3, -facing);
                float ink = shade ? 0.0 : band * mix(1.0, stroke, onWhite);
                float cut = shade ? band * smoothstep(-0.2, 0.4, facing) : 0.0;
                tone = mix(tone, tb, cv);
                cover += (1.0 - cover) * cv;
                black = black * (1.0 - cv) + ink;
                white = white * (1.0 - cv) + cut;
                if (cv > 0.5) { lay = layer; slant = shade ? bl.x : 0.0; about = bl.y; }
            }
        }
    }
    float t = clamp(tone + (1.0 - cover) * halo, 0.0, 1.0);
    if (cover < 0.5) { lay = haloLayer; slant = 0.0; }
    vec2 fr = frames[lay];
    float xl = p.x - fr.x;
    float r = (p.y - fr.y - slant * (p.x - about)) / s;
    float j0 = floor(r + 0.5);
    r += 0.07 * sin(xl * 0.041 + j0 * 1.7) + 0.16 * (hash(vec2(j0, 41.0)) - 0.5);   // a hand's waver, its spacing uneven
    float j = floor(r + 0.5);
    float dist = abs(r - j) * s;                                     // pixels from the line's middle
    float w;
    if (mod(j, 2.0) < 0.5) w = t < 0.2 ? 2.0 * t : max(0.4, t);       // the open sky's lines...
    else {                                                           // ...and, in the shade, as many between them
        float t2 = t + 0.14 * (hash(vec2(j, floor(xl / (6.0 * s)))) - 0.5);
        w = t2 < 0.2 ? 0.0 : (t2 < 0.4 ? 2.0 * (t2 - 0.2) : t2);
    }
    float f = xl / (8.0 * s), h0 = hash(vec2(j, floor(f) + 500.0)), h1 = hash(vec2(j, floor(f) + 501.0));
    w *= s * (0.8 + 0.4 * mix(h0, h1, smoothstep(0.0, 1.0, fract(f))));   // swelling and thinning as they were cut
    float len = s * (5.0 + 5.0 * hash(vec2(j, 7.0)));
    float u = xl / len + hash(vec2(j, 3.0));
    if (fract(u) * len < 0.8 * s && hash(vec2(j, floor(u) + 900.0)) < 0.1 + 0.7 * (1.0 - smoothstep(0.06, 0.4, t)))
        w = 0.0;                                                     // broken here and there, the light ones most
    float ink = w >= 1.0 ? clamp(0.5 + 0.5 * w - dist, 0.0, 1.0) : w * clamp(1.0 - dist, 0.0, 1.0);
    ink = max(ink, black) * (1.0 - clamp(white, 0.0, 1.0));
    finalColor = vec4(mix(vec3(0.955, 0.942, 0.905), vec3(0.075, 0.068, 0.062), clamp(ink, 0.0, 1.0)), 0.0);
}
)";

// The clouds of the three layers (far to near, as in engraveClouds), for the sky shader. A cloud,
// in shares of its width w from the middle of its base: a long body in the shade, low; over it a
// mound of billows -- their heads in the light, highest left of the middle, a few lower ones on the
// right in the shade, often one low on the left in the light -- the high ones behind; round it all,
// its halo of open sky. Its body's lines slant down to the left, as the light comes.
struct SkyLayout {
    int clouds, shapes;
    float cl[kSkyClouds * 16];     // per cloud: box; (first shape, shapes, layer, strength); halo; (slant, about x, 0, 0)
    float sh[kSkyShapes * 8];      // per shape: (x, y, rx, ry); (kind: 0 billow / 1 body, shade, phase, scallops)
};
static void layoutClouds(int W, int H, const Vector2 scroll[3], SkyLayout& L) {
    float ui = H / 720.0f;
    const float rowY[3] = {0.18f, 0.32f, 0.5f}, cellW[3] = {340, 520, 720}, size[3] = {210, 340, 500};
    const float strength[3] = {0.55f, 0.78f, 1.0f};
    L.clouds = L.shapes = 0;
    for (int k = 0; k < 3; k++) {
        float cell = cellW[k] * ui, ox = -scroll[k].x, oy = -scroll[k].y;
        int i0 = (int)floorf(-ox / cell) - 2, i1 = (int)ceilf((W - ox) / cell) + 1;
        for (int i = i0; i <= i1; i++) {
            unsigned h = hash2(i, 9100 + k * 37);
            if (h % 4 == 0) continue;                                     // gaps of open sky
            float w = size[k] * ui * (0.7f + (h >> 8) % 60 / 100.0f);
            Vector2 o = {ox + i * cell + (h >> 4) % (int)(cell * 0.4f), H * rowY[k] + oy + ((int)((h >> 14) % 40) - 20) * ui};
            if (o.x + w < -40 || o.x - w > W + 40) continue;
            if (L.clouds == kSkyClouds || L.shapes + 10 > kSkyShapes) return;
            auto rnd = [&](int s) { return hash2((int)(h % 100003) + s * 31, 7717) % 1000 / 1000.0f; };
            struct Billow { float x, y, r, shade; };
            Billow b[9];
            int nb = 0, nTop = 3 + h % 2, nShade = 2 + (h >> 3) % 2;
            for (int t = 0; t < nTop; t++) {                  // the heads overlap: one white mass
                float u = (t + 0.25f + 0.5f * rnd(t)) / nTop;
                b[nb++] = {-0.3f + 0.44f * u, -0.17f - 0.13f * sinf(PI * u) * (1.1f - 0.25f * u) - 0.02f * rnd(t + 10),
                           (0.11f + 0.06f * sinf(PI * u)) * (0.88f + 0.24f * rnd(t + 20)), 0};
            }
            for (int t = 0; t < nShade; t++) {
                float v = (t + 0.5f) / nShade;
                b[nb++] = {0.16f + 0.24f * v + 0.03f * rnd(t + 30), -0.12f + 0.06f * v + 0.02f * rnd(t + 40),
                           (0.1f - 0.025f * v) * (0.88f + 0.24f * rnd(t + 50)), 0.85f + 0.15f * rnd(t + 60)};
            }
            if ((h >> 5) % 3) b[nb++] = {-0.36f + 0.05f * rnd(70), -0.1f, 0.09f * (0.9f + 0.2f * rnd(71)), 0};
            std::sort(b, b + nb, [](const Billow& p, const Billow& q) { return p.y < q.y; });
            Vector2 bc = {o.x + 0.05f * w, o.y - 0.05f * w}, br = {0.5f * w, 0.18f * w};     // the body
            Vector2 hc = {o.x - 0.02f * w, o.y - 0.2f * w}, hr = {0.6f * w, 0.4f * w};       // the halo
            float box[4] = {hc.x - 1.12f * hr.x, hc.y - 1.12f * hr.y, hc.x + 1.12f * hr.x, hc.y + 1.12f * hr.y};
            auto put = [&](float x, float y, float rx, float ry, float kind, float shade, float phase, float scallops) {
                float* q = L.sh + 8 * L.shapes++;
                q[0] = x; q[1] = y; q[2] = rx; q[3] = ry; q[4] = kind; q[5] = shade; q[6] = phase; q[7] = scallops;
                box[0] = fminf(box[0], x - 1.16f * rx); box[1] = fminf(box[1], y - 1.16f * ry);
                box[2] = fmaxf(box[2], x + 1.16f * rx); box[3] = fmaxf(box[3], y + 1.16f * ry);
            };
            int first = L.shapes;
            put(bc.x, bc.y, br.x, br.y, 1, 0, 0, 0);
            for (int j = 0; j < nb; j++)
                put(o.x + b[j].x * w, o.y + b[j].y * w, b[j].r * w, b[j].r * w, 0, b[j].shade, rnd(80 + j) * 6.28f,
                    (float)(7 + (int)(rnd(90 + j) * 4)));
            float* c = L.cl + 16 * L.clouds++;
            const float rec[16] = {box[0], box[1], box[2], box[3], (float)first, (float)(L.shapes - first), (float)k, strength[k],
                                   hc.x, hc.y, hr.x, hr.y, -(0.15f + 0.07f * rnd(99)), bc.x, 0, 0};
            memcpy(c, rec, sizeof rec);
        }
    }
}

// The engraving shader: every half-tone of the world cut into black lines on paper -- one hatching
// for the half-tones, crossed in the shadows, crossed again in the deepest -- the lines fixed on the
// world (the camera's transform undone from each pixel), wavering a little as if cut by hand. Where
// the tint canvas covers (tintTex: Leon or Anna, the one waiting, the Monstermunchs), hand-coloured,
// as an old print: the same lines, a little lighter, its colour washed in under them, not paper.
static const char* kEngraveFS = GRAVITY_FRAGMENT_INPUTS R"(
uniform sampler2D texture0;
uniform vec2 screen;        // the screen, in pixels...
uniform vec2 pixelScale;    // ...and its pixels per framebuffer pixel
uniform vec2 camTarget;
uniform vec2 camOffset;
uniform float camRot;       // radians
uniform float camZoom;
uniform float spacing;      // between lines, in screen pixels
uniform sampler2D tintTex;  // (the colours to wash in, where its alpha is)
out vec4 finalColor;

float strokes(float u, float cover) {        // 1 on the lines; `cover`: the share of each period inked
    float d = abs(fract(u) - 0.5) * 2.0;
    float w = max(fwidth(u) * 2.0, 0.02);
    return (1.0 - smoothstep(cover - w, cover + w, d)) * smoothstep(0.0, 0.15, cover);   // (none at all on bare paper)
}

void main() {
    vec4 t = texture(texture0, fragTexCoord);   // (alpha: 1 to engrave, 0 to keep the colour as drawn)
    vec3 c = t.rgb;
    vec4 tc = texture(tintTex, fragTexCoord);
    float tinted = step(0.5, tc.a) * step(0.5, t.a);
    float dark = clamp((0.93 - dot(c, vec3(0.299, 0.587, 0.114))) / 0.82, 0.0, 1.0) * mix(1.0, 0.75, tinted);
    vec2 s = vec2(gl_FragCoord.x * pixelScale.x, screen.y - gl_FragCoord.y * pixelScale.y) - camOffset;
    float cr = cos(camRot), sr = sin(camRot);
    vec2 p = (camTarget + vec2(cr * s.x + sr * s.y, -sr * s.x + cr * s.y) / camZoom) * camZoom / spacing;
    float wob = 0.16 * sin(p.y * 0.23 + p.x * 0.05) + 0.08 * sin(p.x * 0.41);
    float ink = strokes(dot(p, vec2(0.7071, 0.7071)) + wob, dark * 0.95);
    ink = max(ink, strokes(dot(p, vec2(0.7071, -0.7071)) + wob, clamp((dark - 0.42) * 1.6, 0.0, 1.0)));
    ink = max(ink, strokes(p.y + wob * 0.5, clamp((dark - 0.72) * 2.6, 0.0, 1.0)));
    ink = max(ink, smoothstep(0.86, 0.94, dark));
    vec3 paper = mix(vec3(0.955, 0.942, 0.905), mix(tc.rgb / max(tc.a, 0.001), vec3(0.955, 0.942, 0.905), 0.12), tinted);
    vec3 engraved = mix(paper, vec3(0.075, 0.068, 0.062), ink);
    finalColor = vec4(mix(c, engraved, t.a), 1.0);
}
)";

// A small arrow centred on c, pointing at angle `deg` (0 = right, 90 = down), `size` long.
static void arrow(Vector2 c, float deg, float size, Color col) {
    float a = deg * DEG2RAD, ca = cosf(a), sa = sinf(a);
    auto at = [&](float x, float y) { return Vector2{c.x + x * ca - y * sa, c.y + x * sa + y * ca}; };
    float h = size / 2;
    inkLine(at(-h, 0), at(h * 0.1f, 0), size * 0.2f, col);
    DrawTriangle(at(h, 0), at(-h * 0.05f, -h * 0.75f), at(-h * 0.05f, h * 0.75f), col);
}

// Diagonal hatching "////" clipped to a rectangle, world-aligned so stacked blocks join up: each
// line a band cut exactly along the rectangle's sides -- the next rectangle's band goes on from that
// very edge, no notch at the seam (a line's own square end would leave one) and no overlap (a darker
// dot, the ink being see-through).
static void hatch(Rectangle r, float spacing, float thick, Color col) {
    float x0 = r.x, y0 = r.y, x1 = r.x + r.width, y1 = r.y + r.height;
    float h = thick * 0.70710678f;                       // (half the band's width, along x + y)
    float k0 = ceilf((x0 + y0 - h) / spacing) * spacing;
    for (float k = k0; k < x1 + y1 + h; k += spacing) { // lines x + y = k
        if (fmaxf(x0, k - h - y1) >= fminf(x1, k + h - y0)) continue;
        Vector2 p[8], q[8];                              // the rectangle (clockwise on the screen), cut by both sides of the band
        int n = 4, m;
        p[0] = {x0, y0}; p[1] = {x1, y0}; p[2] = {x1, y1}; p[3] = {x0, y1};
        for (int side = 0; side < 2; side++) {           // keep x + y >= k - h, then x + y <= k + h
            m = 0;
            for (int i = 0; i < n; i++) {
                Vector2 a = p[i], b = p[(i + 1) % n];
                float da = side ? k + h - (a.x + a.y) : a.x + a.y - (k - h), db = side ? k + h - (b.x + b.y) : b.x + b.y - (k - h);
                if (da >= 0) q[m++] = a;
                if ((da >= 0) != (db >= 0)) q[m++] = Vector2Lerp(a, b, da / (da - db));
            }
            n = m;
            for (int i = 0; i < n; i++) p[i] = q[i];
        }
        if (n >= 3) DrawTriangleFan(p, n, col);
    }
}

// Deadly spikes: two isosceles triangles per tile on a dark base, pointing at the open air.
static void drawSpikes(const Level& L, int x, int y, float X, float Y) {
    // They point away from what holds them (solid ground -- not more spikes) into the open, so a
    // row along a wall points off the wall to its very end. Held by nothing: the first open side.
    auto open = [&](int ax, int ay) { return !L.block(ax, ay) && !L.oneway(ax, ay); };
    auto held = [&](int ax, int ay) { return L.block(ax, ay) && !L.deadly(ax, ay); };
    float ang = 0;                                        // 0: up
    if (held(x, y + 1) && open(x, y - 1)) ang = 0;
    else if (held(x, y - 1) && open(x, y + 1)) ang = 180;
    else if (held(x + 1, y) && open(x - 1, y)) ang = -90;
    else if (held(x - 1, y) && open(x + 1, y)) ang = 90;
    else if (open(x, y - 1)) ang = 0;
    else if (open(x - 1, y)) ang = -90;
    else if (open(x + 1, y)) ang = 90;
    else if (open(x, y + 1)) ang = 180;
    const Color steel = {176, 182, 194, 255}, shade = {112, 118, 132, 255}, base = {58, 58, 70, 255};
    rlPushMatrix();
    rlTranslatef(X + kTile / 2, Y + kTile / 2, 0);
    rlRotatef(ang, 0, 0, 1);
    const float h = kTile / 2;
    DrawRectangleV({-h, h - 7}, {kTile, 7}, base);
    for (int k = 0; k < 2; k++) {
        float x0 = -h + k * h, x1 = x0 + h, xm = (x0 + x1) / 2;
        Vector2 a = {x0 + 1, h - 7}, b = {x1 - 1, h - 7}, tip = {xm, -h + 3};
        DrawTriangle(a, b, tip, steel);
        DrawTriangle({xm, h - 7}, b, tip, shade);             // the shaded side
        inkLine(a, tip, 2.0f, kInk); inkLine(tip, b, 2.0f, kInk);
    }
    inkLine({-h, h - 7}, {h, h - 7}, 2.0f, kInk);
    rlPopMatrix();
}

// Corner k of a tile-sized square -- 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left --
// rounded to radius r: the centre of its arc.
static Vector2 cornerCentre(float X, float Y, float s, float r, int k) {
    return {k == 0 || k == 3 ? X + r : X + s - r, k < 2 ? Y + r : Y + s - r};
}

// A tile-sized square with the corners cv[k] rounded to radius r.
static void roundedTile(float X, float Y, float s, const bool cv[4], float r, Color col) {
    DrawRectangleRec({X + r, Y, s - 2 * r, s}, col);              // the middle, top to bottom
    DrawRectangleRec({X, Y + r, r, s - 2 * r}, col);              // the two sides, between corners
    DrawRectangleRec({X + s - r, Y + r, r, s - 2 * r}, col);
    const Vector2 sq[4] = {{X, Y}, {X + s - r, Y}, {X + s - r, Y + s - r}, {X, Y + s - r}};
    for (int k = 0; k < 4; k++) {
        if (cv[k]) DrawCircleSector(cornerCentre(X, Y, s, r, k), r, 180.0f + 90.0f * k, 270.0f + 90.0f * k, 8, col);
        else DrawRectangleRec({sq[k].x, sq[k].y, r, r}, col);
    }
}

// A gravity platform's corner radius: gravity_platform_round of its width (left to right, as in
// the level file), at most half its smaller side -- and half a tile, a corner being drawn in one.
static float gravityCornerR(const Level& L, int x, int y, float round) {
    char c = L.at(x, y);
    int x0 = x, x1 = x, y0 = y, y1 = y;
    while (L.at(x0 - 1, y) == c) x0--;
    while (L.at(x1 + 1, y) == c) x1++;
    while (L.at(x, y0 - 1) == c) y0--;
    while (L.at(x, y1 + 1) == c) y1++;
    float w = (x1 - x0 + 1) * kTile, h = (y1 - y0 + 1) * kTile;
    return fminf(round * w, fminf(fminf(w, h), kTile) / 2);
}

static void drawLevel(const Game& g, Rectangle view) {
    const Level& L = g.level;
    // (What cannot reach the screen -- gSeen, the screen as it is turned -- is left out: a slab wholly
    // off it, a tile more than a tile off it (what spills over a tile, an ink line, an arrow, a flower,
    // stays within the next). The rest is drawn as ever, cut to `view` as ever.)
    for (const Rectangle& r : L.solidRects) {
        if (g.engrave) break;
        if (!CheckCollisionRecs(r, view) || !CheckCollisionRecs(r, gSeen)) continue;
        Rectangle c = GetCollisionRec(r, view);
        DrawRectangleRec(c, kSlab);
        hatch(c, 13.0f, 1.6f, fade(kInk, 0.45f));
    }
    // Ink outline only where solid meets air, so merged blocks read as one mass.
    int tx0 = std::max(0, (int)floorf(view.x / kTile) - 1), tx1 = std::min(L.w, (int)ceilf((view.x + view.width) / kTile) + 1);
    int ty0 = std::max(0, (int)floorf(view.y / kTile) - 1), ty1 = std::min(L.h, (int)ceilf((view.y + view.height) / kTile) + 1);
    tx0 = std::max(tx0, (int)floorf(gSeen.x / kTile) - 1); tx1 = std::min(tx1, (int)ceilf((gSeen.x + gSeen.width) / kTile) + 1);
    ty0 = std::max(ty0, (int)floorf(gSeen.y / kTile) - 1); ty1 = std::min(ty1, (int)ceilf((gSeen.y + gSeen.height) / kTile) + 1);
    const float th = 3.0f, e = th / 2;
    // A gravity tile's outer corners (cv: top left, top right, bottom right, bottom left) and their radius.
    // (An ice one: square, as a wall.)
    auto gravityShape = [&](int x, int y, bool cv[4]) {
        char c = L.at(x, y);
        auto same = [&](int dx, int dy) { return L.at(x + dx, y + dy) == c; };
        cv[0] = !same(-1, 0) && !same(0, -1); cv[1] = !same(1, 0) && !same(0, -1);
        cv[2] = !same(1, 0) && !same(0, 1);   cv[3] = !same(-1, 0) && !same(0, 1);
        float r = (cv[0] || cv[1] || cv[2] || cv[3]) && !iceGravity(c) ? gravityCornerR(L, x, y, g.t.gravRound) : 0;
        if (r < 0.5f) { r = 0; cv[0] = cv[1] = cv[2] = cv[3] = false; }
        return r;
    };
    // Its arrows -- drawn once every gravity tile is down, so an arrow crossing into the next tile is not
    // covered by it. They run slowly the way they point (gravity_arrow_speed), fading in and out within
    // gravity_arrow_border of the platform's ends that way: an endless stream.
    auto gravityArrows = [&](int x, int y) {
        float X = x * kTile, Y = y * kTile;
        char c = L.at(x, y);
        int f = gravityFrame(c);
        float deg = (f == 3) ? 0 : (f == 0) ? 90 : (f == 1) ? 180 : 270;
        bool cv[4];
        float r = gravityShape(x, y, cv);
        const int n = 3;
        const float cell = kTile / n, size = kLeonDrawH / 5;
        bool horiz = f == 3 || f == 1;
        int p0 = horiz ? x : y, p1 = p0;                 // the platform's run that way (tiles)
        while (horiz ? L.at(p0 - 1, y) == c : L.at(x, p0 - 1) == c) p0--;
        while (horiz ? L.at(p1 + 1, y) == c : L.at(x, p1 + 1) == c) p1++;
        float runA = p0 * kTile, runB = (p1 + 1) * kTile;
        float off = fmodf(g.time * g.t.gravArrowSpeed, cell) * ((f == 3 || f == 0) ? 1.0f : -1.0f);
        for (int j = 0; j < n; j++)                      // (across)
            for (int i = -1; i <= n; i++) {              // (along: one more each side, the stream through the tile)
                float u = (i + 0.5f) * cell + off;
                if (u < 0 || u >= kTile) continue;       // (its middle in this tile: this tile's arrow)
                Vector2 a = horiz ? Vector2{X + u, Y + (j + 0.5f) * cell} : Vector2{X + (j + 0.5f) * cell, Y + u};
                float along = horiz ? a.x : a.y;
                float alpha = g.t.gravArrowBorder > 0 ? clampf(fminf(along - runA, runB - along) / g.t.gravArrowBorder, 0, 1) : 1;
                bool cut = alpha <= 0;                   // an arrow a rounded corner would clip: left out
                for (int k = 0; k < 4; k++) {
                    Vector2 o = cornerCentre(X, Y, kTile, r, k);
                    bool out = (k == 0 || k == 3 ? a.x < o.x : a.x > o.x) && (k < 2 ? a.y < o.y : a.y > o.y);
                    if (cv[k] && out && Vector2Distance(a, o) > r - size * 0.6f) cut = true;
                }
                if (!cut) arrow(a, deg, size, fade(iceGravity(c) ? kIceArrow : kArrow, alpha));
            }
    };
    for (int y = ty0; y < ty1; y++)
        for (int x = tx0; x < tx1; x++) {
            float X = x * kTile, Y = y * kTile;
            if (L.endWall(x, y)) continue;               // (the ending's wall: never seen)
            if (L.solid(x, y)) {
                Color ink = kInk;
                if (g.engrave) engraveSolid(L, x, y, X, Y);
                auto edge = [&](int ax, int ay) { return !L.solid(ax, ay); };   // (round a Monstermunch too: dug out round it)
                if (edge(x, y - 1)) inkLine({X - e, Y}, {X + kTile + e, Y}, th, ink);
                if (edge(x, y + 1)) inkLine({X - e, Y + kTile}, {X + kTile + e, Y + kTile}, th, ink);
                if (edge(x - 1, y)) inkLine({X, Y - e}, {X, Y + kTile + e}, th, ink);
                if (edge(x + 1, y)) inkLine({X + kTile, Y - e}, {X + kTile, Y + kTile + e}, th, ink);
            } else if (L.slippy(x, y)) {
                // Slippery: icy blue, glossy streaks.
                DrawRectangle((int)X, (int)Y, (int)kTile, (int)kTile, kIce);
                for (int k = 0; k < 2; k++) {
                    float o = 8 + k * 18;
                    inkLine({X + o, Y + 5}, {X + o + 9, Y + 5}, 2.0f, kIceShine);
                    inkLine({X + o - 4, Y + 11}, {X + o + 2, Y + 11}, 1.5f, kIceShine);
                }
                if (!L.slippy(x, y - 1)) inkLine({X - e, Y}, {X + kTile + e, Y}, th, kInk);
                if (!L.slippy(x, y + 1)) inkLine({X - e, Y + kTile}, {X + kTile + e, Y + kTile}, th, kInk);
                if (!L.slippy(x - 1, y)) inkLine({X, Y - e}, {X, Y + kTile + e}, th, kInk);
                if (!L.slippy(x + 1, y)) inkLine({X + kTile, Y - e}, {X + kTile, Y + kTile + e}, th, kInk);
            } else if (L.deadly(x, y)) {
                drawSpikes(L, x, y, X, Y);            // no frame round them: the ground keeps its own line
            } else if (L.gravity(x, y)) {
                // Gravity platform: black, paved with arrows a fifth of Leon's height (gravityArrows,
                // once every tile is down), its outer corners rounded (gravity_platform_round of its width).
                // An ice one: white, its arrows dark -- no outline (the rock keeps its own line along it).
                char c = L.at(x, y);
                auto same = [&](int dx, int dy) { return L.at(x + dx, y + dy) == c; };
                bool cv[4];
                float r = gravityShape(x, y, cv);
                roundedTile(X, Y, kTile, cv, r, iceGravity(c) ? kIceGravity : kGravity);
                if (iceGravity(c)) {                     // (along the rock: its line again, whole -- half of it lay under this, its ends dots)
                    if (L.solid(x, y - 1)) inkLine({X, Y}, {X + kTile, Y}, th, kInk);
                    if (L.solid(x, y + 1)) inkLine({X, Y + kTile}, {X + kTile, Y + kTile}, th, kInk);
                    if (L.solid(x - 1, y)) inkLine({X, Y}, {X, Y + kTile}, th, kInk);
                    if (L.solid(x + 1, y)) inkLine({X + kTile, Y}, {X + kTile, Y + kTile}, th, kInk);
                    continue;
                }
                // Ink where it meets anything else -- round its outer corners.
                float t0 = cv[0] ? r : -e, t1 = cv[1] ? r : -e, t2 = cv[2] ? r : -e, t3 = cv[3] ? r : -e;
                if (!same(0, -1)) inkLine({X + t0, Y}, {X + kTile - t1, Y}, th, kInk);
                if (!same(0, 1)) inkLine({X + t3, Y + kTile}, {X + kTile - t2, Y + kTile}, th, kInk);
                if (!same(-1, 0)) inkLine({X, Y + t0}, {X, Y + kTile - t3}, th, kInk);
                if (!same(1, 0)) inkLine({X + kTile, Y + t1}, {X + kTile, Y + kTile - t2}, th, kInk);
                for (int k = 0; k < 4; k++)
                    if (cv[k]) DrawRing(cornerCentre(X, Y, kTile, r, k), fmaxf(0, r - th / 2), r + th / 2,
                                        180.0f + 90.0f * k, 270.0f + 90.0f * k, 8, kInk);
            } else if (L.oneway(x, y) && g.engrave) {
                engraveGirder(L, x, y, X, Y);
            } else if (L.oneway(x, y)) {
                const float sh = 12.0f;
                Rectangle r = {X, Y, kTile, sh};
                DrawRectangleRec(r, kSlab);
                hatch(r, 9.0f, 1.4f, fade(kInk, 0.5f));
                inkLine({X - e, Y}, {X + kTile + e, Y}, th, kInk);
                inkLine({X - e, Y + sh}, {X + kTile + e, Y + sh}, 2.0f, kInk);
                if (!L.oneway(x - 1, y)) inkLine({X, Y}, {X, Y + sh}, th, kInk);
                if (!L.oneway(x + 1, y)) inkLine({X + kTile, Y}, {X + kTile, Y + sh}, th, kInk);
            }
        }
    for (int y = ty0; y < ty1; y++)                      // the gravity platforms' arrows, over them all
        for (int x = tx0; x < tx1; x++)
            if (L.gravity(x, y)) gravityArrows(x, y);
}

static void drawStar(Vector2 c, float R, float rot, Color fill, float outline) {
    Vector2 pts[10];
    for (int i = 0; i < 10; i++) {
        float a = rot - PI / 2 + i * PI / 5;
        float r = (i % 2 == 0) ? R : R * 0.45f;
        pts[i] = {c.x + cosf(a) * r, c.y + sinf(a) * r};
    }
    for (int i = 0; i < 10; i++) DrawTriangle(c, pts[(i + 1) % 10], pts[i], fill);
    if (outline > 0)
        for (int i = 0; i < 10; i++) inkLine(pts[i], pts[(i + 1) % 10], outline, kInk);
}

// The hat: a tall crown in four bands, from the brim up colour, white, colour, white -- the colour
// the last key taken's, black (ink) before any (gHatBand) -- on a black brim whose ends curl up.
// `base`: the middle of the brim, where it sits; `deg`: its turn; `hl`, `hr`: how tall the crown's
// left and right sides are (1 = whole); `curl`: how curled the brim's ends are (1 as drawn, 0 flat,
// more while it rings).
// The crown is a bellows, as an accordion's, its bands its panels: two pleats a side, stiff panels
// hinged at ridges (out) and creases (in) -- the coloured ones facing down, folded. Whole, flat;
// squeezed, the panels fold out -- and stay deep until it is nearly open, as stiff panels do -- the
// ridges and creases thin lines across, its sides' ink thinner.
static const float kBrimHalf = 1.25f;                // half the brim's thickness
static Color gHatWhite = kBody;               // the hat's white: on his head, gLeonHat (drawLeon); knocked off, its own (Game::hatWhite); at its spot, white
static Color gHatBand = kInk;                 // its coloured bands: the last key taken's colour -- black before any; the Love door's pink (main)
static void drawHat(Vector2 base, float deg, float hl, float hr, float curl = 1) {
    const float cw = 15, hL = kHatCrown * hl, hR = kHatCrown * hr;
    const int n = 4;                                     // panels a side, from the brim up: crease, ridge, ...
    float ampL = 5.0f * sqrtf(fmaxf(0, 1 - hl * hl)), ampR = 5.0f * sqrtf(fmaxf(0, 1 - hr * hr));
    float fold = fminf(1, fmaxf(ampL, ampR) / 2.5f);    // how plainly it shows as bellows
    rlPushMatrix();
    rlTranslatef(base.x, base.y, 0);
    rlRotatef(deg, 0, 0, 1);
    Vector2 L[n + 1], R[n + 1];
    for (int k = 0; k <= n; k++) {
        L[k] = {-cw / 2 - ((k % 2) ? ampL : 0), -hL * k / n};
        R[k] = {cw / 2 + ((k % 2) ? ampR : 0), -hR * k / n};
    }
    const Color white = gHatWhite;                       // (a colour fly's tint, on his head)
    for (int k = 0; k < n; k++) {                        // the crown, panel by panel: colour, white, colour, white
        Color c = k % 2 == 0 ? gHatBand : white;
        DrawTriangle(L[k], R[k], R[k + 1], c);
        DrawTriangle(L[k], R[k + 1], L[k + 1], c);
    }
    if (fold > 0.02f)                                    // folded: the ridges and creases across
        for (int k = 1; k < n; k++) inkLine(L[k], R[k], k % 2 ? 0.8f : 0.6f, fade(kInk, fold * (k % 2 ? 0.7f : 0.4f)));
    const float side = lerpf(2.2f, 1.5f, fold);
    for (int k = 0; k < n; k++) { inkLine(L[k], L[k + 1], side, kInk); inkLine(R[k], R[k + 1], side, kInk); }
    inkLine(L[n], R[n], 2.2f, kInk);
    // The brim: flat where it sits on the head, its ends curling up and back in -- each segment
    // turned `curl` times as far as drawn (the flat part being the 0 they turn from).
    const Vector2 shape[7] = {{0, 0}, {12.5f, 0}, {14.6f, -1.6f}, {16, -4}, {16.6f, -6.6f}, {16, -8.8f}, {14.8f, -9.8f}};
    Vector2 half[7] = {shape[0], shape[1]};
    for (int k = 2; k < 7; k++) {
        Vector2 seg = Vector2Subtract(shape[k], shape[k - 1]);
        float ang = atan2f(seg.y, seg.x) * curl, len = Vector2Length(seg);
        half[k] = Vector2Add(half[k - 1], {cosf(ang) * len, sinf(ang) * len});
    }
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 6; k++) {
            Vector2 a = {half[k].x * side, half[k].y}, b = {half[k + 1].x * side, half[k + 1].y};
            inkLine(a, b, 2 * kBrimHalf, kInk);
            DrawCircleV(b, kBrimHalf, kInk);
        }
    rlPopMatrix();
}

// The hat on his head, standing: as folded as it is -- and no taller than the room a ceiling
// leaves above him, crown and brim alike: crushed, not through it. `seat`: how far above his
// hitbox's top its brim sits; in the drawing's frame, squashed or stretched with him.
static void hatShape(const Game& g, float seat, float& hl, float& hr, float& curl) {
    g.hatSides(hl, hr);
    curl = g.hatCurl();
    float sy = 1 - clampf(g.squash, -0.4f, 0.6f);
    float room = (kLeonH + fminf(g.hatRoom, 1000.0f)) / sy - kLeonH - seat;   // from the brim up
    hl = fminf(hl, fmaxf(0, room) / kHatCrown);
    hr = fminf(hr, fmaxf(0, room) / kHatCrown);
    curl = fminf(curl, fmaxf(0, room - kBrimHalf) / kBrimRise);
}

// In the dark (dark air, '.'), Leon is drawn a second time over the black: his eyes only.
static bool gEyesOnly = false;
static bool gEyesInDark = false;   // ...that is, over the dark (eyesOver): no straining lids -- round, they hide his lidded ones under them
static bool gNoHat = false, gHatOnly = false;  // Leon drawn without his hat -- or his hat alone (with gEyesOnly: nothing else)
static bool gAnnaShape = false;               // the player drawn as Anna (Game::anna; main, each frame): round, her face, her hat
static bool gTitleDuo = false;                // the title, the two games finished: the two face to face, nobody's game yet (main)
static float gAnnaMouthScale = 0.9f, gAnnaMouthH = 0.4f;   // Anna's mouth open: anna_mouth_body_scale, anna_mouth_height (main)
static void annaFace(const Game& g, Vector2 c, Vector2 look, int eyes, float big, float lid, float squint = 1);
static void annaHat(const Game& g, Vector2 base, float deg, bool fit);
static void drawAnnaHat(Vector2 base, float deg, const float h[4], float lean, float tip, float curlL, float curlR, float press, float rise, float reachL,
                        float reachR, float bs);
// Her eyes in a dash, narrowed: their height, of their round (1: as ever; Game::dashSquint).
static float annaSquint(const Game& g) { return 1 - (1 - g.t.dashEyes) * g.dashSquint(); }
static bool leonHat(const Game& g) { return g.hasHat && (gHatOnly || (!gEyesOnly && !gNoHat)); }
static Color gLeonBody = kBody;               // his body's colour: his own (Game::bodyColour) -- tinted with a colour fly in his belly...
static Color gLeonHat = kBody;                // ...his hat's, on his head: the Love door's pink (Game::hatWhite; the Solo door's gold, not), the fly's tint (GOD: golden)
static float gPupilR = 1.9f;                  // his pupils' radius (GOD too: as ever)
static Color gTongue = kTongue;               // his tongue's: pink -- with a colour fly in his belly, the fly's colour
// A colour, paler: its saturation down to `keep` of it, a little lighter for it.
static Color paler(Color c, float keep) {
    Vector3 h = ColorToHSV(c);
    Color o = ColorFromHSV(h.x, h.y * keep, h.z + (1 - h.z) * (1 - keep) * 0.5f);
    o.a = c.a;
    return o;
}
// What the screen can show (world): a square round its middle as wide as its diagonal, plus a margin
// -- whatever the turn of the screen with gravity, even mid-turn. Things wholly outside it (r: how
// far they reach from p) are not drawn.
static Rectangle gView{};
static bool inView(Vector2 p, float r) { return CheckCollisionCircleRec(p, r, gView); }

static void drawBodyFace(const Game& g, Rectangle body, int eyes /*0 open, 1 shut, 2 crossed*/) {
    float cx = body.x + body.width / 2;
    if (gAnnaShape) {                                    // Anna: round, her hat on her (as folded as it is), her face
        Vector2 c = {cx, body.y + body.height / 2};
        if (!gEyesOnly) {
            DrawEllipseV(c, body.width / 2, body.height / 2, kInk);
            DrawEllipseV(c, body.width / 2 - kOutline, body.height / 2 - kOutline, gLeonBody);
        }
        if (leonHat(g)) annaHat(g, {cx, c.y - (kBodyH / 2 - 2) - kAnnaSeat}, 0, false);
        annaFace(g, c, {g.facing * 2.0f, 0}, eyes, 0, 0);
        return;
    }
    if (!gEyesOnly) {
        DrawRectangleRounded(body, kBodyRound, 8, gLeonBody);
        DrawRectangleRoundedLinesEx(body, kBodyRound, 8, kOutline, kInk);
    }
    if (g.t.barDecor && !gEyesOnly) {                    // display_Leon_bar_decorator
        float edgeX = cx - g.facing * (body.width / 2 - 5);
        inkLine({edgeX, body.y + 4}, {edgeX, body.y + body.height - 4}, 2.2f, kInk);
    }
    if (leonHat(g)) {                                    // resting on the outline (drawn outside the body)
        float hl, hr;
        g.hatSides(hl, hr);
        drawHat({cx, body.y - kOutline - kBrimHalf + 0.5f}, 0, hl, hr, g.hatCurl());
    }
    for (int side = -1; side <= 1 && !gHatOnly; side += 2) {
        Vector2 e = {cx + g.facing * 4.0f + side * 7.0f, body.y + 10};
        if (eyes == 2) {
            const float k = 3.6f;
            inkLine({e.x - k, e.y - k}, {e.x + k, e.y + k}, 2.4f, kInk);
            inkLine({e.x - k, e.y + k}, {e.x + k, e.y - k}, 2.4f, kInk);
        } else if (eyes == 1) {
            inkLine({e.x - 4, e.y + 0.5f}, {e.x + 4, e.y + 0.5f}, 2.2f, kInk);
        } else {
            DrawCircleV(e, 4.8f, kInk);
            DrawCircleV(e, 3.2f, kBody);
            DrawCircleV({e.x + g.facing * 2.0f, e.y}, gPupilR, kInk);
        }
    }
}

static void dizzyStars(const Game& g, Vector2 top, float alpha) {
    if (alpha <= 0 || gEyesOnly) return;
    for (int i = 0; i < 3; i++) {
        float ang = g.time * 4.5f + i * (2 * PI / 3);
        Vector2 sp = {top.x + cosf(ang) * 20, top.y - 12 + sinf(ang) * 6};
        drawStar(sp, 6, ang, fade(kStar, alpha), alpha > 0.5f ? 1.6f : 0);
    }
}

// Leon's drawing frame -- the exact chain of transforms drawLeon/drawLeonLocal apply -- so that
// a world point can be found on his body and back. Squash is anchored at the feet, the lean
// and the double-jump flip turn about the middle of the hitbox.
struct LeonFrame {
    Vector2 c;         // hitbox centre, world
    float ang;         // his turn with the world (degrees)
    float sx, sy;      // squash
    float tf;          // lean + flip (degrees)
};
static float flipAngle(const Game& g) {
    if (g.spin <= 0 || g.t.doubleJumpSpin <= 0) return 0;
    float k = 1 - g.spin / g.t.doubleJumpSpin;
    return (1 - (1 - k) * (1 - k)) * 360.0f * g.facing;
}
static LeonFrame leonFrame(const Game& g, Vector2 centre, float angle) {
    float sq = clampf(g.squash, -0.4f, 0.6f);
    return {centre, angle, 1 + sq * 0.8f, 1 - sq, g.tilt + flipAngle(g)};
}
static Vector2 frameToWorld(const LeonFrame& f, Vector2 l) {
    Vector2 a = Vector2Rotate(l, f.tf * DEG2RAD);
    a.y -= kLeonH / 2;
    a = {a.x * f.sx, a.y * f.sy};
    a.y += kLeonH / 2;
    return Vector2Add(f.c, Vector2Rotate(a, f.ang * DEG2RAD));
}
static Vector2 worldToFrame(const LeonFrame& f, Vector2 w) {
    Vector2 a = Vector2Rotate(Vector2Subtract(w, f.c), -f.ang * DEG2RAD);
    a.y -= kLeonH / 2;
    a = {a.x / f.sx, a.y / f.sy};
    a.y += kLeonH / 2;
    return Vector2Rotate(a, -f.tf * DEG2RAD);
}
static Rectangle bodyRect() { return {-kBodyW / 2, -kLeonH / 2 - 2, kBodyW, kBodyH}; }

// The cube's outline, its corners rounded as DrawRectangleRounded rounds them (kBodyRound): round
// 0, half-sides hw and hh -- clockwise, a convex polygon.
static std::vector<Vector2> bodyOutline(float hw, float hh) {
    if (gAnnaShape) {                                    // (Anna: her round body, inscribed in the cube's place -- the ink's
        std::vector<Vector2> p;                          // middle line: drawn on it, it covers what her closed one does)
        for (int i = 0; i < 36; i++) {
            float a = (-90 + 10.0f * i) * DEG2RAD;
            p.push_back({(hw - kOutline / 2) * cosf(a), (hh - kOutline / 2) * sinf(a)});
        }
        return p;
    }
    float r = kBodyRound * fminf(hw, hh);
    const Vector2 c[4] = {{hw - r, -(hh - r)}, {hw - r, hh - r}, {-(hw - r), hh - r}, {-(hw - r), -(hh - r)}};
    std::vector<Vector2> p;
    const int seg = 4;
    for (int k = 0; k < 4; k++)
        for (int j = 0; j <= seg; j++) {
            float a = (-90 + 90 * k + 90.0f * j / seg) * DEG2RAD;
            p.push_back({c[k].x + r * cosf(a), c[k].y + r * sinf(a)});
        }
    return p;
}

// Anna, her mouth open (`open`, 0 to 1): her body anna_mouth_body_scale of its size, eased in over the
// first quarter of the opening; her mouth's line through anna_mouth_height of its height from its
// bottom (0.5: its middle) -- the chin bigger or smaller. Leon: as ever, his cube, cut through its middle.
static float mouthBodyScale(float open) { return gAnnaShape ? 1 - (1 - gAnnaMouthScale) * smooth01(open / 0.25f) : 1; }
static Vector2 mouthCentre(Rectangle body, float open) {
    Vector2 bc = {body.x + body.width / 2, body.y + body.height / 2};
    if (gAnnaShape) bc.y += body.height / 2 * mouthBodyScale(open) * (1 - 2 * gAnnaMouthH);
    return bc;
}

// The jaws: the cube cut along u through its middle, the halves turned about the back point.
struct Jaws {
    Vector2 front, back, tipA, tipB;   // tipA/tipB: the two jaw tips (front, turned)
    float alpha;
    Vector2 lips() const { return Vector2Lerp(tipA, tipB, 0.5f); }   // where the tongue leaves
};
static Jaws jaws(Rectangle body, Vector2 u, float open) {
    const Vector2 bc = {body.x + body.width / 2, body.y + body.height / 2}, mc = mouthCentre(body, open);
    const float k = mouthBodyScale(open);
    std::vector<Vector2> o = bodyOutline(body.width / 2 * k, body.height / 2 * k);
    for (Vector2& q : o) q = Vector2Subtract(q, Vector2Subtract(mc, bc));   // (from the mouth's middle)
    Jaws j;
    j.front = Vector2Add(mc, rayExit(o, u));
    j.back = Vector2Add(mc, rayExit(o, Vector2Negate(u)));
    j.alpha = open * 26.0f * DEG2RAD;
    j.tipA = Vector2Add(j.back, Vector2Rotate(Vector2Subtract(j.front, j.back), j.alpha));
    j.tipB = Vector2Add(j.back, Vector2Rotate(Vector2Subtract(j.front, j.back), -j.alpha));
    return j;
}

// Where the mouth must point (drawing frame, unit) and where the tongue leaves it (world):
// aimed from the cube's middle at the tongue's first point outside the body, so the root in
// the mouth and the first segment outside are one straight line, meeting at the lips.
struct MouthPose { Vector2 u; Vector2 lipsW; };
static MouthPose mouthPose(const Game& g, const LeonFrame& f) {
    Rectangle body = bodyRect();
    Vector2 bc = mouthCentre(body, g.mouth);              // (Anna: her mouth's line, lower)
    Vector2 target;
    if (g.tg == Game::tgStuck) {
        int i = g.tongueExit();
        target = (i >= 0) ? g.rope[i] : g.piv.back();
    } else if (g.tg == Game::tgOut || g.tg == Game::tgBack) {
        target = g.tgBnc.empty() ? Vector2Add(g.centreW(), Vector2Scale(g.tgDir, fmaxf(g.tgExt, 30.0f)))
                                 : Vector2Add(g.centreW(), Vector2Scale(g.mouthDir(), 60.0f));   // (bounced: the first stretch)
    } else {
        target = Vector2Add(f.c, Vector2Scale(g.mouthDir(), 60.0f));
    }
    Vector2 d = Vector2Subtract(worldToFrame(f, target), bc);
    Vector2 u = (Vector2Length(d) > 0.01f) ? Vector2Normalize(d) : Vector2{g.facing, 0};
    return {u, frameToWorld(f, jaws(body, u, g.mouth).lips())};
}

static void drawEye(Vector2 e, Vector2 look, bool shut, float big = 0, float lid = 0) {
    if (gHatOnly) return;
    float k = 1 + 0.4f * big;              // popping while he chews the tongue back
    if (shut) {
        inkLine({e.x - 4 * k, e.y + 0.5f}, {e.x + 4 * k, e.y + 0.5f}, 2.2f, kInk);
    } else {
        DrawCircleV(e, 4.8f * k, kInk);
        DrawCircleV(e, 3.2f * k, kBody);
        DrawCircleV({e.x + look.x * k, e.y + look.y * k}, gPupilR, kInk);
        if (lid > 0 && !gEyesInDark) {     // a heavy lid, straining: the eye's top, down to its edge -- in the eye, nothing round it -- his body's colour (gold, pink...)
            float r = 4.8f * k, s = clampf(2 * lid - 1, -1, 1), a0 = asinf(s), y = e.y + r * s, hw = r * cosf(a0);
            Vector2 cap[26];
            for (int i = 0; i < 26; i++) {     // (its arc, left over the top to right: the chord closes it)
                float a = PI - a0 + (PI + 2 * a0) * i / 25;
                cap[i] = {e.x + r * cosf(a), e.y + r * sinf(a)};
            }
            DrawTriangleFan(cap, 26, gLeonBody);
            inkLine({e.x - hw, y}, {e.x + hw, y}, 2.2f, kInk);
        }
    }
}

// The ending's faces (Game::endT). A smile: the bottom of a circle -- radius r, centred at c --
// shown from its middle out, `u` of it (0 none, 1 whole: 80 degrees each way from its lowest
// point), not stretched: an ink line, its ends round.
static void drawSmile(Vector2 c, float r, float u) {
    if (u <= 0 || gHatOnly) return;
    const float span = 80 * DEG2RAD * u, th = 2.2f;
    const int n = 2 + (int)(14 * u);
    Vector2 prev{};
    for (int i = 0; i <= n; i++) {
        float a = PI / 2 - span + 2 * span * i / n;
        Vector2 q = {c.x + r * cosf(a), c.y + r * sinf(a)};
        if (i > 0) inkLine(prev, q, th, kInk);
        prev = q;
    }
    for (float a : {PI / 2 - span, PI / 2 + span}) DrawCircleV({c.x + r * cosf(a), c.y + r * sinf(a)}, th / 2, kInk);
}
// Happy, an eye: ^ -- the top of a circle, an ink line, its ends round.
static void drawHappyEye(Vector2 e) {
    if (gHatOnly) return;
    const float r = 3.8f, th = 2.2f;
    const Vector2 c = {e.x, e.y + 1.5f};
    Vector2 prev{};
    for (int i = 0; i <= 10; i++) {
        float a = PI + PI * i / 10;                // from its left over its top to its right
        Vector2 q = {c.x + r * cosf(a), c.y + r * sinf(a)};
        if (i > 0) inkLine(prev, q, th, kInk);
        prev = q;
    }
    DrawCircleV({c.x - r, c.y}, th / 2, kInk);
    DrawCircleV({c.x + r, c.y}, th / 2, kInk);
}

// Anna (the ending: workshop/ending (draft).png), on the tower's top, facing him (drawAnnaFigure): a
// round body in anna_color, inked; short legs; his eyes, following him; a sorceress's hat -- a tall
// cone of her colour on a brim of ink, its ends curled up in a spiral each. No mouth till she smiles
// (Game::endSmile). Happy, ^^, swaying as he does (endSway): her body, face and hat turning about her
// middle, lowered just enough that neither leg has to stretch, her feet planted, her knees giving,
// outwards; her brim a party horn (endBrimCurl). Upright in the world, as the tower is.
static void drawLeg(Vector2 hip, Vector2 foot, float seg, Vector2 bend, float amt, float th = 3.2f, float ext = 0);
// One of the two standing, as at the ending (drawAnnaFigure, drawLeonFigure): feet, facing, whom the
// eyes follow, the sway (degrees), the glow, the smile shown, the brim's curl, happy (^^), blinking --
// and the colour (white, as played -- her hat a checkerboard, his banded, the last key's colour, black
// before any --, unless given: waiting on the tower's top, anna_color, the hat all of it too).
struct StandPose { Vector2 feet; float dir; Vector2 lookAt; float sway, glow, smile, curl; bool happy, shut; Color body = kBody; };
static void drawLeonFigure(const Game& g, const StandPose& P);
static void drawAnnaFigure(const Game& g, const StandPose& P);
// The tower's top: Anna, waiting for him -- or, Anna playing, Leon waiting for her.
static void drawAnna(const Game& g) {
    if (g.level.endX0 < 0) return;
    const Vector2 F = g.level.annaFeet();
    if (!inView({F.x, F.y - 45}, 70)) return;
    const StandPose P = {F, g.centreW().x < F.x ? -1.0f : 1.0f, g.centreW(), g.endSway(), g.endGlow(), g.endSmile(), g.endBrimCurl(),
                         g.endHappy(), fmodf(g.time + 0.7f, 4.3f) < 0.12f};
    StandPose W = P;                               // (the one waiting: blue, hat and all -- she for him, he for her)
    W.body = g.t.annaCol;
    if (g.anna) drawLeonFigure(g, W);
    else drawAnnaFigure(g, W);
}
// Anna standing -- waiting for him on the tower's top, in her own colour (anna_color), hat and all; on
// the title, the two face to face, white, her hat a checkerboard, as played --: as she is when played
// (drawLeonLocal), her round body in the cube's place, her face and her hat kAnnaPlayS of the tower's
// measures. Facing `dir`, her eyes on `lookAt`, her smile; ^^ when happy; swaying about her middle
// (glowing, as he does), her feet planted, her knees giving, outwards; her brim a party horn.
static void drawAnnaFigure(const Game& g, const StandPose& P) {
    const Vector2 F = P.feet;
    const float dir = P.dir, r = P.sway * DEG2RAD;
    const Color col = P.body;
    const float mid = kLeonH + 2 - kBodyH / 2;               // (her body's middle over her feet: his)
    const float hipY = kBodyH / 2 - 1, footY = mid - 2, L = footY - hipY;
    float crouch = 0;
    for (int side = -1; side <= 1; side += 2) {
        Vector2 hip = Vector2Rotate({side * 6.0f, hipY}, r);
        float dx = side * 6.0f - hip.x;
        crouch = fmaxf(crouch, footY - sqrtf(fmaxf(0, L * L - dx * dx)) - hip.y);
    }
    const Vector2 C = {F.x, F.y - mid + crouch};
    for (int side = -1; side <= 1; side += 2) {               // her legs, her feet planted, both knees towards him
        Vector2 hip = Vector2Add(C, Vector2Rotate({side * 6.0f, hipY}, r)), foot = {F.x + side * 6.0f, F.y - 2};
        drawLeg(hip, foot, L / 2, {dir, 0}, 1, 2.6f, 2.5f);
        DrawEllipse((int)(foot.x + dir * 2.5f), (int)foot.y, 4.2f, 2.2f, kInk);
    }
    rlPushMatrix();                                            // the rest of her turning with her body
    rlTranslatef(C.x, C.y, 0);
    rlRotatef(P.sway, 0, 0, 1);
    DrawEllipseV({0, 0}, kBodyW / 2, kBodyH / 2, kInk);       // her body: inked round
    DrawEllipseV({0, 0}, kBodyW / 2 - kOutline, kBodyH / 2 - kOutline, ColorLerp(col, kPaper, P.glow));   // (face to face with him: glowing, as he does)
    rlPushMatrix();
    rlScalef(kAnnaPlayS, kAnnaPlayS, 1);
    for (int side = -1; side <= 1; side += 2) {
        Vector2 e = {dir * 2.75f + side * 7.15f, -4.2f};
        Vector2 to = Vector2Rotate(Vector2Subtract(P.lookAt, Vector2Add(C, Vector2Rotate(Vector2Scale(e, kAnnaPlayS), r))), -r);   // (her eyes on him)
        float len = Vector2Length(to);
        Vector2 look = len > 1 ? Vector2Scale(to, 2.2f / len) : Vector2{dir * 2.2f, 0};
        if (P.happy) drawHappyEye(e);
        else drawEye(e, look, P.shut);
    }
    drawSmile({dir * 3.0f, 1.5f}, 6.5f, P.smile);
    rlPopMatrix();
    const float whole[4] = {1, 1, 1, 1};
    const Color band = gHatBand, white = gHatWhite;
    if (ColorIsEqual(col, kBody)) {                            // (as played: the checkerboard, the last key's colour, black before any)
        gHatBand = g.lastKey >= 0 ? g.t.keyCol[g.lastKey] : kInk;
        gHatBand.a = 255;
        gHatWhite = kBody;
    } else gHatBand = gHatWhite = col;                         // (waiting: her colour all over)
    drawAnnaHat({0, -(kBodyH / 2 - 2) - kAnnaSeat}, 0, whole, 0, 0, P.curl, P.curl, 1, 1e9f, 1e9f, 1e9f, g.t.annaBrim);
    gHatBand = band; gHatWhite = white;
    rlPopMatrix();
}

// Leon standing, as at his ending (Anna playing: waiting for her on the tower's top, in her blue, his hat
// too -- on the title, the two face to face, in his white, his hat's bands the last key's colour, black
// before any): his cube glowing as she does, his eyes on her, his smile, ^^ when happy; swaying about his
// middle, his feet planted, his knees giving, outwards; his brim a party horn.
static void drawLeonFigure(const Game& g, const StandPose& P) {
    const Vector2 F = P.feet;
    const float dir = P.dir, r = P.sway * DEG2RAD;
    const float mid = kLeonH / 2 + 2 - kBodyH / 2 + kLeonH / 2;   // (his body's middle over his feet)
    const float hipY = kBodyH / 2 - 1, footY = mid - 2, L = footY - hipY;
    float crouch = 0;
    for (int side = -1; side <= 1; side += 2) {
        Vector2 hip = Vector2Rotate({side * 8.0f, hipY}, r);
        float dx = side * 8.0f - hip.x;
        crouch = fmaxf(crouch, footY - sqrtf(fmaxf(0, L * L - dx * dx)) - hip.y);
    }
    const Vector2 C = {F.x, F.y - mid + crouch};
    for (int side = -1; side <= 1; side += 2) {               // his legs, his feet planted, both knees towards her
        Vector2 hip = Vector2Add(C, Vector2Rotate({side * 8.0f, hipY}, r)), foot = {F.x + side * 8.0f, F.y - 2};
        drawLeg(hip, foot, L / 2, {dir, 0}, 1, 3.2f);
        DrawEllipse((int)(foot.x + dir * 2.5f), (int)foot.y, 5, 2.6f, kInk);
    }
    rlPushMatrix();
    rlTranslatef(C.x, C.y, 0);
    rlRotatef(P.sway, 0, 0, 1);
    const Rectangle body = {-kBodyW / 2, -kBodyH / 2, kBodyW, kBodyH};
    DrawRectangleRounded(body, kBodyRound, 8, ColorLerp(P.body, kPaper, P.glow));
    DrawRectangleRoundedLinesEx(body, kBodyRound, 8, kOutline, kInk);
    for (int side = -1; side <= 1; side += 2) {
        Vector2 e = {dir * 4.0f + side * 7.0f, body.y + 10};
        Vector2 to = Vector2Rotate(Vector2Subtract(P.lookAt, Vector2Add(C, Vector2Rotate(e, r))), -r);   // (his eyes on her)
        float len = Vector2Length(to);
        Vector2 look = len > 1 ? Vector2Scale(to, 2.0f / len) : Vector2{dir * 2.0f, 0};
        if (P.happy) drawHappyEye(e);
        else drawEye(e, look, P.shut);
    }
    drawSmile({dir * 4.0f, body.y + 15.5f}, 6.5f, P.smile);
    const Color band = gHatBand, white = gHatWhite;
    if (ColorIsEqual(P.body, kBody)) {
        gHatBand = g.lastKey >= 0 ? g.t.keyCol[g.lastKey] : kInk;
        gHatBand.a = 255;
        gHatWhite = kBody;
    } else gHatBand = gHatWhite = P.body;                  // (in her blue: his hat all of it)
    drawHat({0, body.y - 3.0f - kBrimHalf + 0.5f}, 0, 1, 1, P.curl);
    gHatBand = band; gHatWhite = white;
    rlPopMatrix();
}

// The title, the two games finished: Leon and Anna face to face where he starts, smiling, as on the
// tower's top (`clock`: the title's -- their blinks).
static void drawTitleDuo(const Game& g, float clock) {
    const Vector2 S = g.level.spawn, lf = {S.x - 32, S.y}, af = {S.x + 32, S.y};
    drawLeonFigure(g, {lf, 1, {af.x, af.y - (kLeonH + 2 - kBodyH / 2)}, 0, 0, 1, 1, false, fmodf(clock + 2.1f, 3.7f) < 0.12f});
    drawAnnaFigure(g, {af, -1, {lf.x, lf.y - kLeonH / 2}, 0, 0, 1, 1, false, fmodf(clock + 0.7f, 4.3f) < 0.12f});
}

// A heart, `s` wide, its middle at c: two round lobes and a point -- one outline, filled in one fan
// from its middle (inside both lobes and the point), nothing laid twice: faded, it fades evenly.
static void drawHeart(Vector2 c, float s, Color col) {
    const float r = s * 0.28f, lx = s * 0.24f, ly = -s * 0.12f, tip = s * 0.48f;   // the lobes: radius, centres (-lx, ly) and (lx, ly); the tip, under the middle
    // Each side goes straight up from the tip to just touch its lobe -- running into it unbroken, no
    // corner. The left lobe's outline: from there, round its outside and over the top, to the dip
    // (where the two lobes cross).
    const float dx = lx, dy = tip - ly;                                  // (the tip, from the left lobe's centre)
    float a0 = atan2f(dy, dx) + acosf(r / sqrtf(dx * dx + dy * dy)), a1 = atan2f(-sqrtf(r * r - lx * lx), lx);
    if (a1 < a0) a1 += 2 * PI;
    constexpr int n = 32;
    Vector2 p[2 * n + 4];
    int k = 0;
    p[k++] = c;
    for (int i = 0; i <= n; i++) {                                       // the left lobe, to the dip...
        float a = a0 + (a1 - a0) * i / n;
        p[k++] = {c.x - lx + r * cosf(a), c.y + ly + r * sinf(a)};
    }
    for (int i = n - 1; i >= 0; i--) p[k++] = {2 * c.x - p[1 + i].x, p[1 + i].y};   // ...the right one, its mirror, back down...
    p[k++] = {c.x, c.y + tip};                                           // ...the tip, and up the left side
    p[k++] = p[1];
    DrawTriangleFan(p, k, col);
}
// The ending's hearts (Game::heartsStep): coming in over a third of a second, fading out from
// ending_heart_fade_from to ending_heart_fade_to times the distance between the two, from their middle.
static void drawHearts(const Game& g) {
    if (g.hearts.empty()) return;
    Vector2 M;
    float D;
    g.heartFrame(M, D);
    const float from = g.t.endHeartFadeFrom * D, to = g.t.endHeartFadeTo * D;
    for (const Game::Heart& h : g.hearts) {
        float a = clampf(h.age / 0.33f, 0, 1) * (1 - clampf((Vector2Distance(h.p, M) - from) / (to - from), 0, 1));
        if (a > 0 && inView(h.p, g.t.endHeartSize)) drawHeart(h.p, g.t.endHeartSize, fade(g.t.endHeartCol, a));
    }
}

// The mouth: the cube -- its corners rounded, as when shut -- splits along the tongue's direction
// into two halves, hinged at the back, that open like jaws. The eyes ride on whichever half they sit in.
static void drawOpenBody(const Game& g, Rectangle body, Vector2 u, float open, Vector2 look, bool shut) {
    Vector2 bc = {body.x + body.width / 2, body.y + body.height / 2};
    const Vector2 mc = mouthCentre(body, open);          // (Anna: the cut lower than her middle -- Leon: his middle)
    const float bk = mouthBodyScale(open);               // (Anna: her body smaller, open)
    Jaws jw = jaws(body, u, open);
    Vector2 front = jw.front, back = jw.back;
    Vector2 nrm = {-u.y, u.x};
    std::vector<std::pair<float, Vector2>> pa, pb;
    for (Vector2 r : bodyOutline(body.width / 2 * bk, body.height / 2 * bk)) {
        Vector2 c = Vector2Add(bc, r), m = gAnnaShape ? Vector2Subtract(c, mc) : r;   // (from the cut's middle)
        float ang = atan2f(Vector2DotProduct(m, nrm), Vector2DotProduct(m, u));
        if (ang > 0) pa.push_back({ang, c}); else pb.push_back({-ang, c});
    }
    std::sort(pa.begin(), pa.end(), [](auto& a, auto& b) { return a.first < b.first; });
    std::sort(pb.begin(), pb.end(), [](auto& a, auto& b) { return a.first < b.first; });
    float alpha = jw.alpha;
    auto turn = [&](Vector2 p, float a) { return Vector2Add(back, Vector2Rotate(Vector2Subtract(p, back), a)); };
    std::vector<Vector2> A{turn(front, alpha)}, B{turn(front, -alpha)};
    for (auto& k : pa) A.push_back(turn(k.second, alpha));
    for (auto& k : pb) B.push_back(turn(k.second, -alpha));
    A.push_back(back); B.push_back(back);

    // Inside of the mouth, with the root of the tongue.
    Vector2 mouthTri[3] = {back, A[0], B[0]};
    if (!gEyesOnly) DrawTriangle(mouthTri[0], mouthTri[1], mouthTri[2], kMouth);
    if (g.tg != Game::tgNone && !gEyesOnly) {             // the root, ending exactly where the tongue leaves
        Vector2 root = Vector2Lerp(back, front, 0.35f), lips = jw.lips();
        inkLine(root, lips, 4.5f, gTongue);
        DrawCircleV(lips, 2.25f, gTongue);
    }

    for (auto* half : {&A, &B}) {
        if (gEyesOnly) break;
        auto& h = *half;
        DrawTriangleFan(h.data(), (int)h.size(), gLeonBody);
        for (size_t i = 0; i < h.size(); i++) {
            Vector2 a = h[i], b = h[(i + 1) % h.size()];
            inkLine(a, b, 3.0f, kInk);
            DrawCircleV(a, 1.5f, kInk);
        }
    }
    if (gAnnaShape) {                                    // Anna: her face on the top jaw, her hat over it
        // Each eye on the jaw it is in, as his -- one too near the mouth's line moved off it, onto its own
        // side, wholly on her body, never in her mouth.
        const float s = kAnnaPlayS, big = g.eyeBig, er = 4.8f * s * (1 + 0.4f * big) + 1.5f;   // (an eye's round, ink and all, a little room)
        const Vector2 top = {bc.x, -kLeonH / 2 - kAnnaSeat + body.height / 2 * (1 - bk)};   // (her hat on her smaller top)
        for (int side = -1; side <= 1; side += 2) {
            Vector2 e = {bc.x + (g.facing * 2.75f + side * 7.15f) * s, bc.y - 4.2f * s};
            const float d = Vector2DotProduct(Vector2Subtract(e, mc), nrm), sd = d >= 0 ? 1.0f : -1.0f;
            if (fabsf(d) < er) e = Vector2Add(e, Vector2Scale(nrm, sd * (er - fabsf(d))));
            const Vector2 at = turn(e, sd * alpha);
            rlPushMatrix();
            rlTranslatef(at.x, at.y, 0);
            rlScalef(s, s * annaSquint(g), 1);
            drawEye({0, 0}, look, shut, big, (g.pulling || g.pushing) ? 0.5f : 0.0f);
            rlPopMatrix();
        }
        if (leonHat(g)) {
            float a = (Vector2DotProduct(Vector2Subtract(top, mc), nrm) >= 0) ? alpha : -alpha;
            annaHat(g, turn(top, a), a * RAD2DEG, true);
        }
        return;
    }
    for (int side = -1; side <= 1; side += 2) {
        Vector2 e = {bc.x + g.facing * 4.0f + side * 7.0f, body.y + 10};
        float a = (Vector2DotProduct(Vector2Subtract(e, bc), nrm) >= 0) ? alpha : -alpha;
        drawEye(turn(e, a), look, shut, g.eyeBig, (g.pulling || g.pushing) ? 0.5f : 0.0f);
    }
    if (leonHat(g)) {                                    // the hat rides on the top jaw
        Vector2 top = {bc.x, body.y - 1.5f - kBrimHalf + 0.5f};   // resting on the jaw's outline (3 px, centred)
        float a = (Vector2DotProduct(Vector2Subtract(top, bc), nrm) >= 0) ? alpha : -alpha;
        float hl, hr, curl;
        hatShape(g, kHatSeat - 1.5f, hl, hr, curl);   // (on the jaw's outline: 1.5 px lower)
        drawHat(turn(top, a), a * RAD2DEG, hl, hr, curl);
    }
}

// The tongue in the world: thick pink with an ink edge, a sticky blob at its tip.
static void drawTongue(const Game& g, Vector2 mouthW, Vector2 leonC) {
    if (g.tg == Game::tgNone) return;
    std::vector<Vector2> pts;
    Vector2 blob;
    Vector2 tipW = Vector2Add(leonC, Vector2Scale(g.tgDir, g.tgExt));
    if ((g.tg == Game::tgOut || g.tg == Game::tgBack) && !g.tgBnc.empty()) {   // bounced off ice: along its path
        pts = g.tonguePath(leonC);
        pts[0] = mouthW;
        blob = pts.back();
    } else if (g.tg == Game::tgDead) {                  // dead: limp, the way it hangs
        if (g.rope.size() < 2) return;
        pts = g.rope;
        pts[0] = mouthW;
        blob = pts.back();
    } else if (g.tg == Game::tgOut) {
        pts = {mouthW, tipW};
        blob = pts.back();
    } else if (g.tg == Game::tgBack && g.hatPutT >= 0) {
        // Putting the hat back on: bent, out of his mouth the way it points, curling over to its tip.
        Vector2 tip = g.hatPutTip(), k = Vector2Add(mouthW, Vector2Scale(g.tgDir, 0.7f * Vector2Distance(mouthW, tip)));
        const int n = 16;
        for (int i = 0; i <= n; i++) {
            float s = (float)i / n;
            pts.push_back(Vector2Lerp(Vector2Lerp(mouthW, k, s), Vector2Lerp(k, tip, s), s));
        }
        blob = tip;
    } else if (g.tg == Game::tgBack) {
        // Coming back as a whip: a wave runs along it, pinned at the mouth, fading as it shortens.
        const int n = 24;
        Vector2 axis = Vector2Subtract(tipW, mouthW);
        float len = Vector2Length(axis);
        Vector2 dir = (len > 0.01f) ? Vector2Scale(axis, 1 / len) : g.tgDir;
        Vector2 nrm = {-dir.y, dir.x};
        float amp = 9.0f * clampf(g.tgExt / 60.0f, 0, 1);
        for (int i = 0; i <= n; i++) {
            float sLen = len * i / n;
            float w = amp * sinf(2 * PI * sLen / 70.0f - g.whip * 45.0f) * sinf(PI * i / n * 0.5f + 0.0001f);
            pts.push_back(Vector2Add(Vector2Add(mouthW, Vector2Scale(dir, sLen)), Vector2Scale(nrm, w)));
        }
        blob = pts.back();
    } else {
        pts = g.piv;
        int exit = g.tongueExit();            // chain points inside the body are skipped
        for (int i = 1; i <= exit; i++) pts.push_back(g.rope[i]);
        pts.push_back(mouthW);
        blob = g.piv[0];
    }
    for (int pass = 0; pass < 2; pass++) {
        Color col = pass ? gTongue : kInk;
        float w = pass ? 4.4f : 7.4f;
        for (size_t i = 0; i + 1 < pts.size(); i++) inkLine(pts[i], pts[i + 1], w, col);
        for (auto& q : pts) DrawCircleV(q, w / 2, col);
    }
    DrawCircleV(blob, 5.8f, kInk);
    DrawCircleV(blob, 4.2f, gTongue);
}

// Knocked out: a square rolling on its corners, legs pulled in. The body is placed so that the
// lowest point of its rounded, inked outline rests exactly on the ground line at his feet.
static void drawKnocked(const Game& g, Vector2 p) {
    const float bw = kBodyW, bh = kBodyH;
    const float r = kBodyRound * std::min(bw, bh) / 2;          // corner radius
    float th = g.knockAngle * DEG2RAD;
    float hc = (bw / 2 - r) * fabsf(sinf(th)) + (bh / 2 - r) * fabsf(cosf(th)) + r + kOutline / 2;
    if (gAnnaShape) hc = sqrtf(powf(bw / 2 * sinf(th), 2) + powf(bh / 2 * cosf(th), 2));   // (Anna: round -- the ink on her outline)
    Vector2 c = {p.x, p.y - hc};

    rlPushMatrix();
    rlTranslatef(c.x, c.y, 0);
    rlRotatef(g.knockAngle, 0, 0, 1);
    drawBodyFace(g, {-bw / 2, -bh / 2, bw, bh}, 2);
    rlPopMatrix();
    if (g.knock == Game::kDown) dizzyStars(g, {c.x, c.y - bh / 2}, 1);
}

// Getting up: the legs unfold from under the body, knees bowed outwards in a funny stance,
// while the body bobs up past its height and settles, swaying. The knees straighten at the end
// into the normal stance; the eyes blink open twice.
static void drawGettingUp(const Game& g, Vector2 p, float tau) {
    const float bw = kBodyW, bh = kBodyH;
    const float T = g.t.knockRise;
    const float lying = kOutline / 2, standing = 13.0f;          // body bottom above the ground

    // Height of the body's bottom: a damped spring from lying to standing.
    float h = lying + (standing - lying) * (1 - expf(-tau / 0.13f) * cosf(2 * PI * 2.8f * tau));
    float sway = 9.0f * sinf(2 * PI * 2.2f * tau) * expf(-tau / 0.22f);
    float sy = 1 + clampf((h - standing) / (standing - lying), -0.3f, 0.3f) * 0.6f;
    float sx = 1 / sqrtf(sy);

    // Legs: two segments each, growing out of the body, knees pushed outwards; straight by the end.
    Vector2 base = {p.x, p.y - h};                               // bottom centre of the body
    float sa = sinf(sway * DEG2RAD), ca = cosf(sway * DEG2RAD);
    float grow = clampf(tau / 0.1f, 0, 1);
    float straighten = clampf((tau - (T - 0.12f)) / 0.12f, 0, 1);
    for (int side = -1; side <= 1 && !gEyesOnly; side += 2) {
        Vector2 hl = {side * 8.0f * sx, -1.0f};                  // hip in the body's frame
        if (gAnnaShape) {                                        // (Anna: closer, up on her round bottom's ink -- no gap)
            const float rx = bw / 2 - kOutline / 2, ry = bh / 2 - kOutline / 2, x = 6.0f;
            hl = {side * x * sx, (-bh / 2 + ry * sqrtf(1 - x * x / (rx * rx))) * sy};
        }
        Vector2 hip = {base.x + hl.x * ca - hl.y * sa, base.y + hl.x * sa + hl.y * ca};
        Vector2 foot = {p.x + side * (8.0f + 4.0f * (1 - straighten)), p.y - 2};
        float dx = foot.x - hip.x, dy = foot.y - hip.y;
        float d = sqrtf(dx * dx + dy * dy);
        float seg = lerpf(10.5f * grow, d / 2, straighten);
        if (2 * seg < d) {                                       // not long enough yet: dangling
            foot = {hip.x + dx / d * 2 * seg, hip.y + dy / d * 2 * seg};
            d = 2 * seg;
        }
        float out = sqrtf(fmaxf(0, seg * seg - d * d / 4));
        Vector2 n = {dy / fmaxf(d, 0.001f), -dx / fmaxf(d, 0.001f)};   // perpendicular...
        if (n.x * side < 0) n = {-n.x, -n.y};                    // ...on the outer side
        Vector2 knee = {(hip.x + foot.x) / 2 + n.x * out, (hip.y + foot.y) / 2 + n.y * out};
        const float th = gAnnaShape ? 2.6f : 3.2f;              // (Anna: her thinner legs, smaller feet -- her thighs up into her)
        Vector2 thigh = hip;
        if (gAnnaShape) { Vector2 a = Vector2Subtract(hip, knee); float l = Vector2Length(a); if (l > 0.001f) thigh = Vector2Add(hip, Vector2Scale(a, 2.5f / l)); }
        inkLine(thigh, knee, th, kInk);
        inkLine(knee, foot, th, kInk);
        DrawCircleV(knee, th / 2, kInk);
        DrawEllipse((int)(foot.x + g.facing * 2.5f), (int)foot.y, (gAnnaShape ? 4.2f : 5) * grow, (gAnnaShape ? 2.2f : 2.6f) * grow, kInk);
    }

    // Eyes: shut, open, shut, open.
    int eyes = (tau < 0.10f || (tau > 0.19f && tau < 0.26f)) ? 1 : 0;
    rlPushMatrix();
    rlTranslatef(base.x, base.y, 0);
    rlRotatef(sway, 0, 0, 1);
    rlScalef(sx, sy, 1);
    drawBodyFace(g, {-bw / 2, -bh, bw, bh}, eyes);
    rlPopMatrix();
    dizzyStars(g, {p.x, base.y - bh}, g.starsAlpha());
}

// A leg, two segments of `seg` each from hip to foot: straight when the foot is that far (or
// farther: stretched), else the knee comes out towards `bend` -- by `amt` of what the length asks.
// (`ext`: the thigh drawn that much further, past the hip -- up into a round body, no gap where it curves.)
static void drawLeg(Vector2 hip, Vector2 foot, float seg, Vector2 bend, float amt, float th, float ext) {
    float d = Vector2Distance(hip, foot);
    float out = amt * sqrtf(fmaxf(0, seg * seg - d * d / 4));
    auto top = [&](Vector2 to) {                         // (the hip, or past it, away from `to`)
        if (ext <= 0) return hip;
        Vector2 a = Vector2Subtract(hip, to);
        float l = Vector2Length(a);
        return l > 0.001f ? Vector2Add(hip, Vector2Scale(a, ext / l)) : hip;
    };
    if (out < 0.05f) { inkLine(top(foot), foot, th, kInk); return; }
    Vector2 u = Vector2Scale(Vector2Subtract(foot, hip), 1 / fmaxf(d, 0.001f));
    Vector2 n = {-u.y, u.x};
    if (Vector2DotProduct(n, bend) < 0) n = Vector2Negate(n);
    Vector2 knee = Vector2Add(Vector2Lerp(hip, foot, 0.5f), Vector2Scale(n, out));
    inkLine(top(knee), knee, th, kInk);
    inkLine(knee, foot, th, kInk);
    DrawCircleV(knee, th / 2, kInk);
}

// Anna's hat, playing -- the tower's cone (52 px, 25 wide) in four bands from the brim up, a
// checkerboard: each band split down its middle, the last key's colour (black before any: gHatBand) on
// one side, white (gHatWhite) on the other, the other way round from band to band.
// In the tower's measures, kAnnaPlayS of them: `base` the brim's middle (her frame), `deg` its turn;
// `h` each band's height (1 whole, 0 flat: Game::annaBands) -- folding, a band's sides fold out at its
// middle, a bellows' pleat, a thin crease across; flat, a line of ink --; `lean` how far the cone bends
// over (degrees at its tip, + to the right: each band turned a quarter of it more than the one under
// it), `tip` how far it is tipped over besides (degrees, the whole cone above its base -- the base flat
// on the brim); `curlL` / `curlR` its brim's curls, left and right (1 as drawn, 0 unrolled flat: the ending's
// party horn, more: tighter); `press` how high
// its curls stand (1 as drawn, 0 pressed flat: a bump, as his brim goes flat -- a little more ringing
// back), `rise` the most they may (tower pixels: a ceiling); `reachL` / `reachR` how far the brim may
// reach each way (a wall: its end bends up, along it, as far as it must -- the curls on it).
static void drawAnnaHat(Vector2 base, float deg, const float h[4], float lean, float tip, float curlL, float curlR, float press, float rise, float reachL,
                        float reachR, float bs) {
    const float H = 52, W = 25, q = H / 4, brimW = 29 * bs;
    rlPushMatrix();
    rlTranslatef(base.x, base.y, 0);
    rlRotatef(deg, 0, 0, 1);
    rlScalef(kAnnaPlayS, kAnnaPlayS, 1);
    Vector2 P = {0, 0};                                  // (up its middle line: the band's bottom)
    bool any = false;
    for (int k = 0; k < 4; k++) {
        // (its base flat on the brim whatever the tip: neither corner sunk into her nor lifted off it --
        // the tip turns it from its first band up)
        const float a0 = ((k ? tip : 0) + lean * k / 4) * DEG2RAD, a1 = (tip + lean * (k + 1) / 4) * DEG2RAD, am = (a0 + a1) / 2;
        const Vector2 n0 = {cosf(a0), sinf(a0)}, n1 = {cosf(a1), sinf(a1)}, nm = {cosf(am), sinf(am)};
        const float w0 = W / 2 * (1 - k / 4.0f), w1 = W / 2 * (1 - (k + 1) / 4.0f), hk = clampf(h[k], 0, 1.2f) * q;
        const Vector2 Q = Vector2Add(P, Vector2Scale({sinf(am), -cosf(am)}, hk));   // (its top)
        const Vector2 L0 = Vector2Subtract(P, Vector2Scale(n0, w0)), R0 = Vector2Add(P, Vector2Scale(n0, w0));
        if (hk < 0.05f) {                                // flat: a line of ink, the band folded away
            inkLine(L0, R0, 1.6f, kInk);
            continue;
        }
        any = true;
        // (A checkerboard: each band split down its middle line -- colour on one side, white on the
        // other, the other way round on the next band.)
        const Color cl = k % 2 == 0 ? gHatBand : gHatWhite, cr = k % 2 == 0 ? gHatWhite : gHatBand;
        const float amp = 3.5f * sqrtf(fmaxf(0, 1 - h[k] * h[k])), wm = (w0 + w1) / 2;
        const Vector2 M = Vector2Lerp(P, Q, 0.5f);
        const Vector2 L1 = Vector2Subtract(Q, Vector2Scale(n1, w1)), R1 = Vector2Add(Q, Vector2Scale(n1, w1));
        const Vector2 Lm = Vector2Subtract(M, Vector2Scale(nm, wm + amp)), Rm = Vector2Add(M, Vector2Scale(nm, wm + amp));
        auto tri = [](Vector2 a, Vector2 b, Vector2 c, Color col) {   // (either winding: the one raylib shows)
            if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) < 0) DrawTriangle(a, b, c, col); else DrawTriangle(a, c, b, col);
        };
        tri(P, L0, Lm, cl); tri(P, Lm, M, cl); tri(M, Lm, L1, cl); tri(M, L1, Q, cl);
        tri(P, R0, Rm, cr); tri(P, Rm, M, cr); tri(M, Rm, R1, cr); tri(M, R1, Q, cr);
        if (amp > 0.3f) inkLine(Lm, Rm, 0.7f, fade(kInk, fminf(1, amp / 2.5f) * 0.6f));
        for (int sd = -1; sd <= 1; sd += 2) {
            Vector2 a = sd < 0 ? L0 : R0, m = sd < 0 ? Lm : Rm, b = sd < 0 ? L1 : R1;
            inkLine(a, m, 2.1f, kInk);
            inkLine(m, b, 2.1f, kInk);
            DrawCircleV(m, 1.05f, kInk);
        }
        P = Q;
    }
    if (any && h[3] >= 0.05f) DrawCircleV(P, 1.05f, kInk);
    // The brim, each half from the middle: straight out, then its curl -- a line of points, each step
    // turned `curl` times as far as drawn. Then bent up, from a little short of the wall, as much as the
    // wall asks (the most it can: up along it, and over).
    const float step = 390.0f / 40 * DEG2RAD;
    for (int side = -1; side <= 1; side += 2) {
        const float curlS = side < 0 ? curlL : curlR;
        std::vector<Vector2> pts = {{0, 0}};
        for (int j = 1; j <= 8; j++) pts.push_back({brimW * j / 8, 0});
        float head = 0;
        for (int i = 0; i < 40; i++) {
            float rad = (11 - 7 * (i + 0.5f) / 40) * bs;
            head += curlS * step / 2;
            Vector2 b = pts.back();
            pts.push_back({b.x + cosf(head) * rad * step, b.y - sinf(head) * rad * step});
            head += curlS * step / 2;
        }
        const float reach = (side < 0 ? reachL : reachR) / kAnnaPlayS - 1.3f;
        auto bend = [&](float th, std::vector<Vector2>& out) {   // (past the pivot, turned up more and more over 8 px, then by th)
            out.assign(1, pts[0]);
            const float pivot = fmaxf(W / 2 + 2, reach - 7);
            float along = -1;                                    // (brim past the pivot so far: -1, not there yet)
            for (size_t i = 1; i < pts.size(); i++) {
                Vector2 d = Vector2Subtract(pts[i], pts[i - 1]);
                if (along < 0 && pts[i - 1].x >= pivot) along = 0;
                float a = along < 0 ? 0 : th * clampf(along / 8, 0, 1);
                if (along >= 0) along += Vector2Length(d);
                out.push_back(Vector2Add(out.back(), Vector2Rotate(d, -a)));
            }
        };
        auto widest = [](const std::vector<Vector2>& v) { float m = 0; for (Vector2 p : v) m = fmaxf(m, p.x); return m; };
        std::vector<Vector2> drawn;
        bend(0, drawn);
        if (widest(drawn) > reach) {
            float lo = 0, hi = 160 * DEG2RAD;
            for (int it = 0; it < 14; it++) {
                float mid = (lo + hi) / 2;
                bend(mid, drawn);
                (widest(drawn) > reach ? lo : hi) = mid;
            }
            bend(hi, drawn);
        }
        float top = 0;                                   // (pressed down: the curls' height times `press`, no higher than `rise`)
        for (Vector2 p : drawn) top = fmaxf(top, -p.y);
        const float k = fmaxf(0, top > 0.01f ? fminf(press, rise / top) : 1);
        for (Vector2& p : drawn) p.y *= k;
        for (size_t i = 1; i < drawn.size(); i++) {
            Vector2 a = {side * drawn[i - 1].x, drawn[i - 1].y}, b = {side * drawn[i].x, drawn[i].y};
            inkLine(a, b, 2.6f, kInk);
            DrawCircleV(b, 1.3f, kInk);
        }
    }
    rlPopMatrix();
}

// Anna's face, playing: as on the tower's top, kAnnaPlayS of its size, round her body's middle `c` (her
// frame) -- her eyes (`big`: popping, as his; `lid`: straining; `squint`: their height, narrowed in a
// dash), happy at the ending, her smile. `eyes`: 0 open, 1 shut, 2 crossed.
static void annaFace(const Game& g, Vector2 c, Vector2 look, int eyes, float big, float lid, float squint) {
    rlPushMatrix();
    rlTranslatef(c.x, c.y, 0);
    rlScalef(kAnnaPlayS, kAnnaPlayS, 1);
    for (int side = -1; side <= 1 && !gHatOnly; side += 2) {
        Vector2 e = {g.facing * 2.75f + side * 7.15f, -4.2f};
        if (eyes == 2) {
            const float k = 3.6f;
            inkLine({e.x - k, e.y - k}, {e.x + k, e.y + k}, 2.4f, kInk);
            inkLine({e.x - k, e.y + k}, {e.x + k, e.y - k}, 2.4f, kInk);
        } else if (g.endHappy() && eyes == 0) drawHappyEye(e);
        else {
            rlPushMatrix();
            rlTranslatef(e.x, e.y, 0);
            rlScalef(1, squint, 1);
            drawEye({0, 0}, look, eyes == 1, big, lid);
            rlPopMatrix();
        }
    }
    drawSmile({g.facing * 3.0f, 1.5f}, 6.5f, g.endSmile());
    rlPopMatrix();
}

// Her hat on her head: its brim's middle at `base` (her frame), turned `deg`. As folded as it is; `fit`:
// standing, also no taller than the room above her -- bent over first, back from where she goes
// (Game::hatLeanDir), as far as it must, up to 75 degrees at its tip; past that, its top bands folded,
// down to a line --, its curls flat under a ceiling, its brim bent up against a wall aside.
static void annaHat(const Game& g, Vector2 base, float deg, bool fit) {
    float h[4];
    g.annaBands(h);
    // (Whole, its curl is his brim's -- the ending's party horn, flapping in a long fall; bumped, his
    // brim's going flat and springing back is hers pressed flat and standing up again.)
    const bool whole = g.hatT < 0;
    float curl = whole ? g.hatCurl() : 1, press = whole ? 1 : g.hatCurl(), rise = 1e9f, reachL = 1e9f, reachR = 1e9f, lean = 0;
    if (fit) {
        float sy = 1 - clampf(g.squash, -0.4f, 0.6f);
        float room = (kLeonH + fminf(g.hatRoom, 1000.0f)) / sy - kLeonH - kAnnaSeat;   // from the brim up
        const float R = fmaxf(0, room) / kAnnaPlayS, q = 13;   // (in the tower's measures: a band 13 tall)
        auto band = [&](int k, float L) { return q * cosf(L * DEG2RAD * (k + 0.5f) / 4); };   // (band k's height upright, bent L)
        auto tall = [&](float L) { float y = 0; for (int k = 0; k < 4; k++) y += h[k] * band(k, L); return y; };
        if (tall(0) > R) {
            const float most = 75 * fabsf(g.hatLeanDir), side = g.hatLeanDir < 0 ? -1.0f : 1.0f;
            if (tall(most) <= R) {
                float lo = 0, hi = most;
                for (int it = 0; it < 14; it++) { float mid = (lo + hi) / 2; (tall(mid) > R ? lo : hi) = mid; }
                lean = side * hi;
            } else {                                     // (bent all it can: the top bands folded for the rest)
                lean = side * most;
                float left = R;
                for (int k = 0; k < 4; k++) { h[k] = fminf(h[k], fmaxf(0, left) / band(k, most)); left -= h[k] * band(k, most); }
            }
        }
        rise = fmaxf(0, room - kBrimHalf) / kAnnaPlayS;
        reachL = g.hatSideL; reachR = g.hatSideR;
    }
    drawAnnaHat(base, deg, h, lean, 0, curl, curl, press, rise, reachL, reachR, g.t.annaBrim);
}

// Anna in her dash (Game::dashPose, workshop/Anna dash.jpg), her feet's middle at p: her legs, from her
// stretched body's bottom, at their angle and bend -- planted on the ground when not far from upright;
// her body, stretched and tilted about its middle; her face slid forwards, squeezed with her body;
// her hat on her top, a little back, its cone tipped and bent, its brims curled as the pose has them.
static void drawAnnaDash(const Game& g, Vector2 p, const Game::DashPose& d) {
    const float dir = g.dashDir, rx = kBodyW / 2 * d.sx, ry = kBodyH / 2 * d.sy, L = 12;   // (L: her leg, upright, hip to foot)
    const Vector2 C = {0, -d.lift};
    rlPushMatrix();
    rlTranslatef(p.x, p.y, 0);
    for (int side = -1; side <= 1 && !gEyesOnly; side += 2) {
        const float hx = side * 6 * fminf(d.sx, 1.6f), ex = rx - kOutline / 2, ey = ry - kOutline / 2;
        const Vector2 hip = {C.x + hx, C.y + ey * sqrtf(fmaxf(0, 1 - hx * hx / (ex * ex)))};   // (on her bottom's ink)
        Vector2 foot = Vector2Add(hip, Vector2Scale(Vector2Rotate({0, 1}, dir * d.legA * DEG2RAD), L * (1 - 0.4f * d.legK)));
        if (g.onGround && fabsf(d.legA) < 60) foot.y = -2;    // (planted)
        drawLeg(hip, foot, L / 2 + 0.5f, {dir, 0}, 1, 2.6f, 2.5f);
        DrawEllipse((int)(foot.x + dir * 2.5f), (int)foot.y, 4.2f, 2.2f, kInk);
    }
    rlTranslatef(C.x, C.y, 0);                                 // (the rest about her body's middle, tilted)
    rlRotatef(dir * d.tilt, 0, 0, 1);
    if (!gEyesOnly) {
        DrawEllipseV({0, 0}, rx, ry, kInk);
        DrawEllipseV({0, 0}, rx - kOutline, ry - kOutline, gLeonBody);
    }
    const bool shut = g.eyesHeld ? !g.goldenRun : g.blink < 0 || g.stun > 0 || g.wakeShut() || g.dashBonkShut();
    rlPushMatrix();
    rlTranslatef(dir * d.face * rx, 0, 0);
    rlScalef(1, clampf(d.sy * 1.5f, 0.7f, 1), 1);
    annaFace(g, {0, 0}, {dir * 2.2f, d.lookDown * 1.6f}, shut ? 1 : 0, 0, 0, annaSquint(g));
    rlPopMatrix();
    if (leonHat(g)) {
        float h[4];
        g.annaBands(h);
        const float back = d.brimBack, front = d.brimFront;
        drawAnnaHat({-dir * 0.15f * rx, -ry + kOutline / 2 + 0.14f}, 0, h, dir * d.hat * 0.4f, dir * d.hat * 0.6f,   // (tipped over from its base, mostly; bent the rest)
                    dir > 0 ? back : front, dir > 0 ? front : back, 1, 1e9f, 1e9f, 1e9f, g.t.annaBrim);
    }
    rlPopMatrix();
}

// Leon, drawn in his own frame: origin at the middle of his feet, squash anchored at the feet,
// lean about the middle of the body.
static void drawLeonLocal(const Game& g, Vector2 p, Vector2 mouthU) {
    float tau = g.getUpTime();
    if (tau >= 0) { drawGettingUp(g, p, tau); return; }
    if (g.knock != Game::kNone) { drawKnocked(g, p); return; }
    if (gAnnaShape && g.mouth < 0.03f) {                 // Anna dashing: her own poses
        Game::DashPose dp;
        if (g.dashPose(dp)) { drawAnnaDash(g, p, dp); return; }
    }
    float sq = clampf(g.squash, -0.4f, 0.6f);
    float sx = 1 + sq * 0.8f, sy = 1 - sq;
    // The ending, swaying (Game::endSway): his body lowered just enough that neither leg -- its foot
    // planted -- has to stretch; the knees give, both towards Anna.
    float crouch = 0;
    if (g.endHappy()) {
        const float r = (g.tilt + flipAngle(g)) * DEG2RAD, hipY = -kLeonH / 2 - 2 + kBodyH - 1, footY = kLeonH / 2 - 2, L = footY - hipY;
        for (int side = -1; side <= 1; side += 2) {
            Vector2 hip = Vector2Rotate({side * 8.0f, hipY}, r);
            float dx = side * 8.0f - hip.x;
            crouch = fmaxf(crouch, footY - sqrtf(fmaxf(0, L * L - dx * dx)) - hip.y);
        }
    }
    rlPushMatrix();
    rlTranslatef(p.x, p.y, 0);
    rlScalef(sx, sy, 1);
    rlTranslatef(0, -kLeonH / 2 + crouch, 0);
    // Double-jump flip: one full turn, forwards, eased out.
    rlRotatef(g.tilt + flipAngle(g), 0, 0, 1);   // signed by velocity: forward on the ground, nose up in a leap

    const float bw = kBodyW, bh = kBodyH;
    const float bodyTop = -kLeonH / 2 - 2, bodyBot = bodyTop + bh;   // legs below, to the feet
    const float legLen = kLeonH / 2 - bodyBot;
    float speed = g.groundSpeed / g.t.runSpeed;

    // Legs. Standing (not walking), the feet stay where they would be with no lean -- on the
    // ground -- while the body leans or sways over them: the knees give, outwards. Falling for
    // fall_pedal_after, he pedals: each foot round a circle, the knees forwards.
    const float rot = g.tilt + flipAngle(g), restLen = kLeonH / 2 - 2 - (bodyBot - 1);   // hip to foot, standing
    const float plant = g.onGround ? 1 - clampf(g.gaitAmt, 0, 1) : 0;
    // A quarter turn begun on the ground (Game::turnStood): drawn standing on it till he turns -- his
    // speed along it, rising or falling in the new frame, neither tucks nor swings his legs yet.
    const float aloft = 1 - g.turnStood(g.time - g.t.playerTurnDelay);
    const float pedal = g.onGround ? 0 : clampf((g.fallT - g.t.pedalAfter) / 0.3f, 0, 1) * aloft;
    const float legOut = g.swingLegs();                  // (swinging on his tongue: each leg turned out from the body...)
    const bool hanging = g.tg == Game::tgStuck && !g.onGround;   // (...and in the air on it, each foot pointing out: the left one left)
    // His mouth open in the air, the body split in two jaws hinged at its back (drawOpenBody): each
    // hip rides on the half it is in, as the eyes do, its leg with it -- hanging from the body's
    // bottom as drawn.
    const bool jawed = g.mouth >= 0.03f && !g.onGround;
    const Vector2 bodyC = {0, bodyTop + bh / 2}, cutN = {-mouthU.y, mouthU.x};
    const Jaws jw = jaws({-bw / 2, bodyTop, bw, bh}, mouthU, g.mouth);
    for (int side = -1; side <= 1 && !gEyesOnly; side += 2) {
        float hipX = side * (gAnnaShape ? 6.0f : 8.0f);   // (Anna: closer, under her round body)
        float fx = hipX, fy = kLeonH / 2;
        if (g.onGround) {
            // Planted while down (moving back at the body's speed), then lifted and swung forward.
            float ph = fmodf(g.gait + (side > 0 ? 0.5f : 0.0f), 1.0f);
            float S = g.gaitStride, b = g.gaitStance, off, lift = 0;
            if (ph < b) off = S / 2 - S * (ph / b);
            else {
                float u = (ph - b) / (1 - b);
                off = -S / 2 + S * u * u * (3 - 2 * u);
                lift = sinf(PI * u) * lerpf(3.0f, 5.0f, clampf(speed, 0, 1));
            }
            fx += off * g.moveDir * g.gaitAmt;
            fy -= lift * g.gaitAmt;
            if (g.landFlash > 0) fx += side * 6.0f * g.landFlash;   // splayed after a hard landing
        } else if (g.vel.y < 0) {
            fy -= legLen * 0.45f * aloft;                              // tucked on the way up
            fx += side * 1.5f * aloft;
        } else {
            fx += side * 4.0f * clampf(g.vel.y / g.t.maxFall * 2, 0, 1) * aloft;   // dangling on the way down
        }
        // (At the ending, both knees towards Anna -- his facing: the far one inwards.)
        Vector2 hip = {hipX, bodyBot - 1}, foot = {fx, fy - 2}, bend = {g.endT >= 0 ? g.facing : (float)side, 0};
        if (jawed) {
            float a = Vector2DotProduct(Vector2Subtract(hip, bodyC), cutN) >= 0 ? jw.alpha : -jw.alpha;
            Vector2 by = Vector2Subtract(Vector2Add(jw.back, Vector2Rotate(Vector2Subtract(hip, jw.back), a)), hip);
            hip = Vector2Add(hip, by); foot = Vector2Add(foot, by);
        }
        if (legOut != 0) foot = Vector2Add(hip, Vector2Rotate(Vector2Subtract(foot, hip), -side * legOut * DEG2RAD));
        float knee = 0;
        if (plant > 0) {                                  // the foot un-turned by the lean: on the ground (under a crouch too)
            foot = Vector2Lerp(foot, Vector2Rotate({foot.x, foot.y - crouch}, -rot * DEG2RAD), plant);
            knee = plant;
        }
        float seg = restLen / 2;
        if (pedal > 0) {                                  // (the legs a little longer: a knee always bent, and below the body)
            float ph = (g.fallT - g.t.pedalAfter) * g.t.pedalRate * 2 * PI + (side > 0 ? PI : 0);
            Vector2 c = {hipX + g.facing * 3.0f, hip.y + restLen * 0.75f};   // the circle's centre, below the hip
            foot = Vector2Lerp(foot, {c.x + g.facing * 4.0f * cosf(ph), c.y + 4.0f * sinf(ph)}, pedal);
            seg = lerpf(seg, restLen * 0.6f, pedal);
            bend = {g.facing, 0};
            knee = pedal;
        }
        const float fw = gAnnaShape ? 4.2f : 5, fh = gAnnaShape ? 2.2f : 2.6f;   // (Anna: her thinner legs, her smaller feet)
        drawLeg(hip, foot, seg, bend, knee, gAnnaShape ? 2.6f : 3.2f, gAnnaShape ? 2.5f : 0);
        if (g.endT >= 0 && plant > 0) {                  // the ending: planted as Anna's -- flat on the ground, held still, not turned with his sway
            Vector2 toe = Vector2Add(foot, Vector2Rotate({g.facing * 2.5f, 0}, -rot * plant * DEG2RAD));
            rlPushMatrix();
            rlTranslatef(toe.x, toe.y, 0);
            rlRotatef(-rot * plant, 0, 0, 1);
            DrawEllipse(0, 0, fw, fh, kInk);
            rlPopMatrix();
        } else DrawEllipse((int)(foot.x + (hanging ? side : g.facing) * 2.5f), (int)foot.y, fw, fh, kInk);
    }

    // Eyes: pupils looking where he goes (and down when falling) -- or, with the tongue out,
    // along its first segment out of the mouth.
    float lookX = g.facing * 2.0f + clampf(g.vel.x / g.t.runSpeed, -1, 1) * 0.8f;
    float lookY = clampf(clampf(g.vel.y / 900.0f, -1, 1) * 1.8f + (g.look + g.farLook.y * 4.0f) * 2.2f, -2.2f, 2.2f);
    if (g.farLook.x != 0) lookX = clampf(g.farLook.x * 4.0f, -1, 1) * 2.2f;   // right stick: looking aside
    if (g.tg != Game::tgNone) { lookX = mouthU.x * 2.0f; lookY = mouthU.y * 2.0f; }
    if (g.hatOnTongue) {                                 // his hat on it: on the hat
        Vector2 d = g.toLocal(Vector2Subtract(g.hatP, g.centreW()));
        float L = Vector2Length(d);
        if (L > 1) { lookX = d.x / L * 2.2f; lookY = d.y / L * 2.2f; }
    }
    if (g.suicideT > 0) { lookX = g.pupil.x; lookY = g.pupil.y; }   // bouncing about in his eyes
    bool shut = g.eyesHeld ? !g.goldenRun : g.blink < 0 || g.stun > 0 || g.wakeShut() || (gAnnaShape && g.dashBonkShut());   // (3, 2, 1: shut till GO -- in a golden run, wide open -- Anna: a dash into a wall)
    Rectangle body = {-bw / 2, bodyTop, bw, bh};
    if (gAnnaShape && g.mouth < 0.03f) {                 // Anna: round, inked; her face; her hat on her head
        const Vector2 c = {0, bodyTop + bh / 2};
        if (!gEyesOnly) {
            DrawEllipseV(c, bw / 2, bh / 2, kInk);
            DrawEllipseV(c, bw / 2 - kOutline, bh / 2 - kOutline, gLeonBody);
        }
        annaFace(g, c, {lookX, lookY}, shut ? 1 : 0, g.eyeBig, g.pushing ? 0.5f : 0.0f, annaSquint(g));
        if (leonHat(g)) annaHat(g, {0, -kLeonH / 2 - kAnnaSeat}, 0, true);
    } else if (g.mouth < 0.03f) {
        // Body: a rounded cube -- with, if display_Leon_bar_decorator, the second edge on the
        // trailing side that the sketch has.
        if (!gEyesOnly) {
            DrawRectangleRounded(body, 0.28f, 8, gLeonBody);
            DrawRectangleRoundedLinesEx(body, 0.28f, 8, 3.0f, kInk);
        }
        if (g.t.barDecor && !gEyesOnly) {
            float edgeX = -g.facing * (bw / 2 - 5);
            inkLine({edgeX, bodyTop + 4}, {edgeX, bodyBot - 4}, 2.2f, kInk);
        }
        for (int side = -1; side <= 1; side += 2) {
            Vector2 e = {g.facing * 4.0f + side * 7.0f, bodyTop + 10};
            if (g.endHappy()) drawHappyEye(e);           // (the ending, happy: ^^)
            else drawEye(e, {lookX, lookY}, shut, g.eyeBig, g.pushing ? 0.5f : 0.0f);
        }
        drawSmile({g.facing * 4.0f, bodyTop + 15.5f}, 6.5f, g.endSmile());   // (the ending: his smile)
        if (leonHat(g)) {                                // resting on the outline (drawn outside the body)
            float hl, hr, curl;
            hatShape(g, kHatSeat, hl, hr, curl);
            drawHat({0, bodyTop - 3.0f - kBrimHalf + 0.5f}, 0, hl, hr, curl);
        }
    } else {
        drawOpenBody(g, body, mouthU, g.mouth, {lookX, lookY}, shut);
    }
    rlPopMatrix();
    dizzyStars(g, {p.x, p.y - kLeonH - 2}, g.starsAlpha());   // a get-up cut short: they circle on
}

// Leon in the world: his body centre at `centre`, turned by `angle` (he lags the world's turn).
// Trembling (suicide): shaken about, fast and at random (Perlin noise), a little turned too.
static void drawLeon(const Game& g, Vector2 centre, float angle, Vector2 mouthU) {
    rlPushMatrix();
    rlTranslatef(centre.x, centre.y, 0);
    rlRotatef(angle, 0, 0, 1);
    if (float a = g.tremble(); a > 0) {
        rlTranslatef(a * perlin1(g.time * 40), a * perlin1(g.time * 40 + 57.0f), 0);
        rlRotatef(a * 0.8f * perlin1(g.time * 33 + 113.0f), 0, 0, 1);
    }
    gHatWhite = gLeonHat;                     // (his hat on his head: his colour)
    drawLeonLocal(g, {0, kLeonH / 2}, mouthU);
    gHatWhite = kBody;
    rlPopMatrix();
}

// Flies: a black dot with two flickering wings on top -- 'top' being against gravity: they are
// drawn turned with the world (`angle`, the screen's turn), so they stay upright on screen.
// A golden one: gold, its wings pale, in a soft breathing glow. `which`: 0 all of them, 1 the
// ordinary ones, 2 the golden ones.
static void drawFlies(const Game& g, float angle, int which = 0) {
    for (const Fly& f : g.flies) {
        if (f.inMouth || !inView(f.p, 20)) continue;               // being chewed, or out of sight
        bool lit = f.golden || f.pole >= 0;                         // (a colour pole's: the same, in its colour)
        if ((which == 1 && lit) || (which == 2 && !lit)) continue;
        Color body = kGold, wing = kGoldWing, glow = kGoldGlow;
        if (f.pole >= 0) {
            body = g.t.colorOf[g.level.poleColour[f.pole]];
            wing = fade(paler(body, 0.3f), 0.8f);
            glow = fade(body, 0.45f);
        }
        float w = 2.2f + 1.4f * sinf(f.phase);
        if (lit) DrawCircleGradient(f.p, 12 + 2 * sinf(f.phase * 0.05f), glow, fade(glow, 0));
        rlPushMatrix();
        rlTranslatef(f.p.x, f.p.y, 0);
        rlRotatef(angle, 0, 0, 1);
        DrawEllipseV({-2.5f, -2}, 2.6f, w, lit ? wing : kWing);
        DrawEllipseV({2.5f, -2}, 2.6f, w, lit ? wing : kWing);
        if (lit) {                                                // a thin black line round it all
            DrawEllipseLinesV({-2.5f, -2}, 2.6f, w, kInk);
            DrawEllipseLinesV({2.5f, -2}, 2.6f, w, kInk);
        }
        DrawCircleV({0, 0}, 2.4f, lit ? body : kInk);
        if (lit) DrawCircleLinesV({0, 0}, 2.4f, kInk);
        rlPopMatrix();
    }
}

// Dust and sparks behind Leon; saliva (`drops`) in front of him, so a drool is never hidden.
// Blocks: a sketched crate -- an inset frame and a cross brace.
static void drawCrates(const Game& g) {
    for (size_t i = 0; i < g.crates.size(); i++) {
        Rectangle wr = g.crateRect(i);
        if (!inView(g.crates[i].c, wr.width)) continue;
        rlPushMatrix();
        rlTranslatef(g.crates[i].c.x + g.crates[i].visOff.x, g.crates[i].c.y + g.crates[i].visOff.y, 0);
        rlRotatef(g.crates[i].ang, 0, 0, 1);
        Rectangle r = {-wr.width / 2, -wr.height / 2, wr.width, wr.height};
        DrawRectangleRounded(r, 0.08f, 6, kCrate);
        Rectangle in = {r.x + 8, r.y + 8, r.width - 16, r.height - 16};
        hatch(in, 16.0f, 1.2f, fade(kInk, 0.18f));
        DrawRectangleLinesEx(in, 2.0f, fade(kInk, 0.8f));
        inkLine({in.x, in.y}, {in.x + in.width, in.y + in.height}, 3.0f, kInk);
        inkLine({in.x + in.width, in.y}, {in.x, in.y + in.height}, 3.0f, kInk);
        DrawRectangleRoundedLinesEx(r, 0.08f, 6, 3.0f, kInk);
        for (int k = 0; k < 4; k++)
            DrawCircleV({r.x + 4 + (k & 1) * (r.width - 8), r.y + 4 + (k >> 1) * (r.height - 8)}, 1.8f, kInk);
        rlPopMatrix();
    }
}

// Leon in pieces: 16 little cubes.
// The part of convex polygon p inside convex polygon c (the one cut by each side of the other).
static std::vector<Vector2> clipToConvex(std::vector<Vector2> p, const std::vector<Vector2>& c) {
    Vector2 mid{};
    for (Vector2 q : c) mid = Vector2Add(mid, Vector2Scale(q, 1.0f / c.size()));
    for (size_t i = 0; i < c.size() && p.size() >= 3; i++) {
        Vector2 a = c[i], b = c[(i + 1) % c.size()], n = {a.y - b.y, b.x - a.x};
        if (Vector2DotProduct(Vector2Subtract(mid, a), n) < 0) n = Vector2Negate(n);   // (pointing in)
        std::vector<Vector2> out;
        for (size_t j = 0; j < p.size(); j++) {
            Vector2 s = p[j], e = p[(j + 1) % p.size()];
            float ds = Vector2DotProduct(Vector2Subtract(s, a), n), de = Vector2DotProduct(Vector2Subtract(e, a), n);
            if (ds >= 0) out.push_back(s);
            if ((ds >= 0) != (de >= 0)) out.push_back(Vector2Lerp(s, e, ds / (ds - de)));
        }
        p = out;
    }
    return p;
}

// The span [t0, t1] of segment p -> q inside convex polygon c (false: none of it).
static bool segSpan(Vector2 p, Vector2 q, const std::vector<Vector2>& c, float& t0, float& t1) {
    Vector2 mid{}, d = Vector2Subtract(q, p);
    for (Vector2 x : c) mid = Vector2Add(mid, Vector2Scale(x, 1.0f / c.size()));
    t0 = 0; t1 = 1;
    for (size_t i = 0; i < c.size(); i++) {
        Vector2 a = c[i], b = c[(i + 1) % c.size()], n = {a.y - b.y, b.x - a.x};
        if (Vector2DotProduct(Vector2Subtract(mid, a), n) < 0) n = Vector2Negate(n);   // (pointing in)
        float num = Vector2DotProduct(Vector2Subtract(p, a), n), den = Vector2DotProduct(d, n);
        if (fabsf(den) < 1e-9f) { if (num < 0) return false; continue; }
        if (den > 0) t0 = fmaxf(t0, -num / den); else t1 = fminf(t1, -num / den);
        if (t0 > t1) return false;
    }
    return true;
}
// A teardrop: the convex hull of a circle (centre c, radius r) and a point p outside it.
static std::vector<Vector2> teardrop(Vector2 c, float r, Vector2 p, int n) {
    Vector2 d = Vector2Subtract(p, c);
    float D = Vector2Length(d), phi = atan2f(d.y, d.x), beta = D > r ? acosf(r / D) : 0;
    std::vector<Vector2> out;
    if (D > r) out.push_back(p);
    for (int q = 0; q <= n; q++) {
        float a = phi + beta + (2 * PI - 2 * beta) * q / n;
        out.push_back({c.x + r * cosf(a), c.y + r * sinf(a)});
    }
    return out;
}

// Bits of Leon and saliva in a Monstermunch's place show only in its mouth's blank space: drawn
// behind its face and teeth, cut to its mouth -- the V, opening on out (world; none while shut).
static int monsterAt(const Game& g, Vector2 p) {
    for (size_t i = 0; i < g.monsters.size(); i++) if (CheckCollisionPointRec(p, g.monsters[i].r)) return (int)i;
    return -1;
}
static std::vector<Vector2> mouthWedge(const Monster& m) {
    if (m.open <= 0.02f) return {};
    MouthV v = mouthV(m);
    return {m.toWorld(v.a), m.toWorld(Vector2Add(v.a, Vector2Scale(Vector2Subtract(v.e1, v.a), 50))),
            m.toWorld(Vector2Add(v.a, Vector2Scale(Vector2Subtract(v.e2, v.a), 50)))};
}
static void drawInMouth(const Game& g, int mi, std::vector<Vector2> poly, Color fill, Color ink, float th) {
    std::vector<Vector2> wedge = mouthWedge(g.monsters[mi]);
    if (wedge.empty()) return;
    poly = clipToConvex(poly, wedge);
    if (poly.size() < 3) return;
    DrawTriangleFan(poly.data(), (int)poly.size(), fill);
    for (size_t j = 0; j < poly.size(); j++) inkLine(poly[j], poly[(j + 1) % poly.size()], th, ink);
}

static void drawPieces(const Game& g, float alpha) {
    Color body = fade(g.bodyColour(), alpha), pink = fade(kTongue, alpha), red = fade(kBlood, alpha), ink = fade(kInk, alpha);
    for (const Piece& pc : g.pieces) {
        float s = pc.size > 0 ? pc.size : kBodyW / 4;
        if (!inView(pc.p, s)) continue;
        int mi = pc.size > 0 ? monsterAt(g, pc.p) : -1;
        if (mi >= 0) {                                   // a munch's bit in a Monstermunch's place: through its mouth only
            std::vector<Vector2> sq;
            for (Vector2 c : {Vector2{-1, -1}, Vector2{1, -1}, Vector2{1, 1}, Vector2{-1, 1}})
                sq.push_back(Vector2Add(pc.p, Vector2Rotate(Vector2Scale(c, s / 2), pc.a * DEG2RAD)));
            drawInMouth(g, mi, sq, pc.blood ? red : body, ink, 1.5f);
            continue;
        }
        rlPushMatrix();
        rlTranslatef(pc.p.x, pc.p.y, 0);
        rlRotatef(pc.a, 0, 0, 1);
        Rectangle r = {-s / 2, -s / 2, s, s};
        DrawRectangleRounded(r, 0.3f, 4, pc.blood ? red : pc.tongue ? pink : body);
        DrawRectangleRoundedLinesEx(r, 0.3f, 4, pc.size > 0 ? 1.5f : 2.0f, ink);
        rlPopMatrix();
    }
}

// A Monstermunch's numb tongue, s of the way along it (0 at its lips, 1 at its tip): its own pink
// turning (f: 0..1) to blotches of pink, red and purple -- each Monstermunch's (i) its own.
static Color numbColour(float s, float f, int i) {
    static const Color red = {196, 34, 52, 255}, purple = {124, 46, 138, 255};
    float a = 0.5f + 0.5f * perlin1(s * 3.1f + i * 5.7f), b = 0.5f + 0.5f * perlin1(s * 2.3f + 17.9f + i * 3.1f);
    return ColorLerp(kTongue, ColorLerp(ColorLerp(kTongue, red, a), purple, 0.85f * b), f);
}

// The Monstermunchs (Monster): a white face in the ground, ink round it, its corners rounded; on it a
// red heart with its eyes in it (workshop/monstermunch-love.jpg). Asleep: eyes shut, a z rising now
// and then. Awake: its eyes open on Leon, blinking. Its mouth: a V cut into the face from its open
// side -- empty, open to the air -- teeth along both sides, inside it; the heart cut in two by it, each
// half turned with its side; its tongue out of the V's point. Swallowed, Leon shows in it -- till the
// first munch: then he is gone.
// Two passes: behind (`front` false) the mouth's inside, him in it and the tongue -- then the blood
// cubes, popping out of its middle -- then in front the face round the mouth, the heart, the teeth.
// Drawn in its own frame: its middle at 0, its open face up -- turned to the world by its face.
static void drawMonsters(const Game& g, bool front) {
    for (size_t i = 0; i < g.monsters.size(); i++) {
        const Monster& m = g.monsters[i];
        if ((int)i != g.eatenBy && !inView(m.centre(), m.r.width + fmaxf(m.ext, m.ropeLen) + kTile)) continue;   // (face, tongue and all out of sight)
        float S = m.r.width, k = S / 80.0f;               // (drawn for 80 pixels, the 2 tiles)
        MouthV v = mouthV(m);
        Vector2 mc = m.centre(), ul = rotQ(-m.face, m.u);
        bool asleep = m.st == Monster::kSleep;
        bool mouthShows = m.open > 0.02f;
        // The V's two sides, and which way each one's outside is.
        Vector2 cen = {(v.e1.x + v.e2.x + v.a.x) / 3, (v.e1.y + v.e2.y + v.a.y) / 3};
        auto outward = [&](Vector2 p0, Vector2 p1) {
            Vector2 d = Vector2Subtract(p1, p0), n = Vector2Normalize({-d.y, d.x});
            if (Vector2DotProduct(Vector2Subtract(cen, p0), n) > 0) n = Vector2Negate(n);
            return n;
        };
        Vector2 n1 = mouthShows ? outward(v.e1, v.a) : Vector2{0, 0}, n2 = mouthShows ? outward(v.e2, v.a) : Vector2{0, 0};
        auto side = [&](Vector2 p, Vector2 n) { return Vector2DotProduct(Vector2Subtract(p, v.a), n); };   // > 0: out of the mouth that side
        // (Behind it, nothing: the ground is dug out round it -- its rounded corners and its open
        // mouth let through whatever lies behind.)
        Vector2 throat = m.toWorld(Vector2Add(v.a, Vector2Scale(Vector2Normalize(Vector2Subtract(v.f, v.a)), 11 * k)));   // (a tongue's root, in its mouth)
        if (!front && m.rope.size() >= 2) {                // (numb -- or, his own having caught it, eating him all the same)
            // Its numb tongue, one piece -- one contour all along: from its root inside the mouth (the
            // face, drawn over it, hides all but what lies in the V) out through its lips, lying out
            // of them (world).
            float f = clampf(m.numbT / 0.5f, 0, 1);             // (turning, over half a second)
            std::vector<Vector2> pts{throat};
            pts.insert(pts.end(), m.rope.begin(), m.rope.end());
            int n = (int)pts.size(), nr = (int)m.rope.size();
            auto along = [&](int j) { return j == 0 ? 0.0f : (float)(j - 1) / (nr - 1); };   // (0 at its lips, 1 at its tip)
            for (int pass = 0; pass < 2; pass++) {
                for (int j = 0; j < n; j++) {
                    float s = along(j), w = (lerpf(13, 7, s) + (pass ? 0 : 5)) * k;
                    Color col = pass ? numbColour(s, f, (int)i) : kInk;
                    if (j + 1 < n) inkLine(pts[j], pts[j + 1], w, col);
                    DrawCircleV(pts[j], w / 2, col);
                }
                DrawCircleV(pts.back(), (pass ? 6.0f : 8.5f) * k, pass ? numbColour(1, f, (int)i) : kInk);
            }
        }
        if (!front && (int)i == g.eatenBy && g.eatRope.size() >= 2) {
            // Leon's tongue, going down with him: as drawn out of his mouth (drawTongue), from it -- he
            // is drawn over it -- then, once he is in, from its root in the mouth, one piece out
            // through its lips (world).
            std::vector<Vector2> pts;
            if (m.st != Monster::kPull) pts.push_back(throat);
            pts.insert(pts.end(), g.eatRope.begin(), g.eatRope.end());
            for (int pass = 0; pass < 2; pass++) {
                Color col = pass ? gTongue : kInk;
                float w = pass ? 4.4f : 2 * kTongueHalfW;
                for (size_t j = 0; j + 1 < pts.size(); j++) inkLine(pts[j], pts[j + 1], w, col);
                for (Vector2 q : pts) DrawCircleV(q, w / 2, col);
            }
            DrawCircleV(pts.back(), 5.8f, kInk);
            DrawCircleV(pts.back(), 4.2f, gTongue);
        }
        auto tube = [&](Vector2 from, Vector2 to, float w0, float w1, Color col) {   // (a tapering stretch of tongue)
            Vector2 d = Vector2Normalize(Vector2Subtract(to, from)), nn = {-d.y, d.x};
            Vector2 q[4] = {Vector2Add(from, Vector2Scale(nn, w0 / 2)), Vector2Subtract(from, Vector2Scale(nn, w0 / 2)),
                            Vector2Subtract(to, Vector2Scale(nn, w1 / 2)), Vector2Add(to, Vector2Scale(nn, w1 / 2))};
            DrawTriangleFan(q, 4, col);
        };
        if (!front && m.ext > 0 && !m.bnc.empty()) {
            // Thrown, and bounced off ice: along its way, round each bounce -- tapering, ink-edged, a
            // blob at the tip (world; from its root in the mouth, the face drawn over it).
            std::vector<Vector2> pts{m.toWorld(Vector2Add(v.a, Vector2Scale(ul, 11 * k)))};
            pts.insert(pts.end(), m.bnc.begin(), m.bnc.end());
            pts.push_back(m.tongueAt(m.ext));
            float total = 0;
            for (size_t j = 0; j + 1 < pts.size(); j++) total += Vector2Distance(pts[j], pts[j + 1]);
            for (int pass = 0; pass < 2; pass++) {
                Color col = pass ? kTongue : kInk;
                auto width = [&](float at) { return (lerpf(13, 7, at / fmaxf(1, total)) + (pass ? 0 : 5)) * k; };
                float at = 0;
                for (size_t j = 0; j + 1 < pts.size(); j++) {
                    float L = Vector2Distance(pts[j], pts[j + 1]);
                    tube(pts[j], pts[j + 1], width(at), width(at + L), col);
                    if (j > 0) DrawCircleV(pts[j], width(at) / 2, col);   // (round at each bounce)
                    at += L;
                }
                DrawCircleV(pts.back(), (pass ? 6.0f : 8.5f) * k, col);
            }
        }
        rlPushMatrix();
        rlTranslatef(mc.x, mc.y, 0);
        rlRotatef(90.0f * m.face, 0, 0, 1);
        if (!front) {
            if (mouthShows) {                            // (empty: nothing drawn in it -- what lies behind shows)
                if (g.eatenBy == (int)i && m.st == Monster::kMunch && m.munched == 0)   // him in it, till the first munch: his own size,
                    drawBodyFace(g, {cen.x - kBodyW / 2, cen.y - kBodyH / 2, kBodyW, kBodyH}, 2);   // eyes crossed (the face hides the rest of him)
            }
            // The tongue, a tube out of its throat: one piece from just inside the face's far border (its
            // base clear of it) -- the face and the teeth, drawn over it, hide all but what lies in the
            // mouth -- and on out of it, whole. (Bounced off ice: drawn above, along its way.)
            if (m.ext > 0 && m.bnc.empty()) {                    // thrown: tapering, ink-edged, a blob at the tip
                Vector2 tip = Vector2Scale(ul, m.ext), root = Vector2Add(v.a, Vector2Scale(ul, 11 * k));
                for (int pass = 0; pass < 2; pass++) {
                    Color col = pass ? kTongue : kInk;
                    tube(root, tip, (pass ? 13.0f : 18.0f) * k, (pass ? 7.0f : 12.0f) * k, col);
                    DrawCircleV(tip, (pass ? 6.0f : 8.5f) * k, col);
                }
            }
            rlPopMatrix();
            continue;
        }
        // The face: the rounded square -- open, less the V: what lies outside each of its sides.
        // Its white (its teeth's too; not its eyes', as his) the colour of a fly in his belly as it
        // munches him: coming in with the munches, whole by the last, till the fly gets out -- then
        // white again over monstermunch_color_fade_time.
        Color white = kBody;
        if ((int)i == g.eatenBy && g.bellyFly >= 0 && m.st >= Monster::kMunch)
            white = ColorLerp(kBody, g.t.colorOf[g.bellyColour()], m.st == Monster::kChew ? 1 : clampf(m.t / g.t.mmMunchT, 0, 1));
        else if (m.tintCol >= 0 && g.time - m.tintAt < g.t.mmColorFade)
            white = ColorLerp(kBody, g.t.colorOf[m.tintCol], 1 - (g.time - m.tintAt) / g.t.mmColorFade);
        std::vector<Vector2> square = roundedSquare(S / 2, S / 10);
        if (!mouthShows) DrawTriangleFan(square.data(), (int)square.size(), white);
        else
            for (Vector2 n : {n1, n2}) {                         // (each a convex piece: the square cut by a line)
                std::vector<Vector2> piece;
                for (size_t j = 0; j < square.size(); j++) {
                    Vector2 p = square[j], q = square[(j + 1) % square.size()];
                    float dp = side(p, n), dq = side(q, n);
                    if (dp >= 0) piece.push_back(p);
                    if ((dp >= 0) != (dq >= 0)) piece.push_back(Vector2Lerp(p, q, dp / (dp - dq)));
                }
                if (piece.size() >= 3) DrawTriangleFan(piece.data(), (int)piece.size(), white);
            }
        // The motif, its eyes in it (workshop/monstermunch-love.jpg): a playing card's suit, each
        // Monstermunch its own (monstermunch_suits, in map order) -- the heart and the diamond red, the
        // spade and the club black, their eyes' lines red. Made of convex pieces, inked round their
        // union: the heart two teardrops (each lobe with the tip); the spade the same upside down, on a
        // stem; the club three discs and the triangle between them, on a stem; the diamond a square on its corner.
        // The mouth open, it is cut in two down the V's middle, each half turned about the V's point
        // with its side of the mouth, its eye riding on it.
        int suit = g.t.mmSuits[i % g.t.mmSuits.size()];
        bool black = suit == 1 || suit == 2;
        Color fill = black ? kSuitBlack : kHeart, eyeInk = black ? kHeart : kInk;
        float H = g.t.mmHeartScale, W = 0.62f * S * H, rl = 0.27f * W;
        std::vector<std::vector<Vector2>> pieces;
        Vector2 eyeAt[2];
        if (suit == 0) {                                         // the heart: lobes up, tip down
            float yl = -0.10f * S * H;
            Vector2 tip = {0, yl - rl + 0.92f * W};
            for (int j = 0; j < 2; j++) {
                Vector2 lobe = {(j ? 1 : -1) * (W / 2 - rl), yl};
                pieces.push_back(teardrop(lobe, rl, tip, 20));
                eyeAt[j] = {lobe.x * 0.95f, yl + 0.30f * rl};
            }
        } else if (suit == 1) {                                  // the spade: lobes down, tip up, a stem under them
            float stem = 0.10f * S * H, ylo = (0.92f * W - stem) / 2 - rl;
            Vector2 tip = {0, ylo + rl - 0.92f * W};
            for (int j = 0; j < 2; j++) {
                Vector2 lobe = {(j ? 1 : -1) * (W / 2 - rl), ylo};
                pieces.push_back(teardrop(lobe, rl, tip, 20));
                eyeAt[j] = {lobe.x * 0.95f, ylo - 0.15f * rl};
            }
            pieces.push_back({{0, ylo}, {0.16f * W, ylo + rl + stem}, {-0.16f * W, ylo + rl + stem}});
        } else if (suit == 2) {                                  // the club: a disc on top, two below, a stem
            float cr = 0.25f * W, yb = 0.03f * S * H, yt = yb - 0.24f * S * H;
            Vector2 c[3] = {{0, yt}, {-0.27f * W, yb}, {0.27f * W, yb}};
            for (Vector2 q : c) {
                std::vector<Vector2> d;
                for (int j = 0; j < 24; j++) d.push_back({q.x + cr * cosf(2 * PI * j / 24), q.y + cr * sinf(2 * PI * j / 24)});
                pieces.push_back(d);
            }
            pieces.push_back({c[0], c[1], c[2]});
            pieces.push_back({{0, yb}, {0.14f * W, yb + 0.30f * S * H}, {-0.14f * W, yb + 0.30f * S * H}});
            for (int j = 0; j < 2; j++) eyeAt[j] = {c[1 + j].x * 0.95f, yb - 0.05f * cr};
        } else {                                                 // the diamond: a square turned 45 degrees
            float w = 1.1f * 0.55f * S * H / 2;
            pieces.push_back({{0, -w}, {w, 0}, {0, w}, {-w, 0}});
            for (int j = 0; j < 2; j++) eyeAt[j] = {(j ? 1 : -1) * 0.42f * w, 0};
        }
        // The cut, and towards e1's side -- told by e1 from e2 (f between them): thrown steeply, f is
        // held at the face's corner and e1 sits on it, on the cut itself, so e1 alone cannot tell.
        Vector2 ax = Vector2Normalize(Vector2Subtract(v.f, v.a)), nA = {-ax.y, ax.x};
        if (Vector2DotProduct(Vector2Subtract(v.e1, v.e2), nA) < 0) nA = Vector2Negate(nA);
        auto turnTo = [&](Vector2 e) {
            Vector2 d = Vector2Normalize(Vector2Subtract(e, v.a));
            return atan2f(ax.x * d.y - ax.y * d.x, Vector2DotProduct(ax, d));
        };
        float turn[2] = {mouthShows ? turnTo(v.e1) : 0, mouthShows ? turnTo(v.e2) : 0};
        auto turned = [&](Vector2 p, int h) { return Vector2Add(v.a, Vector2Rotate(Vector2Subtract(p, v.a), turn[h])); };
        auto sideOfCut = [&](Vector2 p, int h) { return Vector2DotProduct(Vector2Subtract(p, v.a), h == 0 ? nA : Vector2Negate(nA)); };
        int halves = mouthShows ? 2 : 1;
        for (int h = 0; h < halves; h++)
            for (auto& d : pieces) {
                std::vector<Vector2> piece;
                for (size_t q = 0; q < d.size(); q++) {          // (its share of each piece: cut, turned, in the face)
                    Vector2 p = d[q], p2 = d[(q + 1) % d.size()];
                    float dp = halves == 1 ? 1 : sideOfCut(p, h), dq = halves == 1 ? 1 : sideOfCut(p2, h);
                    if (dp >= 0) piece.push_back(p);
                    if ((dp >= 0) != (dq >= 0)) piece.push_back(Vector2Lerp(p, p2, dp / (dp - dq)));
                }
                for (Vector2& p : piece) p = turned(p, h);
                piece = clipToConvex(piece, square);
                if (piece.size() >= 3) DrawTriangleFan(piece.data(), (int)piece.size(), fill);
            }
        auto motifInk = [&](Vector2 x0, Vector2 x1) {          // a stretch of its outline: each half's share of it
            for (int h = 0; h < halves; h++) {
                Vector2 p = x0, q = x1;
                if (halves == 2) {
                    float dp = sideOfCut(p, h), dq = sideOfCut(q, h);
                    if (dp < 0 && dq < 0) continue;
                    if (dp < 0) p = Vector2Lerp(p, q, dp / (dp - dq));
                    else if (dq < 0) q = Vector2Lerp(p, q, dp / (dp - dq));
                }
                p = turned(p, h); q = turned(q, h);
                float t0, t1;
                if (!segSpan(p, q, square, t0, t1)) continue;
                Vector2 a0 = Vector2Lerp(p, q, t0), a1 = Vector2Lerp(p, q, t1);
                inkLine(a0, a1, 2.4f * k, kInk);
                DrawCircleV(a0, 1.2f * k, kInk); DrawCircleV(a1, 1.2f * k, kInk);
            }
        };
        for (size_t a = 0; a < pieces.size(); a++)               // (each piece's edge, where it is outside all the others)
            for (size_t q = 0; q < pieces[a].size(); q++) {
                Vector2 p = pieces[a][q], p2 = pieces[a][(q + 1) % pieces[a].size()];
                std::vector<std::pair<float, float>> in;
                for (size_t b = 0; b < pieces.size(); b++) {
                    float t0, t1;
                    if (b != a && segSpan(p, p2, pieces[b], t0, t1)) in.push_back({t0, t1});
                }
                std::sort(in.begin(), in.end());
                float at = 0;
                for (auto [t0, t1] : in) {
                    if (t0 > at) motifInk(Vector2Lerp(p, p2, at), Vector2Lerp(p, p2, t0));
                    at = fmaxf(at, t1);
                }
                if (at < 1) motifInk(Vector2Lerp(p, p2, at), p2);
            }
        // The eyes: each round, drawn out to a point at its inner corner, a little low; white, a
        // yellow retina in it with its black pupil, following Leon -- each eye aims at him from where
        // it is, its retina as far as 0.6 of the eye's radius that way, what passes the eye's edge
        // hidden under its rim. Each its own twitch; both blinking (the lids closing on them and
        // opening again); asleep, shut.
        float R = 0.5f * rl * g.t.mmEyeScale, eh = 2 * R;       // (eh: an open eye's height)
        float rr = g.t.mmRetinaR * kTile * k;
        bool hasHim = g.eatenBy == (int)i && m.st >= Monster::kMunch;   // (him in its mouth: its eyes on him there, crossed)
        float heed = hasHim ? 1 : fminf(1, Vector2Length(m.look));   // (awake, its eyes on him: 1)
        float lid = asleep ? 0 : m.blinkT >= 0 ? 1 - sinf(PI * clampf(m.blinkT / g.t.mmBlinkT, 0, 1)) : 1;   // (how open)
        Vector2 leonL = hasHim ? cen : rotQ(-m.face, Vector2Subtract(g.centreW(), mc));
        auto ellipse = [](Vector2 c, float rx, float ry, int n) {
            std::vector<Vector2> p;
            for (int q = 0; q < n; q++) p.push_back({c.x + rx * cosf(2 * PI * q / n), c.y + ry * sinf(2 * PI * q / n)});
            return p;
        };
        for (int sd = -1; sd <= 1; sd += 2) {
            int j = sd < 0 ? 0 : 1;                              // (its left eye, its right eye)
            Vector2 c0 = eyeAt[j];
            int h = halves == 2 && sideOfCut(c0, 0) < 0 ? 1 : 0;  // (the half it sits in)
            c0.y += spasmWave(m.spasmT[j]) * g.t.mmSpasmA[j] * eh;
            Vector2 c = turned(c0, h);
            float ang = turn[h];
            auto place = [&](Vector2 p) { return Vector2Add(c, Vector2Rotate(p, ang)); };
            Vector2 P = {-sd * 1.35f * R, 0.45f * R};             // (its inner corner)
            if (lid < 0.12f) {                                   // shut: a line, out to its inner corner
                Vector2 a0 = place({sd * R, 0}), a1 = place({P.x, 0.3f * R});
                inkLine(a0, a1, 2.5f * k, eyeInk);
                DrawCircleV(a0, 1.25f * k, eyeInk); DrawCircleV(a1, 1.25f * k, eyeInk);
                continue;
            }
            std::vector<Vector2> eye = teardrop({0, 0}, R, P, 20);
            for (Vector2& p : eye) p = place({p.x, p.y * lid});
            DrawTriangleFan(eye.data(), (int)eye.size(), kBody);
            Vector2 to = Vector2Subtract(leonL, c);
            float L = Vector2Length(to);
            Vector2 pc = L > 1 ? Vector2Add(c, Vector2Scale(to, heed * 0.6f * R / L)) : c;
            std::vector<Vector2> ret = clipToConvex(ellipse(pc, rr, rr, 24), eye);
            if (ret.size() >= 3) {
                DrawTriangleFan(ret.data(), (int)ret.size(), kRetina);
                for (size_t q = 0; q < ret.size(); q++) inkLine(ret[q], ret[(q + 1) % ret.size()], 1.2f * k, eyeInk);
            }
            std::vector<Vector2> pup = clipToConvex(ellipse(pc, rr * g.t.mmPupil, rr * g.t.mmPupil, 16), eye);
            if (g.t.mmPupil > 0 && pup.size() >= 3) DrawTriangleFan(pup.data(), (int)pup.size(), kInk);
            for (size_t q = 0; q < eye.size(); q++) {
                inkLine(eye[q], eye[(q + 1) % eye.size()], 1.8f * k, eyeInk);
                DrawCircleV(eye[q], 0.9f * k, eyeInk);
            }
        }
        if (mouthShows) {
            // The teeth: a row along each side, attached to it, full size, pointing in -- the second
            // row half a tooth on from the first (clenched, a saw). What of a tooth passes the other
            // side is behind that half of the face: only what lies in the mouth shows.
            auto inMouth = [&](std::vector<Vector2> poly, Vector2 n) {   // the part on the mouth's side of a side (n: its outside)
                std::vector<Vector2> out;
                for (size_t q = 0; q < poly.size(); q++) {
                    Vector2 p = poly[q], p2 = poly[(q + 1) % poly.size()];
                    float dp = side(p, n), dq = side(p2, n);
                    if (dp <= 0) out.push_back(p);
                    if ((dp <= 0) != (dq <= 0)) out.push_back(Vector2Lerp(p, p2, dp / (dp - dq)));
                }
                return out;
            };
            auto inkInMouth = [&](Vector2 p, Vector2 q, Vector2 n) {
                float dp = side(p, n), dq = side(q, n);
                if (dp > 0 && dq > 0) return;
                if (dp > 0) p = Vector2Lerp(p, q, dp / (dp - dq));
                else if (dq > 0) q = Vector2Lerp(p, q, dp / (dp - dq));
                inkLine(p, q, 1.6f, kInk);
            };
            const int teeth = 4;
            for (int sd = 0; sd < 2; sd++) {
                Vector2 p0 = sd ? v.e2 : v.e1, n = sd ? n2 : n1, other = sd ? n1 : n2;
                Vector2 d = Vector2Subtract(v.a, p0);
                float L = Vector2Length(d);
                if (L < 6) continue;
                float h = fminf(9 * k, L / teeth * 0.9f), shift = sd ? 0.5f : 0.0f;
                for (int j = 0; j < teeth - sd; j++) {          // (the second row: one fewer, between the first's)
                    Vector2 b0 = Vector2Add(p0, Vector2Scale(d, (j + shift) / teeth)), b1 = Vector2Add(p0, Vector2Scale(d, (j + shift + 1) / teeth));
                    Vector2 tipT = Vector2Subtract(Vector2Lerp(b0, b1, 0.5f), Vector2Scale(n, h));
                    std::vector<Vector2> tooth = inMouth({b0, b1, tipT}, other);
                    if (tooth.size() >= 3) DrawTriangleFan(tooth.data(), (int)tooth.size(), white);
                    inkInMouth(b0, tipT, other); inkInMouth(tipT, b1, other);
                }
            }
            inkLine(v.e1, v.a, 3, kInk);
            inkLine(v.e2, v.a, 3, kInk);
        }
        // Ink all round the face, its corners rounded -- its open side's straight edge too, but
        // not across the mouth.
        const float th = 3;
        for (size_t j = 0; j + 1 < square.size(); j++) {        // (all but the last edge: the open face's straight one)
            inkLine(square[j], square[j + 1], th, kInk);
            DrawCircleV(square[j + 1], th / 2, kInk);
        }
        Vector2 tl = square.back(), tr = square.front();         // (that edge's ends, where its corners' arcs begin)
        if (mouthShows) {
            if (v.e1.x > tl.x + 0.5f) inkLine(tl, v.e1, th, kInk);
            if (v.e2.x < tr.x - 0.5f) inkLine(v.e2, tr, th, kInk);
        } else {
            inkLine(tl, tr, th, kInk);
        }
        DrawCircleV(tr, th / 2, kInk);
        rlPopMatrix();
    }
}

// A dashed circle (for a bubble's pop).
static void dashedCircle(Vector2 c, float r, float thick, Color col) {
    const int n = 10;
    for (int i = 0; i < n; i++) {
        float a0 = i * 2 * PI / n, a1 = a0 + PI / n;
        inkLine({c.x + cosf(a0) * r, c.y + sinf(a0) * r}, {c.x + cosf(a1) * r, c.y + sinf(a1) * r}, thick, col);
    }
}

// A pineapple: a crosshatched golden body and a crown of leaves, upright with gravity.
// Brighter by factor k (1 = as is), each channel clamped.
static Color brighten(Color c, float k) {
    return {(unsigned char)fminf(255, c.r * k), (unsigned char)fminf(255, c.g * k), (unsigned char)fminf(255, c.b * k), c.a};
}

// A reached checkpoint's halo: a soft warm glow, pulsing with it (`a`: as much of it as shows). Drawn
// over the background but under the level, so the ground in front hides it.
static void drawPineHalo(Vector2 p, float pulse, float a) {
    float r = 34 + 8 * pulse;
    DrawCircleGradient(p, r, {255, 236, 150, (unsigned char)((110 + 60 * pulse) * a)}, {255, 236, 150, 0});
    DrawCircleGradient(p, r * 0.55f, {255, 250, 210, (unsigned char)((90 + 50 * pulse) * a)}, {255, 250, 210, 0});
}

// A pineapple: grey until reached; then in colour, its brightness breathing 100% -> 135% -> 100%
// once a second.
// A checkpoint: a green fruit with a leaf -- grey until reached; then in colour, its brightness
// breathing 100% -> 135% -> 100% once a second (and a halo, drawn separately); `a`: as much of it as
// shows (lit a while, it fades out: Game::pineShow).
static void drawCheckpoint(Vector2 p, float angle, bool lit, float time, float a) {
    float k = lit ? 1 + 0.35f * (0.5f - 0.5f * cosf(2 * PI * time)) : 1;
    Color body = fade(lit ? brighten({120, 200, 90, 255}, k) : Color{178, 178, 178, 255}, a);
    Color leaf = fade(lit ? brighten(kLeaf, k * 0.85f) : Color{130, 130, 130, 255}, a), ink = fade(kInk, a);
    rlPushMatrix();
    rlTranslatef(p.x, p.y, 0);
    rlRotatef(angle, 0, 0, 1);
    inkLine({0, -8}, {1, -13}, 2.0f, ink);                        // stem
    DrawEllipseV({5, -12}, 5, 2.5f, leaf);                           // leaf
    DrawEllipseLinesV({5, -12}, 5, 2.5f, ink);
    DrawCircleV({0, 0}, 9, body);
    DrawCircleLinesV({0, 0}, 9, ink);
    DrawCircleV({-3, -3}, 2.2f, fade(kIceShine, 0.9f * a));
    rlPopMatrix();
}

static void drawPineapple(Vector2 p, float angle, bool lit, float time) {
    float k = lit ? 1 + 0.35f * (0.5f - 0.5f * cosf(2 * PI * time)) : 1;
    Color leaf = lit ? brighten(kLeaf, k) : Color{140, 140, 140, 255};
    Color body = lit ? brighten(kPine, k) : Color{178, 178, 178, 255};
    rlPushMatrix();
    rlTranslatef(p.x, p.y, 0);
    rlRotatef(angle, 0, 0, 1);
    for (int k = -2; k <= 2; k++) {                       // leaves
        Vector2 base = {k * 2.5f, -10}, tip = {k * 4.0f, -19 - (2 - abs(k)) * 3.0f};
        Vector2 a = {base.x - 2.5f, base.y}, b = {base.x + 2.5f, base.y};
        DrawTriangle(a, tip, b, leaf);
        inkLine(a, tip, 1.6f, kInk); inkLine(tip, b, 1.6f, kInk);
    }
    const float ea = 9.5f, eb = 12.5f, ey = 1;
    DrawEllipse(0, (int)ey, ea, eb, body);
    // The crosshatch: diagonal lines, each clipped exactly to the oval.
    for (int dir = -1; dir <= 1; dir += 2)
        for (int k = -3; k <= 3; k++) {
            Vector2 o = {k * 5.0f, ey}, d = Vector2Normalize({(float)dir * 0.6f, 1.0f});
            // (o.x + s d.x)^2/a^2 + (o.y - ey + s d.y)^2/b^2 = 1
            float A = d.x * d.x / (ea * ea) + d.y * d.y / (eb * eb);
            float B = 2 * (o.x * d.x / (ea * ea) + (o.y - ey) * d.y / (eb * eb));
            float C = o.x * o.x / (ea * ea) + (o.y - ey) * (o.y - ey) / (eb * eb) - 1;
            float disc = B * B - 4 * A * C;
            if (disc <= 0) continue;
            float s0 = (-B - sqrtf(disc)) / (2 * A), s1 = (-B + sqrtf(disc)) / (2 * A);
            inkLine(Vector2Add(o, Vector2Scale(d, s0 + 0.6f)), Vector2Add(o, Vector2Scale(d, s1 - 0.6f)), 1.2f, fade(kInk, 0.55f));
        }
    DrawEllipseLines(0, 1, 9.5f, 12.5f, kInk);
    DrawEllipseLines(0, 1, 10.0f, 13.0f, kInk);
    rlPopMatrix();
}

// A flag on a pole -- the green one, at the end of the way back -- standing on its ground for the
// gravity of the moment (Level::anchorFlag: `foot`, the ground on `side`, a pole `poleH` tall).
// Furled until it goes up (`up` seconds ago; -1: not yet); then it unfurls and flies, rippling in a
// gentle wind. `rainbow` (the black flag's look, `col` aside): six upright bars, as wide each, pole to
// tip round the rainbow -- up, rolling along it as the title's colours do (title_scores_rainbow_speed).
static Color rainbowAt(float h);
static void drawFlag(const Game& g, Vector2 foot, int side, float poleH, float up, Color col, bool rainbow = false) {
    const float flagL = 1.3f * kTile, flagH = 0.8f * kTile;
    float open = up < 0 ? 0 : clampf(up / 0.8f, 0, 1);
    open = 1 - (1 - open) * (1 - open) * (1 - open);   // eased out
    rlPushMatrix();
    rlTranslatef(foot.x, foot.y, 0);
    rlRotatef(side * 90.0f, 0, 0, 1);                   // the pole straight out of its ground
    const int n = rainbow ? 18 : 14;                    // (the rainbow's: three to a bar)
    Vector2 top[19], bot[19];
    float len = lerpf(5, flagL, open), wind = g.time * 2 * PI * 0.7f;
    for (int i = 0; i <= n; i++) {
        float u = (float)i / n;
        float wave = (1.0f + 3.5f * open) * u * sinf(wind - u * 2.6f * PI);   // a ripple, stronger towards the tip
        float x = 1.5f + len * u, y0 = -poleH + 2 + wave;
        top[i] = {x, y0 + u * flagH / 2};
        bot[i] = {x, y0 + flagH - u * flagH / 2};
    }
    float roll = rainbow && up >= 0 ? g.time * g.t.scoresRainbow : 0;
    for (int i = 0; i < n; i++) {
        Color s = rainbow ? rainbowAt((float)(i * 6 / n) / 6 - roll) : col;
        DrawTriangle(top[i], bot[i], bot[i + 1], s);
        DrawTriangle(top[i], bot[i + 1], top[i + 1], s);
    }
    for (int i = 0; i < n; i++) {                       // a thin black border all round
        inkLine(top[i], top[i + 1], 1.5f, kInk);
        inkLine(bot[i], bot[i + 1], 1.5f, kInk);
    }
    inkLine(top[0], bot[0], 1.5f, kInk);
    inkLine({0, 0}, {0, -poleH}, 3.0f, kInk);         // the pole, and its knob
    DrawCircleV({0, -poleH}, 3.2f, kInk);
    rlPopMatrix();
}

// The key, lying flat, bow on the left, its bit hanging down at the right: centred on `c`, turned
// by `angle`, scaled by `s`. Inked all round, as everything else.
static void drawKey(Vector2 c, float angle, float s, Color col, float alpha = 1) {
    const float ro = kKeyH / 2, ri = 3.6f, bx = -kKeyLen / 2 + ro;   // the bow: a ring at the left end
    const float x0 = bx + ro - 2, x1 = kKeyLen / 2, th = 2.6f;        // the shaft, half as thick as this
    const Rectangle parts[3] = {{x0, -th, x1 - x0, 2 * th},          // shaft...
                                {x1 - 3.2f, th, 3.2f, 5.0f},          // ...and two teeth
                                {x1 - 9.0f, th, 3.2f, 3.4f}};
    const float o = 1.6f;                                            // the ink round it
    Color ink = fade(kInk, alpha), fill = fade(col, alpha);
    rlPushMatrix();
    rlTranslatef(c.x, c.y, 0);
    rlRotatef(angle, 0, 0, 1);
    rlScalef(s, s, 1);
    DrawRing({bx, 0}, ri - o, ro + o, 0, 360, 36, ink);
    for (const Rectangle& r : parts) DrawRectangleRec({r.x - o, r.y - o, r.width + 2 * o, r.height + 2 * o}, ink);
    DrawRing({bx, 0}, ri, ro, 0, 360, 36, fill);
    for (const Rectangle& r : parts) DrawRectangleRec(r, fill);
    DrawCircleV({bx - ro * 0.45f, -ro * 0.45f}, 1.6f, fade(WHITE, 0.55f * alpha));   // a glint on the bow
    rlPopMatrix();
}

// The golden key's glow: a golden halo round it, breathing -- its radius from half the key's length
// to one and a half, and back, over gold_key_glow_period -- a brighter core inside. (`s`: the key's
// scale.)
static void drawKeyGlow(Vector2 c, float s, float time, float period) {
    float k = 0.5f - 0.5f * cosf(2 * PI * time / period);    // 0..1, smoothly
    float r = kKeyLen * s * (0.5f + k);
    DrawCircleGradient(c, r, {255, 214, 90, (unsigned char)(150 + 60 * k)}, {255, 214, 90, 0});
    DrawCircleGradient(c, r * 0.6f, {255, 244, 196, (unsigned char)(110 + 50 * k)}, {255, 244, 196, 0});
}

// Lamp posts: a stick two tiles tall, a small disk on top (its fly circles it: drawFlies).
static void drawLamps(const Game& g) {
    for (size_t k = 0; k < g.level.lamps.size(); k++) {
        Vector2 foot = g.level.lamps[k], top = g.lampTop(k);
        if (!inView(Vector2Lerp(foot, top, 0.5f), kTile + 8)) continue;
        inkLine(foot, top, 3.0f, kInk);
        DrawCircleV(top, 6.0f, kInk);
        DrawCircleV({top.x - 1.8f, top.y - 1.8f}, 1.8f, fade(WHITE, 0.45f));   // a glint
    }
}

// A colour premultiplied by an opacity: its red, green and blue times it, its alpha it.
static Color premul(Color c, float a) {
    a = clampf(a, 0, 1);
    return {(unsigned char)(c.r * a), (unsigned char)(c.g * a), (unsigned char)(c.b * a), (unsigned char)(255 * a)};
}
// The colour poles' gas (workshop/colorpole.jpg): each cloud a bunch of disks -- inked round as one
// (all of them a little bigger in a deeper tone, then all of them in the colour, paler: the lines
// inside left out; color_gas_contour: the ink or not) -- premultiplied by its opacity `a`. Drawn
// with a blend keeping the most of each channel, its disks make one cloud, the fill over the rim
// (paler: more in each channel) -- see the clouds' layers in main.
static void drawGas(const Game& g, const Game::Gas& gs, float a) {
    Color col = g.t.colorOf[gs.colour], fill = premul(paler(col, g.t.gasPale), a), edge = premul(ColorLerp(col, kInk, 0.3f), a);
    for (int pass = g.t.gasContour ? 0 : 1; pass < 2; pass++)   // pass 0: the darker rim, a little bigger; pass 1: the fill
        for (size_t i = 0; i < gs.r.size(); i++) {
            Vector2 p;
            float r;
            if (g.puffAt(gs, i, p, r) && inView(p, r + 2)) DrawCircleV(p, pass ? r : r + 2.2f, pass ? fill : edge);
        }
}

// A colour pole: a black pole as a lamp post's, its balloon on top -- a real balloon's shape, an egg
// longer than wide, rounder at its top (upright against gravity, pushed over by a wall in its way),
// inked round, a knot at its foot, a glint on it -- swelling, paler as it does (color_balloon_pale),
// till it bursts. A colour fly's pole: the same pole, no balloon -- the dot on its top its fly's
// colour, inked round thin.
static void drawPoles(const Game& g) {
    // Where a cloud -- any -- covers it, a pole shows color_hide_pole of itself, the cloud behind it
    // in its place; as much as the cloud is there (its opacity: back as it evaporates). Its stick
    // cut where the puffs' edges cross it, each piece as the cloud over its middle.
    // A balloon in a cloud -- a new one growing before the last one's has gone -- shows
    // color_hide_balloon of itself, as much as the cloud covers it (17 points in it) and is there.
    auto cloudAt = [&](Vector2 q) {                      // the most opaque puff over q
        float a = 0;
        Vector2 p;
        float r;
        for (const Game::Gas& gs : g.gases) {
            float ga = g.gasOpacity(gs);
            if (ga <= a) continue;
            for (size_t i = 0; i < gs.p.size(); i++)
                if (g.puffAt(gs, i, p, r) && Vector2Distance(p, q) < r) { a = ga; break; }
        }
        return a;
    };
    auto inkAt = [&](Vector2 q) { return fade(kInk, 1 - cloudAt(q) * (1 - g.t.colorHidePole)); };
    auto stick = [&](Vector2 foot, Vector2 top) {
        std::vector<float> cut{0, 1};
        Vector2 d = Vector2Subtract(top, foot), p;
        float r, dd = Vector2DotProduct(d, d);
        if (g.t.colorHidePole < 1 && dd > 0)
            for (const Game::Gas& gs : g.gases)
                for (size_t i = 0; i < gs.p.size(); i++) {
                    if (!g.puffAt(gs, i, p, r)) continue;
                    Vector2 f = Vector2Subtract(foot, p);
                    float b = Vector2DotProduct(f, d), disc = b * b - dd * (Vector2DotProduct(f, f) - r * r);
                    if (disc <= 0) continue;
                    for (float u : {(-b - sqrtf(disc)) / dd, (-b + sqrtf(disc)) / dd})
                        if (u > 0 && u < 1) cut.push_back(u);
                }
        std::sort(cut.begin(), cut.end());
        for (size_t j = 0; j + 1 < cut.size(); j++) {
            if (cut[j + 1] <= cut[j]) continue;
            Color c = inkAt(Vector2Lerp(foot, top, (cut[j] + cut[j + 1]) / 2));
            if (c.a > 0) inkLine(Vector2Lerp(foot, top, cut[j]), Vector2Lerp(foot, top, cut[j + 1]), 3.0f, c);
        }
    };
    for (size_t k = 0; k < g.level.poles.size(); k++) {
        Vector2 foot = g.level.poles[k], top = g.poleTop(k);
        if (!inView(Vector2Lerp(foot, top, 0.5f), 2 * kTile + 8)) continue;
        if (g.level.poleFly[k]) {
            stick(foot, top);
            Color ink = inkAt(top);                      // (in a cloud: as the pole, the dot's colour too)
            DrawCircleV(top, 4.6f, ink);
            DrawCircleV(top, 3.6f, fade(g.t.colorOf[g.level.poleColour[k]], ink.a / 255.0f));
            continue;
        }
        float R = g.balloonR(k), H = R * Game::kBalloonLong, grown = k < g.balloonT.size() ? clampf(g.balloonT[k] / g.t.balloonGrow, 0, 1) : 0;
        Vector2 a = g.balloonAxis(k), s = {-a.y, a.x}, c = g.balloonC(k);
        auto egg = [&](float out) {                      // its outline, `out` px past it: a fan round its middle
            std::vector<Vector2> p{c};
            const int n = 32;
            for (int j = 0; j <= n; j++) {
                float t = 2 * PI * j / n, along = -cosf(t);      // (-1 at its foot, 1 at its top)
                float across = sinf(t) * (1 + 0.12f * along);    // (wider above)
                p.push_back(Vector2Add(c, Vector2Add(Vector2Scale(s, (R + out) * across), Vector2Scale(a, (H + out) * along))));
            }
            float area = 0;                              // (screen-clockwise, for the fan to show: positive, y down)
            for (size_t j = 1; j + 1 < p.size(); j++) area += p[j].x * p[j + 1].y - p[j + 1].x * p[j].y;
            if (area < 0) std::reverse(p.begin() + 1, p.end());
            return p;
        };
        stick(foot, top);
        if (k < g.balloonT.size() && g.balloonT[k] < 0) { DrawCircleV(top, 2.6f, inkAt(top)); continue; }   // burst, the next not yet growing: its knot only
        // About to burst (its last color_balloon_dash_time): its ink line in dashes, color_balloon_dashes
        // of them round it, gaps as long, as if it were tearing -- color_balloon_dash_thickness thick
        // (the ink line's 1.6 px: as thick), outwards from its edge as the line is.
        bool dashed = k < g.balloonT.size() && g.balloonT[k] > g.t.balloonGrow - g.t.balloonDashTime;
        float alpha = 1;                                 // (in a cloud: color_hide_balloon)
        if (g.t.colorHideBalloon < 1 && !g.gases.empty()) {
            float cover = cloudAt(c);
            for (float rho : {0.45f, 0.85f})
                for (int j = 0; j < 8; j++) {
                    float t = PI * j / 4;
                    cover += cloudAt(Vector2Add(c, Vector2Add(Vector2Scale(s, R * rho * sinf(t)), Vector2Scale(a, -H * rho * cosf(t)))));
                }
            alpha = 1 - cover / 17 * (1 - g.t.colorHideBalloon);
        }
        const Color line = fade(kInk, alpha);
        std::vector<Vector2> fill = egg(0);
        if (!dashed) {
            std::vector<Vector2> ink = egg(1.6f);
            if (alpha >= 1) DrawTriangleFan(ink.data(), (int)ink.size(), kInk);
            else {                                       // (see-through: its line a ring, not under the fill)
                std::vector<Vector2> ring;
                for (size_t j = 1; j < ink.size(); j++) { ring.push_back(ink[j]); ring.push_back(fill[j]); }
                DrawTriangleStrip(ring.data(), (int)ring.size(), line);
            }
        }
        DrawTriangleFan(fill.data(), (int)fill.size(), fade(paler(g.t.colorOf[g.level.poleColour[k]], lerpf(1, g.t.balloonPale, grown)), alpha));
        if (dashed) {
            std::vector<Vector2> q = egg(g.t.balloonDashW / 2);   // (the line's middle)
            float L = 0;
            for (size_t j = 1; j + 1 < q.size(); j++) L += Vector2Distance(q[j], q[j + 1]);
            float P = L / g.t.balloonDashes, s0 = 0;      // (a dash and its gap; each dash [m P, m P + P/2] along it)
            for (size_t j = 1; j + 1 < q.size(); j++) {
                float d = Vector2Distance(q[j], q[j + 1]), s1 = s0 + d;
                for (float on = floorf(s0 / P) * P; d > 0 && on < s1; on += P) {
                    float u0 = fmaxf(on, s0), u1 = fminf(on + P / 2, s1);
                    if (u1 > u0) inkLine(Vector2Lerp(q[j], q[j + 1], (u0 - s0) / d), Vector2Lerp(q[j], q[j + 1], (u1 - s0) / d), g.t.balloonDashW, line);
                }
                s0 = s1;
            }
        }
        DrawCircleV(Vector2Add(c, Vector2Add(Vector2Scale(a, H * 0.45f), Vector2Scale(s, R * 0.4f))), fmaxf(1, R * 0.16f), fade(WHITE, 0.7f * alpha));
        DrawCircleV(top, 2.6f, inkAt(top));              // (the knot)
    }
}

// A locked door: a red slab inked round, a second line round it 1/8 of its size inside, and a
// black keyhole -- a disc over a widening trapeze -- in the middle. The key in it, it turns gold
// (door_gold_time), then fades out, open (door_fade_time).
static void drawDoors(const Game& g, float angle) {
    for (size_t i = 0; i < g.level.doors.size(); i++) {
        float d = i < g.doorT.size() ? g.doorT[i] : -1;
        float gold = d < 0 ? 0 : clampf(d / g.t.doorGold, 0, 1);
        float a = d < g.t.doorGold ? 1 : 1 - (d - g.t.doorGold) / g.t.doorFade;
        if (a <= 0 || !CheckCollisionRecs(g.level.doors[i], gView)) continue;
        Rectangle r = g.level.doors[i];
        char dc = g.level.doorChar[i];
        Color col = ColorLerp(g.t.keyCol[Game::keyOfDoor(dc)], kGold, gold), ink = fade(kInk, a);
        DrawRectangleRec(r, fade(col, a));
        DrawRectangleLinesEx(r, 1.6f, ink);
        DrawRectangleLinesEx({r.x + r.width / 8, r.y + r.height / 8, r.width * 0.75f, r.height * 0.75f}, 1.4f, ink);
        rlPushMatrix();                                   // the keyhole upright on the screen, whatever the gravity (as a key)
        rlTranslatef(r.x + r.width / 2, r.y + r.height / 2, 0);
        rlRotatef(angle, 0, 0, 1);
        DrawCircleV({0, -4}, 4.4f, ink);
        Vector2 tl = {-1.8f, -2}, tr = {1.8f, -2}, bl = {-3.6f, 8}, br = {3.6f, 8};
        DrawTriangle(tl, bl, br, ink);
        DrawTriangle(tl, br, tr, ink);
        rlPopMatrix();
    }
}

// Ability fruits: the pineapple (double jump), blue with a crown (tongue).
static void drawFruits(const Game& g, float angle) {
    for (size_t i = 0; i < g.level.fruits.size(); i++) {
        if (g.fruitEaten[i] || !inView(g.level.fruits[i], kTile)) continue;
        Vector2 p = Vector2Add(g.level.fruits[i], Vector2Scale(g.pullW, -sinf(g.time * 2.4f + i) * 3.0f));
        rlPushMatrix();
        rlTranslatef(p.x, p.y, 0);
        rlRotatef(angle, 0, 0, 1);
        if (g.level.fruitKind[i] == 'D') {                  // double jump: the pineapple
            rlPopMatrix();
            drawPineapple(p, angle, true, g.time);
            continue;
        } else if (g.level.fruitKind[i] == 'U') {           // the banana: a yellow crescent, inked, a brown tip
            const Color yellow = {244, 210, 60, 255}, brown = {110, 78, 40, 255};
            // Along an arc (the bottom of a circle), thin at both ends, plump in the middle.
            auto at = [](float u) { float a = PI * (0.15f + 0.7f * u); return Vector2{16 * cosf(a), -11 + 16 * sinf(a)}; };
            auto crescent = [&](float grow, Color col) {
                const int n = 16;
                for (int k = 0; k < n; k++) {
                    float u0 = (float)k / n, u1 = (float)(k + 1) / n;
                    float w0 = 0.6f + 3.0f * sinf(PI * u0) + grow, w1 = 0.6f + 3.0f * sinf(PI * u1) + grow;
                    Vector2 p0 = at(u0), p1 = at(u1);
                    Vector2 n0 = Vector2Normalize({p0.x, p0.y + 11}), n1 = Vector2Normalize({p1.x, p1.y + 11});
                    Vector2 o0 = Vector2Add(p0, Vector2Scale(n0, w0)), i0 = Vector2Subtract(p0, Vector2Scale(n0, w0));
                    Vector2 o1 = Vector2Add(p1, Vector2Scale(n1, w1)), i1 = Vector2Subtract(p1, Vector2Scale(n1, w1));
                    DrawTriangle(i0, o0, o1, col); DrawTriangle(i0, o1, i1, col);   // (both windings: one shows)
                    DrawTriangle(i0, o1, o0, col); DrawTriangle(i0, i1, o1, col);
                }
            };
            crescent(1.6f, kInk);
            crescent(0, yellow);
            DrawCircleV(at(0), 2.0f, brown);
            DrawCircleV(at(1), 1.8f, brown);
            rlPopMatrix();
            continue;
        } else if (g.level.fruitKind[i] == 'Z') {           // the kaki: a squat orange globe, inked, four green sepals and a stem on top
            const Color orange = {236, 122, 32, 255}, stem = {96, 70, 40, 255};
            DrawEllipse(0, 2, 11.5f, 9.5f, kInk);
            DrawEllipse(0, 2, 10, 8, orange);
            DrawEllipse(-3, -1, 3.2f, 2.0f, fade(kIceShine, 0.8f));
            for (int k = 0; k < 4; k++) {                    // the sepals: a little cross of leaves, splayed flat
                float a = (k * 90 + 45) * DEG2RAD;
                Vector2 c = {cosf(a) * 4.0f, -6.5f + sinf(a) * 1.6f};
                DrawEllipseV(c, 4.0f, 1.9f, kLeaf);
                DrawEllipseLinesV(c, 4.0f, 1.9f, kInk);
            }
            inkLine({0, -6.5f}, {1, -11}, 2.2f, stem);
            rlPopMatrix();
            continue;
        } else {
            DrawCircleV({0, 0}, 9, {80, 110, 210, 255});
            for (int k = 0; k < 5; k++) {
                float a = -PI / 2 + (k - 2) * 0.35f;
                inkLine({cosf(a) * 6, sinf(a) * 6}, {cosf(a) * 11, sinf(a) * 11}, 2.0f, kInk);
            }
        }
        DrawCircleLinesV({0, 0}, 9, kInk);
        DrawCircleV({-3, -3}, 2.2f, fade(kIceShine, 0.9f));
        rlPopMatrix();
    }
    // Anna's grape: a little bunch, purple, inked, a stem and a leaf on top -- bobbing as the fruits do.
    if (g.grapeAvailable() && !gTitleDuo && inView(g.grapeAt(), kTile)) {
        const Color purple = {124, 62, 160, 255}, stem = {96, 70, 40, 255};
        Vector2 p = Vector2Add(g.grapeAt(), Vector2Scale(g.pullW, -sinf(g.time * 2.4f + 7) * 3.0f));
        rlPushMatrix();
        rlTranslatef(p.x, p.y, 0);
        rlRotatef(angle, 0, 0, 1);
        inkLine({0, -7}, {1.5f, -14}, 2.0f, stem);
        DrawEllipseV({6, -12}, 4.5f, 2.2f, kLeaf);
        DrawEllipseLinesV({6, -12}, 4.5f, 2.2f, kInk);
        const Vector2 berry[6] = {{-6, -5}, {0, -6}, {6, -5}, {-3, 1}, {3, 1}, {0, 7}};
        for (Vector2 b : berry) DrawCircleV(b, 4.6f, kInk);
        for (Vector2 b : berry) {
            DrawCircleV(b, 3.5f, purple);
            DrawCircleV({b.x - 1.2f, b.y - 1.2f}, 1.1f, fade(kIceShine, 0.8f));
        }
        rlPopMatrix();
    }
}

// The double cherry ('C'): two red cherries on stems joined at the top, a leaf there, bobbing.
static void drawCherries(const Game& g, float angle) {
    const Color red = {196, 28, 48, 255}, stem = {96, 112, 52, 255};
    for (size_t i = 0; i < g.level.cherries.size(); i++) {
        if (g.cherryEaten[i] || !inView(g.level.cherries[i], kTile)) continue;
        Vector2 p = Vector2Add(g.level.cherries[i], Vector2Scale(g.pullW, -sinf(g.time * 2.3f + i) * 3.0f));
        rlPushMatrix();
        rlTranslatef(p.x, p.y, 0);
        rlRotatef(angle, 0, 0, 1);
        const Vector2 top = {1, -14}, fruit[2] = {{-5.5f, 4}, {6, 6}}, bend[2] = {{-4, -5}, {5, -4}};
        for (int k = 0; k < 2; k++) {                   // the stems, each with a little bend
            inkLine(top, bend[k], 1.8f, stem);
            inkLine(bend[k], {fruit[k].x, fruit[k].y - 5}, 1.8f, stem);
        }
        DrawEllipseV({top.x + 5, top.y - 1}, 4.5f, 2.2f, kLeaf);
        DrawEllipseLinesV({top.x + 5, top.y - 1}, 4.5f, 2.2f, kInk);
        for (int k = 0; k < 2; k++) {
            DrawCircleV(fruit[k], 5.8f, red);
            DrawCircleLinesV(fruit[k], 5.8f, kInk);
            DrawCircleV({fruit[k].x - 2, fruit[k].y - 2}, 1.5f, fade(kIceShine, 0.9f));
        }
        rlPopMatrix();
    }
}

static void drawParticles(const Game& g, bool drops) {
    for (const Particle& q : g.parts) {
        if (q.drop != drops || !inView(q.p, 12)) continue;   // (bubbles are not drops: drawn behind him, at his feet)
        float k = q.life / q.maxLife;
        if (q.bubble) {
            // Grows a little, then -- in its last few hundredths of a second -- pops: a dashed
            // ring flashing outwards.
            const float popT = 0.06f;
            if (q.life > popT) {
                float grow = 1 + 0.5f * (1 - (q.life - popT) / (q.maxLife - popT));
                float r = q.r * grow;
                DrawCircleV(q.p, r, kBubble);
                DrawCircleLinesV(q.p, r, fade(kInk, 0.45f));
                DrawCircleV({q.p.x - r * 0.35f, q.p.y - r * 0.35f}, r * 0.22f, kIceShine);
            } else {
                float u = 1 - q.life / popT;
                dashedCircle(q.p, q.r * (1.5f + 1.6f * u), 1.2f, fade(kInk, 0.6f * (1 - u)));
            }
        } else if (q.drop) {
            if (monsterAt(g, q.p) >= 0) continue;   // (in a Monstermunch's place: drawMouthDrops)
            DrawCircleV(q.p, q.r, fade(kSpit, fminf(1, k * 2)));
            DrawCircleLinesV(q.p, q.r, fade(kInk, fminf(1, k * 2) * 0.5f));
        } else if (q.spark) {
            Vector2 tail = {q.p.x - q.v.x * 0.04f, q.p.y - q.v.y * 0.04f};
            inkLine(tail, q.p, 2.4f, fade(kInk, k));
        } else {
            float r = q.r * (1.6f - 0.6f * k);
            DrawCircleV(q.p, r, fade(kSlab, k * 1.2f));
            DrawCircleLinesV(q.p, r, fade(kInk, k * 0.8f));
        }
    }
}

// The saliva in a Monstermunch's place: only through its mouth, behind its face and teeth.
static void drawMouthDrops(const Game& g) {
    for (const Particle& q : g.parts) {
        if (!q.drop || !inView(q.p, 12)) continue;
        int mi = monsterAt(g, q.p);
        if (mi < 0) continue;
        float a = fminf(1, q.life / q.maxLife * 2);
        std::vector<Vector2> c;
        for (int j = 0; j < 12; j++) c.push_back({q.p.x + q.r * cosf(2 * PI * j / 12), q.p.y + q.r * sinf(2 * PI * j / 12)});
        drawInMouth(g, mi, c, fade(kSpit, a), fade(kInk, a * 0.5f), 1.0f);
    }
}

// Dotted rectangle, as in the sketch: the zone Leon may not leave.
static void dottedRect(Rectangle r, Color c) {
    const float dash = 6, gap = 8;
    for (float x = r.x; x < r.x + r.width; x += dash + gap) {
        float e = fminf(x + dash, r.x + r.width);
        inkLine({x, r.y}, {e, r.y}, 2, c);
        inkLine({x, r.y + r.height}, {e, r.y + r.height}, 2, c);
    }
    for (float y = r.y; y < r.y + r.height; y += dash + gap) {
        float e = fminf(y + dash, r.y + r.height);
        inkLine({r.x, y}, {r.x, e}, 2, c);
        inkLine({r.x + r.width, y}, {r.x + r.width, e}, 2, c);
    }
}

// ---- bug reports: Shift+B ------------------------------------------------------------------------
// The text is UTF-8: the cursor only ever sits on a character's first byte.
static size_t prevChar(const std::string& s, size_t i) {
    if (i == 0) return 0;
    i--;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
    return i;
}
static size_t nextChar(const std::string& s, size_t i) {
    if (i >= s.size()) return s.size();
    i++;
    while (i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
    return i;
}
// A word left / right of i: past the spaces, then past the word.
static size_t wordLeft(const std::string& s, size_t i) {
    while (i > 0 && isspace((unsigned char)s[i - 1])) i--;
    while (i > 0 && !isspace((unsigned char)s[i - 1])) i--;
    return i;
}
static size_t wordRight(const std::string& s, size_t i) {
    while (i < s.size() && isspace((unsigned char)s[i])) i++;
    while (i < s.size() && !isspace((unsigned char)s[i])) i++;
    return i;
}

// The text word-wrapped to a width, each line as the bytes [first, second) it shows (paragraphs
// kept; the space a line breaks at belongs to neither line).
typedef std::vector<std::pair<size_t, size_t>> Lines;
static Lines wrapLines(const std::string& text, float width, float size) {
    Font f = GetFontDefault();
    auto wide = [&](size_t a, size_t b) { return MeasureTextEx(f, text.substr(a, b - a).c_str(), size, size / 10).x; };
    Lines out;
    size_t at = 0;
    while (true) {
        size_t nl = text.find('\n', at), pe = nl == std::string::npos ? text.size() : nl;   // the paragraph [at, pe)
        size_t ls = at, le = at, i = at;                  // the line so far: [ls, le); the next word starts at i
        while (true) {
            size_t j = text.find(' ', i);
            if (j == std::string::npos || j > pe) j = pe;
            if (le > ls && wide(ls, j) > width) { out.push_back({ls, le}); ls = i; }   // the word goes down a line
            le = j;
            while (wide(ls, le) > width && nextChar(text, ls) < le) {   // a word longer than the line: cut
                size_t cut = le;
                while (cut > nextChar(text, ls) && wide(ls, cut) > width) cut = prevChar(text, cut);
                out.push_back({ls, cut});
                ls = cut;
            }
            if (j >= pe) break;
            i = j + 1;
        }
        out.push_back({ls, pe});
        if (nl == std::string::npos) break;
        at = nl + 1;
    }
    return out;
}
// The line the cursor is on: the last one starting at or before it.
static int lineOf(const Lines& lines, size_t c) {
    int k = 0;
    for (int i = 0; i < (int)lines.size(); i++) if (lines[i].first <= c) k = i;
    return k;
}
// How far into its line the cursor is (pixels) -- and back: the place on a line nearest x.
static float colX(const std::string& s, std::pair<size_t, size_t> line, size_t c, float fs) {
    size_t e = std::min(std::max(c, line.first), line.second);
    return MeasureTextEx(GetFontDefault(), s.substr(line.first, e - line.first).c_str(), fs, fs / 10).x;
}
static size_t atX(const std::string& s, std::pair<size_t, size_t> line, size_t last, float x, float fs) {
    size_t best = line.first;
    float bd = 1e9f;
    for (size_t i = line.first;; i = nextChar(s, i)) {
        float d = fabsf(colX(s, line, i, fs) - x);
        if (d < bd) { bd = d; best = i; }
        if (i >= last) break;
    }
    return best;
}

// The box, laid out for the screen as it is.
struct ReportBoxGeom { Rectangle box, area; float ui, pad, ts, hs, fs, lh; };
static ReportBoxGeom reportBoxGeom() {
    int sw = gameW(), sh = gameH();
    ReportBoxGeom g;
    g.ui = fmaxf(1.0f, sh / 720.0f);
    g.box = {sw * 0.14f, sh * 0.16f, sw * 0.72f, sh * 0.6f};
    g.pad = 18 * g.ui; g.ts = 30 * g.ui; g.hs = 15 * g.ui; g.fs = 20 * g.ui; g.lh = g.fs * 1.25f;
    g.area = {g.box.x + g.pad, g.box.y + g.pad + g.ts + 2 * g.hs + 26 * g.ui, g.box.width - 2 * g.pad, 0};
    g.area.height = g.box.y + g.box.height - g.pad - g.area.y;
    return g;
}

// `scroll`: the first line in view -- moved only as far as it takes to keep the cursor in view.
static void drawReportBox(const std::string& text, size_t cursor, float clock, int& scroll) {
    ReportBoxGeom g = reportBoxGeom();
    Font f = GetFontDefault();
    DrawRectangle(0, 0, gameW(), gameH(), fade(kBlack, 0.45f));
    DrawRectangleRec(g.box, kPaper);
    DrawRectangleLinesEx(g.box, 3 * g.ui, kInk);
    DrawTextEx(f, "Bug report", {g.box.x + g.pad, g.box.y + g.pad}, g.ts, g.ts / 10, kInk);
    float hy = g.box.y + g.pad + g.ts + 6 * g.ui;
    DrawTextEx(f, "Enter: save    Shift+Enter: new line    Arrows: move in the text    Esc: cancel",
               {g.box.x + g.pad, hy}, g.hs, g.hs / 10, fade(kInk, 0.65f));
    DrawTextEx(f, "(the picture is the moment you pressed Shift+B)", {g.box.x + g.pad, hy + g.hs + 4 * g.ui}, g.hs, g.hs / 10,
               fade(kInk, 0.65f));
    Rectangle area = g.area;
    DrawRectangleLinesEx({area.x - 6 * g.ui, area.y - 6 * g.ui, area.width + 12 * g.ui, area.height + 12 * g.ui}, 1.5f * g.ui,
                         fade(kInk, 0.4f));
    Lines lines = wrapLines(text, area.width, g.fs);
    int fit = std::max(1, (int)(area.height / g.lh));
    int k = lineOf(lines, cursor);
    if (k < scroll) scroll = k;
    if (k >= scroll + fit) scroll = k - fit + 1;
    scroll = std::clamp(scroll, 0, std::max(0, (int)lines.size() - fit));
    for (int i = scroll; i < std::min((int)lines.size(), scroll + fit); i++)
        DrawTextEx(f, text.substr(lines[i].first, lines[i].second - lines[i].first).c_str(),
                   {area.x, area.y + (i - scroll) * g.lh}, g.fs, g.fs / 10, kInk);
    if (fmodf(clock, 1.0f) < 0.55f) {                                  // the cursor
        float cx = area.x + colX(text, lines[k], cursor, g.fs) + g.ui;
        float cy = area.y + (k - scroll) * g.lh;
        DrawRectangle((int)cx, (int)cy, (int)(2 * g.ui), (int)g.fs, kInk);
    }
}

// Reports/<YYYYMMDD>_<HH>h<MM>m<SS>s_[N_]open.txt / .png -- N (2, 3, ...) only if the name is taken.
static std::string reportBase(const std::string& dir, const std::string& stamp) {
    std::string base = dir + "/" + stamp + "_open";
    for (int n = 2; FileExists((base + ".txt").c_str()) || FileExists((base + ".png").c_str()); n++)
        base = dir + "/" + stamp + "_" + std::to_string(n) + "_open";
    return base;
}

static bool writeText(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    fclose(f);
    return ok;
}

// Reads a report back: its [state] lines into `state`, its copy of params.txt into `params`.
static bool readReport(const std::string& path, std::map<std::string, std::string>& state, std::string& params) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    fclose(f);
    size_t data = text.find("=== GRAVITY LEON REPORT DATA");
    if (data == std::string::npos) return false;
    size_t st = text.find("\n[state]\n", data), pp = text.find("\n[params.txt]", data), pe = text.find("\n[end params.txt]", data);
    if (st == std::string::npos || pp == std::string::npos) return false;
    size_t at = st + 9;
    while (at < pp) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos || end > pp) end = pp;
        std::string l = text.substr(at, end - at);
        at = end + 1;
        size_t colon = l.find(": ");
        if (colon != std::string::npos) state[l.substr(0, colon)] = l.substr(colon + 2);
    }
    size_t ps = text.find('\n', pp + 1);
    if (ps != std::string::npos) params = text.substr(ps + 1, (pe == std::string::npos ? text.size() : pe + 1) - (ps + 1));
    return true;
}

static std::string screenshotPath(const char* name, bool withSeconds) {
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char stamp[32];
    strftime(stamp, sizeof(stamp), withSeconds ? "%Y%m%d_%H%M%S" : "%Y%m%d_%H%M", &tmv);
    if (!DirectoryExists("screenshots")) MakeDirectory("screenshots");
    std::string base = std::string("screenshots/") + stamp + "_" + name;
    std::string p = base + ".png";
    for (int n = 2; FileExists(p.c_str()); n++) p = base + "_" + std::to_string(n) + ".png";
    return p;
}

static void saveScreen(const std::string& path) {
    rlDrawRenderBatchActive();   // flush the HUD, still queued until EndDrawing
    Image img = gameShot();
#ifdef __EMSCRIPTEN__
    if (ExportImage(img, path.c_str())) Web::download(path.c_str());
#else
    ExportImage(img, path.c_str());
#endif
    UnloadImage(img);
    TraceLog(LOG_INFO, "SHOT: %s", path.c_str());
}

// The help lines at the bottom of the screen: where the first one starts (the title's credits
// sit above them).
static const int kHintLines = 4;
static float hintTop() {
    float ui = fmaxf(1.0f, gameH() / 720.0f);
    return gameH() - ui * (kHintLines * 22.0f + 8.0f);
}

// Near a golden door, its name -- the left one
// "Love door", the right one "Solo door" -- fixed in the air over the third tile before it (on the side
// facing the other golden door: the hall's), as high as over Leon's hat were he standing there; each
// letter in the title's rolling rainbow. It fades in as he comes near, by where he is: none
// golden_door_name_distance tiles from the door (its nearest edge, any way), whole golden_door_name_fade
// tiles nearer. A wave runs
// along it: each letter swollen by a gaussian bump (golden_door_name_zoom at its peak, its width
// golden_door_name_wave_width letters) crossing the text in golden_door_name_wave_time, then
// golden_door_name_wave_pause at rest. A golden door open, both names fade out for good over
// golden_door_name_fade_out. (Not while he is dead, gone, or held in a golden run's way in.)
static void drawDoorName(const Game& g, const Camera2D& cam, Vector2 leonW) {
    if (g.dead || g.gone() || g.goldenHeld()) return;
    const char* name = nullptr;
    float a = 0;
    Vector2 anchor{};
    for (size_t i = 0; i < g.level.doors.size(); i++) {
        if (g.level.doorChar[i] != 'O') continue;
        Rectangle r = g.level.doors[i];
        float dx = fmaxf(fmaxf(r.x - leonW.x, leonW.x - (r.x + r.width)), 0), dy = fmaxf(fmaxf(r.y - leonW.y, leonW.y - (r.y + r.height)), 0);
        float dist = sqrtf(dx * dx + dy * dy) / kTile;
        float ai = g.t.goldDoorFade > 0 ? clampf((g.t.goldDoorName - dist) / g.t.goldDoorFade, 0, 1) : (dist <= g.t.goldDoorName ? 1.0f : 0.0f);
        if (ai <= a) continue;
        a = ai; name = g.goldDoorRank(i) == 0 ? "Love door" : "Solo door";
        float side = 1;                                  // (towards the other golden door)
        for (size_t j = 0; j < g.level.doors.size(); j++)
            if (j != i && g.level.doorChar[j] == 'O') side = g.level.doors[j].x > r.x ? 1.0f : -1.0f;
        anchor = {r.x + r.width / 2 + side * 3 * kTile, r.y + r.height - kLeonH - 2 - 1.5f * (kHatSeat + kHatCrown)};
    }
    float open = -1;                                     // a golden door open: both names fade out, golden_door_name_fade_out, for good
    for (size_t j = 0; j < g.level.doors.size(); j++)
        if (g.level.doorChar[j] == 'O' && g.doorT[j] >= g.t.doorGold) open = fmaxf(open, g.doorT[j] - g.t.doorGold);
    if (open >= 0) a *= g.t.doorNameOut > 0 ? 1 - clampf(open / g.t.doorNameOut, 0, 1) : 0;
    if (!name || a <= 0) return;
    Vector2 at = GetWorldToScreen2D(anchor, cam);        // (its letters' feet there)
    Font f = GetFontDefault();
    const int n = (int)strlen(name);
    float size = 30 * gameH() / 720.0f;
    const float w = g.t.doorWaveWidth, run = g.t.doorWaveTime, cycle = run + g.t.doorWavePause;
    float u = fmodf(g.time, cycle);                      // the bump's middle: from 2 widths before the first letter to 2 past the last
    float mid = u < run ? -2 * w + (n - 1 + 4 * w) * u / run : -1e9f;
    float k[64], total = 0;
    for (int i = 0; i < n && i < 64; i++) {
        float d = (i - mid) / w;
        k[i] = 1 + (g.t.doorZoom - 1) * expf(-0.5f * d * d);
        char one[2] = {name[i], 0};
        total += MeasureTextEx(f, one, size * k[i], size * k[i] / 10).x + (i + 1 < n ? size / 10 : 0);
    }
    float x = at.x - total / 2;
    for (int i = 0; i < n && i < 64; i++) {
        char one[2] = {name[i], 0};
        float s = size * k[i];
        DrawTextEx(f, one, {roundf(x), roundf(at.y - s)}, s, s / 10, fade(rainbowAt((float)i / n - g.time * g.t.scoresRainbow), a));
        x += MeasureTextEx(f, one, s, s / 10).x + size / 10;
    }
}

// The guide arrow: on the edge of the screen, on the line from Leon to where he should go next
// -- or, when that is in view, just short of it -- pointing there. Leon-sized, breathing.
static void drawGuideArrow(const Game& g, const Camera2D& cam, Vector2 leonW) {
    Vector2 to;
    if (!g.guideTarget(to)) return;
    Vector2 L = GetWorldToScreen2D(leonW, cam), T = GetWorldToScreen2D(to, cam);
    Vector2 d = Vector2Subtract(T, L);
    float dist = Vector2Length(d);
    if (dist < 1) return;
    d = Vector2Scale(d, 1 / dist);
    float len = kLeonDrawH * g.t.arrowSize * cam.zoom * (1 + g.t.arrowPulse * sinf(2 * PI * g.time / g.t.arrowPeriod));
    float m = len * 0.75f + 8;                                 // kept this far inside the screen
    Rectangle in = {m, m, gameW() - 2 * m, gameH() - 2 * m};
    Vector2 at;
    if (CheckCollisionPointRec(T, in)) {
        at = Vector2Subtract(T, Vector2Scale(d, len * 0.9f));   // in view: just short of it
    } else {                                                    // out of view: where the line leaves it
        float sx = d.x > 0 ? (in.x + in.width - L.x) / d.x : d.x < 0 ? (in.x - L.x) / d.x : 1e9f;
        float sy = d.y > 0 ? (in.y + in.height - L.y) / d.y : d.y < 0 ? (in.y - L.y) / d.y : 1e9f;
        at = Vector2Add(L, Vector2Scale(d, fmaxf(0, fminf(sx, sy))));
        at = {clampf(at.x, in.x, in.x + in.width), clampf(at.y, in.y, in.y + in.height)};
    }
    float hl = len * 0.45f, hw = len * 0.34f, sw = len * 0.13f;   // head length; head and shaft half-widths
    Vector2 p[7] = {{-len / 2, -sw}, {len / 2 - hl, -sw}, {len / 2 - hl, -hw}, {len / 2, 0},
                    {len / 2 - hl, hw}, {len / 2 - hl, sw}, {-len / 2, sw}};
    float ang = atan2f(d.y, d.x);
    for (Vector2& q : p) q = Vector2Add(at, Vector2Rotate(q, ang));
    Color fill = fade(kGuide, g.arrowA), ink = fade(kInk, g.arrowA);
    DrawTriangle(p[0], p[6], p[5], fill);                       // the shaft...
    DrawTriangle(p[0], p[5], p[1], fill);
    DrawTriangle(p[2], p[4], p[3], fill);                       // ...and the head
    float th = fmaxf(2.0f, len * 0.06f);
    for (int k = 0; k < 7; k++) inkLine(p[k], p[(k + 1) % 7], th, ink);
    for (int k = 0; k < 7; k++) DrawCircleV(p[k], th / 2, ink);  // round joins
}

// ---------------------------------------------------------------------------------------------
// Highscores: for each part of the level -- easy, hard, dark, harder and cameleon mode, and the golden
// run -- the five best times, three letters each, kept on disk in data/highscores.txt, a line an entry
// ("easy NIK 83.450").
// ---------------------------------------------------------------------------------------------
struct Scores {
    struct Entry { std::string name; float t; };
    static constexpr int kParts = 6, kRows = 5;
    std::vector<Entry> part[kParts];
    std::string path;
    static const char* tag(int k) { static const char* t[kParts] = {"easy", "hard", "dark", "harder", "cameleon", "golden"}; return t[k]; }
    static const char* title(int k) {
        static const char* t[kParts] = {"EASY MODE", "HARD MODE", "DARK MODE", "HARDER MODE", "CAMELEON MODE", "SOLO RUN"};   // (the golden run: SOLO RUN on the screen)
        return t[k];
    }
    void load(const std::string& p) {
        path = p;
        for (auto& v : part) v.clear();
        FILE* f = fopen(p.c_str(), "r");
        if (!f) return;
        char tg[32], nm[32];
        float t;
        while (fscanf(f, "%31s %31s %f", tg, nm, &t) == 3)
            for (int k = 0; k < kParts; k++)
                if (!strcmp(tg, tag(k))) part[k].push_back({std::string(nm).substr(0, 3), t});
        fclose(f);
        for (auto& v : part) {
            std::stable_sort(v.begin(), v.end(), [](const Entry& a, const Entry& b) { return a.t < b.t; });
            if ((int)v.size() > kRows) v.resize(kRows);
        }
    }
    void save() const {
        FILE* f = fopen(path.c_str(), "w");
        if (!f) { TraceLog(LOG_WARNING, "SCORES: cannot write %s", path.c_str()); return; }
        for (int k = 0; k < kParts; k++)
            for (const Entry& e : part[k]) fprintf(f, "%s %s %.3f\n", tag(k), e.name.c_str(), e.t);
#ifdef __EMSCRIPTEN__
        if (fclose(f) == 0) Web::scoresChanged();
#else
        fclose(f);
#endif
    }
    // Where a time would go in its part's table (0 the best), or -1: not good enough.
    int place(int k, float t) const {
        int i = 0;
        while (i < (int)part[k].size() && part[k][i].t <= t) i++;
        return i < kRows ? i : -1;
    }
    void insert(int k, const std::string& name, float t) {
        int i = place(k, t);
        if (i < 0) return;
        part[k].insert(part[k].begin() + i, {name, t});
        if ((int)part[k].size() > kRows) part[k].resize(kRows);
        save();
    }
};

// What has been finished this session -- the game launched, Leon's game always comes first: Leon's
// game, then Anna's -- the one unlocking hers, the two the title's choice. The stage the title is at:
// 0 Leon's game, 1 Anna's, 2 the choice. (Nothing kept on disk: the highscores are, not this.)
struct Progress {
    bool leon = false, anna = false;
    int stage() const { return !leon ? 0 : !anna ? 1 : 2; }
};

static void scoreTime(float t, char* buf, size_t n) { snprintf(buf, n, "%02d:%02d.%d", (int)(t / 60), (int)t % 60, (int)(t * 10) % 10); }

// A new highscore's name, typed in while the game goes on (main): its part, its place in that
// table, its time, the letters so far -- or this game's name, offered again (`copied`: untouched
// yet). Once kept, `part` and `place` still mark the run's line.
struct NameEntry {
    bool open = false;
    int part = -1, place = -1;
    float time = 0, clock = 0;
    char name[4] = "";
    int len = 0;
    bool copied = false;
};

// The highscores' rainbow -- the title's tables, and each flag's: line `line` of seven (a heading,
// a title, the five lines), the whole rainbow over them, rolling down them `speed` times a second;
// the yellows a little darker, to read on paper.
static Color rainbowAt(float h) {                        // (h of the way round it, wrapping)
    h = fmodf(h, 1.0f);
    if (h < 0) h += 1;
    float yellow = fmaxf(0, cosf((h * 360 - 60) * DEG2RAD));
    return ColorFromHSV(h * 360, 0.85f, 0.82f - 0.22f * yellow);
}
static Color rainbowLine(int line, float clock, float speed) { return rainbowAt(line / (2.0f + Scores::kRows) - clock * speed); }
// Text in bold: drawn twice, `bold` pixels apart, its letters spread by as much -- still a gap
// between them. (bold 0: as usual.)
static float boldWidth(Font f, const char* s, float size, float bold) { return MeasureTextEx(f, s, size, size / 10 + bold).x + bold; }
static void drawBold(Font f, const char* s, Vector2 at, float size, float bold, Color col) {
    for (float b : {0.0f, bold}) DrawTextEx(f, s, {roundf(at.x + b), roundf(at.y)}, size, size / 10 + bold, col);
}
// A highscore's name: its three letters each centred in a cell as wide as the widest letter, so
// that one missing -- a blank, a blinking slot -- moves nothing on its line.
static float nameWidth(Font f, float size) { return 3 * MeasureTextEx(f, "W", size, size / 10).x + 2 * (size / 10); }
static void drawName(Font f, const char* name, Vector2 at, float size, float bold, Color col) {
    float cell = MeasureTextEx(f, "W", size, size / 10).x, x = at.x;
    for (int i = 0; i < 3 && name[i]; i++) {
        char one[2] = {name[i], 0};
        if (one[0] != ' ') drawBold(f, one, {x + (cell - MeasureTextEx(f, one, size, size / 10).x) / 2, at.y}, size, bold, col);
        x += cell + size / 10;
    }
}

// A line of how-to -- a lesson's, the end's --, its colours marked in the text: {i} ink, {r} red, {o} orange, {n} brown,
// {v} violet, {b} blue, and {A} / {B} a pad's button in its circle, A green, B red (room on each side
// for it). Drawn from x (its left) at y, `size` high, faded by a -- or, draw false, only measured.
// Its width.
static float howLine(const char* text, float x, float y, int size, float a, Color ink, bool draw) {
    const int sp = size / 10;
    const float ring = 0.3f * size, x0 = x;
    Color col = ink;
    std::string run;
    auto flush = [&] {
        if (run.empty()) return;
        if (draw) DrawText(run.c_str(), (int)x, (int)y, size, fade(col, a));
        x += MeasureText(run.c_str(), size) + sp;
        run.clear();
    };
    for (const char* s = text; *s; s++) {
        if (s[0] != '{' || !s[1] || s[2] != '}') { run += *s; continue; }
        flush();
        char m = s[1];
        if (m == 'A' || m == 'B') {
            char b[2] = {m, 0};
            Color bc = m == 'A' ? kFlagGreen : kFlagRed;
            float w = (float)MeasureText(b, size);
            x += ring;
            if (draw) {
                DrawRing({x + w / 2, y + size / 2.0f}, 0.56f * size, 0.64f * size, 0, 360, 36, fade(bc, a));
                DrawText(b, (int)x, (int)y, size, fade(bc, a));
            }
            x += w + sp + ring;
        } else {
            switch (m) {
                case 'r': col = kFlagRed; break;
                case 'o': col = ORANGE; break;
                case 'n': col = BROWN; break;
                case 'v': col = kFlagViolet; break;
                case 'b': col = kKeyBlue; break;
                default:  col = ink; break;
            }
        }
        s += 2;
    }
    flush();
    return x - sp - x0;
}

static void drawHud(const Game& g, const Scores& sc, const NameEntry& en, bool clocks) {   // (clocks: not before the game starts)
    int sw = gameW();
    float ui = fmaxf(1.0f, gameH() / 720.0f);
    int fs = (int)(24 * ui);
    float leftX = 6 * ui;                                // the top left: the stars, then the key
    if (!g.level.stars.empty()) {
        const char* n = TextFormat("%d / %d", g.got, (int)g.level.stars.size());
        drawStar({24 * ui + 6, 26 * ui}, 14 * ui, 0, kStar, 2.2f);
        DrawText(n, (int)(46 * ui), (int)(15 * ui), fs, kInk);
        leftX = 46 * ui + MeasureText(n, fs) + 24 * ui;
    }
    auto heldKey = [&](Color col, bool glow = false) {   // a key held, until a door takes it (the golden one glowing still)
        float s = 1.25f * ui, w = kKeyLen * s;
        if (glow) drawKeyGlow({leftX + 6 * ui + w / 2, 15 * ui + fs / 2.0f}, s, g.time, g.t.goldKeyGlow);
        drawKey({leftX + 6 * ui + w / 2, 15 * ui + fs / 2.0f}, 0, s, col);
        leftX += w + 16 * ui;
    };
    if (g.keyHeld()) heldKey(g.t.keyCol[0]);
    if (g.hasBlueKey) heldKey(g.t.keyCol[1]);
    if (g.hasGreenKey) heldKey(g.t.keyCol[2]);
    if (g.hasPurpleKey) heldKey(g.t.keyCol[3]);
    if (g.hasGoldKey) heldKey(g.t.keyCol[4], true);
    // The clocks, each still shown once stopped: the run's (easy mode), stopped at the yellow flag;
    // from the red key, hard mode's, stopped at the red flag; from the green key, dark mode's,
    // stopped at the green flag; from the blue key, harder mode's, stopped at the blue flag; from
    // the violet flag, cameleon mode's. Each named by its mode, RUN for MODE (EASY RUN, HARD RUN, DARK
    // RUN, HARDER RUN, CAMELEON RUN -- as the golden run's SOLO RUN). (A level without an exit has no modes: its clock goes unnamed.)
    auto clockText = [](float t, char* buf, size_t n) { snprintf(buf, n, "%02d:%02d.%d", (int)(t / 60), (int)t % 60, (int)(t * 10) % 10); };
    char tt[32], ht[32], dt[32], rt[32], ct[32], easy[48], hard[48], dark[48], harder[48], cam[48];
    clockText(g.runTime, tt, sizeof tt);
    clockText(g.hardTime, ht, sizeof ht);
    clockText(g.darkTime, dt, sizeof dt);
    clockText(g.harderTime, rt, sizeof rt);
    clockText(g.camTime, ct, sizeof ct);
    snprintf(harder, sizeof harder, "HARDER RUN  %s", rt);
    snprintf(cam, sizeof cam, "CAMELEON RUN  %s", ct);
    snprintf(easy, sizeof easy, "%s%s", g.level.hasExit ? "EASY RUN  " : "", tt);
    snprintf(hard, sizeof hard, "HARD RUN  %s", ht);
    snprintf(dark, sizeof dark, "DARK RUN  %s", dt);
    // Each digit in a cell as wide as the widest one ("1" takes the room of "9"), centred: a
    // running clock keeps its width, and its label stays put.
    Font df = GetFontDefault();
    float sp = (float)(fs / 10);                          // DrawText's spacing for the default font
    auto glyphW = [&](char c) { char one[2] = {c, 0}; return MeasureTextEx(df, one, (float)fs, sp).x; };
    float cell = 0;
    for (char d = '0'; d <= '9'; d++) cell = fmaxf(cell, glyphW(d));
    auto charW = [&](char c) { return isdigit((unsigned char)c) ? cell : glyphW(c); };
    // A clock running near its limit (clock_limit): red, blinking clock_warning_blinks times a second
    // in its last clock_warning_time seconds, clock_alarm_blinks in its last clock_alarm_time.
    // (a: how much of it shows; rainbow: its letters in the title's rolling rainbow, as the golden run's.)
    auto clockLine = [&](const char* s, float y, int k, float a = 1, bool rainbow = false) {
        if (a <= 0) return;
        Color col = kInk;
        float left = g.t.clockLimit - g.clockOf(k);
        if (g.clockRuns(k) && left <= g.t.clockWarnT) {
            float rate = left <= g.t.clockAlarmT ? g.t.clockAlarmBlinks : g.t.clockWarnBlinks;
            if (fmodf(g.clockOf(k) * rate, 1.0f) >= 0.5f) return;   // (blinked out)
            col = kFlagRed; rainbow = false;
        }
        float w = -sp;
        for (const char* c = s; *c; c++) w += charW(*c) + sp;
        float x = sw - w - 18 * ui;
        const int n = (int)strlen(s);
        for (int i = 0; i < n; i++) {
            char one[2] = {s[i], 0};
            Color ci = rainbow ? rainbowAt((float)i / n - g.time * g.t.scoresRainbow) : col;
            DrawTextEx(df, one, {roundf(x + (charW(s[i]) - glyphW(s[i])) / 2), roundf(y)}, (float)fs, sp, fade(ci, a));   // on whole pixels: crisp
            x += charW(s[i]) + sp;
        }
    };
    float clockY = 15 * ui;
    if (g.goldenRun) {                                   // a golden run: its clock only, in a rainbow -- fading in as he is let go
        char gt[32], golden[48];
        clockText(g.goldenTime, gt, sizeof gt);
        snprintf(golden, sizeof golden, "SOLO RUN  %s", gt);
        float ga = g.goldenClockAt >= 0 ? clampf((g.time - g.goldenClockAt) / g.t.goldenClockIn, 0, 1) : g.goldenSeq >= 0 ? 0 : 1;
        if (clocks) clockLine(golden, clockY, 5, ga, true);
    } else {
        // (The golden key taken -- the first: the modes' clocks fade out, golden_clocks_fade_out.)
        bool goldTaken = std::find(g.goldKeyTaken.begin(), g.goldKeyTaken.end(), true) != g.goldKeyTaken.end();
        float ma = !goldTaken ? 1 : g.goldKeyAt < 0 ? 0 : 1 - clampf((g.time - g.goldKeyAt) / g.t.goldenClocksOut, 0, 1);
        if (clocks && g.startPart == 0) clockLine(easy, clockY, 0, ma);
        else clockY -= fs + 10 * ui;                     // (start_level: easy mode not run -- the next clock takes its line)
        if (g.hardMode) clockLine(hard, clockY += fs + 10 * ui, 1, ma);
        if (g.darkMode) clockLine(dark, clockY += fs + 10 * ui, 2, ma);
        if (g.harderMode) clockLine(harder, clockY += fs + 10 * ui, 3, ma);
        if (g.camMode) clockLine(cam, clockY += fs + 10 * ui, 4, ma);
    }

    float lessonY = gameH() * 0.22f;        // the lessons' line -- below the goal's text, if any
    if (g.level.hasExit) {
        // Each part reached shows its highscores where it ends -- by its flag, once touched (easy mode
        // without a yellow flag: by the red key, done; hard mode without a red flag: at the start,
        // done) -- fading out with the distance, and only till the next part's end is reached: over
        // them EASY, HARD, DARK, HARDER, CAMELEON MODE COMPLETED -- or, the mode not run (a debug
        // key), just its name. A new highscore's name is typed there, by its flag: its table shows
        // only there too.
        auto nearA = [&](Vector2 p) {
            float reach = g.t.goalTextDist, d = Vector2Distance(g.centreW(), p) / kTile;
            return clampf((reach - d) / (reach / 3.0f), 0, 1);
        };
        // (The golden run's: by the black flag, once touched -- a level without one: by the golden key,
        // once taken again. In a golden run, its own only.)
        const Level::Flag* fl = g.level.flags;
        bool done[Scores::kParts] = {g.finished && g.startPart == 0, g.hardDone, g.darkDone, g.harderDone, g.camDone, g.goldenDone};
        const int flagOf[Scores::kParts - 1] = {flYellow, flRed, flGreen, flBlue, flViolet};
        bool reached[Scores::kParts], placed[Scores::kParts];
        Vector2 at[Scores::kParts];
        for (int k = 0; k < Scores::kParts; k++) {
            if (k == 5) { placed[k] = g.partPlace(k, at[k]); reached[k] = g.goldenDone || g.blackFlagTouched; continue; }
            const Level::Flag& f = fl[flagOf[k]];
            placed[k] = f.has || k < 2;
            reached[k] = !g.goldenRun && (f.has ? g.flagTouched[flagOf[k]] : done[k]);
            at[k] = f.has ? Game::middle(f.rect) : k == 0 ? Game::middle(g.level.exitRect) : g.startCentre();
        }
        int part = -1;
        float a = 0;
        for (int k = 0; k < Scores::kParts; k++) {
            if (!reached[k] || !placed[k] || (k + 1 < Scores::kParts && reached[k + 1])) continue;
            float na = nearA(at[k]);
            if (na > a) { a = na; part = k; }
        }
        if (en.open) { part = en.part; a = g.partNear(part); }
        if (part >= 0 && a > 0) {
            static const char* title[Scores::kParts] = {"EASY MODE COMPLETED !", "HARD MODE COMPLETED !", "DARK MODE COMPLETED !",
                                                        "HARDER MODE COMPLETED !", "CAMELEON MODE COMPLETED !", "SOLO RUN COMPLETED !"};
            int big = (int)(44 * ui), rs = (int)(30 * ui), hs = (int)(24 * ui);
            // All of it in the title's rainbow, rolling: the title the first line, HIGHSCORE the
            // second, then the five lines (in the dark, paler: read on black).
            auto rb = [&](int line) {
                return fade(ColorLerp(rainbowLine(line, g.time, g.t.scoresRainbow), kPaper, 0.5f * g.dark), a);
            };
            // The title at the top, each letter riding its own point of a wave, as "Press A" does:
            // goal_text_wave_amplitude letter heights (a capital's: 7 of the font's 10 rows) up and
            // down, goal_text_wave_period a round -- its highest letters just under the top of the screen.
            const char* tt = done[part] ? title[part] : TextFormat("= %s =", Scores::title(part));
            float wsp = big / 10.0f, amp = g.t.goalWave * big * 0.7f, yc = 16 * ui + amp;
            float tx = (sw - MeasureTextEx(df, tt, (float)big, wsp).x) / 2;
            for (int i = 0; tt[i]; i++) {
                char one[2] = {tt[i], 0};
                float dy = amp * sinf(2 * PI * g.time / g.t.goalWavePeriod - i * 0.7f);
                DrawTextEx(df, one, {roundf(tx), roundf(yc + dy)}, (float)big, wsp, rb(0));
                tx += MeasureTextEx(df, one, (float)big, wsp).x + wsp;
            }
            lessonY = fmaxf(lessonY, yc + amp + big + 18 * ui);
            // At the bottom of the screen, HIGHSCORE in bold, then the five lines, name and time. A
            // new one goes in at its place, its name as typed so far (the next letter's slot
            // blinking), in bold -- and stays so once kept.
            std::vector<Scores::Entry> rows = sc.part[part];
            int mine = en.part == part ? en.place : -1;
            if (en.open) {
                std::string nm;
                for (int i = 0; i < 3; i++) nm += i < en.len ? en.name[i] : i == en.len && fmodf(en.clock, 0.8f) >= 0.55f ? ' ' : '_';
                rows.insert(rows.begin() + std::min(mine, (int)rows.size()), {nm, en.time});
            }
            float nameW = nameWidth(df, (float)rs), timeW = (float)MeasureText("00:00.0", rs), gap = rs * 1.5f;
            float x0 = (sw - (nameW + gap + timeW)) / 2;
            float hb = 1.5f * hs;                                // (HIGHSCORE: half as big again)
            float y = gameH() - 30 * ui - (Scores::kRows - 1) * rs * 1.25f - rs - hb - 0.6f * hs;
            drawBold(df, "HIGHSCORE", {(sw - boldWidth(df, "HIGHSCORE", hb, hb / 10.0f)) / 2, y}, hb, hb / 10.0f, rb(1));
            y += hb + 0.6f * hs;
            for (int i = 0; i < Scores::kRows; i++) {
                const Scores::Entry* e = i < (int)rows.size() ? &rows[i] : nullptr;
                char tb[32];
                if (e) scoreTime(e->t, tb, sizeof tb); else snprintf(tb, sizeof tb, "--:--.-");
                float bold = i == mine ? rs / 10.0f : 0;
                drawName(df, e ? e->name.c_str() : "---", {x0, y}, (float)rs, bold, rb(2 + i));
                drawBold(df, tb, {x0 + nameW + gap, y}, (float)rs, bold, rb(2 + i));
                y += rs * 1.25f;
            }
        }
    } else if (g.finished) {
        const char* msg = "ALL STARS!";
        int big = (int)(56 * ui);
        int w = MeasureText(msg, big);
        DrawText(msg, (sw - w) / 2, (int)(90 * ui), big, kInk);
        const char* sub = TextFormat("%s  -  Shift+R to play again", tt);
        DrawText(sub, (sw - MeasureText(sub, fs)) / 2, (int)(90 * ui) + big + 6, fs, kInk);
        lessonY = fmaxf(lessonY, 90 * ui + big + 6 + fs + 18 * ui);
    }

    // An ability's lesson (a fruit eaten, the hat taken): it fades in, stays lesson_show_time,
    // then fades out.
    const char* lesson = nullptr;
    float lessonA = 0;
    if (g.lessonText && g.lessonT >= 0) {
        float in = g.t.lessonIn, show = g.t.lessonShow, u = g.lessonT;
        lessonA = u < in ? u / in : u < in + show ? 1 : 1 - (u - in - show) / g.t.lessonOut;
        if (lessonA > 0) lesson = g.lessonText;
    }
    if (lesson && lessonA > 0) {
        int big = (int)(34 * ui);
        Color ink = ColorLerp(kInk, kPaper, g.dark);
        const char* w = g.lessonWord ? strstr(lesson, g.lessonWord) : nullptr;
        if (w) {                                         // "<Colour> key !", the word in its colour
            std::string part[3] = {std::string(lesson, w), g.lessonWord, w + strlen(g.lessonWord)};
            int sp = big / 10, tw = -sp;                 // (the default font: size/10 between letters)
            for (auto& p : part) tw += MeasureText(p.c_str(), big) + sp;
            int x = (sw - tw) / 2;
            for (int k = 0; k < 3; k++) {
                DrawText(part[k].c_str(), x, (int)lessonY, big, fade(k == 1 ? g.lessonCol : ink, lessonA));
                x += MeasureText(part[k].c_str(), big) + sp;
            }
        } else {
            DrawText(lesson, (sw - MeasureText(lesson, big)) / 2, (int)lessonY, big, fade(ink, lessonA));
        }
        if (g.lessonSub) {                               // under it, smaller: how, in its colours (howLine)
            int small = (int)(big * 0.8f);
            float y = lessonY + big * 1.3f;
            float w = howLine(g.lessonSub, 0, y, small, lessonA, ink, false);
            howLine(g.lessonSub, (sw - w) / 2, y, small, lessonA, ink, true);
        }
    } else if (g.cpMsg > 0) {
        const char* msg = "CHECKPOINT";
        int big = (int)(40 * ui);
        float a = clampf(g.cpMsg / 0.4f, 0, 1);
        DrawText(msg, (sw - MeasureText(msg, big)) / 2, (int)lessonY, big, fade(kInk, a));
    }

    // Debug mode (a debug key used, a start_with_* on, start_level past easy mode): DEBUG MODE in
    // the bottom right corner, in a rainbow rolling along its letters (once round every two seconds).
    if (g.debugMode()) {
        const char* s = "DEBUG MODE";
        int n = (int)strlen(s);
        float x = sw - MeasureTextEx(df, s, (float)fs, sp).x - 18 * ui, y = gameH() - fs - 15 * ui;
        for (int j = 0; j < n; j++) {
            char one[2] = {s[j], 0};
            DrawTextEx(df, one, {roundf(x), roundf(y)}, (float)fs, sp, rainbowAt((float)j / n - g.time * 0.5f));
            x += glyphW(s[j]) + sp;
        }
        // ...and in the bottom left, the game's own load (perf.h): its frames a second, its processor
        // time, its memory -- read twice a second, so they can be read.
        static double next = 0;
        static int fps = 0;
#ifdef __EMSCRIPTEN__
        if (GetTime() >= next) { next = GetTime() + 0.5; fps = GetFPS(); }
        const char* line[] = {TextFormat("FPS : %d per sec", fps)};
#else
        static float cpu = 0, mem = 0;
        if (GetTime() >= next) { next = GetTime() + 0.5; fps = GetFPS(); cpu = Perf::cpuPercent(); mem = Perf::memoryMB(); }
        const char* line[3] = {TextFormat("FPS : %d per sec", fps), TextFormat("CPU : %.0f%%", cpu), TextFormat("MEM : %.0fMB", mem)};
#endif
        const int lines = sizeof(line) / sizeof(line[0]);
        int ss = (int)(18 * ui);
        Color ink = ColorLerp(kInk, kPaper, g.dark);
        for (int j = 0; j < lines; j++) {
            float ly = gameH() - 15 * ui - (lines - j) * ss - (lines - 1 - j) * 6 * ui;
            DrawText(line[j], (int)(18 * ui), (int)ly, ss, ink);   // (no plate: in the engraving's dark, white on it)
        }
    }

    if (g.hintTimer > 0 && g.t.titleHelp) {
        float a = clampf(g.hintTimer, 0, 1);
        const char* lines[kHintLines] = {
            TextFormat("Run: Left/Right or stick      Jump: Space, %s or (A) -- again in mid-air", kJumpKeyName),
            "Look: Up/Down      Look far: Alt+arrows or Right Stick",
            "Tongue: X or (B)      Swallow it back: V or (RT)      Pay it out: C or (LT), held",
            "Shift+T: screenshot    Shift+R: restart    Shift+F: fullscreen    Shift+S: look    Shift+B: bug report"};
        int hs = (int)(16 * ui);
        float y = hintTop();
        for (int i = 0; i < kHintLines; i++)
            DrawText(lines[i], (sw - MeasureText(lines[i], hs)) / 2, (int)(y + i * 22 * ui), hs,
                     fade(kInk, a * (i < kHintLines - 1 ? 0.85f : 0.6f)));
    }
}

// ESC's question over the screen fading to black (a: 0..1): the question, and under it YES and NO,
// the one picked in bold, the other dimmer.
// The end credits (assets/endcredits.txt), rolling up the right two thirds of the screen at the
// ending (Game::endCreditsT): markdown of a kind, a line each -- "# " a title, "## " a subtitle, any
// other line normal text, an empty one a gap; **bold**, *italic*, ***both***, {colour}...{/} (a name
// -- red orange yellow green blue violet pink gold grey black white ink -- or #rrggbb); \* a star;
// ^^ a red heart as tall as a capital (the ending's, ending_heart_color; \^^ the two marks);
// ![0.5](assets/a picture.png) a picture (PNG), centred, its width that share of the two thirds' (its
// height as its own proportions make it); <!-- ... --> left out, over lines too. Sizes and heights on
// a 720-line screen.
struct CreditSpan { std::string s; bool bold = false, italic = false; Color col = kInk; bool heart = false; };
struct CreditLine {
    float size = 22, height = 34;
    std::vector<CreditSpan> spans;
    float imgW = 0;                            // a picture: its width, a share of the two thirds (0: text)
    Texture2D tex{};
};
static std::vector<CreditLine> gCredits;
static Color creditColour(const std::string& name, Color was) {
    static const std::pair<const char*, Color> named[] = {
        {"red", {214, 52, 40, 255}}, {"orange", {236, 132, 28, 255}}, {"yellow", {222, 178, 20, 255}}, {"green", {58, 146, 64, 255}},
        {"blue", {58, 110, 200, 255}}, {"violet", {140, 66, 200, 255}}, {"pink", {226, 104, 160, 255}}, {"gold", {214, 172, 32, 255}},
        {"grey", {128, 128, 128, 255}}, {"gray", {128, 128, 128, 255}}, {"black", kInk}, {"ink", kInk}, {"white", kPaper}};
    for (const auto& n : named) if (name == n.first) return n.second;
    unsigned rgb;
    if (name.size() == 7 && name[0] == '#' && sscanf(name.c_str() + 1, "%6x", &rgb) == 1)
        return {(unsigned char)(rgb >> 16), (unsigned char)(rgb >> 8 & 255), (unsigned char)(rgb & 255), 255};
    return was;                                // (not a colour known: as it was)
}
// A credits line's height on a 720-line screen as wide as `aspect` says: a picture's by its width (and
// 6 px above and below it), a line of text's by its size.
static float creditLineH(const CreditLine& L, float aspect) {
    if (L.imgW <= 0) return L.height;
    if (L.tex.id == 0) return 0;
    return L.imgW * 720 * aspect * 2 / 3 * L.tex.height / L.tex.width + 12;
}
static float creditsHeight(float aspect) {
    float h = 0;
    for (const CreditLine& L : gCredits) h += creditLineH(L, aspect);
    return h;
}
static void loadCredits(const char* path) {
    gCredits.clear();
    FILE* f = fopen(path, "rb");
    if (!f) { TraceLog(LOG_WARNING, "CREDITS: cannot open %s", path); return; }
    std::string text;
    char buf[4096];
    for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
    fclose(f);
    for (size_t a; (a = text.find("<!--")) != std::string::npos;) {   // comments out (over lines too)
        size_t b = text.find("-->", a);
        text.erase(a, b == std::string::npos ? std::string::npos : b + 3 - a);
    }
    size_t at = 0;
    while (at <= text.size()) {
        size_t e = text.find('\n', at);
        std::string line = text.substr(at, e == std::string::npos ? std::string::npos : e - at);
        at = e == std::string::npos ? text.size() + 1 : e + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t lead = line.find_first_not_of(" \t");
        line = lead == std::string::npos ? std::string() : line.substr(lead);
        CreditLine L;
        size_t mid = line.find("]("), close = line.rfind(')');
        if (line.rfind("![", 0) == 0 && mid != std::string::npos && close != std::string::npos && close > mid + 2) {   // a picture
            float w = (float)atof(line.substr(2, mid - 2).c_str());
            L.imgW = clampf(w > 0 ? w : 0.5f, 0.01f, 1);
            std::string file = line.substr(mid + 2, close - mid - 2);
            std::string data;
            if (FILE* fi = fopen(file.c_str(), "rb")) {   // (read as the game reads its files: baked in too, by make dist)
                for (size_t n; (n = fread(buf, 1, sizeof buf, fi)) > 0;) data.append(buf, n);
                fclose(fi);
            }
            Image im = data.empty() ? Image{} : LoadImageFromMemory(GetFileExtension(file.c_str()), (const unsigned char*)data.data(), (int)data.size());
            if (im.data) {
                L.tex = LoadTextureFromImage(im);
                SetTextureFilter(L.tex, TEXTURE_FILTER_BILINEAR);
                UnloadImage(im);
            } else TraceLog(LOG_WARNING, "CREDITS: cannot load the picture %s", file.c_str());
            gCredits.push_back(L);
            continue;
        }
        if (line.rfind("## ", 0) == 0) { L.size = 30; L.height = 48; line = line.substr(3); }
        else if (line.rfind("# ", 0) == 0) { L.size = 44; L.height = 66; line = line.substr(2); }
        else if (line.empty()) L.height = 22;
        bool bold = false, italic = false;
        std::vector<Color> cols = {kInk};
        std::string cur;
        auto flush = [&] { if (!cur.empty()) { L.spans.push_back({cur, bold, italic, cols.back()}); cur.clear(); } };
        for (size_t i = 0; i < line.size();) {
            if (line[i] == '\\' && i + 1 < line.size()) { cur += line[i + 1]; i += 2; continue; }
            if (line.compare(i, 2, "^^") == 0) { flush(); L.spans.push_back({"", bold, italic, cols.back(), true}); i += 2; continue; }
            if (line.compare(i, 3, "***") == 0) { flush(); bold = !bold; italic = !italic; i += 3; continue; }
            if (line.compare(i, 2, "**") == 0) { flush(); bold = !bold; i += 2; continue; }
            if (line[i] == '*') { flush(); italic = !italic; i++; continue; }
            size_t brace = line[i] == '{' ? line.find('}', i) : std::string::npos;
            if (brace != std::string::npos) {
                std::string name = line.substr(i + 1, brace - i - 1);
                flush();
                if (name == "/") { if (cols.size() > 1) cols.pop_back(); }
                else cols.push_back(creditColour(name, cols.back()));
                i = brace + 1;
                continue;
            }
            cur += line[i++];
        }
        flush();
        gCredits.push_back(L);
    }
    while (!gCredits.empty() && gCredits.back().spans.empty() && gCredits.back().imgW <= 0) gCredits.pop_back();   // (no gap at the end)
    TraceLog(LOG_INFO, "CREDITS: %d lines, %.0f px high on a 720-line screen (16:9)", (int)gCredits.size(), creditsHeight(16.0f / 9));
}
// A heart's size (drawHeart's s) to stand as tall as a capital of text `size` (0.7 of it, the default
// font's): its lobes' top to its tip, 0.88 of s. As wide as 1.04 of s.
static float creditHeartS(float size) { return 0.7f * size / 0.88f; }
static float creditSpanW(Font f, const CreditSpan& s, float size) {
    if (s.heart) return 1.04f * creditHeartS(size);
    return boldWidth(f, s.s.c_str(), size, s.bold ? size / 10 : 0);
}
// A span at `at` (its top left): text -- or a heart, upright, on the capitals' middle (as "Game Over"'s, drawEnd).
static void drawCreditSpan(Font f, const CreditSpan& s, Vector2 at, float size, Color heartCol) {
    if (s.heart) {
        const float hs = creditHeartS(size);
        drawHeart({at.x + 0.52f * hs, at.y + 0.43f * size - 0.04f * hs}, hs, heartCol);
        return;
    }
    float bold = s.bold ? size / 10 : 0;
    if (!s.italic) { drawBold(f, s.s.c_str(), at, size, bold, s.col); return; }
    rlPushMatrix();                            // italic: leaning, sheared from its baseline
    rlTranslatef(roundf(at.x), roundf(at.y + size), 0);
    const float shear[16] = {1, 0, 0, 0, -0.22f, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    rlMultMatrixf(shear);
    drawBold(f, s.s.c_str(), {0, -size}, size, bold, s.col);
    rlPopMatrix();
}
static void drawCredits(const Game& g) {
    float ct = g.endCreditsT();
    if (ct < 0 || gCredits.empty()) return;
    const int sw = gameW(), sh = gameH();
    const float ui = sh / 720.0f, cx = sw * 2.0f / 3.0f, aspect = (float)sw / sh;   // (the middle of the right two thirds)
    float y = sh - ct * g.t.endCreditsSpeed * ui;
    Font f = GetFontDefault();
    for (const CreditLine& L : gCredits) {
        const float size = L.size * ui, h = creditLineH(L, aspect) * ui;
        if (y < sh && y + h > 0) {
            if (L.imgW > 0 && L.tex.id != 0) {
                float w = L.imgW * sw * 2 / 3.0f;
                DrawTexturePro(L.tex, {0, 0, (float)L.tex.width, (float)L.tex.height}, {cx - w / 2, y + 6 * ui, w, h - 12 * ui}, {0, 0}, 0, WHITE);
            } else if (!L.spans.empty()) {
                float w = 0;
                for (const CreditSpan& s : L.spans) w += creditSpanW(f, s, size) + size / 10;
                float x = cx - (w - size / 10) / 2;
                for (const CreditSpan& s : L.spans) {
                    drawCreditSpan(f, s, {x, y + (h - size) / 2}, size, g.t.endHeartCol);
                    x += creditSpanW(f, s, size) + size / 10;
                }
            }
        }
        y += h;
    }
}
// The end (Game::endBlack): the screen black -- the music on --, and as the questions are shown, "Game
// Over" between two red hearts as tall as its capitals (Game::endMsg: or "GAME 100% OVER" between two
// a side, "GAME OVER (AGAIN)" between three); under it, smaller, the way back to the title, its
// commands in colours as a lesson's (howLine).
static void drawEnd(const Game& g) {
    float a = g.endBlack();
    if (a <= 0) return;
    const int sw = gameW(), sh = gameH();
    const float ui = sh / 720.0f, qs = 56 * ui;
    Font f = GetFontDefault();
    DrawRectangle(0, 0, sw, sh, fade(kBlack, a));
    const char* q = g.endMsg == 1 ? "GAME 100% OVER" : g.endMsg == 2 ? "GAME OVER (AGAIN)" : "Game Over";
    const float qw = MeasureTextEx(f, q, qs, qs / 10).x, cap = 0.7f * qs, hw = cap / 0.88f, gap = 0.45f * qs;
    const float x = (sw - qw) / 2, y = roundf(sh * 0.38f), mid = y + 0.43f * qs;   // (mid: the capitals' middle)
    const int hearts = g.endMsg == 1 ? 2 : g.endMsg == 2 ? 3 : 1;
    for (int k = 0; k < hearts; k++) drawHeart({x - gap - hw / 2 - k * hw * 1.2f, mid - 0.04f * hw}, hw, fade(g.t.endHeartCol, a));
    DrawTextEx(f, q, {roundf(x), y}, qs, qs / 10, fade(kPaper, a));
    for (int k = 0; k < hearts; k++) drawHeart({x + qw + gap + hw / 2 + k * hw * 1.2f, mid - 0.04f * hw}, hw, fade(g.t.endHeartCol, a));
    const char* how = "{i}Go to menu (Joypad {A}{i} or Keyboard {b}<space>{i})";
    const int os = (int)(40 * ui * 0.7f);
    float w = howLine(how, 0, 0, os, a, kPaper, false);
    howLine(how, (sw - w) / 2, sh * 0.38f + qs * 1.8f, os, a, kPaper, true);
}

static void drawAsk(const char* q, bool yes, float a) {
    int sw = gameW(), sh = gameH();
    float ui = sh / 720.0f, qs = 56 * ui, os = 40 * ui;
    Font f = GetFontDefault();
    DrawRectangle(0, 0, sw, sh, fade(kBlack, a));
    Vector2 qm = MeasureTextEx(f, q, qs, qs / 10);
    DrawTextEx(f, q, {roundf((sw - qm.x) / 2), roundf(sh * 0.38f)}, qs, qs / 10, fade(kPaper, a));
    const char* opt[2] = {"YES", "NO"};
    for (int k = 0; k < 2; k++) {
        bool on = (k == 0) == yes;
        float bold = on ? os / 10 : 0;                   // (bold: drawn again a font pixel further, the letters as much apart)
        float w = MeasureTextEx(f, opt[k], os, os / 10 + bold).x + bold;
        float cx = sw / 2.0f + (k == 0 ? -1 : 1) * os * 2.2f;
        for (float b : {0.0f, bold})
            DrawTextEx(f, opt[k], {roundf(cx - w / 2 + b), roundf(sh * 0.38f + qs * 1.8f)}, os, os / 10 + bold, fade(kPaper, a * (on ? 1 : 0.55f)));
    }
}

// The restart's countdown (Shift+Ctrl+R), `left` seconds to go: 3, 2, 1 (the game held), GO, over
// Leon -- its bottom middle at `at` (screen) -- white outlined in black (seen on paper and in the
// dark alike), each landing from a little larger, then fading out at the end of its second.
static void drawCountdown(float left, Vector2 at) {
    int n = (int)ceilf(left);                           // 4, 3, 2: the digits 3, 2, 1 -- 1: GO
    float u = n - left;                                 // 0 -> 1 through its second
    float land = clampf(u / 0.2f, 0, 1);
    float a = clampf((1 - u) / 0.25f, 0, 1);
    int sw = gameW(), sh = gameH();
    Font f = GetFontDefault();
    const char* s = n > 1 ? TextFormat("%d", n - 1) : "GO";
    float size = sh * 0.1f * (1.3f - 0.3f * land * land * (3 - 2 * land));
    Vector2 m = MeasureTextEx(f, s, size, size / 10);
    Vector2 p = {at.x - m.x / 2, at.y - m.y};
    (void)sw;
    float o = fmaxf(2.0f, size / 28.0f);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) DrawTextEx(f, s, {p.x + dx * o, p.y + dy * o}, size, size / 10, fade(kBlack, a));
    DrawTextEx(f, s, p, size, size / 10, fade(WHITE, a));
}

// What the title keeps clear of, on screen: the ground Leon stands on (the bottom of its line), the
// top of Leon (hat and all) and half his height -- and any ground poking down into the title from
// above (groundAbove: how far down a box must go to clear it).
struct TitleClear {
    float floorY, leonTop, leonHalf;
    std::function<float(Rectangle)> groundAbove;
};

// The title card, over the game held at the start: GRAVITY LEON across the whole width, grey
// letters outlined in black, clear of any ground poking into it from above by half that ground's
// height; "Press A" between it and Leon, half a Leon above him at its lowest; the ground under Leon
// plain light grey, the highscores on it, between its line and the credits, small at the bottom,
// above the help if shown (title_help).
// (golden: the golden run's table too -- one completed since the game was launched.)
// `who`: whose title -- 0 Leon's game, as ever; 1 Anna's: GRAVITY ANNA, ANNA's letters a rainbow rolling
// through them (title_anna_rainbow_speed), and Leon's highscores and hers (annaSc, annaGolden) in turn,
// title_scores_switch_time each, the one scrolling away left as the other comes in, over
// title_scores_scroll_time; 2 the two finished: the same, and "Press Y for Leon, X for Anna".
static void drawTitle(float clock, const Tune& tn, const Scores& sc, const TitleClear& tc, float introA = 1, float restA = 1,
                      float scoresA = 1, bool golden = false, int who = 0, const Scores* annaSc = nullptr, bool annaGolden = false) {
    const float a = restA;                               // (all but the title and its ground: title_fade_in_rest_time -- the highscores: scoresA)
    int sw = gameW(), sh = gameH();
    Font f = GetFontDefault();
    float floorY = tc.floorY;
    DrawRectangleRec({0, floorY, (float)sw, (float)sh}, kTitleGround);   // (from the line's very edge: no row of the world's hatching between them)
    const char* title = who ? "GRAVITY ANNA" : "GRAVITY LEON";
    float spacing = 0.12f;
    float size = 10;
    Vector2 m = MeasureTextEx(f, title, size, size * spacing);
    size *= (sw * 0.94f) / m.x;                          // span the screen
    m = MeasureTextEx(f, title, size, size * spacing);
    // (The two finished: LEON and ANNA in turn -- title_name_switch_time each, morphing into the other
    // over title_name_morph_time: the title's size going from the one's to the other's, as wide.)
    const char* names[2] = {"GRAVITY LEON", "GRAVITY ANNA"};
    float morph = 0;                                     // (0 LEON, 1 ANNA)
    if (who == 2) {
        const float S = tn.titleNameShow, M = tn.titleNameMorph, ph = fmodf(clock, 2 * (S + M));
        morph = ph < S ? 0 : ph < S + M ? (ph - S) / M : ph < 2 * S + M ? 1 : 1 - (ph - 2 * S - M) / M;
        float sz[2];
        for (int k = 0; k < 2; k++) sz[k] = 10 * (sw * 0.94f) / MeasureTextEx(f, names[k], 10, 10 * spacing).x;
        size = lerpf(sz[0], sz[1], smooth01(morph));
        m = MeasureTextEx(f, names[0], size, size * spacing);
        m.x = lerpf(m.x, MeasureTextEx(f, names[1], size, size * spacing).x, smooth01(morph));
    }
    Vector2 p = {(sw - m.x) / 2, sh * 0.06f};             // at the top: the highscores under it
    float o = fmaxf(2.0f, size / 22.0f);                 // outline thickness
    // The letters as inked: capitals from the font's second row down to its eighth, the outline round them.
    Rectangle ink = {p.x - o, p.y + size / 10 - o, m.x + 2 * o, size * 0.7f + 2 * o};
    for (int k = 0; k < 4; k++) {                        // (moved down: maybe into more ground)
        float d = tc.groundAbove(ink);
        if (d <= 0) break;
        p.y += d; ink.y += d;
    }
    Color edge = kBlack, fill = {150, 150, 156, 255};
    if (who == 2) {                                      // GRAVITY, then the name: each letter morphing into the other's in
        const char* pre = "GRAVITY";                     // turn, left to right -- the one fading out as the other fades in,
        for (int dy = -1; dy <= 1; dy++)                 // both squeezed a little, each sliding from its place to the other's
            for (int dx = -1; dx <= 1; dx++)
                if (dx || dy) DrawTextEx(f, pre, {p.x + dx * o, p.y + dy * o}, size, size * spacing, edge);
        DrawTextEx(f, pre, p, size, size * spacing, fill);
        for (int i = 8; i < 12; i++) {
            const float u = smooth01(morph * 1.75f - (i - 8) * 0.25f), squeeze = 1 - 0.35f * sinf(PI * u);
            float x[2];
            for (int k = 0; k < 2; k++) x[k] = p.x + MeasureTextEx(f, std::string(names[k], i).c_str(), size, size * spacing).x + size * spacing;
            const float lx = lerpf(x[0], x[1], u), mid = p.y + size * 0.45f;
            for (int k = 0; k < 2; k++) {
                const float al = k ? u : 1 - u;
                if (al <= 0.003f) continue;
                char one[2] = {names[k][i], 0};
                const Color c = k ? rainbowAt((i - 8) / 4.0f - clock * tn.titleAnnaRainbow) : fill;
                rlPushMatrix();
                rlTranslatef(0, mid, 0);
                rlScalef(1, squeeze, 1);
                rlTranslatef(0, -mid, 0);
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if (dx || dy) DrawTextEx(f, one, {lx + dx * o, p.y + dy * o}, size, size * spacing, fade(edge, al));
                DrawTextEx(f, one, {lx, p.y}, size, size * spacing, fade(c, al));
                rlPopMatrix();
            }
        }
    } else {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (dx || dy) DrawTextEx(f, title, {p.x + dx * o, p.y + dy * o}, size, size * spacing, edge);
        DrawTextEx(f, title, p, size, size * spacing, fill);
    }
    if (who == 1)                                        // ANNA: each letter its colour in a rainbow rolling through them
        for (int i = 8; i < 12; i++) {
            char one[2] = {title[i], 0};
            float lx = p.x + MeasureTextEx(f, std::string(title, i).c_str(), size, size * spacing).x + size * spacing;
            DrawTextEx(f, one, {lx, p.y}, size, size * spacing, rainbowAt((i - 8) / 4.0f - clock * tn.titleAnnaRainbow));
        }
    // From black: the title and all behind it -- the game held, the ground -- fading in over
    // title_fade_in_time (introA); the rest of it after that (restA).
    if (introA < 1) DrawRectangle(0, 0, sw, sh, fade(kBlack, 1 - introA));
    const char* credits = "Design and Art by Niko/Factory, Music by tno/Factory, Vibecoded with Claude - Fall 2026";
    float cs = fmaxf(14.0f, sh / 48.0f);
    Vector2 cm = MeasureTextEx(f, credits, cs, cs * 0.1f);
    float footerBottom = tn.titleHelp ? hintTop() : sh;
    float above = footerBottom - cm.y - 18 * fmaxf(1.0f, sh / 720.0f);   // clear of the help lines
    DrawTextEx(f, credits, {(sw - cm.x) / 2, above}, cs, cs * 0.1f, fade(kInk, a));

    // The highscores, the five parts side by side under HIGHSCORE -- easy, hard, dark in the
    // middle, harder, cameleon. In a rainbow: each line one colour, the whole rainbow over the seven
    // lines, rolling down them (title_scores_rainbow_speed) -- and each table one line's colour on
    // from the one on its left: the rainbow runs across the tables too.
    float ui = sh / 720.0f;
    float hs = 20 * ui, rs = 28 * ui;
    floorY = clampf(floorY, ink.y + ink.height, above);   // (the ground off the screen: held between the title and the credits)
    auto rainbow = [&](int line, float alpha) { return fade(rainbowLine(line, clock, tn.scoresRainbow), alpha); };
    // (bold: drawn again that far right, the letters that much further apart -- still a gap between them)
    float dx = 0;                                        // (the tables slid aside: Leon's and Anna's taking turns)
    auto centred = [&](const char* s, float yy, float size, Color col, float bold) {
        Vector2 sm = MeasureTextEx(f, s, size, size / 10 + bold);
        for (float b : {0.0f, bold}) DrawTextEx(f, s, {roundf((sw - sm.x - bold) / 2 + b + dx), roundf(yy)}, size, size / 10 + bold, col);
    };
    // Each table as wide as its title or its lines, whichever is wider, a gap between them -- all
    // of them smaller, if need be, to fit across the screen. (The golden run's -- SOLO RUN --, the last, only
    // once one has been completed: a time of it in the tables on disk, or one since the game was launched.)
    float visibleRowSize = 0;
    auto tables = [&](const Scores& sc, bool golden, float slide, const char* heading) {
        dx = slide;
        const int nt = golden ? Scores::kParts : Scores::kParts - 1;
        float k = 1, colW[Scores::kParts], total = 0;
        for (int pass = 0; pass < 2; pass++) {
            float th = hs * k, tr = rs * k;
            total = 0;
            for (int c = 0; c < nt; c++) {
                float head = MeasureTextEx(f, TextFormat("= %s =", Scores::title(c)), th, th / 5).x + th / 10;   // (bold)
                float row = nameWidth(f, tr) + tr + MeasureTextEx(f, "00:00.0", tr, tr / 10).x;
                colW[c] = fmaxf(head, row);
                total += colW[c] + (c ? tr * 1.5f : 0);
            }
            if (pass == 0) k = fminf(1, sw * 0.96f / total);
        }
        float th = hs * k, tr = rs * k, tx = (sw - total) / 2 + dx;
        visibleRowSize += tr * (1 - fabsf(slide) / sw);   // during a scroll, weight each table by the fraction on screen
        float hb = 1.5f * hs, lift = hb - hs;                // (HIGHSCORE: half as big again, the rest under it lower)
        float tall = hs * 3.2f + lift + (Scores::kRows - 1) * rs * 1.35f + tr;   // HIGHSCORE's top to the last line's bottom
        float y = (floorY + above) / 2 - tall / 2;
        centred(heading, y, hb, rainbow(0, scoresA), hb / 10);  // (bold: a pixel of the font further)
        y += lift;
        for (int c = 0; c < nt; c++) {
            const char* head = TextFormat("= %s =", Scores::title(c));   // (bold, as HIGHSCORE)
            float hw = MeasureTextEx(f, head, th, th / 5).x + th / 10;
            for (float b : {0.0f, th / 10})
                DrawTextEx(f, head, {roundf(tx + (colW[c] - hw) / 2 + b), roundf(y + hs * 1.4f)}, th, th / 5, rainbow(1 + c, scoresA));
            float nameW = nameWidth(f, tr), timeW = MeasureTextEx(f, "00:00.0", tr, tr / 10).x;
            float x0 = tx + (colW[c] - (nameW + tr + timeW)) / 2, ry = y + hs * 3.2f;
            for (int i = 0; i < Scores::kRows; i++) {
                const Scores::Entry* e = i < (int)sc.part[c].size() ? &sc.part[c][i] : nullptr;
                char tb[32];
                if (e) scoreTime(e->t, tb, sizeof tb); else snprintf(tb, sizeof tb, "--:--.-");
                drawName(f, e ? e->name.c_str() : "---", {x0, ry}, tr, 0, rainbow(2 + i + c, scoresA));
                DrawTextEx(f, tb, {roundf(x0 + nameW + tr), roundf(ry)}, tr, tr / 10, rainbow(2 + i + c, scoresA));
                ry += rs * 1.35f;
            }
            tx += colW[c] + tr * 1.5f;
        }
    };
    if (who == 0) tables(sc, golden, 0, "HIGHSCORE");
    else {                                               // Leon's and Anna's in turn, the one playing's first
        const Scores* first = who == 1 ? annaSc : &sc, *second = who == 1 ? &sc : annaSc;
        const bool g1 = who == 1 ? annaGolden : golden, g2 = who == 1 ? golden : annaGolden;
        const char* h1 = who == 1 ? "ANNA'S HIGHSCORE" : "LEON'S HIGHSCORE", *h2 = who == 1 ? "LEON'S HIGHSCORE" : "ANNA'S HIGHSCORE";
        const float S = tn.titleScoresShow, T = tn.titleScoresScroll, ph = fmodf(clock, 2 * (S + T));
        auto ease = [](float u) { return u * u * (3 - 2 * u); };   // (off slowly, fast, slowly in)
        if (ph < S) tables(*first, g1, 0, h1);
        else if (ph < S + T) { float u = ease((ph - S) / T); tables(*first, g1, -u * sw, h1); tables(*second, g2, (1 - u) * sw, h2); }
        else if (ph < 2 * S + T) tables(*second, g2, 0, h2);
        else { float u = ease((ph - 2 * S - T) / T); tables(*second, g2, -u * sw, h2); tables(*first, g1, (1 - u) * sw, h1); }
    }
    const float buildSize = visibleRowSize * 0.5f, margin = 4 * ui;
    DrawTextEx(f, "Build " GRAVITY_LEON_BUILD_TIME, {roundf(margin), roundf(footerBottom - margin - buildSize)},
               buildSize, buildSize / 10, fade({128, 128, 128, 255}, a));

    // "Press A or <spacebar>": small, blinking softly, each letter riding its own point of a wave that
    // runs left to right -- a discrete wave, letter by letter; the pad's A in green, in a green circle (a
    // pad's button), the keyboard's <spacebar> in blue.
    // (The two finished: "Press Y/<Y> for Leon, X/<X> for Anna" -- the pad's Y in yellow, its X in
    // blue, each in its circle; the keyboard's keys in blue.)
    const char* press = who == 2 ? "Press Y/<Y> for Leon, X/<X> for Anna" : "Press A or <spacebar>";
    const int padAt = who == 2 ? 6 : (int)(strstr(press, " A ") + 1 - press), padAt2 = who == 2 ? 22 : -1;
    const char* keyWay = who == 2 ? press + 7 : strstr(press, "<spacebar>");
    const Color padCol = who == 2 ? Color{222, 160, 22, 255} : kFlagGreen, padCol2 = {52, 112, 214, 255};
    auto keyboard = [&](int i) { return who == 2 ? (i >= 8 && i <= 10) || (i >= 24 && i <= 26) : press + i >= keyWay; };
    float ps = tn.pressSize * sh / 720.0f;               // sized for a 720-line screen, scaled
    float blink = a * lerpf(tn.pressAlphaMin, tn.pressAlphaMax, 0.5f + 0.5f * cosf(2 * PI * clock / tn.pressBlinkPeriod));
    float ring = 0.3f * ps;                              // (room on each side of the A, for its circle)
    Vector2 pm = MeasureTextEx(f, press, ps, ps * 0.15f);
    pm.x += 2 * ring;
    if (padAt2 >= 0) pm.x += 2 * ring;
    // The wave's band -- its highest letter's top to its lowest one's bottom -- centred between the
    // title and the ground; but never lower than half a Leon above him. Then half a letter lower.
    float x = (sw - pm.x) / 2, y0 = (ink.y + ink.height + floorY) / 2 - pm.y / 2;
    y0 = fminf(y0, tc.leonTop - tc.leonHalf - pm.y - tn.pressWaveAmp * ps);
    y0 += pm.y / 2;
    int n = (int)strlen(press);
    for (int i = 0; i < n; i++) {
        char ch[2] = {press[i], 0};
        // (the wave as long as it was over "Press A" -- 0.7 radians a letter for seven -- spread over
        // the whole line: it rolls along it, not jumping from letter to letter)
        float dy = tn.pressWaveAmp * ps * sinf(2 * PI * clock / tn.pressWavePeriod - i * 4.9f / n);
        float w = MeasureTextEx(f, ch, ps, 0).x;
        const bool pad = i == padAt || i == padAt2;
        if (pad) {                                       // the A, in its circle
            x += ring;
            Vector2 c = {x + w / 2, y0 + dy + pm.y / 2};
            DrawRing(c, 0.56f * ps, 0.64f * ps, 0, 360, 36, fade(i == padAt ? padCol : padCol2, blink));
        }
        Color col = i == padAt ? padCol : i == padAt2 ? padCol2 : keyboard(i) ? kKeyBlue : kInk;
        DrawTextEx(f, ch, {x, y0 + dy}, ps, 0, fade(col, blink));
        x += w + ps * 0.15f + (pad ? ring : 0);
    }
}

// ---------------------------------------------------------------------------------------------
// A canvas to draw on, off the screen: raylib's LoadRenderTexture less its depth buffer -- 32 bits a
// pixel, as much again as the colours at the screen's size, for each canvas, and never used (nothing
// here is drawn with a depth test). Should the driver not take a framebuffer without one: raylib's.
static RenderTexture2D loadCanvas(int w, int h) {
    RenderTexture2D t{};
    t.id = rlLoadFramebuffer();
    if (t.id == 0) return LoadRenderTexture(w, h);
    rlEnableFramebuffer(t.id);
    t.texture.id = rlLoadTexture(nullptr, w, h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    t.texture.width = w; t.texture.height = h; t.texture.mipmaps = 1; t.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    rlFramebufferAttach(t.id, t.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    const bool ok = rlFramebufferComplete(t.id);
    rlDisableFramebuffer();
    if (!ok) { UnloadRenderTexture(t); return LoadRenderTexture(w, h); }
    TraceLog(LOG_INFO, "FBO: [ID %i] canvas %d x %d, colours only", t.id, w, h);
    return t;
}

int main(int argc, char** argv) {
    // --smoke-name NAME [--smoke-time S] [--spawn X,Y] [--smoke-double]: play a scripted run
    // (run right, hop -- and hop again in mid-air with --smoke-double)
    // for S seconds of game time, save screenshots/<date>_NAME.png and quit.
    const char* smokeName = nullptr;
    float smokeTime = 3.0f;
    bool smokeDouble = false, traceKnock = false;
    int smokeJumps = -1;                     // --smoke-jumps N: stop hopping after N jumps
    int smokeLook = 0;                       // --smoke-look up|down: hold it from 0.3 s
    int smokeLookSide = 0;                   // --smoke-look-side -1|1: the right stick held aside
    int smokeRun = 1;                        // --smoke-run -1|0|1: which way the script runs
    float smokeStop = -1;                    // --smoke-stop T: stop running at T
    bool traceLean = false;                  // --trace-lean: print the lean every step
    std::vector<Vector3> smokeTongue;        // --smoke-tongue T,DX,DY: press B at T aiming (DX,DY)
    std::vector<float> smokeGod;             // --smoke-god T: Shift+G at T (GOD on / off)
    std::vector<float> smokeChew;            // --smoke-chew T: pull RT at T
    std::vector<Vector2> smokePay;           // --smoke-pay T0,T1: hold LT from T0 to T1
    std::vector<float> smokeJumpAt;          // --smoke-jump-at T: press jump at T (a tap)
    std::vector<float> smokeDash;            // --smoke-dash T: Anna's dash at T (the way she faces)
    std::vector<Vector2> smokeJumpHold;      // --smoke-jump-hold T0,T1: press jump at T0, hold it to T1
    float smokeSuicide = -1;                 // --smoke-suicide T: hold the suicide combo from T
    float smokeSwitch = -1;                  // --smoke-switch T: the title's debug switch (Shift+Enter) at T
    std::vector<float> smokeFullscreen;      // --smoke-fullscreen T: Shift+F at T (tests)
    std::vector<Vector3> smokeResize;        // --smoke-resize T,W,H: the window made W x H at T (tests)
    float smokeLevelAt = -1;                 // --smoke-level-at T,FILE: another map at T, loaded as params' `level` switches it live (tests)
    std::string smokeLevelFile;
    std::string scoresFile = "data/highscores.txt";   // --scores-file FILE: the highscores elsewhere (tests)
    int startStage = 0;                      // --stage N: start as if Leon's game (1), or both (2), had been finished this session (tests)
    bool noHud = false;                      // --no-hud: no counters, clocks or debug readout on the screen (pictures: tools/make_icon.py)
    int forcePlayer = -1;                    // --leon / --anna: who plays, whatever has been finished (tests)
    bool scoresFileSet = false;
    std::vector<std::pair<int, float>> testScores;     // --test-score PART,SECONDS: a part done, at the start (tests)
    std::string testName;                               // --test-name NAME: typed for the first highscore, then Enter (tests)
    float smokeCountdown = -1;               // --smoke-countdown T: Shift+Ctrl+R at T
    float smokeStart = -1;                   // --smoke-start T: A on the title (--show-title) at T
    float smokeEsc = -1;                     // --smoke-esc T: ESC at T
    int startLevelArg = -1;                  // --start-level NAME: params' start_level, for this run (tests)
    int flyAtX = -1, flyAtY = -1;            // --fly-at X,Y: put the first fly on that tile
    std::string levelFile;                   // --level FILE: play another map (tests); else params' `level`
    std::string fromReport;                  // --from-report FILE: back to the state a bug report recorded
    std::string reportsDir = "Reports";      // --reports-dir DIR: where Shift+B writes (tests: elsewhere)
    std::vector<std::string> testReports;    // --test-report TEXT: write a report at the end of a test run
    std::string showReportBox;               // --show-report-box TEXT: show the box with that text (tests)
    bool showReportBoxSet = false;
    int startGrav = -1;                      // --grav F: start in that gravity frame (tests)
    bool allAbilities = false;               // --abilities: start with the double jump and the tongue (tests)
    float spawnPx = -1, spawnPy = -1;        // --spawn-px X,Y: start with his feet there (world pixels)
    int spawnX = -1, spawnY = -1;
    unsigned seed = 0;                       // --seed N: the random numbers drawn from seed N (tests: two runs alike, pixel for pixel)
    bool seedSet = false;
    std::string frameLog;                    // --frame-log FILE: each frame's start and work time, written there at the end (benchmarks)
    bool noVsync = false;                    // --no-vsync: frames as fast as they come (benchmarks)
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--smoke-name") && i + 1 < argc) smokeName = argv[++i];
        else if (!strcmp(argv[i], "--smoke-time") && i + 1 < argc) smokeTime = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--smoke-double")) smokeDouble = true;
        else if (!strcmp(argv[i], "--trace")) traceKnock = true;
        else if (!strcmp(argv[i], "--smoke-jumps") && i + 1 < argc) smokeJumps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--smoke-run") && i + 1 < argc) smokeRun = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--smoke-stop") && i + 1 < argc) smokeStop = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--trace-lean")) traceLean = true;
        else if (!strcmp(argv[i], "--smoke-god") && i + 1 < argc) smokeGod.push_back((float)atof(argv[++i]));
        else if (!strcmp(argv[i], "--smoke-tongue") && i + 1 < argc) {
            Vector3 ev{};
            if (sscanf(argv[++i], "%f,%f,%f", &ev.x, &ev.y, &ev.z) == 3) smokeTongue.push_back(ev);
        }
        else if (!strcmp(argv[i], "--smoke-chew") && i + 1 < argc) smokeChew.push_back((float)atof(argv[++i]));
        else if (!strcmp(argv[i], "--smoke-suicide") && i + 1 < argc) smokeSuicide = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--smoke-switch") && i + 1 < argc) smokeSwitch = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--scores-file") && i + 1 < argc) { scoresFile = argv[++i]; scoresFileSet = true; }
        else if (!strcmp(argv[i], "--stage") && i + 1 < argc) startStage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-hud")) noHud = true;
        else if (!strcmp(argv[i], "--leon")) forcePlayer = 0;
        else if (!strcmp(argv[i], "--anna")) forcePlayer = 1;
        else if (!strcmp(argv[i], "--test-name") && i + 1 < argc) testName = argv[++i];
        else if (!strcmp(argv[i], "--test-score") && i + 1 < argc) {
            int k = 0; float s = 0;
            if (sscanf(argv[++i], "%d,%f", &k, &s) == 2) testScores.push_back({k, s});
        }
        else if (!strcmp(argv[i], "--smoke-countdown") && i + 1 < argc) smokeCountdown = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--smoke-start") && i + 1 < argc) smokeStart = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--smoke-esc") && i + 1 < argc) smokeEsc = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--start-level") && i + 1 < argc) { ++i; startLevelArg = !strcmp(argv[i], "5") ? 5 : modeNamed(argv[i]); }   // (5: the golden key, as cheat key P)
        else if (!strcmp(argv[i], "--smoke-pay") && i + 1 < argc) {
            Vector2 h{};
            if (sscanf(argv[++i], "%f,%f", &h.x, &h.y) == 2) smokePay.push_back(h);
        }
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) levelFile = argv[++i];
        else if (!strcmp(argv[i], "--from-report") && i + 1 < argc) fromReport = argv[++i];
        else if (!strcmp(argv[i], "--reports-dir") && i + 1 < argc) reportsDir = argv[++i];
        else if (!strcmp(argv[i], "--test-report") && i + 1 < argc) testReports.push_back(argv[++i]);
        else if (!strcmp(argv[i], "--show-report-box") && i + 1 < argc) { showReportBox = argv[++i]; showReportBoxSet = true; }
        else if (!strcmp(argv[i], "--grav") && i + 1 < argc) startGrav = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--abilities")) allAbilities = true;
        else if (!strcmp(argv[i], "--spawn-px") && i + 1 < argc) sscanf(argv[++i], "%f,%f", &spawnPx, &spawnPy);
        else if (!strcmp(argv[i], "--fly-at") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &flyAtX, &flyAtY);
        else if (!strcmp(argv[i], "--smoke-jump-at") && i + 1 < argc) smokeJumpAt.push_back((float)atof(argv[++i]));
        else if (!strcmp(argv[i], "--smoke-dash") && i + 1 < argc) smokeDash.push_back((float)atof(argv[++i]));
        else if (!strcmp(argv[i], "--smoke-jump-hold") && i + 1 < argc) {
            Vector2 h;
            if (sscanf(argv[++i], "%f,%f", &h.x, &h.y) == 2) smokeJumpHold.push_back(h);
        }
        else if (!strcmp(argv[i], "--smoke-look-side") && i + 1 < argc) smokeLookSide = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--smoke-look") && i + 1 < argc) smokeLook = strcmp(argv[++i], "up") ? 1 : -1;
        else if (!strcmp(argv[i], "--spawn") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &spawnX, &spawnY);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) { seed = (unsigned)strtoul(argv[++i], nullptr, 10); seedSet = true; }
        else if (!strcmp(argv[i], "--frame-log") && i + 1 < argc) frameLog = argv[++i];
        else if (!strcmp(argv[i], "--no-vsync")) noVsync = true;
        else if (!strcmp(argv[i], "--bg-fallback")) gBgStrokesOnly = true;
        else if (!strcmp(argv[i], "--bg-probe")) gBgProbe = true;
        else if (!strcmp(argv[i], "--smoke-fullscreen") && i + 1 < argc) smokeFullscreen.push_back((float)atof(argv[++i]));
        else if (!strcmp(argv[i], "--smoke-resize") && i + 1 < argc) {
            Vector3 r{};
            if (sscanf(argv[++i], "%f,%f,%f", &r.x, &r.y, &r.z) == 3) smokeResize.push_back(r);
        }
        else if (!strcmp(argv[i], "--smoke-level-at") && i + 1 < argc) {
            char file[512] = {};
            if (sscanf(argv[++i], "%f,%511[^\n]", &smokeLevelAt, file) == 2) smokeLevelFile = file;
        }
    }

    for (int i = 1; i + 1 < argc; i++)          // --audio-check DIR: test the sounds, silently
        if (!strcmp(argv[i], "--audio-check")) {
            ChangeDirectory(GetApplicationDirectory());
            Audio::check({kMusic[0], kMusic[1], kMusic[2]}, argv[i + 1]);
            return 0;
        }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | (noVsync ? 0 : FLAG_VSYNC_HINT) | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Gravity Leon");
#ifdef __EMSCRIPTEN__
    if (!IsWindowReady()) {
        Web::graphicsFailed();
        return 1;
    }
#endif
    if (seedSet) SetRandomSeed(seed);        // (after InitWindow, which seeds from the clock; before the first draw)
#ifdef __EMSCRIPTEN__
    SetTargetFPS(0);                         // requestAnimationFrame drives presentation
    if (Web::macKeyboard()) {
        kJumpKey1 = KEY_LEFT_SUPER; kJumpKey2 = KEY_RIGHT_SUPER; kJumpKeyName = "Cmd";
    }
#else
    ChangeDirectory(GetApplicationDirectory());
    // Run as a macOS app (make dist_mac: GravityLeon.app/Contents/MacOS): what it writes -- the highscores,
    // screenshots, bug reports -- in the user's own folder, ~/Library/Application Support/GravityLeon,
    // not inside the bundle (which may be read-only). Its files are baked in: read wherever it runs.
    if (strstr(GetApplicationDirectory(), ".app/Contents/MacOS") && getenv("HOME")) {
        std::string support = std::string(getenv("HOME")) + "/Library/Application Support/GravityLeon";
        if (!DirectoryExists(support.c_str())) MakeDirectory(support.c_str());
        if (DirectoryExists(support.c_str())) ChangeDirectory(support.c_str());
    }
    SetTargetFPS(smokeName ? 0 : 144);
#endif
    SetExitKey(KEY_NULL);                    // (ESC asks first: the question, in the main loop)
    rlDisableBackfaceCulling();

    Game g;
    g.trace = traceKnock;
    g.traceLean = traceLean;
    g.params.load("data/params.txt");
    std::map<std::string, std::string> reported;  // --from-report: the state it recorded
    if (!fromReport.empty()) {
        std::string reportedParams;
        if (!readReport(fromReport, reported, reportedParams)) {
            TraceLog(LOG_ERROR, "REPORT: cannot read %s", fromReport.c_str());
            CloseWindow();
            return 1;
        }
        g.params.loadText(reportedParams, fromReport.c_str());   // the params as they were
        if (levelFile.empty() && reported.count("level.file")) levelFile = reported["level.file"];
    }
    g.t.read(g.params);
    if (startLevelArg >= 0) g.t.startPart = startLevelArg;
    Input::init("assets/gamecontrollerdb.txt", g.t.deadzone);
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--pad-check")) {         // the pads as the system sees them -- then, live, as the game does
            float secs = (i + 1 < argc && argv[i + 1][0] != '-') ? (float)atof(argv[i + 1]) : 30.0f;
            padCheckSystem();
            padRawStart();
            static const char* kBtn[] = {"-", "D-pad up", "D-pad right", "D-pad down", "D-pad left", "Y", "B", "A", "X", "LB", "LT",
                                         "RB", "RT", "View", "Xbox", "Menu", "left stick in", "right stick in"};
            for (int j = 0; j < 8; j++)
                if (IsGamepadAvailable(j)) printf("PADCHECK game: pad %d \"%s\", %d axes -- press buttons, move sticks and triggers\n", j,
                                                  GetGamepadName(j), GetGamepadAxisCount(j));
            float last[8][8] = {};
            int moves = 0;
            for (float t0 = 0; t0 < secs && !WindowShouldClose(); t0 += GetFrameTime()) {
                BeginDrawing();
                ClearBackground(kPaper);
                DrawText(TextFormat("pad check: press buttons, move sticks (%.0f s) -- the terminal says what comes in", secs - t0), 20, 20, 20, kInk);
                EndDrawing();
                Input::poll();
                if (t0 == 0) {                            // (the first reading: where everything rests)
                    for (int j = 0; j < 8; j++) for (int k = 0; k < 8; k++) last[j][k] = IsGamepadAvailable(j) ? GetGamepadAxisMovement(j, k) : 0;
                    continue;
                }
                // The cable Xbox pad, as the game now reads it (its packets decoded by hand).
                for (int b = 1; b <= 17; b++)
                    if (Input::cablePad() && Input::padPressed(b) && !IsGamepadButtonPressed(0, b)) {
                        printf("PADCHECK game: cable Xbox pad button %s\n", kBtn[b]); moves++;
                    }
                if (Input::cablePad()) {
                    static float lastStick[4] = {0, 0, 0, 0};
                    float now[4] = {Input::stickX(), Input::stickY(), Input::rightX(), Input::rightY()};
                    static const char* names[4] = {"left stick x", "left stick y", "right stick x", "right stick y"};
                    for (int k = 0; k < 4; k++)
                        if (fabsf(now[k] - lastStick[k]) > 0.25f) { printf("PADCHECK game: cable Xbox pad %s = %+.2f\n", names[k], now[k]); lastStick[k] = now[k]; moves++; }
                    static bool lastLT = false, lastRT = false;
                    if (Input::leftTrigger() != lastLT) { lastLT = !lastLT; printf("PADCHECK game: cable Xbox pad LT %s\n", lastLT ? "pulled" : "let go"); moves++; }
                    if (Input::rightTrigger() != lastRT) { lastRT = !lastRT; printf("PADCHECK game: cable Xbox pad RT %s\n", lastRT ? "pulled" : "let go"); moves++; }
                }
                for (int j = 0; j < 8; j++) {
                    if (!IsGamepadAvailable(j)) continue;
                    for (int b = 1; b <= 17; b++)
                        if (IsGamepadButtonPressed(j, b)) { printf("PADCHECK game: pad %d button %s\n", j, kBtn[b]); moves++; }
                    for (int k = 0; k < GetGamepadAxisCount(j) && k < 8; k++) {
                        float v = GetGamepadAxisMovement(j, k);
                        if (fabsf(v - last[j][k]) > 0.25f) { printf("PADCHECK game: pad %d axis %d = %+.2f\n", j, k, v); last[j][k] = v; moves++; }
                    }
                }
                fflush(stdout);
            }
            int raw = padRawStop();
            printf("PADCHECK game: %d change(s) seen%s\n", moves,
                   moves ? "" : " -- none: no pad, nothing pressed, or a pad with no line in assets/gamecontrollerdb.txt (frozen)");
            printf("PADCHECK raw: %d change(s) straight from the pads%s\n", raw,
                   raw ? (moves ? "" : " -- the pad does send: the game's mapping does not match it")
                       : " -- nothing: the system hands the game nothing from this pad (whatever the mapping)");
            CloseWindow();
            return 0;
        }
    auto applyAudio = [&]() {
        Audio::setMusicOn(g.t.musicOn); Audio::setMusicVolume(g.t.musicVolume);
        Audio::setSoundOn(g.t.soundOn); Audio::setSoundVolume(g.t.soundVolume);
    };
    applyAudio();
    bool audioMuted = false;                 // --audio-muted: tests -- audio running, at zero volume
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--audio-muted")) audioMuted = true;
    if (!smokeName || audioMuted) Audio::init(kMusic[0]);   // test runs stay silent
    loadCredits("assets/endcredits.txt");                // the ending's credits (their height, each frame: main loop)
    int musicPlaying = 0;                    // which of kMusic is (or is becoming) the music
    if (audioMuted) { Audio::setMusicVolume(0); Audio::setSoundVolume(0); }
    // Which level: params.txt's `level` (a name in levels/), unless --level gave a file.
    std::string levelName = g.params.s("level", "testing ground");
    auto levelPath = [&]() { return levelFile.empty() ? "levels/" + levelName + ".txt" : levelFile; };
    if (!g.level.load(levelPath().c_str())) { CloseWindow(); return 1; }
    if (spawnX >= 0) g.level.spawn = {(spawnX + 0.5f) * kTile, (spawnY + 1) * kTile};
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--show-title")) g.onTitle = true;
    if (!smokeName) g.onTitle = true;               // (the title up: the level as at its start, whatever start_level)
    fitGame(g.t.screenRatio);                       // (the game's area, before anything is laid out in it)
    gBgFrom = gBgTo = g.t.bgPattern[0];             // (the far layer: easy mode's, no fade to begin with)
    // Who plays: Leon's game till it has been finished once this session, then Anna's; both finished,
    // the title's choice (Y Leon, X Anna).
    Progress progress;
    progress.leon = startStage >= 1;
    progress.anna = startStage >= 2;
    g.anna = forcePlayer >= 0 ? forcePlayer == 1 : progress.stage() == 1;
    // The end's words: Anna's game finished the first time, 100%; past that, again.
    auto endWords = [&] { return progress.stage() == 2 ? 2 : g.anna && progress.stage() == 1 ? 1 : 0; };
    g.endMsg = endWords();
    g.reset();
    if (spawnPx >= 0) { g.pos = g.prevPos = {spawnPx, spawnPy}; g.cam = g.prevCam = {spawnPx, spawnPy - kLeonH / 2}; g.fallTop = spawnPy; }
    if (startGrav > 0) g.forceGravity(startGrav);
    if (allAbilities) { g.canDouble = g.canTongue = true; }
    if (flyAtX >= 0 && !g.flies.empty()) g.flies[0].p = {(flyAtX + 0.5f) * kTile, (flyAtY + 0.5f) * kTile};
    if (smokeName) g.hintTimer = 0;
    g.engrave = g.t.engrave;
    if (!reported.empty()) {
        g.applyReport(reported);
        if (reported.count("engraving")) g.engrave = atoi(reported["engraving"].c_str()) != 0;
        TraceLog(LOG_INFO, "REPORT: state restored from %s", fromReport.c_str());
    }
    for (int i = 1; i < argc; i++) {                                                          // tests
        if (!strcmp(argv[i], "--decor-off") || !strcmp(argv[i], "--sketch")) g.engrave = false;
        if (!strcmp(argv[i], "--engraving")) g.engrave = true;
    }
    // Fullscreen (borderless) from the start (not for test runs), the mouse pointer hidden while it is.
    // The windowed size and place are kept here, not left to raylib: macOS resizes the window a
    // moment after it is asked, and a toggle meanwhile had raylib take the size in between for the
    // windowed one -- the window stayed screen-sized while raylib thought it windowed, and the
    // picture no longer matched it (the top half blurred). So also: one toggle at a time
    // (fullscreen_toggle_wait).
    float fsSince = 1e9f;                      // seconds since the last toggle
#ifndef __EMSCRIPTEN__
    Vector2 winPos = GetWindowPosition();
    int winW = GetScreenWidth(), winH = GetScreenHeight();
    if (!smokeName && g.params.b("fullscreen", true)) { ToggleBorderlessWindowed(); HideCursor(); fsSince = 0; }
#endif
    bool titleWait = !smokeName;               // the title card, holding the game until A...
    float startFade = -1;                      // ...then the fade out of it and in to the game, the game still held (-1: none)
    // ESC: a question first, over a screen fading to black (esc_fade_time) -- on the title "Exit
    // Game?" (yes: the game quits), in the game "Back to Title?" (yes: back to the title, the level
    // as at the start). NO at first; left / right (arrows, stick, D-pad) pick, a jump (or Enter)
    // answers, ESC again is NO. (Not while the bug-report box or a highscore's name has ESC.)
    int ask = 0;                               // the question: 0 none, 1 exit the game, 2 back to the title
    bool askOpen = false, askYes = false;      // up (else fading out, still holding the game), the answer picked
    float askA = 0;                            // its fade, 0..1
    bool quit = false;
    bool startLatch = false;                   // the press that started the game is the title's: Leon never hears it
    float titleClock = 0;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--show-title")) { titleWait = true; g.hintTimer = 9.0f; }   // tests: capture it, help and all

    float acc = 0, reloadCheck = 0, smokeClock = 0;
    // The engraving: the world drawn whole into engraveRT, then through the engraving shader.
    // (Should the shader not compile, it is drawn as it is, in its half-tones.)
    RenderTexture2D engraveRT{};
    RenderTexture2D darkRT{};                             // the dark air's black, with the lamps' light let through
    RenderTexture2D eyesRT{};                             // his eyes over it, alone -- fading with him into a dark light
    RenderTexture2D leonRT{};                             // Leon alone, to fade into a cloud of his colour...
    RenderTexture2D blurRT{};                             // (Anna dashing: her copies along her way, summed)
    RenderTexture2D tintRT{};                             // the engraving: what keeps its colours under the lines (hand-coloured)
    RenderTexture2D leonCoverRT{}, hatCoverRT{};          // the engraving: where the clouds cover him (his hat), melted -- kept as drawn
    RenderTexture2D hatRT{};                              // ...and his hat, the same...
    RenderTexture2D maskRT{};                             // ...melted into the clouds that count, drawn here
    RenderTexture2D gasRT{}, gasScratch{};                // the colour clouds, premultiplied, different colours mixed where they cross (one colour's at a time)
    RenderTexture2D otherMaskRT{};                        // ...and, of those, the ones NOT his colour: Leon seen through them
    RenderTexture2D groundRT{};                           // the level alone (in front of the clouds): its line under his feet showing through them, melted
    Vector2 cloudScroll[3] = {};             // its three layers of clouds: how far each has slid (screen pixels)
    Vector2 cloudCamW{};
    bool cloudInit = false;
    Shader engraveSh = LoadShaderFromMemory(nullptr, kEngraveFS);
    const bool engraveOk = IsShaderValid(engraveSh) && engraveSh.id != rlGetShaderIdDefault();
    if (!engraveOk) TraceLog(LOG_WARNING, "ENGRAVING: the shader did not compile -- drawn without it");
    const char* engraveNames[8] = {"screen", "pixelScale", "camTarget", "camOffset", "camRot", "camZoom", "spacing", "tintTex"};
    int engraveLoc[8];
    for (int k = 0; k < 8; k++) engraveLoc[k] = GetShaderLocation(engraveSh, engraveNames[k]);
    // ...and its sky, cut by a shader of its own (drawn on a stretched white texel); without it,
    // the old drawn clouds.
    Shader skySh = LoadShaderFromMemory(nullptr, kSkyFS);
    bool skyOk = IsShaderValid(skySh) && skySh.id != rlGetShaderIdDefault();
#ifdef __EMSCRIPTEN__
    skyOk = skyOk && Web::initSky(skySh.id, sizeof(SkyLayout::cl), sizeof(SkyLayout::sh));
#endif
    if (!skyOk) TraceLog(LOG_WARNING, "ENGRAVING: the sky's shader did not compile -- drawn clouds instead");
    const char* skyNames[7] = {"screen", "spacing", "light", "count", "clouds", "shapes", "frames"};
    int skyLoc[7];
    for (int k = 0; k < 7; k++) {
#ifdef __EMSCRIPTEN__
        if (k == 4 || k == 5) continue;       // clouds and shapes live in the uniform block
#endif
        skyLoc[k] = GetShaderLocation(skySh, skyNames[k]);
    }
    const Texture2D whiteTex = {rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    float tongueWait = 0;                    // B pressed, waiting a moment for a direction
    bool rtWas = false;                      // RT last frame: a chew per pull, not per frame
    bool shiftWasDown = false;
    float restartIn = -1;                    // Shift+Ctrl+R: the restart at once, then 3, 2, 1 (the game held), GO -- seconds left of the 4 (-1: none)
    bool goldenSeen[2] = {};                 // a golden run completed since the game was launched (Leon's, Anna's): its table on the title from then on
    float switchT = -1;                      // the title's debug switch between Leon's game and Anna's: seconds into its fade (-1: none)
    bool endCounted = false;                 // this game's end counted as finished (Progress)
    int codeAt = 0;                          // ...the code's letters typed so far
    float cheatsWait = 0;                    // ...just typed: seconds before a cheat key counts (its last letter is one)
    float codeT = 0;                         // ...seconds since its first one
    float titleFadeIn = 0;                   // back on the title when the time ran out: seconds left of its fade in
    float titleIntro = 0;                    // seconds since the title came up (title_fade_in_time: from black)
    // Highscores: loaded now, and a part done in a good time asks for three letters, typed in while
    // the game goes on: the letter keys write the name meanwhile (and play no more), Backspace takes
    // one back, Enter keeps it. A name given is offered again for the game's next highscore -- kept as
    // it is, or a letter typed starts another. No name (Esc too): '???'. The time is kept whatever
    // happens: Enter, the next highscore, a restart, quitting. (Not from test runs -- unless pointed
    // elsewhere with --scores-file -- nor with any start_with_* help or debug key: not a fair time.)
    // Leon's, and Anna's beside them (data/highscores_anna.txt): the one playing's are the game's.
    Scores scores, annaScores;
    if (!scoresFileSet && !DirectoryExists("data")) MakeDirectory("data");
    scores.load(scoresFile);
    {
        size_t dot = scoresFile.find_last_of('.'), slash = scoresFile.find_last_of('/');
        bool ext = dot != std::string::npos && (slash == std::string::npos || dot > slash);
        annaScores.load(ext ? scoresFile.substr(0, dot) + "_anna" + scoresFile.substr(dot) : scoresFile + "_anna");
    }
    auto myScores = [&]() -> Scores& { return g.anna ? annaScores : scores; };
    std::deque<std::pair<int, float>> scorePending(testScores.begin(), testScores.end());
    NameEntry entry;
    std::string gameName;                                // this game's name: the letters of its last highscore
    // This game's highscores: by their flag again, once Leon has been away, their names can be typed again.
    struct MyScore { int part; float time; bool away; };
    std::vector<MyScore> mine;
    auto closeEntry = [&]() {                            // (letters missing: '?')
        if (!entry.open) return;
        std::string nm;
        for (int i = 0; i < 3; i++) nm += i < entry.len ? entry.name[i] : '?';
        myScores().insert(entry.part, nm, entry.time);
        bool known = false;
        for (MyScore& ms : mine) if (ms.part == entry.part && ms.time == entry.time) { known = true; ms.away = false; }
        if (!known) mine.push_back({entry.part, entry.time, false});
        if (entry.len > 0) gameName.assign(entry.name, entry.len);
        if (g.trace) fprintf(stderr, "highscore %s: %s %.1f s%s\n", Scores::tag(entry.part), nm.c_str(), entry.time, entry.copied ? " (the game's name)" : "");
        entry.open = false;
    };
    // Shift+B: a bug report. The picture and the state are taken on the frame it is pressed;
    // then the game holds while the text is typed. Enter saves both into Reports/, Esc drops them.
    bool reportWanted = false, reportOpen = false;
    std::string reportText, reportState, reportStamp, reportWhen;
    size_t reportCursor = 0;                 // where typing goes (a byte in reportText)
    float reportWantX = -1;                  // Up / Down keep to the column they started from
    int reportScroll = 0;                    // the first line in view
    Image reportShot{};
    float reportClock = 0, reportToast = 0;
    std::string reportToastText;
    auto saveReport = [&](const std::string& userText) -> std::string {
        if (!DirectoryExists(reportsDir.c_str())) MakeDirectory(reportsDir.c_str());
        std::string base = reportBase(reportsDir, reportStamp);
        std::string png = base + ".png", txt = base + ".txt";
        std::string pngName = png.substr(png.find_last_of('/') + 1);
        std::string body = userText;
        while (!body.empty() && (body.back() == ' ' || body.back() == '\n')) body.pop_back();
        if (body.empty()) body = "(no text)";
        body += "\n\n=== GRAVITY LEON REPORT DATA (written by the game, to find and reproduce this moment) ===\n";
        body += "written: " + reportWhen + "\n";
        body += "snapshot: " + pngName + "\n";
        body += "reproduce: ./gravityleon --from-report \"" + txt + "\" --smoke-name repro --smoke-run 0 --smoke-jumps 0 --smoke-time 0.1\n";
        body += "\n[state]\n" + reportState;
        body += "\n[params.txt]   (the params in effect at that moment)\n" + g.params.raw();
        if (!body.empty() && body.back() != '\n') body += "\n";
        body += "[end params.txt]\n";
        bool ok = ExportImage(reportShot, png.c_str()) && writeText(txt, body);
#ifdef __EMSCRIPTEN__
        if (ok) { Web::download(png.c_str()); Web::download(txt.c_str()); }
#endif
        TraceLog(ok ? LOG_INFO : LOG_WARNING, "REPORT: %s %s", ok ? "saved" : "could not save", txt.c_str());
        return ok ? txt : std::string();
    };
    // Takes the picture (the frame as drawn so far) and the state: called before the swap.
    auto captureReport = [&]() {
        rlDrawRenderBatchActive();
        if (reportShot.data) UnloadImage(reportShot);
        reportShot = gameShot();
        time_t now = ::time(nullptr);
        struct tm tmv;
        localtime_r(&now, &tmv);
        char b1[32], b2[32];
        strftime(b1, sizeof(b1), "%Y%m%d_%Hh%Mm%Ss", &tmv);
        strftime(b2, sizeof(b2), "%Y-%m-%d %H:%M:%S", &tmv);
        reportStamp = b1; reportWhen = b2;
        reportState = g.reportState(levelPath());
        appendf(reportState, "engraving: %d\n", g.engrave ? 1 : 0);
        appendf(reportState, "screen: %d %d   (pixels; fullscreen %d)\n", GetScreenWidth(), GetScreenHeight(),
#ifdef __EMSCRIPTEN__
                Web::fullscreen() ? 1 : 0);
#else
                IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? 1 : 0);
#endif
        appendf(reportState, "game.area: %d %d %d %d   (the game's part of it: x y width height -- black bands round it)\n", gGameX, gGameY, gGameW, gGameH);
#ifdef __EMSCRIPTEN__
        appendf(reportState, "canvas: %d %d framebuffer pixels\n", GetRenderWidth(), GetRenderHeight());
#else
        {   // (the window as the system has it: if the picture and the window part ways, it shows here)
            Vector2 wp = GetWindowPosition(), dpi = GetWindowScaleDPI();
            int mon = GetCurrentMonitor();
            appendf(reportState, "window: at %.0f %.0f, render %d %d, dpi scale %.2f %.2f, monitor %d: %d %d\n", wp.x, wp.y,
                    GetRenderWidth(), GetRenderHeight(), dpi.x, dpi.y, mon, GetMonitorWidth(mon), GetMonitorHeight(mon));
        }
#endif
    };
    if (showReportBoxSet) {                   // tests: "[|]" in the text marks where the cursor is
        reportOpen = true;
        reportText = showReportBox;
        size_t m = reportText.find("[|]");
        if (m != std::string::npos) reportText.erase(m, 3);
        reportCursor = m != std::string::npos ? m : reportText.size();
    }
    std::vector<float> frameMs;              // --frame-log: each frame's start (ms from the first) and its work before the swap (ms)
    const double logT0 = GetTime();
#ifdef __EMSCRIPTEN__
    while (true) {
        float frame = Web::nextFrame();
#else
    while (!WindowShouldClose()) {
        float frame = smokeName ? 1.0f / 60.0f : fminf(GetFrameTime(), 0.1f);
#endif
        const double frameT0 = frameLog.empty() ? 0 : GetTime();
        fitGame(g.t.screenRatio);                         // (the window may have changed: the game's area in it, its shape kept)
        {   // the far layer's pattern: the mode he is in -- the latest started (the title: easy) -- a change crossfading;
            // a golden key taken (and a golden run), the golden one (background_pattern_golden)
            int mode = g.onTitle ? 0 : g.camMode ? 4 : g.harderMode ? 3 : g.darkMode ? 2 : g.hardMode ? 1 : 0;
            int pat = !g.onTitle && g.goldTaken() ? g.t.bgGolden : g.t.bgPattern[mode];
            if (pat != gBgTo) { gBgFrom = gBgTo; gBgTo = pat; gBgFade = 0; }
            gBgFade = g.t.bgFade > 0 ? fminf(1, gBgFade + frame / g.t.bgFade) : 1;
        }
        Input::poll();                                    // (the cable Xbox pad, which GLFW cannot read)

        if (smokeName && smokeLevelAt >= 0 && smokeClock >= smokeLevelAt && smokeClock - frame < smokeLevelAt) {   // (tests: as the live switch below)
            levelFile = smokeLevelFile;
            if (g.level.load(levelPath().c_str())) g.reset();
        }
        reloadCheck += frame;
        if (reloadCheck > 0.5f) {
            reloadCheck = 0;
            if (g.params.reloadIfChanged()) {
                bool engraveWas = g.t.engrave;
                g.t.read(g.params); Input::setDeadzone(g.t.deadzone); applyAudio();
                if (startLevelArg >= 0) g.t.startPart = startLevelArg;
                if (g.t.engrave != engraveWas) {          // params switched the look
                    g.engrave = g.t.engrave;
                }
                std::string want = g.params.s("level", "testing ground");
                if (want != levelName && levelFile.empty()) {          // another level chosen: go there
                    levelName = want;
                    if (g.level.load(levelPath().c_str())) g.reset();
                }
            }
        }

        // ---- the bug-report box: typing ----
        // (decided before the typing: the frame that saves or cancels still gives the game nothing --
        //  the Enter or Esc belongs to the box)
        const bool boxOpen = reportOpen || ask != 0;      // (they hold the game, and its keys)
        const bool escHit = IsKeyPressed(KEY_ESCAPE) || Input::padPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT);   // ESC, or the pad's Menu: the same
        const bool typing = entry.open && !reportOpen;    // a highscore's name: the letter keys write it, the game goes on
        if (typing) {                                     // ---- a new highscore: its three letters ----
            entry.clock += frame;
            auto editName = [&](int key) {
                if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) {
                    if (entry.copied) entry.len = 0;      // (the game's name offered: a letter typed starts another)
                    entry.copied = false;
                    if (entry.len < 3) entry.name[entry.len++] = (char)toupper(key);
                    entry.clock = 0;
                } else if (key == '\b' && entry.len > 0) {
                    entry.len--; entry.copied = false; entry.clock = 0;
                } else if (key == '\r' || key == 27) {
                    if (key == 27) entry.len = 0;         // (no name: '???')
                    closeEntry();
                }
            };
#ifdef __EMSCRIPTEN__
            // DOM keydown carries both the layout's letter and its editing keys, in order.
            while (int key = Web::nextScoreKey()) if (entry.open) editName(key);
            if (entry.open && escHit) editName(27);       // also the gamepad's Menu button
#else
            while (int cp = GetCharPressed()) if (cp >= 32) editName(cp);
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) editName('\b');
            if (escHit) editName(27);
            else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) editName('\r');
#endif
        }
        if (reportOpen && !showReportBoxSet) {
            reportClock += frame;
            // Everything happens at the cursor. Moving or typing shows it at once (the blink restarts).
            size_t was = reportCursor, wasSize = reportText.size();
            auto insert = [&](std::string s) {
                if (reportText.size() + s.size() > 8000) return;
                reportText.insert(reportCursor, s);
                reportCursor += s.size();
            };
            int cp;
            while ((cp = GetCharPressed()) > 0)
                if (cp >= 32 && cp != 127) {
                    int n = 0;
                    const char* u = CodepointToUTF8(cp, &n);
                    insert(std::string(u, n));
                }
            bool shiftK = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            bool cmdK = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) || IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            bool altK = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
            auto pressed = [](int k) { return IsKeyPressed(k) || IsKeyPressedRepeat(k); };
#ifdef __EMSCRIPTEN__
            const std::string pasted = Web::takePaste();
            const char* clip = pasted.empty() ? nullptr : pasted.c_str();
#else
            const char* clip = cmdK && IsKeyPressed(KEY_V) ? GetClipboardText() : nullptr;
#endif
            if (clip) {
                std::string s;
                for (const char* q = clip; *q; q++)
                    if (*q != '\r' && ((unsigned char)*q >= 32 || *q == '\n')) s += *q;
                if (reportText.size() + s.size() > 8000) {            // as much as fits, whole characters
                    size_t n = 8000 - reportText.size();
                    while (n > 0 && n < s.size() && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
                    s.resize(n);
                }
                insert(s);
            }
            if (pressed(KEY_BACKSPACE) && reportCursor > 0) {             // one whole character back...
                size_t p = prevChar(reportText, reportCursor);
                reportText.erase(p, reportCursor - p);
                reportCursor = p;
            }
            if (pressed(KEY_DELETE) && reportCursor < reportText.size())   // ...or ahead
                reportText.erase(reportCursor, nextChar(reportText, reportCursor) - reportCursor);
            // Left / Right: a character (Alt: a word). Up / Down: a line, keeping to the column.
            // Home / End or Cmd+Left / Cmd+Right: the line's ends; Cmd+Up / Cmd+Down: the text's.
            bool left = pressed(KEY_LEFT), right = pressed(KEY_RIGHT), up = pressed(KEY_UP), down = pressed(KEY_DOWN);
            bool home = IsKeyPressed(KEY_HOME) || (cmdK && left), end = IsKeyPressed(KEY_END) || (cmdK && right);
            if (left || right || up || down || home || end) {
                ReportBoxGeom bg = reportBoxGeom();
                Lines lines = wrapLines(reportText, bg.area.width, bg.fs);
                int k = lineOf(lines, reportCursor);
                if (home) reportCursor = lines[k].first;
                else if (end) reportCursor = lines[k].second;
                else if (left) reportCursor = altK ? wordLeft(reportText, reportCursor) : prevChar(reportText, reportCursor);
                else if (right) reportCursor = altK ? wordRight(reportText, reportCursor) : nextChar(reportText, reportCursor);
                else if (cmdK) reportCursor = up ? 0 : reportText.size();
                else {
                    if (reportWantX < 0) reportWantX = colX(reportText, lines[k], reportCursor, bg.fs);
                    int to = k + (up ? -1 : 1);
                    if (to < 0) reportCursor = 0;                          // above the first line: its start
                    else if (to >= (int)lines.size()) reportCursor = reportText.size();   // below the last: the end
                    else {                                                 // (a word cut in two: not past its line's end)
                        bool cut = to + 1 < (int)lines.size() && lines[to + 1].first == lines[to].second;
                        size_t last = cut ? prevChar(reportText, lines[to].second) : lines[to].second;
                        reportCursor = atX(reportText, lines[to], last, reportWantX, bg.fs);
                    }
                }
                if (!(up || down) || cmdK) reportWantX = -1;
            }
            bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
            if (enter && shiftK) insert("\n");
            if (reportText.size() != wasSize) reportWantX = -1;
            if (reportCursor != was || reportText.size() != wasSize) reportClock = 0;
            bool save = enter && !shiftK, cancel = escHit;
            if (save || cancel) {
                if (save) {
                    std::string txt = saveReport(reportText);
                    reportToastText = txt.empty() ? "Could not save the report" : "Report saved: " + txt;
                } else {
                    reportToastText = "Report cancelled";
                }
                reportToast = 3.0f;
                UnloadImage(reportShot);
                reportShot = Image{};
                reportOpen = false;
            }
        }

        // ---- input, once per frame ----
        // (A highscore's name being typed: the letter keys are its, not the game's.)
        auto letterDown = [&](int k) { return !typing && IsKeyDown(k); };
        auto letterPressed = [&](int k) { return !typing && IsKeyPressed(k); };
        const bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const bool dashInputEnabled = g.anna && !g.dead && g.eatenBy < 0 && g.lostT < 0 && g.goldenSeq < 0 && g.endT < 0;
        if (shiftDown && !shiftWasDown) g.shiftDashPending = true;
        shiftWasDown = shiftDown;
        // Wait for release so using a Shift shortcut does not also start a dash.
        if (!dashInputEnabled || boxOpen || typing || titleWait || startFade >= 0 || restartIn > 1 ||
            escHit || !IsWindowFocused()) g.shiftDashPending = false;
        const bool keyboardDash = !shiftDown && g.shiftDashPending;
        if (!shiftDown) g.shiftDashPending = false;
        auto shiftShortcut = [&](int key) {
            if (!shiftDown || !letterPressed(key)) return false;
            g.shiftDashPending = false;
            return true;
        };
        const bool arrows = !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT);
        bool kl = (arrows && IsKeyDown(KEY_LEFT)) || letterDown(KEY_A) || Input::padDown(GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        bool kr = (arrows && IsKeyDown(KEY_RIGHT)) || letterDown(KEY_D) || Input::padDown(GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        // Up is LOOK, not jump: jumping is Space / W / Cmd (Ctrl off a Mac) / pad A.
        bool jumpPress = IsKeyPressed(KEY_SPACE) || letterPressed(KEY_W) || IsKeyPressed(kJumpKey1) || IsKeyPressed(kJumpKey2) ||
                         Input::padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        g.jumpHeld = IsKeyDown(KEY_SPACE) || letterDown(KEY_W) || IsKeyDown(kJumpKey1) || IsKeyDown(kJumpKey2) ||
                     Input::padDown(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        g.downHeld = (arrows && IsKeyDown(KEY_DOWN)) || letterDown(KEY_S) ||
                     Input::padDown(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || Input::stickY() > 0.5f;
        bool upHeld = (arrows && IsKeyDown(KEY_UP)) || Input::padDown(GAMEPAD_BUTTON_LEFT_FACE_UP) || Input::stickY() < -0.5f;
        float stick = Input::stickX();
        // Tongue: B (keyboard X), thrown where he looks -- the stick, else the arrows, else ahead.
        bool tonguePress = letterPressed(KEY_X) || Input::padPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        // The direction held THIS frame: the stick (its true angle), else the arrows / D-pad.
        Input::Stick st = Input::stick2();
        Vector2 held = {st.x, st.y};
        if (Vector2Length(held) < 0.3f) {
            held = {(kr ? 1.0f : 0.0f) - (kl ? 1.0f : 0.0f), (g.downHeld ? 1.0f : 0.0f) - (upHeld ? 1.0f : 0.0f)};
            if (Vector2Length(held) < 0.1f) held = {0, 0};
        }
        // B pressed with no direction yet -- the stick is often still on its way when the
        // thumb hits B -- waits a moment for one before throwing straight ahead.
        Vector2 aim = held;
        if (tonguePress && Vector2Length(held) < 0.1f) { tongueWait = g.t.tongueAimGrace; tonguePress = false; }
        else if (tongueWait > 0) {
            tongueWait -= frame;
            if (Vector2Length(held) >= 0.1f) { tonguePress = true; tongueWait = 0; }
            else if (tongueWait <= 0) {
                tonguePress = true;
                aim = (Vector2Length(Vector2{st.x, st.y}) > 0.12f) ? Vector2{st.x, st.y} : Vector2{g.facing, 0};
            }
        }
        if (tonguePress && Vector2Length(aim) < 0.1f) aim = {g.facing, 0};

        if (smokeName) {   // scripted: run right, hop now and then
            kl = false;
            kr = smokeClock > 0.3f;
            auto held = [&](float clk) {
                if (smokeJumps >= 0 && clk >= 0.9f + 1.6f * smokeJumps) return false;
                float c = fmodf(clk, 1.6f);
                if (smokeDouble) return (c > 0.9f && c < 1.05f) || (c > 1.2f && c < 1.45f);
                return c > 0.9f && c < 1.3f;
            };
            g.jumpHeld = held(smokeClock);
            jumpPress = g.jumpHeld && !held(smokeClock - frame);
            g.downHeld = smokeLook > 0 && smokeClock > 0.3f;
            upHeld = smokeLook < 0 && smokeClock > 0.3f;
            stick = 0;
            bool running = smokeClock > 0.3f && (smokeStop < 0 || smokeClock < smokeStop);
            kr = smokeRun > 0 && running;
            kl = smokeRun < 0 && running;
            tonguePress = false; tongueWait = 0;
            for (float jt : smokeJumpAt)
                if (smokeClock >= jt && smokeClock - frame < jt) { jumpPress = true; g.jumpHeld = true; }
            for (auto& h : smokeJumpHold) {
                if (smokeClock >= h.x && smokeClock < h.y) g.jumpHeld = true;
                if (smokeClock >= h.x && smokeClock - frame < h.x) jumpPress = true;
            }
            for (auto& ev : smokeTongue)
                if (smokeClock >= ev.x && smokeClock - frame < ev.x) { tonguePress = true; aim = {ev.y, ev.z}; }
            for (float tg : smokeGod)
                if (smokeClock >= tg && smokeClock - frame < tg) g.toggleGod();
        }
        if (tonguePress) { g.tonguePress = true; g.tongueAim = Vector2Normalize(aim); }
        {   // Stuck tongue: RT / V swallows one chew per pull, LT / C held pays it out fast.
            bool rt = letterDown(KEY_V) || Input::rightTrigger();
            bool lt = letterDown(KEY_C) || Input::leftTrigger();
            if (smokeName) {
                rt = false; lt = false;
                for (float ct : smokeChew) if (smokeClock >= ct && smokeClock < ct + 0.05f) rt = true;
                for (auto& h : smokePay) if (smokeClock >= h.x && smokeClock < h.y) lt = true;
            }
            if (rt && !rtWas) g.chewPress = true;
            rtWas = rt;
            g.payHeld = lt;
        }
        {   // Suicide: RT + LT + both sticks pushed in, all held (the pad only).
            bool all = Input::leftTrigger() && Input::rightTrigger() &&
                       Input::padDown(GAMEPAD_BUTTON_LEFT_THUMB) && Input::padDown(GAMEPAD_BUTTON_RIGHT_THUMB);
            if (smokeName) all = smokeSuicide >= 0 && smokeClock >= smokeSuicide;
            g.suicideHeld = all && !boxOpen;
        }
        // Looking: Up/Down, the D-pad, the left stick -- a little (look_screen_position) -- and the
        // right stick, which does nothing else, so the left one can run flat out while the right
        // one looks: much further, as far as it is pushed. One way at a time: up/down or aside.
        g.look = (g.downHeld ? 1.0f : 0.0f) - (upHeld ? 1.0f : 0.0f);
        float rx = smokeName ? (float)smokeLookSide : Input::rightX();
        float ry = smokeName ? 0.0f : Input::rightY();
        if (!arrows && !smokeName) {                      // Alt + arrows: the right stick, pushed all the way
            float ax = (IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) - (IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f);
            float ay = (IsKeyDown(KEY_DOWN) ? 1.0f : 0.0f) - (IsKeyDown(KEY_UP) ? 1.0f : 0.0f);
            if (ax != 0 || ay != 0) { rx = ax; ry = ay; }
        }
        g.farLook = fabsf(rx) > fabsf(ry) ? Vector2{rx, 0} : Vector2{0, ry};

        // Digital command (keys, D-pad): ramps up while held, so a tap is a slow, precise step
        // (low gain) and a long press reaches full speed (high gain) -- the spec's G(v_cmd).
        float kdir = (kr ? 1.0f : 0.0f) - (kl ? 1.0f : 0.0f);
        if (kdir == 0) g.keyCmd = 0;
        else if (signf(g.keyCmd) != kdir) g.keyCmd = kdir * g.t.keyStart;
        else g.keyCmd = clampf(g.keyCmd + kdir * frame / fmaxf(0.001f, g.t.keyRamp), -1, 1);
        g.cmd = (fabsf(stick) > fabsf(g.keyCmd)) ? stick : g.keyCmd;
        if (startLatch) {                                // until the title's A (or Space) is let go, no jump
            if (g.jumpHeld) { jumpPress = false; g.jumpHeld = false; }
            else startLatch = false;
        }
        if (jumpPress) g.jumpBuf = g.t.jumpBuffer;
        bool dash = Input::padPressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1) || Input::padPressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) || keyboardDash;
        if (smokeName) {
            dash = false;
            for (float d : smokeDash)
                if (smokeClock >= d && smokeClock - frame < d) dash = true;
        }
        if (dashInputEnabled && dash) g.dashPress = true;

        if (boxOpen) {                                                    // typing: the game hears nothing
            g.jumpBuf = 0; g.tonguePress = false; g.chewPress = false; g.cmd = 0; g.keyCmd = 0; tongueWait = 0;
            g.dashPress = false;
        }
        {   // The guide arrow comes once no control has been touched for guide_arrow_wait.
            bool any = boxOpen || titleWait || kl || kr || g.jumpHeld || g.downHeld || upHeld || tonguePress ||
                       tongueWait > 0 || rtWas || g.payHeld || g.farLook.x != 0 || g.farLook.y != 0 || Input::anyActive();
            for (int k = KEY_SPACE; k <= KEY_KB_MENU && !any; k++) any = IsKeyDown(k);
            g.idle = any ? 0 : g.idle + frame;
            if (g.idle >= g.t.arrowWait && !g.dead && g.eatenBy < 0 && !g.gone()) g.arrowA = fminf(1, g.arrowA + frame / g.t.arrowFadeIn);
            else g.arrowA = fmaxf(0, g.arrowA - frame / g.t.arrowFadeOut);
        }
        if (!boxOpen && !reportWanted && !smokeName && shiftShortcut(KEY_B)) reportWanted = true;
        auto titleWho = [&] {                             // (back to the title: Leon's game till it is finished, then Anna's -- both: the choice, as last)
            if (forcePlayer < 0 && progress.stage() < 2) g.anna = progress.stage() == 1;
            g.endMsg = endWords();
        };
        auto restartNow = [&] {
            closeEntry();                                 // (kept; the run's line no longer marked -- a new game, no name yet)
            entry.part = -1;
            gameName.clear();
            mine.clear();
            g.level.load(levelPath().c_str());
            if (spawnX >= 0) g.level.spawn = {(spawnX + 0.5f) * kTile, (spawnY + 1) * kTile};
            g.reset();
            restartIn = -1;
            if (g.trace) fprintf(stderr, "t=%.2f restart\n", g.time);
        };
        bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool restartKey = shiftShortcut(KEY_R);
        bool countdownKey = restartKey && ctrlDown;
        if (smokeName && smokeCountdown >= 0 && smokeClock >= smokeCountdown && smokeClock - frame < smokeCountdown) countdownKey = true;
        if (!boxOpen && countdownKey) {                   // Shift+Ctrl+R: the restart -- then 3, 2, 1, GO, Leon held till GO
            restartNow();
            restartIn = 4;
        } else if (!boxOpen && restartKey) {   // Shift+R: restart
            restartNow();
        }
        if (restartIn > 0 && !boxOpen && (restartIn -= frame) <= 0) restartIn = -1;
        const bool countingIn = restartIn > 1;            // 3, 2, 1: nothing moves, the clocks wait -- his eyes shut till GO
        g.eyesHeld = countingIn;
        if (countingIn) g.fadeIn = fmaxf(0, g.fadeIn - frame);   // (back after a suicide: the fade in plays meanwhile)
        if (countingIn && g.goldenOutT >= 0) g.goldenOutT += frame;   // (the golden run's start: its light fades out meanwhile)
        if (!boxOpen && shiftShortcut(KEY_G) && g.debugMode()) g.toggleGod();   // Shift+G, in debug mode: GOD on / off
        if (!boxOpen && shiftShortcut(KEY_S)) g.engrave = !g.engrave;   // Shift+S: the sketch <-> the engraving
        // Cheat mode -- the debug keys, the jumps from mode to mode -- locked till the code is typed:
        // cheat_code (SUPERBO), its letters one after the other (either case: Shift and Caps Lock let
        // be), nothing else pressed (key or pad button) between them, all within 4 seconds. Typed,
        // debug mode at once (DEBUG MODE, the stats), till the game quits.
        if (!g.cheats && !boxOpen && !typing) {
            const std::string& code = g.t.cheatCode;
            const int len = (int)code.size();
            auto keyOf = [](char c) { return c >= '0' && c <= '9' ? KEY_ZERO + (c - '0') : KEY_A + (c - 'a'); };
            int pressed = 0, key = 0;
            for (int k = KEY_SPACE; k <= KEY_KB_MENU; k++)
                if (IsKeyPressed(k) && k != KEY_LEFT_SHIFT && k != KEY_RIGHT_SHIFT && k != KEY_CAPS_LOCK) { pressed++; key = k; }
            for (int b = 1; b <= GAMEPAD_BUTTON_RIGHT_THUMB; b++) if (Input::padPressed(b)) pressed += 2;
            codeT += frame;
            if (codeAt >= len || (codeAt > 0 && codeT > 4)) codeAt = 0;   // (too slow -- or the code changed in params.txt: from the start again)
            if (pressed) {
                if (pressed == 1 && key == keyOf(code[codeAt])) { if (codeAt == 0) codeT = 0; codeAt++; }
                else { codeAt = pressed == 1 && key == keyOf(code[0]) ? 1 : 0; codeT = 0; }
                if (codeAt == len) {
                    g.cheats = true; codeAt = 0; cheatsWait = 0.5f;
                    g.lesson("Cheats !");
                    g.sound(Audio::kFruit, 0.8f);
                    if (g.trace) fprintf(stderr, "t=%.2f cheats unlocked\n", g.time);
                }
            }
        }
        cheatsWait = fmaxf(0, cheatsWait - frame);       // (the code just typed: half a second before any cheat key -- not its own last O)
        if (g.cheats && cheatsWait <= 0 && !boxOpen && !titleWait && !typing) {   // cheat mode: 1 / 2 / 3 / 4 / 5 / 8 / 9 / 0, on or off (Game::toggle)
            const int keys[][2] = {{KEY_ONE, 1}, {KEY_TWO, 2}, {KEY_THREE, 3}, {KEY_FOUR, 4}, {KEY_FIVE, 5}, {KEY_EIGHT, 8}, {KEY_NINE, 9}, {KEY_ZERO, 0}};
            for (auto& k : keys) if (IsKeyPressed(k[0])) g.toggle(k[1]);
        }
        // Cheat mode: T, Y, U, I, O -- off to the start of easy, hard, dark, harder, cameleon mode: the run
        // restarted there, as start_level would have it. P: the end of the game -- at the golden key.
        if (g.cheats && cheatsWait <= 0 && !boxOpen && !typing && !titleWait && !shiftDown) {
            const int keys[6] = {KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P};
            for (int k = 0; k < 6; k++)
                if (IsKeyPressed(keys[k])) {
                    int keep = g.t.startPart;
                    g.t.startPart = k;
                    restartNow();
                    g.t.startPart = keep;
                    if (g.trace) fprintf(stderr, "t=%.2f cheat: to the start of mode %d\n", g.time, k);
                }
        }
        fsSince += frame;
#ifdef __EMSCRIPTEN__
        if (!boxOpen) shiftShortcut(KEY_F);     // consumes the Shift dash chord
#else
        bool smokeFs = false;                             // (tests: --smoke-fullscreen T)
        for (float ft : smokeFullscreen) if (smokeName && smokeClock >= ft && smokeClock - frame < ft) smokeFs = true;
        if (smokeFs || (!boxOpen && shiftShortcut(KEY_F) && fsSince >= g.t.fullscreenWait)) {   // Shift+F: fullscreen
            bool leaving = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            if (!leaving) { winPos = GetWindowPosition(); winW = GetScreenWidth(); winH = GetScreenHeight(); }
            ToggleBorderlessWindowed();
            if (leaving) { SetWindowSize(winW, winH); SetWindowPosition((int)winPos.x, (int)winPos.y); }
            if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) HideCursor(); else ShowCursor();
            fsSince = 0;
        }
#endif
        for (const Vector3& rs : smokeResize)             // (tests: --smoke-resize T,W,H)
            if (smokeName && smokeClock >= rs.x && smokeClock - frame < rs.x) SetWindowSize((int)rs.y, (int)rs.z);
        bool shot = !boxOpen && shiftShortcut(KEY_T);   // Shift+T: a screenshot

        // ---- ESC: the question ----
        bool escPress = escHit || (smokeName && smokeEsc >= 0 && smokeClock >= smokeEsc && smokeClock - frame < smokeEsc);
        if (ask && askOpen) {
            if (kl || stick < -0.5f) askYes = true;
            if (kr || stick > 0.5f) askYes = false;
            bool answer = jumpPress || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
            if (escPress) { askYes = false; answer = true; }
            if (answer) {
                askOpen = false;
                startLatch = true;                        // (the jump that answered is not one)
                if (g.trace) fprintf(stderr, "t=%.2f %s: %s\n", g.time, ask == 1 ? "exit the game" : "back to the title", askYes ? "yes" : "no");
                if (askYes && ask == 1) quit = true;
                if (askYes && ask == 2) { g.onTitle = true; titleWho(); restartNow(); titleWait = true; startFade = -1; titleIntro = 0; }
            }
        } else if (!ask && !boxOpen && !typing && startFade < 0 && escPress) {
            ask = titleWait ? 1 : 2; askOpen = true; askYes = false;
        }
        askA = askOpen ? fminf(1, askA + frame / g.t.escFade) : fmaxf(0, askA - frame / g.t.escFade);
        if (!askOpen && askA <= 0) ask = 0;
        if (quit) break;

        // The title holds the game until A (or Space / Enter); that press is used up by it. Then the
        // screen fades out and back in to the game over title_start_fade_time, the game still held:
        // it starts -- the clock with it -- once the fade is over.
        if (titleWait || startFade >= 0) {
            titleClock += frame;
            if (titleWait && fsSince >= g.t.fullscreenWait) titleIntro += frame;   // (black till the window has settled: the switch to fullscreen at the start glitched the fade)
            bool go = Input::padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER);
            if (smokeName) go = smokeStart >= 0 && smokeClock >= smokeStart;
            // Both games finished: Y (pad or keyboard) for Leon, X for Anna -- A starts nothing. (Tests: the
            // start, as --leon / --anna says -- Leon if neither.)
            const int stage = progress.stage();
            int pick = -1;
            if (stage == 2) {
                bool y = Input::padPressed(GAMEPAD_BUTTON_RIGHT_FACE_UP) || IsKeyPressed(KEY_Y);
                bool x = Input::padPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT) || IsKeyPressed(KEY_X);
                if (smokeName) { y = go && forcePlayer != 1; x = go && forcePlayer == 1; }
                pick = y ? 0 : x ? 1 : -1;
                go = pick >= 0;
            }
            // Debug mode, the two not both finished: Shift+Enter, or LB and RB together, switches between
            // Leon's game and Anna's -- out to black and in again over title_switch_fade_time.
            bool switchPress = shiftDown && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
            switchPress = switchPress || (Input::padDown(GAMEPAD_BUTTON_LEFT_TRIGGER_1) && Input::padDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) &&
                                          (Input::padPressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1) || Input::padPressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)));
            if (smokeName) switchPress = smokeSwitch >= 0 && smokeClock >= smokeSwitch && smokeClock - frame < smokeSwitch;
            if (switchPress) go = false;                  // (Shift+Enter: not a start)
            if (titleWait && switchPress && switchT < 0 && stage < 2 && g.debugMode() && !boxOpen && titleIntro >= g.t.titleIntro) {
                switchT = 0;
                if (g.trace) fprintf(stderr, "t=%.2f title: switching to %s's game\n", g.time, g.anna ? "Leon" : "Anna");
            }
            if (switchT >= 0) {
                float was = switchT, half = g.t.titleSwitchFade / 2;
                switchT += frame;
                if (was < half && switchT >= half) { g.anna = !g.anna; g.endMsg = endWords(); restartNow(); }
                if (switchT >= g.t.titleSwitchFade) switchT = -1;
                go = false;
            }
            if (titleWait && go && !boxOpen && titleIntro >= g.t.titleIntro) {   // (not before the title is in)
                titleWait = false; startFade = 0; startLatch = true;
                if (pick >= 0) g.anna = pick == 1;
                g.endMsg = endWords();
                if (g.trace) fprintf(stderr, "t=%.2f title: %s's game\n", g.time, g.anna ? "Anna" : "Leon");
            }
            else if (startFade >= 0) {
                float was = startFade;
                startFade += frame;
                if (g.onTitle && was < g.t.titleStartFade / 2 && startFade >= g.t.titleStartFade / 2) {
                    g.onTitle = false;                    // (the screen black: on to start_level's place)
                    g.startAtPart(g.t.startPart);
                }
                if (startFade >= g.t.titleStartFade) startFade = -1;
            }
            g.jumpBuf = 0; g.tonguePress = false; g.chewPress = false; g.cmd = 0; g.keyCmd = 0; g.dashPress = false;
        }
        const bool starting = startFade >= 0;
        const bool titleShown = titleWait || (starting && startFade < g.t.titleStartFade / 2);   // (out, then in to the game)
        const int titleLook = progress.stage() == 2 ? 2 : g.anna ? 1 : 0;   // (drawTitle's `who`)
        const bool duo = titleShown && titleLook == 2;    // (the two finished: on the title, face to face -- not the one playing)
        gTitleDuo = duo;

        // ---- simulation, fixed steps ----
        if (countingIn) { g.jumpBuf = 0; g.tonguePress = false; g.chewPress = false; g.cmd = 0; g.keyCmd = 0; g.suicideHeld = false; g.dashPress = false; }
        g.creditsFast = g.downHeld;                       // (the ending's credits: down held, faster)
        if (g.endT >= 0) {                                // the ending: Leon no longer his to move -- nor to look about
            g.jumpBuf = 0; g.jumpHeld = false; g.tonguePress = false; g.chewPress = false; g.cmd = 0; g.keyCmd = 0; g.suicideHeld = false;
            g.downHeld = false; g.payHeld = false; g.look = 0; g.farLook = {0, 0};
        }
        if (!titleWait && !starting && !boxOpen && !countingIn) acc += frame;   // the box holds the game (and the 3, 2, 1)
        g.creditsH = creditsHeight((float)gameW() / std::max(1, gameH()));   // (the ending's credits: how long they roll, pictures and all)
        while (acc >= kStep) { g.step(kStep); acc -= kStep; }
        if (g.goldenDone) goldenSeen[g.anna] = true;
        if (g.endT < 0) endCounted = false;
        else if (!endCounted && g.endBlack() >= 1) {      // the end reached: this game finished, for this session
            endCounted = true;
            (g.anna ? progress.anna : progress.leon) = true;
            if (g.trace) fprintf(stderr, "t=%.2f %s's game finished (stage %d now)\n", g.time, g.anna ? "Anna" : "Leon", progress.stage());
        }
        if (g.wantCountdown) {                            // back after a suicide -- or landed, a golden run's start: 3, 2, 1, GO -- no saliva hanging in the air meanwhile
            g.wantCountdown = false; restartIn = 4;
            g.parts.clear();                              // (a drool begun as he came back would hang there, frozen, all through it)
            if (g.goldenRun && !g.goldenDone) g.goldenTime = 0;   // (its clock from GO)
        }
        if (g.endBlack() >= 1 && !boxOpen && !ask) {      // the ending's end, its message: A (or Space) -- back to the title
            bool go = Input::padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_SPACE);
            if (smokeName) go = smokeStart >= 0 && smokeClock >= smokeStart;
            if (go) {
                if (g.trace) fprintf(stderr, "t=%.2f the ending: back to the title\n", g.time);
                g.onTitle = true;
                titleWho();
                restartNow();
                titleWait = true; startFade = -1; titleIntro = 0;
                if (g.t.titleIntro <= 0) titleFadeIn = g.t.titleStartFade / 2;   // (else: from black, as ever)
            }
        }
        if (g.timeUpOver) {                               // a clock's hour out, he exploded: back to the title, fading in
            g.onTitle = true;
            titleWho();
            restartNow();
            titleWait = true; startFade = -1; titleIntro = 0;
            if (g.t.titleIntro <= 0) titleFadeIn = g.t.titleStartFade / 2;   // (else: from black, as ever)
        }
        if (!g.partsDone.empty()) {                       // parts done: their times against the highscores
            // (A key or a power given -- start_with_*, start_level, or a debug key -- and the run's times are
            // not fair: the tables still show at the flags, no name is asked.)
            // (In a golden run, its own time only: the modes run in it count for nothing.)
            if ((!smokeName || scoresFileSet) && !g.debugMode())
                for (auto& pd : g.partsDone)
                    if (!g.goldenRun || pd.first == 5) scorePending.push_back(pd);
            g.partsDone.clear();
        }
        while (!reportOpen && !scorePending.empty()) {   // a good one: three letters (one still waiting: kept as it is)
            auto [k, tm] = scorePending.front();
            scorePending.pop_front();
            int place = myScores().place(k, tm);
            if (place < 0) continue;
            closeEntry();
            entry = NameEntry{};
            entry.open = true; entry.part = k; entry.time = tm; entry.place = place;
            entry.len = (int)gameName.size(); entry.copied = entry.len > 0;
            memcpy(entry.name, gameName.c_str(), gameName.size());
            if (!testName.empty()) {                      // tests: the name typed, then Enter
                entry.len = (int)std::min<size_t>(3, testName.size()); entry.copied = false;
                memcpy(entry.name, testName.c_str(), entry.len);
                testName.clear();
                closeEntry();
                continue;
            }
            while (GetCharPressed() > 0) {}
        }
        // A highscore's name is typed by its flag only: walking off, it is kept as it is ('???' if
        // none typed); coming back to that flag later, it can be typed again.
        if (entry.open && g.partNear(entry.part) <= 0) {
            closeEntry();
            for (MyScore& ms : mine) if (ms.part == entry.part && ms.time == entry.time) ms.away = true;
        }
        for (MyScore& ms : mine) {
            if (g.partNear(ms.part) <= 0) { ms.away = true; continue; }
            if (!ms.away || entry.open || reportOpen) continue;
            ms.away = false;
            std::vector<Scores::Entry>& rows = myScores().part[ms.part];
            for (size_t r = 0; r < rows.size(); r++) {
                if (fabsf(rows[r].t - ms.time) > 1e-4f) continue;
                entry = NameEntry{};
                entry.open = true; entry.part = ms.part; entry.time = ms.time; entry.place = (int)r;
                const std::string& nm = rows[r].name;     // (its name offered again -- a letter typed starts another)
                while (entry.len < 3 && entry.len < (int)nm.size() && nm[entry.len] != '?') { entry.name[entry.len] = nm[entry.len]; entry.len++; }
                entry.copied = entry.len > 0;
                rows.erase(rows.begin() + r);             // (back in at its place once kept)
                while (GetCharPressed() > 0) {}
                if (g.trace) fprintf(stderr, "highscore %s: back by its flag, its name to be typed again\n", Scores::tag(ms.part));
                break;
            }
        }
        float alpha = acc / kStep;
        for (auto& e : g.sounds) Audio::play(e.id, e.pitch, e.volume);
        g.sounds.clear();
        if (g.music() != musicPlaying) {                  // the music the moment calls for: out, silence, in
            musicPlaying = g.music();
            Audio::changeMusic(kMusic[musicPlaying]);
            if (g.trace) fprintf(stderr, "t=%.2f music: %s\n", g.time, *kMusic[musicPlaying] ? kMusic[musicPlaying] : "none (silence)");
        }
        const bool quietForPrompt = ask == 2 && askOpen && g.endOffT < 0;
        Audio::update(quietForPrompt, g.t.musicFadeOut, g.t.musicGap, g.t.musicFadeIn);
        // The music's time, for the ending's brims to keep with: the mixer gives it in steps (10 ms), so
        // it runs on the real frame time, drawn gently onto the mixer's -- taken as it is when it jumps
        // (the music just begun, round again, off).
        if (float mt = Audio::musicTime(kMusic[musicPlaying]); mt < 0) g.musicT = -1;
        else {
#ifdef __EMSCRIPTEN__
            float ahead = g.musicT + frame;
#else
            float ahead = g.musicT + fminf(GetFrameTime(), 0.1f);
#endif
            g.musicT = g.musicT < 0 || fabsf(mt - ahead) > 0.25f ? mt : ahead + 0.1f * (mt - ahead);
        }
        if (g.trace && g.musicT >= 0) {                    // (tests: the music's clock, once a second of it)
            static int musicSecond = -1;
            if ((int)g.musicT != musicSecond) {
                musicSecond = (int)g.musicT;
                fprintf(stderr, "t=%.2f music time %.2f s (%s)%s\n", g.time, g.musicT, kMusic[musicPlaying], g.endHappy() ? TextFormat(", brims %.3f", g.endBrimCurl()) : "");
            }
        }
        if (!titleWait && !starting) g.hintTimer -= frame;   // the help waits with the title
        smokeClock += frame;

        // ---- draw ----
        float zoom = gameH() / g.viewSize().y;
        Vector2 camP = {lerpf(g.prevCam.x, g.cam.x, alpha), lerpf(g.prevCam.y, g.cam.y, alpha)};
        Vector2 leonP = {lerpf(g.prevPos.x, g.pos.x, alpha), lerpf(g.prevPos.y, g.pos.y, alpha)};
        float tr = g.trauma * g.trauma;
        float n1 = sinf(g.time * 71.0f) * 0.6f + sinf(g.time * 113.0f) * 0.4f;
        float n2 = sinf(g.time * 83.0f + 1.7f) * 0.6f + sinf(g.time * 127.0f) * 0.4f;
        float n3 = sinf(g.time * 59.0f + 3.1f);

        // Everything below is in the world: turn Leon's frame out, and turn the screen so that
        // gravity points down it. Leon himself lags the turn by player_turn_delay.
        float drawTime = g.time - (1 - alpha) * kStep;
        float worldAngle = g.frameAngle(drawTime);
        float leonAngle = g.frameAngle(drawTime - g.t.playerTurnDelay);
        Vector2 camW = g.toWorld(camP);
        Vector2 leonC = g.toWorld({leonP.x, leonP.y - kLeonH / 2});
        leonC = Vector2Add(leonC, g.turnLift(leonC, drawTime - g.t.playerTurnDelay));   // (a quarter turn begun on the ground: not sunk in)

        Camera2D c{};
        c.offset = {gameW() / 2.0f, gameH() / 2.0f};
        c.target = {camW.x + n1 * g.t.shakePx * tr, camW.y + n2 * g.t.shakePx * tr};
        c.rotation = -worldAngle + n3 * g.t.shakeRot * tr;
        c.zoom = zoom;
        Vector2 vs = g.viewSize();
        float rad = sqrtf(vs.x * vs.x + vs.y * vs.y) / 2 + 80;   // any rotation of the view fits
        Rectangle view = {c.target.x - rad, c.target.y - rad, 2 * rad, 2 * rad};
        gView = view;
        {   // (the world the game's area shows, turned as it is: the box round its four corners, a pixel more)
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            for (Vector2 s : {Vector2{0, 0}, Vector2{(float)gameW(), 0}, Vector2{0, (float)gameH()}, Vector2{(float)gameW(), (float)gameH()}}) {
                Vector2 w = GetScreenToWorld2D(s, c);
                x0 = fminf(x0, w.x); y0 = fminf(y0, w.y); x1 = fmaxf(x1, w.x); y1 = fmaxf(y1, w.y);
            }
            gSeen = {x0 - 1, y0 - 1, x1 - x0 + 2, y1 - y0 + 2};
        }

        // The engraving's clouds slide by the camera's own motion on the screen (a share of it for
        // each layer) and their wind: a change of gravity turns the frame but moves no camera, so
        // they never jump; nor do they sweep across the sky when the camera is cut to a checkpoint.
        {
            if (!cloudInit) { cloudCamW = camW; cloudInit = true; }
            Vector2 dW = Vector2Subtract(camW, cloudCamW);
            cloudCamW = camW;
            if (Vector2Length(dW) > vs.x + vs.y) dW = {0, 0};
            float a = -worldAngle * DEG2RAD, ca = cosf(a), sa = sinf(a);
            Vector2 dS = {(ca * dW.x - sa * dW.y) * zoom, (sa * dW.x + ca * dW.y) * zoom};
            float ui = fmaxf(1.0f, gameH() / 720.0f);
            for (int k = 0; k < 3; k++) {
                cloudScroll[k].x += dS.x * kCloudPar[k] - kCloudWind[k] * g.t.engraveWind * ui * frame;
                cloudScroll[k].y = (cloudScroll[k].y + dS.y * kCloudPar[k] * 0.5f) * expf(-frame / 6.0f);   // back to their height, slowly
            }
        }
        BeginDrawing();
        ClearBackground(BLACK);                           // (the bands round the game's area)
        gameViewport();
        // The world, drawn straight to the screen (the engraving: on a canvas first, cut into lines).
        const int sw = gameW(), sh = gameH();
        Camera2D cw = c;
        // A colour fly in his belly: his body tinted with its colour (his hat's white too). In a
        // cloud, Leon melts into it where it covers him -- tongue and all; the rest of him whole,
        // tinted, and his eyes over it all: in a cloud of the colour he glows, down to color_hide
        // (his hat, on his head or not, color_hide_hat); with no colour fly in him, in a cloud of any
        // colour, color_hide_plain (his hat as much). Melting, not fading: what is behind him stays
        // hidden -- the cloud shows in his place. In a cloud of another colour than his, he is seen
        // through it, its colour and his mixed (subtractive, as where clouds cross). He and his hat
        // are drawn first on their own canvases, the world's size, the clouds that count laid over
        // them (their colours mixed in, all but that share), the others' filters over that; his
        // shape kept; then laid into the world.
        const int bellyCol = g.bellyColour(), hatCol = g.hatColour();   // (the hat's: his, or the one it keeps a while -- Game::hatKeep)
        auto tinted = [&](int col) { return col < 0 ? kBody : ColorLerp(kBody, g.t.colorOf[col], 0.4f + 0.1f * sinf(g.time * 4)); };
        gAnnaShape = g.anna;
        gAnnaMouthScale = g.t.annaMouthScale; gAnnaMouthH = g.t.annaMouthHeight;
        gLeonBody = g.god ? kGold : bellyCol >= 0 ? tinted(bellyCol) : g.bodyColour();   // (GOD: golden)
        gLeonBody = ColorLerp(gLeonBody, kPaper, g.endGlow());     // (the ending, face to face with Anna: glowing)
        gLeonHat = g.god ? kGold : bellyCol >= 0 ? tinted(bellyCol) : g.hatWhite();   // (his hat's white: the Love door's pink, a fly's tint, GOD's gold -- the Solo door's gold, not)
        gHatBand = g.lastKey >= 0 ? g.t.keyCol[g.lastKey] : kInk;   // (the hat's bands: the last key's colour, black before any)
        gHatBand.a = 255;
        gHatBand = ColorLerp(gHatBand, g.t.leonLove, g.hatPink());  // (through the Love door: pink, as its white -- no stripes)
        gTongue = bellyCol < 0 ? kTongue : g.t.colorOf[bellyCol];
        auto cloudNear = [&](Vector2 q, float reach) {     // a puff of any colour, within `reach` of q
            Vector2 p;
            float r;
            for (const Game::Gas& gs : g.gases)
                for (size_t i = 0; i < gs.p.size(); i++)
                    if (g.puffAt(gs, i, p, r) && Vector2Distance(p, q) < r + reach) return true;
            return false;
        };
        const bool hatOff = !g.hasHat && (g.hatLoose || (!g.hatTaken && g.level.hasHatSpot));
        const bool hidden = !g.dead && !g.swallowed()                // (his hat on him too; his tongue out: any cloud)
                            && (cloudNear(leonC, 2 * kLeonH) || (g.tg != Game::tgNone && cloudNear(leonC, 1e6f)));
        const bool hatHidden = g.hasHat ? hidden : hatOff && cloudNear(g.hatLoose ? g.hatP : g.level.hatSpot, kTile);
        const float leonHide = bellyCol < 0 ? g.t.colorHidePlain : g.t.colorHide;
        const float hatHide = hatCol < 0 ? g.t.colorHidePlain : g.t.colorHideHat;
        // Anna's off her head: whole, its brim 11 px under its middle as his (her cone's middle near it) --
        // no taller than `room` over its brim, its top bands folded down (its spot, under a block).
        auto annaHatOff = [&](Color cone, float room = 1e9f) {
            float h[4], left = room / kAnnaCone * 4;
            for (int k = 0; k < 4; k++) { h[k] = clampf(left, 0, 1); left -= h[k]; }
            const Color was = gHatWhite;
            gHatWhite = cone;
            drawAnnaHat({0, 11}, 0, h, 0, 0, 1, 1, 1, fmaxf(0, room - kBrimHalf) / kAnnaPlayS, 1e9f, 1e9f, g.t.annaBrim);
            gHatWhite = was;
        };
        auto waitingHat = [&] {                            // the hat at its spot, bobbing, not taken yet
            if (!g.level.hasHatSpot || g.hatTaken || !inView(g.level.hatSpot, kTile)) return;
            Vector2 hp = Vector2Add(g.level.hatSpot, Vector2Scale(g.pullW, -sinf(g.time * 2.1f) * 3.0f));
            rlPushMatrix();
            rlTranslatef(hp.x, hp.y, 0);
            rlRotatef(worldAngle, 0, 0, 1);
            if (g.anna) {                              // (Anna playing: hers, her colour -- under its spot's ceiling)
                const int tx = (int)floorf(g.level.hatSpot.x / kTile), ty = (int)floorf(g.level.hatSpot.y / kTile);
                float room = 1e9f;
                if (g.grav == 0)
                    for (int up = 1; up <= 2 && room > 1e8f; up++)
                        if (g.level.block(tx, ty - up)) room = g.level.hatSpot.y + 11 - (ty - up + 1) * kTile;
                annaHatOff(g.t.annaCol, room);
            }
            else drawHat({0, 11}, 0, 1, 1);
            rlPopMatrix();
        };
        auto looseHat = [&] {                              // the hat knocked off his head: rolling, lying there, on his tongue
            if (!g.hatLoose || !inView(g.hatP, kTile)) return;
            rlPushMatrix();
            rlTranslatef(g.hatP.x, g.hatP.y, 0);
            rlRotatef(g.hatA, 0, 0, 1);
            gHatWhite = g.hatKeep >= 0 ? tinted(g.hatKeep) : g.hatWhite();   // (the colour it keeps a while -- else its own, as on his head)
            if (g.anna) annaHatOff(gHatWhite);
            else drawHat({0, 11}, 0, 1, 1);
            gHatWhite = kBody;
            rlPopMatrix();
        };
        auto canvas = [&](RenderTexture2D& rt, auto draw) {
            const int W = sw, H = sh;
            if (rt.id == 0 || rt.texture.width != W || rt.texture.height != H) {
                if (rt.id != 0) UnloadRenderTexture(rt);
                rt = loadCanvas(W, H);
                SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
            }
            BeginTextureMode(rt);
            ClearBackground(BLANK);
            BeginMode2D(cw);
            draw();
            EndMode2D();
            EndTextureMode();
            gameViewport();
        };
        // The clouds: each colour on its own canvas, its clouds one gas -- the most of them where
        // they cross, never thicker -- premultiplied by their opacity a (Game::gasOpacity); then
        // the colours mixed where they cross as inks or coloured glass do (subtractive: blue and
        // gold make a grey-green): each a filter, 1 - a (1 - its colour), multiplied in, its cover
        // joined -- into gasRT, laid into the world behind the ground. Leon or his hat in a cloud:
        // those that count into maskRT too (he melts into them), the others into otherMaskRT (his
        // colours through their filter). A canvas starts white, through and through; it holds the
        // product of the filters, and in its alpha that of what each lets through (1 - a); done,
        // it is turned premultiplied, as one cloud is: the filters less what shows through, the cover.
        // Every pass but the clears only where the clouds are -- gasBox, their extent on the
        // canvas, in whole pixels: nothing reads the canvases outside it (a blend over the whole
        // screen, a dozen times a frame, cost a third of the frame rate).
        const int cvW = sw, cvH = sh;
        Rectangle gasBox{0, 0, 0, 0};
        {
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            Vector2 p;
            float r;
            for (const Game::Gas& gs : g.gases)
                for (size_t i = 0; i < gs.p.size(); i++)
                    if (g.puffAt(gs, i, p, r)) {
                        r += 3;                            // (its rim: 2.2 px past it)
                        x0 = fminf(x0, p.x - r); y0 = fminf(y0, p.y - r); x1 = fmaxf(x1, p.x + r); y1 = fmaxf(y1, p.y + r);
                    }
            if (x1 > x0) {                                 // (its corners on the canvas: turned with the screen, maybe)
                float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
                for (Vector2 q : {Vector2{x0, y0}, Vector2{x1, y0}, Vector2{x0, y1}, Vector2{x1, y1}}) {
                    Vector2 s = GetWorldToScreen2D(q, cw);
                    u0 = fminf(u0, s.x); v0 = fminf(v0, s.y); u1 = fmaxf(u1, s.x); v1 = fmaxf(v1, s.y);
                }
                u0 = fmaxf(0, floorf(u0) - 1); v0 = fmaxf(0, floorf(v0) - 1);
                u1 = fminf((float)cvW, ceilf(u1) + 1); v1 = fminf((float)cvH, ceilf(v1) + 1);
                if (u1 > u0 && v1 > v0) gasBox = {u0, v0, u1 - u0, v1 - v0};
            }
        }
        const bool gasOn = gasBox.width > 0;
        auto boxedIn = [&](const RenderTexture2D& rt, Color tint, Rectangle r) {   // a canvas's rectangle r, drawn where it is
            DrawTextureRec(rt.texture, {r.x, cvH - r.y - r.height, r.width, -r.height}, {r.x, r.y}, tint);
        };
        auto boxed = [&](const RenderTexture2D& rt, Color tint) { boxedIn(rt, tint, gasBox); };   // (its gasBox)
        Rectangle feetBox{0, 0, 0, 0};                    // a band round his feet, on the canvas, in gasBox (for cloudOver)
        if (gasOn) {
            Vector2 F = Vector2Add(leonC, Vector2Scale(g.pullW, kLeonH / 2)), s = {-g.pullW.y, g.pullW.x};
            float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
            for (float i : {-1.0f, 1.0f})
                for (float j : {-1.0f, 1.0f}) {
                    Vector2 q = GetWorldToScreen2D(Vector2Add(F, Vector2Add(Vector2Scale(s, i * (kLeonW / 2 + 6)), Vector2Scale(g.pullW, j * 6))), cw);
                    u0 = fminf(u0, q.x); v0 = fminf(v0, q.y); u1 = fmaxf(u1, q.x); v1 = fmaxf(v1, q.y);
                }
            Rectangle r = {floorf(u0), floorf(v0), ceilf(u1) - floorf(u0), ceilf(v1) - floorf(v0)};
            if (CheckCollisionRecs(r, gasBox)) feetBox = GetCollisionRec(r, gasBox);
        }
        auto mixInto = [&](RenderTexture2D& rt, const RenderTexture2D& layer) {   // (dst times the filter: a c + 1 - a)
            BeginTextureMode(rt);
            rlSetBlendFactorsSeparate(RL_DST_COLOR, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            boxed(layer, WHITE);
            EndBlendMode();
            EndTextureMode();
            gameViewport();
        };
        auto mixDone = [&](RenderTexture2D& rt) {          // (colour - alpha, 1 - alpha)
            BeginTextureMode(rt);
            rlSetBlendFactorsSeparate(RL_DST_ALPHA, RL_ONE, RL_ONE, RL_ONE, RL_FUNC_REVERSE_SUBTRACT, RL_FUNC_SUBTRACT);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            DrawRectangleRec(gasBox, WHITE);
            EndBlendMode();
            EndTextureMode();
            gameViewport();
        };
        // The masks are for one colour (maskCol): Leon's -- or his hat's, built again for it when
        // it keeps another a while (Game::hatKeep).
        const bool masks = gasOn && (hidden || hatHidden);
        int maskCol = hidden ? bellyCol : hatCol;
        auto layers = [&](bool gas) {                     // (gas: the world's canvas too)
            const bool others = masks && maskCol >= 0;
            if (gas) canvas(gasRT, [] { ClearBackground(WHITE); });
            if (masks) canvas(maskRT, [] { ClearBackground(WHITE); });
            if (others) canvas(otherMaskRT, [] { ClearBackground(WHITE); });
            for (int k = 0; k < 3; k++) {
                bool any = false;
                for (const Game::Gas& gs : g.gases) any = any || gs.colour == k;
                if (!any) continue;
                canvas(gasScratch, [&] {
                    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ONE, RL_ONE, RL_MAX, RL_MAX);
                    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                    for (const Game::Gas& gs : g.gases)
                        if (gs.colour == k) drawGas(g, gs, g.gasOpacity(gs));
                    EndBlendMode();
                });
                if (gas) mixInto(gasRT, gasScratch);
                if (maskCol < 0 || k == maskCol) { if (masks) mixInto(maskRT, gasScratch); }
                else if (others) mixInto(otherMaskRT, gasScratch);
            }
            if (gas) mixDone(gasRT);
            if (masks) mixDone(maskRT);
            if (others) mixDone(otherMaskRT);
        };
        if (gasOn) layers(true);
        auto cloudOver = [&](RenderTexture2D& rt, float hide, bool feet = false, RenderTexture2D* cover = nullptr) {   // a canvas melted into the clouds that count (all but `hide`), seen through the others; its alpha as it was
            if (!masks) return false;                      // (no cloud on the screen)
            const bool others = maskCol >= 0;
            const unsigned char m = (unsigned char)(255 * (1 - hide));
            BeginTextureMode(rt);
            rlSetBlendFactorsSeparate(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            boxed(maskRT, {m, m, m, m});
            EndBlendMode();
            if (others) {
                rlSetBlendFactorsSeparate(RL_DST_COLOR, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                boxed(otherMaskRT, WHITE);
                EndBlendMode();
            }
            EndTextureMode();
            // Melted, his feet leave the ground's ink line under them in their place, as if they
            // were not there -- not notched by them: the level on its own canvas, times the clouds'
            // cover (in gasScratch), taken out of his canvas's alpha as much as he melts, in a band
            // round his feet only (feetBox; the ground he overlaps elsewhere, a wall: as ever).
            if (feet && feetBox.width > 0) {
                canvas(groundRT, [&] { drawLevel(g, view); });
                BeginTextureMode(gasScratch);
                rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM);
                boxedIn(groundRT, WHITE, feetBox);
                EndBlendMode();
                rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);   // (its alpha times the cover)
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                boxedIn(maskRT, WHITE, feetBox);
                EndBlendMode();
                EndTextureMode();
                BeginTextureMode(rt);
                rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                boxedIn(gasScratch, {255, 255, 255, m}, feetBox);
                EndBlendMode();
                EndTextureMode();
            }
            // The engraving: where the clouds cover him, his colours kept as melted, not cut into lines
            // -- the clouds' cover, times his shape, on a canvas of its own (`cover`), which takes the
            // engraving's alpha out under him as he is laid into the world (meltedIn).
            if (cover && g.engrave) {
                canvas(*cover, [] {});
                BeginTextureMode(*cover);
                rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM);
                boxed(maskRT, {m, m, m, m});
                EndBlendMode();
                rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);   // (times his shape)
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                boxed(rt, WHITE);
                EndBlendMode();
                EndTextureMode();
            }
            gameViewport();
            return cover && g.engrave;
        };
        const MouthPose leonMouth = mouthPose(g, leonFrame(g, leonC, leonAngle));
        const Vector2 mouthU = leonMouth.u;
        bool leonCovered = false, hatCovered = false;      // (the engraving: a cover canvas made for him, his hat)
        if (hidden) {
            canvas(leonRT, [&] {
                drawTongue(g, leonMouth.lipsW, leonC);
                gNoHat = true;
                drawLeon(g, leonC, leonAngle, mouthU);
                gNoHat = false;
            });
            leonCovered = cloudOver(leonRT, leonHide, true, &leonCoverRT);
        }
        if (hatHidden) {
            canvas(hatRT, [&] {
                if (!g.hasHat) { waitingHat(); looseHat(); return; }
                gEyesOnly = gHatOnly = true;
                drawLeon(g, leonC, leonAngle, mouthU);
                gEyesOnly = gHatOnly = false;
            });
            if (masks && hatCol != maskCol) { maskCol = hatCol; layers(false); }   // (a colour of its own: its masks apart)
            hatCovered = cloudOver(hatRT, hatHide, false, &hatCoverRT);
        }
        const float leonFade = g.leonFade();              // into a dark light: fading out, whole, on his own canvas (then gone)
        if (leonFade > 0 && leonFade < 1 && !hidden)
            canvas(leonRT, [&] {
                drawTongue(g, leonMouth.lipsW, leonC);
                drawLeon(g, leonC, leonAngle, mouthU);
            });
        // Anna dashing: blurred along her way -- on her own canvas, then summed on another, sixteen times
        // along anna_dash_blur_length behind her, each copy a sixteenth of her (premultiplied: where all
        // of them are, as solid as she is) -- laid into the world in her place.
        const float dashBlur = g.anna && !hidden && leonFade >= 1 ? g.dashBlur() : 0;
        if (dashBlur > 0) {
            canvas(leonRT, [&] {
                drawTongue(g, leonMouth.lipsW, leonC);
                drawLeon(g, leonC, leonAngle, mouthU);
            });
            canvas(blurRT, [] {});
            const int W = sw, H = sh, K = 16;
            Vector2 a = GetWorldToScreen2D(leonC, cw);
            Vector2 b = GetWorldToScreen2D(Vector2Subtract(leonC, Vector2Scale(g.toWorld({g.dashDir, 0}), g.t.dashBlurLen * dashBlur)), cw);
            BeginTextureMode(blurRT);
            rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM);
            for (int i = 0; i < K; i++) {
                unsigned char sh8 = (unsigned char)(255 * (i + 1) / K - 255 * i / K);   // (the shares adding up to 255)
                Vector2 o = Vector2Scale(Vector2Subtract(b, a), (float)i / (K - 1));
                DrawTextureRec(leonRT.texture, {0, 0, (float)W, -(float)H}, o, {sh8, sh8, sh8, sh8});
            }
            EndBlendMode();
            EndTextureMode();
            gameViewport();
        }
        auto drawWorld = [&](int W, int H) {
            auto layIn = [&](RenderTexture2D& rt, Color tint = WHITE) {   // a canvas drawn before (melted where clouds cover it): into the world
                EndMode2D();
                DrawTextureRec(rt.texture, {0, 0, (float)W, -(float)H}, {0, 0}, tint);
                BeginMode2D(cw);
            };
            auto meltedIn = [&](RenderTexture2D& rt, RenderTexture2D& cover, bool covered) {   // a melted canvas into the world --
                layIn(rt);                                 // engraved, as the rest of him, but where the clouds cover him: as drawn (melted)
                if (!covered) return;
                rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                layIn(cover);
                EndBlendMode();
            };
            auto hatAway = [&] {                           // the hat off his head, wherever it is
                if (hatHidden && !g.hasHat) meltedIn(hatRT, hatCoverRT, hatCovered);
                else { waitingHat(); looseHat(); }
            };
            if (g.engrave) {                               // the engraving: an engraver's sky, clouds only
                const bool drawn = g.t.engraveDrawings && loadCloudDrawings();   // (the workshop's clouds: the sky's lines alone, then them)
                if (skyOk) {                               // cut by its own shader, written as is (alpha 0: left alone)
                    static SkyLayout sky;
                    layoutClouds(W, H, cloudScroll, sky);
                    if (drawn) sky.clouds = sky.shapes = 0;
                    float scr[2] = {(float)W, (float)H}, light[2] = {-0.6f, -0.8f};
                    float sp = g.t.engraveSkySpacing * fmaxf(1.0f, H / 720.0f);
                    float frames[6];
                    for (int k = 0; k < 3; k++) { frames[2 * k] = -cloudScroll[k].x; frames[2 * k + 1] = -cloudScroll[k].y; }
                    SetShaderValue(skySh, skyLoc[0], scr, SHADER_UNIFORM_VEC2);
                    SetShaderValue(skySh, skyLoc[1], &sp, SHADER_UNIFORM_FLOAT);
                    SetShaderValue(skySh, skyLoc[2], light, SHADER_UNIFORM_VEC2);
                    SetShaderValue(skySh, skyLoc[3], &sky.clouds, SHADER_UNIFORM_INT);
                    if (sky.clouds > 0) {
#ifdef __EMSCRIPTEN__
                        Web::uploadSky(sky.cl, 16 * sky.clouds * sizeof(float), sky.sh, 8 * sky.shapes * sizeof(float));
#else
                        SetShaderValueV(skySh, skyLoc[4], sky.cl, SHADER_UNIFORM_VEC4, 4 * sky.clouds);
                        SetShaderValueV(skySh, skyLoc[5], sky.sh, SHADER_UNIFORM_VEC4, 2 * sky.shapes);
#endif
                    }
                    SetShaderValueV(skySh, skyLoc[6], frames, SHADER_UNIFORM_VEC2, 3);
                    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
                    BeginBlendMode(BLEND_CUSTOM);
                    BeginShaderMode(skySh);
                    DrawTexturePro(whiteTex, {0, 0, 1, 1}, {0, 0, (float)W, (float)H}, {0, 0}, 0, WHITE);
                    EndShaderMode();
                    EndBlendMode();
                } else {
                    engraveSky(W, H);
                    if (!drawn) engraveClouds(W, H, cloudScroll);
                }
                if (drawn) drawCloudDrawings(W, H, cloudScroll);
            } else {
                DrawRectangle(0, 0, W, H, kPaper);        // (the game's area only: the window's bands stay black)
            }
            // In the engraving, the fruits, the flag, the key, the doors and the golden flies keep their colours: drawn
            // with a blend that also clears the alpha under them, which the engraving shader reads
            // as "leave this pixel as drawn" (whatever covers them later puts the alpha back).
            auto inColour = [&](auto draw) {
                if (!g.engrave) { draw(); return; }
                rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA,
                                          RL_FUNC_ADD, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                draw();
                EndBlendMode();
            };
            BeginMode2D(cw);
            drawBackground(g, view);
            inColour([&] {
                for (size_t i = 0; i < g.level.pines.size(); i++)      // halos: behind the ground
                    if (g.pineEaten[i] && !g.pineAway(i) && g.level.pineapple(i) && g.pineShow(i) > 0 && inView(g.level.pines[i], 2 * kTile))
                        drawPineHalo(g.pinePos(i), 0.5f - 0.5f * cosf(2 * PI * g.time), g.pineShow(i));
            });
            if (gasOn) {                                  // the clouds (the ground cuts them off), premultiplied -- in the engraving, clearing the alpha as inColour does
                EndMode2D();
                rlSetBlendFactorsSeparate(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, g.engrave ? RL_ZERO : RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                boxed(gasRT, WHITE);
                EndBlendMode();
                BeginMode2D(cw);
            }
            drawLevel(g, view);
            drawLamps(g);
            inColour([&] { drawPoles(g); });
            for (size_t i = 0; i < g.level.stars.size(); i++)
                if (!g.taken[i] && inView(g.level.stars[i], 24)) drawStar(g.starPos(i), 14, sinf(g.time * 1.5f + i) * 0.25f, kStar, 2.4f);
            drawParticles(g, false);
            drawFlies(g, worldAngle, g.engrave ? 1 : 0);
            if (g.engrave) inColour([&] { drawFlies(g, worldAngle, 2); });
            MouthPose mp = mouthPose(g, leonFrame(g, leonC, leonAngle));
            inColour([&] {
                drawDoors(g, worldAngle);
                if (g.level.hasExit && !g.completed && inView(g.keyPos(), kTile)) drawKey(g.keyPos(), worldAngle, 1, g.t.keyCol[0]);
                for (size_t i = 0; i < g.level.blueKeys.size(); i++)
                    if (!g.blueKeyTaken[i] && inView(g.level.blueKeys[i], kTile)) drawKey(g.keyBob(g.level.blueKeys[i]), worldAngle, 1, g.t.keyCol[1]);
                for (size_t i = 0; i < g.level.greenKeys.size(); i++)
                    if (!g.greenKeyTaken[i] && inView(g.level.greenKeys[i], kTile)) drawKey(g.keyBob(g.level.greenKeys[i]), worldAngle, 1, g.t.keyCol[2]);
                for (size_t i = 0; i < g.level.purpleKeys.size(); i++)
                    if (!g.purpleKeyTaken[i] && inView(g.level.purpleKeys[i], kTile)) drawKey(g.keyBob(g.level.purpleKeys[i]), worldAngle, 1, g.t.keyCol[3]);
                for (size_t i = 0; i < g.level.goldKeys.size(); i++)
                    if (!g.goldKeyTaken[i] && inView(g.level.goldKeys[i], 2 * kKeyLen)) {
                        Vector2 kb = g.keyBob(g.level.goldKeys[i]);
                        drawKeyGlow(kb, 1, g.time, g.t.goldKeyGlow);
                        drawKey(kb, worldAngle, 1, g.t.keyCol[4]);
                    }
                for (int f = 0; f < flCount; f++) {
                    const Level::Flag& fl = g.level.flags[f];
                    if (fl.has && !g.goldenRun && inView(fl.foot[g.grav], 4 * kTile)) drawFlag(g, fl.foot[g.grav], fl.side[g.grav], fl.pole[g.grav], g.flagT[f], kFlagCols[f]);
                }
                const Level::Flag& bf = g.level.blackFlag;       // (the black one -- a rainbow flag: in a golden run too)
                if (bf.has && inView(bf.foot[g.grav], 4 * kTile)) drawFlag(g, bf.foot[g.grav], bf.side[g.grav], bf.pole[g.grav], g.blackFlagT, BLANK, true);
            });
            inColour([&] {
                drawFruits(g, worldAngle);
                drawCherries(g, worldAngle);
            });
            if (!g.hatOnTongue) hatAway();
            drawCrates(g);
            inColour([&] {
                for (size_t i = 0; i < g.level.pines.size(); i++)      // they stay: grey, or lit
                    if (!g.pineAway(i) && g.level.pineapple(i) && g.pineShow(i) > 0 && inView(g.level.pines[i], kTile))
                        drawCheckpoint(g.pinePos(i), worldAngle, g.pineEaten[i], g.time, g.pineShow(i));
            });
            drawMonsters(g, false);                    // the Monstermunchs: behind their faces...
            if (g.eatenBy >= 0) drawPieces(g, 1);       // ...the blood cubes of its munches, out of its middle...
            drawMouthDrops(g);                         // ...and the saliva in their mouths...
            drawMonsters(g, true);                     // ...then the faces
            drawAnna(g);                               // Anna, on the tower's top (the ending) -- engraved, hand-coloured
            if (duo) drawTitleDuo(g, titleClock);
            if (!g.dead && !g.swallowed() && leonFade > 0 && !duo) {
                if (bellyCol >= 0)                     // a colour fly in his belly: he glows its colour
                    inColour([&] {
                        Color glow = g.t.colorOf[bellyCol];
                        DrawCircleGradient(leonC, 46 + 4 * sinf(g.time * 3), fade(glow, 0.45f * g.leonShow * leonFade), fade(glow, 0));
                    });
                if (leonFade < 1 && !hidden) {         // into a dark light: fading out, his tongue and hat with him
                    layIn(leonRT, fade(WHITE, leonFade));
                } else if (hidden) {                   // in a cloud: melted into it (engraved: where it covers him, its colour, as drawn)...
                    meltedIn(leonRT, leonCoverRT, leonCovered);
                    if (g.hasHat) meltedIn(hatRT, hatCoverRT, hatCovered);   // ...his hat by its own measure...
                    gEyesOnly = true;                  // ...his eyes whole
                    drawLeon(g, leonC, leonAngle, mp.u);
                    gEyesOnly = false;
                } else if (dashBlur > 0) {             // Anna dashing: her blurred copies, premultiplied
                    rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD);
                    BeginBlendMode(BLEND_CUSTOM);
                    layIn(blurRT);
                    EndBlendMode();
                } else {
                    drawTongue(g, mp.lipsW, leonC);    // (hidden: on his canvas)
                    drawLeon(g, leonC, leonAngle, mp.u);
                }
                if (g.hatOnTongue) hatAway();          // (on the tip, over his head as it goes on)
            }
            inColour([&] { drawHearts(g); });          // the ending's hearts, in front of the two
            drawParticles(g, true);
            EndMode2D();
            // Dark matter: the world fades to black first, Leon's pieces still bouncing on it in
            // full view; then the pieces fade too. Eaten: the end fading out. Coming back: a quick
            // fade in -- played out first, should he die again meanwhile. (A golden run's way in: black,
            // then back. Out of the map: the world fades to black, as for dark matter.)
            float black = fmaxf(fmaxf(g.fadeIn / g.t.deathFadeIn, g.eatenFade()), g.goldenBlack());
            if (g.dead) black = fmaxf(black, clampf(g.deadT / g.t.deathWorldFade, 0, 1));
            if (g.lostT >= 0) black = fmaxf(black, clampf(g.lostT / g.t.deathWorldFade, 0, 1));
            if (black > 0) DrawRectangle(0, 0, W, H, fade(kBlack, black));
            if (g.dead) {
                float pa = 1 - clampf((g.deadT - g.t.deathPiecesWait) / g.t.deathPiecesFade, 0, 1);
                BeginMode2D(cw);
                drawPieces(g, pa);
                EndMode2D();
            }
        };
        if (g.engrave) {                                   // the engraving: drawn whole, then cut into lines
            if (engraveRT.id == 0 || engraveRT.texture.width != sw || engraveRT.texture.height != sh) {
                if (engraveRT.id != 0) UnloadRenderTexture(engraveRT);
                engraveRT = loadCanvas(sw, sh);
                SetTextureFilter(engraveRT.texture, TEXTURE_FILTER_BILINEAR);
            }
            BeginTextureMode(engraveRT);
            drawWorld(sw, sh);
            EndTextureMode();
            gameViewport();
            // What keeps its colours under the lines, as an old print hand-coloured: Leon or Anna -- his
            // tongue, his hat, on him or off --, the one waiting on the tower (the two, on the title),
            // the Monstermunchs: drawn again, alone, on their own canvas, which the shader reads. (Melted
            // into a cloud, he is the cloud's colour, as drawn, there: the wash goes only where he is engraved.)
            canvas(tintRT, [&] {
                drawMonsters(g, false);
                drawMouthDrops(g);
                drawMonsters(g, true);
                drawAnna(g);
                if (duo) drawTitleDuo(g, titleClock);
                waitingHat(); looseHat();
                if (!g.dead && !g.swallowed() && leonFade > 0 && !duo) {
                    drawTongue(g, leonMouth.lipsW, leonC);
                    drawLeon(g, leonC, leonAngle, mouthU);
                }
            });
            if (engraveOk) {
                float scr[2] = {(float)sw, (float)sh};
                float pix[2] = {GetScreenWidth() / (float)std::max(1, GetRenderWidth()), GetScreenHeight() / (float)std::max(1, GetRenderHeight())};   // (the window's points to its pixels)
                float tgt[2] = {cw.target.x, cw.target.y}, off[2] = {cw.offset.x, cw.offset.y};
                float rot = cw.rotation * DEG2RAD, zm = cw.zoom, sp = g.t.engraveSpacing * fmaxf(1.0f, sh / 720.0f);
                SetShaderValue(engraveSh, engraveLoc[0], scr, SHADER_UNIFORM_VEC2);
                SetShaderValue(engraveSh, engraveLoc[1], pix, SHADER_UNIFORM_VEC2);
                SetShaderValue(engraveSh, engraveLoc[2], tgt, SHADER_UNIFORM_VEC2);
                SetShaderValue(engraveSh, engraveLoc[3], off, SHADER_UNIFORM_VEC2);
                SetShaderValue(engraveSh, engraveLoc[4], &rot, SHADER_UNIFORM_FLOAT);
                SetShaderValue(engraveSh, engraveLoc[5], &zm, SHADER_UNIFORM_FLOAT);
                SetShaderValue(engraveSh, engraveLoc[6], &sp, SHADER_UNIFORM_FLOAT);
                BeginShaderMode(engraveSh);
                SetShaderValueTexture(engraveSh, engraveLoc[7], tintRT.texture);
            }
            else {                                         // (no shader: the picture as it is, its alpha marks ignored)
                rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
                BeginBlendMode(BLEND_CUSTOM);
            }
            DrawTexturePro(engraveRT.texture, {0, 0, (float)sw, -(float)sh}, {0, 0, (float)sw, (float)sh}, {0, 0}, 0, WHITE);
            if (engraveOk) EndShaderMode();
            else EndBlendMode();
        } else {
            drawWorld(sw, sh);
        }
        // His eyes over the dark (dark air, a dark light) -- fading out with him into a dark light: then
        // on a canvas of their own, laid over at his fade.
        auto eyesOver = [&] {
            float a = g.leonFade();
            if (a <= 0 || g.dead || g.swallowed() || duo) return;
            if (a < 1) {
                if (eyesRT.id == 0 || eyesRT.texture.width != sw || eyesRT.texture.height != sh) {
                    if (eyesRT.id != 0) UnloadRenderTexture(eyesRT);
                    eyesRT = loadCanvas(sw, sh);
                }
                BeginTextureMode(eyesRT);
                ClearBackground(BLANK);
            }
            gEyesOnly = gEyesInDark = true;
            BeginMode2D(c);
            drawLeon(g, leonC, leonAngle, mouthPose(g, leonFrame(g, leonC, leonAngle)).u);
            EndMode2D();
            gEyesOnly = gEyesInDark = false;
            if (a < 1) {
                EndTextureMode();
                gameViewport();
                DrawTexturePro(eyesRT.texture, {0, 0, (float)sw, -(float)sh}, {0, 0, (float)sw, (float)sh}, {0, 0}, 0, fade(WHITE, a));
            }
        };
        // Dark lights ('@'): a light the other way round -- round each, dark_light_radius tiles, the
        // black thickens to dark_light_depth at its heart (wavering as a lamp's light, lamp_flicker_*),
        // over the world in every look; in it, his eyes still show, as in dark air. (And the golden
        // run's, over the start: fading in, then out -- not one of the level's, it takes no one.)
        {
            bool inPool = false;
            float reach = g.t.darkLightRadius * kTile * 1.1f;
            const size_t nl = g.level.darkLights.size();
            for (size_t k = 0; k <= nl; k++) {
                float a = k < nl ? 1 : g.goldenLightA();
                if (a <= 0) continue;
                Vector2 at = k < nl ? g.level.darkLights[k] : g.goldenLightAt();
                if (reach <= 0 || !inView(at, reach)) continue;
                float x = g.time * g.t.lampFlickerFreq + k * 53.1f + 17.0f;
                float r = g.t.darkLightRadius * kTile * (1 + g.t.lampFlicker * (0.75f * perlin1(x) + 0.25f * perlin1(2.7f * x + 11.0f)));
                Vector2 s = GetWorldToScreen2D(at, c);
                DrawCircleGradient(s, r * c.zoom, fade(kBlack, a * g.t.darkLightDepth), fade(kBlack, 0));
                DrawCircleGradient(s, r * c.zoom * 0.5f, fade(kBlack, a * 0.6f * g.t.darkLightDepth), fade(kBlack, 0));   // blacker near its heart
                if (Vector2Distance(leonC, at) < r) inPool = true;
            }
            if (inPool && g.dark <= 0.002f) eyesOver();   // (in dark air, his eyes come after it anyway)
        }
        // Dark air: black over the world -- effects, every look -- and over it, his eyes.
        // A lamp's fly is a candle in it: round it, lamp_light_radius tiles, the black thins out
        // (flickering: Perlin noise, two octaves -- lamp_flicker_*), and a warm glow is added.
        if (g.dark > 0.002f) {
            std::vector<Vector2> lights;
            std::vector<float> strength;                  // a fly's whole; a lamp post's own, dimmed (lamp_post_light)
            std::vector<size_t> key;                      // (each light's place in the whole list: its flicker's)
            float reach = g.t.lampLight * kTile * 1.1f;   // (lights whose glow cannot reach the screen: left out)
            size_t n = 0;
            for (const Fly& f : g.flies)
                if (f.lamp >= 0) {
                    // (in his belly: it glows through him, lamp_belly_light of its power, flickering --
                    // Perlin noise, two octaves, lamp_belly_flicker_*)
                    Vector2 at = f.inMouth ? leonC : f.p;
                    float s = 1;
                    if (f.inMouth) {
                        float x = g.time * g.t.lampBellyFlickerFreq + n * 37.3f + 5.1f;
                        s = clampf(g.t.lampBellyLight * (1 + g.t.lampBellyFlicker * (0.75f * perlin1(x) + 0.25f * perlin1(2.7f * x + 11.0f))), 0, 1);
                    }
                    if (inView(at, reach)) { lights.push_back(GetWorldToScreen2D(at, c)); strength.push_back(s); key.push_back(n); }
                    n++;
                }
            if (g.god && !g.dead && inView(leonC, reach)) {      // GOD: three golden flies' belly light, his own -- lights add up
                const size_t own = 1000;                       // (its flicker its own: the others' as without it)
                float x = g.time * g.t.lampBellyFlickerFreq + own * 37.3f + 5.1f;
                float s = clampf(g.t.lampBellyLight * (1 + g.t.lampBellyFlicker * (0.75f * perlin1(x) + 0.25f * perlin1(2.7f * x + 11.0f))), 0, 1);
                for (int j = 0; j < 3; j++) { lights.push_back(GetWorldToScreen2D(leonC, c)); strength.push_back(s); key.push_back(own); }
            }
            if (g.t.lampPostLight > 0)
                for (size_t k = 0; k < g.level.lamps.size(); k++, n++) {
                    if (!inView(g.lampTop(k), reach)) continue;
                    lights.push_back(GetWorldToScreen2D(g.lampTop(k), c));
                    strength.push_back(g.t.lampPostLight * (g.lampPostOn(k) ? 1 : g.t.lampBlackLevel));   // (a blackout now and then)
                    key.push_back(n);
                }
            if (lights.empty()) DrawRectangle(0, 0, sw, sh, fade(kBlack, g.dark));
            else {
                if (darkRT.id == 0 || darkRT.texture.width != sw || darkRT.texture.height != sh) {
                    if (darkRT.id != 0) UnloadRenderTexture(darkRT);
                    darkRT = loadCanvas(sw, sh);
                }
                auto radius = [&](size_t k) {
                    float x = g.time * g.t.lampFlickerFreq + key[k] * 37.3f;
                    float fl = 1 + g.t.lampFlicker * (0.75f * perlin1(x) + 0.25f * perlin1(2.7f * x + 11.0f));
                    return g.t.lampLight * kTile * c.zoom * fl;
                };
                BeginTextureMode(darkRT);
                ClearBackground(fade(kBlack, g.dark));          // (cleared to it: its opacity exactly g.dark, no blend)
                // Light takes the black away -- and lights add up: each one subtracts its own light
                // from the black's opacity (the colour left as it is), so two halos over one another
                // light it as much as both together.
                rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ONE, RL_ONE, RL_FUNC_ADD, RL_FUNC_REVERSE_SUBTRACT);
                BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                for (size_t k = 0; k < lights.size(); k++) {
                    float s = strength[k] * g.dark;
                    DrawCircleGradient(lights[k], radius(k), fade(WHITE, s), fade(WHITE, 0));
                    DrawCircleGradient(lights[k], radius(k) * 0.5f, fade(WHITE, 0.6f * s), fade(WHITE, 0));   // brighter near the flame
                }
                EndBlendMode();
                EndTextureMode();
                gameViewport();
                DrawTexturePro(darkRT.texture, {0, 0, (float)sw, -(float)sh}, {0, 0, (float)sw, (float)sh}, {0, 0}, 0, WHITE);
                BeginBlendMode(BLEND_ADDITIVE);
                for (size_t k = 0; k < lights.size(); k++)
                    DrawCircleGradient(lights[k], radius(k), {255, 196, 96, (unsigned char)(70 * g.dark * strength[k])}, {255, 196, 96, 0});
                EndBlendMode();
            }
            if (!g.dead && !g.swallowed()) {
                eyesOver();
            } else if (g.t.darkPieces > 0) {              // dead: his pieces show a little (dark_pieces_light), fading as ever
                float pa = 1 - clampf((g.deadT - g.t.deathPiecesWait) / g.t.deathPiecesFade, 0, 1);
                BeginMode2D(c);
                drawPieces(g, pa * g.t.darkPieces * g.dark);
                EndMode2D();
            }
        }
        if (g.t.zoneShow == 1 || (g.t.zoneShow == 2 && g.debugMode())) {   // the camera zone (camera_zone_show)
            float k = sqrtf(g.t.camArea);
            float w = gameW() * k, h = gameH() * k;
            dottedRect({(gameW() - w) / 2, (gameH() - h) / 2, w, h}, fade(kInk, 0.6f));
            DrawCircleV(c.offset, 3, fade(kInk, 0.6f));
        }
        if (g.arrowA > 0) drawGuideArrow(g, c, leonC);
        drawDoorName(g, c, leonC);
        if (!noHud) drawHud(g, myScores(), entry, !titleShown);
        if (!titleShown) drawCredits(g);                  // the ending's credits, rolling up
        if (restartIn > 0) {                              // over Leon: its bottom 1.5 hat heights above his head
            Vector2 up = Vector2Rotate({0, -1}, leonAngle * DEG2RAD);
            float rise = kLeonH / 2 + 2 + 1.5f * (kHatSeat + kHatCrown);
            drawCountdown(restartIn, GetWorldToScreen2D(Vector2Add(leonC, Vector2Scale(up, rise)), c));
        }
        if (titleShown) {
            // Ground poking down into a box of the screen (the title): a column of it -- solid, or a
            // thin platform -- ending above the box's middle. How far down the box must go to clear
            // them all by half the height of their last tile (a thin platform: its strip).
            auto groundAbove = [&](Rectangle r) {
                const Level& L = g.level;
                Vector2 a = GetScreenToWorld2D({r.x, r.y}, c), b = GetScreenToWorld2D({r.x + r.width, r.y + r.height}, c);
                int x0 = (int)floorf(fminf(a.x, b.x) / kTile), x1 = (int)floorf(fmaxf(a.x, b.x) / kTile);
                int y0 = (int)floorf(fminf(a.y, b.y) / kTile), y1 = (int)floorf(fmaxf(a.y, b.y) / kTile);
                auto ground = [&](int x, int y) { return L.block(x, y) || L.oneway(x, y); };
                float need = 0, mid = r.y + r.height / 2;
                for (int x = x0; x <= x1; x++)
                    for (int y = y0; y <= y1; y++) {
                        if (!ground(x, y)) continue;
                        int top = y, bot = y;
                        while (ground(x, top - 1)) top--;
                        while (ground(x, bot + 1)) bot++;
                        float last = L.oneway(x, bot) ? 12.0f : kTile;   // (a thin platform's strip: drawLevel)
                        float sTop = GetWorldToScreen2D({x * kTile, (float)top * kTile}, c).y;
                        float sLastTop = GetWorldToScreen2D({x * kTile, (float)bot * kTile}, c).y;
                        float sBot = GetWorldToScreen2D({x * kTile, bot * kTile + last}, c).y;
                        if (sTop < r.y + r.height && sBot > r.y && sBot < mid)
                            need = fmaxf(need, sBot + (sBot - sLastTop) / 2 - r.y);
                        y = bot;
                    }
                return need;
            };
            float feet = GetWorldToScreen2D(g.toWorld(leonP), c).y;
            float body = GetWorldToScreen2D(g.toWorld({leonP.x, leonP.y - kLeonDrawH}), c).y;
            float top = GetWorldToScreen2D(g.toWorld({leonP.x, leonP.y - kLeonDrawH - (g.hasHat ? kHatSeat + kHatCrown : 0)}), c).y;
            if (titleLook == 2) top = GetWorldToScreen2D(g.toWorld({leonP.x, leonP.y - kLeonDrawH - kHatSeat - kHatCrown}), c).y;   // (the two: his hat on -- her cone's tip, narrow, under the line)
            else if (g.anna && g.hasHat) top = GetWorldToScreen2D(g.toWorld({leonP.x, leonP.y - kLeonH - kAnnaSeat - kAnnaCone}), c).y;
            float line = GetWorldToScreen2D(g.toWorld({leonP.x, leonP.y + 1.5f}), c).y;   // (the ground's line: 3 px, on its top)
            const float T1 = g.t.titleIntro, T2 = g.t.titleIntroRest, S = fmaxf(0, T1 - g.t.titleScoresEarly);
            drawTitle(titleClock, g.t, scores, {line, top, (feet - body) / 2, groundAbove},
                      T1 > 0 ? clampf(titleIntro / T1, 0, 1) : 1,
                      T2 > 0 ? clampf((titleIntro - T1) / T2, 0, 1) : (titleIntro >= T1 ? 1.0f : 0.0f),
                      T2 > 0 ? clampf((titleIntro - S) / T2, 0, 1) : (titleIntro >= S ? 1.0f : 0.0f),   // (the highscores: title_scores_fade_in_early before the rest)
                      goldenSeen[0] || !scores.part[5].empty(), titleLook, &annaScores, goldenSeen[1] || !annaScores.part[5].empty());
        }
        if (titleFadeIn > 0) {                            // back on the title after the time ran out: fading in
            DrawRectangle(0, 0, gameW(), gameH(), fade(kBlack, titleFadeIn / (g.t.titleStartFade / 2)));
            titleFadeIn -= frame;
        }
        if (starting) {                                   // the fade out of the title, then in to the game
            float half = g.t.titleStartFade / 2, u = startFade < half ? startFade / half : (g.t.titleStartFade - startFade) / half;
            DrawRectangle(0, 0, gameW(), gameH(), fade(kBlack, clampf(u, 0, 1)));
        }
        if (switchT >= 0)                                 // the title's debug switch: out to black, in again
            DrawRectangle(0, 0, gameW(), gameH(), fade(kBlack, clampf(1 - fabsf(2 * switchT / g.t.titleSwitchFade - 1), 0, 1)));
        if (!titleShown) drawEnd(g);                      // the ending's end: black, and back to the title
        if (ask) drawAsk(ask == 1 ? "Exit Game?" : "Back to Title?", askYes, askA);
        if (!frameLog.empty()) { frameMs.push_back((float)((frameT0 - logT0) * 1000)); frameMs.push_back((float)((GetTime() - frameT0) * 1000)); }
        // Shift+B was pressed: picture and state now -- this frame, before the box covers it.
        if (reportWanted) {
            captureReport();
            reportWanted = false;
            reportOpen = true;
            reportText.clear();
            reportCursor = 0; reportWantX = -1; reportScroll = 0;
            reportClock = 0;
            while (GetCharPressed() > 0) {}                               // not the 'B' itself
        }
        bool smokeDone = smokeName && smokeClock >= smokeTime;
        if (smokeDone && !testReports.empty()) {                           // tests: write reports now
            for (auto& tr : testReports) {
                captureReport();
                std::string txt = saveReport(tr);
                if (!txt.empty()) TraceLog(LOG_INFO, "REPORT: test report %s", txt.c_str());
            }
            UnloadImage(reportShot);
            reportShot = Image{};
        }
        if (reportOpen) drawReportBox(reportText, reportCursor, reportClock, reportScroll);
        if (reportToast > 0) {
            reportToast -= frame;
            float ui = fmaxf(1.0f, gameH() / 720.0f);
            int fs = (int)(18 * ui);
            DrawText(reportToastText.c_str(), (gameW() - MeasureText(reportToastText.c_str(), fs)) / 2,
                     (int)(gameH() * 0.9f), fs, fade(kInk, clampf(reportToast, 0, 1)));
        }
        // Read the frame back BEFORE the swap: the back buffer is undefined afterwards.
        if (shot) saveScreen(screenshotPath("shot", true));
        if (smokeDone) saveScreen(screenshotPath(smokeName, false));
        EndDrawing();
#ifdef __EMSCRIPTEN__
        Web::inputMode(reportOpen, entry.open && !reportOpen, !reportOpen && !entry.open && !ask, g.t.fullscreenWait);
#endif
        if (smokeDone) break;
    }
    closeEntry();                                         // (quitting with a name waiting: kept as it is)
    if (gBgMesh.peak > 0) TraceLog(LOG_INFO, "BACKGROUND: the pattern buffer held %.2f MiB at most (%lld bytes)", gBgMesh.peak / 1048576.0, (long long)gBgMesh.peak);
    if (!frameLog.empty())
        if (FILE* f = fopen(frameLog.c_str(), "w")) {
            fprintf(f, "frame,start_ms,work_ms\n");
            for (size_t k = 0; k + 1 < frameMs.size(); k += 2) fprintf(f, "%zu,%.3f,%.3f\n", k / 2, frameMs[k], frameMs[k + 1]);
            fclose(f);
        }
    if (reportShot.data) UnloadImage(reportShot);
    Audio::shutdown();
#ifdef __EMSCRIPTEN__
    Web::releaseSky();
#endif
    CloseWindow();
#ifdef __EMSCRIPTEN__
    Web::finish();
#endif
    return 0;
}
