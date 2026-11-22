#include "platform.h"
#include <emscripten.h>
#include <GLES3/gl3.h>
#include <cstdlib>

// Only this translation unit needs the browser bridge and raw WebGL buffer calls.
namespace {
EM_JS(double, waitFrame, (), {
    return Asyncify.handleSleep(wake => GravityLeon.nextFrame().then(wake));
});
EM_JS(int, isMac, (), { return /Mac|iPhone|iPad/.test(navigator.platform); });
EM_JS(int, isFullscreen, (), { return !!document.fullscreenElement; });
EM_JS(void, setInputMode, (int report, int score, int toggle, double delay), {
    GravityLeon.setInputMode(!!report, !!score, !!toggle, delay);
});
EM_JS(int, readScoreKey, (), { return GravityLeon.nextScoreKey(); });
EM_JS(char*, readPaste, (), {
    const text = GravityLeon.takePaste();
    return text ? stringToNewUTF8(text) : 0;
});
EM_JS(void, save, (), { Module.scoresChanged(); });
EM_JS(void, exportFile, (const char* filename), {
    const path = UTF8ToString(filename);
    GravityLeon.offerFile(path.split('/').pop(), FS.readFile(path));
    // Keep the name reserved for the game's collision check after transferring the pixels.
    FS.truncate(path, 0);
});
EM_JS(void, finishGame, (), {
    Asyncify.handleSleep(wake => {
        Promise.resolve(Module.flushScores()).then(() => {
            GravityLeon.stopped('Gravity Leon has ended.');
            wake();
        });
    });
});
float delta = 0;
GLuint skyBuffer = 0;
int shapeOffset = 0;
}

float Web::nextFrame() { return delta = static_cast<float>(waitFrame()); }
float Web::frameTime() { return delta; }
bool Web::macKeyboard() { return isMac(); }
bool Web::fullscreen() { return isFullscreen(); }
void Web::inputMode(bool report, bool score, bool canToggle, float delay) { setInputMode(report, score, canToggle, delay); }
int Web::nextScoreKey() { return readScoreKey(); }
void Web::scoresChanged() { save(); }
void Web::download(const char* path) { exportFile(path); }
void Web::finish() { finishGame(); }
void Web::graphicsFailed() { EM_ASM(GravityLeon.stopped('Could not start WebGL 2. Check your browser graphics settings.')); }
std::string Web::takePaste() {
    char* raw = readPaste();
    std::string text = raw ? raw : "";
    std::free(raw);
    return text;
}

bool Web::initSky(unsigned int shader, int cloudBytes, int shapeBytes) {
    GLuint block = glGetUniformBlockIndex(shader, "SkyLayoutData");
    if (block == GL_INVALID_INDEX) return false;
    shapeOffset = cloudBytes;
    glGenBuffers(1, &skyBuffer);
    if (!skyBuffer) return false;
    glBindBuffer(GL_UNIFORM_BUFFER, skyBuffer);
    glBufferData(GL_UNIFORM_BUFFER, cloudBytes + shapeBytes, nullptr, GL_DYNAMIC_DRAW);
    glUniformBlockBinding(shader, block, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, skyBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

void Web::uploadSky(const float* clouds, int cloudBytes, const float* shapes, int shapeBytes) {
    glBindBuffer(GL_UNIFORM_BUFFER, skyBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, cloudBytes, clouds);
    glBufferSubData(GL_UNIFORM_BUFFER, shapeOffset, shapeBytes, shapes);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
void Web::releaseSky() { glDeleteBuffers(1, &skyBuffer); }
