# libUIOHook: Cross-platform keyboard and mouse hooking from userland.

> [!NOTE]
> This is a fork of libuiohook that exists solely for the needs of
> [SharpHook](https://github.com/TolikPylypchuk/SharpHook). It is not intended for general-purpose usage that's not
> related to SharpHook.

## Compiling

Prerequisites:

- [cmake](https://cmake.org)
- gcc, clang or msvc
- x11 dependencies:
  - libx11-dev
  - libxtst-dev
  - libxt-dev
  - libxinerama-dev
  - libx11-xcb-dev
  - libxkbcommon-dev
  - libxkbcommon-x11-dev
  - libxkbfile-dev
  - libxrandr-dev

```
$ git clone https://github.com/kwhat/libuiohook
$ cd libuiohook
$ mkdir build && cd build
$ cmake -S .. -D BUILD_DEMO=ON -DCMAKE_INSTALL_PREFIX=../dist
$ cmake --build . --parallel 2 --target install
```

### Configuration

|           | option                        | description            | default |
| --------- | ----------------------------- | ---------------------- | ------- |
| **all**   | BUILD_DEMO:BOOL               | demo applications      | OFF     |
|           | ENABLE_TEST:BOOL              | testing                | OFF     |
| **OSX**   | USE_APPLICATION_SERVICES:BOOL | framework              | ON      |
|           | USE_IOKIT:BOOL                | framework              | ON      |
|           | USE_APPKIT:BOOL               | obj-c api              | ON      |
| **Linux** | USE_XINERAMA:BOOL             | xinerama library       | ON      |
|           | USE_XRANDR:BOOL               | xrandt extension       | OFF     |

## Usage

- [Hook Demo](demo/demo_hook.c)
- [Async Hook Demo](demo/demo_hook_async.c)
- [Event Post Demo](demo/demo_post.c)
- [Properties Demo](demo/demo_properties.c)
- [Public Interface](include/uiohook.h)
- Please see the man pages for function documentation.
