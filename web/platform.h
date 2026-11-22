#pragma once
#include <string>

namespace Web {
float nextFrame();
float frameTime();
bool macKeyboard();
bool fullscreen();
void inputMode(bool report, bool score, bool canToggle, float delay);
int nextScoreKey();
std::string takePaste();
void scoresChanged();
void download(const char* path);
void finish();
void graphicsFailed();
bool initSky(unsigned int shader, int cloudBytes, int shapeBytes);
void uploadSky(const float* clouds, int cloudBytes, const float* shapes, int shapeBytes);
void releaseSky();
}
