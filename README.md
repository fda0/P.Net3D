3D game/engine with networking. Written in C. Uses SDL3.

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
Project can be built on Windows using MSVC (default) or clang.  
Currently, only Windows is supported due to a dependency on DirectX 12. Adding Vulkan support in the future should be relatively easy, as the project uses the SDL_gpu API to handle GPU boilerplate setup.

Arguments to build script
- `game` - compile game target (default)
- `sdl` - compile SDL libraries
- `msvc` - compile using MSVC compiler (default)
- `clang` - compile using clang compiler
- `release` - compile with optimizations (todo: not well supported yet)

Example commands:
```bash
build.bat
```
```bash
build.bat game sdl
```
```bash
build.bat game clang
```

# Resources
## Collision
The game uses the SAT algorithm for collision detection.
Two resources I found especially useful when researching its implementation:
- [Collision Detection with SAT (Math for Game Developers) by pikuma](https://www.youtube.com/watch?v=-EsWKT7Doww)
- [N Tutorial A – Collision Detection and Response](https://www.metanetsoftware.com/2016/n-tutorial-a-collision-detection-and-response)

## Game math
I used the [Handmade Math](https://github.com/HandmadeMath/HandmadeMath) library as a reference for matrix and quaternion math implementations.

Resources on quaternions:
- [Quaternions, Double-cover, and the Rest Pose Neighborhood (2006) by Casey Muratori](https://caseymuratori.com/blog_0002) + [video](https://www.youtube.com/watch?v=vmAY5kP-tpU)
- [Understanding Slerp, Then Not Using It by Jonathan Blow](http://number-none.com/product/Understanding%20Slerp,%20Then%20Not%20Using%20It/)
