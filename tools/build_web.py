"""Build Gravity Leon and its pinned WebAssembly dependencies inside this checkout."""
import argparse
import datetime
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile
from embed_gen import score_resources

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / ".scratch" / "web"
OUTPUT = ROOT / "dist" / "web"
ARCHIVE = ROOT / "dist" / "game-web.zip"
EMSDK = "4.0.20"
RAYLIB = "6.0"
FILES = ("index.html", "index.js", "index.wasm", "index.data")


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True, **kwargs)


def checkout(project, tag):
    name = project.split("/")[-1]
    dest = WORK / (name + "-" + tag)
    if not dest.exists():
        with tempfile.TemporaryDirectory(dir=WORK) as tmp:
            source = Path(tmp) / name
            run("git", "clone", "--depth", "1", "--branch", tag, "--single-branch",
                "https://github.com/" + project + ".git", source)
            source.rename(dest)
    return dest


def toolchain():
    sdk = checkout("emscripten-core/emsdk", EMSDK)
    compiler = sdk / "upstream" / "emscripten"
    config = sdk / ".emscripten"
    env = os.environ.copy()
    # Do not inherit a system SDK, its config, node, flags, sysroot or ports cache.
    for key in list(env):
        if key.startswith(("EMSDK", "EM_", "EMCC_", "EMMAKEN_", "EMSCRIPTEN")):
            del env[key]
    env["EM_CONFIG"] = str(config)
    if not config.is_file() or not (compiler / "em++").is_file():
        run(sys.executable, sdk / "emsdk.py", "install", EMSDK, env=env)
        run(sys.executable, sdk / "emsdk.py", "activate", EMSDK, env=env)
    env["PATH"] = str(compiler) + os.pathsep + env["PATH"]
    env["EMSDK"] = str(sdk)
    env["EM_CACHE"] = str(compiler / "cache")  # Includes the SDK's prebuilt sysroot.
    return compiler, env


def assets():
    yield ROOT / "data" / "params.txt"
    for folder in ("assets", "levels"):
        for path in sorted((ROOT / folder).rglob("*")):
            if (path.is_file() and not path.is_symlink() and
                    all(not part.startswith(".") for part in path.relative_to(ROOT).parts)):
                yield path


def build():
    WORK.mkdir(parents=True, exist_ok=True)
    compiler, env = toolchain()
    raylib = checkout("raysan5/raylib", RAYLIB)
    objects = WORK / ("raylib-web-" + RAYLIB)
    run(compiler / "emcmake", "cmake", "-S", raylib, "-B", objects, "-G", "Unix Makefiles",
        "-DPLATFORM=Web", "-DOPENGL_VERSION=ES 3.0", "-DBUILD_EXAMPLES=OFF",
        "-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_BUILD_TYPE=Release", env=env)
    run("cmake", "--build", objects, "--target", "raylib", "--parallel", env=env)
    static_lib, = objects.rglob("libraylib.a")
    sources = sorted((ROOT / "src").glob("*.cpp"))
    resources = [(path.relative_to(ROOT).as_posix(), path) for path in assets()]
    resources.extend(score_resources(ROOT))
    # Keep an inventory as well as timestamps so a removed asset or source triggers a relink.
    inputs = [Path(__file__).resolve(), static_lib, *sorted((ROOT / "web").glob("*")),
              *sources, *sorted((ROOT / "src").glob("*.h")), ROOT / "tools/embed_gen.py",
              *(path for _, path in resources)]
    stamp_file = objects / "game-inputs.json"
    inventory = json.dumps([[str(p.relative_to(ROOT)), p.stat().st_size, p.stat().st_mtime_ns]
                            for p in inputs if p.is_file()])
    if stamp_file.exists() and stamp_file.read_text() == inventory and all(
            (OUTPUT / name).is_file() for name in FILES):
        validate(OUTPUT)
        print("Up to date:", OUTPUT.relative_to(ROOT))
        return
    OUTPUT.parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=WORK) as tmp:
        stage = Path(tmp)
        preload = []
        for name, path in resources:
            source = path.relative_to(ROOT).as_posix().replace("@", "@@")
            preload.extend(["--preload-file", source + "@/" + name.replace("@", "@@")])
        stamp = datetime.datetime.now().strftime("%Y-%m-%d_%H:%M")
        run(compiler / "em++", "-std=c++17", "-O2", "-Wall", "-fexceptions",
            '-DGRAVITY_LEON_BUILD_TIME="' + stamp + '"', "-DPLATFORM_WEB", "-DGRAPHICS_API_OPENGL_ES3",
            "-I", raylib / "src",
            *sources, ROOT / "web/platform.cpp", static_lib,
            "-sUSE_GLFW=3", "-sMIN_WEBGL_VERSION=2", "-sMAX_WEBGL_VERSION=2",
            "-sGL_ENABLE_GET_PROC_ADDRESS=1", "-sASYNCIFY", "-sASYNCIFY_STACK_SIZE=65536",
            "-sASYNCIFY_IMPORTS=waitFrame,finishGame",
            "-sSTACK_SIZE=2097152", "-sINITIAL_MEMORY=134217728", "-sALLOW_MEMORY_GROWTH=1",
            "-sFORCE_FILESYSTEM=1", "-lidbfs.js", "-sEXPORTED_RUNTIME_METHODS=callMain,stringToNewUTF8",
            "-sENVIRONMENT=web", "--pre-js", ROOT / "web/storage.js", *preload,
            "-o", stage / "index.js", env=env)
        shutil.copyfile(ROOT / "web/index.html", stage / "index.html")
        validate(stage)
        OUTPUT.mkdir(exist_ok=True)
        if stamp_file.exists():
            stamp_file.unlink()
        for name in FILES:
            os.replace(stage / name, OUTPUT / name)
        stamp_file.write_text(inventory)
    print("Built", OUTPUT.relative_to(ROOT))


def validate(directory):
    for name in FILES:
        if not (directory / name).stat().st_size:
            raise ValueError("Empty release file " + name)
    with (directory / "index.wasm").open("rb") as wasm:
        if wasm.read(8) != b"\0asm\x01\0\0\0":
            raise ValueError("Invalid WebAssembly module header")


def archive():
    run(sys.executable, ROOT / "tools/update_dist_highscores.py")
    build()
    validate(OUTPUT)
    with tempfile.TemporaryDirectory(dir=ARCHIVE.parent) as tmp:
        path = Path(tmp) / ARCHIVE.name
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as out:
            for name in FILES:
                out.write(OUTPUT / name, name)
        with zipfile.ZipFile(path) as check:
            if tuple(check.namelist()) != FILES or check.testzip():
                raise ValueError("Invalid itch.io distribution archive")
        os.replace(path, ARCHIVE)
    print("Packaged", ARCHIVE.relative_to(ROOT))


def clean():
    # Keep the isolated downloads, compiler and compiled toolchain cache for the next run.
    for directory in (OUTPUT, WORK / ("raylib-web-" + RAYLIB)):
        if directory.exists():
            shutil.rmtree(directory)
    if ARCHIVE.exists():
        ARCHIVE.unlink()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build", "archive", "clean"))
    try:
        action = parser.parse_args().action
        WORK.mkdir(parents=True, exist_ok=True)
        with (WORK / "build.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            {"build": build, "archive": archive, "clean": clean}[action]()
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        sys.exit(str(error))
