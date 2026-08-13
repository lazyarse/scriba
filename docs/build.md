# Building Scriba

## Prerequisites

- CMake 3.16+
- Qt6 development libraries
  ```bash
  sudo apt install qt6-base-dev qt6-webengine-dev qt6-webchannel-dev
  ```
- GCC/Clang with C++23 support (Linux) or Visual Studio 2022 with "Desktop development with C++" workload (Windows)
- On Windows: Qt 6.8+ (MSVC 2022 64-bit) from qt.io, CMake, Git for Windows
- **Chromium ≥ 140** (for PDF export). Install on Debian/Ubuntu:
  ```bash
  sudo apt install chromium
  ```
  If Chromium is not found, PDF export falls back to Qt's built-in renderer
  (cannot suppress default page headers/footers).
- Approximately 10 minutes of your time that you will never get back.

## Building

### Build on Linux

```bash
mkdir -p build
cmake --build build --target clean 2>/dev/null   # remove stale _autogen dirs after branch switches
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

The post-build step automatically removes cached base stylesheets (`~/.config/scriba/*.css`), so no manual cleanup needed on rebuild.

### Build on Windows

In an **x64 Native Tools Command Prompt for VS 2022**:

```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\msvc2022_64"
cmake --build build -j4 --config Release
```

Binary: `build\Release\scriba.exe`

Copy from QEMU to host (host needs an ssh-server running): `scp file.txt user@10.0.2.2:~/`

### Build Windows installer

Requires [NSIS](https://nsis.sourceforge.io/) installed (`choco install nsis`). In an **x64 Native Tools Command Prompt for VS 2022**:

```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\msvc2022_64"
cmake --build build -j4 --config Release
cpack --config build/CPackConfig.cmake -G NSIS
```

Output: `scriba-<version>-win64.exe` (version derived from `git describe --tags --always --dirty`; includes a `-dirty` suffix if the working tree has uncommitted changes)

The NSIS installer adds Scriba to the Start Menu and registers `.md` files to open with Scriba.

> **Windows filename note:** colons (`:`) are not allowed in filenames on Windows. If your git tag
> contains a colon, `cpack` will fail. Stick to semver tags like `v1.2.3` to avoid this.

### Build .deb package

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cpack --config build/CPackConfig.cmake -G DEB
```

Copy the `.deb` to `/tmp/` before installing — your home directory likely blocks `_apt`:
```bash
cp scriba-*-Linux.deb /tmp/ && sudo apt install /tmp/scriba-*-Linux.deb
```

If you install straight from `./scriba-...deb` and your home directory is mode `700` (the default for `~/`) you may see:

```
'...scriba-...-Linux.deb' couldn't be accessed by user '_apt'. - pkgAcquire::Run (13: Permission denied)
```

and the same message for every parent directory of the `.deb`.

**This is an apt behaviour, not a Scriba bug.** To isolate package acquisition, `apt` reads the file as the unprivileged `_apt` user, which must be able to traverse *every* directory above the `.deb` — not just read the file itself. `apt` can reach `/var/cache/apt/archives/` (where it downloads normally-installed packages itself), but `_apt` cannot step into a `700` home directory, so it falls back to running as root. The install still succeeds; the message is non-fatal.

It surfaces for `./scriba` because it's a *locally-built* `.deb` inside a restrictive home directory — any hand-built package there triggers it, while repos-installed software never does. Fix by copying the `.deb` to a world-traversable location like `/tmp/` (above), or install without apt's sandbox:

```bash
sudo dpkg -i scriba-*-Linux.deb
```

Output: `scriba-1.0.0-Linux.deb` (may include a `-dirty` suffix if the working tree has uncommitted changes)

### Build with tests

During development, build the test suite in a separate Debug build dir (`build-dbg/`) — it compiles faster than a Release test build and keeps `build/` for the Release binary:

```bash
cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && cmake --build build-dbg -j$(nproc)
```

The `-D` flags are only needed the first time a build dir is configured; CMake caches them in `<dir>/CMakeCache.txt`, so later rebuilds are just `cmake --build build-dbg -j$(nproc)`.

### Shared object libraries

To avoid recompiling the same sources dozens of times, `CMakeLists.txt` builds three **object libraries** (not static archives — see below) and links every target against them:

- `scriba_resources` — compiles `resources/scriba.qrc` once instead of once per consumer (24 targets); `scriba_twemoji` does the same for `resources/twemoji-svg.qrc`.
- `scriba_app` — compiles the ~66 app sources in `SCRIBA_APP_WEBENGINE_SOURCES` (MainWindow, Editor, Preview, dialogs) once; the `scriba` binary and the 11 full-app test targets link the objects instead of recompiling them.

These must be OBJECT libraries, never STATIC. The generated `qrc_scriba.cpp` registers resources through an anonymous-namespace static initializer, and several app sources carry similar static-initializer side effects; an archive would silently drop those objects from any consumer that doesn't reference their symbols. Note also that object-library *files* do not propagate transitively — each consumer of `scriba_app` must link `scriba_resources` (and `scriba_twemoji` where emoji rendering is needed) explicitly.

### Run tests

```bash
cd build-dbg && ctest --output-on-failure -j4
```

Tests auto-wrap in `xvfb-run` when available (CMake detects it) so they don't crash on your headless CI. Parallel runs are safe: `setupTestConfig()` gives each test process a unique application name, so Qt WebEngine's disk-based data dirs (AppDataLocation/CacheLocation) are per-process too and even WebEngine-spawning suites may run concurrently under `ctest -jN`.
