CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall

RL_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RL_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
# Linux without a system raylib: the one built from source in ~/.local/opt (raylib 6.0, X11).
RL_LOCAL := $(HOME)/.local/opt/raylib-6.0/src
ifeq ($(RL_LIBS),)
  ifneq ($(wildcard $(RL_LOCAL)/libraylib.a),)
    RL_CFLAGS := -I$(RL_LOCAL)
    RL_LIBS   := $(RL_LOCAL)/libraylib.a -l:libX11.so.6 -lm -lpthread -ldl -lrt
  else
    RL_LIBS := -lraylib
  endif
endif
ifeq ($(shell uname),Darwin)
  RL_LIBS += -framework IOKit -framework CoreFoundation   # --pad-check lists the HID game controllers
endif

SRCS := $(wildcard src/*.cpp)
HDRS := $(wildcard src/*.h)
BUILD_TIME_FLAG = -DGRAVITY_LEON_BUILD_TIME=\"$$(date +%Y-%m-%d_%H:%M)\"

gravityleon: $(SRCS) $(HDRS) Makefile
	$(CXX) $(CXXFLAGS) $(BUILD_TIME_FLAG) $(RL_CFLAGS) $(SRCS) -o $@ $(RL_LIBS)

run: gravityleon
	./gravityleon

clean:
	rm -f gravityleon

# ---- make dist : one self-contained, stripped executable (macOS) --------------------------------
# An extra, optional target that changes nothing above: plain `make` still builds `gravityleon`,
# reading data/params.txt, levels/ and assets/ from disk as always. `make dist` instead bakes those into
# the binary (obfuscated -- tools/embed_gen.py + src/embed.h), statically links raylib and strips the
# symbols, so dist/GravityLeon is a single file that runs on another Mac with nothing beside it.
# It still writes its high scores, screenshots and reports next to itself at runtime, as ever.
DIST_OUT   := dist/GravityLeon
DIST_GEN   := dist/embed_data.cpp
RL_STATIC  := $(shell pkg-config --variable=libdir raylib 2>/dev/null)/libraylib.a
DIST_FRAMEWORKS := -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio \
                   -framework CoreFoundation -framework AudioToolbox -framework OpenGL

dist:
ifneq ($(shell uname),Darwin)
	@echo "make dist is for macOS (single-file build); on this platform use plain 'make'"; exit 1
else
	@test -f "$(RL_STATIC)" || { echo "no static raylib at $(RL_STATIC) -- 'brew install raylib'"; exit 1; }
	@mkdir -p dist
	python3 tools/update_dist_highscores.py
	python3 tools/embed_gen.py $(DIST_GEN) . data/params.txt levels assets data
	$(CXX) $(CXXFLAGS) $(BUILD_TIME_FLAG) -DEMBED_ASSETS -include src/embed.h $(RL_CFLAGS) $(SRCS) $(DIST_GEN) -o $(DIST_OUT) $(RL_STATIC) $(DIST_FRAMEWORKS); \
	status=$$?; rm -f $(DIST_GEN); exit $$status
	strip $(DIST_OUT)
	@echo "built $(DIST_OUT) -- one file, resources embedded; copy it anywhere and run"
endif

# make dist_mac packages the dist build as dist/GravityLeon.app.
# Finder and the Dock use the rounded version of tools/icon.png.
# The app writes its highscores, screenshots and bug reports in ~/Library/Application Support/GravityLeon.
# Signed ad hoc only: downloaded, macOS asks first (right-click > Open).
APP     := dist/GravityLeon.app
MAC_ICON := tools/icon-macos.png

$(MAC_ICON): tools/icon.png tools/make_icon.py
	python3 tools/make_icon.py --from-artwork

dist_mac: dist $(MAC_ICON)
	rm -rf $(APP)
	mkdir -p $(APP)/Contents/MacOS $(APP)/Contents/Resources
	cp $(DIST_OUT) $(APP)/Contents/MacOS/GravityLeon
	cp tools/Info.plist $(APP)/Contents/Info.plist
	python3 tools/make_icon.py --icns $(APP)/Contents/Resources/GravityLeon.icns
	codesign --force --deep --sign - $(APP)
	touch $(APP)
	@echo "built $(APP) -- the game as a macOS app, with its icon"

linux_dist windows_dist:
	@echo "$@ packaging is not implemented yet"

distclean:
	rm -rf dist

.PHONY: run clean dist distclean dist_mac linux_dist windows_dist

# Browser build tools, objects and the playable files live outside the native build.
WEB_PORT ?= 8000

web:
	python3 tools/build_web.py build

web-dist:
	python3 tools/build_web.py archive

web-run: web
	python3 -m http.server $(WEB_PORT) --bind 127.0.0.1 --directory dist/web

web-clean:
	python3 tools/build_web.py clean

.PHONY: web web-dist web-run web-clean
