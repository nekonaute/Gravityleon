#include "input.h"
#include "padcheck.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>

namespace {

const int kMaxPads = 8;
float gDead = 0.2f;

// The Xbox pad on its cable, which GLFW cannot read (padcheck.h): its state this frame and last,
// in raylib's terms -- a bit per GamepadButton, the axes as GetGamepadAxisMovement gives them.
struct Gip { bool on = false; unsigned buttons = 0; float ax[6] = {0, 0, 0, 0, -1, -1}; };
Gip gGip, gGipPrev;

Gip fromGip(const GipPad& p) {
    Gip g;
    g.on = p.seen;
    if (!p.seen) return g;
    auto set = [&](bool down, int b) { if (down) g.buttons |= 1u << b; };
    set(p.face & 0x04, GAMEPAD_BUTTON_MIDDLE_RIGHT);    // Menu
    set(p.face & 0x08, GAMEPAD_BUTTON_MIDDLE_LEFT);     // View
    set(p.face & 0x10, GAMEPAD_BUTTON_RIGHT_FACE_DOWN); // A
    set(p.face & 0x20, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);// B
    set(p.face & 0x40, GAMEPAD_BUTTON_RIGHT_FACE_LEFT); // X
    set(p.face & 0x80, GAMEPAD_BUTTON_RIGHT_FACE_UP);   // Y
    set(p.more & 0x01, GAMEPAD_BUTTON_LEFT_FACE_UP);
    set(p.more & 0x02, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    set(p.more & 0x04, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    set(p.more & 0x08, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    set(p.more & 0x10, GAMEPAD_BUTTON_LEFT_TRIGGER_1);  // LB
    set(p.more & 0x20, GAMEPAD_BUTTON_RIGHT_TRIGGER_1); // RB
    set(p.more & 0x40, GAMEPAD_BUTTON_LEFT_THUMB);
    set(p.more & 0x80, GAMEPAD_BUTTON_RIGHT_THUMB);
    set(p.xbox, GAMEPAD_BUTTON_MIDDLE);
    set(p.lt > 512, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
    set(p.rt > 512, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
    g.ax[GAMEPAD_AXIS_LEFT_X] = p.lx / 32767.0f;
    g.ax[GAMEPAD_AXIS_LEFT_Y] = -p.ly / 32767.0f;       // (raylib: down positive)
    g.ax[GAMEPAD_AXIS_RIGHT_X] = p.rx / 32767.0f;
    g.ax[GAMEPAD_AXIS_RIGHT_Y] = -p.ry / 32767.0f;
    g.ax[GAMEPAD_AXIS_LEFT_TRIGGER] = p.lt / 1023.0f * 2 - 1;   // (raylib: -1 at rest)
    g.ax[GAMEPAD_AXIS_RIGHT_TRIGGER] = p.rt / 1023.0f * 2 - 1;
    for (float& a : g.ax) a = fminf(1.0f, fmaxf(-1.0f, a));
    return g;
}

// Whichever pad is pushed furthest wins, so a resting pad never cancels a live one.
// Past the dead zone the value is rescaled to still run 0..1, so the slowest walk is reachable.
float axis(int ax) {
    float best = 0.0f;
    for (int g = 0; g <= kMaxPads; g++) {               // (kMaxPads: the cable Xbox pad)
        float v;
        if (g == kMaxPads) { if (!gGip.on) continue; v = gGip.ax[ax]; }
        else { if (!IsGamepadAvailable(g)) continue; v = GetGamepadAxisMovement(g, ax); }
        if (fabsf(v) <= gDead) continue;
        float out = (v < 0 ? -1.0f : 1.0f) * fminf(1.0f, (fabsf(v) - gDead) / (1.0f - gDead));
        if (fabsf(out) > fabsf(best)) best = out;
    }
    return best;
}

}  // namespace

void Input::init(const std::string& path, float deadzone) {
    setDeadzone(deadzone);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        TraceLog(LOG_WARNING, "INPUT: no gamepad mappings at %s", path.c_str());
        return;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string txt((size_t)(n > 0 ? n : 0), '\0');
    if (n > 0 && fread(&txt[0], 1, (size_t)n, f) != (size_t)n) txt.clear();
    fclose(f);
    if (!txt.empty())
        TraceLog(LOG_INFO, "INPUT: %d gamepad mapping(s) loaded", SetGamepadMappings(txt.c_str()));
    // A pad connected with no line here may still be mapped by GLFW's own list -- or may sit
    // there frozen. Say which pads those are, with the GUID a line would need.
    for (const auto& [name, guid] : systemPads())
        if (txt.find(guid) == std::string::npos)
            TraceLog(LOG_WARNING, "INPUT: pad \"%s\" (GUID %s) has no line in %s -- if it does nothing, add one (./gravityleon --pad-check)",
                     name.c_str(), guid.c_str(), path.c_str());
    gipStart();                                          // an Xbox pad on its cable: read by hand
}

void Input::poll() {
    gGipPrev = gGip;
    gGip = fromGip(gipState());
}

bool Input::cablePad() { return gGip.on; }

void Input::setDeadzone(float dz) { gDead = fminf(0.9f, fmaxf(0.0f, dz)); }

float Input::stickX() { return axis(GAMEPAD_AXIS_LEFT_X); }
float Input::stickY() { return axis(GAMEPAD_AXIS_LEFT_Y); }
float Input::rightX() { return axis(GAMEPAD_AXIS_RIGHT_X); }
float Input::rightY() { return axis(GAMEPAD_AXIS_RIGHT_Y); }

bool Input::padDown(int b) {
    if (gGip.buttons & (1u << b)) return true;
    for (int g = 0; g < kMaxPads; g++)
        if (IsGamepadAvailable(g) && IsGamepadButtonDown(g, b)) return true;
    return false;
}

bool Input::padPressed(int b) {
    if ((gGip.buttons & (1u << b)) && !(gGipPrev.buttons & (1u << b))) return true;
    for (int g = 0; g < kMaxPads; g++)
        if (IsGamepadAvailable(g) && IsGamepadButtonPressed(g, b)) return true;
    return false;
}

Input::Stick Input::stick2() {
    Stick best = {0, 0};
    float bestLen = 0;
    for (int g = 0; g <= kMaxPads; g++) {               // (kMaxPads: the cable Xbox pad)
        float x, y;
        if (g == kMaxPads) { if (!gGip.on) continue; x = gGip.ax[GAMEPAD_AXIS_LEFT_X]; y = gGip.ax[GAMEPAD_AXIS_LEFT_Y]; }
        else {
            if (!IsGamepadAvailable(g)) continue;
            x = GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_X); y = GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_Y);
        }
        float len = sqrtf(x * x + y * y);
        if (len <= gDead || len <= bestLen) continue;
        float k = fminf(1.0f, (len - gDead) / (1.0f - gDead)) / len;
        best = {x * k, y * k};
        bestLen = len;
    }
    return best;
}

static bool triggerHeld(int ax, int button) {
    if (gGip.on && (gGip.ax[ax] > 0.5f || (gGip.buttons & (1u << button)))) return true;
    for (int g = 0; g < kMaxPads; g++) {
        if (!IsGamepadAvailable(g)) continue;
        if (IsGamepadButtonDown(g, button)) return true;
        if (GetGamepadAxisMovement(g, ax) > 0.5f) return true;
    }
    return false;
}
bool Input::anyActive() {
    if (gGip.on) {
        if (gGip.buttons) return true;
        for (int a = GAMEPAD_AXIS_LEFT_X; a <= GAMEPAD_AXIS_RIGHT_Y; a++)
            if (fabsf(gGip.ax[a]) > gDead) return true;
        if (gGip.ax[GAMEPAD_AXIS_LEFT_TRIGGER] > 0.5f || gGip.ax[GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f) return true;
    }
    for (int g = 0; g < kMaxPads; g++) {
        if (!IsGamepadAvailable(g)) continue;
        for (int b = GAMEPAD_BUTTON_LEFT_FACE_UP; b <= GAMEPAD_BUTTON_RIGHT_THUMB; b++)
            if (IsGamepadButtonDown(g, b)) return true;
        for (int a = GAMEPAD_AXIS_LEFT_X; a <= GAMEPAD_AXIS_RIGHT_Y; a++)
            if (fabsf(GetGamepadAxisMovement(g, a)) > gDead) return true;
        if (GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f || GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f)
            return true;
    }
    return false;
}

bool Input::leftTrigger() { return triggerHeld(GAMEPAD_AXIS_LEFT_TRIGGER, GAMEPAD_BUTTON_LEFT_TRIGGER_2); }
bool Input::rightTrigger() { return triggerHeld(GAMEPAD_AXIS_RIGHT_TRIGGER, GAMEPAD_BUTTON_RIGHT_TRIGGER_2); }
