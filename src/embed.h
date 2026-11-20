#pragma once
// Baked-in resources for the "make dist" build -- and nothing at all for any other build.
//
// Only `make dist` defines EMBED_ASSETS, and it force-includes this header into every translation
// unit (c++ -include src/embed.h). With EMBED_ASSETS on, fopen() is redirected to embed_fopen(),
// which serves the game's own files (params.txt, levels/, assets/) straight from memory -- they are
// compiled into the binary, obfuscated -- and hands every other path, and every write, to the real
// fopen (so the high scores, the screenshots and the reports still land on disk as always).
//
// The normal build never defines EMBED_ASSETS and never includes this header, so it reads its files
// from disk exactly as before: this file changes nothing outside the dist build.
#ifdef EMBED_ASSETS
#include <cstdio>
extern "C" FILE* embed_fopen(const char* path, const char* mode);
// A baked-in file's bytes and size (null: not one of them) -- the very ones embed_fopen serves, kept
// for the whole run: for what opens its files itself, past fopen (raylib's music streams).
extern "C" const unsigned char* embed_data(const char* path, unsigned long* size);
#define fopen embed_fopen
#endif
