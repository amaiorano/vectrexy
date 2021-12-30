# Vectrexy

Vectrexy is a [Vectrex](https://en.wikipedia.org/wiki/Vectrex) emulator programmed in C++.

This project is open source and available on GitHub: https://github.com/amaiorano/vectrexy

## Download Latest Build

### Build status ![master branch status](https://github.com/amaiorano/vectrexy/actions/workflows/ci.yml/badge.svg?branch=master)

* [Vectrexy for Windows 64-bit](https://dl.cloudsmith.io/public/vectrexy/vectrexy/raw/files/vectrexy-windows-x64.zip)

* [Vectrexy for Windows 32-bit](https://dl.cloudsmith.io/public/vectrexy/vectrexy/raw/files/vectrexy-windows-x86.zip)

* [Vectrexy for Linux 64-bit](https://dl.cloudsmith.io/public/vectrexy/vectrexy/raw/files/vectrexy-ubuntu-x64.zip)

\
[![Hosted By: Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.com)

Package repository hosting is graciously provided by  [Cloudsmith](https://cloudsmith.com).
Cloudsmith is the only fully hosted, cloud-native, universal package management solution, that
enables your organization to create, store and share packages in any format, to any place, with total
confidence.

## Twitch Development

I regularly stream part of the development on Twitch [right here](https://www.twitch.tv/daroou2). Follow me to know when I'm streaming!

## Compatibility

See the [Vectrexy Compatibility List](docs/vectrexy-compatilibity-list.md) for the list of games that Vectrexy can run.

## Controls

| Devices     | Player 1  | Player 2  |
| ----------- | --------- | --------- |
| No gamepads | Keyboard  | N/A       |
| 1 gamepad   | Gamepad 1 | Keyboard  |
| 2 gamepads  | Gamepad 1 | Gamepad 2 |

Keyboard key bindings: ASDF + Arrow keys

## Overlays

The Vectrex display is black & white, so to add color, each game cartridge came with a transparent colored overlay that would be slotted in front of the screen. For emulation purposes, you should be able to find png files for these overlays. If you place these png file in the `data/overlays` folder, Vectrexy will attempt to match the rom's file name to the overlay name using "fuzzy" string matching (in other words, the file names do not need to match exactly).

## What's a Vectrex and why did you write this emulator?

The Vectrex is a really cool and unique video game console that was released in 1982. What made it unique was that it came with its own screen and displayed vector-based graphics, rather than the typical sprite/raster based graphics of most game consoles. My uncle had gotten me a Vectrex when I was only 8 years old, and I still have it, and it's still awesome.

## Credits

Although the emulator core is written by me, Antonio Maiorano (Daroou2), it makes use of third party libraries, and is packaged with overlays created by other people. I hope I've got everyone covered here; if not, please let me know and I'll be happy to correct this list.

- Overlays: THK-Hyperspin, Gigapig-Hyperspin, Nosh01-GitHub, and other unknown authors.
- FastBoot and SkipBoot bios roms: Franck Chevassu, author of the exellent Vectrex emulator, [ParaJVE](http://www.vectrex.fr/ParaJVE/)
- SDL2: [SDL2 Credits](https://www.libsdl.org/credits.php)
- GLEW: [GLEW authors](https://github.com/nigels-com/glew#authors)
- GLM: [G-Truc Creations](http://www.g-truc.net/)
- stb: [Sean Barrett (Nothings)](http://nothings.org/)
- Dear ImGui: [omar (ocornut)](http://www.miracleworld.net/)
- linenoise: [Salvatore Sanfilippo (antirez)](http://invece.org/)
- noc: [Guillaume Chereau](https://blog.noctua-software.com/)

## Building the code

### Windows

Install:
* [CMake](https://cmake.org/)
* [Visual Studio](https://www.visualstudio.com/downloads/)

Clone and build vectrexy using CMake:
```bash
git clone --recursive https://github.com/amaiorano/vectrexy.git
cd vectrexy
mkdir build && cd build
cmake ..
cmake --build .
```

### Ubuntu

Install:
* [CMake](https://cmake.org/)
* gcc 8 or higher

Install a compiler and some Linux-specific libs we depend on:
```bash
sudo apt-get install g++-8 libgtk2.0-dev
```

SDL2 has many dependencies, some of which you may need to install:
```
# SDL static lib dependencies (see https://hg.libsdl.org/SDL/file/default/docs/README-linux.md)
# Alternatively, you can just 'apt-get libsdl2-dev' to build against the dynamic library

sudo apt-get install build-essential mercurial make cmake autoconf automake libtool libasound2-dev libpulse-dev libaudio-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxxf86vm-dev libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev libgles1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libibus-1.0-dev fcitx-libs-dev libsamplerate0-dev libsndio-dev
```

Clone and build vectrexy using CMake:
```bash
git clone --recursive https://github.com/amaiorano/vectrexy.git
cd vectrexy
mkdir build && cd build
cmake ..
cmake --build .
```

### Extra CMake Build Args

Use ```-D<VAR_NAME>=<VALUE>``` from the CMake CLI, or use cmake-gui to set these.

#### BUILD_SHARED_LIBS=on|off (Default: off)

If enabled, builds a DLL/.so version.

**NOTE**: On Windows, vcpkg's "static" triplets (e.g. x64-windows-static) create static libs that link against the static CRT (/MT), while CMake generates shared library builds that link against the dynamic CRT (/MD). Thus, when building, the linker will emit: `LINK : warning LNK4098: defaultlib 'LIBCMT' conflicts with use of other libs; use /NODEFAULTLIB:library`. You can ignore this for the most part; however, you can fix this warning by creating a custom vcpkg triplet, e.g. x64-windows-static-md.cmake, that is a copy of x64-windows-static.cmake, except with `set(VCPKG_CRT_LINKAGE dynamic)`. If you use this triplet to build dependencies with vcpkg, and specify it as CMake's `VCPKG_TARGET_TRIPLET`, all libraries will use the dynamic CRT, and no warning will be emitted by the linker.

#### DEBUG_UI=on|off (Default: on)

If enabled, the in-game debug UI can be displayed. Mostly useful for development.

#### ENGINE_TYPE=null|sdl (Default: sdl)

The type of engine to use. By default, SDL is used. If "null" is specified, the emulator will execute without any audio or visuals; however, the debugger will work, which can be useful for testing the emulator, or as a starting point for a new engine type.

## Contributing

As the emulator is still in early stages of development, I generally won't be looking at or accepting pull requests. Once the project has matured enough, this will likely change. If you wish, [follow my stream](https://www.twitch.tv/daroou2) and make suggestions in chat instead.
