3D game/engine with networking. Written in C. Uses SDL3.  
[Video - Mar 15, 2025](https://www.youtube.com/watch?v=tm6N5Fy2Jxs)
[Video - Feb 27, 2025](https://www.youtube.com/watch?v=567seD6WJco)
[Video - Feb 11, 2025](https://www.youtube.com/watch?v=v2UXzL0xuF4)


# Cloning the project
To download the repository and SDL3 at the same time, run:
```bash
git clone --recurse-submodules git@github.com:fda0/P.Net3D.git
```
To download the repository and its submodules separately, run
```bash
git clone git@github.com:fda0/P.Net3D.git
git submodule update --init --recursive
```


# Building
Project can be built on Windows using [MSVC (default)](https://github.com/EpicGamesExt/raddebugger?tab=readme-ov-file#development-setup-instructions) or clang.
Currently, only Windows is supported due to a dependency on DirectX 12. Adding Vulkan support in the future should be relatively easy, as the project uses the SDL_gpu API to handle GPU boilerplate setup.

Arguments to build script
- `game` - compile game target (default)
- `sdl` - compile SDL libraries
- `msvc` - compile using MSVC compiler (default)
- `clang` - compile using clang compiler
- `release` - compile with optimizations

Example commands:
```bash
# build sdl & game - with optimizations enabled
build.bat sdl game release
```
```bash
# build sdl & game - debug builds
build.bat sdl game
```
```bash
# build game - debug build
build.bat game clang
```
```bash
# build game using clang - debug build
build.bat game clang
```


# Codebase
## `src/base`
Small libraries that could be used across multiple projects.
- `base_types.h` - Define types and must-have-macros.
- `base_arena.h` - Simple & fast bump/stack-like allocator.
- `base_string.h` - Defines `S8` - length encoded string and its helper functions.
- `base_printer.h` - String builder based. Tracks pre-allocated buffer and allows to easily copy text into it.
- `base_math.h` - Trigonometry functions, vectors, matrices, quaternions.

## `src/meta`
`meta_entry.c` is compiled and ran before the game starts compiling.
The plan is that all metaprogramming, code generation, and asset preparation will live there.
Right now, its only job is to load .obj model files and transform them into a format expected by the engine.
It can automatically generate per-vertex-normals if these are missing from the .obj file.

## `src/gen`
Auto-generated files like models' vertex buffers or precompiled shaders go here.
Automatically cleared before build. Committed using `git lfs` to reduce spam in change history.

## `src/game`
The game uses the ["unity build"](https://en.wikipedia.org/wiki/Unity_build) technique to compile all .c files in one translation unit.
- `game_sdl_entry.c` - Entry point of the game. Initialized SDL contexts, spawns game window, poll user input.
- `game_constants.h` - Compile time constants used to modify how the game works.
- `game_core` - Manages game logic loop, initialization. Contains code that didn't fit anywhere else yet.
- `game_gpu`, `game_render` - Manages GPU setup, vertex layouts, uploads data to GPU and dispatches render calls.
- `game_network` - Setting up connection, sending and receiving payloads over network.
- `game_animation` - Manages animation state, prepares joint transformations into a format that can be sent to the GPU.
- `game_server` / `game_client` - Server / client side networking logic.
- `game_object` - Constructors and helpers for Objects. Most things that are visible on the screen are called an Object in this engine.
- `game_tick` - Game physics simulation. Physics updates use a fixed timestep in this engine.
- `game_key.c` - Helper functions for checking user input.
- `game_util` - Helpers that are somewhat universal but still specific to the game - so they don't end up in `src/base`
- `shader_*` - hlsl shaders. In the future I'll try looking into systems that auto-translate shaders for hlsl & vulkan targets.


# Resources
## Collision
The game uses the SAT algorithm for collision detection.
Two resources I found especially useful when researching its implementation:
- [Collision Detection with SAT (Math for Game Developers) by pikuma](https://www.youtube.com/watch?v=-EsWKT7Doww)
- [N Tutorial A – Collision Detection and Response](https://www.metanetsoftware.com/2016/n-tutorial-a-collision-detection-and-response)

## Networking
[Gaffer on Games](https://gafferongames.com/categories/game-networking/) articles are great to get an overview of various networking topics.

## Math
I used the [Handmade Math](https://github.com/HandmadeMath/HandmadeMath) library as a reference for matrix and quaternion math implementations.

Resources on quaternions:
- [Quaternions, Double-cover, and the Rest Pose Neighborhood (2006) by Casey Muratori](https://caseymuratori.com/blog_0002) + [video](https://www.youtube.com/watch?v=vmAY5kP-tpU)
- [Understanding Slerp, Then Not Using It by Jonathan Blow](http://number-none.com/product/Understanding%20Slerp,%20Then%20Not%20Using%20It/)

## Using .gltf files - models and animations
- https://github.com/jkuhlmann/cgltf - small portable library that parses .gltf files and gives you structs and arrays of data.
- [Skeletal animation in glTF - lisyarus blog](https://lisyarus.github.io/blog/posts/gltf-animation.html) - the best article on the topic of using animations from .gltf files that I found.

## Shadow mapping
- [Shadow maps, shadow volumes, deep shadow maps by Justin Solomon](https://www.youtube.com/watch?v=QCIKgyL3ePo) - MIT lecture - a good overview of different shadow rendering techniques.
- [Shadow Mapping learnopengl.com](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping) - good step by step tutorial for getting shadow maps on the screen. Explains common issues.
