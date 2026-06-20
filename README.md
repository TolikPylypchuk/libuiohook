# libUIOHook: Cross-platform keyboard and mouse hooking from userland.

> [!NOTE]
> This is a fork of libuiohook that exists solely for the needs of
> [SharpHook](https://github.com/TolikPylypchuk/SharpHook). It is not intended for general-purpose usage that's not
> related to SharpHook.

## Compiling

Prerequisites:

- [CMake](https://cmake.org) - at least version 4.2
- GCC, Clang, or MSVC
- X11 dependencies:
  - libx11-dev
  - libxtst-dev
  - libxrandr-dev
  - libxt-dev
  - libx11-xcb-dev
  - libxkbcommon-dev
  - libxkbcommon-x11-dev
  - libxkbfile-dev

To build, run the following commands:

```sh
git clone https://github.com/TolikPylypchuk/libuiohook
cd libuiohook
mkdir build && cd build
cmake -S .. -D BUILD_DEMO=ON -D CMAKE_INSTALL_PREFIX=../dist
cmake --build . --parallel 2 --target install
```

On Windows, add CMake parameters `-G "Visual Studio 18 2026"` and `-D CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`.
On macOS and Linux, add CMake parameter `-G "Unix Makefiles"`.

On macOS, you can add the `MAC_CATALYST=ON` option to build libuiohook for Mac Catalyst instead of macOS.

You can optionally add the `BUILD_DEMO=ON` option to build demo applications, and `BUILD_TEST=ON` to build tests.
Note that on Linux, tests require X11 to be present, so they cannot run in headless environments like CI pipelines.

## Usage

- [Hook Demo](demo/demo_hook.c)
- [Async Hook Demo](demo/demo_hook_async.c)
- [Event Post Demo](demo/demo_post.c)
- [Properties Demo](demo/demo_properties.c)
- [Public Interface](include/uiohook.h)
