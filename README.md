# Vectrexy

Vectrexy is a [Vectrex](https://en.wikipedia.org/wiki/Vectrex) emulator programmed in C++.

## Twitch Development

I regularly stream part of the development on Twitch [right here](https://www.twitch.tv/daroou2). Follow me to know when I'm streaming!

## What's a Vectrex and why are you writing this emulator?

The Vectrex is a really cool and unique video game console that was released in 1982. What made it unique was that it came with its own screen and displayed vector-based graphics, rather than the typical sprite/raster based graphics of most game consoles. My uncle had gotten me a Vectrex when I was only 8 or so years old, and I still have it, and it's still awesome :)

## Building the code

The emulator is being developed mainly in [Visual Studio 2017](https://www.visualstudio.com/downloads/), and makes use of C++17 features. To build it, install [CMake](https://cmake.org/), then clone the repo and generate the build system. In my case, I generate using the "Visual Studio 15 2017" generator, then open and build the generated vectrexy.sln.

Here's an example of how to build (on Windows):

```bash
git clone https://github.com/amaiorano/vectrexy.git
cd vectrexy
mkdir build && cd build
cmake -G "Visual Studio 15 2017" -DSDL2=C:\code\SDL2-2.0.4 -DGLEW_ROOT_DIR=C:\code\glew-2.1.0 -DGLEW_USE_STATIC_LIBS=1 -DGLM_ROOT_DIR=C:\code\active\glm-0.9.8.5 ..
vectrexy.sln ..
```

## Contributing

As the emulator is still in early stages of development, I generally won't be looking at or accepting pull requests. Once the project has matured enough, this will likely change. If you wish, [follow my stream](https://www.twitch.tv/daroou2) and make suggestions in chat instead.
