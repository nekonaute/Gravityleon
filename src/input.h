#pragma once
#include <string>

// Gamepad reading, after Hydropostal's input module: EVERY connected pad is polled, because
// on this machine the controller does not always land at slot 0, and SDL mappings are loaded
// first, because GLFW leaves an unmapped pad enumerated but frozen.
namespace Input {
void  init(const std::string& mappingsPath, float deadzone);
void  setDeadzone(float dz);
// Once a frame, before anything is read: takes in the Xbox pad on its cable, which GLFW cannot
// read (padcheck.h) -- it then counts as one more pad everywhere below.
void  poll();
bool  cablePad();          // that pad has sent something
float stickX();            // left stick X, past the dead zone, rescaled to -1..1
float stickY();            // left stick Y, down positive
float rightX();            // right stick X (looking left/right)
float rightY();            // right stick Y, down positive (looking up/down)
// Left stick as a direction: dead zone on the length, not per axis, so diagonals keep
// their true angle. Length 0..1 past the dead zone; {0,0} inside it.
struct Stick { float x, y; };
Stick stick2();
bool  padDown(int button);
// Triggers, asked two ways: as a button, or as an axis past halfway (it rests at -1 on some
// pads, at 0 on others -- both read as 'not pulled').
bool  leftTrigger();
bool  rightTrigger();
bool  padPressed(int button);
// Anything on any pad being touched: a button, a stick out of its dead zone, a trigger pulled.
bool  anyActive();
}
