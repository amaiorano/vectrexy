# Vectrexy

Vectrexy is a [Vectrex](https://en.wikipedia.org/wiki/Vectrex) emulator programmed in C++.

This project is open source and available on GitHub: https://github.com/amaiorano/vectrexy

## Download Latest Build

Windows [![Build status](https://ci.appveyor.com/api/projects/status/0adcwixak7ul9oet?svg=true)](https://ci.appveyor.com/project/amaiorano/vectrexy)

* [Vectrexy for Windows 64-bit](https://dl.bintray.com/amaiorano/vectrexy/vectrexy_win64.zip)
* [Vectrexy for Windows 32-bit](https://dl.bintray.com/amaiorano/vectrexy/vectrexy_win32.zip)

Linux [![Build Status](https://travis-ci.org/amaiorano/vectrexy.svg?branch=master)](https://travis-ci.org/amaiorano/vectrexy)

* [Vectrexy for Linux 64-bit](https://dl.bintray.com/amaiorano/vectrexy/vectrexy_linux64.zip)

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
* [vcpkg](https://github.com/Microsoft/vcpkg)

Install dependent packages with vcpkg:
```bash
vcpkg install sdl2:x64-windows-static sdl2-net:x64-windows-static glew:x64-windows-static glm:x64-windows-static stb:x64-windows-static imgui:x64-windows-static
```

Clone and build vectrexy using CMake:
```bash
git clone --recursive https://github.com/amaiorano/vectrexy.git
cd vectrexy
mkdir build && cd build
cmake -A x64 -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static ..
vectrexy.sln ..
```

### Ubuntu

Install:
* gcc 8 or higher
* [CMake](https://cmake.org/)
* [vcpkg](https://github.com/Microsoft/vcpkg)

Install some Linux-specific libs we depend on:
```bash
sudo apt-get install g++-8 libgtk2.0-dev

# SDL static lib dependencies (see https://hg.libsdl.org/SDL/file/default/docs/README-linux.md)
# Alternatively, you can just 'apt-get libsdl2-dev' to build against the dynamic library
sudo apt-get install build-essential mercurial make cmake autoconf automake libtool libasound2-dev libpulse-dev libaudio-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxxf86vm-dev libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev libgles1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libibus-1.0-dev fcitx-libs-dev libsamplerate0-dev libsndio-dev
```

Install the rest of the dependencies through vcpkg:
```bash
cd vcpkg
./vcpkg install sdl2 sdl2-net glew glm stb imgui
```

Clone and build vectrexy using CMake:
```bash
git clone --recursive https://github.com/amaiorano/vectrexy.git
cd vectrexy
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake ..
make
```

### Extra CMake Build Args

Use ```-D<VAR_NAME>=<VALUE>``` from the CMake CLI, or use cmake-gui to set these.

#### BUILD_SHARED_LIBS=on|off (Default: off)

If enabled, builds a DLL/.so version.

**NOTE**: On Windows, vcpkg's "static" triplets (e.g. x64-windows-static) create static libs that link against the static CRT (/MT), while CMake generates shared library builds that link against the dynamic CRT (/MD). Thus, when building, the linker will emit: `LINK : warning LNK4098: defaultlib 'LIBCMT' conflicts with use of other libs; use /NODEFAULTLIB:library`. You can ignore this for the most part; however, you can fix this problem by creating a custom vcpkg triplet, e.g. x64-windows-static-md.cmake, that is a copy of x64-windows-static.cmake, except with `set(VCPKG_CRT_LINKAGE dynamic)`. If you use this triplet to build dependencies with vcpkg, and specify it as CMake's `VCPKG_TARGET_TRIPLET`, all libraries will use the dynamic CRT, and no warning will be emitted by the linker.

#### DEBUG_UI=on|off (Default: on)

If enabled, the in-game debug UI can be displayed. Mostly useful for development.

#### ENGINE_TYPE=null|sdl (Default: sdl)

The type of engine to use. By default, SDL is used. If "null" is specified, the emulator will execute without any audio or visuals; however, the debugger will work, which can be useful for testing the emulator, or as a starting point for a new engine type.


## Contributing

As the emulator is still in early stages of development, I generally won't be looking at or accepting pull requests. Once the project has matured enough, this will likely change. If you wish, [follow my stream](https://www.twitch.tv/daroou2) and make suggestions in chat instead.
