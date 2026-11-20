#pragma once
#include <string>
#include <utility>
#include <vector>

// The game controllers the system sees, as (name, GUID): the GUID GLFW gives each one, the key of
// its line in assets/gamecontrollerdb.txt. (raylib reads pads only through GLFW, and GLFW leaves a
// pad with no matching line frozen.) macOS only; elsewhere, none.
std::vector<std::pair<std::string, std::string>> systemPads();

// --pad-check: prints them, with their USB vendor/product/version.
void padCheckSystem();
// --pad-check, under GLFW: listens to the game controllers' raw HID values itself, and prints each
// change -- does the pad send anything at all? (Delivered while the window polls its events.)
void padRawStart();
int padRawStop();                              // how many raw changes came in

// An Xbox pad on its cable (macOS): it sends only Microsoft's own protocol (GIP) -- packets GLFW
// cannot make sense of, so it sits there frozen. Read here, from its HID reports, and decoded as
// the Linux xpad driver does: report 0x20 the buttons, triggers and sticks, report 0x07 the Xbox
// button. (Delivered while the window polls its events.)
struct GipPad {
    bool seen = false;                         // a report came in
    unsigned char face = 0, more = 0;          // Menu 0x04, View 0x08, A 0x10, B 0x20, X 0x40, Y 0x80 --
                                               // D-pad up 0x01, down 0x02, left 0x04, right 0x08,
                                               // LB 0x10, RB 0x20, left stick in 0x40, right stick in 0x80
    bool xbox = false;
    int lt = 0, rt = 0;                        // 0..1023
    int lx = 0, ly = 0, rx = 0, ry = 0;        // -32768..32767, up positive
};
void gipStart();
GipPad gipState();
