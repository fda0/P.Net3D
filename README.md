# P.Net3D
3D game/engine with networking. Continuation of previous project (2D) (https://github.com/fda0/demongus / https://github.com/poleonek/demongus).
Uses SDL3 & SDL_net. Currently only on Windows is supported due to dependency on DirectX12. Adding Vulkan support should be relatively easy as the project uses SDL_gpu api to abstract GPU setup boilerplate.

## CLONING THE PROJECT
Repository uses git submodules to include SDL3.

To download the repo and SDL3 at the same time run:
```bash
git clone --recurse-submodules git@github.com:fda0/P.Net3D.git
```
To clone repo and update submodules separately run:
```bash
git clone git@github.com:fda0/P.Net3D.git
git submodule update --init --recursive
```

## BUILD
On Windows: `./build.bat`

On Linux: `./build.sh`

Example commands:
```bash
./build.sh sdl game
```
```bash
./build.sh game
```
```bash
./build.sh sdl game release
```

# Resources
## Collision
The game uses SAT algorithm for collision detection.
Two links that I found especially useful while researching how to implement it:
- [Collision Detection with SAT (Math for Game Developers) by pikuma](https://www.youtube.com/watch?v=-EsWKT7Doww)
- [N Tutorial A – Collision Detection and Response](https://www.metanetsoftware.com/2016/n-tutorial-a-collision-detection-and-response)
